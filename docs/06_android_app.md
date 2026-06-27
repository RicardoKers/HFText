# Android Application

## Status

The Android application has started with a minimal Kotlin/Compose shell and JNI bridge. It builds as a debug APK and displays version, protocol, Fast/Slow profile summaries, sanitized text, payload preview, symbol counts, tone frequencies, and TX estimates read through the portable C ABI. It can also generate TX audio through the native core and play it with `AudioTrack` after an explicit operator action. It can capture microphone audio with `AudioRecord`, display native audio level/clipping statistics, feed captured blocks to the native streaming receiver, and show accepted messages in a timestamped history plus receiver status summarized through the shared C ABI RX-event helper. Android RX can select voice-recognition, raw/unprocessed, or normal microphone input, applies limited digital gain before the receiver, counts low-confidence receiver activity for field diagnosis, and saves recent 240 s raw/modem-input WAV evidence for PC-side replay with captured-duration reporting. Audio capture is intentionally decoupled from receiver processing so evidence capture can remain real-time even when native decoding is slower on the device. Saved WAV evidence is written in buffered PCM chunks so saving should not spend several seconds on many tiny writes. Android evidence export also writes a TXT report with metadata, tone frequencies, RX counters, and received-message history. The latest saved evidence bundle can be shared through Android's system share sheet using a scoped `FileProvider`, without broad storage permissions. The Compose UI now separates normal field operation from diagnostics so the main workflow stays compact while native status and evidence details remain available. The Android received-message panel keeps a scrollable recent history of up to 100 timestamped messages. Local operator state, including callsign, draft message, Fast/Slow profile, audio input mode, and recent received-message history, is restored from app-private preferences on launch. Diagnostics can reset the local operator settings without clearing received messages or saved evidence files; reset uses `nocall` as a generic callsign placeholder. The screen is kept awake while TX or RX is active to avoid interrupting long field captures.

Development should remain incremental. The PC app and C++ core are still the reference implementation for modem behavior.

Android tool installation can be checked with:

```powershell
.\scripts\check_android_environment.ps1
```

The script reports Android Studio, SDK, platform-tools, command-line tools, NDK, SDK CMake, and Java runtime availability, including detected versions or warnings when practical. It does not install tools or change the machine.

For Windows installation steps, see `docs/11_android_windows_setup.md`.

Build the current Android shell from the repository root with:

```powershell
.\scripts\build_android_debug.ps1
```

The debug APK intentionally builds the native C++ modem core with optimization
enabled. Long-symbol 8-FSK needs enough CPU headroom to keep the streaming
decoder close to real time on phones.

Install the debug APK on a connected emulator or device with:

```powershell
.\scripts\install_android_debug.ps1
```

Use `-ListDevices` to show ADB devices and `-DeviceId <id>` when more than one
device is connected.

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

Transmit behavior uses the shared C++ TX helpers for callsign insertion, payload validation, duration estimates, and audio generation. Kotlin provides text, selected profile, and audio output plumbing, not a separate modem implementation.

Android tuning and level indicators should reuse the shared C++ tone-frequency and audio-statistics helpers where practical.

Android RX status and logs should reuse the shared C++ RX event summary helpers. Kotlin may choose different wording or layout, but strong-sync thresholds, rejected-candidate filtering, progress, and session counters should come from the same core logic used by the PC app.

The first Android-facing core boundary is `core/include/hftext_c_api.h`. It is intentionally small and C-compatible so it can be called from JNI without exposing C++ object lifetimes to Kotlin. The Android app now has a small JNI wrapper that calls this C ABI for metadata, profile summaries, text preparation, TX estimates, generated TX audio, RX sample rate, audio statistics, and streaming receiver updates. The C ABI usage contract is documented in `docs/12_c_api_reference.md`. The current API exposes:

- application and protocol version metadata;
- default Fast/Slow modem profiles;
- validated modem configuration for a selected profile and sample rate;
- sanitized TX text, payload preview, and payload symbol counts;
- modem tone frequencies for tuning displays;
- audio peak, clipping, and duration statistics;
- transmit estimates for callsign plus message text;
- generated normalized floating-point TX audio, with explicit native buffer release;
- an opaque streaming receiver handle that accepts audio blocks and returns decoded messages plus RX events.

