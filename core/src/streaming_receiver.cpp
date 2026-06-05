#include "hftext_streaming_receiver.h"

#include "hftext_frame.h"
#include "hftext_robust.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace hftext {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr int kBitsPerSymbol = 6;
constexpr int kSyncBits = 16;
constexpr int kLengthBits = 8;
constexpr int kCrcBits = 16;
constexpr int kConvTailBits = 2;
constexpr int kConvOutputBitsPerInputBit = 2;
constexpr int kMaxRetainedBits = 1800;
constexpr int kPhaseDivisions = 20;
constexpr int kPhysicalLengthBits = kBitsPerByte * kPhysicalLengthRepeat;

std::size_t packedPayloadBytes(int symbolCount) {
    if (symbolCount <= 0) {
        return 0;
    }
    return static_cast<std::size_t>((symbolCount * kBitsPerSymbol + kBitsPerByte - 1) / kBitsPerByte);
}

int bitMismatchCount(
    const std::vector<std::uint8_t>& bits,
    std::size_t start,
    const std::vector<std::uint8_t>& pattern
) {
    if (start + pattern.size() > bits.size()) {
        return static_cast<int>(pattern.size());
    }

    int mismatches = 0;
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        if (bits[start + index] != pattern[index]) {
            ++mismatches;
        }
    }
    return mismatches;
}

int logicalFrameBitCountForPayloadLength(int payloadLength) {
    return (kHeaderBytes + payloadByteCount(payloadLength) + kCrcBytes) * kBitsPerByte;
}

std::size_t robustFrameBitCountForPayloadLength(int payloadLength) {
    const int logicalBits = logicalFrameBitCountForPayloadLength(payloadLength);
    return static_cast<std::size_t>(logicalBits + kConvTailBits) * kConvOutputBitsPerInputBit;
}

double toneEnergyWindow(
    const std::vector<float>& samples,
    std::size_t start,
    std::size_t count,
    int sampleRate,
    float frequencyHz
) {
    double inPhase = 0.0;
    double quadrature = 0.0;

    for (std::size_t index = 0; index < count; ++index) {
        const double t = static_cast<double>(index) / sampleRate;
        const double phase = 2.0 * kPi * frequencyHz * t;
        const double sample = samples[start + index];
        inPhase += sample * std::cos(phase);
        quadrature += sample * std::sin(phase);
    }

    return inPhase * inPhase + quadrature * quadrature;
}

BitDecision demodulateSymbol(
    const std::vector<float>& samples,
    std::size_t start,
    std::size_t count,
    const ModemConfig& config
) {
    const double energy0 = toneEnergyWindow(
        samples,
        start,
        count,
        config.sampleRate,
        config.frequency0Hz
    );
    const double energy1 = toneEnergyWindow(
        samples,
        start,
        count,
        config.sampleRate,
        config.frequency1Hz
    );
    const double totalEnergy = energy0 + energy1;
    const double confidence = totalEnergy <= 0.0 ? 0.0 : std::abs(energy1 - energy0) / totalEnergy;

    return BitDecision{
        static_cast<std::uint8_t>(energy1 > energy0 ? 1 : 0),
        static_cast<float>(std::clamp(confidence, 0.0, 1.0))
    };
}

float meanConfidence(
    const std::vector<BitDecision>& decisions,
    std::size_t start,
    std::size_t count
) {
    if (start >= decisions.size() || count == 0) {
        return 0.0F;
    }

    const auto end = std::min(decisions.size(), start + count);
    double sum = 0.0;
    for (std::size_t index = start; index < end; ++index) {
        sum += decisions[index].confidence;
    }

    return static_cast<float>(sum / static_cast<double>(end - start));
}

}  // namespace

