#include "hftext_streaming_receiver.h"

#include "hftext_encoder.h"
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
constexpr std::size_t kMaxEventsPerBatch = 512;
constexpr int kPhaseDivisions = 20;
constexpr int kPhysicalLengthBits = kBitsPerByte * kPhysicalLengthRepeat;
constexpr float kStreamingFrequencyOffsetsHz[] = {
    0.0F,
    5.0F,
    -5.0F,
    7.5F,
    -7.5F,
    10.0F,
    -10.0F,
    15.0F,
    -15.0F,
};
constexpr float kFastStreamingFrequencyOffsetsHz[] = {
    0.0F,
    7.5F,
    -7.5F,
    15.0F,
    -15.0F,
};
constexpr float kLong8FskStreamingFrequencyOffsetsHz[] = {
    0.0F,
    5.0F,
    -5.0F,
    10.0F,
    -10.0F,
    15.0F,
    -15.0F,
};

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

double weightedMismatchCount(
    const std::vector<BitDecision>& decisions,
    std::size_t start,
    const std::vector<std::uint8_t>& pattern
) {
    if (start + pattern.size() > decisions.size()) {
        return static_cast<double>(pattern.size());
    }

    double mismatches = 0.0;
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        if (decisions[start + index].bit != pattern[index]) {
            mismatches += decisions[start + index].quality;
        }
    }
    return mismatches;
}

bool acceptsStartSyncCandidate(
    const std::vector<std::uint8_t>& bits,
    const std::vector<BitDecision>& decisions,
    std::size_t start,
    const std::vector<std::uint8_t>& pattern,
    int maxHardMismatches
) {
    const int hardMismatches = bitMismatchCount(bits, start, pattern);
    if (hardMismatches <= maxHardMismatches) {
        return true;
    }

    const int softHardLimit = maxHardMismatches + std::max(2, static_cast<int>(pattern.size() / 8));
    if (hardMismatches > softHardLimit) {
        return false;
    }

    return weightedMismatchCount(decisions, start, pattern) <= static_cast<double>(maxHardMismatches);
}

int decodePhysicalLengthDecisions(const std::vector<BitDecision>& decisions, std::size_t start) {
    const auto requiredBits = static_cast<std::size_t>(kPhysicalLengthBits);
    if (start + requiredBits > decisions.size()) {
        return -1;
    }

    int value = 0;
    for (int bitIndex = 0; bitIndex < kBitsPerByte; ++bitIndex) {
        int hardOnes = 0;
        double score = 0.0;
        for (int repeat = 0; repeat < kPhysicalLengthRepeat; ++repeat) {
            const auto& decision = decisions[start + static_cast<std::size_t>(repeat * kBitsPerByte + bitIndex)];
            hardOnes += decision.bit;
            score += decision.bit == 1 ? decision.quality : -decision.quality;
        }

        const auto bit = static_cast<std::uint8_t>(
            score > 0.0 || (score == 0.0 && hardOnes >= 2) ? 1 : 0
        );
        value = (value << 1) | bit;
    }

    if ((value & 0x80) != 0 || value > kMaxPayloadSymbols) {
        return -1;
    }
    return value;
}

int logicalFrameBitCountForPayloadLength(int payloadLength) {
    return (kHeaderBytes + payloadByteCount(payloadLength) + kCrcBytes) * kBitsPerByte;
}

std::size_t robustFrameBitCountForPayloadLength(int payloadLength) {
    const int logicalBits = logicalFrameBitCountForPayloadLength(payloadLength);
    return static_cast<std::size_t>(logicalBits + kConvTailBits) * kConvOutputBitsPerInputBit;
}

bool isValidTonePair(const ModemConfig& config) {
    const float nyquistHz = static_cast<float>(config.sampleRate) / 2.0F;
    if (config.frequency0Hz <= 0.0F || config.frequency1Hz <= 0.0F || config.frequency0Hz == config.frequency1Hz) {
        return false;
    }
    if (toneCount(config.modulationMode) > 2 && modulationToneSpacingHz(config) <= 0.0F) {
        return false;
    }
    return highestModulationToneHz(config) < nyquistHz;
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
        sum += decisions[index].quality;
    }

    return static_cast<float>(sum / static_cast<double>(end - start));
}

