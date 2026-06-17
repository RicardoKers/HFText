# Android Application

## Status

The Android application is a future phase. Development should continue on Python, C++ core, CLI tools, and the PC app until the protocol and receiver behavior are stable enough.

The repository currently contains only a placeholder `android-app/README.md`. It exists to reserve the expected project location and document the intended boundaries; it is not an Android implementation.

## Goal

The Android app should let an operator send and receive HFText messages using a phone or tablet audio interface connected to a radio.

## Planned Architecture

```text
Jetpack Compose UI
        |
Kotlin controller layer
        |
AudioTrack / AudioRecord
        |
JNI bridge
        |
Portable C ABI
        |
Portable C++ core
```

Fast/Slow profile defaults, modulation keys, display names, tone spacing, amplitude, preamble length, and modem configuration validation should come from the shared C++ application settings helpers in `core/`. Android-specific storage may be different, but the interpretation of modem settings should not be duplicated in Kotlin.

Transmit behavior should also use the shared C++ TX helpers for callsign insertion, payload validation, duration estimates, and audio generation. Kotlin should provide text, selected profile, and audio output plumbing, not a separate modem implementation.

Android tuning and level indicators should reuse the shared C++ tone-frequency and audio-statistics helpers where practical.

Android RX status and logs should reuse the shared C++ RX event summary helpers. Kotlin may choose different wording or layout, but strong-sync thresholds, rejected-candidate filtering, progress, and session counters should come from the same core logic used by the PC app.

The first Android-facing core boundary is `core/include/hftext_c_api.h`. It is intentionally small and C-compatible so it can be called from JNI without exposing C++ object lifetimes to Kotlin. The current API exposes:

- application and protocol version metadata;
- default Fast/Slow modem profiles;
- validated modem configuration for a selected profile and sample rate;
- sanitized TX text, payload preview, and payload symbol counts;
- modem tone frequencies for tuning displays;
- audio peak, clipping, and duration statistics;
- transmit estimates for callsign plus message text;
- generated normalized floating-point TX audio, with explicit native buffer release;
- an opaque streaming receiver handle that accepts audio blocks and returns decoded messages plus RX events.

The CMake target `hftext_c_api_shared` builds this boundary as a shared native library (`hftext_c_api.dll` on Windows, `.so` on Unix-like targets). Android should eventually build the same C ABI sources through the NDK and load them from JNI.

Only the public C ABI functions are explicitly exported through the `HFTEXT_C_API` macro. JNI glue should depend on this exported C surface rather than C++ symbols.

The shared C ABI target is tested by normal link-time use and by runtime symbol loading. The dynamic-loading test is intentionally close to how JNI will resolve native entry points from the Android shared library. It resolves every public C ABI function by name and exercises metadata, text preparation, estimates, tone lists, audio statistics, generated audio, receiver control, and a short streaming-RX roundtrip through that loaded C surface.

Evidence export and higher-level Android UI state should be added around this C ABI incrementally as the Android app needs them.

## Requirements

- Reuse the C++ core.
- Keep the same protocol and modem settings as the PC application.
- Reuse the shared C++ Fast/Slow profile defaults and validation.
- Reuse the shared C++ TX helpers.
- Reuse shared C++ tone-frequency and audio-statistics helpers for diagnostics.
- Reuse shared C++ RX event summary helpers for status and session diagnostics.
- Reach the C++ core through a narrow C ABI suitable for JNI.
- Support direct audio TX after explicit operator action.
- Support continuous RX without unbounded memory growth.
- Provide a compact operation screen and a separate settings/debug area.
- Export logs or evidence in a format compatible with the PC-side analysis tools when practical.

## Not Planned Yet

- Android-specific protocol changes.
- Automatic transmission without operator action.
- Encryption.
- Starting Android before the PC/core behavior is mature.
