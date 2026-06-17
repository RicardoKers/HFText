#include "hftext_c_api.h"

#include "hftext_encoder.h"
#include "hftext_version.h"

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
