#include "hftext_config.h"
#include "hftext_result.h"

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
}
