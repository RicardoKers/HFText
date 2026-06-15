#include "hftext_demodulator.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace hftext {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

struct SymbolMetrics {
    std::vector<double> energies;
    double power = 0.0;
};

int samplesPerSymbol(int sampleRate, float symbolDurationSec) {
    const auto samples = static_cast<int>(std::lround(static_cast<double>(sampleRate) * symbolDurationSec));
    if (samples <= 0) {
        throw std::invalid_argument("symbol duration is too short for sample_rate");
    }
    return samples;
}

void validateConfig(const ModemConfig& config, int startOffset) {
    if (config.sampleRate <= 0) {
        throw std::invalid_argument("sample_rate must be positive");
    }
    if (config.symbolDurationSec <= 0.0F) {
        throw std::invalid_argument("symbol_duration must be positive");
    }
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
    const ModemConfig& config
) {
    const int tones = toneCount(config.modulationMode);
    std::vector<double> inPhase(static_cast<std::size_t>(tones), 0.0);
    std::vector<double> quadrature(static_cast<std::size_t>(tones), 0.0);
    double power = 0.0;

    for (std::size_t index = 0; index < count; ++index) {
        const double t = static_cast<double>(index) / config.sampleRate;
        const double sample = samples[start + index];
        for (int tone = 0; tone < tones; ++tone) {
            const double phase = 2.0 * kPi * modulationToneFrequencyHz(config, tone) * t;
            inPhase[static_cast<std::size_t>(tone)] += sample * std::cos(phase);
            quadrature[static_cast<std::size_t>(tone)] += sample * std::sin(phase);
        }
        power += sample * sample;
    }

    std::vector<double> energies;
    energies.reserve(static_cast<std::size_t>(tones));
    for (int tone = 0; tone < tones; ++tone) {
        const auto index = static_cast<std::size_t>(tone);
        energies.push_back(inPhase[index] * inPhase[index] + quadrature[index] * quadrature[index]);
    }

    return {std::move(energies), power};
}

std::size_t bestToneIndex(const SymbolMetrics& metrics) {
    if (metrics.energies.empty()) {
        return 0;
    }
    return static_cast<std::size_t>(
        std::distance(metrics.energies.begin(), std::max_element(metrics.energies.begin(), metrics.energies.end()))
    );
}

double totalToneEnergy(const SymbolMetrics& metrics) {
    double total = 0.0;
    for (double energy : metrics.energies) {
        total += energy;
    }
    return total;
}

double symbolSeparation(const SymbolMetrics& metrics) {
    if (metrics.energies.empty()) {
        return 0.0;
    }

    double best = 0.0;
    double second = 0.0;
    for (double energy : metrics.energies) {
        if (energy >= best) {
            second = best;
            best = energy;
        } else if (energy > second) {
            second = energy;
        }
    }

    const double topEnergy = best + second;
    if (topEnergy <= 0.0) {
        return 0.0;
    }
    return (best - second) / topEnergy;
}

double symbolQuality(const SymbolMetrics& metrics, std::size_t count) {
    if (metrics.power <= 0.0 || count == 0) {
        return 0.0;
    }

    const double totalEnergy = totalToneEnergy(metrics);
    if (totalEnergy <= 0.0) {
        return 0.0;
    }

    const double separation = symbolSeparation(metrics);
    const double coherentConcentration = totalEnergy / (metrics.power * static_cast<double>(count));
    const double coherentWeight = std::clamp(coherentConcentration / 0.05, 0.0, 1.0);
    return separation * coherentWeight;
}

