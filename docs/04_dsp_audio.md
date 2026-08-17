# DSP and Audio

## Goals

The DSP layer must generate and decode HFText audio tones reliably on ordinary sound cards, radio audio interfaces, and SDR audio paths.

Robustness remains the first design constraint. The current implementation
priority is reducing continuous receiver cost enough to stay ahead of real time
on older PCs, without weakening demonstrated weak-signal behavior.

## Audio Representation

The C++ core uses mono floating-point samples:

```text
-1.0 <= sample <= +1.0
```

WAV debug tools read and write PCM16 files. The PC app captures and plays PCM audio through platform audio APIs outside the core.

On Windows, physical input devices use the legacy `waveIn` capture path. Output
devices may also be captured with WASAPI loopback in shared mode. Loopback input
is converted from the endpoint's PCM or float mix format, averaged to mono, and
linearly resampled to the configured RX rate before it reaches the normal
streaming pipeline. This conversion belongs to the PC audio layer, not the modem
DSP core.

Some Windows output endpoints stop producing loopback packets while no
application is rendering audio. The PC backend therefore maintains an inaudible
shared render stream and groups the resulting mix into 100 ms blocks. Silence is
retained in the RX timeline; it is not removed or compressed. Physical input and
loopback consequently present equivalent block timing to evidence capture,
waterfall rendering, and the streaming modem.

Loopback captures the complete mix rendered to the selected endpoint. It does
not isolate the SDR process, and it may therefore contain system notifications,
browser audio, or HFText's own TX when they use the same output.

## Modulation

HFText uses non-coherent FSK-style modulation.

Configuration fields:

- sample rate;
- symbol duration;
- base frequency;
- tone spacing;
- tone count derived from the modulation mode;
- amplitude;
- preamble length.

2-FSK uses two tones. 4-FSK uses four equally spaced tones. 8-FSK uses eight equally spaced tones.

All tones must stay below Nyquist. For HF SSB field operation, tones should normally remain within the useful radio audio band, approximately 300 Hz to 3 kHz.

## Demodulation

The demodulator estimates tone energy in each symbol window and chooses the strongest tone. It also estimates confidence from the separation between the winning tone and the alternatives.

Tone analysis precomputes one complex oscillator step per configured tone and
advances it with a double-precision recurrence inside each symbol window. The
oscillators reset at every window. This preserves the existing tone-energy and
confidence model while avoiding per-sample trigonometric calls and inner-loop
energy allocations.

The receiver may use confidence for:

- `START_SYNC` search;
- `PHYS_LENGTH` recovery;
- Viterbi soft decisions;
- UI quality indicators and logs.

Confidence is diagnostic and improves decoding decisions, but CRC and payload validation remain the final acceptance criteria.

## Frequency Error

Real radio and SDR captures may include BFO/synthesizer error, sample-rate mismatch, filter skew, or operator tuning error.

The receiver should tolerate modest frequency error by testing small offsets around the configured tones. The waterfall tone markers help the operator see whether received tracks are shifted relative to the expected tones.

The live streaming receiver should keep this search bounded. Experimental long-symbol 8-FSK uses a reduced live hypothesis grid with fewer timing phases while still testing frequency offsets up to approximately +/-15 Hz. Wider or heavier searches belong in offline debug tools unless field evidence proves they are needed in normal operation.

## Timing

Symbol duration is configurable. Longer symbols improve tone energy at the cost of channel time. Shorter symbols improve speed but require better signal quality and timing.

The offline WAV decoder may try multiple initial sample offsets within a symbol. The normal streaming receiver must process audio continuously and avoid long multi-pass decoding after reception ends.

## Streaming Receiver

Normal RX operation is continuous:

```text
audio blocks
-> demodulated tone decisions
-> START_SYNC candidates
-> PHYS_LENGTH recovery
-> ROBUST_FRAME accumulation
-> Viterbi and frame validation
-> accepted message event
```

The receiver must avoid unbounded growth:

- audio queues are bounded;
- the recent evidence audio buffer is circular;
- detailed logs may drop or aggregate excessive events;
- stopping RX must not trigger a full offline decode of the whole capture.
- the live search grid must stay small enough that decoding does not build a long backlog after the audio frame has ended.
- a bounded queue is a safety limit, not a normal load-shedding mechanism;
- live processing rate, pending-audio trend, dropped samples, candidate load,
  and acceptance latency must be measurable in Release builds.

The staged optimization and regression guardrails are documented in
`docs/16_receiver_performance_plan.md`.

## Preamble and Synchronization

The preamble helps the radio/audio chain settle and gives the receiver visible tone activity. `START_SYNC` is the actual physical frame marker.

The receiver searches for `START_SYNC` in the recovered bit stream. After sync, it recovers `PHYS_LENGTH` and knows how many robust bits to accumulate.

This avoids waiting until the entire audio session ends and avoids scanning every possible payload length.

## Waterfall

The PC app waterfall is an operator aid, not part of decoding. It displays approximate audio energy between 300 Hz and 3 kHz.

Current behavior:

- blue for weak/normal signal energy;
- yellow for strong energy near saturation;
- red when the corresponding audio block is near full scale;
- vertical yellow markers at all configured/derived modem tones;
- slightly accelerated visual scroll for easier reading of short signals.

The waterfall must not block audio capture or modem decoding.

## Known Limitations

- No automatic gain control yet.
- No continuous fine timing tracker yet.
- No automatic carrier/tone tracking loop yet.
- 4-FSK and 8-FSK remain experimental until field data is broad enough.
