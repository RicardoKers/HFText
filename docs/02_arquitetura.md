# Architecture

## Repository Layout

```text
HFText/
+-- python-sim/   # Simulation, sweeps, WAV tools, Python tests
+-- core/         # Portable C++ modem core, CLI tools, C++ tests
+-- pc-app/       # Qt 6 Widgets PC application
+-- android-app/  # Future Android app
+-- docs/         # Project documentation
+-- scripts/      # Release and maintenance scripts
```

## Layering

HFText is intentionally layered:

```text
UI / CLI / scripts
        |
Modem controller glue
        |
Portable C++ core
        |
Text codec, frame, robust layer, modulation, demodulation
```

The core must not depend on Qt, Android, Windows audio APIs, or GUI classes. Platform-specific audio and UI code live outside `core/`.

Operating profile defaults and modem-setting validation also live in the portable core. This keeps the PC app and the future Android app aligned on Fast/Slow profiles, modulation names, tone spacing, amplitude, and preamble defaults without duplicating modem rules in each interface.

The core also exposes application-level TX helpers for callsign payload insertion, transmit-duration estimation, preamble generation, and audio generation. Interfaces should use these helpers instead of rebuilding TX behavior locally.

Shared audio helpers provide sample peak, clipping percentage, duration, and modem tone-frequency lists. These values are used by the PC waterfall, logs, and evidence reports and can be reused by Android for level and tuning displays.

Shared RX event helpers summarize streaming receiver events into UI-friendly counters, progress, quality, and best-candidate selections. Interfaces should format their own text, but use the common helper decisions so PC and Android diagnostics stay consistent.

The core exposes a small C ABI in `hftext_c_api.h` as the stable boundary for future JNI integration. This keeps Android glue code from depending directly on C++ classes and gives Kotlin a simple way to read version metadata, default Fast/Slow profiles, validated modem configs, prepared TX text and payload symbol counts, modem tone frequencies, audio level statistics, transmit-duration estimates, generated TX audio buffers, and incremental streaming RX results/events. Public C ABI functions use an explicit `HFTEXT_C_API` export macro so the shared library exposes the intended boundary. The C ABI usage contract is documented in `docs/12_c_api_reference.md`.

## Python Simulation

`python-sim/` contains:

- the original text codec and frame implementation;
- WAV TX/RX debug tools;
- noise/channel sweeps;
- FEC and interleaving experiments;
- field evidence aggregation and replay helpers;
- pytest regression tests.

Python is used for quick validation and exploration. Operational behavior should eventually be mirrored in the C++ core.

## C++ Core

`core/` contains:

- text encoding and decoding;
- CRC and logical frame parsing;
- robust transmission builder;
- modulation and demodulation for 2-FSK, 4-FSK, and 8-FSK;
- streaming receiver;
- shared Fast/Slow profile defaults and modem configuration validation;
- shared transmit helpers for payload construction, estimates, and audio generation;
- shared audio statistics and tone-frequency helpers for diagnostics and tuning displays;
- shared RX event summary helpers for progress, quality, and session counters;
- a portable C ABI foundation for future JNI bindings;
- a shared C ABI library target for JNI-style native integration;
- WAV I/O helpers for CLI/debug tools;
- tests for all main behaviors.

The core uses `std::vector<float>` for normalized audio buffers.

## CLI Tools

The CLI tools are thin wrappers around the core:

- `hftext_tx_wav`: generate a transmit WAV.
- `hftext_rx_wav`: decode a WAV offline.
- `hftext_stream_wav`: replay a WAV through the streaming receiver.

They are useful for tests, packaging checks, and replaying field captures. They are not the primary operating UI.

## PC Application

`pc-app/` is a Qt 6 Widgets application. It provides:

- a chat-style operation screen;
- a Fast/Slow operating profile selector;
- direct sound-card TX;
- continuous sound-card RX;
- waterfall and tone markers;
- a local editable `hftext.ini` file for advanced modem parameters;
- compact settings, logs, and evidence export.

`ModemController` connects the UI to the C++ core. It must not implement DSP logic itself.

The PC app reads and writes its local `hftext.ini`, but the meaning of profile settings is provided by the core-level application settings helpers.

## Android Application

`android-app/` contains the initial Kotlin/Compose shell. JNI, native core loading, and audio integration are still future steps. The desired Android architecture is:

```text
Kotlin / Jetpack Compose UI
        |
AudioRecord / AudioTrack
        |
JNI bridge
        |
Portable C ABI
        |
Portable C++ core
```

Android should reuse the same protocol and core behavior validated on PC. It should also reuse the core-level application settings helpers so the Android Fast/Slow profiles match the PC defaults unless a deliberate product decision changes them.

The Android Gradle project is intentionally isolated under `android-app/` so the root CMake build remains focused on the portable core, CLI tools, and PC app.

## Data Flow

TX:

```text
operator text
-> callsign prefix
-> text sanitization and 6-bit symbols
-> logical frame
-> robust encoding and interleaving
-> physical sync and length
-> FSK modulation
-> audio output
```

RX:

```text
audio input blocks
-> tone energy demodulation
-> START_SYNC search
-> PHYS_LENGTH recovery
-> ROBUST_FRAME accumulation
-> deinterleaving
-> Viterbi decoding
-> logical frame validation
-> text display
```

## Build Targets

The root CMake project builds the core, the shared C ABI library, tests, CLI tools, and PC app when Qt 6 Widgets is available. The PC app should be skipped cleanly on machines without Qt.
