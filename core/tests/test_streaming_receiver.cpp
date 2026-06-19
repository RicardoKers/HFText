#include "hftext_core.h"
#include "hftext_frame.h"
#include "hftext_modulator.h"
#include "hftext_robust.h"
#include "hftext_streaming_receiver.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <string>
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

std::vector<std::uint8_t> alternatingPreamble(int bitCount) {
    std::vector<std::uint8_t> bits;
    bits.reserve(static_cast<std::size_t>(bitCount));
    for (int index = 0; index < bitCount; ++index) {
        bits.push_back(static_cast<std::uint8_t>(index % 2 == 0 ? 1 : 0));
    }
    return bits;
}

std::vector<float> damagedRobustFrameAudio(const std::string& text, const hftext::ModemConfig& config) {
    auto bits = hftext::buildRobustTransmission(text, alternatingPreamble(config.preambleBits));
    const auto robustStart = static_cast<std::size_t>(config.preambleBits)
        + hftext::startSyncBits().size()
        + hftext::physicalLengthBits(0).size();
    for (std::size_t offset = 0; offset < 96 && robustStart + offset < bits.size(); offset += 3) {
        bits[robustStart + offset] ^= 1U;
    }
    return hftext::modulateBitsFsk(bits, config);
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

    hftext::ModemConfig fsk4Config = config;
    fsk4Config.modulationMode = hftext::ModulationMode::Fsk4;
    fsk4Config.frequency0Hz = 1000.0F;
    fsk4Config.frequency1Hz = 1200.0F;
    const auto fsk4Audio = hftext::modulateText("pu5lrk fsk4", fsk4Config);
    hftext::StreamingReceiver fsk4Receiver(fsk4Config);
    std::vector<hftext::DecodeResult> fsk4Results;
    for (std::size_t offset = 0; offset < fsk4Audio.size(); offset += chunkSize) {
        const auto end = std::min(fsk4Audio.size(), offset + chunkSize);
        const std::vector<float> chunk(fsk4Audio.begin() + static_cast<std::ptrdiff_t>(offset),
                                       fsk4Audio.begin() + static_cast<std::ptrdiff_t>(end));
        const auto results = fsk4Receiver.pushSamples(chunk);
        fsk4Results.insert(fsk4Results.end(), results.begin(), results.end());
    }
    assert(fsk4Results.size() == 1);
    assert(fsk4Results.front().crcOk);
    assert(fsk4Results.front().payloadValid);
    assert(fsk4Results.front().text == "pu5lrk fsk4");

    hftext::ModemConfig fsk8Config = config;
    fsk8Config.modulationMode = hftext::ModulationMode::Fsk8;
    fsk8Config.frequency0Hz = 1000.0F;
    fsk8Config.frequency1Hz = 1200.0F;
    const auto fsk8Audio = hftext::modulateText("pu5lrk fsk8", fsk8Config);
    hftext::StreamingReceiver fsk8Receiver(fsk8Config);
    std::vector<hftext::DecodeResult> fsk8Results;
    for (std::size_t offset = 0; offset < fsk8Audio.size(); offset += chunkSize) {
        const auto end = std::min(fsk8Audio.size(), offset + chunkSize);
        const std::vector<float> chunk(fsk8Audio.begin() + static_cast<std::ptrdiff_t>(offset),
                                       fsk8Audio.begin() + static_cast<std::ptrdiff_t>(end));
        const auto results = fsk8Receiver.pushSamples(chunk);
        fsk8Results.insert(fsk8Results.end(), results.begin(), results.end());
    }
    assert(fsk8Results.size() == 1);
    assert(fsk8Results.front().crcOk);
    assert(fsk8Results.front().payloadValid);
    assert(fsk8Results.front().text == "pu5lrk fsk8");

    hftext::ModemConfig longFsk8Config;
    longFsk8Config.sampleRate = 1600;
    longFsk8Config.symbolDurationSec = 0.2F;
    longFsk8Config.frequency0Hz = 200.0F;
    longFsk8Config.frequency1Hz = 240.0F;
    longFsk8Config.preambleBits = 64;
    longFsk8Config.modulationMode = hftext::ModulationMode::Fsk8;
    auto longFsk8Audio = hftext::modulateText("pu5lrk 8fsk longo", longFsk8Config);
    longFsk8Audio.insert(longFsk8Audio.begin(), 37, 0.0F);
    hftext::StreamingReceiver longFsk8Receiver(longFsk8Config);
    std::vector<hftext::DecodeResult> longFsk8Results;
    const std::size_t longFsk8ChunkSize = 719;
    for (std::size_t offset = 0; offset < longFsk8Audio.size(); offset += longFsk8ChunkSize) {
        const auto end = std::min(longFsk8Audio.size(), offset + longFsk8ChunkSize);
        const std::vector<float> chunk(
            longFsk8Audio.begin() + static_cast<std::ptrdiff_t>(offset),
            longFsk8Audio.begin() + static_cast<std::ptrdiff_t>(end)
        );
        const auto results = longFsk8Receiver.pushSamples(chunk);
        longFsk8Results.insert(longFsk8Results.end(), results.begin(), results.end());
    }
    assert(longFsk8Results.size() == 1);
    assert(longFsk8Results.front().crcOk);
    assert(longFsk8Results.front().payloadValid);
    assert(longFsk8Results.front().text == "pu5lrk 8fsk longo");

    hftext::ModemConfig longShiftedFsk8TxConfig = longFsk8Config;
    longShiftedFsk8TxConfig.frequency0Hz += 15.0F;
    longShiftedFsk8TxConfig.frequency1Hz += 15.0F;
    auto longShiftedFsk8Audio = hftext::modulateText("pu5lrk 8fsk shift", longShiftedFsk8TxConfig);
    longShiftedFsk8Audio.insert(longShiftedFsk8Audio.begin(), 53, 0.0F);
    hftext::StreamingReceiver longShiftedFsk8Receiver(longFsk8Config);
    std::vector<hftext::DecodeResult> longShiftedFsk8Results;
    for (std::size_t offset = 0; offset < longShiftedFsk8Audio.size(); offset += longFsk8ChunkSize) {
        const auto end = std::min(longShiftedFsk8Audio.size(), offset + longFsk8ChunkSize);
        const std::vector<float> chunk(
            longShiftedFsk8Audio.begin() + static_cast<std::ptrdiff_t>(offset),
            longShiftedFsk8Audio.begin() + static_cast<std::ptrdiff_t>(end)
        );
        const auto results = longShiftedFsk8Receiver.pushSamples(chunk);
        longShiftedFsk8Results.insert(longShiftedFsk8Results.end(), results.begin(), results.end());
    }
    assert(longShiftedFsk8Results.size() == 1);
    assert(longShiftedFsk8Results.front().crcOk);
    assert(longShiftedFsk8Results.front().payloadValid);
    assert(longShiftedFsk8Results.front().text == "pu5lrk 8fsk shift");

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

    receiver.reset();
    const auto damagedAudio = damagedRobustFrameAudio(
        "pu5lrk long message with bad crc before the next transmission",
        config
    );
    const auto recoveredAudio = hftext::modulateText("pu5lrk recuperou", config);
    std::vector<float> rejectedThenValidAudio = damagedAudio;
    rejectedThenValidAudio.insert(rejectedThenValidAudio.end(), 311, 0.0F);
    rejectedThenValidAudio.insert(rejectedThenValidAudio.end(), recoveredAudio.begin(), recoveredAudio.end());
    std::vector<hftext::DecodeResult> recoveredResults;
    for (std::size_t offset = 0; offset < rejectedThenValidAudio.size(); offset += chunkSize) {
        const auto end = std::min(rejectedThenValidAudio.size(), offset + chunkSize);
        const std::vector<float> chunk(
            rejectedThenValidAudio.begin() + static_cast<std::ptrdiff_t>(offset),
            rejectedThenValidAudio.begin() + static_cast<std::ptrdiff_t>(end)
        );
        const auto results = receiver.pushSamples(chunk);
        recoveredResults.insert(recoveredResults.end(), results.begin(), results.end());
    }
    assert(recoveredResults.size() == 1);
    assert(recoveredResults.front().crcOk);
    assert(recoveredResults.front().payloadValid);
    assert(recoveredResults.front().text == "pu5lrk recuperou");
    assert(hasEvent(receiver.takeEvents(), hftext::StreamingReceiverEventType::FrameRejected));
}
