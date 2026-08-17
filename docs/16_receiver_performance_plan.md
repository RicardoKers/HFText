# Receiver Performance Plan

## Priority

Reducing continuous receiver compute cost is the highest implementation priority
after HFText 0.6.0 field validation.

The objective is to keep PC RX ahead of real time on older hardware without
changing the on-air protocol or sacrificing the weak-signal behavior already
demonstrated by field captures. A bounded pending-audio queue is only a safety
limit; it is not a substitute for a receiver that processes audio faster than it
arrives.

## Evidence Behind the Priority

The August 16 and 17, 2026 evidence provides both a sensitivity guardrail and
performance stress cases:

| Capture | Profile and result | Performance observation |
| --- | --- | --- |
| `Evidence/Sinal muito fraco e decodificou/HFText-rx-evidence-20260816-191519.wav` | Slow 8-FSK, 9-symbol payload accepted with CRC OK at 17.1% reported confidence | 81.30 s saved, 0.10 s peak pending, no dropped samples |
| `Evidence/Computador fraco/HFText-rx-evidence-20260816-204434.wav` | Fast 8-FSK on a second-generation Core i5 notebook; two messages had been accepted live when evidence was saved | 300.00 s saved, 103.90 s current and 104.20 s peak pending, no dropped samples; about 30% process CPU was observed |
| `Evidence/Computador fraco/HFText-rx-evidence-20260816-212643.wav` | Fast 8-FSK, 66-symbol payload accepted with CRC OK at 98.7% reported confidence | 110.40 s saved, 28.60 s current and 28.70 s peak pending, no dropped samples |
| `Evidence/Ev1/HFText-rx-evidence-20260817-120142.wav` | Fast 8-FSK on the same old notebook; five visible transmissions were not accepted live | 2,082.70 s captured, 1,508.30 s processed, 119.80 s pending, and 453.60 s dropped; session rate 0.724x and about 43% process CPU |
| `Evidence/Ev2/HFText-rx-evidence-20260817-130726.wav` | Optimized Fast 8-FSK on the same old notebook; all nine transmissions were accepted live, including five 127-symbol payloads | 2,039.60 s captured and processed, 0.10 s peak pending, no dropped samples, decoder real-time factor 4.009x, and about 23-25% process CPU |
| `Evidence/Ev2/HFText-rx-evidence-20260817-141049.wav` | Optimized Slow 8-FSK on the same old notebook; all seven transmissions were accepted live, including three 127-symbol payloads | 3,401.90 s captured and processed, 0.10 s peak pending, no dropped samples, decoder real-time factor 3.303x, about 23-26% process CPU, and stable 196 MB RAM observed |

Offline replay of the 300-second capture finds four valid transmissions. Only
the first two had reached the live decoder when its report was saved; the last
two were still represented by pending audio. This indicates computational
backlog rather than an RF or protocol failure. The current PC queue permits up
to 120 seconds of pending audio, so a similar sustained deficit can eventually
discard samples during a long unattended session.

The August 17 capture closes that hypothesis. Its accounting is exact:
`1508.30 s processed + 119.80 s pending + 453.60 s dropped + 1.00 s active =
2082.70 s captured`. Offline replay recovers five `PU5LRK oi` frames at the five
visible modem intervals. The live failure is therefore computational
throughput, not RF quality, protocol validity, or missing transmissions.

The WAV files remain local and are intentionally ignored by Git. Their SHA-256
identifiers make the benchmark corpus unambiguous:

| Capture timestamp | Bytes | SHA-256 |
| --- | ---: | --- |
| `20260816-191519` | 7,804,844 | `ceff3a2f7734a0c1f9e9181bc4cffb7588689ac6569c0381bdd23956579cd1ea` |
| `20260816-204434` | 28,800,044 | `b42547e724f9510487501a2fb51b428d2ee516d414ceea98707b6c26e5d6add6` |
| `20260816-212643` | 10,598,444 | `38b2919ed75f04207aee3831f0793c23a30a4f2b260971e16717b6f8ac75bdcb` |
| `20260817-120142` | 28,800,044 | `ad0737b64d1f47b395790d1c6f3b875fc7fad0b76b01ca6108d606b451c3ecf7` |
| `20260817-130726` | 28,800,044 | `6acc2e9becdf9dc4beb0a76843a78d3fb93bb1ff7754183c0055ffad463004d0` |
| `20260817-141049` | 28,800,044 | `ef38564eb1fade255f011337038930312756de8b1f5d64c515bbfc1029e99226` |

## Performance Metrics

Receiver work must be measured in Release builds with structured output. The
primary metrics are:

- **Processing rate:** processed audio seconds divided by wall-clock seconds.
  A value above `1.0` is required for sustainable continuous RX.
- **Offline real-time factor:** WAV duration divided by replay wall time. This
  allows repeatable comparisons but does not replace live RX testing.
- **Pending audio:** current and peak queued audio duration, plus its trend over
  a long session.
