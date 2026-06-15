#pragma once

#include "hftext_config.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace hftext {

struct BitDecision {
    std::uint8_t bit = 0;
    float confidence = 0.0F;
    float quality = 0.0F;
};

double toneEnergy(const std::vector<float>& samples, int sampleRate, float frequencyHz);

std::vector<BitDecision> demodulateSymbolDecisionsFsk(
    const std::vector<float>& samples,
    std::size_t start,
    std::size_t count,
    const ModemConfig& config
);

std::vector<BitDecision> demodulateBitDecisionsFsk(
    const std::vector<float>& samples,
    const ModemConfig& config = ModemConfig{},
    int startOffset = 0
);

std::vector<BitDecision> demodulateBitDecisions2Fsk(
    const std::vector<float>& samples,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    int startOffset = 0
);

std::vector<BitDecision> demodulateBitDecisions2Fsk(
    const std::vector<float>& samples,
    const ModemConfig& config = ModemConfig{},
    int startOffset = 0
);

std::vector<BitDecision> demodulateBitDecisions4Fsk(
    const std::vector<float>& samples,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    int startOffset = 0
);

std::vector<BitDecision> demodulateBitDecisions4Fsk(
    const std::vector<float>& samples,
    const ModemConfig& config = ModemConfig{},
    int startOffset = 0
);

std::vector<std::uint8_t> demodulateBitsFsk(
    const std::vector<float>& samples,
    const ModemConfig& config = ModemConfig{},
    int startOffset = 0
);

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

std::vector<std::uint8_t> demodulateBits4Fsk(
    const std::vector<float>& samples,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    int startOffset = 0
);

std::vector<std::uint8_t> demodulateBits4Fsk(
    const std::vector<float>& samples,
    const ModemConfig& config = ModemConfig{},
    int startOffset = 0
);

}  // namespace hftext
