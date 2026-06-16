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
- direct sound-card TX;
- continuous sound-card RX;
- waterfall and tone markers;
- RX state/session/quality/level diagnostics;
- settings, logs, WAV debug tools, and evidence export.

`ModemController` connects the UI to the C++ core. It must not implement DSP logic itself.

## Android Application

`android-app/` is reserved for a later phase. The desired Android architecture is:

```text
Kotlin / Jetpack Compose UI
        |
AudioRecord / AudioTrack
        |
JNI bridge
        |
Portable C++ core
```

Android should reuse the same protocol and core behavior validated on PC.

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

The root CMake project builds the core, tests, CLI tools, and PC app when Qt 6 Widgets is available. The PC app should be skipped cleanly on machines without Qt.
