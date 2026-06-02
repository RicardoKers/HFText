#include "hftext_modulator.h"

#include <cmath>
#include <stdexcept>

namespace hftext {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

int samplesPerSymbol(int sampleRate, float symbolDurationSec) {
    const auto samples = static_cast<int>(std::lround(static_cast<double>(sampleRate) * symbolDurationSec));
    if (samples <= 0) {
        throw std::invalid_argument("symbol duration is too short for sample_rate");
    }
    return samples;
}

void validateConfig(int sampleRate, float symbolDurationSec, float frequency0Hz, float frequency1Hz, float amplitude) {
    if (sampleRate <= 0) {
        throw std::invalid_argument("sample_rate must be positive");
    }
    if (symbolDurationSec <= 0.0F) {
        throw std::invalid_argument("symbol_duration must be positive");
    }
    if (frequency0Hz <= 0.0F || frequency1Hz <= 0.0F) {
        throw std::invalid_argument("frequencies must be positive");
    }
    if (amplitude < 0.0F || amplitude > 1.0F) {
        throw std::invalid_argument("amplitude must be between 0.0 and 1.0");
    }
}

}  // namespace

std::vector<float> modulateBits2Fsk(
    const std::vector<std::uint8_t>& bits,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    float amplitude
) {
    validateConfig(sampleRate, symbolDurationSec, frequency0Hz, frequency1Hz, amplitude);
    const int symbolSamples = samplesPerSymbol(sampleRate, symbolDurationSec);

    std::vector<float> audio;
    audio.resize(bits.size() * static_cast<std::size_t>(symbolSamples));

    double phase = 0.0;
    std::size_t offset = 0;
    for (std::uint8_t bit : bits) {
        if (bit != 0 && bit != 1) {
            throw std::invalid_argument("invalid bit");
        }

        const double frequency = bit == 0 ? frequency0Hz : frequency1Hz;
        const double phaseStep = 2.0 * kPi * frequency / sampleRate;
        for (int sample = 0; sample < symbolSamples; ++sample) {
            audio[offset + static_cast<std::size_t>(sample)] = amplitude * static_cast<float>(std::sin(phase));
            phase += phaseStep;
            if (phase >= 2.0 * kPi) {
                phase = std::fmod(phase, 2.0 * kPi);
            }
        }
        offset += static_cast<std::size_t>(symbolSamples);
    }

    return audio;
}

std::vector<float> modulateBits2Fsk(const std::vector<std::uint8_t>& bits, const ModemConfig& config) {
    return modulateBits2Fsk(
        bits,
        config.sampleRate,
        config.symbolDurationSec,
        config.frequency0Hz,
        config.frequency1Hz,
        config.amplitude
    );
}

}  // namespace hftext
