"""High-level HFText receive helpers."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from hftext.demodulator import demodulate_bits_2fsk
from hftext.frame import FrameResult, parse_frame_from_stream
from hftext.modulator import (
    DEFAULT_F0,
    DEFAULT_F1,
    DEFAULT_SAMPLE_RATE,
    DEFAULT_SYMBOL_DURATION,
)


@dataclass(frozen=True)
class ReceiveResult:
    frame_result: FrameResult
    bits: list[int]
    start_offset: int
    offsets_tried: int


def default_offset_step(sample_rate: int, symbol_duration: float) -> int:
    """Return a practical sample-offset search step."""
    samples_per_symbol = int(round(sample_rate * symbol_duration))
    if samples_per_symbol <= 0:
        raise ValueError("symbol duration is too short for sample_rate")
    return max(1, samples_per_symbol // 20)


def receive_samples_2fsk(
    samples: np.ndarray,
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
    sync_search: bool = True,
    offset_step: int | None = None,
) -> ReceiveResult:
    """Demodulate samples and parse the first valid frame in the bit stream."""
    if sample_rate <= 0:
        raise ValueError("sample_rate must be positive")
    if symbol_duration <= 0:
        raise ValueError("symbol_duration must be positive")
    if offset_step is not None and offset_step <= 0:
        raise ValueError("offset_step must be positive")

    samples_per_symbol = int(round(sample_rate * symbol_duration))
    if samples_per_symbol <= 0:
        raise ValueError("symbol duration is too short for sample_rate")

    if not sync_search:
        bits = demodulate_bits_2fsk(samples, sample_rate, symbol_duration, f0, f1)
        return ReceiveResult(parse_frame_from_stream(bits), bits, 0, 1)

    step = default_offset_step(sample_rate, symbol_duration) if offset_step is None else offset_step
    fallback: ReceiveResult | None = None
    offsets_tried = 0

    for start_offset in range(0, samples_per_symbol, step):
        bits = demodulate_bits_2fsk(
            samples,
            sample_rate,
            symbol_duration,
            f0,
            f1,
            start_offset=start_offset,
        )
        result = parse_frame_from_stream(bits)
        offsets_tried += 1

        candidate = ReceiveResult(result, bits, start_offset, offsets_tried)
        if fallback is None or (result.frame_detected and not fallback.frame_result.frame_detected):
            fallback = candidate
        if result.crc_ok and result.payload_valid:
            return candidate

    assert fallback is not None
    return ReceiveResult(
        fallback.frame_result,
        fallback.bits,
        fallback.start_offset,
        offsets_tried,
    )
