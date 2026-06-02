#pragma once

#include <cstdint>

namespace hftext {

struct ModemConfig {
    std::int32_t sampleRate = 48000;
    float symbolDurationSec = 0.5F;
    float frequency0Hz = 1200.0F;
    float frequency1Hz = 1600.0F;
    float amplitude = 0.8F;
    std::int32_t preambleBits = 64;
    bool syncSearch = true;
};

}  // namespace hftext
