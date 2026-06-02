import pytest

from hftext.codec import (
    ALPHABET,
    MAX_LENGTH,
    MAX_PAYLOAD_SYMBOLS,
    MIN_LENGTH,
    SHIFT_SYMBOL,
    decode_symbols_to_text,
    encode_text_to_symbols,
    frame_length,
    pack_symbols_to_bytes,
    sanitize_text,
    unpack_symbols_from_bytes,
)


def test_encode_known_payload_symbols():
    assert encode_text_to_symbols("abc 123") == [1, 2, 3, 0, 28, 29, 30]


def test_uppercase_letters_are_encoded_with_shift():
    assert encode_text_to_symbols("AbZ") == [SHIFT_SYMBOL, 1, 2, SHIFT_SYMBOL, 26]
    assert decode_symbols_to_text(encode_text_to_symbols("AbZ")) == "AbZ"


def test_supported_symbols_are_encoded():
    text = ".,?!/-+:;@#$%&*()_=<>\\|"
    assert decode_symbols_to_text(encode_text_to_symbols(text)) == text


def test_invalid_characters_are_replaced_with_question_mark():
    assert sanitize_text("a~b") == "a?b"
    assert decode_symbols_to_text(encode_text_to_symbols("a~b")) == "a?b"


def test_all_printable_protocol_symbols_round_trip():
    assert decode_symbols_to_text(encode_text_to_symbols(ALPHABET)) == ALPHABET


def test_payload_length_is_limited_after_shift_expansion():
    encode_text_to_symbols("a" * MAX_PAYLOAD_SYMBOLS)

    with pytest.raises(ValueError, match="at most"):
        encode_text_to_symbols("a" * (MAX_PAYLOAD_SYMBOLS + 1))

    with pytest.raises(ValueError, match="at most"):
        encode_text_to_symbols("A" * 64)


def test_frame_length_counts_encoded_payload_symbols():
    assert frame_length("") == MIN_LENGTH
    assert frame_length("a" * MAX_PAYLOAD_SYMBOLS) == MAX_LENGTH
    assert frame_length("Aa") == 3


def test_shift_before_non_letter_is_ignored():
    assert decode_symbols_to_text([SHIFT_SYMBOL, 27]) == "0"


def test_trailing_shift_is_ignored():
    assert decode_symbols_to_text([1, SHIFT_SYMBOL]) == "a"


def test_pack_symbols_to_bytes_uses_msb_first_order():
    assert pack_symbols_to_bytes([1, 2, 3, 0]) == bytes([0x04, 0x20, 0xC0])


def test_pack_and_unpack_symbols_round_trip():
    symbols = encode_text_to_symbols("pu5lrk Teste")
    packed = pack_symbols_to_bytes(symbols)
    assert unpack_symbols_from_bytes(packed, len(symbols)) == symbols


def test_unpack_uses_symbol_count_to_ignore_padding_bits():
    assert unpack_symbols_from_bytes(bytes([0x04]), 1) == [1]


def test_pack_rejects_values_outside_six_bit_range():
    with pytest.raises(ValueError, match="6-bit"):
        pack_symbols_to_bytes([64])


def test_unpack_rejects_insufficient_data():
    with pytest.raises(ValueError, match="not enough"):
        unpack_symbols_from_bytes(bytes([0x00]), 2)


def test_decode_rejects_reserved_symbols():
    with pytest.raises(ValueError, match="invalid symbol"):
        decode_symbols_to_text([61])
