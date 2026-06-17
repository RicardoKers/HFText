#pragma once

#include <cstddef>
#include <vector>

namespace hftext {

struct AudioStats {
    std::size_t sampleCount = 0;
    float peak = 0.0F;
    std::size_t clippedSamples = 0;
};

AudioStats analyzeAudioSamples(const std::vector<float>& samples, float clippingThreshold = 0.98F);
double audioDurationSeconds(std::size_t sampleCount, int sampleRate);
double clippingPercent(std::size_t clippedSamples, std::size_t sampleCount);

}  // namespace hftext
