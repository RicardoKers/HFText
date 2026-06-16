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
- Fast/Slow speed selector;
- short TX estimate and progress;
- message field and send button at the bottom.

The Settings tab is for callsign, audio devices, RX control, detailed log toggle, logs, and evidence export.

Advanced modem parameters are stored in `hftext.ini`, created automatically next to `hftext_pc.exe` when missing.

## Basic Transmit

1. Open Settings.
2. Confirm the callsign.
3. Select the audio output device.
4. Return to Operation.
5. Select `Fast` or `Slow`.
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

Default profiles:

```text
Slow: 8-FSK experimental v0.3, 0.300 s/symbol
Fast: 8-FSK experimental v0.3, 0.100 s/symbol
```

Default common modem settings in `hftext.ini`:

```text
TX/RX sample rate: 48000 Hz
Base frequency: 1050 Hz
Tone spacing: 130 Hz
Amplitude: 0.05
Preamble: 72 bits
```

For debug or field experiments, edit `hftext.ini` and restart HFText. Supported modulation values are `2fsk`, `4fsk`, and `8fsk`.

`Load defaults` in Settings rewrites `hftext.ini` with the default Fast and Slow profiles.

## Save Evidence

Use `Save RX evidence` after a test. HFText writes:

- a recent RX WAV capture;
- a TXT report with settings, logs, timestamped received text, summary CSV, and accepted-frame CSV.

Evidence files are the best way to compare settings and debug failures later.

## Save Logs

Use `Save log` for a lighter report without the recent RX WAV. Logs include version, protocol, settings, and timestamped events.

The normal log is filtered for operation: long-frame progress appears in coarse milestones, low-confidence rejected candidates are hidden, and accepted frames are shown as `CRC OK` with their decoder confidence. Enable `Detailed RX log` in Settings when raw receiver telemetry is needed.

## WAV Debug Tools

Normal operation should use direct TX and continuous RX. WAV generation and decoding are debug workflows handled by the CLI tools:

The CLI tools can also be used from a terminal:

```powershell
hftext_tx_wav.exe --callsign pu5lrk "Test" test.wav
hftext_rx_wav.exe --verbose test.wav
hftext_stream_wav.exe --verbose test.wav
```

Use `--version` on any CLI tool to print version and protocol information.