std::vector<BitDecision> decisionsFromMetrics(const SymbolMetrics& metrics, const ModemConfig& config, std::size_t count) {
    const auto tone = static_cast<std::uint8_t>(bestToneIndex(metrics));
    const double confidence = symbolSeparation(metrics);
    const double quality = symbolQuality(metrics, count);
    const int bitsPerSymbol = bitsPerModulationSymbol(config.modulationMode);

    std::vector<BitDecision> decisions;
    decisions.reserve(static_cast<std::size_t>(bitsPerSymbol));
    for (int bitIndex = bitsPerSymbol - 1; bitIndex >= 0; --bitIndex) {
        decisions.push_back(
            BitDecision{
                static_cast<std::uint8_t>((tone >> bitIndex) & 0x01U),
                static_cast<float>(std::clamp(confidence, 0.0, 1.0)),
                static_cast<float>(std::clamp(quality, 0.0, 1.0))
            }
        );
    }
    return decisions;
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

std::vector<BitDecision> demodulateSymbolDecisionsFsk(
    const std::vector<float>& samples,
    std::size_t start,
    std::size_t count,
    const ModemConfig& config
) {
    validateConfig(config, 0);
    if (start > samples.size() || start + count > samples.size()) {
        throw std::invalid_argument("symbol window is outside samples");
    }
    const auto metrics = symbolMetricsWindow(samples, start, count, config);
    return decisionsFromMetrics(metrics, config, count);
}

std::vector<BitDecision> demodulateBitDecisionsFsk(
    const std::vector<float>& samples,
    const ModemConfig& config,
    int startOffset
) {
    validateConfig(config, startOffset);
    const int symbolSamples = samplesPerSymbol(config.sampleRate, config.symbolDurationSec);

    if (static_cast<std::size_t>(startOffset) >= samples.size()) {
        return {};
    }

    const auto usableSamples = samples.size() - static_cast<std::size_t>(startOffset);
    const auto symbolCount = usableSamples / static_cast<std::size_t>(symbolSamples);
    std::vector<BitDecision> decisions;
    decisions.reserve(symbolCount * static_cast<std::size_t>(bitsPerModulationSymbol(config.modulationMode)));

    for (std::size_t symbolIndex = 0; symbolIndex < symbolCount; ++symbolIndex) {
        const auto start = static_cast<std::size_t>(startOffset) + symbolIndex * static_cast<std::size_t>(symbolSamples);
        const auto count = static_cast<std::size_t>(symbolSamples);
        auto symbolDecisions = demodulateSymbolDecisionsFsk(samples, start, count, config);
        decisions.insert(decisions.end(), symbolDecisions.begin(), symbolDecisions.end());
    }

    return decisions;
}

std::vector<BitDecision> demodulateBitDecisions2Fsk(
    const std::vector<float>& samples,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    int startOffset
) {
    ModemConfig config;
    config.sampleRate = sampleRate;
    config.symbolDurationSec = symbolDurationSec;
    config.frequency0Hz = frequency0Hz;
    config.frequency1Hz = frequency1Hz;
    config.modulationMode = ModulationMode::Fsk2;
    return demodulateBitDecisionsFsk(samples, config, startOffset);
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

std::vector<BitDecision> demodulateBitDecisions4Fsk(
    const std::vector<float>& samples,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    int startOffset
) {
    ModemConfig config;
    config.sampleRate = sampleRate;
    config.symbolDurationSec = symbolDurationSec;
    config.frequency0Hz = frequency0Hz;
    config.frequency1Hz = frequency1Hz;
    config.modulationMode = ModulationMode::Fsk4;
    return demodulateBitDecisionsFsk(samples, config, startOffset);
}

std::vector<BitDecision> demodulateBitDecisions4Fsk(
    const std::vector<float>& samples,
    const ModemConfig& config,
    int startOffset
) {
    ModemConfig fsk4Config = config;
    fsk4Config.modulationMode = ModulationMode::Fsk4;
    return demodulateBitDecisionsFsk(samples, fsk4Config, startOffset);
}

std::vector<std::uint8_t> demodulateBitsFsk(
    const std::vector<float>& samples,
    const ModemConfig& config,
    int startOffset
) {
    const auto decisions = demodulateBitDecisionsFsk(samples, config, startOffset);
    std::vector<std::uint8_t> bits;
    bits.reserve(decisions.size());
    for (const auto& decision : decisions) {
        bits.push_back(decision.bit);
    }
    return bits;
}

std::vector<std::uint8_t> demodulateBits2Fsk(
    const std::vector<float>& samples,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    int startOffset
) {
    ModemConfig config;
    config.sampleRate = sampleRate;
    config.symbolDurationSec = symbolDurationSec;
    config.frequency0Hz = frequency0Hz;
    config.frequency1Hz = frequency1Hz;
    config.modulationMode = ModulationMode::Fsk2;
    return demodulateBitsFsk(samples, config, startOffset);
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

std::vector<std::uint8_t> demodulateBits4Fsk(
    const std::vector<float>& samples,
    int sampleRate,
    float symbolDurationSec,
    float frequency0Hz,
    float frequency1Hz,
    int startOffset
) {
    ModemConfig config;
    config.sampleRate = sampleRate;
    config.symbolDurationSec = symbolDurationSec;
    config.frequency0Hz = frequency0Hz;
    config.frequency1Hz = frequency1Hz;
    config.modulationMode = ModulationMode::Fsk4;
    return demodulateBitsFsk(samples, config, startOffset);
}

std::vector<std::uint8_t> demodulateBits4Fsk(
    const std::vector<float>& samples,
    const ModemConfig& config,
    int startOffset
) {
    ModemConfig fsk4Config = config;
    fsk4Config.modulationMode = ModulationMode::Fsk4;
    return demodulateBitsFsk(samples, fsk4Config, startOffset);
}

}  // namespace hftext
