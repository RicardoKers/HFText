#include "hftext_c_api.h"

#include <stddef.h>
#include <stdint.h>

#define STATIC_REQUIRE(name, condition) typedef char static_require_##name[(condition) ? 1 : -1]

STATIC_REQUIRE(ok_status_zero, HFTEXT_STATUS_OK == 0);
STATIC_REQUIRE(two_fsk_value, HFTEXT_MODULATION_2FSK == 2);
STATIC_REQUIRE(four_fsk_value, HFTEXT_MODULATION_4FSK == 4);
STATIC_REQUIRE(eight_fsk_value, HFTEXT_MODULATION_8FSK == 8);
STATIC_REQUIRE(tone_capacity, sizeof(((HFTextToneFrequencies*)0)->frequencies_hz) / sizeof(float) == 8);

int main(void) {
    HFTextAppModemProfiles profiles;
    HFTextModemConfig config;
    HFTextPreparedText prepared;
    HFTextFloatAudio audio;
    HFTextToneFrequencies tones;
    HFTextAudioStats stats;
    HFTextDecodeResult result;
    HFTextStreamingReceiverEvent event;
    HFTextStreamingReceiver* receiver;

    profiles.tx_sample_rate = 48000;
    profiles.rx_sample_rate = 48000;
    profiles.slow.modulation_mode = HFTEXT_MODULATION_8FSK;
    profiles.fast.modulation_mode = HFTEXT_MODULATION_8FSK;

    config.sample_rate = profiles.tx_sample_rate;
    config.modulation_mode = HFTEXT_MODULATION_8FSK;
    config.sync_search = 1;

    prepared.max_payload_symbols = 127;
    audio.samples = NULL;
    audio.sample_count = 0;
    tones.tone_count = 0;
    stats.sample_count = 0;
    result.frame_detected = 0;
    result.text_utf8 = NULL;
    result.text_size = 0;
    event.type = HFTEXT_RX_EVENT_SYNC_FOUND;
    receiver = NULL;

    (void)profiles;
    (void)config;
    (void)prepared;
    (void)audio;
    (void)tones;
    (void)stats;
    (void)result;
    (void)event;
    (void)receiver;
    (void)hftext_c_application_name;
    (void)hftext_c_streaming_receiver_free;

    return 0;
}
