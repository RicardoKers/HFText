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

The current focus is receiver compute-cost reduction, based on field evidence
from a second-generation Core i5 notebook:

- establish structured Release-build performance measurements;
- keep continuous RX ahead of real time under noise and false candidates;
- preserve the accepted very-weak-signal regression capture;
- reduce message latency and prevent pending-audio growth on older PCs;
- continue field validation while the receiver changes remain behaviorally
  compatible.

The detailed sequence and exit criteria are in
`docs/16_receiver_performance_plan.md`.

## Near-Term Tasks

1. Record the instrumented Release baseline on the old notebook.
2. Profile the tone-demodulation kernel identified by the development-PC corpus.
3. Remove the largest measured repeated tone-analysis, allocation, or setup costs
   and verify exact corpus parity after each change.
4. Improve candidate/search efficiency only with weak-signal regression proof.
5. Validate 30-minute continuous RX, Fast/Slow latency, and zero sample drops on
   the target notebook.
6. Continue representative radio/SDR and Android regression tests.
7. Resume lower-priority UI refinement after the performance exit criteria are
   met or a critical usability defect requires attention.

## Future Tasks

- Automatic frequency/tone tracking.
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
