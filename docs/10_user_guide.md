# User Guide

## Install

Use the packaged Windows release from `dist/`. Extract the ZIP to a local folder and run `hftext_pc.exe`.

The package includes:

- `hftext_pc.exe`;
- CLI tools for WAV debugging;
- Qt and C++ runtime dependencies;
- project documentation;
- `PACKAGE.txt` with version, build time, and git metadata.

## First Launch

On launch, HFText starts RX automatically when an input device is available.

The Operation tab is for normal use:

- received messages at the top;
- RX waterfall in the middle;
- short TX estimate and progress;
- message field and send button at the bottom.

The Settings tab is for configuration, diagnostics, logs, evidence export, and WAV debug tools.
It scrolls when the window is short, so all controls remain available without forcing the main window to a tall minimum height.

## Basic Transmit

1. Open Settings.
2. Confirm the callsign.
3. Select the audio output device.
4. Select the modulation and tone settings.
5. Return to Operation.
6. Type a message.
7. Press the send button.

Transmission only starts after pressing the send button. While TX is active, the same button stops/cancels TX.

## Basic Receive

1. Connect radio or SDR audio to the selected input device.
2. Confirm RX is running.
3. Tune until received tone tracks align with the yellow waterfall markers.
4. Keep input level below clipping.
5. Accepted messages appear in the received-message area with a local date/time timestamp.

Blue waterfall traces are weak/normal energy, yellow indicates strong energy near saturation, and red indicates near-full-scale input blocks.

## Recommended Test Settings

Conservative baseline:

```text
Modulation: 2-FSK robust v0.1
Symbol duration: 0.300 s or 0.500 s
Base frequency: 1200 Hz
Tone spacing: 400 Hz
Preamble: 64 bits
```

Experimental faster modes:

```text
4-FSK experimental v0.2
8-FSK experimental v0.3
Base frequency: 1000 Hz
Tone spacing: 200 Hz
```

Use real field evidence before trusting an experimental mode for regular operation.

## Save Evidence

Use `Save RX evidence` after a test. HFText writes:

- a recent RX WAV capture;
- a TXT report with settings, logs, timestamped received text, summary CSV, and accepted-frame CSV.

Evidence files are the best way to compare settings and debug failures later.

## Save Logs

Use `Save log` for a lighter report without the recent RX WAV. Logs include version, protocol, settings, and timestamped events.

The normal log is filtered for operation: long-frame progress appears in coarse milestones, low-confidence rejected candidates are hidden, and accepted frames are shown as `CRC OK` with their decoder confidence. Enable `Detailed RX log` in Settings when raw receiver telemetry is needed.

## WAV Debug Tools

`Generate WAV` and `Decode WAV` are debug tools in Settings. Normal operation should use direct TX and continuous RX.

The CLI tools can also be used from a terminal:

```powershell
hftext_tx_wav.exe --callsign pu5lrk "Test" test.wav
hftext_rx_wav.exe --verbose test.wav
hftext_stream_wav.exe --verbose test.wav
```

Use `--version` on any CLI tool to print version and protocol information.
