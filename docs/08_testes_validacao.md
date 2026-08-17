# Testing and Validation

## Automated Tests

Run Python tests:

```powershell
cd python-sim
python -m pytest tests
```

Run C++ tests:

```powershell
cmake --build build-qt15 --config Release
ctest --test-dir build-qt15 -C Release --output-on-failure
```

The exact build directory may vary. Use the active CMake build directory for the current machine.

Check Android development tools when preparing the Android phase:

```powershell
.\scripts\check_android_environment.ps1
```

Use `-Strict` only when missing Android tools should make the check fail.
For Windows setup details, see `docs/11_android_windows_setup.md`.
For the JNI-facing native API contract, see `docs/12_c_api_reference.md`.

Build the Android debug shell:

```powershell
.\scripts\build_android_debug.ps1
```

The GitHub Actions workflow runs Python simulation tests and standalone C++ core tests on every push and pull request.

## What Must Be Tested

Core protocol behavior:

- text encoding and decoding;
- unsupported-character replacement;
- direct uppercase base-layer behavior;
- Text Codec v0.2 shift-layer behavior, including punctuation, accents, reserved shifted values, and trailing shift;
- payload length limits;
- CRC correctness;
- logical frame build/parse;
- robust layer encode/decode;
- modulation and demodulation for each supported physical mode;
- streaming receiver event flow and frame acceptance.
- portable C ABI behavior for JNI-facing metadata, profiles, modem config, prepared TX text, tone frequencies, audio statistics, TX estimates, generated TX audio buffers, and streaming RX block processing.
- C compatibility of the public `hftext_c_api.h` header.
- shared-library linking for the C ABI target.
- explicit public-symbol export for the C ABI shared-library target.
- runtime dynamic loading, full public-symbol lookup, metadata/helper calls, and generated-audio streaming RX roundtrip for the C ABI shared-library target.

Application behavior:

- English UI labels and logs;
- direct TX from the message field;
- TX cancellation;
- optional RX-during-TX pause persists, suppresses local self-reception, and resumes a previously active RX session after both TX completion and cancellation;
- RX-during-TX pause must not start RX when it was stopped before TX;
- waterfall and evidence capture continue while decoder input is paused during TX;
- PC TX and RX entries remain visible in one timestamped message history, with
  multiline text kept inside one record;
- automatic RX start;
- Fast/Slow speed profile selection from Operation;
- automatic `hftext.ini` creation when missing;
- RX restart when the speed profile, input device, or detailed-log setting changes;
- Windows audio-source enumeration distinguishes physical `Input:` sources from
  WASAPI `Loopback:` output sources and persists the selected endpoint ID;
- loopback format conversion handles PCM16 and float32, stereo-to-mono downmix,
  silent packets, and streaming 44.1-to-48 kHz resampling;
- loopback keeps advancing during output silence and aggregates native WASAPI
  packets into the same fixed 100 ms blocks used by physical input;
- a loopback endpoint failure or disconnect stops RX cleanly and reports the
  audio-source error;
