"""HFText Basic v0.1 frame assembly and parsing."""

from __future__ import annotations

from dataclasses import dataclass

from hftext.codec import (
    BITS_PER_BYTE,
    BITS_PER_SYMBOL,
    MAX_PAYLOAD_SYMBOLS,
    decode_symbols_to_text,
    encode_text_to_symbols,
    pack_symbols_to_bytes,
    unpack_symbols_from_bytes,
)
from hftext.crc16 import crc16_ccitt_false

SYNC_VALUE = 0x2DD4
SYNC_BYTES = SYNC_VALUE.to_bytes(2, "big")
PREAMBLE_BITS = [1, 0] * 32
CRC_BYTES = 2
HEADER_BYTES = 3


@dataclass(frozen=True)
class FrameResult:
    frame_detected: bool
    crc_ok: bool
    payload_valid: bool
    text: str
    length: int = 0
    payload_symbols: list[int] | None = None
    error: str | None = None
    sync_index: int | None = None


def bytes_to_bits(data: bytes) -> list[int]:
    """Return bytes as MSB-first bits."""
    bits = []
    for byte in data:
        for shift in range(BITS_PER_BYTE - 1, -1, -1):
            bits.append((byte >> shift) & 1)
    return bits


SYNC_BITS = bytes_to_bits(SYNC_BYTES)


def bits_to_bytes(bits: list[int]) -> bytes:
    """Return MSB-first bits as bytes."""
    if len(bits) % BITS_PER_BYTE:
        raise ValueError("bit count must be a multiple of 8")

    output = bytearray()
    for offset in range(0, len(bits), BITS_PER_BYTE):
        byte = 0
        for bit in bits[offset : offset + BITS_PER_BYTE]:
            if bit not in (0, 1):
                raise ValueError(f"invalid bit: {bit}")
            byte = (byte << 1) | bit
        output.append(byte)
    return bytes(output)


def payload_byte_count(length: int) -> int:
    """Return the number of packed bytes needed for length symbols."""
    if length < 0 or length > MAX_PAYLOAD_SYMBOLS:
        raise ValueError(f"length must be between 0 and {MAX_PAYLOAD_SYMBOLS}")
    return (length * BITS_PER_SYMBOL + BITS_PER_BYTE - 1) // BITS_PER_BYTE


def build_frame_bytes(payload_text: str) -> bytes:
    """Build an HFText frame as bytes."""
    payload_symbols = encode_text_to_symbols(payload_text)
    length = len(payload_symbols)
    payload = pack_symbols_to_bytes(payload_symbols)
    crc = crc16_ccitt_false(payload).to_bytes(CRC_BYTES, "big")

    return SYNC_BYTES + bytes([length]) + payload + crc


def build_frame(payload_text: str) -> list[int]:
    """Build an HFText frame as MSB-first bits."""
    return bytes_to_bits(build_frame_bytes(payload_text))


def build_transmission(payload_text: str, preamble_bits: list[int] | None = None) -> list[int]:
    """Build TX bits with a preamble followed by the HFText frame."""
    preamble = PREAMBLE_BITS if preamble_bits is None else preamble_bits
    for bit in preamble:
        if bit not in (0, 1):
            raise ValueError(f"invalid preamble bit: {bit}")
    return list(preamble) + build_frame(payload_text)