std::vector<SoftBitDecision> softBitsFromDecisions(
    const std::vector<BitDecision>& decisions,
    std::size_t start,
    std::size_t count
) {
    std::vector<SoftBitDecision> softBits;
    softBits.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto& decision = decisions[start + index];
        softBits.push_back({decision.bit, decision.confidence});
    }
    return softBits;
}

const float* frequencyOffsetsForConfig(const ModemConfig& config, std::size_t& count) {
    if (config.symbolDurationSec <= 0.15F) {
        count = sizeof(kFastStreamingFrequencyOffsetsHz) / sizeof(kFastStreamingFrequencyOffsetsHz[0]);
        return kFastStreamingFrequencyOffsetsHz;
    }

    if (config.modulationMode == ModulationMode::Fsk8) {
        count = sizeof(kLong8FskStreamingFrequencyOffsetsHz) / sizeof(kLong8FskStreamingFrequencyOffsetsHz[0]);
        return kLong8FskStreamingFrequencyOffsetsHz;
    }

    count = sizeof(kStreamingFrequencyOffsetsHz) / sizeof(kStreamingFrequencyOffsetsHz[0]);
    return kStreamingFrequencyOffsetsHz;
}

int phaseDivisionsForConfig(const ModemConfig& config) {
    if (config.symbolDurationSec <= 0.15F) {
        return 10;
    }
    if (config.modulationMode == ModulationMode::Fsk8) {
        return 8;
    }
    if (config.modulationMode == ModulationMode::Fsk4) {
        return 12;
    }
    return kPhaseDivisions;
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
    if (syncBitIndex != other.syncBitIndex) {
        return syncBitIndex < other.syncBitIndex;
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
    return samplesPerSymbol(config_);
}

int StreamingReceiver::samplesPerSymbol(const ModemConfig& config) const {
    const auto samples = static_cast<int>(
        std::lround(static_cast<double>(config.sampleRate) * config.symbolDurationSec)
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
    const int phaseDivisions = phaseDivisionsForConfig(config_);
    const int step = std::max(1, symbolSamples / phaseDivisions);

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
    const auto offsets = phaseOffsets();
    std::size_t frequencyOffsetCount = 0;
    const float* frequencyOffsets = frequencyOffsetsForConfig(config_, frequencyOffsetCount);
    for (std::size_t frequencyOffsetIndex = 0; frequencyOffsetIndex < frequencyOffsetCount; ++frequencyOffsetIndex) {
        const float frequencyOffsetHz = frequencyOffsets[frequencyOffsetIndex];
        ModemConfig phaseConfig = config_;
        phaseConfig.frequency0Hz += frequencyOffsetHz;
        phaseConfig.frequency1Hz += frequencyOffsetHz;
        if (!isValidTonePair(phaseConfig)) {
            continue;
        }

        for (const int offset : offsets) {
            PhaseState phase;
            phase.offsetSamples = offset;
            phase.frequencyOffsetHz = frequencyOffsetHz;
            phase.config = phaseConfig;
            phase.nextStartSample = sampleCursor_ + static_cast<std::size_t>(offset);
            phase.firstBitSample = phase.nextStartSample;
            phases_.push_back(std::move(phase));
        }
    }
}

void StreamingReceiver::processPhase(PhaseState& phase) {
    const std::size_t symbolSamples = static_cast<std::size_t>(samplesPerSymbol(phase.config));
    const std::size_t sampleEnd = sampleCursor_ + buffer_.size();

    if (phase.nextStartSample < sampleCursor_) {
        phase.nextStartSample = sampleCursor_ + static_cast<std::size_t>(phase.offsetSamples);
        phase.firstBitSample = phase.nextStartSample;
        phase.decisions.clear();
        phase.bits.clear();
        phase.rejectedSyncBitKeys.clear();
    }

    while (phase.nextStartSample + symbolSamples <= sampleEnd) {
        const auto localStart = phase.nextStartSample - sampleCursor_;
        if (phase.bits.empty()) {
            phase.firstBitSample = phase.nextStartSample;
        }

        const auto decisions = demodulateSymbolDecisionsFsk(buffer_, localStart, symbolSamples, phase.config);
        for (const auto& decision : decisions) {
            phase.decisions.push_back(decision);
            phase.bits.push_back(decision.bit);
        }
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

    for (auto& phase : phases_) {
        if (phase.bits.size() < startSync.size() + static_cast<std::size_t>(kPhysicalLengthBits)) {
            continue;
        }

        const std::size_t lastSyncStart = phase.bits.size() - startSync.size();
        const auto bitsPerAudioSymbol = static_cast<std::size_t>(bitsPerModulationSymbol(phase.config.modulationMode));
        const auto symbolSamples = static_cast<std::size_t>(samplesPerSymbol(phase.config));
        for (std::size_t syncStart = 0; syncStart <= lastSyncStart; ++syncStart) {
            const int syncMismatches = bitMismatchCount(phase.bits, syncStart, startSync);
            if (!acceptsStartSyncCandidate(phase.bits, phase.decisions, syncStart, startSync, maxSyncMismatches)) {
                continue;
            }

            const auto syncSample = static_cast<std::int64_t>(
                phase.firstBitSample + (syncStart / bitsPerAudioSymbol) * symbolSamples
            );
            const auto syncBitKey = static_cast<std::int64_t>(
                syncSample * static_cast<std::int64_t>(bitsPerAudioSymbol)
                    + static_cast<std::int64_t>(syncStart % bitsPerAudioSymbol)
            );
            if (phase.rejectedSyncBitKeys.find(syncBitKey) != phase.rejectedSyncBitKeys.end()) {
                continue;
            }

            const auto lengthStart = syncStart + startSync.size();
            if (lengthStart + static_cast<std::size_t>(kPhysicalLengthBits) > phase.bits.size()) {
                continue;
            }

            StreamingReceiverEvent syncEvent;
            syncEvent.type = StreamingReceiverEventType::SyncFound;
            syncEvent.phaseOffsetSamples = phase.offsetSamples;
            syncEvent.syncSample = syncSample;
            syncEvent.syncBitIndex = static_cast<std::int32_t>(syncStart);
            syncEvent.syncMismatches = syncMismatches;
            emitEvent(syncEvent);

            const int payloadLength = decodePhysicalLengthDecisions(phase.decisions, lengthStart);
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

            const auto candidateBits = softBitsFromDecisions(phase.decisions, robustStart, robustBits);
            auto robustResult = parseRobustFrameSoftBits(
                candidateBits,
                logicalFrameBitCountForPayloadLength(payloadLength)
            );
            auto candidate = robustResult.frame;
            candidate.syncIndex = static_cast<int>(robustStart);
            candidate.startOffset = phase.offsetSamples;
            candidate.offsetsTried = static_cast<std::int32_t>(phases_.size());
            candidate.confidence = meanConfidence(phase.decisions, robustStart, robustBits);

            const auto candidateEndSample = phase.firstBitSample
                + ((robustStart + robustBits + bitsPerAudioSymbol - 1U) / bitsPerAudioSymbol) * symbolSamples;

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
                phase.rejectedSyncBitKeys.insert(syncBitKey);
                if (phase.rejectedSyncBitKeys.size() > 512) {
                    phase.rejectedSyncBitKeys.clear();
                }
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
        event.syncBitIndex,
        bucket,
    };
    if (reportedEvents_.insert(key).second) {
        if (events_.size() < kMaxEventsPerBatch) {
            events_.push_back(event);
        }
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
        const auto bitsPerAudioSymbol = static_cast<std::size_t>(bitsPerModulationSymbol(phase.config.modulationMode));
        const auto roundedExcess = std::min(
            phase.bits.size(),
            ((excess + bitsPerAudioSymbol - 1U) / bitsPerAudioSymbol) * bitsPerAudioSymbol
        );
        phase.bits.erase(phase.bits.begin(), phase.bits.begin() + static_cast<std::ptrdiff_t>(roundedExcess));
        phase.decisions.erase(
            phase.decisions.begin(),
            phase.decisions.begin() + static_cast<std::ptrdiff_t>(std::min(roundedExcess, phase.decisions.size()))
        );
        phase.firstBitSample += (roundedExcess / bitsPerAudioSymbol) * static_cast<std::size_t>(samplesPerSymbol(phase.config));
    }
}

}  // namespace hftext
