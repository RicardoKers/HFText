# Codex Backlog

This backlog is intentionally incremental. Do not implement multiple unrelated items in one pass.

## Done

### Repository and Documentation

- Initial repository structure.
- Project overview, requirements, architecture, protocol, DSP, PC app, Android plan, validation plan, and backlog.
- Documentation updated for HFText Basic v0.1, experimental 4-FSK v0.2, and experimental 8-FSK v0.3.
- Product-facing language standardized to English.
- Basic user guide.
- Android placeholder directory, Windows setup guide, and environment-check script.
- C ABI reference for JNI integration.
- Minimal Android Kotlin/Compose shell with Gradle wrapper and debug build script.
- Minimal Android JNI bridge for C ABI metadata and Fast/Slow profile summaries.
- Android JNI bridge for text preparation, payload preview, symbol counts, and TX estimates.
- Android JNI bridge for generated TX audio.
- Android AudioTrack TX after explicit operator action, with cancel/stop behavior.
- Android AudioRecord capture with microphone permission, stop/release behavior, and C ABI audio level/clipping statistics.
- Android AudioRecord blocks streamed through the C ABI receiver with accepted messages and filtered RX status in the UI.
- Android RX diagnostic improvements: selectable microphone source, fallback source label, limited modem-input gain, raw/modem level display, low-level hint, and low-confidence event count.
- Android RX debug WAV export for recent raw and modem-input audio.
- Android RX buffer-duration display and saved-evidence duration warning.
- Android RX capture/decoder thread separation so evidence capture can stay real-time when decoding lags.
- Android Fast RX evidence replayed successfully on PC from Xiaomi POCO F1 captures.
- Android Slow RX latency reduced by bounding the live 8-FSK streaming search grid while preserving +/-15 Hz frequency-offset coverage.
- Android RX evidence WAV export switched to buffered chunked PCM writes.

### Python Simulation

- 6-bit text codec.
- Uppercase shift.
- Acute, tilde, and `ç` support.
- Unsupported-character replacement with `?`.
- CRC-16/CCITT-FALSE.
- Frame build/parse.
- 2-FSK modulation and demodulation.
- WAV TX/RX scripts.
- Noise, channel, FEC, interleaving, repetition, and MFSK sweeps.
- Field evidence aggregation and replay helpers.

### C++ Core

- Portable core types.
- Text codec compatible with Python.
- CRC and frame handling.
- Robust layer with `conv_k3`, deterministic interleaving, and Viterbi.
- 2-FSK baseline.
- Experimental 4-FSK and 8-FSK.
- Confidence-aware demodulation and streaming receiver.
- Shared application modem settings for Fast/Slow profiles and default validation.
- Shared application TX helpers for callsign insertion, estimates, and audio generation.
- Shared audio statistics and tone-frequency helpers for diagnostics.
- Shared RX event summary helpers for progress, quality, and session counters.
- Portable C ABI foundation for JNI integration, including prepared TX text, tone frequencies, audio statistics, generated TX audio buffers, and streaming RX block processing.
- C ABI usage contract documented for JNI integration.
- C compilation test for the public C ABI header.
- Shared-library target and link test for the portable C ABI.
- Explicit public-symbol export macro for the C ABI shared-library target.
- Runtime dynamic-loading, full public-symbol lookup, helper-call, and generated-audio roundtrip test for the portable C ABI shared-library target.
- CLI tools for TX WAV, RX WAV, and streaming WAV replay.
- Regression tests.

### PC Application

- Qt 6 Widgets app.
- Chat-like Operation tab.
- Settings tab for callsign, audio devices, RX control, logs, and evidence export.
- Fast/Slow speed profile selector in Operation.
- Editable `hftext.ini` for advanced modem parameters.
- Direct sound-card TX.
- Continuous sound-card RX.
- Automatic RX start.
- Automatic RX restart when receive settings change.
- TX progress and cancel behavior.
- Waterfall from 300 Hz to 3 kHz with tone markers.
- Blue/yellow/red waterfall palette for signal level and saturation hints.
- RX evidence export with WAV, log, summary CSV, and accepted-frame CSV.
- Log export.
- Load Defaults button.
- HFText app icon and release packaging.
- Visible app, CLI, log, and evidence version metadata.
- Repeatable Windows release packaging script.
- Basic GitHub Actions CI.

## Current Validation Tasks

1. Collect real radio/SDR evidence for 8-FSK.
2. Compare 2-FSK, 4-FSK, and 8-FSK under similar propagation.
3. Compare symbol durations, especially 0.1 s, 0.3 s, and 0.5 s.
4. Track field acceptance rate and quality using `field_summary.py`.
5. Replay accepted evidence with `field_replay.py` after decoder changes.
6. Watch for repeated failure causes: missed sync, invalid `PHYS_LENGTH`, CRC failure, payload failure, latency after frame end, evidence-save time, or UI responsiveness.

## Near-Term UI Tasks

- Review final English wording in the packaged app.
- Keep `docs/10_user_guide.md` aligned with the current interface.
- Keep the Operation tab visually simple.
- Keep advanced modem parameters in `hftext.ini` unless field operation proves a setting belongs in the UI.
- Improve message history only if field use shows a need.
- Consider a clearer receive/tuning aid if waterfall markers are not enough.
- Keep debug tools in Settings, not in the normal operation path.

## Near-Term DSP Tasks

- Validate 8-FSK with real captures before promoting it.
- Improve frequency tolerance only from repeatable evidence.
- Consider a lightweight frequency tracking loop if mistuning remains common.
- Consider a timing tracker if symbol-boundary drift appears in long captures.
- Keep 2-FSK as the conservative baseline until field data says otherwise.

## Future Protocol Tasks

These require explicit protocol-version planning:

- operational repetition;
- ACK/retry;
- message type;
- optional timestamp;
- negotiation or mode announcement;
- automatic link adaptation.

## Android Tasks

Android remains incremental and should continue to reuse the PC/core behavior:

1. Improve Android operation UI without duplicating modem logic in Kotlin.
2. Reuse shared C++ tone-frequency helpers for tuning UI.
3. Reuse shared C++ RX event summary helpers for richer status and logs.
4. Add evidence/log export where practical.
5. Add device/emulator validation around JNI TX/RX flows when the workflow stabilizes.

## Release Tasks

For each meaningful user-visible change:

1. Build Release.
2. Run C++ tests.
3. Run Python tests.
4. Package the Windows app with dependencies.
5. Smoke-test the packaged executable and CLI tools.
6. Keep old release artifacts only when they are still useful.
