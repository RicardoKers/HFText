# Testing and Validation

## Automated Tests

Run Python tests:

```powershell
python -m pytest python-sim/tests
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
- uppercase shift behavior;
- acute/tilde and `ç` behavior;
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
- automatic RX start;
- Fast/Slow speed profile selection from Operation;
- automatic `hftext.ini` creation when missing;
- RX restart when the speed profile, input device, or detailed-log setting changes;
- evidence and log export;
- waterfall tone markers;
- default-settings button;
- no console window in the packaged GUI application.
- Android debug shell builds successfully.
- Android JNI metadata bridge loads successfully and shows core metadata/profile summaries in the app.
- Android JNI text-preparation and TX-estimate bridge loads successfully and updates the app from the native core path.
- Android explicit TX audio generation loads successfully through JNI and plays with `AudioTrack` only after pressing `Send audio`.
- Android RX capture requests microphone permission, starts/stops `AudioRecord`, and updates RX level/clipping through the native C ABI.
- Android RX reports the selected microphone source and shows both raw peak and modem-input peak after limited digital gain.
- Android RX capture feeds audio blocks into the native streaming receiver and displays accepted messages only after core-side frame, payload, and CRC success.
- Android RX counts low-confidence receiver events so weak activity can be distinguished from a completely idle decoder.
- Android `Save RX audio` writes recent raw and modem-input WAV files that can be pulled with `adb` and replayed by the PC-side CLI tools.
- Android RX evidence reports captured duration and should be saved only after it covers the selected TX duration plus margin.
- Android `RX buffer` duration should advance in real time; slower growth indicates capture is blocked or audio data is being lost.

## Field Validation

Useful field test matrix:

- 2-FSK, 4-FSK, and 8-FSK;
- symbol durations such as 0.1 s, 0.3 s, and 0.5 s;
- good signal, weak signal, noisy signal, and mistuned signal;
- direct speaker/microphone path;
- radio-to-SDR path;
- partial packets for negative testing.

For each test, save RX evidence from the app. Evidence should include the recent WAV, settings, logs, `Summary CSV`, and `Accepted Frames CSV`.

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
