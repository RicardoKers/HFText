#include "hftext_modulator.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

double tonePower(const std::vector<float>& samples, int sampleRate, double frequency) {
    double sineProjection = 0.0;
    double cosineProjection = 0.0;

    for (std::size_t index = 0; index < samples.size(); ++index) {
        const double t = static_cast<double>(index) / sampleRate;
        const double phase = 2.0 * kPi * frequency * t;
        sineProjection += samples[index] * std::sin(phase);
        cosineProjection += samples[index] * std::cos(phase);
    }

    return sineProjection * sineProjection + cosineProjection * cosineProjection;
}

}  // namespace

int main() {
    auto audio = hftext::modulateBits2Fsk({0, 1, 0}, 8000, 0.1F, 1200.0F, 1600.0F, 0.8F);
    assert(audio.size() == 2400);

    audio = hftext::modulateBits2Fsk({0, 1}, 8000, 0.1F, 1200.0F, 1600.0F, 0.8F);
    const auto [minIt, maxIt] = std::minmax_element(audio.begin(), audio.end());
    assert(*minIt >= -1.0F);
    assert(*maxIt <= 1.0F);

    const auto quietAudio = hftext::modulateBits2Fsk({0, 1}, 8000, 0.1F, 1200.0F, 1600.0F, 0.2F);
    const auto loudAudio = hftext::modulateBits2Fsk({0, 1}, 8000, 0.1F, 1200.0F, 1600.0F, 0.8F);
    const auto quietPeak = std::max(std::abs(*std::min_element(quietAudio.begin(), quietAudio.end())),
                                    std::abs(*std::max_element(quietAudio.begin(), quietAudio.end())));
    const auto loudPeak = std::max(std::abs(*std::min_element(loudAudio.begin(), loudAudio.end())),
                                   std::abs(*std::max_element(loudAudio.begin(), loudAudio.end())));
    assert(loudPeak > quietPeak * 3.5F);

    const int sampleRate = 8000;
    const float symbolDuration = 0.1F;
    const int samplesPerSymbol = static_cast<int>(sampleRate * symbolDuration);
    audio = hftext::modulateBits2Fsk({0, 1}, sampleRate, symbolDuration, 1000.0F, 2000.0F, 0.8F);
    const std::vector<float> first(audio.begin(), audio.begin() + samplesPerSymbol);
    const std::vector<float> second(audio.begin() + samplesPerSymbol, audio.end());
    assert(tonePower(first, sampleRate, 1000.0) > tonePower(first, sampleRate, 2000.0));
    assert(tonePower(second, sampleRate, 2000.0) > tonePower(second, sampleRate, 1000.0));

    audio = hftext::modulateBits4Fsk({0, 0, 0, 1, 1, 0, 1, 1}, sampleRate, symbolDuration, 1000.0F, 1200.0F, 0.8F);
    assert(audio.size() == static_cast<std::size_t>(4 * samplesPerSymbol));
    for (int symbol = 0; symbol < 4; ++symbol) {
        const auto begin = audio.begin() + symbol * samplesPerSymbol;
        const auto end = begin + samplesPerSymbol;
        const std::vector<float> window(begin, end);
        const double expectedFrequency = 1000.0 + 200.0 * symbol;
        assert(tonePower(window, sampleRate, expectedFrequency) > tonePower(window, sampleRate, 1000.0 + 200.0 * ((symbol + 1) % 4)));
    }

    assert(hftext::modulateBits2Fsk({}, 8000, 0.1F, 1200.0F, 1600.0F, 0.8F).empty());

    hftext::ModemConfig config;
    config.sampleRate = 8000;
    config.symbolDurationSec = 0.1F;
    assert(hftext::modulateBits2Fsk({0, 1}, config).size() == 1600);

    bool invalidBitRejected = false;
    try {
        (void)hftext::modulateBits2Fsk({0, 2, 1});
    } catch (const std::invalid_argument&) {
        invalidBitRejected = true;
    }
    assert(invalidBitRejected);

    invalidBitRejected = false;
    try {
        (void)hftext::modulateBits4Fsk({1, 2}, 8000, 0.1F, 1000.0F, 1200.0F, 0.8F);
    } catch (const std::invalid_argument&) {
        invalidBitRejected = true;
    }
    assert(invalidBitRejected);

    for (const auto invalidConfig : {
             hftext::ModemConfig{0, 0.5F, 1200.0F, 1600.0F, 0.8F, 64, true},
             hftext::ModemConfig{8000, 0.0F, 1200.0F, 1600.0F, 0.8F, 64, true},
             hftext::ModemConfig{8000, 0.1F, 0.0F, 1600.0F, 0.8F, 64, true},
             hftext::ModemConfig{8000, 0.1F, 1200.0F, 1600.0F, 1.1F, 64, true},
         }) {
        bool rejected = false;
        try {
            (void)hftext::modulateBits2Fsk({0}, invalidConfig);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
}
