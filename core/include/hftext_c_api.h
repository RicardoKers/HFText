#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum HFTextStatus {
    HFTEXT_STATUS_OK = 0,
    HFTEXT_STATUS_INVALID_ARGUMENT = 1,
    HFTEXT_STATUS_EXCEPTION = 2,
};

enum HFTextModulationMode {
    HFTEXT_MODULATION_2FSK = 2,
    HFTEXT_MODULATION_4FSK = 4,
    HFTEXT_MODULATION_8FSK = 8,
};

enum HFTextSpeedProfile {
    HFTEXT_SPEED_PROFILE_SLOW = 0,
    HFTEXT_SPEED_PROFILE_FAST = 1,
};

typedef struct HFTextSpeedProfileConfig {
    enum HFTextModulationMode modulation_mode;
    float symbol_duration_sec;
} HFTextSpeedProfileConfig;

typedef struct HFTextAppModemProfiles {
    int32_t tx_sample_rate;
    int32_t rx_sample_rate;
    float base_frequency_hz;
    float tone_spacing_hz;
    float amplitude;
    int32_t preamble_bits;
    HFTextSpeedProfileConfig slow;
    HFTextSpeedProfileConfig fast;
} HFTextAppModemProfiles;

typedef struct HFTextModemConfig {
    int32_t sample_rate;
    float symbol_duration_sec;
    float frequency0_hz;
    float frequency1_hz;
    float amplitude;
    int32_t preamble_bits;
    int32_t sync_search;
    enum HFTextModulationMode modulation_mode;
} HFTextModemConfig;

typedef struct HFTextTransmissionEstimate {
    int32_t message_empty;
    int32_t payload_too_long;
    int32_t payload_symbols;
    int32_t max_payload_symbols;
    int32_t frame_bits;
    int32_t transmission_bits;
    double duration_seconds;
} HFTextTransmissionEstimate;

typedef struct HFTextFloatAudio {
    float* samples;
    size_t sample_count;
    int32_t sample_rate;
    double duration_seconds;
} HFTextFloatAudio;

const char* hftext_c_application_name(void);
const char* hftext_c_version(void);
const char* hftext_c_version_label(void);
const char* hftext_c_release_track(void);
const char* hftext_c_protocol_version(void);

int32_t hftext_c_default_app_modem_profiles(HFTextAppModemProfiles* out_profiles);

int32_t hftext_c_modem_config_for_profile(
    const HFTextAppModemProfiles* profiles,
    enum HFTextSpeedProfile profile,
    int32_t sample_rate,
    HFTextModemConfig* out_config,
    char* error_message,
    size_t error_message_size
);

int32_t hftext_c_estimate_transmission(
    const char* callsign_utf8,
    const char* message_utf8,
    const HFTextModemConfig* config,
    HFTextTransmissionEstimate* out_estimate,
    char* error_message,
    size_t error_message_size
);

int32_t hftext_c_generate_transmit_audio(
    const char* callsign_utf8,
    const char* message_utf8,
    const HFTextModemConfig* config,
    HFTextFloatAudio* out_audio,
    char* error_message,
    size_t error_message_size
);

/* Releases audio allocated by hftext_c_generate_transmit_audio.
   Pass zero-initialized or previously released HFTextFloatAudio structs to the generator. */
void hftext_c_free_audio(HFTextFloatAudio* audio);

#ifdef __cplusplus
}
#endif
