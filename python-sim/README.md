# HFText Python Simulation

`python-sim/` contains the original simulation and validation environment for HFText. It is useful for protocol experiments, channel sweeps, WAV debugging, and comparison with the C++ core.

## Tests

```powershell
python -B -m pytest
```

Channel tests with white noise:

```powershell
python -B -m pytest tests\test_channel.py
```

## Generate a WAV

```powershell
python tx_wav.py --callsign pu5lrk "Test" generated\test.wav
```

The transmitter inserts the callsign at the beginning of the payload, followed by one space. The generated WAV includes the physical preamble before the frame.

## Decode a WAV

```powershell
python rx_wav.py generated\test.wav
```

If the CRC fails, the receiver does not print the message as valid.

The RX path searches for sync in the bit stream and, by default, tries multiple symbol offsets. To disable sync search:

```powershell
python rx_wav.py --no-sync-search generated\test.wav
```

To print receive diagnostics:

```powershell
python rx_wav.py --verbose generated\test.wav
```

## Noise Sweep

```powershell
python noise_sweep.py --callsign pu5lrk --symbol-duration 0.05 --trials 20 "HFText test"
```

The script writes example WAVs, aggregate `summary.csv`, and per-trial `trials.csv` under `generated\noise_sweep\`. It reports BER, CRC success, payload validity, and demodulator confidence for each SNR.

## Experimental MFSK Sweep

```powershell
python mfsk_sweep.py --callsign pu5lrk --symbol-duration 0.05 --trials 20 "HFText test"
```

The script compares 2-FSK, 4-FSK, and 8-FSK at the physical layer and writes results under `generated\mfsk_sweep\`. Operational validation of 4-FSK and 8-FSK still depends on the robust C++ core, CLI tools, and real captures.

## Field Evidence Summary

After saving evidence from the PC app into `logs\`, aggregate the `Summary CSV` blocks:

```powershell
python field_summary.py --input-dir ..\logs --output ..\logs\field_summary.csv
```

Without `--output`, the script writes `field_summary.csv` in the input directory. Use `--stdout` to print the aggregate CSV. The parser also accepts older Portuguese evidence markers so previous field captures remain usable.

When writing files, the script also creates:

- `field_summary_groups.csv`: grouped by modulation, symbol duration, tones, amplitude, preamble, and detailed-log state.
- `field_frames.csv`: one deduplicated row per accepted RX frame.

Use `--group-by`, `--group-output`, `--no-groups`, `--frames-output`, `--no-frames`, and `--keep-duplicate-frames` to customize the output.

## Field Replay

Replay accepted evidence WAVs through the C++ WAV decoder:

```powershell
python field_replay.py --input-dir ..\logs --output ..\logs\field_replay.csv
```

The script searches for `hftext_rx_wav`, uses the settings saved in the evidence TXT, and compares the decoded line with the received text saved by the app. Use `--rx-exe` when the executable is in another location.

## Historical Experiments

The following scripts remain useful for controlled comparisons:

- `repetition_sweep.py`: bit repetition experiments under AWGN and fading.
- `interleaving_sweep.py`: interleaving geometry comparisons.
- `fec_sweep.py`: no-FEC, Hamming(7,4), and convolutional-code comparisons.
- `fec_interleaving_sweep.py`: FEC plus interleaving sweeps.
- `channel_sweep.py`: named channel impairments such as AWGN, attenuation, DC offset, clipping, frequency offset, and block fading.

The Python FEC helpers include Hamming(7,4) for experiments and the current convolutional `conv_k3` code. The operational receiver in C++ may use soft-decision Viterbi when symbol confidence is available.
