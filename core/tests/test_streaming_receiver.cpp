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

    receiver.reset();
    std::vector<float> delayedAudio(17, 0.0F);
    delayedAudio.insert(delayedAudio.end(), audio.begin(), audio.end());
    std::vector<hftext::DecodeResult> delayedResults;
    for (std::size_t offset = 0; offset < delayedAudio.size(); offset += chunkSize) {
        const auto end = std::min(delayedAudio.size(), offset + chunkSize);
        const std::vector<float> chunk(delayedAudio.begin() + static_cast<std::ptrdiff_t>(offset),
                                       delayedAudio.begin() + static_cast<std::ptrdiff_t>(end));
        const auto results = receiver.pushSamples(chunk);
        delayedResults.insert(delayedResults.end(), results.begin(), results.end());
    }
    assert(delayedResults.size() == 1);
    assert(delayedResults.front().text == "pu5lrk streaming");

    receiver.reset();
    const auto secondAudio = hftext::modulateText("pu5lrk segunda", config);
    std::vector<float> continuousAudio = delayedAudio;
    continuousAudio.insert(continuousAudio.end(), 231, 0.0F);
    continuousAudio.insert(continuousAudio.end(), secondAudio.begin(), secondAudio.end());
    std::vector<hftext::DecodeResult> continuousResults;
    for (std::size_t offset = 0; offset < continuousAudio.size(); offset += chunkSize) {
        const auto end = std::min(continuousAudio.size(), offset + chunkSize);
        const std::vector<float> chunk(continuousAudio.begin() + static_cast<std::ptrdiff_t>(offset),
                                       continuousAudio.begin() + static_cast<std::ptrdiff_t>(end));
        const auto results = receiver.pushSamples(chunk);
        continuousResults.insert(continuousResults.end(), results.begin(), results.end());
    }
    assert(continuousResults.size() == 2);
    assert(continuousResults[0].text == "pu5lrk streaming");
    assert(continuousResults[1].text == "pu5lrk segunda");
}
