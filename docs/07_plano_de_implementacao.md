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
- Shared core-level Fast/Slow profile defaults and modem-setting validation for PC and future Android reuse.
- Shared core-level TX helpers for PC, CLI, and future Android reuse.
- Shared core-level tone-frequency and audio-statistics helpers for diagnostics and future Android reuse.
- Shared core-level RX event summary helpers for PC and future Android diagnostics.

## Current Focus

The current focus is field validation and operator usability:

- compare 2-FSK, 4-FSK, and 8-FSK with real radio/SDR captures;
- keep continuous RX responsive under noise and false candidates;
- improve diagnostics without cluttering normal operation;
- package repeatable Windows releases for testing on multiple computers.

## Near-Term Tasks

1. Validate 8-FSK in real HF/SDR captures.
2. Compare symbol durations such as 0.1 s, 0.3 s, and 0.5 s in each modulation mode.
3. Review field evidence CSVs for acceptance rate, quality, and failure modes.
4. Improve frequency/timing tolerance only when field logs show a repeatable weakness.
5. Keep the PC interface polished and simple.
6. Refresh release packages after meaningful user-visible changes.
7. Keep the user guide aligned with the packaged interface.

## Future Tasks

- Automatic frequency/tone tracking.
- Automatic gain or level guidance.
- More structured message history.
- Replay selected evidence files from the app.
- Android application.
- ACK/retry or repetition as explicit future protocol versions.

## Guardrails

- Do not change the protocol silently.
- Do not add large UI or DSP rewrites without tests.
- Do not remove WAV tools; they are still important for debugging.
- Keep the robust mode as the normal path.
- Keep experimental modes clearly labeled until field data justifies promotion.
