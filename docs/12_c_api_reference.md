# C API Reference

## Purpose

`core/include/hftext_c_api.h` is the stable native boundary for JNI integration.
It exposes the modem core as a small C-compatible API so Android can reuse the same
protocol, text preparation, modem defaults, TX helpers, and streaming RX behavior as
the PC application.

This document is a usage contract for application glue code. The header remains the
source of truth for exact types and function signatures.

## Build Target

The shared-library target is:

```text
hftext_c_api_shared
```

It builds the exported C ABI library:

- Windows: `hftext_c_api.dll`
- Unix-like targets: `libhftext_c_api.so`

Only functions marked with `HFTEXT_C_API` are intended to be loaded from the shared
library. JNI code should depend on these exported C functions, not on C++ symbols or
classes.

## General Conventions

- All text input and output is UTF-8.
- Audio samples are normalized `float` values, normally between `-1.0` and `+1.0`.
- Function return values use `HFTextStatus`.
- Optional `error_message` buffers are cleared on entry and filled with a
  null-terminated diagnostic string on failure when space is available.
- Output structs are reset before use where practical.
- Caller-provided string buffers are always null-terminated when their size is
  greater than zero.
- A truncation flag means the full output did not fit in the caller-provided buffer.
- `HFTextStreamingReceiver*` is opaque and must be released with
  `hftext_c_streaming_receiver_free`.
- `HFTextFloatAudio.samples` is allocated by the core and must be released with
  `hftext_c_free_audio`.
- Do not pass an unreleased `HFTextFloatAudio` back to
  `hftext_c_generate_transmit_audio`; free it first or use a zero-initialized
  struct.
- A receiver handle represents one RX stream. Do not call the same handle
  concurrently from multiple threads without external synchronization.

## Status Values

| Value | Meaning |
| --- | --- |
| `HFTEXT_STATUS_OK` | The call succeeded. |
| `HFTEXT_STATUS_INVALID_ARGUMENT` | A null pointer, invalid enum, invalid modem setting, or other caller-provided value was rejected. |
| `HFTEXT_STATUS_EXCEPTION` | An unexpected internal exception was caught and converted to a C status. |

Always check the status before using output fields.

## Metadata

These functions return static null-terminated strings. The caller must not free them.

```c
hftext_c_application_name();
hftext_c_version();
hftext_c_version_label();
hftext_c_release_track();
hftext_c_protocol_version();
```

Use them for About screens, logs, evidence export, and compatibility checks.

## Profiles and Modem Config

`hftext_c_default_app_modem_profiles` returns the same application defaults used by
the PC app. At the time this document was written, both Fast and Slow profiles use
8-FSK, with Slow at 0.300 s per symbol and Fast at 0.100 s per symbol.

`hftext_c_modem_config_for_profile` converts a stored app profile into a validated
`HFTextModemConfig` for a selected sample rate.

Recommended app flow:

1. Load local app settings.
2. If they do not exist, call `hftext_c_default_app_modem_profiles`.
3. Select `HFTEXT_SPEED_PROFILE_SLOW` or `HFTEXT_SPEED_PROFILE_FAST`.
4. Call `hftext_c_modem_config_for_profile`.
5. Use the resulting `HFTextModemConfig` for TX, RX, tones, and estimates.

Do not duplicate modem-setting validation in Kotlin.

## Text Preparation and TX Estimates

`hftext_c_prepare_text` sanitizes a message, builds the transmit payload, and reports
symbol counts. The callsign is optional; when present, it is sanitized and inserted
at the start of the payload followed by one space. Unsupported characters are
replaced with `?` according to the protocol text codec.

The function can be used while the operator is typing:

- `sanitized_message_utf8` shows what will be transmitted.
- `payload_utf8` shows the final callsign-plus-message payload.
- `message_symbols` and `payload_symbols` count 6-bit text symbols, including shift
  and accent modifier symbols.
- `payload_too_long` indicates that the payload exceeds 127 symbols.

`hftext_c_estimate_transmission` returns payload length, frame bit count,
transmission bit count, and estimated duration for the selected modem config.

## Tone and Audio Diagnostics

`hftext_c_tone_frequencies` returns the active modem tone list. For 8-FSK it returns
eight tones starting at `frequency0_hz` and spaced by
`frequency1_hz - frequency0_hz`.

Use this for waterfall markers and operator tuning aids.

`hftext_c_analyze_audio_samples` returns sample count, peak, clipped-sample count,
clipping percentage, and duration. A null sample pointer is valid only when the
sample count is zero.

Use this for RX level indicators, saturation warnings, and evidence metadata.

## TX Audio Generation

`hftext_c_generate_transmit_audio` builds normalized float audio for an explicit
operator-triggered transmission.

```c
HFTextFloatAudio audio = {0};
int32_t status = hftext_c_generate_transmit_audio(
    callsign,
    message,
    &config,
    &audio,
    error,
    sizeof(error)
);
```

When the call succeeds:

