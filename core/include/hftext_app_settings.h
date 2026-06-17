#pragma once

#include "hftext_config.h"

#include <string>
#include <string_view>
#include <vector>

namespace hftext {

enum class SpeedProfile {
    Slow,
    Fast,
};

struct SpeedProfileConfig {
    ModulationMode modulationMode = ModulationMode::Fsk8;
    float symbolDurationSec = 0.300F;
};

struct AppModemProfiles {
    int txSampleRate = 48000;
    int rxSampleRate = 48000;
    float baseFrequencyHz = 1050.0F;
    float toneSpacingHz = 130.0F;
    float amplitude = 0.05F;
    int preambleBits = 72;
    SpeedProfileConfig slow{ModulationMode::Fsk8, 0.300F};
    SpeedProfileConfig fast{ModulationMode::Fsk8, 0.100F};
};

AppModemProfiles defaultAppModemProfiles();

SpeedProfile speedProfileFromKey(std::string_view key);
const char* speedProfileKey(SpeedProfile profile);
const char* speedProfileLabel(SpeedProfile profile);

const char* modulationModeKey(ModulationMode mode);
bool tryParseModulationModeKey(std::string_view key, ModulationMode& mode);
ModulationMode parseModulationModeKey(std::string_view key);
ModulationMode modulationModeFromKey(std::string_view key, ModulationMode fallback);
std::string modulationModeDisplayName(ModulationMode mode);
const char* modulationModeProtocolName(ModulationMode mode);

void validateAppModemConfig(const ModemConfig& config);
ModemConfig modemConfigForProfile(
    const AppModemProfiles& profiles,
    SpeedProfile profile,
    int sampleRate
);
std::string speedProfileDisplayName(const AppModemProfiles& profiles, SpeedProfile profile);
std::vector<float> modulationToneFrequenciesHz(const ModemConfig& config);

}  // namespace hftext