bool StreamingReceiver::EventKey::operator<(const EventKey& other) const {
    if (type != other.type) {
        return static_cast<int>(type) < static_cast<int>(other.type);
    }
    if (phaseOffsetSamples != other.phaseOffsetSamples) {
        return phaseOffsetSamples < other.phaseOffsetSamples;
    }
    if (syncSample != other.syncSample) {
        return syncSample < other.syncSample;
    }
    return bucket < other.bucket;
}

StreamingReceiver::StreamingReceiver(const ModemConfig& config)
    : config_(config) {
    resetPhaseStates();
}

void StreamingReceiver::setConfig(const ModemConfig& config) {
    config_ = config;
    reset();
}

const ModemConfig& StreamingReceiver::config() const {
    return config_;
}

void StreamingReceiver::reset() {
    buffer_.clear();
    sampleCursor_ = 0;
    events_.clear();
    reportedEvents_.clear();
    resetPhaseStates();
}

std::vector<DecodeResult> StreamingReceiver::pushSamples(const std::vector<float>& samples) {
    if (samples.empty()) {
        return {};
    }

    if (phases_.empty()) {
        resetPhaseStates();
    }

    buffer_.insert(buffer_.end(), samples.begin(), samples.end());

    std::vector<DecodeResult> results;
    while (true) {
        for (auto& phase : phases_) {
            processPhase(phase);
        }

        DecodeResult result;
        std::size_t frameEndSample = 0;
        if (!findBestFrame(result, frameEndSample)) {
            break;
        }

        results.push_back(result);
        resetAfterFrame(frameEndSample);
        if (buffer_.empty()) {
            break;
        }
    }

    trimBitBuffers();
    trimBuffer();
    return results;
}

std::vector<StreamingReceiverEvent> StreamingReceiver::takeEvents() {
    auto events = events_;
    events_.clear();
    return events;
}

int StreamingReceiver::samplesPerSymbol() const {
    const auto samples = static_cast<int>(
        std::lround(static_cast<double>(config_.sampleRate) * config_.symbolDurationSec)
    );
    if (samples <= 0) {
        throw std::invalid_argument("symbol duration is too short for sample_rate");
    }
    return samples;
}

std::size_t StreamingReceiver::frameBitCount(const DecodeResult& result) const {
    return robustFrameBitCountForPayloadLength(result.length);
}

std::vector<int> StreamingReceiver::phaseOffsets() const {
    const int symbolSamples = samplesPerSymbol();
    const int step = std::max(1, symbolSamples / kPhaseDivisions);

    std::vector<int> offsets;
    for (int offset = 0; offset < symbolSamples; offset += step) {
        offsets.push_back(offset);
    }
    if (offsets.empty()) {
        offsets.push_back(0);
    }
    return offsets;
}

void StreamingReceiver::resetPhaseStates() {
    phases_.clear();
    for (const int offset : phaseOffsets()) {
        PhaseState phase;
        phase.offsetSamples = offset;
        phase.nextStartSample = sampleCursor_ + static_cast<std::size_t>(offset);
        phase.firstBitSample = phase.nextStartSample;
        phases_.push_back(std::move(phase));
    }
}

void StreamingReceiver::processPhase(PhaseState& phase) {
    const std::size_t symbolSamples = static_cast<std::size_t>(samplesPerSymbol());
    const std::size_t sampleEnd = sampleCursor_ + buffer_.size();

    if (phase.nextStartSample < sampleCursor_) {
        phase.nextStartSample = sampleCursor_ + static_cast<std::size_t>(phase.offsetSamples);
        phase.firstBitSample = phase.nextStartSample;
        phase.decisions.clear();
        phase.bits.clear();
    }

    while (phase.nextStartSample + symbolSamples <= sampleEnd) {
        const auto localStart = phase.nextStartSample - sampleCursor_;
        if (phase.bits.empty()) {
            phase.firstBitSample = phase.nextStartSample;
        }

        const auto decision = demodulateSymbol(buffer_, localStart, symbolSamples, config_);
        phase.decisions.push_back(decision);
        phase.bits.push_back(decision.bit);
        phase.nextStartSample += symbolSamples;
    }
}

