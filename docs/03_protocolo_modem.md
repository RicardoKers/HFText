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

The text alphabet has 64 symbols encoded as 6-bit values:

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
37 = .
38 = ,
39 = ?
40 = !
41 = /
42 = -
43 = +
44 = :
45 = ;
46 = @
47 = #
48 = $
49 = %
50 = &
51 = *
52 = (
53 = )
54 = _
55 = =
56 = <
57 = >
58 = \
59 = |
60 = shift
61 = acute
62 = tilde
63 = ç
```

Unsupported input characters must be replaced with `?`, making the substitution visible to the operator.

### Uppercase

Letters are transmitted as lowercase symbols. Uppercase letters use `shift` followed by the lowercase symbol.

```text
a  -> [a]
A  -> [shift, a]
Ab -> [shift, a, b]
AB -> [shift, a, shift, b]
```

On receive, `shift` affects only the next alphabetic symbol. If `shift` appears before a non-letter, the receiver ignores the shift and displays the following symbol normally. A trailing `shift` is ignored.

### Accents

`acute` and `tilde` are presentation modifiers. They do not represent standalone characters.

Rules:

- `acute + a/e/i/o/u` displays `á/é/í/ó/ú`;
- `tilde + a/o` displays `ã/õ`;
- `ç` uses symbol 63 directly;
- `Ç` uses `shift + ç`;
- uppercase accented vowels use modifier + `shift` + vowel;
- standalone typed acute or tilde characters are unsupported and become `?`;
- if a received modifier is followed by an invalid target, the receiver displays `?` before the target.

Examples:

```text
á -> [acute, a]
Á -> [acute, shift, a]
ã -> [tilde, a]
Ã -> [tilde, shift, a]
ç -> [ç]
Ç -> [shift, ç]
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
- uppercase letters count as 2 symbols;
- accented lowercase vowels count as 2 symbols;
- accented uppercase vowels count as 3 symbols;
- `ç` counts as 1 symbol and `Ç` counts as 2 symbols.

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
