#include "hftext_app_settings.h"

#include <cassert>
#include <stdexcept>
#include <string>

int main() {
    const auto defaults = hftext::defaultAppModemProfiles();
    assert(defaults.txSampleRate == 48000);
    assert(defaults.rxSampleRate == 48000);
    assert(defaults.baseFrequencyHz == 1050.0F);
    assert(defaults.toneSpacingHz == 130.0F);
    assert(defaults.amplitude == 0.05F);
    assert(defaults.preambleBits == 72);
    assert(defaults.slow.modulationMode == hftext::ModulationMode::Fsk8);
    assert(defaults.fast.modulationMode == hftext::ModulationMode::Fsk8);
    assert(defaults.slow.symbolDurationSec == 0.300F);
    assert(defaults.fast.symbolDurationSec == 0.100F);

    assert(hftext::speedProfileFromKey("fast") == hftext::SpeedProfile::Fast);
    assert(hftext::speedProfileFromKey("FAST") == hftext::SpeedProfile::Fast);
    assert(hftext::speedProfileFromKey("slow") == hftext::SpeedProfile::Slow);
    assert(hftext::speedProfileFromKey("unknown") == hftext::SpeedProfile::Slow);

    assert(std::string(hftext::modulationModeKey(hftext::ModulationMode::Fsk2)) == "2fsk");
    assert(std::string(hftext::modulationModeKey(hftext::ModulationMode::Fsk4)) == "4fsk");
    assert(std::string(hftext::modulationModeKey(hftext::ModulationMode::Fsk8)) == "8fsk");
    hftext::ModulationMode parsedMode = hftext::ModulationMode::Fsk2;
    assert(hftext::tryParseModulationModeKey("8-FSK", parsedMode));
    assert(parsedMode == hftext::ModulationMode::Fsk8);
    assert(!hftext::tryParseModulationModeKey("bad", parsedMode));
    assert(hftext::parseModulationModeKey("4-FSK") == hftext::ModulationMode::Fsk4);
    bool invalidModeRejected = false;
    try {
        (void)hftext::parseModulationModeKey("bad");
    } catch (const std::invalid_argument&) {
        invalidModeRejected = true;
    }
    assert(invalidModeRejected);
    assert(hftext::modulationModeFromKey("8-FSK", hftext::ModulationMode::Fsk2) == hftext::ModulationMode::Fsk8);
    assert(hftext::modulationModeFromKey("4_fsk", hftext::ModulationMode::Fsk2) == hftext::ModulationMode::Fsk4);
    assert(hftext::modulationModeFromKey("2 fsk", hftext::ModulationMode::Fsk8) == hftext::ModulationMode::Fsk2);
    assert(hftext::modulationModeFromKey("bad", hftext::ModulationMode::Fsk4) == hftext::ModulationMode::Fsk4);
    assert(std::string(hftext::modulationModeProtocolName(hftext::ModulationMode::Fsk2)) == "robust-v0.1-2fsk");
    assert(std::string(hftext::modulationModeProtocolName(hftext::ModulationMode::Fsk4)) == "robust-v0.2-exp-4fsk");
    assert(std::string(hftext::modulationModeProtocolName(hftext::ModulationMode::Fsk8)) == "robust-v0.3-exp-8fsk");

    const auto slow = hftext::modemConfigForProfile(defaults, hftext::SpeedProfile::Slow, defaults.rxSampleRate);
    assert(slow.sampleRate == 48000);
    assert(slow.symbolDurationSec == 0.300F);
    assert(slow.frequency0Hz == 1050.0F);
    assert(slow.frequency1Hz == 1180.0F);
    assert(slow.amplitude == 0.05F);
    assert(slow.preambleBits == 72);
    assert(slow.modulationMode == hftext::ModulationMode::Fsk8);
    assert(hftext::highestModulationToneHz(slow) == 1960.0F);
    const auto tones = hftext::modulationToneFrequenciesHz(slow);
    assert(tones.size() == 8);
    assert(tones.front() == 1050.0F);
    assert(tones.back() == 1960.0F);

    const auto fast = hftext::modemConfigForProfile(defaults, hftext::SpeedProfile::Fast, defaults.txSampleRate);
    assert(fast.symbolDurationSec == 0.100F);
    assert(hftext::speedProfileDisplayName(defaults, hftext::SpeedProfile::Slow)
        == "Slow (8-FSK exp v0.3, 0.300 s/symbol)");
    assert(hftext::speedProfileDisplayName(defaults, hftext::SpeedProfile::Fast)
        == "Fast (8-FSK exp v0.3, 0.100 s/symbol)");

    auto invalid = defaults;
    invalid.toneSpacingHz = 0.0F;
    bool rejected = false;
    try {
        (void)hftext::modemConfigForProfile(invalid, hftext::SpeedProfile::Slow, invalid.rxSampleRate);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    invalid = defaults;
    invalid.baseFrequencyHz = 23000.0F;
    invalid.toneSpacingHz = 500.0F;
    rejected = false;
    try {
        (void)hftext::modemConfigForProfile(invalid, hftext::SpeedProfile::Slow, invalid.rxSampleRate);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
