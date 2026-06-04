#include "hftext_core.h"
#include "hftext_frame.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

float deterministicNoise(std::uint32_t& state, float amplitude) {
    state = state * 1664525U + 1013904223U;
    const float normalized = static_cast<float>((state >> 8) & 0xFFFFU) / 32767.5F - 1.0F;
    return normalized * amplitude;
}

std::vector<float> noisyCaptureLikeAudio(
    const std::vector<float>& audio,
    int sampleRate,
    float leadingSeconds,
    float trailingSeconds,
    float noiseAmplitude
) {
    std::uint32_t state = 0x12345678U;
    std::vector<float> output;
    output.reserve(
        static_cast<std::size_t>(sampleRate * (leadingSeconds + trailingSeconds))
        + audio.size()
    );

    const int leadingSamples = static_cast<int>(std::lround(sampleRate * leadingSeconds));
    for (int index = 0; index < leadingSamples; ++index) {
        output.push_back(deterministicNoise(state, noiseAmplitude));
    }

    for (float sample : audio) {
        output.push_back(sample + deterministicNoise(state, noiseAmplitude));
    }

    const int trailingSamples = static_cast<int>(std::lround(sampleRate * trailingSeconds));
    for (int index = 0; index < trailingSamples; ++index) {
        output.push_back(deterministicNoise(state, noiseAmplitude));
    }
    return output;
}

std::vector<float> clippedNoisyCaptureLikeAudio(
    const std::vector<float>& audio,
    int sampleRate,
    float symbolDurationSec,
    int clippedSymbols,
    float leadingSeconds,
    float trailingSeconds,
    float noiseAmplitude
) {
    const int clippedSamples = static_cast<int>(std::lround(sampleRate * symbolDurationSec * clippedSymbols));
    const auto start = audio.begin() + std::min<std::ptrdiff_t>(
        static_cast<std::ptrdiff_t>(std::max(0, clippedSamples)),
        static_cast<std::ptrdiff_t>(audio.size())
    );
    return noisyCaptureLikeAudio(
        std::vector<float>(start, audio.end()),
        sampleRate,
        leadingSeconds,
        trailingSeconds,
        noiseAmplitude
    );
}

}  // namespace

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
    assert(result.syncIndex == config.preambleBits + static_cast<int>(hftext::syncBits().size()));
    assert(result.startOffset == 0);
    assert(result.offsetsTried >= 1);
    assert(result.confidence > 0.9F);

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

    const int leadingSilenceSamples = static_cast<int>(config.sampleRate * 0.25F);
    const int trailingSilenceSamples = static_cast<int>(config.sampleRate * 0.15F);
    std::vector<float> recordedAudio(static_cast<std::size_t>(leadingSilenceSamples), 0.0F);
    recordedAudio.insert(recordedAudio.end(), audio.begin(), audio.end());
    recordedAudio.insert(recordedAudio.end(), static_cast<std::size_t>(trailingSilenceSamples), 0.0F);
    const auto recordedResult = hftext::demodulateSamples(recordedAudio, config);
    assert(recordedResult.frameDetected);
    assert(recordedResult.crcOk);
    assert(recordedResult.payloadValid);
    assert(recordedResult.text == "pu5lrk Teste");
    assert(recordedResult.syncIndex >= config.preambleBits + static_cast<int>(hftext::syncBits().size()));
    assert(recordedResult.offsetsTried >= 1);

    hftext::ModemConfig captureConfig = config;
    captureConfig.symbolDurationSec = 0.3F;
    const auto slowAudio = hftext::modulateText("pu5lrk Teste", captureConfig);
    const auto noisyRecordedAudio = noisyCaptureLikeAudio(
        slowAudio,
        captureConfig.sampleRate,
        0.8F,
        0.5F,
        0.01F
    );
    const auto noisyRecordedResult = hftext::demodulateSamples(noisyRecordedAudio, captureConfig);
    assert(noisyRecordedResult.frameDetected);
    assert(noisyRecordedResult.crcOk);
    assert(noisyRecordedResult.payloadValid);
    assert(noisyRecordedResult.text == "pu5lrk Teste");

    hftext::ModemConfig clippedConfig = config;
    clippedConfig.symbolDurationSec = 0.03F;
    const auto clippedSourceAudio = hftext::modulateText("pu5lrk Teste", clippedConfig);
    const auto clippedRecordedAudio = clippedNoisyCaptureLikeAudio(
        clippedSourceAudio,
        clippedConfig.sampleRate,
        clippedConfig.symbolDurationSec,
        20,
        0.4F,
        0.2F,
        0.01F
    );
    const auto clippedRecordedResult = hftext::demodulateSamples(clippedRecordedAudio, clippedConfig);
    assert(clippedRecordedResult.frameDetected);
    assert(clippedRecordedResult.crcOk);
    assert(clippedRecordedResult.payloadValid);
    assert(clippedRecordedResult.text == "pu5lrk Teste");

    config.syncSearch = false;
    const auto noSyncSearch = hftext::demodulateSamples(audio, config);
    assert(noSyncSearch.frameDetected);
    assert(noSyncSearch.crcOk);
    assert(noSyncSearch.payloadValid);
    assert(noSyncSearch.text == "pu5lrk Teste");

    config.syncSearch = true;
    config.preambleBits = 0;
    const auto frameOnlyAudio = hftext::modulateText("abc", config);
    const auto frameOnlyResult = hftext::demodulateSamples(frameOnlyAudio, config);
    assert(frameOnlyResult.crcOk);
    assert(frameOnlyResult.payloadValid);
    assert(frameOnlyResult.text == "abc");
    assert(frameOnlyResult.syncIndex == static_cast<int>(hftext::syncBits().size()));
    assert(frameOnlyResult.confidence > 0.9F);

    config.preambleBits = -1;
    bool rejected = false;
    try {
        (void)hftext::modulateText("abc", config);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
