# Modem Protocol

## Purpose

This document defines the HFText frame format, text alphabet, robust coding layer, and physical modulation modes.

## Operational Baseline: HFText Basic v0.1

HFText Basic v0.1 is the conservative operational baseline. It uses one robust transmit mode. There is no supported normal mode without FEC and interleaving.

Logical frame:

```text
SYNC | LENGTH | PAYLOAD | CRC16
```

Before modulation, the logical frame is transformed into the transmitted physical flow:

```text
logical frame
-> convolutional code rate 1/2, K=3, generators 111 and 101
-> deterministic rectangular interleaving
-> PREAMBLE | START_SYNC | PHYS_LENGTH | ROBUST_FRAME
-> 2-FSK
```

Incompatible changes to the frame, field semantics, FEC, interleaving, or modulation must be documented as a later protocol version, not as a silent v0.1 variation.

The active text representation is Text Codec v0.2, documented below and in
`docs/13_text_codec_v02.md`. It is an incompatible replacement for the original
v0.1 alphabet; no legacy text-codec compatibility mode is implemented.

## Experimental Physical Modes

### HFText v0.2 Experimental 4-FSK

4-FSK v0.2 reuses the same logical frame, robust layer, `START_SYNC`, and `PHYS_LENGTH`. Each audio symbol carries 2 bits MSB-first.

```text
logical frame v0.1
-> robust layer
-> PREAMBLE | START_SYNC | PHYS_LENGTH | ROBUST_FRAME
-> 4-FSK
```

4-FSK is explicit and experimental.

### HFText v0.3 Experimental 8-FSK

8-FSK v0.3 reuses the same logical frame, robust layer, `START_SYNC`, and `PHYS_LENGTH`. Each audio symbol carries 3 bits MSB-first.

```text
logical frame v0.1
-> robust layer
-> PREAMBLE | START_SYNC | PHYS_LENGTH | ROBUST_FRAME
-> 8-FSK
```

In 8-FSK, `START_SYNC` does not need to start at a bit index that is a multiple of 3. The receiver must search the recovered bit stream and accept only frames that pass `PHYS_LENGTH`, Viterbi, logical `LENGTH`, payload validation, and CRC.

8-FSK is explicit and experimental.

## Text Alphabet

HFText uses Text Codec v0.2. The physical alphabet still has 64 symbols encoded
as 6-bit values, but symbol `63` is a shift prefix that selects a second layer
for the next symbol only.

Base symbols:

```text
0  = space
1  = a
2  = b
...
26 = z
27 = 0
28 = 1
...
36 = 9
37 = A
38 = B
...
62 = Z
63 = shift
```

Shifted symbols:

