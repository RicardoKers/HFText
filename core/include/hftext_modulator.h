#pragma once

#include "hftext_config.h"

#include <cstdint>
#include <vector>

namespace hftext {

std::vector<float> modulateBits2Fsk(
    const std::vector<std::uint8_t>& bits,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    float amplitude
);

std::vector<float> modulateBits2Fsk(
    const std::vector<std::uint8_t>& bits,
    const ModemConfig& config = ModemConfig{}
);

std::vector<float> modulateBits4Fsk(
    const std::vector<std::uint8_t>& bits,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    float amplitude
);

std::vector<float> modulateBits4Fsk(
    const std::vector<std::uint8_t>& bits,
    const ModemConfig& config = ModemConfig{}
);

std::vector<float> modulateBitsFsk(
    const std::vector<std::uint8_t>& bits,
    const ModemConfig& config = ModemConfig{}
);

}  // namespace hftext