- **Dropped audio:** duration and sample count removed from the pending queue.
- **Acceptance latency:** time between the last frame sample and the accepted
  message event.
- **Candidate load:** sync candidates, valid/invalid `PHYS_LENGTH` candidates,
  robust-frame decodes, Viterbi runs, CRC failures, and payload failures per
  audio minute.
- **Stage cost:** wall/CPU time spent in tone analysis, timing/frequency
  hypotheses, synchronization, physical-length recovery, and robust decoding.
- **Responsiveness:** longest receiver work item and visible UI stalls.

Process CPU is a useful secondary measurement only when comparing the same
machine, power mode, input, and build. It is not a portable pass/fail metric
because Task Manager percentages depend on core count and scheduling.

## Acceptance Targets

The target platform is the tested second-generation mobile Core i5 class of
hardware. Receiver optimization is complete only when all of these conditions
hold:

1. Existing Python, C++ core, C ABI, CLI, and application tests pass.
2. The three identified field WAVs produce the same accepted payloads as the
   HFText 0.6.0 baseline, including the 17.1% weak-signal frame.
3. A Release replay reaches an offline real-time factor of at least `1.10` on
   the target notebook for both Fast and Slow benchmark sets.
4. A 30-minute continuous channel/noise test has no dropped samples, no
   monotonically growing pending queue, and a peak pending duration no greater
   than 5 seconds.
5. Short, long, and 127-symbol frames in Fast and Slow are presented no later
   than 2 seconds after their final audio sample on the target notebook.
6. The same-machine sustained CPU measurement improves by at least 25% from the
   recorded baseline scenario, unless the real-time and latency targets are met
   and profiling proves another system component dominates the displayed CPU.
7. PC and Android sensitivity regressions are not introduced by shared-core
   changes.

The targets may be revised only from recorded benchmark data. Increasing the
pending queue or dropping audio is not an acceptable performance fix.

## Benchmark Set

The repeatable benchmark set must include:

- the three field captures identified above;
- quiet audio and ordinary channel noise without a valid frame;
- noise containing false sync and invalid `PHYS_LENGTH` candidates;
- truncated and CRC-corrupted frames;
- back-to-back valid frames;
- short and 127-symbol Fast and Slow frames;
- frequency offsets through the supported live range;
- a 30-minute continuous fixture built from representative channel audio.

Each run must record build type, commit, CPU model, logical-core count, Windows
power mode, profile, input duration, wall time, accepted payloads, all
performance metrics, and whether detailed logging was enabled. Detailed logging
must be benchmarked separately because it can alter cost.

## Implementation Sequence

### Phase 0: Measurement

1. Add cumulative captured/processed sample counters and a live processing-rate
   metric to PC evidence.
2. Add low-overhead stage and candidate counters to the streaming receiver.
3. Add optional stage timing suitable for profiling; keep verbose timing off in
   normal operation.
4. Extend `hftext_stream_wav` with machine-readable benchmark output and add a
   small corpus runner under `python-sim/`.
5. Record the unmodified baseline on the development PC and old notebook.

No algorithmic optimization should be accepted before this phase can show where
time is spent and reproduce the current outputs.

#### Phase 0 Status: August 17, 2026

The measurement foundation is implemented:

- `StreamingReceiverMetrics` records pushed samples, phase-symbol and bit work,
  sync search, physical-length work, robust candidates, and decoded frames;
- optional timing separates demodulation, frame search, robust decode, total
  push time, and the slowest push;
- PC evidence records captured/processed audio, active work, session rate,
  decoder real-time factor, workload counters, and optional stage timing;
- `hftext_stream_wav --metrics-json` writes schema version 1 JSON;
- `python-sim/receiver_benchmark.py` replays evidence corpora and writes a
  flattened CSV plus retained per-case JSON reports.

The first algorithm-compatible Release baseline on the development PC is:

| Capture | Audio | Replay | Real-time factor | Demodulation | Search | Robust decode | Robust attempts |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `20260816-191519` | 81.30 s | 37.57 s | 2.164x | 37.37 s | 0.17 s | 0.02 s | 168 |
| `20260816-204434` | 300.00 s | 100.43 s | 2.987x | 93.40 s | 2.52 s | 4.49 s | 10,204 |
| `20260816-212643` | 110.40 s | 36.41 s | 3.032x | 34.41 s | 0.80 s | 1.20 s | 3,157 |

All expected payloads passed, including the 17.1% weak-signal frame and four
valid transmissions in the 300-second WAV. Demodulation accounts for about 94%
to 99.5% of replay time in this corpus. Candidate/Viterbi work is secondary but
measurable in the noisy 300-second capture. The first optimization profile
should therefore examine repeated tone analysis across phases and frequency
hypotheses before reducing search coverage or tuning candidate thresholds.

The August 17 run supplies the instrumented old-notebook baseline: `0.724x`
session processing rate, `453.60 s` dropped audio, no accepted frame, and about
43% process CPU. Phase 0 is complete.

### Phase 1: Low-Risk Cost Removal

