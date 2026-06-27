# Instructions for Codex Agents

This project implements a simple and robust digital text modem over audio, intended for HF radio use.

Development must be incremental, testable, and well documented.

## Main Rules

1. Do not implement large unrelated blocks at once.
2. Before coding, read the documents in `docs/`.
3. Keep the DSP core independent from GUI, Android, Qt, and platform audio APIs.
4. Every new core feature must have tests.
5. Do not remove files without justification.
6. Do not change the protocol without updating `docs/03_protocolo_modem.md`.
7. Prioritize clarity, robustness, and testability.
8. Do not implement encryption.
9. Do not transmit automatically without an explicit user action.
10. Do not start with Android; the correct order is Python simulation, C++ core, CLI, PC app, then Android.

## Desired Architecture

The project is divided into:

- `python-sim/`: simulation, WAV generation, mathematical tests, and initial validation.
- `core/`: portable C++ modem core.
- `pc-app/`: PC application.
- `android-app/`: Android application.
- `docs/`: requirements, architecture, protocol, and validation documentation.

## Current Protocol

Use HFText Basic v0.1:

```text
SYNC | LENGTH | PAYLOAD | CRC16

Transmitted physical flow:

PREAMBLE | START_SYNC | PHYS_LENGTH | ROBUST_FRAME

Rules:

SYNC = 0x2DD4, 2 bytes.
START_SYNC = 0x2DD4 0x2DD4, 32 bits, transmitted directly before PHYS_LENGTH.
PHYS_LENGTH = LENGTH_BYTE repeated 3 times, 24 bits MSB-first, transmitted directly before ROBUST_FRAME.
LENGTH is 1 byte and uses only the lower 7 bits.
Bit 7 of LENGTH must be zero.
LENGTH is the number of 6-bit symbols in PAYLOAD.
Valid LENGTH values: 0 to 127.
PHYS_LENGTH represents the same number of PAYLOAD symbols and must match LENGTH after Viterbi decoding.
PAYLOAD has at most 127 6-bit symbols.
The callsign is not a separate field; when configured, the transmitter inserts it at the beginning of PAYLOAD followed by one space.
The active text alphabet is Text Codec v0.2.
Symbols 0-62 form the base layer: space, lowercase letters, digits, and uppercase ASCII letters.
Symbol 63 is shift and applies only to the next symbol.
The shifted layer carries newline, punctuation, accents, ç/Ç, ñ/Ñ, ü/Ü, degree sign, and other common symbols.
Shift + reserved, shift + shift, or a trailing shift displays ?.
Unsupported characters must be replaced with ?.
CRC16 is CRC-16/CCITT-FALSE over PAYLOAD packed into bytes.
6-bit symbols are packed into bytes MSB-first, with zero padding in the final byte.
SYNC and CRC16 are serialized big-endian; the full logical frame becomes MSB-first bits before modulation.
TX uses a 64-bit preamble before START_SYNC: alternating tones in 2-FSK, a four-tone cycle in 4-FSK, and an eight-tone cycle in 8-FSK.
RX searches for START_SYNC in the bit stream, recovers PHYS_LENGTH, and accumulates the known-size ROBUST_FRAME.
Experimental 8-FSK v0.3 uses the same fields and robust layer; each audio symbol carries 3 bits MSB-first.
In 8-FSK, START_SYNC does not need to start at a bit index that is a multiple of 3.
When symbol confidence is available, RX may weight START_SYNC, PHYS_LENGTH, and Viterbi decisions by that confidence.
Normal RX operation must be continuous and process audio blocks during reception; closed WAV files are a debug tool.
Offline RX may try multiple initial sample offsets inside the symbol to improve timing alignment.
Do not include SYNC or LENGTH in the CRC.
Do not include START_SYNC or PHYS_LENGTH in the CRC.
```

## Historical Initial Implementation

The first implementation used:

- Python;
- 2-FSK;
- offline WAV files;
- CRC16;
- no FEC;
- no interleaving;
- no real-time reception.

The current implementation uses one robust mode with `conv_k3 + interleaving`, physical `PHYS_LENGTH`, continuous RX in the PC app, and confidence-weighted synchronization/Viterbi in C++ when symbol confidence is available. The standard modulation baseline is 2-FSK v0.1; 4-FSK v0.2 and 8-FSK v0.3 are experimental selectable modes.

## Python Style

- Use small functions.
- Use NumPy for audio buffers.
- Use pytest for tests.
- Save generated WAV files in `python-sim/generated/`.
- Do not mix CLI scripts with library code.

## C++ Style

- Use C++17 or newer.
- Use `std::vector<float>` for audio.
- Keep samples normalized between `-1.0` and `+1.0`.
- Avoid platform dependencies in `core/`.
- Separate headers and sources.

## Recommended Flow

1. Text encoding and sanitization in Python.
2. CRC16 in Python.
3. Frame build and parse.
4. 2-FSK modulator.
5. 2-FSK demodulator.
6. TX/RX WAV scripts.
7. Noise validation.
8. Port to C++.
