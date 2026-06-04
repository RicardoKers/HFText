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


def choose_interleave_shape(
    bit_count: int,
    preferred_rows: int = 6,
    min_rows: int = 2,
    max_rows: int = 16,
) -> tuple[int, int]:
    """Choose a deterministic complete rectangular interleaver shape."""
    if bit_count <= 0:
        raise ValueError("bit_count must be positive")
    if preferred_rows <= 0:
        raise ValueError("preferred_rows must be positive")
    if min_rows <= 0:
        raise ValueError("min_rows must be positive")
    if max_rows < min_rows:
        raise ValueError("max_rows must be greater than or equal to min_rows")

    candidates = [rows for rows in range(min_rows, max_rows + 1) if bit_count % rows == 0]
    if not candidates:
        raise ValueError("no interleaving shape exactly fits bit_count")

    rows = min(candidates, key=lambda candidate: (abs(candidate - preferred_rows), candidate))
    return rows, bit_count // rows


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
