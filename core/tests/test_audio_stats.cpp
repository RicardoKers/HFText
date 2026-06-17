#include "hftext_audio_stats.h"

#include <cassert>
#include <stdexcept>
#include <vector>

int main() {
    const std::vector<float> samples{-0.1F, 0.4F, -0.99F, 1.0F, 0.97F};
    const auto stats = hftext::analyzeAudioSamples(samples);
    assert(stats.sampleCount == samples.size());
    assert(stats.peak == 1.0F);
    assert(stats.clippedSamples == 2);
    assert(hftext::audioDurationSeconds(stats.sampleCount, 10) == 0.5);
    assert(hftext::audioDurationSeconds(stats.sampleCount, 0) == 0.0);
    assert(hftext::clippingPercent(stats.clippedSamples, stats.sampleCount) == 40.0);
    assert(hftext::clippingPercent(10, 0) == 0.0);

    const auto customThreshold = hftext::analyzeAudioSamples(samples, 0.40F);
    assert(customThreshold.clippedSamples == 4);

    bool rejected = false;
    try {
        (void)hftext::analyzeAudioSamples(samples, -0.1F);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