- `audio.samples` points to native memory owned by the caller.
- `audio.sample_count` is the number of float samples.
- `audio.sample_rate` is copied from the modem config.
- `audio.duration_seconds` is derived from sample count and sample rate.

The caller must eventually call:

```c
hftext_c_free_audio(&audio);
```

After release, the struct is reset to an empty state.

Do not transmit automatically on app startup or after receiving a message. TX must
remain tied to an explicit operator action.

## Streaming RX

Create one receiver per active audio stream:

```c
HFTextStreamingReceiver* receiver = NULL;
hftext_c_streaming_receiver_create(&config, &receiver, error, sizeof(error));
```

Feed audio blocks as they arrive from the platform audio API:

```c
HFTextDecodeResult results[2] = {0};
char text0[256] = {0};
char text1[256] = {0};
results[0].text_utf8 = text0;
results[0].text_size = sizeof(text0);
results[1].text_utf8 = text1;
results[1].text_size = sizeof(text1);

HFTextStreamingReceiverEvent events[128] = {0};
size_t result_count = 0;
size_t event_count = 0;

hftext_c_streaming_receiver_push_samples(
    receiver,
    samples,
    sample_count,
    results,
    2,
    &result_count,
    events,
    128,
    &event_count,
    error,
    sizeof(error)
);
```

`out_result_count` and `out_event_count` report the total number produced by the
core for that push. If a caller-provided array is smaller than the total count, only
the first entries are copied. Increase capacity if dropped diagnostics matter.

Each `HFTextDecodeResult` text buffer must be provided by the caller before calling
`hftext_c_streaming_receiver_push_samples`. The core copies decoded UTF-8 text into
that buffer and sets `text_truncated` when it did not fit.

Accept a received message only when all of these are true:

- `frame_detected != 0`
- `crc_ok != 0`
- `payload_valid != 0`
- `text_truncated == 0`, unless the UI deliberately supports truncated display

Events provide progress and diagnostics for the UI:

- `HFTEXT_RX_EVENT_SYNC_FOUND`
- `HFTEXT_RX_EVENT_PHYSICAL_LENGTH_RECOVERED`
- `HFTEXT_RX_EVENT_PHYSICAL_LENGTH_INVALID`
- `HFTEXT_RX_EVENT_FRAME_WAITING`
- `HFTEXT_RX_EVENT_FRAME_REJECTED`
- `HFTEXT_RX_EVENT_FRAME_DECODED`

Use `hftext_c_summarize_rx_events` to apply the same core-level event selection
rules used by the PC app. It accepts the copied event array for one receiver push
and fills `HFTextRxEventSummary` with:

- best event indexes for decoded, length, waiting, sync, and rejected events;
- filtered rejected-candidate count;
- invalid-length presence;
- session counter increments for sync, length, and rejected events;
- best quality in permille, or `-1` when no displayable quality is available;
- terminal-candidate presence.

Android and other JNI clients should prefer this helper over duplicating receiver
event filtering in platform code. User-facing text can remain platform-specific,
but the decision about which event matters should come from the core.

Use `hftext_c_streaming_receiver_set_config` when the selected profile, tones,
sample rate, or symbol duration changes. Use `hftext_c_streaming_receiver_reset`
to clear receiver state while keeping the handle.

Release the handle with:

```c
hftext_c_streaming_receiver_free(receiver);
```

## Recommended JNI Boundary

Keep JNI narrow and mechanical:

1. Convert Kotlin strings to UTF-8 C strings.
2. Convert app settings to `HFTextAppModemProfiles` only at the boundary.
3. Call the C API for profile validation, text preparation, estimates, tone lists,
   audio stats, TX audio generation, and streaming RX.
4. Copy native output into Kotlin data classes.
5. Release native buffers and handles deterministically.

Kotlin should not implement text encoding, FEC, interleaving, frame parsing,
modulation, demodulation, or CRC validation.

The current Android bridge follows this rule for the first increments: it loads
`libhftext_c_api.so`, calls metadata/profile/text-preparation/TX-estimate/audio
generation/audio-statistics/streaming-RX/RX-event-summary functions through a small
`libhftext_android_jni.so` wrapper, and displays the returned values in Compose.
Explicit TX playback is handled in Kotlin with `AudioTrack`. RX capture is handled
in Kotlin with `AudioRecord`; captured blocks are pushed to an opaque native
receiver handle, and accepted messages are displayed only after frame, payload, and
CRC validation by the core.

## Validation

The C ABI is covered by:

- `core/tests/test_c_api_header.c`: public header compiles as C.
- `core/tests/test_c_api.cpp`: direct link-time API behavior.
- `core/tests/test_c_api_shared_link.cpp`: shared-library link behavior.
- `core/tests/test_c_api_dynamic_load.cpp`: runtime symbol lookup and
  generated-audio streaming RX roundtrip through the loaded shared library.

When the Android app begins, keep these tests passing and add JNI/Android tests
around the same API flows instead of bypassing this boundary.
