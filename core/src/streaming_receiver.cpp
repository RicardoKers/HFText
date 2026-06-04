#include "hftext_streaming_receiver.h"

#include "hftext_frame.h"
#include "hftext_robust.h"

#include <algorithm>
#include <cmath>
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

std::size_t packedPayloadBytes(int symbolCount) {
    if (symbolCount <= 0) {
        return 0;
    }
    return static_cast<std::size_t>((symbolCount * kBitsPerSymbol + kBitsPerByte - 1) / kBitsPerByte);
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
    const double energy0 = toneEnergyWindow(samples, start, count, config.sampleRate, config.frequency0Hz);
    const double energy1 = toneEnergyWindow(samples, start, count, config.sampleRate, config.frequency1Hz);
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
    const auto logicalBits = static_cast<std::size_t>(kSyncBits + kLengthBits + kCrcBits)
        + packedPayloadBytes(result.length) * kBitsPerByte;
    return (logicalBits + kConvTailBits) * kConvOutputBitsPerInputBit;
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

bool StreamingReceiver::findBestFrame(DecodeResult& result, std::size_t& frameEndSample) const {
    bool hasCandidate = false;
    DecodeResult bestResult;
    std::size_t bestFrameEndSample = 0;

    for (const auto& phase : phases_) {
        const auto robustResult = parseRobustFrameFromStream(phase.bits);
        if (!robustResult.frame.crcOk || !robustResult.frame.payloadValid) {
            continue;
        }

        auto candidate = robustResult.frame;
        const auto robustBits = frameBitCount(candidate);
        const auto robustStart = static_cast<std::size_t>(candidate.syncIndex);
        candidate.startOffset = phase.offsetSamples;
        candidate.offsetsTried = static_cast<std::int32_t>(phases_.size());
        candidate.confidence = meanConfidence(phase.decisions, robustStart, robustBits);

        const auto candidateEndSample = phase.firstBitSample
            + (robustStart + robustBits) * static_cast<std::size_t>(samplesPerSymbol());

        if (!hasCandidate || candidate.confidence > bestResult.confidence) {
            bestResult = candidate;
            bestFrameEndSample = candidateEndSample;
            hasCandidate = true;
        }
    }

    if (!hasCandidate) {
        return false;
    }

    result = bestResult;
    frameEndSample = bestFrameEndSample;
    return true;
}

void StreamingReceiver::resetAfterFrame(std::size_t frameEndSample) {
    if (frameEndSample > sampleCursor_) {
        const auto removable = std::min(frameEndSample - sampleCursor_, buffer_.size());
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(removable));
        sampleCursor_ += removable;
    } else {
        buffer_.clear();
    }

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
