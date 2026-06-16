#include "hftext_config.h"
#include "hftext_core.h"
#include "hftext_demodulator.h"
#include "hftext_encoder.h"
#include "hftext_frame.h"
#include "hftext_modulator.h"
#include "hftext_robust.h"
#include "hftext_result.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace hftext {

static_assert(ModemConfig{}.sampleRate == 48000, "unexpected default sample rate");
static_assert(ModemConfig{}.preambleBits == 64, "unexpected default preamble length");

namespace {

int samplesPerSymbol(const ModemConfig& config) {
    const auto samples = static_cast<int>(std::lround(static_cast<double>(config.sampleRate) * config.symbolDurationSec));
    if (samples <= 0) {
        throw std::invalid_argument("symbol duration is too short for sample_rate");
    }
    return samples;
}

int defaultOffsetStep(const ModemConfig& config) {
    return std::max(1, samplesPerSymbol(config) / 20);
}

struct ReceiverSearchVariant {
    float symbolDurationScale = 1.0F;
    float frequencyScale = 1.0F;
    float frequencyOffsetHz = 0.0F;
};

std::vector<ReceiverSearchVariant> receiverSearchVariants() {
    return {
        {1.0F, 1.0F, 0.0F},
        {1.0F, 1.0F, 5.0F},
        {1.0F, 1.0F, 7.5F},
        {1.0F, 1.0F, 10.0F},
        {1.0F, 1.0F, -5.0F},
        {1.0F, 1.0F, -7.5F},
        {1.0F, 1.0F, -10.0F},
        {1.0F, 1.0F, 15.0F},
        {1.0F, 1.0F, -15.0F},
        {0.9975F, 1.0F / 0.9975F, 0.0F},
        {1.0025F, 1.0F / 1.0025F, 0.0F},
        {0.995F, 1.0F / 0.995F, 0.0F},
        {1.005F, 1.0F / 1.005F, 0.0F},
        {0.99F, 1.0F / 0.99F, 0.0F},
        {1.01F, 1.0F / 1.01F, 0.0F},
        {0.995F, 1.0F, 0.0F},
        {1.005F, 1.0F, 0.0F},
        {0.99F, 1.0F, 0.0F},
        {1.01F, 1.0F, 0.0F},
    };
}

std::vector<std::uint8_t> bitsFromDecisions(const std::vector<BitDecision>& decisions) {
    std::vector<std::uint8_t> bits;
    bits.reserve(decisions.size());
    for (const auto& decision : decisions) {
        bits.push_back(decision.bit);
    }
    return bits;
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

float meanConfidence(const std::vector<BitDecision>& decisions, std::size_t start, std::size_t count) {
    if (decisions.empty() || start >= decisions.size() || count == 0) {
        return 0.0F;
    }

    const std::size_t end = std::min(decisions.size(), start + count);
    if (start >= end) {
        return 0.0F;
    }

    double sum = 0.0;
    for (std::size_t index = start; index < end; ++index) {
        sum += decisions[index].quality;
    }
    return static_cast<float>(sum / static_cast<double>(end - start));
}

std::vector<std::uint8_t> preambleBitsFromConfig(const ModemConfig& config) {
    if (config.preambleBits < 0) {
        throw std::invalid_argument("preamble_bits must be non-negative");
    }

    if (config.modulationMode == ModulationMode::Fsk2 && config.preambleBits == kDefaultPreambleBits) {
        return defaultPreambleBits();
    }

    std::vector<std::uint8_t> preamble;
    preamble.reserve(static_cast<std::size_t>(config.preambleBits));
    if (toneCount(config.modulationMode) > 2) {
        const int bitsPerTone = bitsPerModulationSymbol(config.modulationMode);
        const int tones = toneCount(config.modulationMode);
        for (std::int32_t index = 0; index < config.preambleBits; ++index) {
            const auto tone = static_cast<std::uint8_t>((index / bitsPerTone) % tones);
            const int bitIndex = bitsPerTone - 1 - (index % bitsPerTone);
            preamble.push_back(static_cast<std::uint8_t>((tone >> bitIndex) & 0x01U));
        }
    } else {
        for (std::int32_t index = 0; index < config.preambleBits; ++index) {
            preamble.push_back(static_cast<std::uint8_t>(index % 2 == 0 ? 1 : 0));
        }
    }
    return preamble;
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
    const auto requiredBits = static_cast<std::size_t>(kBitsPerByte * kPhysicalLengthRepeat);
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

int preambleScore(
    const std::vector<std::uint8_t>& bits,
    std::size_t syncStart,
    const std::vector<std::uint8_t>& expectedPreamble
) {
    if (expectedPreamble.empty()) {
        return 0;
    }

    const std::size_t targetBits = std::min<std::size_t>(32, expectedPreamble.size());
    const std::size_t availableBits = std::min(syncStart, targetBits);
    const std::size_t preambleStart = syncStart - availableBits;
    const std::size_t expectedStart = expectedPreamble.size() - availableBits;

    int mismatches = static_cast<int>(targetBits - availableBits);
    for (std::size_t index = 0; index < availableBits; ++index) {
        const auto bit = bits[preambleStart + index];
        if (bit != expectedPreamble[expectedStart + index]) {
            ++mismatches;
        }
    }

    return mismatches;
}

struct StartSyncCandidate {
    int frameStart = 0;
    int payloadLength = 0;
};

std::vector<StartSyncCandidate> findStartSyncFrameStarts(
    const std::vector<BitDecision>& decisions,
    const ModemConfig& config
) {
    const auto bits = bitsFromDecisions(decisions);
    const auto startSync = startSyncBits();
    if (bits.size() < startSync.size()) {
        return {};
    }

    struct Candidate {
        double score = 0.0;
        int syncMismatches = 0;
        int frameStart = 0;
        int payloadLength = 0;
    };
    std::vector<Candidate> scoredStarts;
    const std::size_t lastStart = bits.size() - startSync.size();
    const int maxSyncMismatches = std::max(2, static_cast<int>(startSync.size() / 4));
    const auto expectedPreamble = preambleBitsFromConfig(config);
    for (std::size_t syncStart = 0; syncStart <= lastStart; ++syncStart) {
        const int mismatches = bitMismatchCount(bits, syncStart, startSync);
        if (!acceptsStartSyncCandidate(bits, decisions, syncStart, startSync, maxSyncMismatches)) {
            continue;
        }

        const auto lengthStart = syncStart + startSync.size();
        const int payloadLength = decodePhysicalLengthDecisions(decisions, lengthStart);
        if (payloadLength < 0) {
            continue;
        }

        const int candidatePreambleScore = preambleScore(bits, syncStart, expectedPreamble);
        scoredStarts.push_back({
            weightedMismatchCount(decisions, syncStart, startSync) + candidatePreambleScore,
            mismatches,
            static_cast<int>(lengthStart + static_cast<std::size_t>(kBitsPerByte * kPhysicalLengthRepeat)),
            payloadLength
        });
    }

    std::sort(scoredStarts.begin(), scoredStarts.end(), [](const Candidate& left, const Candidate& right) {
        if (left.score != right.score) {
            return left.score < right.score;
        }
        if (left.syncMismatches != right.syncMismatches) {
            return left.syncMismatches < right.syncMismatches;
        }
        return left.frameStart < right.frameStart;
    });

    std::vector<StartSyncCandidate> frameStarts;
    frameStarts.reserve(scoredStarts.size());
    for (const auto& candidate : scoredStarts) {
        const int frameStart = candidate.frameStart;
        const bool nearExisting = std::any_of(frameStarts.begin(), frameStarts.end(), [frameStart](const StartSyncCandidate& existing) {
            return std::abs(existing.frameStart - frameStart) <= 2;
        });
        if (!nearExisting) {
            frameStarts.push_back({frameStart, candidate.payloadLength});
        }
        if (frameStarts.size() >= 64) {
            break;
        }
    }
    return frameStarts;
}

int logicalFrameBitCountForPayloadLength(int payloadLength) {
    return (kHeaderBytes + payloadByteCount(payloadLength) + kCrcBytes) * kBitsPerByte;
}

std::size_t encodedBitCountForLogicalFrameBits(int logicalFrameBitCount) {
    constexpr int kConvTailBits = 2;
    constexpr int kConvOutputBitsPerInputBit = 2;
    return static_cast<std::size_t>(logicalFrameBitCount + kConvTailBits) * kConvOutputBitsPerInputBit;
}

DecodeResult parseRobustFrameAtExpectedOffset(
    const std::vector<BitDecision>& decisions,
    int bitOffset,
    int payloadLength
) {
    if (bitOffset < 0 || static_cast<std::size_t>(bitOffset) >= decisions.size()) {
        DecodeResult result;
        result.error = "robust frame not found";
        return result;
    }

    const auto start = static_cast<std::size_t>(bitOffset);
    if (payloadLength < 0 || payloadLength > kMaxPayloadSymbols) {
        DecodeResult result;
        result.error = "robust frame not found";
        return result;
    }

    const int logicalFrameBitCount = logicalFrameBitCountForPayloadLength(payloadLength);
    const std::size_t encodedBitCount = encodedBitCountForLogicalFrameBits(logicalFrameBitCount);
    if (start + encodedBitCount > decisions.size()) {
        DecodeResult result;
        result.error = "robust frame not found";
        return result;
    }

    const auto candidateBits = softBitsFromDecisions(decisions, start, encodedBitCount);
    auto robustResult = parseRobustFrameSoftBits(candidateBits, logicalFrameBitCount);
    robustResult.frame.syncIndex = bitOffset;
    if (
        robustResult.frame.crcOk
        && robustResult.frame.payloadValid
        && robustResult.frame.length == payloadLength
    ) {
        return robustResult.frame;
    }

    robustResult.frame.frameDetected = false;
    robustResult.frame.crcOk = false;
    robustResult.frame.payloadValid = false;
    robustResult.frame.text.clear();
    robustResult.frame.error = "robust frame not found";
    robustResult.frame.syncIndex = -1;
    return robustResult.frame;
}

DecodeResult demodulateAndParse(
    const std::vector<float>& samples,
    const ModemConfig& config,
    int startOffset
) {
    const auto decisions = demodulateBitDecisionsFsk(samples, config, startOffset);
    DecodeResult result;
    for (const auto& candidate : findStartSyncFrameStarts(decisions, config)) {
        result = parseRobustFrameAtExpectedOffset(decisions, candidate.frameStart, candidate.payloadLength);
        if (result.crcOk && result.payloadValid) {
            result.startOffset = startOffset;
            result.confidence = meanConfidence(decisions, 0, decisions.size());
            return result;
        }
    }
    result.error = "robust frame not found";
    result.startOffset = startOffset;
    result.confidence = meanConfidence(decisions, 0, decisions.size());
    return result;
}

}  // namespace

std::vector<float> modulateText(const std::string& text, const ModemConfig& config) {
    const auto preamble = preambleBitsFromConfig(config);
    const auto bits = buildRobustTransmission(text, preamble);

    return modulateBitsFsk(bits, config);
}

DecodeResult demodulateSamples(const std::vector<float>& samples, const ModemConfig& config) {
    if (!config.syncSearch) {
        auto result = demodulateAndParse(samples, config, 0);
        result.offsetsTried = 1;
        return result;
    }

    DecodeResult fallback;
    bool hasFallback = false;
    DecodeResult bestValid;
    bool hasBestValid = false;
    int offsetsTried = 0;

    for (const auto& variant : receiverSearchVariants()) {
        ModemConfig candidateConfig = config;
        candidateConfig.symbolDurationSec = config.symbolDurationSec * variant.symbolDurationScale;
        candidateConfig.frequency0Hz = config.frequency0Hz * variant.frequencyScale + variant.frequencyOffsetHz;
        candidateConfig.frequency1Hz = config.frequency1Hz * variant.frequencyScale + variant.frequencyOffsetHz;

        const int symbolSamples = samplesPerSymbol(candidateConfig);
        const int step = defaultOffsetStep(candidateConfig);
        for (int startOffset = 0; startOffset < symbolSamples; startOffset += step) {
            auto result = demodulateAndParse(samples, candidateConfig, startOffset);
            ++offsetsTried;
            result.offsetsTried = offsetsTried;

            if (!hasFallback || (result.frameDetected && !fallback.frameDetected)) {
                fallback = result;
                hasFallback = true;
            }
            if (result.crcOk && result.payloadValid) {
                if (!hasBestValid || result.confidence > bestValid.confidence) {
                    bestValid = result;
                    hasBestValid = true;
                }
            }
        }

        if (hasBestValid) {
            bestValid.offsetsTried = offsetsTried;
            return bestValid;
        }
    }

    if (hasBestValid) {
        bestValid.offsetsTried = offsetsTried;
        return bestValid;
    }

    if (hasFallback) {
        fallback.offsetsTried = offsetsTried;
        return fallback;
    }

    DecodeResult result;
    result.offsetsTried = offsetsTried;
    result.error = "robust frame not found";
    return result;
}

}  // namespace hftext
