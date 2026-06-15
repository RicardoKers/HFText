"""FSK audio modulation helpers for HFText."""

from __future__ import annotations

from pathlib import Path

import numpy as np

DEFAULT_SAMPLE_RATE = 48_000
DEFAULT_SYMBOL_DURATION = 0.5
DEFAULT_F0 = 1_200.0
DEFAULT_F1 = 1_600.0
DEFAULT_AMPLITUDE = 0.8


def _validate_common(sample_rate: int, symbol_duration: float, amplitude: float) -> int:
    if sample_rate <= 0:
        raise ValueError("sample_rate must be positive")
    if symbol_duration <= 0:
        raise ValueError("symbol_duration must be positive")
    if not 0.0 <= amplitude <= 1.0:
        raise ValueError("amplitude must be between 0.0 and 1.0")

    samples_per_symbol = int(round(sample_rate * symbol_duration))
    if samples_per_symbol <= 0:
        raise ValueError("symbol duration is too short for sample_rate")
    return samples_per_symbol


def fsk_tones(f0: float, f1: float, tone_count: int) -> list[float]:
    """Return FSK tones; for 4-FSK f0/f1 are the first two adjacent tones."""
    if tone_count not in (2, 4):
        raise ValueError("tone_count must be 2 or 4")
    if f0 <= 0 or f1 <= 0:
        raise ValueError("frequencies must be positive")
    if f0 == f1:
        raise ValueError("frequencies must be different")
    if tone_count == 2:
        return [f0, f1]
    spacing = f1 - f0
    if spacing <= 0:
        raise ValueError("4-FSK requires f1 greater than f0")
    return [f0 + spacing * index for index in range(tone_count)]


def modulate_bits_fsk(
    bits: list[int],
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
    amplitude: float = DEFAULT_AMPLITUDE,
    bits_per_symbol: int = 1,
) -> np.ndarray:
    """Convert bits to normalized mono FSK audio."""
    samples_per_symbol = _validate_common(sample_rate, symbol_duration, amplitude)
    if bits_per_symbol not in (1, 2):
        raise ValueError("bits_per_symbol must be 1 or 2")
    tones = fsk_tones(f0, f1, 1 << bits_per_symbol)

    symbol_count = (len(bits) + bits_per_symbol - 1) // bits_per_symbol
    audio = np.empty(symbol_count * samples_per_symbol, dtype=np.float32)

    phase = 0.0
    offset = 0
    for symbol_index in range(symbol_count):
        tone_index = 0
        for bit_index in range(bits_per_symbol):
            source_index = symbol_index * bits_per_symbol + bit_index
            bit = bits[source_index] if source_index < len(bits) else 0
            if bit not in (0, 1):
                raise ValueError(f"invalid bit: {bit}")
            tone_index = (tone_index << 1) | bit

        frequency = tones[tone_index]
        phase_step = 2.0 * np.pi * frequency / sample_rate
        sample_indices = np.arange(samples_per_symbol, dtype=np.float64)
        phases = phase + phase_step * sample_indices
        audio[offset : offset + samples_per_symbol] = amplitude * np.sin(phases)

        phase = (phase + phase_step * samples_per_symbol) % (2.0 * np.pi)
        offset += samples_per_symbol

    return audio


def modulate_bits_2fsk(
    bits: list[int],
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
    amplitude: float = DEFAULT_AMPLITUDE,
) -> np.ndarray:
    """Convert bits to normalized mono float32 2-FSK audio."""
    return modulate_bits_fsk(bits, sample_rate, symbol_duration, f0, f1, amplitude, bits_per_symbol=1)


def modulate_bits_4fsk(
    bits: list[int],
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = 1_000.0,
    f1: float = 1_200.0,
    amplitude: float = DEFAULT_AMPLITUDE,
) -> np.ndarray:
    """Convert bits to normalized mono float32 4-FSK audio."""
    return modulate_bits_fsk(bits, sample_rate, symbol_duration, f0, f1, amplitude, bits_per_symbol=2)


def save_wav(path: str | Path, samples: np.ndarray, sample_rate: int) -> None:
    """Save normalized mono float samples as a WAV file."""
    if sample_rate <= 0:
        raise ValueError("sample_rate must be positive")

    import soundfile as sf

    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sf.write(str(output_path), np.asarray(samples, dtype=np.float32), sample_rate)
