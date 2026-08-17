# User Guide

## Install

Use the packaged Windows release from `dist/`. Extract the ZIP to a local folder and run `hftext_pc.exe`.

For external testers, prefer downloading builds from the project's GitHub
Releases page. Release assets should include the Windows ZIP package and, when
Android testing is needed, the Android APK. Generated executables, ZIP files,
and APK files should not be committed into the source repository.

External testers should also read `docs/14_field_test_guide.md`. It contains a
short test procedure, evidence instructions, and a feedback template.

The package includes:

- `hftext_pc.exe`;
- CLI tools for WAV debugging;
- Qt and C++ runtime dependencies;
- project documentation;
- `PACKAGE.txt` with version, build time, and git metadata.

HFText 0.7.0 uses Text Codec v0.2 and is not text-compatible with 0.3.x
builds. Use the same HFText version on both ends of a test link.

## First Launch

On launch, HFText starts RX automatically when an input device is available.

Fresh settings use `nocall` as a generic callsign placeholder. Replace it with
the real operator callsign before field transmission.

The Operation tab is for normal use:

- visually distinct TX/RX message history at the top;
- RX waterfall in the middle;
- Fast/Slow speed selector;
- short TX estimate and progress;
- message field and send button at the bottom.

The Settings tab is for callsign, audio devices, optional RX-during-TX pause, RX control, detailed log toggle, logs, and evidence export.

Advanced modem parameters are stored in `hftext.ini`, created automatically next to `hftext_pc.exe` when missing.

## Basic Transmit

1. Open Settings.
2. Confirm the callsign is not the `nocall` placeholder.
3. Select the audio output device.
4. Return to Operation.
5. Select `Fast` or `Slow`.
6. Type a message.
7. Press the send button or Enter.

On PC, Enter sends the current draft and Shift+Enter inserts a newline. The Up
arrow recalls earlier messages transmitted during the current session; continue
with Up for older messages and use Down to move forward and restore the draft
that was present before browsing history.

Transmission only starts after an explicit send-button or Enter action. While
TX is active, the same button stops/cancels TX.

For speaker/microphone operation, enable `Pause RX during TX` in Settings. This prevents HFText from decoding its own transmitted audio. A previously active RX session resumes after TX completes or is cancelled; the option does not start RX when RX was already stopped. Waterfall and evidence capture continue during the pause. The option is disabled by default.

## Basic Receive

1. Connect radio audio to an `Input:` source, or select the `Loopback:` output
   endpoint where a Windows SDR is playing.
2. Confirm RX is running.
3. Tune until received tone tracks align with the yellow waterfall markers.
4. Keep input level below clipping.
5. Accepted RX messages appear in the message history with a local timestamp. Explicit TX messages are recorded there too.

The PC message history follows the latest TX/RX entry automatically. Scroll upward when older traffic needs inspection.

Right-click the PC message history and open `Highlight sender` to choose a
callsign already present at the beginning of an RX message. Matching received
messages use an amber background; all other RX and TX traffic remains visible.
Choose `All senders` to remove the highlight.

Blue waterfall traces are weak/normal energy, yellow indicates strong energy near saturation, and red indicates near-full-scale input blocks.

### Receive Windows SDR Audio Directly

Windows builds list output endpoints in Settings as `Loopback: ...`. Choose the
same endpoint selected in the SDR application. HFText then receives the digital
output mix directly, without requiring a speaker and microphone or a virtual
audio cable. The waterfall continues scrolling at its normal speed during quiet
periods; silence remains part of the receive timeline.

Loopback captures every sound played on that endpoint. Silence notifications and
unrelated applications during weak-signal tests. If HFText also transmits through
the same endpoint, enable `Pause RX during TX` unless intentional self-reception
is part of the test.

If a saved output endpoint is disconnected, select an available source and start
RX again. HFText retains physical `Input:` sources and falls back to the Windows
default recording input when a saved source is unavailable at launch.

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

`Load defaults` in Settings rewrites `hftext.ini` with the default Fast and Slow profiles and restores the callsign field to `nocall`.

## Android Field Notes

The Android app uses the same portable core through JNI. Normal use is split into
Operation and Settings panels:

- Operation keeps the field workflow compact: TX/RX message history, RX
  waterfall, symbol count, TX estimate/progress, message draft, compact Clear
  and Send/Stop controls, and the Fast/Slow selector below the composer.
- Settings shows only callsign, `Pause RX during TX`, Start/Stop RX, Save RX
  evidence, and Reset local settings.

Android stores the local callsign, draft message, speed profile, RX-during-TX
pause state, sender highlight, and up to 100 recent TX/RX messages in app-private preferences. `Reset
local settings` in Settings restores operator settings to defaults, including
`nocall`, but does not clear message history or saved evidence files.

Use the compact `Sender` button beside Clear to select a callsign already found
in received history. Matching RX bubbles turn amber without hiding other
messages. Choose `All senders` to return to the normal colors.

`Save RX evidence` writes recent 240 s raw and modem-input WAV files plus a TXT report.
The TXT report includes the active RX profile and core
latency for accepted Android messages when available. Android Settings and
evidence separate instantaneous decoder activity from the stable `Last accepted`
message and record how long after that message the evidence was saved. The
screen is kept awake while TX or RX is active.

On Android, `Pause RX during TX` keeps microphone capture and evidence running but temporarily withholds samples from a reset decoder. Use it when the phone speaker can reach its microphone or an audio interface feeds TX back into RX.

Android requests the voice-recognition audio source automatically and uses an internal fallback if the device does not provide it. There is no audio-source selector in normal operation.

## Save Evidence

Use `Save RX evidence` after a test. HFText writes:

- a recent RX WAV capture;
- a TXT report with settings, logs, timestamped TX/RX message history, summary CSV, accepted-frame CSV, and message-history CSV.

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
Developers can add `--metrics-json metrics.json` to `hftext_stream_wav.exe` for
a structured receiver performance report; this does not change the WAV or the
decoded payload.
