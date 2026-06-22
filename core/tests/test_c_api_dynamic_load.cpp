#include "hftext_c_api.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

class SharedLibrary {
public:
    explicit SharedLibrary(const char* path) {
#if defined(_WIN32)
        handle_ = LoadLibraryA(path);
#else
        handle_ = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
        if (handle_ == nullptr) {
            std::cerr << "Could not load shared library: " << path << "\n";
#if !defined(_WIN32)
            const char* error = dlerror();
            if (error != nullptr) {
                std::cerr << error << "\n";
            }
#endif
        }
    }

    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;

    ~SharedLibrary() {
        if (handle_ != nullptr) {
#if defined(_WIN32)
            FreeLibrary(handle_);
#else
            dlclose(handle_);
#endif
        }
    }

    bool loaded() const {
        return handle_ != nullptr;
    }

    template <typename Function>
    Function symbol(const char* name) const {
#if defined(_WIN32)
        auto* address = reinterpret_cast<void*>(GetProcAddress(handle_, name));
#else
        dlerror();
        void* address = dlsym(handle_, name);
#endif
        if (address == nullptr) {
            std::cerr << "Missing exported symbol: " << name << "\n";
#if !defined(_WIN32)
            const char* error = dlerror();
            if (error != nullptr) {
                std::cerr << error << "\n";
            }
#endif
            return nullptr;
        }
        return reinterpret_cast<Function>(address);
    }

private:
#if defined(_WIN32)
    HMODULE handle_ = nullptr;
#else
    void* handle_ = nullptr;
#endif
};

