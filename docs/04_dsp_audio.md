# DSP and Audio

## Goals

The DSP layer must generate and decode HFText audio tones reliably on ordinary sound cards, radio audio interfaces, and SDR audio paths.

The first priority is robustness and debuggability. Throughput is secondary.

## Audio Representation

The C++ core uses mono floating-point samples:

```text
-1.0 <= sample <= +1.0
```

WAV debug tools read and write PCM16 files. The PC app captures and plays PCM audio through platform audio APIs outside the core.

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
