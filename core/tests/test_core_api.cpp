#include "hftext_core.h"

#include <cassert>
#include <stdexcept>
#include <vector>

int main() {
    hftext::ModemConfig config;
    config.sampleRate = 8000;
    config.symbolDurationSec = 0.01F;
    config.frequency0Hz = 1000.0F;
    config.frequency1Hz = 2000.0F;

    const auto audio = hftext::modulateText("pu5lrk Teste", config);
    assert(!audio.empty());

    const auto result = hftext::demodulateSamples(audio, config);
    assert(result.frameDetected);
    assert(result.crcOk);
    assert(result.payloadValid);
    assert(result.text == "pu5lrk Teste");
    assert(result.syncIndex == config.preambleBits);
    assert(result.startOffset == 0);
    assert(result.offsetsTried == 1);

    const int halfSymbolSamples = static_cast<int>(config.sampleRate * config.symbolDurationSec / 2.0F);
    std::vector<float> delayedAudio(static_cast<std::size_t>(halfSymbolSamples), 0.0F);
    delayedAudio.insert(delayedAudio.end(), audio.begin(), audio.end());
    const auto delayedResult = hftext::demodulateSamples(delayedAudio, config);
    assert(delayedResult.frameDetected);
    assert(delayedResult.crcOk);
    assert(delayedResult.payloadValid);
    assert(delayedResult.text == "pu5lrk Teste");
    assert(delayedResult.startOffset >= 0);
    assert(delayedResult.startOffset < static_cast<int>(config.sampleRate * config.symbolDurationSec));
    assert(delayedResult.offsetsTried >= 1);

    config.syncSearch = false;
    const auto noSyncSearch = hftext::demodulateSamples(audio, config);
    assert(!noSyncSearch.frameDetected);
    assert(noSyncSearch.error == "sync not found");

    config.syncSearch = true;
    config.preambleBits = 0;
    const auto frameOnlyAudio = hftext::modulateText("abc", config);
    const auto frameOnlyResult = hftext::demodulateSamples(frameOnlyAudio, config);
    assert(frameOnlyResult.crcOk);
    assert(frameOnlyResult.payloadValid);
    assert(frameOnlyResult.text == "abc");
    assert(frameOnlyResult.syncIndex == 0);

    config.preambleBits = -1;
    bool rejected = false;
    try {
        (void)hftext::modulateText("abc", config);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
