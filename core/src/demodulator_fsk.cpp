#include "hftext_demodulator.h"

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

void validateConfig(int sampleRate, float symbolDurationSec, float frequency0Hz, float frequency1Hz, int startOffset) {
    if (sampleRate <= 0) {
        throw std::invalid_argument("sample_rate must be positive");
    }
    if (symbolDurationSec <= 0.0F) {
        throw std::invalid_argument("symbol_duration must be positive");
    }
    if (frequency0Hz <= 0.0F || frequency1Hz <= 0.0F) {
        throw std::invalid_argument("frequencies must be positive");
    }
    if (startOffset < 0) {
        throw std::invalid_argument("start_offset must be non-negative");
    }
}

double toneEnergyWindow(
    const std::vector<float>& samples,
    std::size_t start,
    std::size_t count,
    int sampleRate,
    float frequencyHz
) {
    double inPhase = 0.0;
    double quadrature = 0.0;

    for (std::size_t index = 0; index < count; ++index) {
        const double t = static_cast<double>(index) / sampleRate;
        const double phase = 2.0 * kPi * frequencyHz * t;
        const double sample = samples[start + index];
        inPhase += sample * std::cos(phase);
        quadrature += sample * std::sin(phase);
    }

    return inPhase * inPhase + quadrature * quadrature;
}

}  // namespace

double toneEnergy(const std::vector<float>& samples, int sampleRate, float frequencyHz) {
    if (sampleRate <= 0) {
        throw std::invalid_argument("sample_rate must be positive");
    }
    if (frequencyHz <= 0.0F) {
        throw std::invalid_argument("frequency must be positive");
    }
    return toneEnergyWindow(samples, 0, samples.size(), sampleRate, frequencyHz);
}

std::vector<std::uint8_t> demodulateBits2Fsk(
    const std::vector<float>& samples,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    int startOffset
) {
    validateConfig(sampleRate, symbolDurationSec, frequency0Hz, frequency1Hz, startOffset);
    const int symbolSamples = samplesPerSymbol(sampleRate, symbolDurationSec);

    if (static_cast<std::size_t>(startOffset) >= samples.size()) {
        return {};
    }

    const auto usableSamples = samples.size() - static_cast<std::size_t>(startOffset);
    const auto symbolCount = usableSamples / static_cast<std::size_t>(symbolSamples);
    std::vector<std::uint8_t> bits;
    bits.reserve(symbolCount);

    for (std::size_t symbolIndex = 0; symbolIndex < symbolCount; ++symbolIndex) {
        const auto start = static_cast<std::size_t>(startOffset) + symbolIndex * static_cast<std::size_t>(symbolSamples);
        const auto count = static_cast<std::size_t>(symbolSamples);
        const double energy0 = toneEnergyWindow(samples, start, count, sampleRate, frequency0Hz);
        const double energy1 = toneEnergyWindow(samples, start, count, sampleRate, frequency1Hz);
        bits.push_back(static_cast<std::uint8_t>(energy1 > energy0 ? 1 : 0));
    }

    return bits;
}

std::vector<std::uint8_t> demodulateBits2Fsk(
    const std::vector<float>& samples,
    const ModemConfig& config,
    int startOffset
) {
    return demodulateBits2Fsk(
        samples,
        config.sampleRate,
        config.symbolDurationSec,
        config.frequency0Hz,
        config.frequency1Hz,
        startOffset
    );
}

}  // namespace hftext
