#include "hftext_demodulator.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hftext {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

struct SymbolMetrics {
    double energy0 = 0.0;
    double energy1 = 0.0;
    double power = 0.0;
};

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

SymbolMetrics symbolMetricsWindow(
    const std::vector<float>& samples,
    std::size_t start,
    std::size_t count,
    int sampleRate,
    float frequency0Hz,
    float frequency1Hz
) {
    double inPhase0 = 0.0;
    double quadrature0 = 0.0;
    double inPhase1 = 0.0;
    double quadrature1 = 0.0;
    double power = 0.0;

    for (std::size_t index = 0; index < count; ++index) {
        const double t = static_cast<double>(index) / sampleRate;
        const double phase0 = 2.0 * kPi * frequency0Hz * t;
        const double phase1 = 2.0 * kPi * frequency1Hz * t;
        const double sample = samples[start + index];
        inPhase0 += sample * std::cos(phase0);
        quadrature0 += sample * std::sin(phase0);
        inPhase1 += sample * std::cos(phase1);
        quadrature1 += sample * std::sin(phase1);
        power += sample * sample;
    }

    return {
        inPhase0 * inPhase0 + quadrature0 * quadrature0,
        inPhase1 * inPhase1 + quadrature1 * quadrature1,
        power,
    };
}

double bitSeparation(const SymbolMetrics& metrics) {
    const double totalEnergy = metrics.energy0 + metrics.energy1;
    if (totalEnergy <= 0.0) {
        return 0.0;
    }

    return std::abs(metrics.energy1 - metrics.energy0) / totalEnergy;
}

double bitQuality(const SymbolMetrics& metrics, std::size_t count) {
    if (metrics.power <= 0.0 || count == 0) {
        return 0.0;
    }

    const double totalEnergy = metrics.energy0 + metrics.energy1;
    if (totalEnergy <= 0.0) {
        return 0.0;
    }

    const double separation = bitSeparation(metrics);
    const double coherentConcentration = totalEnergy / (metrics.power * static_cast<double>(count));
    const double coherentWeight = std::clamp(coherentConcentration / 0.05, 0.0, 1.0);
    return separation * coherentWeight;
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

std::vector<BitDecision> demodulateBitDecisions2Fsk(
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
    std::vector<BitDecision> decisions;
    decisions.reserve(symbolCount);

    for (std::size_t symbolIndex = 0; symbolIndex < symbolCount; ++symbolIndex) {
        const auto start = static_cast<std::size_t>(startOffset) + symbolIndex * static_cast<std::size_t>(symbolSamples);
        const auto count = static_cast<std::size_t>(symbolSamples);
        const auto metrics = symbolMetricsWindow(samples, start, count, sampleRate, frequency0Hz, frequency1Hz);
        const double confidence = bitSeparation(metrics);
        const double quality = bitQuality(metrics, count);
        decisions.push_back(
            BitDecision{
                static_cast<std::uint8_t>(metrics.energy1 > metrics.energy0 ? 1 : 0),
                static_cast<float>(std::clamp(confidence, 0.0, 1.0)),
                static_cast<float>(std::clamp(quality, 0.0, 1.0))
            }
        );
    }

    return decisions;
}

std::vector<BitDecision> demodulateBitDecisions2Fsk(
    const std::vector<float>& samples,
    const ModemConfig& config,
    int startOffset
) {
    return demodulateBitDecisions2Fsk(
        samples,
        config.sampleRate,
        config.symbolDurationSec,
        config.frequency0Hz,
        config.frequency1Hz,
        startOffset
    );
}

std::vector<std::uint8_t> demodulateBits2Fsk(
    const std::vector<float>& samples,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    int startOffset
) {
    const auto decisions = demodulateBitDecisions2Fsk(
        samples,
        sampleRate,
        symbolDurationSec,
        frequency0Hz,
        frequency1Hz,
        startOffset
    );
    std::vector<std::uint8_t> bits;
    bits.reserve(decisions.size());
    for (const auto& decision : decisions) {
        bits.push_back(decision.bit);
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
