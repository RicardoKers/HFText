#include "hftext_config.h"
#include "hftext_core.h"
#include "hftext_demodulator.h"
#include "hftext_frame.h"
#include "hftext_modulator.h"
#include "hftext_result.h"

#include <stdexcept>

namespace hftext {

static_assert(ModemConfig{}.sampleRate == 48000, "unexpected default sample rate");
static_assert(ModemConfig{}.preambleBits == 64, "unexpected default preamble length");

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
    const auto bits = demodulateBits2Fsk(samples, config);
    return config.syncSearch ? parseFrameFromStream(bits) : parseFrame(bits);
}

}  // namespace hftext
