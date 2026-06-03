"""Experimental block interleaving helpers for HFText validation."""

from __future__ import annotations


def _validate_bits(bits: list[int]) -> None:
    for bit in bits:
        if bit not in (0, 1):
            raise ValueError(f"invalid bit: {bit}")


def _validate_dimensions(rows: int, columns: int) -> None:
    if rows <= 0:
        raise ValueError("rows must be positive")
    if columns <= 0:
        raise ValueError("columns must be positive")


def interleave_bits(bits: list[int], rows: int, columns: int) -> list[int]:
    """Interleave complete row-major blocks by reading them column-major."""
    _validate_dimensions(rows, columns)
    _validate_bits(bits)

    block_size = rows * columns
    if len(bits) % block_size:
        raise ValueError("bit count must be a multiple of rows * columns")

    output: list[int] = []
    for block_start in range(0, len(bits), block_size):
        block = bits[block_start : block_start + block_size]
        for column in range(columns):
            for row in range(rows):
                output.append(block[row * columns + column])
    return output


def deinterleave_bits(bits: list[int], rows: int, columns: int) -> list[int]:
    """Reverse interleave_bits for complete row-major blocks."""
    _validate_dimensions(rows, columns)
    _validate_bits(bits)

    block_size = rows * columns
    if len(bits) % block_size:
        raise ValueError("bit count must be a multiple of rows * columns")

    output: list[int] = []
    for block_start in range(0, len(bits), block_size):
        block = bits[block_start : block_start + block_size]
        restored = [0] * block_size
        index = 0
        for column in range(columns):
            for row in range(rows):
                restored[row * columns + column] = block[index]
                index += 1
        output.extend(restored)
    return output