bool require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <hftext_c_api shared library>\n";
        return 2;
    }

    SharedLibrary library(argv[1]);
    if (!library.loaded()) {
        return 1;
    }

    using StringFn = const char* (*)();
    using DefaultProfilesFn = int32_t (*)(HFTextAppModemProfiles*);
    using ModemConfigForProfileFn = int32_t (*)(
        const HFTextAppModemProfiles*,
        HFTextSpeedProfile,
        int32_t,
        HFTextModemConfig*,
        char*,
        size_t
    );
    using EstimateTransmissionFn = int32_t (*)(
        const char*,
        const char*,
        const HFTextModemConfig*,
        HFTextTransmissionEstimate*,
        char*,
        size_t
    );
    using PrepareTextFn = int32_t (*)(
        const char*,
        const char*,
        char*,
        size_t,
        char*,
        size_t,
        HFTextPreparedText*,
        char*,
        size_t
    );
    using ToneFrequenciesFn = int32_t (*)(
        const HFTextModemConfig*,
        HFTextToneFrequencies*,
        char*,
        size_t
    );
    using AnalyzeAudioSamplesFn = int32_t (*)(
        const float*,
        size_t,
        int32_t,
        float,
        HFTextAudioStats*,
        char*,
        size_t
    );
    using StreamingReceiverCreateFn = int32_t (*)(
        const HFTextModemConfig*,
        HFTextStreamingReceiver**,
        char*,
        size_t
    );
    using StreamingReceiverFreeFn = void (*)(HFTextStreamingReceiver*);
    using GenerateTransmitAudioFn = int32_t (*)(
        const char*,
        const char*,
        const HFTextModemConfig*,
        HFTextFloatAudio*,
        char*,
        size_t
    );
    using StreamingReceiverResetFn = int32_t (*)(HFTextStreamingReceiver*, char*, size_t);
    using StreamingReceiverSetConfigFn = int32_t (*)(
        HFTextStreamingReceiver*,
        const HFTextModemConfig*,
        char*,
        size_t
    );
    using FreeAudioFn = void (*)(HFTextFloatAudio*);
    using StreamingReceiverPushSamplesFn = int32_t (*)(
        HFTextStreamingReceiver*,
        const float*,
        size_t,
        HFTextDecodeResult*,
        size_t,
        size_t*,
        HFTextStreamingReceiverEvent*,
        size_t,
        size_t*,
        char*,
        size_t
    );
    using SummarizeRxEventsFn = int32_t (*)(
        const HFTextStreamingReceiverEvent*,
        size_t,
        HFTextRxEventSummary*,
        char*,
        size_t
    );

    auto applicationName = library.symbol<StringFn>("hftext_c_application_name");
    auto version = library.symbol<StringFn>("hftext_c_version");
    auto versionLabel = library.symbol<StringFn>("hftext_c_version_label");
    auto releaseTrack = library.symbol<StringFn>("hftext_c_release_track");
    auto protocolVersion = library.symbol<StringFn>("hftext_c_protocol_version");
    auto defaultProfiles =
        library.symbol<DefaultProfilesFn>("hftext_c_default_app_modem_profiles");
    auto modemConfigForProfile =
        library.symbol<ModemConfigForProfileFn>("hftext_c_modem_config_for_profile");
    auto estimateTransmission =
        library.symbol<EstimateTransmissionFn>("hftext_c_estimate_transmission");
    auto prepareText = library.symbol<PrepareTextFn>("hftext_c_prepare_text");
    auto toneFrequencies = library.symbol<ToneFrequenciesFn>("hftext_c_tone_frequencies");
    auto analyzeAudioSamples =
        library.symbol<AnalyzeAudioSamplesFn>("hftext_c_analyze_audio_samples");
    auto streamingReceiverCreate =
        library.symbol<StreamingReceiverCreateFn>("hftext_c_streaming_receiver_create");
    auto streamingReceiverFree =
        library.symbol<StreamingReceiverFreeFn>("hftext_c_streaming_receiver_free");
    auto generateTransmitAudio =
        library.symbol<GenerateTransmitAudioFn>("hftext_c_generate_transmit_audio");
    auto freeAudio = library.symbol<FreeAudioFn>("hftext_c_free_audio");
    auto streamingReceiverReset =
        library.symbol<StreamingReceiverResetFn>("hftext_c_streaming_receiver_reset");
    auto streamingReceiverSetConfig =
        library.symbol<StreamingReceiverSetConfigFn>("hftext_c_streaming_receiver_set_config");
    auto streamingReceiverPushSamples =
        library.symbol<StreamingReceiverPushSamplesFn>("hftext_c_streaming_receiver_push_samples");
    auto summarizeRxEvents =
        library.symbol<SummarizeRxEventsFn>("hftext_c_summarize_rx_events");

    if (applicationName == nullptr ||
        version == nullptr ||
        versionLabel == nullptr ||
        releaseTrack == nullptr ||
        protocolVersion == nullptr ||
        defaultProfiles == nullptr ||
        modemConfigForProfile == nullptr ||
        estimateTransmission == nullptr ||
        prepareText == nullptr ||
        toneFrequencies == nullptr ||
        analyzeAudioSamples == nullptr ||
        streamingReceiverCreate == nullptr ||
        streamingReceiverFree == nullptr ||
        generateTransmitAudio == nullptr ||
        freeAudio == nullptr ||
        streamingReceiverReset == nullptr ||
        streamingReceiverSetConfig == nullptr ||
        streamingReceiverPushSamples == nullptr ||
        summarizeRxEvents == nullptr) {
        return 1;
    }

    if (!require(std::strcmp(applicationName(), "HFText") == 0, "Unexpected application name")) {
        return 1;
    }
    if (!require(std::strlen(version()) > 0, "Version string is empty")) {
        return 1;
    }
    if (!require(std::strlen(versionLabel()) > 0, "Version label string is empty")) {
        return 1;
    }
    if (!require(std::strlen(releaseTrack()) > 0, "Release-track string is empty")) {
        return 1;
    }
    if (!require(std::strlen(protocolVersion()) > 0, "Protocol-version string is empty")) {
        return 1;
    }

    HFTextAppModemProfiles profiles{};
    if (!require(
            defaultProfiles(&profiles) == HFTEXT_STATUS_OK,
            "Could not load default modem profiles"
        )) {
        return 1;
    }
    if (!require(
            profiles.slow.modulation_mode == HFTEXT_MODULATION_8FSK,
            "Unexpected slow-profile modulation"
        )) {
        return 1;
    }

    HFTextModemConfig config{};
    char error[128] = {};
    if (!require(
            modemConfigForProfile(
                &profiles,
                HFTEXT_SPEED_PROFILE_FAST,
                profiles.rx_sample_rate,
                &config,
                error,
                sizeof(error)
            ) == HFTEXT_STATUS_OK,
            "Could not derive fast modem config"
        )) {
        std::cerr << error << "\n";
        return 1;
    }

    const std::string rawMessage = std::string("Ol") + "\xC3\xA1" + " " + "\xE2\x98\x83";
    const std::string expectedSanitized = std::string("Ol") + "\xC3\xA1" + " ?";
    const std::string expectedPayload = std::string("PU5LRK Ol") + "\xC3\xA1" + " ?";
    char sanitized[32] = {};
    char payload[32] = {};
    HFTextPreparedText prepared{};
    if (!require(
            prepareText(
                "PU5LRK",
                rawMessage.c_str(),
                sanitized,
                sizeof(sanitized),
                payload,
                sizeof(payload),
                &prepared,
                error,
                sizeof(error)
            ) == HFTEXT_STATUS_OK,
            "Could not prepare text through dynamic C ABI"
        )) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!require(std::string(sanitized) == expectedSanitized, "Unexpected sanitized text")) {
        return 1;
    }
    if (!require(std::string(payload) == expectedPayload, "Unexpected payload text")) {
        return 1;
    }
    if (!require(!prepared.message_empty, "Prepared message unexpectedly empty")) {
        return 1;
    }
    if (!require(!prepared.payload_too_long, "Prepared payload unexpectedly too long")) {
        return 1;
    }

    HFTextTransmissionEstimate estimate{};
    if (!require(
            estimateTransmission("PU5LRK", rawMessage.c_str(), &config, &estimate, error, sizeof(error))
                == HFTEXT_STATUS_OK,
            "Could not estimate transmission through dynamic C ABI"
        )) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!require(estimate.payload_symbols == prepared.payload_symbols, "Estimate payload count mismatch")) {
        return 1;
    }
    if (!require(estimate.duration_seconds > 0.0, "Estimate duration is not positive")) {
        return 1;
    }

    HFTextToneFrequencies tones{};
    if (!require(
            toneFrequencies(&config, &tones, error, sizeof(error)) == HFTEXT_STATUS_OK,
            "Could not obtain tone frequencies through dynamic C ABI"
        )) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!require(tones.tone_count == 8, "Unexpected tone count")) {
        return 1;
    }
    if (!require(tones.frequencies_hz[0] == config.frequency0_hz, "Unexpected first tone")) {
        return 1;
    }
    if (!require(
            tones.frequencies_hz[7] == config.frequency0_hz + 7.0F * (config.frequency1_hz - config.frequency0_hz),
            "Unexpected last tone"
        )) {
        return 1;
    }

    const float samples[] = {-0.20F, 0.99F, -1.00F, 0.10F};
    HFTextAudioStats stats{};
    if (!require(
            analyzeAudioSamples(
                samples,
                sizeof(samples) / sizeof(samples[0]),
                10,
                0.98F,
                &stats,
                error,
                sizeof(error)
            ) == HFTEXT_STATUS_OK,
            "Could not analyze audio through dynamic C ABI"
        )) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!require(stats.sample_count == 4, "Unexpected audio stats sample count")) {
        return 1;
    }
    if (!require(stats.clipped_samples == 2, "Unexpected clipped-sample count")) {
        return 1;
    }
    if (!require(stats.peak == 1.0F, "Unexpected audio peak")) {
        return 1;
    }

    HFTextStreamingReceiver* receiver = nullptr;
    if (!require(
            streamingReceiverCreate(&config, &receiver, error, sizeof(error)) == HFTEXT_STATUS_OK,
            "Could not create streaming receiver through dynamic C ABI"
        )) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!require(receiver != nullptr, "Streaming receiver handle is null")) {
        return 1;
    }
    if (!require(
            streamingReceiverReset(receiver, error, sizeof(error)) == HFTEXT_STATUS_OK,
            "Could not reset streaming receiver through dynamic C ABI"
        )) {
        std::cerr << error << "\n";
        streamingReceiverFree(receiver);
        return 1;
    }
    if (!require(
            streamingReceiverSetConfig(receiver, &config, error, sizeof(error)) == HFTEXT_STATUS_OK,
            "Could not update streaming receiver config through dynamic C ABI"
        )) {
        std::cerr << error << "\n";
        streamingReceiverFree(receiver);
        return 1;
    }

    streamingReceiverFree(receiver);

    HFTextModemConfig roundtripConfig{
        8000,
        0.010F,
        1000.0F,
        2000.0F,
        0.8F,
        16,
        1,
        HFTEXT_MODULATION_2FSK,
    };
    HFTextFloatAudio audio{};
    if (!require(
            generateTransmitAudio("", "dynamic c abi", &roundtripConfig, &audio, error, sizeof(error))
                == HFTEXT_STATUS_OK,
            "Could not generate TX audio through dynamic C ABI"
        )) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!require(audio.samples != nullptr && audio.sample_count > 0, "Generated audio is empty")) {
        freeAudio(&audio);
        return 1;
    }

    receiver = nullptr;
    if (!require(
            streamingReceiverCreate(&roundtripConfig, &receiver, error, sizeof(error)) == HFTEXT_STATUS_OK,
            "Could not create roundtrip streaming receiver"
        )) {
        std::cerr << error << "\n";
        freeAudio(&audio);
        return 1;
    }

    bool gotDecodedFrame = false;
    bool gotSyncEvent = false;
    bool gotLengthEvent = false;
    bool gotDecodedEvent = false;
    bool gotTerminalSummary = false;
    std::size_t totalDecoded = 0;
    const std::size_t chunkSize = 137;
    for (std::size_t offset = 0; offset < audio.sample_count; offset += chunkSize) {
        const std::size_t end = (std::min)(audio.sample_count, offset + chunkSize);
        char text0[64] = {};
        char text1[64] = {};
        HFTextDecodeResult results[2]{};
        results[0].text_utf8 = text0;
        results[0].text_size = sizeof(text0);
        results[1].text_utf8 = text1;
        results[1].text_size = sizeof(text1);
        HFTextStreamingReceiverEvent events[256]{};
        std::size_t resultCount = 0;
        std::size_t eventCount = 0;
        if (!require(
                streamingReceiverPushSamples(
                    receiver,
                    audio.samples + offset,
                    end - offset,
                    results,
                    sizeof(results) / sizeof(results[0]),
                    &resultCount,
                    events,
                    sizeof(events) / sizeof(events[0]),
                    &eventCount,
                    error,
                    sizeof(error)
                ) == HFTEXT_STATUS_OK,
                "Could not push samples through dynamic C ABI"
            )) {
            std::cerr << error << "\n";
            streamingReceiverFree(receiver);
            freeAudio(&audio);
            return 1;
        }

        const auto copiedResults = (std::min)(resultCount, sizeof(results) / sizeof(results[0]));
        for (std::size_t index = 0; index < copiedResults; ++index) {
            ++totalDecoded;
            gotDecodedFrame = gotDecodedFrame || (
                results[index].frame_detected &&
                results[index].crc_ok &&
                results[index].payload_valid &&
                std::string(results[index].text_utf8) == "dynamic c abi"
            );
        }

        const auto copiedEvents = (std::min)(eventCount, sizeof(events) / sizeof(events[0]));
        HFTextRxEventSummary summary{};
        if (!require(
                summarizeRxEvents(events, copiedEvents, &summary, error, sizeof(error))
                    == HFTEXT_STATUS_OK,
                "Could not summarize RX events through dynamic C ABI"
            )) {
            std::cerr << error << "\n";
            streamingReceiverFree(receiver);
            freeAudio(&audio);
            return 1;
        }
        gotTerminalSummary = gotTerminalSummary || summary.has_terminal_candidate != 0;

        for (std::size_t index = 0; index < copiedEvents; ++index) {
            gotSyncEvent = gotSyncEvent || events[index].type == HFTEXT_RX_EVENT_SYNC_FOUND;
            gotLengthEvent =
                gotLengthEvent || events[index].type == HFTEXT_RX_EVENT_PHYSICAL_LENGTH_RECOVERED;
            gotDecodedEvent = gotDecodedEvent || events[index].type == HFTEXT_RX_EVENT_FRAME_DECODED;
        }
    }

    streamingReceiverFree(receiver);
    freeAudio(&audio);

    if (!require(totalDecoded == 1, "Unexpected decoded-frame count")) {
        return 1;
    }
    if (!require(gotDecodedFrame, "Dynamic C ABI roundtrip did not decode the payload")) {
        return 1;
    }
    if (!require(gotSyncEvent, "Dynamic C ABI roundtrip did not report sync")) {
        return 1;
    }
    if (!require(gotLengthEvent, "Dynamic C ABI roundtrip did not report PHYS_LENGTH")) {
        return 1;
    }
    if (!require(gotDecodedEvent, "Dynamic C ABI roundtrip did not report a decoded frame")) {
        return 1;
    }
    if (!require(gotTerminalSummary, "Dynamic C ABI RX summary did not report a terminal candidate")) {
        return 1;
    }

    return 0;
}
