# Implementation Plan

## Development Order

1. Python simulation.
2. C++ core.
3. CLI tools.
4. PC application.
5. Field validation.
6. Android application.

## Completed Baseline Work

- Repository structure.
- Python text codec, CRC, frame build/parse, 2-FSK modulation and demodulation.
- Python WAV TX/RX tools.
- Noise and channel sweep scripts.
- C++ text codec, frame, CRC, modulation, demodulation, robust layer, and streaming receiver.
- CLI WAV tools.
- Qt PC app with direct TX and continuous RX.
- Field evidence export and aggregation tools.
- 4-FSK and 8-FSK experimental modes.
- Waterfall tone markers and saturation-aware palette.
- English UI/log wording.
- Repeatable Windows release packaging script.
- Basic GitHub Actions CI for Python simulation and C++ core tests.
- Visible app, CLI, log, and evidence version metadata.
- Shared core-level Fast/Slow profile defaults and modem-setting validation for PC and Android reuse.
- Shared core-level TX helpers for PC, CLI, and Android reuse.
- Shared core-level tone-frequency and audio-statistics helpers for diagnostics and Android reuse.
- Shared core-level RX event summary helpers for PC and Android diagnostics.
- Portable C ABI foundation and shared-library target for JNI integration, including metadata, profiles, modem config, prepared TX text, tone frequencies, audio statistics, TX estimates, generated TX audio buffers, and streaming RX block processing.
- C ABI compile, link, export, and runtime dynamic-loading regression tests.
- Android JNI bridge for metadata, text preparation, TX estimates, generated TX audio, audio statistics, and streaming RX blocks.
- Android explicit AudioTrack TX and initial AudioRecord streaming RX through the native receiver.
- Optional persistent RX decoder pause during TX on PC and Android for acoustic echo prevention.

## Current Focus

The receiver compute-cost priority is complete. Fast and Slow remained ahead of
real time during long tests on a second-generation Core i5 notebook, with zero
sample drops and substantially lower CPU use. The measurements and guardrails
remain documented in `docs/16_receiver_performance_plan.md`.

The current focus is field validation, release quality, and small operational
improvements that do not silently change the on-air protocol.

## Near-Term Tasks

1. Continue representative radio/SDR and Android regression tests.
2. Preserve documented weak-signal and performance metrics when changing DSP.
3. Keep PC and Android operation ergonomics aligned where practical.
4. Improve candidate/search behavior only with repeatable field evidence.
5. Keep Windows and Android release packaging reproducible.

## Future Tasks

- Automatic frequency/tone tracking.
- Automatic Fast/Slow receive-profile detection from the known preamble.
- Automatic gain or level guidance.
- Further Android conversation UI refinements after the TX/RX chat-style history is field-tested.
- Replay selected evidence files from the app.
- Android UI polish, richer evidence/log export, and device validation.
- ACK/retry or repetition as explicit future protocol versions.

## Guardrails

- Do not change the protocol silently.
- Do not add large UI or DSP rewrites without tests.
- Do not remove WAV tools; they are still important for debugging.
- Keep the robust mode as the normal path.
- Keep experimental modes clearly labeled until field data justifies promotion.
