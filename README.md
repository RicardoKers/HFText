# HFText

HFText is an experimental digital text modem for HF radio. It converts short typed messages into audio tones, sends them through a radio audio path, and decodes received audio back into text.

The project favors weak-signal robustness and operator clarity over throughput. Useful data rates are intentionally low while the protocol, DSP core, and PC application are validated with real radio and SDR captures.

## Main Goals

- Build a simple, robust audio-based text modem for HF radio.
- Keep the DSP core independent from the graphical interface and audio APIs.
- Validate the protocol first in Python, then in portable C++.
- Provide a PC application for field testing.
- Reuse the C++ core in both PC and Android applications.
- Keep FEC, interleaving, diagnostics, and evidence export testable.

## Current State

Current application version:

- HFText 0.4.0, experimental track.
- Operational protocol baseline: HFText Basic v0.1 + Text Codec v0.2.
- Experimental physical modes: 4-FSK v0.2 and 8-FSK v0.3.

The operational baseline is HFText Basic v0.1:

- logical frame: `SYNC | LENGTH | PAYLOAD | CRC16`;
- robust layer: convolutional code `conv_k3`, deterministic interleaving, and Viterbi decoding;
- transmitted physical flow: `PREAMBLE | START_SYNC | PHYS_LENGTH | ROBUST_FRAME`;
- continuous receive path in the Qt PC application;
- weighted `START_SYNC`, `PHYS_LENGTH`, and Viterbi decisions when symbol confidence is available.

HFText 0.4.0 adopts Text Codec v0.2 and is not text-compatible with 0.3.x
builds. Use the same HFText version on both ends of a test link.

2-FSK is the conservative v0.1 baseline. 4-FSK v0.2 and 8-FSK v0.3 are experimental physical modulation modes that reuse the same logical frame and robust layer. They must be selected explicitly in the CLI tools or PC application.

The Qt PC application can transmit directly through the sound card, receive continuously, show RX level/quality/waterfall, track RX state/session diagnostics, save logs, and export field evidence bundles.

The Android app is an incremental Kotlin/Compose client. It uses JNI and the portable C ABI for metadata, text preparation, TX estimates, tone frequencies, explicit AudioTrack TX, audio level statistics, AudioRecord streaming RX through the native receiver, timestamped received-message history, Android RX evidence export/share, and a compact Operation/Diagnostics UI split.

## Development Strategy

1. Python simulation.
2. Portable C++ core.
3. CLI tools.
4. PC application.
5. Android application.
6. Protocol and robustness improvements.

## Technology

- Python, NumPy, and pytest for simulation and validation.
- C++17 for the portable DSP core.
- CMake for the C++ core, CLI tools, and PC app.
- Qt 6 Widgets for the PC application.
- Kotlin, Jetpack Compose, JNI, AudioTrack, and AudioRecord for Android.

## Quick Start

Build and test locally:

```powershell
cmake -S . -B build-qt15
cmake --build build-qt15 --config Release
ctest --test-dir build-qt15 -C Release --output-on-failure
```

Run Python validation:

```powershell
cd python-sim
python -m pytest tests
```

Create a Windows release package:

```powershell
.\scripts\package_release.ps1
```

The package is written under `dist/` and includes the PC app, CLI tools, Qt/runtime dependencies, and documentation.

Check the Android development environment:

```powershell
.\scripts\check_android_environment.ps1
```

This only reports installed tools; it does not install or change anything.

For Android tool installation steps, see `docs/11_android_windows_setup.md`.

Build the current Android debug shell:

```powershell
.\scripts\build_android_debug.ps1
```

For the native C API used by JNI integration, see
`docs/12_c_api_reference.md`.

For operator workflow, see `docs/10_user_guide.md`.

## Core Principle

The modem core must not depend on Qt, Android, platform audio APIs, or UI code. User interfaces provide text, settings, audio input/output, logs, and visual feedback; modem behavior belongs in the core and its tests.
