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

std::vector<std::uint8_t> bitsFromDecisions(const std::vector<BitDecision>& decisions) {
    std::vector<std::uint8_t> bits;
    bits.reserve(decisions.size());
    for (const auto& decision : decisions) {
        bits.push_back(decision.bit);
    }
    return bits;
}

bool preambleMatches(const std::vector<std::uint8_t>& bits, int preambleBitCount) {
    if (preambleBitCount <= 0) {
        return true;
    }
    if (bits.size() < static_cast<std::size_t>(preambleBitCount)) {
        return false;
    }

    int mismatches = 0;
    for (int index = 0; index < preambleBitCount; ++index) {
        const auto expected = static_cast<std::uint8_t>(index % 2 == 0 ? 1 : 0);
        if (bits[static_cast<std::size_t>(index)] != expected) {
            ++mismatches;
        }
    }

    return mismatches <= std::max(2, preambleBitCount / 8);
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
        sum += decisions[index].confidence;
    }
    return static_cast<float>(sum / static_cast<double>(end - start));
}

std::vector<std::uint8_t> preambleBitsFromConfig(const ModemConfig& config) {
    if (config.preambleBits < 0) {
        throw std::invalid_argument("preamble_bits must be non-negative");
    }

    if (config.preambleBits == kDefaultPreambleBits) {
        return defaultPreambleBits();
    }

    std::vector<std::uint8_t> preamble;
    preamble.reserve(static_cast<std::size_t>(config.preambleBits));
    for (std::int32_t index = 0; index < config.preambleBits; ++index) {
        preamble.push_back(static_cast<std::uint8_t>(index % 2 == 0 ? 1 : 0));
    }
    return preamble;
}

int logicalFrameBitCountForPayloadLength(int payloadLength) {
    return (kHeaderBytes + payloadByteCount(payloadLength) + kCrcBytes) * kBitsPerByte;
}

std::size_t encodedBitCountForLogicalFrameBits(int logicalFrameBitCount) {
    constexpr int kConvTailBits = 2;
    constexpr int kConvOutputBitsPerInputBit = 2;
    return static_cast<std::size_t>(logicalFrameBitCount + kConvTailBits) * kConvOutputBitsPerInputBit;
}

DecodeResult parseRobustFrameAtExpectedOffset(const std::vector<std::uint8_t>& bits, int bitOffset) {
    if (bitOffset < 0 || static_cast<std::size_t>(bitOffset) >= bits.size()) {
        DecodeResult result;
        result.error = "robust frame not found";
        return result;
    }

    const auto start = static_cast<std::size_t>(bitOffset);
    DecodeResult firstCandidate;
    bool hasCandidate = false;
    for (int payloadLength = 0; payloadLength <= kMaxPayloadSymbols; ++payloadLength) {
        const int logicalFrameBitCount = logicalFrameBitCountForPayloadLength(payloadLength);
        const std::size_t encodedBitCount = encodedBitCountForLogicalFrameBits(logicalFrameBitCount);
        if (start + encodedBitCount > bits.size()) {
            continue;
        }

        const std::vector<std::uint8_t> candidateBits(
            bits.begin() + static_cast<std::ptrdiff_t>(start),
            bits.begin() + static_cast<std::ptrdiff_t>(start + encodedBitCount)
        );
        auto robustResult = parseRobustFrameBits(candidateBits, logicalFrameBitCount);
        robustResult.frame.syncIndex = bitOffset;
        if (!hasCandidate) {
            firstCandidate = robustResult.frame;
            hasCandidate = true;
        }
        if (
            robustResult.frame.crcOk
            && robustResult.frame.payloadValid
            && robustResult.frame.length == payloadLength
        ) {
            return robustResult.frame;
        }
    }

    if (hasCandidate) {
        firstCandidate.frameDetected = false;
        firstCandidate.crcOk = false;
        firstCandidate.payloadValid = false;
        firstCandidate.text.clear();
        firstCandidate.error = "robust frame not found";
        firstCandidate.syncIndex = -1;
        return firstCandidate;
    }

    DecodeResult result;
    result.error = "robust frame not found";
    return result;
}

DecodeResult demodulateAndParse(
    const std::vector<float>& samples,
    const ModemConfig& config,
    int startOffset
) {
    const auto decisions = demodulateBitDecisions2Fsk(samples, config, startOffset);
    const auto bits = bitsFromDecisions(decisions);
    DecodeResult result;
    if (preambleMatches(bits, config.preambleBits)) {
        result = parseRobustFrameAtExpectedOffset(bits, config.preambleBits);
    } else {
        result.error = "robust frame not found";
    }
    result.startOffset = startOffset;
    result.confidence = meanConfidence(decisions, 0, decisions.size());
    return result;
}

}  // namespace

std::vector<float> modulateText(const std::string& text, const ModemConfig& config) {
    const auto preamble = preambleBitsFromConfig(config);
    const auto bits = buildRobustTransmission(text, preamble);

    return modulateBits2Fsk(bits, config);
}

DecodeResult demodulateSamples(const std::vector<float>& samples, const ModemConfig& config) {
    if (!config.syncSearch) {
        auto result = demodulateAndParse(samples, config, 0);
        result.offsetsTried = 1;
        return result;
    }

    const int symbolSamples = samplesPerSymbol(config);
    const int step = defaultOffsetStep(config);
    DecodeResult fallback;
    bool hasFallback = false;
    DecodeResult bestValid;
    bool hasBestValid = false;
    int offsetsTried = 0;

    for (int startOffset = 0; startOffset < symbolSamples; startOffset += step) {
        auto result = demodulateAndParse(samples, config, startOffset);
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