- evidence and log export;
- PC evidence includes `Message History CSV` with direction, timestamp, and text;
- waterfall tone markers;
- default-settings button;
- no console window in the packaged GUI application.
- Android debug shell builds successfully.
- Android JNI metadata bridge loads successfully and shows core metadata/profile summaries in the app.
- Android JNI tone-frequency bridge loads successfully and shows the selected profile's tones in the app and TXT evidence report.
- Android JNI text-preparation and TX-estimate bridge loads successfully and updates the app from the native core path.
- Android explicit TX audio generation loads successfully through JNI and plays with `AudioTrack` only after pressing `Send`.
- Android Operation stays compact: timestamped TX/RX history, RX waterfall, Fast/Slow selector, TX symbol/duration estimate, live TX progress, message draft, Clear, and Send/Stop.
- Android Settings contains only callsign, optional RX-during-TX pause, RX capture controls, evidence saving, and local reset.
- Android RX capture requests microphone permission, starts/stops `AudioRecord`, and updates RX level/clipping through the native C ABI.
- Android RX requests voice-recognition input and falls back internally when that source cannot initialize; raw and modem-input levels remain available in saved evidence.
- Android RX capture feeds audio blocks into the native streaming receiver and displays accepted messages only after core-side frame, payload, and CRC success.
- Android RX counts low-confidence receiver events so weak activity can be distinguished from a completely idle decoder.
- Android `Save RX evidence` writes recent 240 s raw and modem-input WAV files plus a TXT report that can be pulled with `adb`; the modem WAV can be replayed by the PC-side CLI tools.
- Android TXT evidence records the RX profile and core-reported latency per accepted message when available, which should be used when comparing Fast and Slow receive timing.
- Android TXT evidence records elapsed time from the latest accepted message to evidence save; this helps decide whether a long Slow packet may have already rolled out of the evidence buffer.
- Android reopens with the last callsign, draft message, speed profile, and RX-during-TX pause state restored from app-private preferences.
- Android `Reset local settings` restores default callsign `nocall`, draft message, speed profile, and RX-during-TX pause state without clearing message history.
- Android reopens with the recent TX/RX message history restored from app-private preferences; `Clear` should remove it.
- Android keeps the screen awake while TX or RX is active and returns to normal timeout after activity stops.
- Android RX evidence reports captured duration and should be saved only after it covers the selected TX duration plus margin.
- Android `RX buffer` duration should advance in real time; slower growth indicates capture is blocked or audio data is being lost.
- Android accepted RX messages and explicit TX messages remain visible in a timestamped chat-style history and are included in the TXT evidence report with direction.
- Android Operation waterfall covers the normal audio passband and shows selected tone markers so tuning can be compared with the PC app.
- Android persists `Pause RX during TX`, includes it in evidence, and resets the native receiver around TX while keeping `AudioRecord` capture active.
- Switching Fast/Slow while Android RX capture is active must recreate the native receiver before more queued audio is decoded; accepted-message history and evidence must record the profile used by that receiver.

## Field Validation

Useful field test matrix:

- 2-FSK, 4-FSK, and 8-FSK;
- symbol durations such as 0.1 s, 0.3 s, and 0.5 s;
- good signal, weak signal, noisy signal, and mistuned signal;
- direct speaker/microphone path;
- radio-to-SDR path;
- partial packets for negative testing.

### Windows Output Loopback Smoke Test

Use headphones so success cannot come from speaker-to-microphone leakage:

1. Open two HFText instances.
2. In the TX instance, select a Windows audio output and stop its RX.
3. In the RX instance, select `Loopback:` for that same output endpoint.
4. Keep both instances on `Fast`, send five short and five long messages, and
   confirm each result appears shortly after TX ends.
5. Repeat on `Slow`.
6. Select a different loopback endpoint and confirm the modem signal disappears;
   then restore the correct endpoint.
7. Return to a physical `Input:` source and confirm microphone/line capture still works.
8. Save RX evidence and inspect duration, clipping, backlog, and accepted-frame data.

Before sending audio, leave the RX instance on loopback for at least 10 seconds.
The waterfall must continue scrolling at the normal physical-input speed. After
the test, the saved evidence duration should approximately match elapsed RX time,
apart from the bounded recent-audio window and short endpoint startup delay.

Developers can run the optional local continuity diagnostic after building:

```powershell
build-loopback\pc-app\Release\hftext_pc_audio_input_devices_test.exe --capture-default-loopback
```

It opens the Windows default loopback for 3.25 seconds and verifies that at least
2.5 seconds of fixed-cadence audio were delivered even when the endpoint is quiet.

For an SDR test, configure the SDR to render to the selected loopback endpoint.
Remember that other sounds rendered there are intentionally part of the capture.

For each test, save RX evidence from the app. Evidence should include the recent WAV, settings, logs, `Summary CSV`, and `Accepted Frames CSV`.

For external testers, use `docs/14_field_test_guide.md`. It keeps the requested
test procedure and feedback fields consistent across different operators.

## Android RX Evidence

On 2026-06-18, two Android RX captures from a Xiaomi POCO F1 decoded successfully
through the PC streaming replay tool after capture/decoder thread separation:

- `logs/android-rx-20260618-fast/hftext-android-rx-1781831682208-modem.wav`
  decoded as `pu5lrk Fast` with 98.4% confidence.
- `logs/android-rx-20260618-fast/hftext-android-rx-1781831914786-modem.wav`
  decoded as `pu5lrk Fast2` with 86.0% confidence.

These captures use 8-FSK, 0.1 s symbols, 1050 Hz base frequency, and 130 Hz tone spacing.

