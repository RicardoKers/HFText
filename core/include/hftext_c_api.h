#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef HFTEXT_C_API
#if defined(_WIN32) && defined(HFTEXT_C_API_EXPORTS)
#define HFTEXT_C_API __declspec(dllexport)
#elif defined(__GNUC__) && defined(HFTEXT_C_API_EXPORTS)
#define HFTEXT_C_API __attribute__((visibility("default")))
#else
#define HFTEXT_C_API
#endif
#endif

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

enum HFTextStreamingReceiverEventType {
    HFTEXT_RX_EVENT_SYNC_FOUND = 0,
    HFTEXT_RX_EVENT_PHYSICAL_LENGTH_RECOVERED = 1,
    HFTEXT_RX_EVENT_PHYSICAL_LENGTH_INVALID = 2,
    HFTEXT_RX_EVENT_FRAME_WAITING = 3,
    HFTEXT_RX_EVENT_FRAME_REJECTED = 4,
    HFTEXT_RX_EVENT_FRAME_DECODED = 5,
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

typedef struct HFTextPreparedText {
    int32_t message_empty;
    int32_t payload_too_long;
    int32_t message_symbols;
    int32_t payload_symbols;
    int32_t max_payload_symbols;
    size_t sanitized_message_bytes;
    size_t payload_bytes;
    int32_t sanitized_message_truncated;
    int32_t payload_truncated;
} HFTextPreparedText;

typedef struct HFTextFloatAudio {
    float* samples;
    size_t sample_count;
    int32_t sample_rate;
    double duration_seconds;
} HFTextFloatAudio;

typedef struct HFTextToneFrequencies {
    float frequencies_hz[8];
    int32_t tone_count;
} HFTextToneFrequencies;

typedef struct HFTextAudioStats {
    size_t sample_count;
    float peak;
    size_t clipped_samples;
    double clipping_percent;
    double duration_seconds;
} HFTextAudioStats;

typedef struct HFTextDecodeResult {
    int32_t frame_detected;
    int32_t crc_ok;
    int32_t payload_valid;
    int32_t length;
    int32_t sync_index;
    int32_t start_offset;
    int32_t offsets_tried;
    float confidence;
    char* text_utf8;
    size_t text_size;
    size_t text_bytes;
    int32_t text_truncated;
} HFTextDecodeResult;

typedef struct HFTextStreamingReceiverEvent {
    enum HFTextStreamingReceiverEventType type;
    int32_t phase_offset_samples;
    int64_t sync_sample;
    int32_t sync_bit_index;
    int32_t sync_mismatches;
    int32_t payload_length;
    int32_t decoded_length;
    int32_t bits_available;
    int32_t bits_expected;
    int32_t crc_ok;
    int32_t payload_valid;
    float confidence;
    float latency_seconds;
} HFTextStreamingReceiverEvent;

typedef struct HFTextStreamingReceiver HFTextStreamingReceiver;

HFTEXT_C_API const char* hftext_c_application_name(void);
HFTEXT_C_API const char* hftext_c_version(void);
HFTEXT_C_API const char* hftext_c_version_label(void);
HFTEXT_C_API const char* hftext_c_release_track(void);
HFTEXT_C_API const char* hftext_c_protocol_version(void);

HFTEXT_C_API int32_t hftext_c_default_app_modem_profiles(HFTextAppModemProfiles* out_profiles);

HFTEXT_C_API int32_t hftext_c_modem_config_for_profile(
    const HFTextAppModemProfiles* profiles,
    enum HFTextSpeedProfile profile,
    int32_t sample_rate,
    HFTextModemConfig* out_config,
    char* error_message,
    size_t error_message_size
);

HFTEXT_C_API int32_t hftext_c_estimate_transmission(
    const char* callsign_utf8,
    const char* message_utf8,
    const HFTextModemConfig* config,
    HFTextTransmissionEstimate* out_estimate,
    char* error_message,
    size_t error_message_size
);

HFTEXT_C_API int32_t hftext_c_prepare_text(
    const char* callsign_utf8,
    const char* message_utf8,
    char* sanitized_message_utf8,
    size_t sanitized_message_size,
    char* payload_utf8,
    size_t payload_size,
    HFTextPreparedText* out_text,
    char* error_message,
    size_t error_message_size
);

HFTEXT_C_API int32_t hftext_c_tone_frequencies(
    const HFTextModemConfig* config,
    HFTextToneFrequencies* out_frequencies,
    char* error_message,
    size_t error_message_size
);

HFTEXT_C_API int32_t hftext_c_analyze_audio_samples(
    const float* samples,
    size_t sample_count,
    int32_t sample_rate,
    float clipping_threshold,
    HFTextAudioStats* out_stats,
    char* error_message,
    size_t error_message_size
);

HFTEXT_C_API int32_t hftext_c_generate_transmit_audio(
    const char* callsign_utf8,
    const char* message_utf8,
    const HFTextModemConfig* config,
    HFTextFloatAudio* out_audio,
    char* error_message,
    size_t error_message_size
);

/* Releases audio allocated by hftext_c_generate_transmit_audio.
   Pass zero-initialized or previously released HFTextFloatAudio structs to the generator. */
HFTEXT_C_API void hftext_c_free_audio(HFTextFloatAudio* audio);

HFTEXT_C_API int32_t hftext_c_streaming_receiver_create(
    const HFTextModemConfig* config,
    HFTextStreamingReceiver** out_receiver,
    char* error_message,
    size_t error_message_size
);

HFTEXT_C_API void hftext_c_streaming_receiver_free(HFTextStreamingReceiver* receiver);

HFTEXT_C_API int32_t hftext_c_streaming_receiver_reset(
    HFTextStreamingReceiver* receiver,
    char* error_message,
    size_t error_message_size
);

HFTEXT_C_API int32_t hftext_c_streaming_receiver_set_config(
    HFTextStreamingReceiver* receiver,
    const HFTextModemConfig* config,
    char* error_message,
    size_t error_message_size
);

HFTEXT_C_API int32_t hftext_c_streaming_receiver_push_samples(
    HFTextStreamingReceiver* receiver,
    const float* samples,
    size_t sample_count,
    HFTextDecodeResult* results,
    size_t result_capacity,
    size_t* out_result_count,
    HFTextStreamingReceiverEvent* events,
    size_t event_capacity,
    size_t* out_event_count,
    char* error_message,
    size_t error_message_size
);

#ifdef __cplusplus
}
#endif
