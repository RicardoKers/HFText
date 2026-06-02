#pragma once

#include "hftext_config.h"

#include <cstdint>
#include <vector>

namespace hftext {

double toneEnergy(const std::vector<float>& samples, int sampleRate, float frequencyHz);

std::vector<std::uint8_t> demodulateBits2Fsk(
    const std::vector<float>& samples,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    int startOffset = 0
);

std::vector<std::uint8_t> demodulateBits2Fsk(
    const std::vector<float>& samples,
    const ModemConfig& config = ModemConfig{},
    int startOffset = 0
);

}  // namespace hftext