Use the profile to remove redundant allocations, copies, coefficient setup, and
repeated calculations. Reuse buffers and immutable tone/configuration data.
Every change must preserve benchmark payloads and receiver event semantics.

#### Phase 1 First Pass: August 17, 2026

The first pass replaced per-sample `sin()`/`cos()` evaluation with one
precomputed complex step per tone and a double-precision oscillator recurrence.
Tone-energy storage in the inner path is now fixed-size. Search coverage,
frequencies, timing phases, thresholds, confidence calculations, and protocol
behavior are unchanged.

Release replay on the development PC produced:

| Capture | Baseline replay | Optimized replay | Speedup | Optimized real-time factor | Frames |
| --- | ---: | ---: | ---: | ---: | ---: |
| `20260816-191519` | 37.57 s | 2.80 s | 13.4x | 29.024x | 1 |
| `20260816-204434` | 100.43 s | 19.62 s | 5.1x | 15.289x | 4 |
| `20260816-212643` | 36.41 s | 4.88 s | 7.5x | 22.635x | 1 |
| `20260817-120142` | 155.32 s | 13.74 s | 11.3x | 21.836x | 5 |

All expected payloads were preserved, including the 17.1% weak-signal frame.
For the August 17 stress WAV, measured demodulation fell from `149.88 s` to
`9.50 s`; search and robust-decode work remained secondary.

The optimized target-hardware run then passed the Fast-profile continuous test:

- session duration `2,039.60 s` (`33 min 59.60 s`);
- all `2,039.60 s` captured audio processed;
- `0.10 s` peak pending and zero dropped samples;
- decoder real-time factor `4.009x`;
- nine accepted transmissions, including five 127-symbol payloads;
- process CPU reduced from about `43%` to `23-25%`, a reduction of about
  `42-47%` on the same notebook.

The saved final 300-second window independently replays both contained frames,
one short and one 127-symbol payload, at `25.617x` real time on the development
PC. This satisfies the Fast continuous-RX, pending-queue, drop, payload, and CPU
targets.

The Slow target-hardware run then remained active for `3,401.90 s` (`56 min
41.90 s`) and produced:

- all captured audio processed, with `0.10 s` peak pending and zero drops;
- decoder real-time factor `3.303x`;
- seven accepted transmissions, including three 127-symbol payloads;
- immediate presentation reported by the operator, with `0.02 s` explicitly
  recorded for representative short frames;
- about `23-26%` process CPU and stable `196 MB` RAM throughout the run.

Its saved final 300-second window independently replays the contained short and
127-symbol frames at `24.470x` real time on the development PC. Fast and Slow
therefore meet the target-PC continuous-RX, bounded-queue, no-drop, payload,
latency, CPU, and memory-stability goals.

The rebuilt Android shared core also passed representative Fast and Slow
reception. Each profile accepted one short and one 127-symbol transmission. The
saved modem WAVs independently replayed both Fast frames at `21.504x` real time
and both Slow frames at `22.024x` on the development PC. Cross-platform receiver
performance validation is complete.

### Phase 2: Search and Candidate Efficiency

Status: deferred. Phase 1 exceeded the target-hardware acceptance thresholds,
so additional search changes are not justified without new evidence of a
performance or sensitivity problem.

Investigate, in measured order:

- reusing tone decisions across overlapping timing hypotheses;
- coarse-to-fine timing/frequency search;
- deduplicating equivalent sync candidates;
- rejecting impossible physical lengths before expensive robust decoding;
- preventing noisy audio from repeatedly launching equivalent Viterbi work.

Candidate gates must be validated against the weak-signal capture before they
are retained. Strong-signal-only tuning is not acceptable.

### Phase 3: Kernel Optimization

Optimize the measured tone-analysis or Viterbi hot paths with contiguous data,
vector-friendly loops, lookup tables, or equivalent numerical techniques.
Compare decisions, confidence, and decoded payloads against the baseline.

### Phase 4: Parallelism if Still Necessary

Only after the earlier phases, consider bounded parallel evaluation of
independent timing/frequency hypotheses or explicit SIMD. Threading must not
block audio capture, reorder accepted messages, create unbounded work, or make
performance worse on two-core hardware.

### Phase 5: Target-Hardware Validation

Run the complete corpus and the 30-minute continuous test on the old notebook,
then repeat representative PC, Android, microphone, cable, and SDR field paths.
Save benchmark reports with the release evidence.

Fast and Slow PC target-hardware validation is complete. Representative Android
reception after rebuilding the shared optimized core is also complete.

## Guardrails

- Do not change HFText Basic v0.1 or Text Codec v0.2 for this work.
- Do not reduce timing, frequency, or weak-signal coverage without corpus proof.
- Do not use a larger queue to hide insufficient processing rate.
- Do not make detailed profiling mandatory in normal operation.
- Keep platform audio and UI concerns outside the portable DSP core.
- Optimize one measured bottleneck at a time, with focused tests and benchmark
  comparison after each change.
