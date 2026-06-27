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
- Fast/Slow speed selector;
- compact TX progress and estimate;
- message field at the bottom;
- icon send button at the right;
- the send button becomes a stop/cancel button during TX.

The Settings tab contains:

- callsign;
- audio output and input device;
- detailed RX log toggle;
- manual Start RX / Stop RX controls;
- log and evidence export buttons;
- a Load Defaults button that rewrites the default `hftext.ini`.
- `Load Defaults` also restores the generic `nocall` callsign placeholder.

Advanced modem parameters are stored in `hftext.ini`, created automatically next to `hftext_pc.exe` when missing. This keeps normal operation clean while still allowing debug and field experiments.

The PC app owns the local file storage, but the default values and validation rules for these profiles come from the portable C++ core so future interfaces can reuse the same modem behavior.

Tone markers, audio peak, duration, and clipping summaries use shared core helpers. The PC app only formats those values for the Qt interface and saved reports.

Default profiles:

```ini
[slow]
modulation=8fsk
symbol_duration_s=0.300

[fast]
modulation=8fsk
symbol_duration_s=0.100
```

Common defaults:

```ini
[common]
tx_sample_rate_hz=48000
rx_sample_rate_hz=48000
base_frequency_hz=1050.0
tone_spacing_hz=130.0
amplitude=0.05
preamble_bits=72
```

## Normal Operation

RX starts automatically when the app opens and an input device is available. RX can be stopped manually and restarted from Settings.

TX is direct through the selected audio output. The operator does not need to save a WAV first. Transmission happens only after pressing the send button.

If the Fast/Slow profile or RX-affecting Settings values change while RX is active, the app restarts RX automatically so the streaming receiver uses the new configuration.

## Text Handling

The message field is sanitized as the operator types:

- supported characters are preserved;
- supported accents remain visible;
- unsupported characters are replaced with `?`;
- the TX estimate updates live.

The callsign is configured separately and inserted automatically at the beginning of the payload followed by one space.
Fresh installations use `nocall` as a generic placeholder; the operator should replace it with the correct callsign before real transmissions.

## Diagnostics

Normal logs are compact and timestamped. Robust-frame progress is summarized in coarse progress milestones, and low-confidence rejected candidates are kept out of the normal log. Detailed RX log shows raw telemetry such as sync candidates, recovered physical length, every robust-frame progress event, candidate rejection, and accepted frames.

The received-message history also prefixes each displayed line with the local date and time, so unattended receive sessions show when each message or decode result arrived.

## RX State and Session

The current RX state is no longer shown as a large always-visible Settings field. It is still written to logs and evidence reports. It summarizes the most useful recent event:

- listening;
- sync detected;
- physical length recovered;
- receiving frame;
- candidate rejected;
- valid frame;
- message accepted.

After an accepted message, `RX state` keeps the successful frame visible briefly and labels its quality as `CRC OK` plus the decoder confidence. This prevents weak idle-channel candidates from immediately replacing a valid reception with an invalid-length status.

`RX session` is also written to logs and evidence reports. It shows elapsed time and consolidated counters:

- accepted frames;
- strong rejected candidates;
- recovered `PHYS_LENGTH` events;
- strong sync candidates.

Weak internal candidates and low-confidence rejections are omitted from the normal UI but remain available in detailed logs.

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

The evidence header and summary CSV include RX worker backlog counters:

- current pending audio;
- peak pending audio;
- dropped pending audio.

These fields help separate an RF/audio failure from a live receiver backlog. A
saved WAV can still be decodable if it came from the longer evidence buffer,
while the live decoder may have missed the frame after pending-audio overflow.

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
- selected Fast/Slow speed profile;
- selected audio devices;
- detailed-log state;
- window geometry.

Advanced modem profile parameters are stored in `hftext.ini` beside the executable rather than in the Qt settings store.

The typed TX message is not persisted. This avoids reopening the app with stale text ready to transmit.

## Debug WAV Tools

Normal operation should use direct TX and continuous RX. WAV generation and decoding remain available through the CLI tools and tests for debug/replay workflows.

## Release Packaging

The Windows release package must include the Qt runtime, C++ runtime dependencies, the PC executable, CLI tools, and documentation needed to test on another computer.

Create the package with:

```powershell
.\scripts\package_release.ps1
```

The script builds and tests by default, runs `windeployqt`, copies the PC app, CLI tools, documentation, and writes `PACKAGE.txt` into the package folder.
