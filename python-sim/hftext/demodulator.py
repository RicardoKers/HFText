"""FSK audio demodulation helpers for HFText."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from hftext.modulator import (
    DEFAULT_F0,
    DEFAULT_F1,
    DEFAULT_SAMPLE_RATE,
    DEFAULT_SYMBOL_DURATION,
    fsk_tones,
)


@dataclass(frozen=True)
class BitDecision:
    bit: int
    confidence: float


def tone_energy(samples: np.ndarray, sample_rate: int, frequency: float) -> float:
    """Return non-coherent tone energy for one symbol window."""
    t = np.arange(len(samples), dtype=np.float64) / sample_rate
    phase = 2.0 * np.pi * frequency * t
    i = float(np.dot(samples, np.cos(phase)))
    q = float(np.dot(samples, np.sin(phase)))
    return i * i + q * q


def demodulate_bit_decisions_2fsk(
    samples: np.ndarray,
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
    start_offset: int = 0,
) -> list[BitDecision]:
    """Demodulate normalized mono 2-FSK audio into bit decisions."""
    return demodulate_bit_decisions_fsk(samples, sample_rate, symbol_duration, f0, f1, start_offset, bits_per_symbol=1)


def demodulate_bit_decisions_fsk(
    samples: np.ndarray,
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
    start_offset: int = 0,
    bits_per_symbol: int = 1,
) -> list[BitDecision]:
    """Demodulate normalized mono FSK audio into bit decisions."""
    if sample_rate <= 0:
        raise ValueError("sample_rate must be positive")
    if symbol_duration <= 0:
        raise ValueError("symbol_duration must be positive")
    if start_offset < 0:
        raise ValueError("start_offset must be non-negative")
    if bits_per_symbol not in (1, 2):
        raise ValueError("bits_per_symbol must be 1 or 2")
    tones = fsk_tones(f0, f1, 1 << bits_per_symbol)

    samples_per_symbol = int(round(sample_rate * symbol_duration))
    if samples_per_symbol <= 0:
        raise ValueError("symbol duration is too short for sample_rate")

    audio = np.asarray(samples, dtype=np.float32)
    if start_offset >= len(audio):
        return []

    audio = audio[start_offset:]
    symbol_count = len(audio) // samples_per_symbol
    decisions = []

    for symbol_index in range(symbol_count):
        start = symbol_index * samples_per_symbol
        end = start + samples_per_symbol
        window = audio[start:end]

        energies = [tone_energy(window, sample_rate, tone) for tone in tones]
        best_index = int(np.argmax(energies))
        ordered = sorted(energies, reverse=True)
        top_energy = ordered[0] + (ordered[1] if len(ordered) > 1 else 0.0)
        confidence = 0.0 if top_energy <= 0.0 else (ordered[0] - ordered[1]) / top_energy
        for bit_index in range(bits_per_symbol - 1, -1, -1):
            decisions.append(BitDecision((best_index >> bit_index) & 1, float(min(1.0, confidence))))

    return decisions


def demodulate_bit_decisions_4fsk(
    samples: np.ndarray,
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = 1_000.0,
    f1: float = 1_200.0,
    start_offset: int = 0,
) -> list[BitDecision]:
    """Demodulate normalized mono 4-FSK audio into bit decisions."""
    return demodulate_bit_decisions_fsk(samples, sample_rate, symbol_duration, f0, f1, start_offset, bits_per_symbol=2)


def demodulate_bits_2fsk(
    samples: np.ndarray,
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
    start_offset: int = 0,
) -> list[int]:
    """Demodulate normalized mono 2-FSK audio into bits."""
    return [
        decision.bit
        for decision in demodulate_bit_decisions_2fsk(
            samples,
            sample_rate,
            symbol_duration,
            f0,
            f1,
            start_offset,
        )
    ]


def demodulate_bits_4fsk(
    samples: np.ndarray,
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = 1_000.0,
    f1: float = 1_200.0,
    start_offset: int = 0,
) -> list[int]:
    """Demodulate normalized mono 4-FSK audio into bits."""
    return [
        decision.bit
        for decision in demodulate_bit_decisions_4fsk(
            samples,
            sample_rate,
            symbol_duration,
            f0,
            f1,
            start_offset,
        )
    ]
