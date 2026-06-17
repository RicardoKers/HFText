#include "hftext_app_settings.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace hftext {
namespace {

std::string normalizedKey(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const char ch : value) {
        if (ch == '-' || ch == '_' || ch == ' ') {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return normalized;
}

const SpeedProfileConfig& profileConfig(const AppModemProfiles& profiles, SpeedProfile profile) {
    return profile == SpeedProfile::Fast ? profiles.fast : profiles.slow;
}

}  // namespace

AppModemProfiles defaultAppModemProfiles() {
    return {};
}

SpeedProfile speedProfileFromKey(std::string_view key) {
    return normalizedKey(key) == "fast" ? SpeedProfile::Fast : SpeedProfile::Slow;
}

const char* speedProfileKey(SpeedProfile profile) {
    return profile == SpeedProfile::Fast ? "fast" : "slow";
}

const char* speedProfileLabel(SpeedProfile profile) {
    return profile == SpeedProfile::Fast ? "Fast" : "Slow";
}

const char* modulationModeKey(ModulationMode mode) {
    switch (mode) {
    case ModulationMode::Fsk8:
        return "8fsk";
    case ModulationMode::Fsk4:
        return "4fsk";
    case ModulationMode::Fsk2:
    default:
        return "2fsk";
    }
}

bool tryParseModulationModeKey(std::string_view key, ModulationMode& mode) {
    const auto normalized = normalizedKey(key);
    if (normalized == "8" || normalized == "8fsk") {
        mode = ModulationMode::Fsk8;
        return true;
    }
    if (normalized == "4" || normalized == "4fsk") {
        mode = ModulationMode::Fsk4;
        return true;
    }
    if (normalized == "2" || normalized == "2fsk") {
        mode = ModulationMode::Fsk2;
        return true;
    }
    return false;
}

ModulationMode parseModulationModeKey(std::string_view key) {
    ModulationMode mode = ModulationMode::Fsk2;
    if (tryParseModulationModeKey(key, mode)) {
        return mode;
    }
    throw std::invalid_argument("invalid mode: " + std::string(key));
}

ModulationMode modulationModeFromKey(std::string_view key, ModulationMode fallback) {
    ModulationMode mode = fallback;
    if (tryParseModulationModeKey(key, mode)) {
        return mode;
    }
    return fallback;
}

std::string modulationModeDisplayName(ModulationMode mode) {
    switch (mode) {
    case ModulationMode::Fsk8:
        return "8-FSK exp v0.3";
    case ModulationMode::Fsk4:
        return "4-FSK exp v0.2";
    case ModulationMode::Fsk2:
    default:
        return "2-FSK v0.1";
    }
}

const char* modulationModeProtocolName(ModulationMode mode) {
    switch (mode) {
    case ModulationMode::Fsk8:
        return "robust-v0.3-exp-8fsk";
    case ModulationMode::Fsk4:
        return "robust-v0.2-exp-4fsk";
    case ModulationMode::Fsk2:
    default:
        return "robust-v0.1-2fsk";
    }
}

void validateAppModemConfig(const ModemConfig& config) {
    if (config.sampleRate <= 0) {
        throw std::invalid_argument("sample rate must be positive");
    }
    if (config.symbolDurationSec <= 0.0F) {
        throw std::invalid_argument("symbol duration must be positive");
    }
    if (config.frequency0Hz <= 0.0F || config.frequency1Hz <= 0.0F) {
        throw std::invalid_argument("tone frequencies must be positive");
    }
    if (config.frequency0Hz == config.frequency1Hz) {
        throw std::invalid_argument("tone spacing must be positive");
    }
    if (toneCount(config.modulationMode) > 2 && modulationToneSpacingHz(config) <= 0.0F) {
        throw std::invalid_argument("MFSK requires positive tone spacing");
    }
    if (config.amplitude < 0.0F || config.amplitude > 1.0F) {
        throw std::invalid_argument("amplitude must be between 0 and 1");
    }
    if (config.preambleBits < 0) {
        throw std::invalid_argument("preamble must be non-negative");
    }

    const float nyquistHz = static_cast<float>(config.sampleRate) / 2.0F;
    if (highestModulationToneHz(config) >= nyquistHz) {
        throw std::invalid_argument("tones must stay below half the sample rate");
    }
}

ModemConfig modemConfigForProfile(
    const AppModemProfiles& profiles,
    SpeedProfile profile,
    int sampleRate
) {
    const auto& selected = profileConfig(profiles, profile);

    ModemConfig config;
    config.sampleRate = sampleRate;
    config.symbolDurationSec = selected.symbolDurationSec;
    config.frequency0Hz = profiles.baseFrequencyHz;
    config.frequency1Hz = profiles.baseFrequencyHz + profiles.toneSpacingHz;
    config.amplitude = profiles.amplitude;
    config.preambleBits = profiles.preambleBits;
    config.modulationMode = selected.modulationMode;
    validateAppModemConfig(config);
    return config;
}

std::string speedProfileDisplayName(const AppModemProfiles& profiles, SpeedProfile profile) {
    const auto& selected = profileConfig(profiles, profile);
    std::string text = speedProfileLabel(profile);
    text += " (";
    text += modulationModeDisplayName(selected.modulationMode);
    text += ", ";

    const int milliseconds = static_cast<int>(selected.symbolDurationSec * 1000.0F + 0.5F);
    text += std::to_string(milliseconds / 1000);
    text += ".";
    const int fractional = milliseconds % 1000;
    if (fractional < 100) {
        text += "0";
    }
    if (fractional < 10) {
        text += "0";
    }
    text += std::to_string(fractional);
    text += " s/symbol)";
    return text;
}

std::vector<float> modulationToneFrequenciesHz(const ModemConfig& config) {
    validateAppModemConfig(config);

    std::vector<float> frequencies;
    frequencies.reserve(static_cast<std::size_t>(toneCount(config.modulationMode)));
    for (int tone = 0; tone < toneCount(config.modulationMode); ++tone) {
        frequencies.push_back(modulationToneFrequencyHz(config, tone));
    }
    return frequencies;
}

}  // namespace hftext
