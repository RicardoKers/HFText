"""2-FSK audio modulation for HFText."""

from __future__ import annotations

from pathlib import Path

import numpy as np

DEFAULT_SAMPLE_RATE = 48_000
DEFAULT_SYMBOL_DURATION = 0.5
DEFAULT_F0 = 1_200.0
DEFAULT_F1 = 1_600.0
DEFAULT_AMPLITUDE = 0.8


def modulate_bits_2fsk(
    bits: list[int],
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
    amplitude: float = DEFAULT_AMPLITUDE,
) -> np.ndarray:
    """Convert bits to normalized mono float32 2-FSK audio."""
    if sample_rate <= 0:
        raise ValueError("sample_rate must be positive")
    if symbol_duration <= 0:
        raise ValueError("symbol_duration must be positive")
    if f0 <= 0 or f1 <= 0:
        raise ValueError("frequencies must be positive")
    if not 0.0 <= amplitude <= 1.0:
        raise ValueError("amplitude must be between 0.0 and 1.0")

    samples_per_symbol = int(round(sample_rate * symbol_duration))
    if samples_per_symbol <= 0:
        raise ValueError("symbol duration is too short for sample_rate")

    total_samples = len(bits) * samples_per_symbol
    audio = np.empty(total_samples, dtype=np.float32)

    phase = 0.0
    offset = 0
    for bit in bits:
        if bit not in (0, 1):
            raise ValueError(f"invalid bit: {bit}")

        frequency = f1 if bit else f0
        phase_step = 2.0 * np.pi * frequency / sample_rate
        sample_indices = np.arange(samples_per_symbol, dtype=np.float64)
        phases = phase + phase_step * sample_indices
        audio[offset : offset + samples_per_symbol] = amplitude * np.sin(phases)

        phase = (phase + phase_step * samples_per_symbol) % (2.0 * np.pi)
        offset += samples_per_symbol

    return audio


def save_wav(path: str | Path, samples: np.ndarray, sample_rate: int) -> None:
    """Save normalized mono float samples as a WAV file."""
    if sample_rate <= 0:
        raise ValueError("sample_rate must be positive")

    import soundfile as sf

    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sf.write(str(output_path), np.asarray(samples, dtype=np.float32), sample_rate)
