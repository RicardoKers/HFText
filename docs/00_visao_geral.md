# HFText Project Overview

## Description

HFText is a digital text communication system over audio. A user message is encoded into a bit stream, protected by a robust coding layer, modulated as audio tones, and sent through an HF radio audio path.

At the receiver, audio from the radio or SDR is captured, demodulated, decoded, checked with CRC, and shown as text.

## Use Case

1. The operator types a short message.
2. The transmitter adds the configured callsign, builds the logical frame, applies FEC and interleaving, and generates audio tones.
3. The audio is sent to the radio.
4. Another station receives the signal.
5. The receiving software listens continuously, detects a frame, decodes it, validates the CRC, and displays the message.

## Technical Goal

The current operational mode is the robust HFText Basic v0.1 baseline:

```text
SYNC | LENGTH | PAYLOAD | CRC16
-> convolutional code conv_k3
-> deterministic interleaving
-> PREAMBLE | START_SYNC | PHYS_LENGTH | ROBUST_FRAME
-> 2-FSK
```

4-FSK v0.2 and 8-FSK v0.3 are experimental physical modulation modes. They reuse the same logical frame and robust layer.

## Desired Characteristics

- Low throughput, optimized for weak HF signals.
- Non-coherent demodulation.
- Tolerance to moderate frequency error, timing error, noise, and fading.
- Portable C++ modem core.
- PC application for real field validation.
- Future Android application using the same core.

## Current Implementation

- Text codec with a 6-bit alphabet, uppercase shift, acute/tilde modifiers, and `ç`.
- Logical framing with CRC-16/CCITT-FALSE.
- Robust layer with convolutional code `conv_k3`, deterministic interleaving, physical `PHYS_LENGTH`, and Viterbi decoding.
- 2-FSK baseline plus experimental 4-FSK and 8-FSK.
- Python simulation and sweep tools.
- C++ core, C ABI foundation for JNI reuse, CLI tools, and tests.
- Qt PC app with direct TX, continuous RX, waterfall, RX diagnostics, logs, and field evidence export.
- Minimal Android Kotlin/Compose shell, without JNI or audio integration yet.

## Not Yet Implemented

- Android JNI bridge and audio integration.
- Automatic gain control.
- Fine continuous carrier/timing tracking.
- ACK or retry protocol.
- Operational repetition mode.
- Encryption, intentionally out of scope.
