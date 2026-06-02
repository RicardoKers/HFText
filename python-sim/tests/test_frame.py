import pytest

from hftext.codec import pack_symbols_to_bytes
from hftext.crc16 import crc16_ccitt_false
from hftext.frame import (
    SYNC_BYTES,
    bits_to_bytes,
    build_frame,
    build_frame_bytes,
    bytes_to_bits,
    parse_frame,
    parse_frame_bytes,
    payload_byte_count,
)


def test_build_and_parse_frame_round_trip_text():
    bits = build_frame("pu5lrk Teste")
    result = parse_frame(bits)

    assert result.frame_detected
    assert result.crc_ok
    assert result.payload_valid
    assert result.text == "pu5lrk Teste"
    assert result.length == 13


def test_build_frame_bytes_uses_protocol_layout():
    frame = build_frame_bytes("a")
    payload = pack_symbols_to_bytes([1])
    expected_crc = crc16_ccitt_false(payload).to_bytes(2, "big")

    assert frame == SYNC_BYTES + bytes([1]) + payload + expected_crc


def test_empty_payload_frame_is_valid():
    result = parse_frame(build_frame(""))

    assert result.frame_detected
    assert result.crc_ok
    assert result.payload_valid
    assert result.text == ""
    assert result.length == 0


def test_parse_rejects_bad_sync():
    frame = bytearray(build_frame_bytes("abc"))
    frame[0] ^= 0xFF

    result = parse_frame_bytes(bytes(frame))

    assert not result.frame_detected
    assert result.error == "sync not found"


def test_parse_rejects_length_with_bit_7_set():
    frame = bytearray(build_frame_bytes("abc"))
    frame[2] = 0x80

    result = parse_frame_bytes(bytes(frame))

    assert result.frame_detected
    assert not result.crc_ok
    assert result.error == "length bit 7 set"


def test_parse_rejects_frame_size_mismatch():
    frame = build_frame_bytes("abc")[:-1]

    result = parse_frame_bytes(frame)

    assert result.frame_detected
    assert not result.crc_ok
    assert "frame size mismatch" in result.error


def test_parse_rejects_bad_crc_without_presenting_text():
    frame = bytearray(build_frame_bytes("abc"))
    frame[-1] ^= 0x01

    result = parse_frame_bytes(bytes(frame))

    assert result.frame_detected
    assert not result.crc_ok
    assert not result.payload_valid
    assert result.text == ""
    assert result.error == "crc mismatch"


def test_parse_rejects_reserved_payload_symbol_without_presenting_text():
    payload = pack_symbols_to_bytes([61])
    crc = crc16_ccitt_false(payload).to_bytes(2, "big")
    frame = SYNC_BYTES + bytes([1]) + payload + crc

    result = parse_frame_bytes(frame)

    assert result.frame_detected
    assert result.crc_ok
    assert not result.payload_valid
    assert result.text == ""
    assert "invalid symbol" in result.error


def test_bits_and_bytes_round_trip_msb_first():
    data = bytes([0x2D, 0xD4])

    assert bytes_to_bits(data)[:8] == [0, 0, 1, 0, 1, 1, 0, 1]
    assert bits_to_bytes(bytes_to_bits(data)) == data


def test_bits_to_bytes_rejects_invalid_bit_count():
    with pytest.raises(ValueError, match="multiple of 8"):
        bits_to_bytes([1, 0, 1])


def test_bits_to_bytes_rejects_invalid_bit_value():
    with pytest.raises(ValueError, match="invalid bit"):
        bits_to_bytes([0, 0, 0, 0, 0, 0, 0, 2])


def test_payload_byte_count_matches_six_bit_packing():
    assert payload_byte_count(0) == 0
    assert payload_byte_count(1) == 1
    assert payload_byte_count(4) == 3
    assert payload_byte_count(127) == 96
