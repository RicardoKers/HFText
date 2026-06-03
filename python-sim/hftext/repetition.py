"""Experimental bit repetition helpers for HFText validation."""

from __future__ import annotations


def repeat_bits(bits: list[int], factor: int) -> list[int]:
    """Repeat each bit factor times."""
    if factor <= 0:
        raise ValueError("factor must be positive")

    output: list[int] = []
    for bit in bits:
        if bit not in (0, 1):
            raise ValueError(f"invalid bit: {bit}")
        output.extend([bit] * factor)
    return output


def majority_vote_bits(bits: list[int], factor: int) -> list[int]:
    """Recover repeated bits by majority vote over fixed-size groups."""
    if factor <= 0:
        raise ValueError("factor must be positive")
    if len(bits) % factor:
        raise ValueError("bit count must be a multiple of factor")

    output: list[int] = []
    for start in range(0, len(bits), factor):
        group = bits[start : start + factor]
        ones = 0
        for bit in group:
            if bit not in (0, 1):
                raise ValueError(f"invalid bit: {bit}")
            ones += bit
        output.append(1 if ones * 2 >= factor else 0)
    return output
