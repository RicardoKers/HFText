#include "hftext_core.h"
#include "hftext_frame.h"
#include "hftext_streaming_receiver.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

bool hasEvent(
    const std::vector<hftext::StreamingReceiverEvent>& events,
    hftext::StreamingReceiverEventType type
) {
    return std::any_of(events.begin(), events.end(), [type](const hftext::StreamingReceiverEvent& event) {
        return event.type == type;
    });
}

void overwriteSymbolWithWeakWrongDecision(
    std::vector<float>& audio,
    const hftext::ModemConfig& config,
    int bitIndex,
    std::uint8_t expectedBit
) {
    const int symbolSamples = static_cast<int>(
        std::lround(static_cast<double>(config.sampleRate) * config.symbolDurationSec)
    );
    const auto start = static_cast<std::size_t>(std::max(0, bitIndex) * symbolSamples);
    if (start + static_cast<std::size_t>(symbolSamples) > audio.size()) {
        return;
    }

    const float correctFrequency = expectedBit == 0 ? config.frequency0Hz : config.frequency1Hz;
    const float wrongFrequency = expectedBit == 0 ? config.frequency1Hz : config.frequency0Hz;
    for (int sample = 0; sample < symbolSamples; ++sample) {
        const double t = static_cast<double>(sample) / config.sampleRate;
        const double correct = 0.95 * std::sin(2.0 * kPi * correctFrequency * t);
        const double wrong = std::sin(2.0 * kPi * wrongFrequency * t);
        audio[start + static_cast<std::size_t>(sample)] = static_cast<float>(0.35 * (correct + wrong));
    }
}

void damageStartSyncAndPhysicalLengthSoftly(std::vector<float>& audio, const hftext::ModemConfig& config) {
    const auto startSync = hftext::startSyncBits();
    for (int index = 0; index < 10; ++index) {
        overwriteSymbolWithWeakWrongDecision(
            audio,
            config,
            config.preambleBits + index,
            startSync[static_cast<std::size_t>(index)]
        );
    }

    const int lengthStart = config.preambleBits + static_cast<int>(startSync.size());
    overwriteSymbolWithWeakWrongDecision(audio, config, lengthStart, 0);
    overwriteSymbolWithWeakWrongDecision(audio, config, lengthStart + hftext::kBitsPerByte, 0);
}

}  // namespace

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
    const auto firstEvents = receiver.takeEvents();
    assert(hasEvent(firstEvents, hftext::StreamingReceiverEventType::SyncFound));
    assert(hasEvent(firstEvents, hftext::StreamingReceiverEventType::PhysicalLengthRecovered));
    assert(hasEvent(firstEvents, hftext::StreamingReceiverEventType::FrameWaiting));
    assert(hasEvent(firstEvents, hftext::StreamingReceiverEventType::FrameDecoded));

    const auto noMoreResults = receiver.pushSamples({0.0F, 0.0F, 0.0F});
    assert(noMoreResults.empty());
    assert(receiver.takeEvents().empty());

    receiver.reset();
    const auto replayResults = receiver.pushSamples(audio);
    assert(replayResults.size() == 1);
    assert(replayResults.front().text == "pu5lrk streaming");
    const auto replayEvents = receiver.takeEvents();
    assert(hasEvent(replayEvents, hftext::StreamingReceiverEventType::FrameDecoded));

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

    hftext::ModemConfig shiftedRxConfig = config;
    shiftedRxConfig.symbolDurationSec = 0.03F;
    hftext::ModemConfig shiftedTxConfig = shiftedRxConfig;
    shiftedTxConfig.frequency0Hz += 15.0F;
    shiftedTxConfig.frequency1Hz += 15.0F;
    const auto shiftedAudio = hftext::modulateText("pu5lrk shifted", shiftedTxConfig);
    hftext::StreamingReceiver shiftedReceiver(shiftedRxConfig);
    std::vector<hftext::DecodeResult> shiftedResults;
    const std::size_t shiftedChunkSize = 997;
    for (std::size_t offset = 0; offset < shiftedAudio.size(); offset += shiftedChunkSize) {
        const auto end = std::min(shiftedAudio.size(), offset + shiftedChunkSize);
        const std::vector<float> chunk(shiftedAudio.begin() + static_cast<std::ptrdiff_t>(offset),
                                       shiftedAudio.begin() + static_cast<std::ptrdiff_t>(end));
        const auto results = shiftedReceiver.pushSamples(chunk);
        shiftedResults.insert(shiftedResults.end(), results.begin(), results.end());
    }
    assert(shiftedResults.size() == 1);
    assert(shiftedResults.front().crcOk);
    assert(shiftedResults.front().payloadValid);
    assert(shiftedResults.front().text == "pu5lrk shifted");

    hftext::ModemConfig midShiftedRxConfig = config;
    midShiftedRxConfig.symbolDurationSec = 0.03F;
    hftext::ModemConfig midShiftedTxConfig = midShiftedRxConfig;
    midShiftedTxConfig.frequency0Hz += 7.5F;
    midShiftedTxConfig.frequency1Hz += 7.5F;
    const auto midShiftedAudio = hftext::modulateText("pu5lrk midshift", midShiftedTxConfig);
    hftext::StreamingReceiver midShiftedReceiver(midShiftedRxConfig);
    std::vector<hftext::DecodeResult> midShiftedResults;
    for (std::size_t offset = 0; offset < midShiftedAudio.size(); offset += shiftedChunkSize) {
        const auto end = std::min(midShiftedAudio.size(), offset + shiftedChunkSize);
        const std::vector<float> chunk(midShiftedAudio.begin() + static_cast<std::ptrdiff_t>(offset),
                                       midShiftedAudio.begin() + static_cast<std::ptrdiff_t>(end));
        const auto results = midShiftedReceiver.pushSamples(chunk);
        midShiftedResults.insert(midShiftedResults.end(), results.begin(), results.end());
    }
    assert(midShiftedResults.size() == 1);
    assert(midShiftedResults.front().crcOk);
    assert(midShiftedResults.front().payloadValid);
    assert(midShiftedResults.front().text == "pu5lrk midshift");

    receiver.reset();
    auto softDamagedAudio = audio;
    damageStartSyncAndPhysicalLengthSoftly(softDamagedAudio, config);
    std::vector<hftext::DecodeResult> softDamagedResults;
    for (std::size_t offset = 0; offset < softDamagedAudio.size(); offset += chunkSize) {
        const auto end = std::min(softDamagedAudio.size(), offset + chunkSize);
        const std::vector<float> chunk(softDamagedAudio.begin() + static_cast<std::ptrdiff_t>(offset),
                                       softDamagedAudio.begin() + static_cast<std::ptrdiff_t>(end));
        const auto results = receiver.pushSamples(chunk);
        softDamagedResults.insert(softDamagedResults.end(), results.begin(), results.end());
    }
    assert(softDamagedResults.size() == 1);
    assert(softDamagedResults.front().crcOk);
    assert(softDamagedResults.front().payloadValid);
    assert(softDamagedResults.front().text == "pu5lrk streaming");

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