bool StreamingReceiver::findBestFrame(DecodeResult& result, std::size_t& frameEndSample) {
    bool hasCandidate = false;
    DecodeResult bestResult;
    std::size_t bestFrameEndSample = 0;
    StreamingReceiverEvent bestEvent;
    const auto startSync = startSyncBits();
    const int maxSyncMismatches = std::max(2, static_cast<int>(startSync.size() / 4));
    const std::size_t latestSample = sampleCursor_ + buffer_.size();

    for (const auto& phase : phases_) {
        if (phase.bits.size() < startSync.size() + static_cast<std::size_t>(kPhysicalLengthBits)) {
            continue;
        }

        const std::size_t lastSyncStart = phase.bits.size() - startSync.size();
        for (std::size_t syncStart = 0; syncStart <= lastSyncStart; ++syncStart) {
            const int syncMismatches = bitMismatchCount(phase.bits, syncStart, startSync);
            if (syncMismatches > maxSyncMismatches) {
                continue;
            }

            const auto lengthStart = syncStart + startSync.size();
            if (lengthStart + static_cast<std::size_t>(kPhysicalLengthBits) > phase.bits.size()) {
                continue;
            }

            const auto syncSample = static_cast<std::int64_t>(
                phase.firstBitSample + syncStart * static_cast<std::size_t>(samplesPerSymbol())
            );

            StreamingReceiverEvent syncEvent;
            syncEvent.type = StreamingReceiverEventType::SyncFound;
            syncEvent.phaseOffsetSamples = phase.offsetSamples;
            syncEvent.syncSample = syncSample;
            syncEvent.syncBitIndex = static_cast<std::int32_t>(syncStart);
            syncEvent.syncMismatches = syncMismatches;
            emitEvent(syncEvent);

            const int payloadLength = decodePhysicalLengthBits(phase.bits, lengthStart);
            if (payloadLength < 0) {
                StreamingReceiverEvent lengthEvent = syncEvent;
                lengthEvent.type = StreamingReceiverEventType::PhysicalLengthInvalid;
                emitEvent(lengthEvent);
                continue;
            }

            const auto robustBits = robustFrameBitCountForPayloadLength(payloadLength);
            StreamingReceiverEvent lengthEvent = syncEvent;
            lengthEvent.type = StreamingReceiverEventType::PhysicalLengthRecovered;
            lengthEvent.payloadLength = payloadLength;
            lengthEvent.bitsExpected = static_cast<std::int32_t>(robustBits);
            emitEvent(lengthEvent);

            const auto robustStart = lengthStart + static_cast<std::size_t>(kPhysicalLengthBits);
            const auto availableRobustBits = robustStart >= phase.bits.size() ? 0 : phase.bits.size() - robustStart;
            if (availableRobustBits < robustBits) {
                StreamingReceiverEvent waitingEvent = lengthEvent;
                waitingEvent.type = StreamingReceiverEventType::FrameWaiting;
                waitingEvent.bitsAvailable = static_cast<std::int32_t>(availableRobustBits);
                waitingEvent.bitsExpected = static_cast<std::int32_t>(robustBits);
                const int bucket = static_cast<int>((availableRobustBits * 4U) / robustBits);
                emitEvent(waitingEvent, std::min(bucket, 3));
                continue;
            }

            const std::vector<std::uint8_t> candidateBits(
                phase.bits.begin() + static_cast<std::ptrdiff_t>(robustStart),
                phase.bits.begin() + static_cast<std::ptrdiff_t>(robustStart + robustBits)
            );

            auto robustResult = parseRobustFrameBits(candidateBits, logicalFrameBitCountForPayloadLength(payloadLength));
            auto candidate = robustResult.frame;
            candidate.syncIndex = static_cast<int>(robustStart);
            candidate.startOffset = phase.offsetSamples;
            candidate.offsetsTried = static_cast<std::int32_t>(phases_.size());
            candidate.confidence = meanConfidence(phase.decisions, robustStart, robustBits);

            const auto candidateEndSample = phase.firstBitSample
                + (robustStart + robustBits) * static_cast<std::size_t>(samplesPerSymbol());

            StreamingReceiverEvent frameEvent = lengthEvent;
            frameEvent.payloadLength = payloadLength;
            frameEvent.decodedLength = candidate.length;
            frameEvent.bitsAvailable = static_cast<std::int32_t>(robustBits);
            frameEvent.bitsExpected = static_cast<std::int32_t>(robustBits);
            frameEvent.crcOk = candidate.crcOk;
            frameEvent.payloadValid = candidate.payloadValid;
            frameEvent.confidence = candidate.confidence;
            frameEvent.latencySeconds = candidateEndSample >= latestSample
                ? 0.0F
                : static_cast<float>(static_cast<double>(latestSample - candidateEndSample) / config_.sampleRate);

            if (candidate.crcOk && candidate.payloadValid && candidate.length == payloadLength) {
                frameEvent.type = StreamingReceiverEventType::FrameDecoded;
                if (!hasCandidate || candidate.confidence > bestResult.confidence) {
                    bestResult = candidate;
                    bestFrameEndSample = candidateEndSample;
                    bestEvent = frameEvent;
                    hasCandidate = true;
                }
            } else {
                frameEvent.type = StreamingReceiverEventType::FrameRejected;
                emitEvent(frameEvent);
            }
        }
    }

    if (!hasCandidate) {
        return false;
    }

    result = bestResult;
    frameEndSample = bestFrameEndSample;
    emitEvent(bestEvent);
    return true;
}