On 2026-06-19, an Android Slow capture from a Xiaomi POCO F1 was pulled from
`/sdcard/Android/data/org.hftext.android/files/rx-evidence` and replayed on PC:

- `logs/android-rx-20260619-0836/hftext-android-rx-1781868933160-modem.wav`
  decoded as `pu5lrk Slow`.
- With the earlier long-symbol 8-FSK live search grid, replay took about 25.2 s.
- After reducing the live 8-FSK hypothesis grid while keeping +/-15 Hz offset
  coverage, the same replay took about 8.6 s and still decoded successfully.

This test covers Android RX latency behavior; it does not change the HFText Basic
v0.1 protocol.

On 2026-06-26, Android Slow SDR evidence from a Xiaomi POCO F1 showed a useful
streaming/offline difference:

- `logs/android-pulled-20260626-161229/hftext-android-rx-1782501011684-modem.wav`
  decoded with the offline PC receiver as a 127-symbol frame, but the streaming
  replay reported zero accepted frames.
- The offline receiver found the frame at an intermediate timing offset that was
  not present in the reduced long-symbol 8-FSK live timing grid.
- Long-symbol 8-FSK streaming therefore kept a 10-phase timing grid after this
  test, still bounded but fine enough for these SDR-to-phone captures.

This test covers continuous Android/PC streaming receiver behavior; it does not
change the HFText Basic v0.1 protocol.

Later on 2026-06-26, another Android Slow SDR evidence file decoded through the
offline and PC streaming replay tools but the live Android app was still
processing the frame when evidence was saved:

- `logs/android-pulled-20260626-1640/hftext-android-rx-1782502750952-modem.wav`
  decoded as the same 127-symbol message accepted by the PC evidence
  `logs/HFText-rx-evidence-20260626-163923.wav`.
- The Android report showed `receiving frame 892/1620 bits`, zero accepted
  messages, and a very high event count, indicating receiver backlog rather
  than bad captured audio.
- Android debug builds therefore compile the native C++ modem core with
  optimization enabled, so field testing is closer to release performance.

After that change, a Slow 8-FSK SDR field retest accepted the same 127-symbol
message on both receivers:

- PC evidence `logs/HFText-rx-evidence-20260626-165205.txt` accepted the frame
  at 2026-06-26 16:51:53 with 22.9% quality.
- Android evidence
  `logs/android-pulled-20260626-1651/hftext-android-rx-1782503518375.txt`
  accepted the frame at 2026-06-26 16:51:54.
- The Android modem WAV replayed through the PC streaming tool and decoded the
  same payload, confirming the saved evidence remains reproducible.

A subsequent Fast 8-FSK SDR retest also accepted the same 127-symbol message on
both receivers:

- PC evidence `logs/HFText-rx-evidence-20260626-165742.txt` accepted the frame
  at 2026-06-26 16:57:34 with 33.3% quality.
- Android evidence
  `logs/android-pulled-20260626-1657/hftext-android-rx-1782503866313.txt`
  accepted the frame at 2026-06-26 16:57:35.
- The Android modem WAV replayed through the PC streaming tool with the same
  payload and high replay confidence.

Later on 2026-06-26, a mixed SDR, speaker, and microphone field sequence ran PC
and Android receive at the same time:

- two short Fast 8-FSK messages were accepted on both devices;
- two 127-symbol Fast 8-FSK messages were accepted on both devices;
- two short Slow 8-FSK messages were accepted on both devices;
- two 127-symbol Slow 8-FSK messages were accepted on Android and not shown as
  accepted by the live PC app, but the saved PC evidence WAVs
  `logs/HFText-rx-evidence-20260626-171313.wav` and
  `logs/HFText-rx-evidence-20260626-171633.wav` replayed successfully through
  the PC streaming CLI;
- after increasing audio volume, a 127-symbol Slow 8-FSK message was accepted on
  both devices;
- a noisy medium Fast 8-FSK message was accepted by Android, while the PC live
  app and the saved PC evidence WAV did not decode it.

This separates two failure classes: cases where a different microphone/audio
path captured a better signal, and cases where the saved PC audio is decodable
but the live PC receiver did not surface the frame before evidence was saved.
The latter is consistent with live receiver backlog or pending-audio overflow:
the PC evidence buffer is intentionally much longer than the worker queue used
for live decoding. The PC app now keeps a larger pending-audio queue and records
current, peak, and dropped RX worker backlog in evidence logs.

