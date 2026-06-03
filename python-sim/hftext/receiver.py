"""High-level HFText receive helpers."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

from hftext.demodulator import demodulate_bit_decisions_2fsk
from hftext.frame import BITS_PER_BYTE, CRC_BYTES, HEADER_BYTES, FrameResult, parse_frame_from_stream, payload_byte_count
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
    confidence: float = 0.0


def default_offset_step(sample_rate: int, symbol_duration: float) -> int:
    """Return a practical sample-offset search step."""
    samples_per_symbol = int(round(sample_rate * symbol_duration))
    if samples_per_symbol <= 0:
        raise ValueError("symbol duration is too short for sample_rate")
    return max(1, samples_per_symbol // 20)


def _mean_confidence(decisions: list, start: int = 0, count: int | None = None) -> float:
    if not decisions:
        return 0.0
    end = len(decisions) if count is None else min(len(decisions), start + count)
    if start < 0 or start >= end:
        return 0.0
    return sum(decision.confidence for decision in decisions[start:end]) / (end - start)


def _confidence_for_result(result: FrameResult, decisions: list) -> float:
    if result.frame_detected and result.sync_index is not None:
        frame_bits = (HEADER_BYTES + payload_byte_count(result.length) + CRC_BYTES) * BITS_PER_BYTE
        return _mean_confidence(decisions, result.sync_index, frame_bits)
    return _mean_confidence(decisions)


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
        decisions = demodulate_bit_decisions_2fsk(samples, sample_rate, symbol_duration, f0, f1)
        bits = [decision.bit for decision in decisions]
        result = parse_frame_from_stream(bits)
        return ReceiveResult(result, bits, 0, 1, _confidence_for_result(result, decisions))

    step = default_offset_step(sample_rate, symbol_duration) if offset_step is None else offset_step
    fallback: ReceiveResult | None = None
    best_valid: ReceiveResult | None = None
    offsets_tried = 0

    for start_offset in range(0, samples_per_symbol, step):
        decisions = demodulate_bit_decisions_2fsk(
            samples,
            sample_rate,
            symbol_duration,
            f0,
            f1,
            start_offset=start_offset,
        )
        bits = [decision.bit for decision in decisions]
        result = parse_frame_from_stream(bits)
        offsets_tried += 1

        candidate = ReceiveResult(result, bits, start_offset, offsets_tried, _confidence_for_result(result, decisions))
        if fallback is None or (result.frame_detected and not fallback.frame_result.frame_detected):
            fallback = candidate
        if result.crc_ok and result.payload_valid:
            if best_valid is None or candidate.confidence > best_valid.confidence:
                best_valid = candidate

    assert fallback is not None
    if best_valid is not None:
        return ReceiveResult(
            best_valid.frame_result,
            best_valid.bits,
            best_valid.start_offset,
            offsets_tried,
            best_valid.confidence,
        )

    return ReceiveResult(
        fallback.frame_result,
        fallback.bits,
        fallback.start_offset,
        offsets_tried,
        fallback.confidence,
    )