void StreamingReceiver::emitEvent(const StreamingReceiverEvent& event, int bucket) {
    if (reportedEvents_.size() > 4096) {
        reportedEvents_.clear();
    }

    const EventKey key{
        event.type,
        event.phaseOffsetSamples,
        event.syncSample,
        bucket,
    };
    if (reportedEvents_.insert(key).second) {
        events_.push_back(event);
    }
}

void StreamingReceiver::resetAfterFrame(std::size_t frameEndSample) {
    if (frameEndSample > sampleCursor_) {
        const auto removable = std::min(frameEndSample - sampleCursor_, buffer_.size());
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(removable));
        sampleCursor_ += removable;
    } else {
        buffer_.clear();
    }

    reportedEvents_.clear();
    resetPhaseStates();
}

void StreamingReceiver::trimBuffer() {
    if (phases_.empty()) {
        return;
    }

    std::size_t safeCursor = sampleCursor_ + buffer_.size();
    for (const auto& phase : phases_) {
        safeCursor = std::min(safeCursor, phase.nextStartSample);
    }

    if (safeCursor > sampleCursor_) {
        const auto removable = std::min(safeCursor - sampleCursor_, buffer_.size());
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(removable));
        sampleCursor_ += removable;
    }

    const auto maxSamples = static_cast<std::size_t>(samplesPerSymbol()) * kMaxRetainedBits;
    if (buffer_.size() > maxSamples) {
        const auto excess = buffer_.size() - maxSamples;
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(excess));
        sampleCursor_ += excess;
        resetPhaseStates();
    }
}

void StreamingReceiver::trimBitBuffers() {
    for (auto& phase : phases_) {
        if (phase.bits.size() <= kMaxRetainedBits) {
            continue;
        }

        const auto excess = phase.bits.size() - kMaxRetainedBits;
        phase.bits.erase(phase.bits.begin(), phase.bits.begin() + static_cast<std::ptrdiff_t>(excess));
        phase.decisions.erase(
            phase.decisions.begin(),
            phase.decisions.begin() + static_cast<std::ptrdiff_t>(std::min(excess, phase.decisions.size()))
        );
        phase.firstBitSample += excess * static_cast<std::size_t>(samplesPerSymbol());
    }
}

}  // namespace hftext