def parse_frame_bytes(frame: bytes) -> FrameResult:
    """Parse an exact HFText frame from bytes."""
    if len(frame) < HEADER_BYTES + CRC_BYTES:
        return FrameResult(False, False, False, "", error="frame too short")

    if frame[:2] != SYNC_BYTES:
        return FrameResult(False, False, False, "", error="sync not found")

    length = frame[2]
    if length & 0x80:
        return FrameResult(True, False, False, "", length=length, error="length bit 7 set")
    if length > MAX_PAYLOAD_SYMBOLS:
        return FrameResult(True, False, False, "", length=length, error="length too large")

    packed_size = payload_byte_count(length)
    expected_size = HEADER_BYTES + packed_size + CRC_BYTES
    if len(frame) != expected_size:
        return FrameResult(
            True,
            False,
            False,
            "",
            length=length,
            error=f"frame size mismatch: expected {expected_size}, got {len(frame)}",
        )

    payload = frame[HEADER_BYTES : HEADER_BYTES + packed_size]
    received_crc = int.from_bytes(frame[HEADER_BYTES + packed_size :], "big")
    calculated_crc = crc16_ccitt_false(payload)
    crc_ok = received_crc == calculated_crc
    if not crc_ok:
        return FrameResult(True, False, False, "", length=length, error="crc mismatch")

    payload_symbols = unpack_symbols_from_bytes(payload, length)
    try:
        text = decode_symbols_to_text(payload_symbols)
    except ValueError as exc:
        return FrameResult(
            True,
            True,
            False,
            "",
            length=length,
            payload_symbols=payload_symbols,
            error=str(exc),
        )

    return FrameResult(True, True, True, text, length=length, payload_symbols=payload_symbols)


def parse_frame(bits: list[int]) -> FrameResult:
    """Parse an exact HFText frame from MSB-first bits."""
    return parse_frame_bytes(bits_to_bytes(bits))


def find_sync(bits: list[int]) -> int | None:
    """Return the bit index of SYNC in a received bit stream."""
    if len(bits) < len(SYNC_BITS):
        return None
    for bit in bits:
        if bit not in (0, 1):
            raise ValueError(f"invalid bit: {bit}")

    last_start = len(bits) - len(SYNC_BITS)
    for index in range(last_start + 1):
        if bits[index : index + len(SYNC_BITS)] == SYNC_BITS:
            return index
    return None


def _bits_to_int(bits: list[int]) -> int:
    value = 0
    for bit in bits:
        if bit not in (0, 1):
            raise ValueError(f"invalid bit: {bit}")
        value = (value << 1) | bit
    return value


def _parse_frame_at_sync(bits: list[int], sync_index: int) -> FrameResult:
    length_start = sync_index + len(SYNC_BITS)
    length_end = length_start + BITS_PER_BYTE
    if len(bits) < length_end:
        return FrameResult(True, False, False, "", error="stream ended before length", sync_index=sync_index)

    length = _bits_to_int(bits[length_start:length_end])
    if length & 0x80:
        return FrameResult(
            True,
            False,
            False,
            "",
            length=length,
            error="length bit 7 set",
            sync_index=sync_index,
        )
    if length > MAX_PAYLOAD_SYMBOLS:
        return FrameResult(
            True,
            False,
            False,
            "",
            length=length,
            error="length too large",
            sync_index=sync_index,
        )

    frame_bit_count = (HEADER_BYTES + payload_byte_count(length) + CRC_BYTES) * BITS_PER_BYTE
    frame_end = sync_index + frame_bit_count
    if len(bits) < frame_end:
        return FrameResult(
            True,
            False,
            False,
            "",
            length=length,
            error="stream ended before complete frame",
            sync_index=sync_index,
        )

    result = parse_frame(bits[sync_index:frame_end])
    return FrameResult(
        result.frame_detected,
        result.crc_ok,
        result.payload_valid,
        result.text,
        length=result.length,
        payload_symbols=result.payload_symbols,
        error=result.error,
        sync_index=sync_index,
    )


def parse_frame_from_stream(bits: list[int]) -> FrameResult:
    """Find SYNC candidates and return the first valid HFText frame."""
    if len(bits) < len(SYNC_BITS):
        return FrameResult(False, False, False, "", error="sync not found")
    for bit in bits:
        if bit not in (0, 1):
            raise ValueError(f"invalid bit: {bit}")

    first_candidate: FrameResult | None = None
    last_start = len(bits) - len(SYNC_BITS)
    for sync_index in range(last_start + 1):
        if bits[sync_index : sync_index + len(SYNC_BITS)] != SYNC_BITS:
            continue

        result = _parse_frame_at_sync(bits, sync_index)
        if first_candidate is None:
            first_candidate = result
        if result.crc_ok and result.payload_valid:
            return result

    if first_candidate is not None:
        return first_candidate

    return FrameResult(False, False, False, "", error="sync not found")
