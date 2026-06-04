import pytest

from hftext.fec import (
    convolutional_k3_decode_bits,
    convolutional_k3_encode_bits,
    hamming74_decode_bits,
    hamming74_encode_bits,
)
from hftext.frame import build_frame, parse_frame


def test_hamming74_encodes_known_nibble():
    assert hamming74_encode_bits([1, 0, 1, 1]) == [0, 1, 1, 0, 0, 1, 1]


def test_hamming74_round_trip_preserves_bits():
    bits = [1, 0, 1, 1, 0, 0, 1, 0, 1]

    encoded = hamming74_encode_bits(bits)
    decoded, corrected_count, invalid_count = hamming74_decode_bits(encoded, len(bits))

    assert decoded == bits
    assert corrected_count == 0
    assert invalid_count == 0


def test_hamming74_corrects_one_bit_per_codeword():
    bits = [1, 0, 1, 1, 0, 1, 0, 0]
    encoded = hamming74_encode_bits(bits)
    encoded[0] ^= 1
    encoded[10] ^= 1

    decoded, corrected_count, invalid_count = hamming74_decode_bits(encoded, len(bits))

    assert decoded == bits
    assert corrected_count == 2
    assert invalid_count == 0


def test_hamming74_padding_is_trimmed_by_original_bit_count():
    encoded = hamming74_encode_bits([1])
    decoded, corrected_count, invalid_count = hamming74_decode_bits(encoded, 1)

    assert decoded == [1]
    assert corrected_count == 0
    assert invalid_count == 0


def test_hamming74_can_recover_frame_with_sparse_errors():
    frame_bits = build_frame("pu5lrk cq")
    encoded = hamming74_encode_bits(frame_bits)
    for index in range(2, len(encoded), 21):
        encoded[index] ^= 1

    decoded, corrected_count, invalid_count = hamming74_decode_bits(encoded, len(frame_bits))
    result = parse_frame(decoded)

    assert corrected_count > 0
    assert invalid_count == 0
    assert result.crc_ok
    assert result.payload_valid
    assert result.text == "pu5lrk cq"


def test_hamming74_helpers_reject_invalid_inputs():
    with pytest.raises(ValueError, match="invalid bit"):
        hamming74_encode_bits([2])

    with pytest.raises(ValueError, match="invalid bit"):
        hamming74_decode_bits([0, 1, 2, 0, 1, 0, 1])

    with pytest.raises(ValueError, match="multiple of 7"):
        hamming74_decode_bits([0, 1])

    with pytest.raises(ValueError, match="original_bit_count"):
        hamming74_decode_bits([0, 0, 0, 0, 0, 0, 0], -1)


def test_convolutional_k3_encodes_known_bits_with_tail():
    assert convolutional_k3_encode_bits([1, 0, 1]) == [1, 1, 1, 0, 0, 0, 1, 0, 1, 1]


def test_convolutional_k3_round_trip_preserves_bits():
    bits = [1, 0, 1, 1, 0, 0, 1]

    encoded = convolutional_k3_encode_bits(bits)
    decoded, distance = convolutional_k3_decode_bits(encoded, len(bits))

    assert decoded == bits
    assert distance == 0


def test_convolutional_k3_recovers_sparse_encoded_bit_errors():
    bits = [1, 0, 1, 1, 0, 0, 1, 0, 1]
    encoded = convolutional_k3_encode_bits(bits)
    encoded[1] ^= 1
    encoded[8] ^= 1

    decoded, distance = convolutional_k3_decode_bits(encoded, len(bits))

    assert decoded == bits
    assert distance == 2


def test_convolutional_k3_can_recover_frame_with_sparse_errors():
    frame_bits = build_frame("pu5lrk cq")
    encoded = convolutional_k3_encode_bits(frame_bits)
    for index in range(3, len(encoded), 31):
        encoded[index] ^= 1

    decoded, distance = convolutional_k3_decode_bits(encoded, len(frame_bits))
    result = parse_frame(decoded)

    assert distance > 0
    assert result.crc_ok
    assert result.payload_valid
    assert result.text == "pu5lrk cq"


def test_convolutional_k3_helpers_reject_invalid_inputs():
    with pytest.raises(ValueError, match="invalid bit"):
        convolutional_k3_encode_bits([2])

    with pytest.raises(ValueError, match="invalid bit"):
        convolutional_k3_decode_bits([0, 2])

    with pytest.raises(ValueError, match="multiple of 2"):
        convolutional_k3_decode_bits([0])

    with pytest.raises(ValueError, match="original_bit_count"):
        convolutional_k3_decode_bits([0, 0], -1)
