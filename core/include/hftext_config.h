#pragma once

#include <cstdint>

namespace hftext {

enum class ModulationMode {
    Fsk2 = 2,
    Fsk4 = 4,
    Fsk8 = 8,
};

struct ModemConfig {
    std::int32_t sampleRate = 48000;
    float symbolDurationSec = 0.5F;
    float frequency0Hz = 1200.0F;
    float frequency1Hz = 1600.0F;
    float amplitude = 0.8F;
    std::int32_t preambleBits = 64;
    bool syncSearch = true;
    ModulationMode modulationMode = ModulationMode::Fsk2;
};

inline int bitsPerModulationSymbol(ModulationMode mode) {
    if (mode == ModulationMode::Fsk8) {
        return 3;
    }
    if (mode == ModulationMode::Fsk4) {
        return 2;
    }
    return 1;
}

inline int toneCount(ModulationMode mode) {
    return 1 << bitsPerModulationSymbol(mode);
}

inline float modulationToneSpacingHz(const ModemConfig& config) {
    return config.frequency1Hz - config.frequency0Hz;
}

inline float modulationToneFrequencyHz(const ModemConfig& config, int toneIndex) {
    if (toneCount(config.modulationMode) > 2) {
        return config.frequency0Hz + modulationToneSpacingHz(config) * static_cast<float>(toneIndex);
    }
    return toneIndex == 0 ? config.frequency0Hz : config.frequency1Hz;
}

inline float highestModulationToneHz(const ModemConfig& config) {
    if (config.modulationMode == ModulationMode::Fsk2) {
        return config.frequency0Hz > config.frequency1Hz ? config.frequency0Hz : config.frequency1Hz;
    }
    return modulationToneFrequencyHz(config, toneCount(config.modulationMode) - 1);
}

}  // namespace hftext