On 2026-06-27, further simultaneous PC and Android tests showed no PC worker
dropouts (`dropped 0`) after the larger queue. Short Fast, short Slow, and long
Fast 8-FSK frames were accepted on both devices. A weak long Slow PC capture at
13:42 did not decode through the live streaming path, but the saved PC WAV
decoded when the streaming replay included a +5 Hz tone offset. Adding +/-5 Hz
to the bounded long-symbol 8-FSK streaming frequency grid recovered this PC
capture without increasing the timing grid.

A temporary 20-phase long-symbol timing grid also decoded the same PC evidence,
but three later Android captures saved at 14:40, 14:44, and 14:45 only decoded
after replay and did not complete live on the phone. Those three Android modem
WAVs decoded with the lighter 10-phase plus +/-5 Hz grid. The chosen live grid
therefore remains 10 timing phases and adds +/-5 Hz frequency offsets for
long-symbol 8-FSK.

Later 2026-06-27 Android evidence saved after that final grid showed the live
phone receiver accepting Slow 8-FSK captures again:

- `hftext-android-rx-1782584328380.txt`: message accepted, about 199 s saved.
- `hftext-android-rx-1782584539463.txt`: session contained one accepted frame;
  the instantaneous decoder state at save time had moved on to an invalid
  `PHYS_LENGTH` candidate.
- `hftext-android-rx-1782584626437.txt`: message accepted, about 49 s saved.

At that point `field_summary.py` over the local `logs/` directory reported 81
evidence files, 48 accepted-frame rows, 21 unique accepted frames, average
quality 44.9%, and minimum accepted quality 11.6%.

Text Codec v0.2 was adopted after these field captures. Older evidence remains
useful for physical receive, timing, and CRC regression work, but old payload
symbols are interpreted through the new alphabet if replayed by HFText 0.4.0 or
later. Do not use pre-0.4.0 evidence as a text-codec compatibility proof.

## Evidence Aggregation

Aggregate saved evidence:

```powershell
python python-sim\field_summary.py --input-dir logs --output logs\field_summary.csv
```

Replay accepted WAV evidence through the offline C++ decoder:

```powershell
python python-sim\field_replay.py --input-dir logs --rx-exe build-qt15\core\Release\hftext_rx_wav.exe
```

The summary parser accepts both current English evidence markers and older Portuguese markers.

## Receiver Performance Validation

Receiver performance is a correctness property for continuous operation, not
only a speed comparison. A build that eventually decodes a WAV but falls behind
live audio is not acceptable for unattended RX.

The benchmark corpus, metrics, target hardware, hashes, and phased acceptance
criteria are defined in `docs/16_receiver_performance_plan.md`. Preserve the
August 16 and 17, 2026 WAVs under local `Evidence/` storage and identify them by
the hashes in that plan.

Every receiver optimization must compare a Release build before and after the
change and record:

- audio duration, wall time, and offline real-time factor;
- captured, processed, current/peak pending, and dropped audio;
- accepted payloads and acceptance latency;
- sync, `PHYS_LENGTH`, Viterbi, CRC, and payload candidate counts;
- CPU model, logical cores, power mode, profile, and detailed-log state.

The minimum regression set is the very-weak accepted frame, both 300-second old
notebook captures, the long-message old notebook capture, synthetic short and
127-symbol Fast/Slow frames, corrupt/partial frames, and continuous channel
noise. Run the 30-minute live/corpus loop on the old notebook before declaring
the performance phase complete.

Replay an evidence directory through the instrumented streaming receiver:

```powershell
python -B python-sim\receiver_benchmark.py `
  --input-dir Evidence `
  --stream-exe build-performance\core\Release\hftext_stream_wav.exe
```

The runner writes `logs/receiver_benchmark.csv` and one JSON report per case
under `logs/receiver_benchmark_metrics/` by default. It prefers a WAV beside the
evidence TXT, even when the TXT contains an absolute path from another PC.

Measure one WAV directly when a corpus report is not needed:

```powershell
hftext_stream_wav.exe --mode 8fsk --symbol-duration 0.1 `
  --f0 1050 --f1 1180 --metrics-json metrics.json capture.wav
