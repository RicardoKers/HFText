#include "hftext_audio_stats.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hftext {

AudioStats analyzeAudioSamples(const std::vector<float>& samples, float clippingThreshold) {
    if (!std::isfinite(clippingThreshold) || clippingThreshold < 0.0F) {
        throw std::invalid_argument("clipping threshold must be non-negative");
    }

    AudioStats stats;
    stats.sampleCount = samples.size();
    for (const float sample : samples) {
        const float absValue = std::abs(sample);
        stats.peak = (std::max)(stats.peak, absValue);
        if (absValue >= clippingThreshold) {
            ++stats.clippedSamples;
        }
    }
    return stats;
}

double audioDurationSeconds(std::size_t sampleCount, int sampleRate) {
    if (sampleRate <= 0) {
        return 0.0;
    }
    return static_cast<double>(sampleCount) / static_cast<double>(sampleRate);
}

double clippingPercent(std::size_t clippedSamples, std::size_t sampleCount) {
    if (sampleCount == 0) {
        return 0.0;
    }
    return 100.0 * static_cast<double>(clippedSamples) / static_cast<double>(sampleCount);
}

}  // namespace hftext
