"""2-FSK audio demodulation for HFText."""

from __future__ import annotations

import numpy as np

from hftext.modulator import (
    DEFAULT_F0,
    DEFAULT_F1,
    DEFAULT_SAMPLE_RATE,
    DEFAULT_SYMBOL_DURATION,
)


def tone_energy(samples: np.ndarray, sample_rate: int, frequency: float) -> float:
    """Return non-coherent tone energy for one symbol window."""
    t = np.arange(len(samples), dtype=np.float64) / sample_rate
    phase = 2.0 * np.pi * frequency * t
    i = float(np.dot(samples, np.cos(phase)))
    q = float(np.dot(samples, np.sin(phase)))
    return i * i + q * q


def demodulate_bits_2fsk(
    samples: np.ndarray,
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
    start_offset: int = 0,
) -> list[int]:
    """Demodulate normalized mono 2-FSK audio into bits."""
    if sample_rate <= 0:
        raise ValueError("sample_rate must be positive")
    if symbol_duration <= 0:
        raise ValueError("symbol_duration must be positive")
    if f0 <= 0 or f1 <= 0:
        raise ValueError("frequencies must be positive")
    if start_offset < 0:
        raise ValueError("start_offset must be non-negative")

    samples_per_symbol = int(round(sample_rate * symbol_duration))
    if samples_per_symbol <= 0:
        raise ValueError("symbol duration is too short for sample_rate")

    audio = np.asarray(samples, dtype=np.float32)
    if start_offset >= len(audio):
        return []

    audio = audio[start_offset:]
    symbol_count = len(audio) // samples_per_symbol
    bits = []

    for symbol_index in range(symbol_count):
        start = symbol_index * samples_per_symbol
        end = start + samples_per_symbol
        window = audio[start:end]

        energy0 = tone_energy(window, sample_rate, f0)
        energy1 = tone_energy(window, sample_rate, f1)
        bits.append(1 if energy1 > energy0 else 0)

    return bits
