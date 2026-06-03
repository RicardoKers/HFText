#include "hftext_config.h"
#include "hftext_core.h"
#include "hftext_demodulator.h"
#include "hftext_frame.h"
#include "hftext_modulator.h"
#include "hftext_result.h"

#include <algorithm>
#include <cmath>
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

DecodeResult demodulateAndParse(
    const std::vector<float>& samples,
    const ModemConfig& config,
    int startOffset
) {
    const auto bits = demodulateBits2Fsk(samples, config, startOffset);
    auto result = parseFrameFromStream(bits);
    result.startOffset = startOffset;
    return result;
}

}  // namespace

std::vector<float> modulateText(const std::string& text, const ModemConfig& config) {
    if (config.preambleBits < 0) {
        throw std::invalid_argument("preamble_bits must be non-negative");
    }

    std::vector<std::uint8_t> bits;
    if (config.preambleBits == kDefaultPreambleBits) {
        bits = buildTransmission(text);
    } else {
        std::vector<std::uint8_t> preamble;
        preamble.reserve(static_cast<std::size_t>(config.preambleBits));
        for (std::int32_t index = 0; index < config.preambleBits; ++index) {
            preamble.push_back(static_cast<std::uint8_t>(index % 2 == 0 ? 1 : 0));
        }
        bits = buildTransmission(text, preamble);
    }

    return modulateBits2Fsk(bits, config);
}

DecodeResult demodulateSamples(const std::vector<float>& samples, const ModemConfig& config) {
    if (!config.syncSearch) {
        const auto bits = demodulateBits2Fsk(samples, config);
        auto result = parseFrame(bits);
        result.startOffset = 0;
        result.offsetsTried = 1;
        return result;
    }

    const int symbolSamples = samplesPerSymbol(config);
    const int step = defaultOffsetStep(config);
    DecodeResult fallback;
    bool hasFallback = false;
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
            return result;
        }
    }

    if (hasFallback) {
        fallback.offsetsTried = offsetsTried;
        return fallback;
    }

    DecodeResult result;
    result.offsetsTried = offsetsTried;
    result.error = "sync not found";
    return result;
}

}  // namespace hftext
