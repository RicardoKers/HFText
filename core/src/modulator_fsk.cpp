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

void validateCommonConfig(int sampleRate, float symbolDurationSec, float amplitude) {
    if (sampleRate <= 0) {
        throw std::invalid_argument("sample_rate must be positive");
    }
    if (symbolDurationSec <= 0.0F) {
        throw std::invalid_argument("symbol_duration must be positive");
    }
    if (amplitude < 0.0F || amplitude > 1.0F) {
        throw std::invalid_argument("amplitude must be between 0.0 and 1.0");
    }
}

void validateFrequencies(const ModemConfig& config) {
    if (config.frequency0Hz <= 0.0F || config.frequency1Hz <= 0.0F) {
        throw std::invalid_argument("frequencies must be positive");
    }
    if (config.frequency0Hz == config.frequency1Hz) {
        throw std::invalid_argument("frequencies must be different");
    }
    if (config.modulationMode == ModulationMode::Fsk4 && modulationToneSpacingHz(config) <= 0.0F) {
        throw std::invalid_argument("4-FSK requires f1 greater than f0");
    }
    const float nyquistHz = static_cast<float>(config.sampleRate) / 2.0F;
    for (int tone = 0; tone < toneCount(config.modulationMode); ++tone) {
        if (modulationToneFrequencyHz(config, tone) >= nyquistHz) {
            throw std::invalid_argument("tone frequencies must be below Nyquist");
        }
    }
}

std::vector<float> modulateBitsWithConfig(const std::vector<std::uint8_t>& bits, const ModemConfig& config) {
    validateCommonConfig(config.sampleRate, config.symbolDurationSec, config.amplitude);
    validateFrequencies(config);
    const int symbolSamples = samplesPerSymbol(config.sampleRate, config.symbolDurationSec);
    const int bitsPerSymbol = bitsPerModulationSymbol(config.modulationMode);
    const std::size_t physicalSymbolCount = (bits.size() + static_cast<std::size_t>(bitsPerSymbol - 1))
        / static_cast<std::size_t>(bitsPerSymbol);

    std::vector<float> audio;
    audio.resize(physicalSymbolCount * static_cast<std::size_t>(symbolSamples));

    double phase = 0.0;
    std::size_t offset = 0;
    for (std::size_t symbolIndex = 0; symbolIndex < physicalSymbolCount; ++symbolIndex) {
        int toneIndex = 0;
        for (int bitIndex = 0; bitIndex < bitsPerSymbol; ++bitIndex) {
            const std::size_t sourceIndex = symbolIndex * static_cast<std::size_t>(bitsPerSymbol)
                + static_cast<std::size_t>(bitIndex);
            const std::uint8_t bit = sourceIndex < bits.size() ? bits[sourceIndex] : 0;
            if (bit != 0 && bit != 1) {
                throw std::invalid_argument("invalid bit");
            }
            toneIndex = (toneIndex << 1) | bit;
        }

        const double frequency = modulationToneFrequencyHz(config, toneIndex);
        const double phaseStep = 2.0 * kPi * frequency / config.sampleRate;
        for (int sample = 0; sample < symbolSamples; ++sample) {
            audio[offset + static_cast<std::size_t>(sample)] = config.amplitude * static_cast<float>(std::sin(phase));
            phase += phaseStep;
            if (phase >= 2.0 * kPi) {
                phase = std::fmod(phase, 2.0 * kPi);
            }
        }
        offset += static_cast<std::size_t>(symbolSamples);
    }

    return audio;
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
    ModemConfig config;
    config.sampleRate = sampleRate;
    config.symbolDurationSec = symbolDurationSec;
    config.frequency0Hz = frequency0Hz;
    config.frequency1Hz = frequency1Hz;
    config.amplitude = amplitude;
    config.modulationMode = ModulationMode::Fsk2;
    return modulateBitsWithConfig(bits, config);
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

std::vector<float> modulateBits4Fsk(
    const std::vector<std::uint8_t>& bits,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    float amplitude
) {
    ModemConfig config;
    config.sampleRate = sampleRate;
    config.symbolDurationSec = symbolDurationSec;
    config.frequency0Hz = frequency0Hz;
    config.frequency1Hz = frequency1Hz;
    config.amplitude = amplitude;
    config.modulationMode = ModulationMode::Fsk4;
    return modulateBitsWithConfig(bits, config);
}

std::vector<float> modulateBits4Fsk(const std::vector<std::uint8_t>& bits, const ModemConfig& config) {
    ModemConfig fsk4Config = config;
    fsk4Config.modulationMode = ModulationMode::Fsk4;
    return modulateBitsWithConfig(bits, fsk4Config);
}

std::vector<float> modulateBitsFsk(const std::vector<std::uint8_t>& bits, const ModemConfig& config) {
    return modulateBitsWithConfig(bits, config);
}

}  // namespace hftext