The CMake target `hftext_c_api_shared` builds this boundary as a shared native library (`hftext_c_api.dll` on Windows, `.so` on Unix-like targets). Android builds the same C ABI sources through the NDK and loads them from JNI.

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

## Current Android Validation

- `.\scripts\build_android_debug.ps1` builds the debug APK.
- `.\scripts\install_android_debug.ps1` installs the debug APK on a connected
  emulator or Android device and launches the app by default.
- The Android package version is aligned with the shared HFText 0.3.0 application version.
- The APK packages `libhftext_c_api.so` and `libhftext_android_jni.so`.
- The debug APK builds the native modem libraries with optimization enabled so
  Android field tests exercise realistic streaming-decoder performance.
- The app opens in the emulator and shows `JNI OK via C ABI`.
- Metadata and Fast/Slow summaries shown in the UI come from the native core path.
- Tone-frequency lists shown in the UI and saved in TXT evidence reports come from the native core path.
- Sanitized text, payload preview, symbol counts, and TX estimates shown in the UI come from the native core path.
- `Send audio` generates normalized float TX audio through JNI and plays it with `AudioTrack`.
- `Stop TX` cancels active Android audio playback.
- `Start RX capture` requests microphone permission when needed, captures float mono audio with `AudioRecord`, and shows peak/clipping stats computed through the C ABI.
- Android RX lets the operator choose voice-recognition, raw/unprocessed, or normal microphone capture and falls back when a selected source does not initialize.
- Android RX shows both raw peak level and the modem-input peak level after limited digital gain.
- `Save RX evidence` writes the recent 240 s raw microphone buffer and modem-input buffer as WAV files plus a TXT report in the app-specific `rx-evidence` directory.
- Android TXT evidence includes the active RX profile and core-reported latency for each accepted message when available.
- Android Diagnostics and TXT evidence keep `Decoder` as the instantaneous receiver state and `Last accepted` as the stable latest CRC-valid message.
- Android TXT evidence records elapsed time from the stable latest accepted message to the evidence save time.
- `Share RX evidence` shares the latest saved TXT, raw WAV, and modem-input WAV through Android's system share sheet with temporary read access.
- The Android UI shows the current RX buffer duration and warns when saved RX evidence is shorter than the selected TX estimate.
- Android RX capture and native decoding run on separate threads; `RX buffer` should advance in real time even if the decoder status lags.
- The native streaming receiver uses a bounded live 8-FSK search grid to reduce Android decode backlog while preserving +/-15 Hz frequency-offset coverage. For long 8-FSK symbols, the timing grid uses 10 phase divisions and includes +/-5 Hz offsets so weak field captures with small tuning errors remain decodable without the heavier 20-phase live search.
- Captured Android RX blocks are streamed into a native receiver handle through JNI.
- The Android UI separates Operation and Diagnostics panels; normal TX/RX controls stay in Operation while metadata, tone lists, RX buffer, decoder, and session counters stay in Diagnostics.
- Android JNI uses the shared C ABI RX-event summary helper for status, quality, and session counter updates instead of duplicating event filtering in Kotlin or JNI glue.
- The Android UI restores callsign, draft message, selected speed profile, and selected audio input mode from app-private preferences on launch.
- The Android UI can reset local operator settings from Diagnostics while preserving received history and evidence files; the reset callsign is `nocall`.
- The Android UI restores up to 100 recent timestamped received messages from app-private preferences on launch.
- The Android UI keeps the screen awake while TX or RX is active and reports that state in Diagnostics.
- The Android UI displays accepted messages only after the native receiver reports frame, payload, and CRC success, and keeps recent accepted messages in a timestamped history.
- Low-confidence receiver events are throttled and counted for diagnosis without flooding the UI.
- `Stop RX capture` stops and releases the active Android recorder.
- A 2026-06-26 Slow 8-FSK SDR field retest accepted the same 127-symbol frame
  on PC and Android about one second apart after enabling native optimization in
  the Android debug APK.
- A 2026-06-26 Fast 8-FSK SDR field retest also accepted the same 127-symbol
  frame on PC and Android about one second apart, with the saved Android modem
  WAV replaying successfully through the PC streaming tool.

## Not Planned Yet

- Android-specific protocol changes.
- Automatic transmission without operator action.
- Encryption.
- Starting Android before the PC/core behavior is mature.