```text
shift 0  = newline
shift 1  = á
shift 2  = à
shift 3  = â
shift 4  = ã
shift 5  = é
shift 6  = ê
shift 7  = í
shift 8  = ó
shift 9  = ô
shift 10 = õ
shift 11 = ú
shift 12 = ü
shift 13 = ç
shift 14 = ñ
shift 15 = .
shift 16 = ,
shift 17 = ?
shift 18 = !
shift 19 = :
shift 20 = ;
shift 21 = '
shift 22 = "
shift 23 = -
shift 24 = _
shift 25 = /
shift 26 = \
shift 27 = +
shift 28 = =
shift 29 = *
shift 30 = %
shift 31 = &
shift 32 = #
shift 33 = @
shift 34 = $
shift 35 = <
shift 36 = >
shift 37 = (
shift 38 = )
shift 39 = [
shift 40 = ]
shift 41 = {
shift 42 = }
shift 43 = |
shift 44 = Á
shift 45 = Â
shift 46 = Ã
shift 47 = É
shift 48 = Ê
shift 49 = Í
shift 50 = Ó
shift 51 = Ô
shift 52 = Õ
shift 53 = Ú
shift 54 = Ü
shift 55 = Ç
shift 56 = Ñ
shift 57 = `
shift 58 = ~
shift 59 = ^
shift 60 = °
shift 61 = reserved
shift 62 = reserved
shift 63 = invalid
```

Unsupported input characters must be replaced with `?`, making the substitution
visible to the operator. Since `?` is in the shifted layer, each replacement
uses two payload symbols: `shift 17`.

### Shift Rules

`shift` affects only the next symbol.

Rules:

- `shift + reserved`, `shift + shift`, or trailing `shift` displays `?`;
- unshifted symbols `61` and `62` are normal base symbols `Y` and `Z`;
- `shift` is not a persistent case state;
- newline is encoded as `shift 0`.

Examples:

```text
a  -> [a]
A  -> [A]
á  -> [shift, a]
Á  -> [shift, H]
.  -> [shift, o]
?  -> [shift, q]
°  -> [shift, X]
```

## Logical Frame

```text
SYNC | LENGTH | PAYLOAD | CRC16
```

### SYNC

`SYNC` is the fixed logical-frame marker.

```text
value: 0x2DD4
size:  2 bytes
byte order: big-endian
```

`SYNC` is not included in the CRC.

The physical `START_SYNC` uses the same base value repeated twice before `PHYS_LENGTH | ROBUST_FRAME`. Logical `SYNC` is visible only after robust decoding.

### LENGTH

`LENGTH` is the number of encoded 6-bit symbols in `PAYLOAD`.

Rules:

- size: 1 byte;
- only the lower 7 bits are used;
- bit 7 must be zero;
- valid range: 0 to 127;
- base-layer characters, including uppercase ASCII letters, count as 1 symbol;
- shifted-layer characters count as 2 symbols;
- unsupported characters are sanitized to `?` and count as 2 symbols.

### PAYLOAD

`PAYLOAD` is the transmitted message after callsign insertion and sanitization.

Rules:

- length: 0 to 127 encoded 6-bit symbols;
- uses the alphabet above;
- unsupported characters become `?`;
- transmit must reject payloads that exceed 127 encoded symbols.

### Callsign

The callsign is not a separate protocol field.

When configured, the transmitter inserts it at the beginning of `PAYLOAD`, followed by one space.

```text
callsign: pu5lrk
typed text: test
payload text: pu5lrk test
```

The callsign can have any length as long as `callsign + space + message` fits within the 127-symbol payload limit.

### CRC16

CRC16 uses CRC-16/CCITT-FALSE over the packed `PAYLOAD` only.

```text
polynomial: 0x1021
initial:    0xFFFF
RefIn:      false
RefOut:     false
XorOut:     0x0000
```

Do not include `SYNC`, `LENGTH`, `START_SYNC`, or `PHYS_LENGTH` in the CRC.

The CRC value is serialized big-endian.

## Packing 6-Bit Symbols

PAYLOAD symbols are packed into bytes MSB-first.

Each 6-bit symbol is emitted from most significant bit to least significant bit. The continuous bit stream is split into 8-bit bytes, also MSB-first. If the last byte is incomplete, remaining low bits are zero padded.

On receive, `LENGTH` defines how many 6-bit symbols to recover. Padding bits at the end are ignored.

The full logical frame is also converted to bits MSB-first before robust encoding.

## Physical Flow

```text
PREAMBLE | START_SYNC | PHYS_LENGTH | ROBUST_FRAME
```

The physical preamble prepares detection and helps radio TX/VOX behavior. It is not part of the logical frame and is not included in the CRC.

### PREAMBLE

Initial length:

```text
64 bits
```

2-FSK v0.1 preamble:

```text
10101010 ...
```

4-FSK v0.2 experimental preamble cycles through all tones:

```text
00 01 10 11 00 01 10 11 ...
```

8-FSK v0.3 experimental preamble cycles through all tones:

```text
000 001 010 011 100 101 110 111 000 ...
```

### START_SYNC

`START_SYNC` is transmitted directly, without FEC or interleaving, immediately before `PHYS_LENGTH`.

```text
value: 0x2DD4 0x2DD4
size:  32 bits
order: big-endian, MSB-first
```

### PHYS_LENGTH

`PHYS_LENGTH` is a physical copy of the payload length transmitted directly after `START_SYNC` and before the robust frame.

```text
format: LENGTH_BYTE repeated 3 times
size:   24 bits
order:  MSB-first
```

Rules:

- each repetition is one byte;
- only the lower 7 bits are used;
- bit 7 must be zero;
- valid range: 0 to 127;
- each bit is recovered by majority vote across the three repetitions;
- `PHYS_LENGTH` is not part of the logical frame and is not included in the CRC;
- `PHYS_LENGTH` must match logical `LENGTH` after Viterbi decoding.

The purpose of `PHYS_LENGTH` is to let the streaming receiver know the exact robust block size before the end of reception.

## Robust Layer

TX:

```text
logical frame
-> convolutional encoder rate 1/2, K=3, generators 111 and 101
-> zero tail bits to return to state zero
-> deterministic rectangular interleaving
-> physical sync/length wrapper
```

RX:

```text
demodulated bits
-> START_SYNC search, optionally confidence-weighted
-> PHYS_LENGTH recovery, optionally confidence-weighted
-> known-size ROBUST_FRAME accumulation
-> deinterleaving
-> Viterbi hard-decision or soft-decision
-> logical frame parse
-> CRC and payload validation
```

Rules:

- the CRC protects only the original packed payload;
- interleaving is applied after FEC and reversed before Viterbi;
- interleaving geometry is derived from the encoded size;
- RX may use symbol confidence for sync, length, and Viterbi decisions;
- text is accepted only when CRC and payload validation pass.

Deterministic interleaving shape:

- calculate the encoded stream size after FEC;
- consider only row counts that divide that size exactly;
- initially limit rows to `2..16`;
- choose the row count closest to 6;
- on ties, choose the smaller row count;
- columns = encoded size / rows.

## Modulation

### 2-FSK v0.1

```text
bit 0 -> base frequency
bit 1 -> base frequency + tone spacing
```

Default parameters:

```text
sampleRate = 48000 Hz
symbolDuration = 0.5 s
baseFrequency = 1200 Hz
toneSpacing = 400 Hz
toneCount = 2
```

### 4-FSK v0.2 Experimental

```text
00 -> base
01 -> base + toneSpacing
10 -> base + 2 * toneSpacing
11 -> base + 3 * toneSpacing
```

Rules:

- bit pairs are formed MSB-first;
- if needed, the final audio symbol is padded with zero in the least significant bit;
- `PHYS_LENGTH` still counts 6-bit payload symbols, not audio symbols;
- TX duration accounts for 2 bits per audio symbol;
- all tones must stay below Nyquist and inside the intended radio audio passband.

### 8-FSK v0.3 Experimental

```text
000 -> base
001 -> base + toneSpacing
010 -> base + 2 * toneSpacing
011 -> base + 3 * toneSpacing
100 -> base + 4 * toneSpacing
101 -> base + 5 * toneSpacing
110 -> base + 6 * toneSpacing
111 -> base + 7 * toneSpacing
```

Rules:

- bit triples are formed MSB-first;
- if needed, the final audio symbol is padded with zeros in least significant bits;
- `PHYS_LENGTH` still counts 6-bit payload symbols, not audio symbols;
- TX duration accounts for 3 bits per audio symbol;
- all tones must stay below Nyquist and, for HF SSB, should typically remain between 300 Hz and 3 kHz.

## Future Versions

The following ideas are not part of v0.1, v0.2, or v0.3:

- operational repetition;
- ACK/retry;
- optional timestamp;
- message type field;
- automatic link negotiation.

If promoted, they must define a later protocol version.
