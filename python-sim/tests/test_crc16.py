from hftext.codec import encode_text_to_symbols, pack_symbols_to_bytes
from hftext.crc16 import crc16_ccitt_false


def test_crc16_ccitt_false_known_vector():
    assert crc16_ccitt_false(b"123456789") == 0x29B1


def test_crc16_empty_payload_uses_initial_value():
    assert crc16_ccitt_false(b"") == 0xFFFF


def test_crc16_changes_when_data_changes():
    original = crc16_ccitt_false(b"hftext")
    changed = crc16_ccitt_false(b"hftexu")

    assert original != changed


def test_crc16_can_run_over_packed_payload_symbols():
    symbols = encode_text_to_symbols("pu5lrk Teste")
    payload_bytes = pack_symbols_to_bytes(symbols)

    assert crc16_ccitt_false(payload_bytes) == crc16_ccitt_false(payload_bytes)
    assert crc16_ccitt_false(payload_bytes) != crc16_ccitt_false(payload_bytes + b"\x00")
