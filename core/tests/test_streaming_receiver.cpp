#include "hftext_core.h"
#include "hftext_streaming_receiver.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

int main() {
    hftext::ModemConfig config;
    config.sampleRate = 8000;
    config.symbolDurationSec = 0.01F;
    config.frequency0Hz = 1000.0F;
    config.frequency1Hz = 2000.0F;
    config.preambleBits = 16;

    const auto audio = hftext::modulateText("pu5lrk streaming", config);

    hftext::StreamingReceiver receiver(config);
    std::vector<hftext::DecodeResult> allResults;
    const std::size_t chunkSize = 137;

    for (std::size_t offset = 0; offset < audio.size(); offset += chunkSize) {
        const auto end = std::min(audio.size(), offset + chunkSize);
        const std::vector<float> chunk(audio.begin() + static_cast<std::ptrdiff_t>(offset),
                                       audio.begin() + static_cast<std::ptrdiff_t>(end));
        const auto results = receiver.pushSamples(chunk);
        allResults.insert(allResults.end(), results.begin(), results.end());
    }

    assert(allResults.size() == 1);
    assert(allResults.front().crcOk);
    assert(allResults.front().payloadValid);
    assert(allResults.front().text == "pu5lrk streaming");

    const auto noMoreResults = receiver.pushSamples({0.0F, 0.0F, 0.0F});
    assert(noMoreResults.empty());

    receiver.reset();
    const auto replayResults = receiver.pushSamples(audio);
    assert(replayResults.size() == 1);
    assert(replayResults.front().text == "pu5lrk streaming");
}
