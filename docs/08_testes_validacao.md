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

## Field Validation

Useful field test matrix:

- 2-FSK, 4-FSK, and 8-FSK;
- symbol durations such as 0.1 s, 0.3 s, and 0.5 s;
- good signal, weak signal, noisy signal, and mistuned signal;
- direct speaker/microphone path;
- radio-to-SDR path;
- partial packets for negative testing.

For each test, save RX evidence from the app. Evidence should include the recent WAV, settings, logs, `Summary CSV`, and `Accepted Frames CSV`.

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
