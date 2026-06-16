# HFText C++ Core

`core/` contains the portable C++ modem implementation. It must remain independent from Qt, Android, sound card APIs, and UI code.

## Build

From the repository root:

```powershell
cmake -S . -B build-qt15
cmake --build build-qt15 --config Release
ctest --test-dir build-qt15 -C Release --output-on-failure
```

The exact build directory name is not important. Existing local build folders such as `build-qt15` are used only for developer convenience.

## Main Components

- `include/`: public core headers.
- `src/`: encoder, frame, CRC, robust layer, modulation, demodulation, streaming RX.
- `tests/`: C++ regression tests.
- `tools/`: CLI tools for WAV transmit, WAV receive, and streaming WAV replay.

## CLI Tools

Generate a WAV:

```powershell
.\build-qt15\core\Release\hftext_tx_wav.exe --callsign pu5lrk "Test" generated\test.wav
```

Decode a WAV:

```powershell
.\build-qt15\core\Release\hftext_rx_wav.exe generated\test.wav
```

Replay a WAV through the streaming receiver:

```powershell
.\build-qt15\core\Release\hftext_stream_wav.exe generated\test.wav
```

Use `--mode 2fsk`, `--mode 4fsk`, or `--mode 8fsk` to select the physical modulation. The operational default is `2fsk`.

## Robust Mode

The CLI tools always use the current robust layer:

```text
logical frame v0.1
-> convolutional code rate 1/2, K=3, generators 111 and 101
-> deterministic rectangular interleaving
-> PREAMBLE | START_SYNC | PHYS_LENGTH | ROBUST_FRAME
```

There is no supported transmit/receive mode without FEC and interleaving.

## Modulation Settings

The modem configuration uses:

- sample rate;
- symbol duration;
- base frequency;
- tone spacing;
- modulation mode;
- amplitude;
- preamble length.

In 2-FSK, the two tones are `base` and `base + toneSpacing`.
In 4-FSK, the tones are `base + n * toneSpacing` for `n = 0..3`.
In 8-FSK, the tones are `base + n * toneSpacing` for `n = 0..7`.

All derived tones must stay below Nyquist and, for HF SSB operation, should remain in the useful radio audio passband.
