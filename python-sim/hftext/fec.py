"""Experimental FEC helpers for HFText validation."""

from __future__ import annotations


def _validate_bits(bits: list[int]) -> None:
    for bit in bits:
        if bit not in (0, 1):
            raise ValueError(f"invalid bit: {bit}")


def _hamming74_encode_nibble(data: list[int]) -> list[int]:
    d1, d2, d3, d4 = data
    p1 = d1 ^ d2 ^ d4
    p2 = d1 ^ d3 ^ d4
    p4 = d2 ^ d3 ^ d4
    return [p1, p2, d1, p4, d2, d3, d4]


def _hamming74_decode_codeword(codeword: list[int]) -> tuple[list[int], bool, bool]:
    p1, p2, d1, p4, d2, d3, d4 = codeword
    s1 = p1 ^ d1 ^ d2 ^ d4
    s2 = p2 ^ d1 ^ d3 ^ d4
    s4 = p4 ^ d2 ^ d3 ^ d4
    syndrome = s1 | (s2 << 1) | (s4 << 2)
    corrected = False
    invalid = False
    fixed = codeword.copy()

    if syndrome:
        if 1 <= syndrome <= 7:
            fixed[syndrome - 1] ^= 1
            corrected = True
        else:
            invalid = True

    return [fixed[2], fixed[4], fixed[5], fixed[6]], corrected, invalid


def hamming74_encode_bits(bits: list[int]) -> list[int]:
    """Encode bits using Hamming(7,4), padding the final nibble with zeros."""
    _validate_bits(bits)

    output: list[int] = []
    for start in range(0, len(bits), 4):
        nibble = bits[start : start + 4]
        padded = nibble + [0] * (4 - len(nibble))
        output.extend(_hamming74_encode_nibble(padded))
    return output


def hamming74_decode_bits(bits: list[int], original_bit_count: int | None = None) -> tuple[list[int], int, int]:
    """Decode Hamming(7,4) bits.

    Returns decoded bits, corrected codeword count and invalid codeword count.
    """
    _validate_bits(bits)
    if len(bits) % 7:
        raise ValueError("bit count must be a multiple of 7")
    if original_bit_count is not None and original_bit_count < 0:
        raise ValueError("original_bit_count must be non-negative")

    output: list[int] = []
    corrected_count = 0
    invalid_count = 0
    for start in range(0, len(bits), 7):
        decoded, corrected, invalid = _hamming74_decode_codeword(bits[start : start + 7])
        output.extend(decoded)
        corrected_count += 1 if corrected else 0
        invalid_count += 1 if invalid else 0

    if original_bit_count is not None:
        output = output[:original_bit_count]
    return output, corrected_count, invalid_count


def _conv_k3_output_bit(register: int, generator: int) -> int:
    masked = register & generator
    parity = 0
    while masked:
        parity ^= masked & 1
        masked >>= 1
    return parity


def _conv_k3_encode_step(state: int, bit: int) -> tuple[int, tuple[int, int]]:
    register = (bit << 2) | state
    output = (
        _conv_k3_output_bit(register, 0b111),
        _conv_k3_output_bit(register, 0b101),
    )
    next_state = register >> 1
    return next_state, output


def convolutional_k3_encode_bits(bits: list[int], tail: bool = True) -> list[int]:
    """Encode bits with a rate 1/2, K=3 convolutional code.

    Generators are 0b111 and 0b101. With tail=True, two zero bits are appended
    to return the encoder to the zero state.
    """
    _validate_bits(bits)

    state = 0
    output: list[int] = []
    input_bits = bits + ([0, 0] if tail else [])
    for bit in input_bits:
        state, pair = _conv_k3_encode_step(state, bit)
        output.extend(pair)
    return output


def convolutional_k3_decode_bits(
    bits: list[int],
    original_bit_count: int | None = None,
    tail: bool = True,
) -> tuple[list[int], int]:
    """Decode the K=3 convolutional code with hard-decision Viterbi.

    Returns decoded bits and the final path distance.
    """
    _validate_bits(bits)
    if len(bits) % 2:
        raise ValueError("bit count must be a multiple of 2")
    if original_bit_count is not None and original_bit_count < 0:
        raise ValueError("original_bit_count must be non-negative")

    inf = 1_000_000_000
    paths: dict[int, tuple[int, list[int]]] = {0: (0, [])}
    for start in range(0, len(bits), 2):
        received = (bits[start], bits[start + 1])
        next_paths: dict[int, tuple[int, list[int]]] = {}
        for state, (distance, path) in paths.items():
            for bit in (0, 1):
                next_state, expected = _conv_k3_encode_step(state, bit)
                branch_distance = (received[0] ^ expected[0]) + (received[1] ^ expected[1])
                candidate = (distance + branch_distance, path + [bit])
                current = next_paths.get(next_state, (inf, []))
                if candidate[0] < current[0]:
                    next_paths[next_state] = candidate
        paths = next_paths

    if tail and 0 in paths:
        distance, decoded = paths[0]
    else:
        _, (distance, decoded) = min(paths.items(), key=lambda item: item[1][0])

    if tail and len(decoded) >= 2:
        decoded = decoded[:-2]
    if original_bit_count is not None:
        decoded = decoded[:original_bit_count]
    return decoded, distance
