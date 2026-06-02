"""CRC-16/CCITT-FALSE for HFText frames."""

from __future__ import annotations

POLY = 0x1021
INITIAL = 0xFFFF
XOR_OUT = 0x0000
MASK = 0xFFFF
TOP_BIT = 0x8000


def crc16_ccitt_false(data: bytes) -> int:
    """Return CRC-16/CCITT-FALSE for data."""
    crc = INITIAL

    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & TOP_BIT:
                crc = ((crc << 1) ^ POLY) & MASK
            else:
                crc = (crc << 1) & MASK

    return crc ^ XOR_OUT

