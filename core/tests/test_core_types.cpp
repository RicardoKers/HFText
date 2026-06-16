#include "hftext_config.h"
#include "hftext_result.h"
#include "hftext_version.h"

#include <cassert>
#include <cstdint>
#include <type_traits>

int main() {
    hftext::ModemConfig config;
    assert(config.sampleRate == 48000);
    assert(config.symbolDurationSec == 0.5F);
    assert(config.frequency0Hz == 1200.0F);
    assert(config.frequency1Hz == 1600.0F);
    assert(config.amplitude == 0.8F);
    assert(config.preambleBits == 64);
    assert(config.syncSearch);
    assert(config.modulationMode == hftext::ModulationMode::Fsk2);
    assert(hftext::bitsPerModulationSymbol(config.modulationMode) == 1);

    config.modulationMode = hftext::ModulationMode::Fsk4;
    config.frequency0Hz = 1000.0F;
    config.frequency1Hz = 1200.0F;
    assert(hftext::bitsPerModulationSymbol(config.modulationMode) == 2);
    assert(hftext::toneCount(config.modulationMode) == 4);
    assert(hftext::modulationToneFrequencyHz(config, 0) == 1000.0F);
    assert(hftext::modulationToneFrequencyHz(config, 1) == 1200.0F);
    assert(hftext::modulationToneFrequencyHz(config, 2) == 1400.0F);
    assert(hftext::highestModulationToneHz(config) == 1600.0F);

    config.modulationMode = hftext::ModulationMode::Fsk8;
    config.frequency0Hz = 1000.0F;
    config.frequency1Hz = 1200.0F;
    assert(hftext::bitsPerModulationSymbol(config.modulationMode) == 3);
    assert(hftext::toneCount(config.modulationMode) == 8);
    assert(hftext::modulationToneFrequencyHz(config, 0) == 1000.0F);
    assert(hftext::modulationToneFrequencyHz(config, 7) == 2400.0F);
    assert(hftext::highestModulationToneHz(config) == 2400.0F);

    hftext::DecodeResult result;
    assert(!result.frameDetected);
    assert(!result.crcOk);
    assert(!result.payloadValid);
    assert(result.text.empty());
    assert(result.error.empty());
    assert(result.length == 0);
    assert(result.syncIndex == -1);
    assert(result.payloadSymbols.empty());
    assert(result.confidence == 0.0F);

    static_assert(std::is_same_v<decltype(result.payloadSymbols)::value_type, std::uint8_t>);

    static_assert(hftext::kVersion[0] != '\0');
    static_assert(hftext::kProtocolVersion[0] != '\0');
}
