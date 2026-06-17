#include "hftext_c_api.h"

#include "hftext_encoder.h"
#include "hftext_version.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#define REQUIRE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "Requirement failed: " #condition << " at line " << __LINE__ << '\n'; \
            std::exit(1); \
        } \
    } while (false)

int main() {
    REQUIRE(std::strcmp(hftext_c_application_name(), "HFText") == 0);
    REQUIRE(std::strcmp(hftext_c_version(), hftext::kVersion) == 0);
    REQUIRE(std::strcmp(hftext_c_version_label(), hftext::kVersionLabel) == 0);
    REQUIRE(std::strcmp(hftext_c_release_track(), hftext::kReleaseTrack) == 0);
    REQUIRE(std::strcmp(hftext_c_protocol_version(), hftext::kProtocolVersion) == 0);

    HFTextAppModemProfiles profiles{};
    REQUIRE(hftext_c_default_app_modem_profiles(&profiles) == HFTEXT_STATUS_OK);
    REQUIRE(profiles.tx_sample_rate == 48000);
    REQUIRE(profiles.rx_sample_rate == 48000);
    REQUIRE(profiles.base_frequency_hz == 1050.0F);
    REQUIRE(profiles.tone_spacing_hz == 130.0F);
    REQUIRE(profiles.amplitude == 0.05F);
    REQUIRE(profiles.preamble_bits == 72);
    REQUIRE(profiles.slow.modulation_mode == HFTEXT_MODULATION_8FSK);
    REQUIRE(profiles.slow.symbol_duration_sec == 0.300F);
    REQUIRE(profiles.fast.modulation_mode == HFTEXT_MODULATION_8FSK);
    REQUIRE(profiles.fast.symbol_duration_sec == 0.100F);
    REQUIRE(hftext_c_default_app_modem_profiles(nullptr) == HFTEXT_STATUS_INVALID_ARGUMENT);

    char error[128] = {};
    HFTextModemConfig config{};
    REQUIRE(hftext_c_modem_config_for_profile(
        &profiles,
        HFTEXT_SPEED_PROFILE_SLOW,
        profiles.rx_sample_rate,
        &config,
        error,
        sizeof(error)
    ) == HFTEXT_STATUS_OK);
    REQUIRE(error[0] == '\0');
    REQUIRE(config.sample_rate == 48000);
    REQUIRE(config.symbol_duration_sec == 0.300F);
    REQUIRE(config.frequency0_hz == 1050.0F);
    REQUIRE(config.frequency1_hz == 1180.0F);
    REQUIRE(config.modulation_mode == HFTEXT_MODULATION_8FSK);

    const std::string rawMessage = std::string("Ol") + "\xC3\xA1" + " " + "\xE2\x98\x83";
    const std::string expectedSanitized = std::string("Ol") + "\xC3\xA1" + " ?";
    const std::string expectedPayload = std::string("PU5LRK Ol") + "\xC3\xA1" + " ?";
    char sanitizedMessage[32] = {};
    char payload[32] = {};
    HFTextPreparedText prepared{};
    REQUIRE(hftext_c_prepare_text(
        "PU5LRK",
        rawMessage.c_str(),
        sanitizedMessage,
        sizeof(sanitizedMessage),
        payload,
        sizeof(payload),
        &prepared,
        error,
        sizeof(error)
    ) == HFTEXT_STATUS_OK);
    REQUIRE(error[0] == '\0');
    REQUIRE(std::string(sanitizedMessage) == expectedSanitized);
    REQUIRE(std::string(payload) == expectedPayload);
    REQUIRE(!prepared.message_empty);
    REQUIRE(!prepared.payload_too_long);
    REQUIRE(prepared.message_symbols == hftext::encodedSymbolCount(expectedSanitized));
    REQUIRE(prepared.payload_symbols == hftext::encodedSymbolCount(expectedPayload));
    REQUIRE(prepared.max_payload_symbols == hftext::kMaxPayloadSymbols);
    REQUIRE(prepared.sanitized_message_bytes == expectedSanitized.size());
    REQUIRE(prepared.payload_bytes == expectedPayload.size());
    REQUIRE(!prepared.sanitized_message_truncated);
    REQUIRE(!prepared.payload_truncated);

    char smallSanitized[3] = {};
    char smallPayload[1] = {};
    REQUIRE(hftext_c_prepare_text(
        "",
        "abcd",
        smallSanitized,
        sizeof(smallSanitized),
        smallPayload,
        sizeof(smallPayload),
        &prepared,
        error,
        sizeof(error)
    ) == HFTEXT_STATUS_OK);
    REQUIRE(std::string(smallSanitized) == "ab");
    REQUIRE(std::string(smallPayload) == "");
    REQUIRE(prepared.sanitized_message_bytes == 4);
    REQUIRE(prepared.payload_bytes == 4);
    REQUIRE(prepared.sanitized_message_truncated);
    REQUIRE(prepared.payload_truncated);
    REQUIRE(prepared.message_symbols == 4);
    REQUIRE(prepared.payload_symbols == 4);

    REQUIRE(hftext_c_prepare_text("", "", nullptr, 0, nullptr, 0, &prepared, error, sizeof(error))
        == HFTEXT_STATUS_OK);
    REQUIRE(prepared.message_empty);
    REQUIRE(prepared.message_symbols == 0);
    REQUIRE(prepared.payload_symbols == 0);
    REQUIRE(!prepared.payload_too_long);

    REQUIRE(hftext_c_prepare_text("", nullptr, nullptr, 0, nullptr, 0, &prepared, error, sizeof(error))
        == HFTEXT_STATUS_INVALID_ARGUMENT);
    REQUIRE(std::strlen(error) > 0);

    HFTextToneFrequencies frequencies{};
    REQUIRE(hftext_c_tone_frequencies(&config, &frequencies, error, sizeof(error)) == HFTEXT_STATUS_OK);
    REQUIRE(error[0] == '\0');
    REQUIRE(frequencies.tone_count == 8);
    REQUIRE(frequencies.frequencies_hz[0] == 1050.0F);
    REQUIRE(frequencies.frequencies_hz[1] == 1180.0F);
    REQUIRE(frequencies.frequencies_hz[7] == 1960.0F);

    REQUIRE(hftext_c_tone_frequencies(nullptr, &frequencies, error, sizeof(error))
        == HFTEXT_STATUS_INVALID_ARGUMENT);
    REQUIRE(frequencies.tone_count == 0);
    REQUIRE(std::strlen(error) > 0);

    const float statsSamples[] = {-0.1F, 0.4F, -0.99F, 1.0F, 0.97F};
    HFTextAudioStats audioStats{};
    REQUIRE(hftext_c_analyze_audio_samples(
        statsSamples,
        sizeof(statsSamples) / sizeof(statsSamples[0]),
        10,
        0.98F,
        &audioStats,
        error,
        sizeof(error)
    ) == HFTEXT_STATUS_OK);
    REQUIRE(error[0] == '\0');
    REQUIRE(audioStats.sample_count == 5);
    REQUIRE(audioStats.peak == 1.0F);
    REQUIRE(audioStats.clipped_samples == 2);
    REQUIRE(audioStats.clipping_percent == 40.0);
    REQUIRE(audioStats.duration_seconds == 0.5);

    REQUIRE(hftext_c_analyze_audio_samples(nullptr, 0, 10, 0.98F, &audioStats, error, sizeof(error))
        == HFTEXT_STATUS_OK);
    REQUIRE(audioStats.sample_count == 0);
    REQUIRE(audioStats.duration_seconds == 0.0);

    REQUIRE(hftext_c_analyze_audio_samples(nullptr, 1, 10, 0.98F, &audioStats, error, sizeof(error))
        == HFTEXT_STATUS_INVALID_ARGUMENT);
    REQUIRE(std::strlen(error) > 0);

    REQUIRE(hftext_c_analyze_audio_samples(statsSamples, 1, 0, 0.98F, &audioStats, error, sizeof(error))
        == HFTEXT_STATUS_INVALID_ARGUMENT);
    REQUIRE(std::strlen(error) > 0);

    REQUIRE(hftext_c_analyze_audio_samples(statsSamples, 1, 10, -0.1F, &audioStats, error, sizeof(error))
        == HFTEXT_STATUS_INVALID_ARGUMENT);
    REQUIRE(std::strlen(error) > 0);

    HFTextTransmissionEstimate estimate{};
    REQUIRE(hftext_c_estimate_transmission("pu5lrk", "Test", &config, &estimate, error, sizeof(error))
        == HFTEXT_STATUS_OK);
    REQUIRE(!estimate.message_empty);
    REQUIRE(!estimate.payload_too_long);
    REQUIRE(estimate.payload_symbols == hftext::encodedSymbolCount("pu5lrk Test"));
    REQUIRE(estimate.max_payload_symbols == 127);
    REQUIRE(estimate.duration_seconds > 0.0);

    auto audioProfiles = profiles;
    audioProfiles.rx_sample_rate = 8000;
    audioProfiles.slow.symbol_duration_sec = 0.010F;
    HFTextModemConfig audioConfig{};
    REQUIRE(hftext_c_modem_config_for_profile(
        &audioProfiles,
        HFTEXT_SPEED_PROFILE_SLOW,
        audioProfiles.rx_sample_rate,
        &audioConfig,
        error,
        sizeof(error)
    ) == HFTEXT_STATUS_OK);

    HFTextFloatAudio audio{};
    REQUIRE(hftext_c_generate_transmit_audio("pu5lrk", "Test", &audioConfig, &audio, error, sizeof(error))
        == HFTEXT_STATUS_OK);
    REQUIRE(error[0] == '\0');
    REQUIRE(audio.samples != nullptr);
    REQUIRE(audio.sample_count > 0);
    REQUIRE(audio.sample_rate == audioConfig.sample_rate);
    REQUIRE(audio.duration_seconds > 0.0);
    bool hasNonZeroSample = false;
    for (std::size_t index = 0; index < audio.sample_count; ++index) {
        REQUIRE(std::fabs(audio.samples[index]) <= audioConfig.amplitude + 0.001F);
        hasNonZeroSample = hasNonZeroSample || std::fabs(audio.samples[index]) > 0.0001F;
    }
    REQUIRE(hasNonZeroSample);

    hftext_c_free_audio(&audio);
    REQUIRE(audio.samples == nullptr);
    REQUIRE(audio.sample_count == 0);
    REQUIRE(audio.sample_rate == 0);
    hftext_c_free_audio(nullptr);

    HFTextModemConfig streamingConfig{
        8000,
        0.010F,
        1000.0F,
        2000.0F,
        0.8F,
        16,
        1,
        HFTEXT_MODULATION_2FSK,
    };
    HFTextFloatAudio streamAudio{};
    REQUIRE(hftext_c_generate_transmit_audio("", "pu5lrk streaming", &streamingConfig, &streamAudio, error, sizeof(error))
        == HFTEXT_STATUS_OK);
    REQUIRE(streamAudio.samples != nullptr);

    HFTextStreamingReceiver* receiver = nullptr;
    REQUIRE(hftext_c_streaming_receiver_create(&streamingConfig, &receiver, error, sizeof(error))
        == HFTEXT_STATUS_OK);
    REQUIRE(receiver != nullptr);

    bool gotDecodedFrame = false;
    bool gotSyncEvent = false;
    bool gotLengthEvent = false;
    bool gotDecodedEvent = false;
    std::size_t totalDecoded = 0;
    const std::size_t chunkSize = 137;
    for (std::size_t offset = 0; offset < streamAudio.sample_count; offset += chunkSize) {
        const std::size_t end = std::min(streamAudio.sample_count, offset + chunkSize);
        char resultText0[64] = {};
        char resultText1[64] = {};
        HFTextDecodeResult results[2]{};
        results[0].text_utf8 = resultText0;
        results[0].text_size = sizeof(resultText0);
        results[1].text_utf8 = resultText1;
        results[1].text_size = sizeof(resultText1);
        HFTextStreamingReceiverEvent events[256]{};
        std::size_t resultCount = 0;
        std::size_t eventCount = 0;
        REQUIRE(hftext_c_streaming_receiver_push_samples(
            receiver,
            streamAudio.samples + offset,
            end - offset,
            results,
            sizeof(results) / sizeof(results[0]),
            &resultCount,
            events,
            sizeof(events) / sizeof(events[0]),
            &eventCount,
            error,
            sizeof(error)
        ) == HFTEXT_STATUS_OK);

        for (std::size_t index = 0; index < std::min(resultCount, sizeof(results) / sizeof(results[0])); ++index) {
            ++totalDecoded;
            gotDecodedFrame = gotDecodedFrame || (
                results[index].frame_detected
                && results[index].crc_ok
                && results[index].payload_valid
                && std::string(results[index].text_utf8) == "pu5lrk streaming"
            );
            REQUIRE(!results[index].text_truncated);
        }

        for (std::size_t index = 0; index < std::min(eventCount, sizeof(events) / sizeof(events[0])); ++index) {
            gotSyncEvent = gotSyncEvent || events[index].type == HFTEXT_RX_EVENT_SYNC_FOUND;
            gotLengthEvent = gotLengthEvent || events[index].type == HFTEXT_RX_EVENT_PHYSICAL_LENGTH_RECOVERED;
            gotDecodedEvent = gotDecodedEvent || events[index].type == HFTEXT_RX_EVENT_FRAME_DECODED;
        }
    }
    REQUIRE(totalDecoded == 1);
    REQUIRE(gotDecodedFrame);
    REQUIRE(gotSyncEvent);
    REQUIRE(gotLengthEvent);
    REQUIRE(gotDecodedEvent);

    REQUIRE(hftext_c_streaming_receiver_reset(receiver, error, sizeof(error)) == HFTEXT_STATUS_OK);
    char replayText[64] = {};
    HFTextDecodeResult replayResult{};
    replayResult.text_utf8 = replayText;
    replayResult.text_size = sizeof(replayText);
    HFTextStreamingReceiverEvent replayEvents[256]{};
    std::size_t replayResultCount = 0;
    std::size_t replayEventCount = 0;
    REQUIRE(hftext_c_streaming_receiver_push_samples(
        receiver,
        streamAudio.samples,
        streamAudio.sample_count,
        &replayResult,
        1,
        &replayResultCount,
        replayEvents,
        sizeof(replayEvents) / sizeof(replayEvents[0]),
        &replayEventCount,
        error,
        sizeof(error)
    ) == HFTEXT_STATUS_OK);
    REQUIRE(replayResultCount == 1);
    REQUIRE(replayResult.crc_ok);
    REQUIRE(replayResult.payload_valid);
    REQUIRE(std::string(replayResult.text_utf8) == "pu5lrk streaming");
    REQUIRE(replayEventCount > 0);

    REQUIRE(hftext_c_streaming_receiver_set_config(receiver, &streamingConfig, error, sizeof(error))
        == HFTEXT_STATUS_OK);
    REQUIRE(hftext_c_streaming_receiver_push_samples(
        nullptr,
        streamAudio.samples,
        streamAudio.sample_count,
        nullptr,
        0,
        &replayResultCount,
        nullptr,
        0,
        &replayEventCount,
        error,
        sizeof(error)
    ) == HFTEXT_STATUS_INVALID_ARGUMENT);
    REQUIRE(std::strlen(error) > 0);

    hftext_c_streaming_receiver_free(receiver);
    hftext_c_streaming_receiver_free(nullptr);
    hftext_c_free_audio(&streamAudio);

    const std::string tooLong(128, 'a');
    REQUIRE(hftext_c_estimate_transmission("", tooLong.c_str(), &config, &estimate, error, sizeof(error))
        == HFTEXT_STATUS_OK);
    REQUIRE(estimate.payload_too_long);
    REQUIRE(estimate.payload_symbols == 128);

    profiles.slow.modulation_mode = static_cast<HFTextModulationMode>(99);
    const auto invalidStatus = hftext_c_modem_config_for_profile(
        &profiles,
        HFTEXT_SPEED_PROFILE_SLOW,
        profiles.rx_sample_rate,
        &config,
        error,
        sizeof(error)
    );
    REQUIRE(invalidStatus == HFTEXT_STATUS_INVALID_ARGUMENT);
    REQUIRE(std::strlen(error) > 0);

    REQUIRE(hftext_c_estimate_transmission("pu5lrk", nullptr, &config, &estimate, error, sizeof(error))
        == HFTEXT_STATUS_INVALID_ARGUMENT);
    REQUIRE(std::strlen(error) > 0);

    REQUIRE(hftext_c_generate_transmit_audio("pu5lrk", nullptr, &config, &audio, error, sizeof(error))
        == HFTEXT_STATUS_INVALID_ARGUMENT);
    REQUIRE(audio.samples == nullptr);
    REQUIRE(audio.sample_count == 0);
    REQUIRE(std::strlen(error) > 0);
}
