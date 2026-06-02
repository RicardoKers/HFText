"""Simple simulated channel effects for HFText validation."""

from __future__ import annotations

import numpy as np


def signal_power(samples: np.ndarray) -> float:
    """Return mean squared signal power."""
    audio = np.asarray(samples, dtype=np.float64)
    if audio.size == 0:
        return 0.0
    return float(np.mean(audio * audio))


def add_awgn(
    samples: np.ndarray,
    snr_db: float,
    rng: np.random.Generator | None = None,
) -> np.ndarray:
    """Add white Gaussian noise at the requested SNR in dB."""
    audio = np.asarray(samples, dtype=np.float32)
    power = signal_power(audio)
    if power == 0.0:
        return audio.copy()

    generator = rng if rng is not None else np.random.default_rng()
    noise_power = power / (10.0 ** (snr_db / 10.0))
    noise = generator.normal(0.0, np.sqrt(noise_power), size=audio.shape)
    return (audio.astype(np.float64) + noise).astype(np.float32)


def attenuate(samples: np.ndarray, gain: float) -> np.ndarray:
    """Scale samples by gain."""
    if gain < 0:
        raise ValueError("gain must be non-negative")
    return (np.asarray(samples, dtype=np.float32) * gain).astype(np.float32)


def add_dc_offset(samples: np.ndarray, offset: float) -> np.ndarray:
    """Add a DC offset to samples."""
    return (np.asarray(samples, dtype=np.float32) + offset).astype(np.float32)


def clip(samples: np.ndarray, limit: float = 1.0) -> np.ndarray:
    """Clip samples symmetrically to +/- limit."""
    if limit <= 0:
        raise ValueError("limit must be positive")
    audio = np.asarray(samples, dtype=np.float32)
    return np.clip(audio, -limit, limit).astype(np.float32)


def bit_error_count(expected: list[int], received: list[int]) -> int:
    """Count bit errors, including missing or extra bits."""
    shared = min(len(expected), len(received))
    errors = sum(1 for index in range(shared) if expected[index] != received[index])
    return errors + abs(len(expected) - len(received))


def bit_error_rate(expected: list[int], received: list[int]) -> float:
    """Return bit error rate against the expected bit sequence."""
    if not expected and not received:
        return 0.0
    denominator = max(len(expected), len(received))
    return bit_error_count(expected, received) / denominator
