# PC Application

## Purpose

The PC application is the main field-test tool for HFText. It lets an operator transmit, receive, inspect, and export evidence using a normal Windows sound card and a radio or SDR audio path.

## Platform

- C++17
- Qt 6 Widgets
- CMake
- Windows audio APIs in `AudioInput` and `AudioOutput`

The modem logic remains in `core/`.

## Current Interface

The interface language is English.

The Settings tab displays the current HFText application version, release track, and protocol baseline. Logs and evidence exports include the same metadata.

The normal operation tab is chat-like:

- received messages at the top;
- RX waterfall in the middle;
- compact TX progress and estimate;
- message field at the bottom;
- icon send button at the right;
- the send button becomes a stop/cancel button during TX.

The Settings tab contains:

- callsign;
- modulation mode;
- TX/WAV sample rate;
- RX sample rate;
- symbol duration;
- base frequency;
- tone spacing;
- amplitude;
- preamble length;
- audio output and input device;
- detailed RX log toggle;
- RX level, progress, quality, state, and session diagnostics;
- manual Start RX / Stop RX controls;
- WAV debug buttons;
- log and evidence export buttons;
- a Load Defaults button.

The Settings tab is scrollable. This keeps the Operation tab usable on shorter window heights while preserving all configuration, diagnostic, log, and evidence controls.

## Normal Operation

RX starts automatically when the app opens and an input device is available. RX can be stopped manually and restarted from Settings.

TX is direct through the selected audio output. The operator does not need to save a WAV first. Transmission happens only after pressing the send button.

If RX-affecting settings change while RX is active, the app restarts RX automatically so the streaming receiver uses the new configuration.

## Text Handling

The message field is sanitized as the operator types:

- supported characters are preserved;
- supported accents remain visible;
- unsupported characters are replaced with `?`;
- the TX estimate updates live.

The callsign is configured separately and inserted automatically at the beginning of the payload followed by one space.

## Diagnostics

The Settings tab shows compact live diagnostics:

- RX level;
- RX progress;
- RX quality;
- RX state;
- RX session counters.

Normal logs are compact and timestamped. Detailed RX log shows raw telemetry such as sync candidates, recovered physical length, robust-frame progress, candidate rejection, and accepted frames.

The received-message history also prefixes each displayed line with the local date and time, so unattended receive sessions show when each message or decode result arrived.

## RX State and Session

`RX state` summarizes the most useful recent event:

- listening;
- sync detected;
- physical length recovered;
- receiving frame;
- candidate rejected;
- valid frame.

`RX session` shows elapsed time and consolidated counters:

- accepted frames;
- strong rejected candidates;
- recovered `PHYS_LENGTH` events;
- strong sync candidates.

Weak internal candidates are omitted from the normal UI but remain available in detailed logs.

## Waterfall

The waterfall displays energy from 300 Hz to 3 kHz and shows yellow vertical tone markers for the selected modulation:

- 2 markers in 2-FSK;
- 4 markers in 4-FSK;
- 8 markers in 8-FSK.

The color palette is:

- blue for weak/normal signal energy;
- yellow for strong energy near saturation;
- red for near-full-scale input blocks.

This helps the operator tune the radio/SDR and adjust receive audio level.

## Logs and Evidence

`Save Log` writes the current operational log to a text file. The header includes current settings and RX state.

`Save RX Evidence` writes two files with the same prefix:

- `.wav`: recent RX audio from the circular evidence buffer;
- `.txt`: settings, recent RX metadata, summary CSV, accepted-frame CSV, received text, and log contents.

The evidence TXT uses English section names:

```text
--- Summary CSV ---
--- Accepted Frames CSV ---
--- Received Text ---
--- Log ---
```

The Python field-summary parser also accepts older Portuguese section names so previous captures remain useful.

## Persistence

The app stores locally:

- callsign;
- modulation;
- sample rates;
- symbol duration;
- base frequency;
- tone spacing;
- amplitude;
- preamble length;
- selected audio devices;
- detailed-log state;
- window geometry.

The typed TX message is not persisted. This avoids reopening the app with stale text ready to transmit.

## Debug WAV Tools

WAV generation and WAV decoding remain in Settings as debug tools. Normal operation should use direct TX and continuous RX.

## Release Packaging

The Windows release package must include the Qt runtime, C++ runtime dependencies, the PC executable, CLI tools, and documentation needed to test on another computer.

Create the package with:

```powershell
.\scripts\package_release.ps1
```

The script builds and tests by default, runs `windeployqt`, copies the PC app, CLI tools, documentation, and writes `PACKAGE.txt` into the package folder.
