# Field Test Guide

This guide is for external HFText testers. HFText is still experimental, so the
goal is to collect useful evidence, not to prove that every link will work.

## Install

Use the latest GitHub Release assets:

- Windows: download the `HFText-win64-release-...zip`, extract it to a local
  folder, and run `hftext_pc.exe`.
- Android: download the APK and install it manually on the test device.

Use the same HFText release on both ends of a link. HFText 0.4.0 uses Text Codec
v0.2 and is not text-compatible with older builds.

## Safety and Operating Notes

- Transmit only where you are licensed and allowed to transmit data/audio tones.
- Keep messages short during tests.
- Use an explicit operator action for every transmission.
- Avoid overdriving radio audio inputs. Clipping hurts decode reliability.
- Do not test with confidential content. HFText does not implement encryption.

## Recommended First Test

Start with a local audio-path test before using RF:

1. Open HFText on the transmit device.
2. Open HFText on the receive device.
3. Set both to the same release and default settings.
4. Select `Fast`.
5. Type a short message such as `test fast`.
6. Play the transmit audio through a speaker and receive it with a microphone.
7. Confirm the received message appears in the history.
8. Repeat with `Slow`.
9. Save RX evidence after each receive test.

This confirms installation, audio output, audio input, and basic modem operation.

## Radio or SDR Test

For RF tests, keep the procedure simple:

1. Start RX before transmitting.
2. Tune until the received traces align with the waterfall tone markers.
3. Keep audio level strong enough to see the tones but below clipping.
4. Test `Fast` with a short message.
5. Test `Slow` with a short message.
6. If both work, try a longer message.
7. Save RX evidence for both successes and failures.

Useful test paths:

- direct speaker to microphone;
- PC to PC through radio/SDR;
- PC to Android through radio/SDR;
- Android to PC through radio/SDR;
- Android to Android through speaker/microphone or radio audio interface.

## What to Save

For every useful test, save RX evidence on the receiving device.

On Windows:

- use `Save RX evidence`;
- send the generated `.txt` and `.wav` files.

On Android:

- use `Save RX evidence`;
- use `Share RX evidence` when practical;
- send the TXT report and both saved WAV files when available.

Evidence is useful even when decoding fails. Failed captures help identify audio
level, tuning, timing, receiver backlog, and channel problems.

## Minimum Feedback Template

Copy this template into a message when reporting a test:

```text
HFText test report

Tester:
Date/time:
HFText release:
Platform RX: Windows / Android
Platform TX: Windows / Android
Mode: Fast / Slow
Path: speaker-mic / radio-SDR / radio-radio / other
Distance:
Radio/SDR/audio devices:
Frequency/band:
TX power:
Message length: short / medium / 127 symbols
Result: decoded / failed / partial / app problem
Decoded text:
RX signal notes: strong / weak / noisy / fading / mistuned / clipped
What happened:
Evidence files attached: yes / no
```

## Good Test Matrix

When time allows, run a small matrix:

| Test | Mode | Message | Signal condition | Expected evidence |
| --- | --- | --- | --- | --- |
| 1 | Fast | short | good audio | success |
| 2 | Slow | short | good audio | success |
| 3 | Fast | longer | good audio | success or useful failure |
| 4 | Slow | longer | good audio | success or useful failure |
| 5 | Fast | short | weak/noisy | boundary behavior |
| 6 | Slow | short | weak/noisy | boundary behavior |

Do not spend too much time forcing failures. A few clean successes plus a few
realistic weak/noisy cases are more useful than many uncontrolled attempts.

## Common Failure Causes

- Wrong HFText release on one side.
- RX not started.
- Wrong audio input or output device.
- Audio too low.
- Audio clipping or radio overdrive.
- Tone frequencies shifted by tuning error.
- Symbol mode mismatch caused by different Fast/Slow selection.
- SDR or radio filtering cutting part of the tone range.
- Long packet saved after the evidence buffer no longer contains the start.
- Propagation fading or interference.

## What Makes a Report Useful

A useful report includes:

- exact HFText release;
- platform used for TX and RX;
- Fast or Slow mode;
- whether the message decoded;
- attached RX evidence;
- a short note about signal quality and setup.

A screenshot is helpful for UI problems, but saved evidence is much better for
modem and audio problems.