```

PC evidence now includes captured and processed audio durations, the active
worker block, session processing rate, decoder real-time factor, core workload
counters, and optional core stage timing. Stage timing is enabled with
`Detailed RX log`; ordinary RX keeps only the lower-cost counters and total
decoder processing time.

The August 17 old-notebook baseline is a required regression case: its live
receiver processed only `0.724x` real time, accumulated `119.80 s` pending, and
dropped `453.60 s` during a 34-minute session. Offline replay recovers all five
transmissions. Optimized builds must recover those same five payloads and then
pass the same-machine live no-drop test.

The first optimized Fast-profile run passed that test over `2,039.60 s`:
captured and processed durations matched, peak pending was `0.10 s`, no samples
were dropped, decoder headroom was `4.009x`, and all nine transmissions were
accepted. Five accepted payloads used the maximum 127-symbol length. Observed
process CPU fell from about `43%` to `23-25%` on the same notebook.

The optimized Slow-profile run also passed over `3,401.90 s`: seven frames were
accepted, including three 127-symbol payloads, decoder headroom was `3.303x`,
peak pending remained `0.10 s`, and no audio was dropped. The operator observed
immediate presentation, `23-26%` CPU, and stable `196 MB` RAM. Its saved final
window independently replayed both contained frames. Fast and Slow target-PC
performance validation is complete.

The rebuilt Android shared core accepted two Fast and two Slow transmissions,
with one short and one 127-symbol payload in each profile. Independent C++
streaming replay recovered all four saved modem WAV frames. The Fast capture
replayed at `21.504x` real time and the Slow capture at `22.024x`, completing the
cross-platform performance guardrail.

Android evidence level fields must summarize the complete saved raw/modem
window, not only the most recent live statistics block. This keeps exported
peak, clipping, and effective RMS gain meaningful when evidence is saved during
post-frame silence.

On-device verification after this correction reported a `23.21 s` saved window
with `43.0%` raw/modem peak, zero clipping, effective RMS gain `3.9x`, and an
accepted Fast frame with core latency rounded to `0.00 s`. These values matched
the visibly strong waterfall signal.

## Acceptance Criteria

A receive test is successful only when:

- a frame is detected;
- `PHYS_LENGTH` is valid;
- Viterbi produces a logical frame;
- logical `LENGTH` matches physical length;
- payload is valid;
- CRC is valid;
- the displayed text matches the transmitted payload.

No message should be shown as accepted without CRC and payload validation.

For continuous RX performance, success additionally requires no dropped audio,
no sustained pending-queue growth, exact benchmark payload parity, and the
real-time/latency targets in `docs/16_receiver_performance_plan.md`.

## Manual Release Smoke Test

After packaging:

1. Start the packaged PC app.
2. Confirm the UI is in English.
3. Confirm RX starts automatically if an input device exists.
4. Confirm `hftext.ini` exists beside `hftext_pc.exe`.
5. Switch between Fast and Slow in Operation and confirm the estimate and waterfall markers update.
6. Send a short message through the selected output device.
7. Confirm TX progress reaches 100%.
8. Decode a generated WAV through the CLI tools.
9. Save a log and RX evidence bundle.
10. Confirm the files contain English labels, speed profile, modem config path, and CSV section names.
11. Confirm version and protocol metadata appear in the app, logs, evidence, CLI tools, and `PACKAGE.txt`.
12. Enable `Pause RX during TX`, transmit with RX active, and confirm no local RX message is accepted and RX resumes afterward.
13. Cancel a second TX and confirm RX resumes; repeat with RX stopped and confirm TX does not start RX.

## Release Packaging

Create a Windows package:

```powershell
.\scripts\package_release.ps1
```

Useful options:

```powershell
.\scripts\package_release.ps1 -SkipBuild -SkipTests
.\scripts\package_release.ps1 -PackageName HFText-win64-release-local-test
```

Use the default path for normal releases; use `-SkipBuild` and `-SkipTests` only for quick local packaging after validation has already passed.

## Known Sources of Failure

- Low signal level.
- Clipping or saturation.
- Wrong sample rate.
- Symbol duration mismatch.
- Frequency/tone mismatch.
- Radio/SDR filtering that attenuates one or more tones.
- Propagation fading.
- Excessive noise or local interference.
