import pytest

from hftext.channel import bit_error_rate
from hftext.frame import build_frame, parse_frame
from hftext.repetition import majority_vote_bits, repeat_bits


def test_repeat_bits_repeats_each_bit():
    assert repeat_bits([0, 1, 1], 3) == [0, 0, 0, 1, 1, 1, 1, 1, 1]


def test_majority_vote_recovers_repeated_bits_with_single_errors():
    repeated = repeat_bits([0, 1, 0, 1], 3)
    repeated[1] = 1
    repeated[5] = 0

    assert majority_vote_bits(repeated, 3) == [0, 1, 0, 1]


def test_majority_vote_handles_even_factor_ties_as_one():
    assert majority_vote_bits([0, 1, 1, 0], 2) == [1, 1]


def test_repetition_can_recover_corrupted_frame_bits_before_crc_check():
    frame_bits = build_frame("pu5lrk cq")
    repeated = repeat_bits(frame_bits, 3)
    for index in range(1, len(repeated), 17):
        repeated[index] ^= 1

    recovered = majority_vote_bits(repeated, 3)
    result = parse_frame(recovered)

    assert bit_error_rate(frame_bits, recovered) == 0.0
    assert result.crc_ok
    assert result.payload_valid
    assert result.text == "pu5lrk cq"


def test_repetition_helpers_reject_invalid_inputs():
    with pytest.raises(ValueError, match="factor"):
        repeat_bits([0], 0)
    with pytest.raises(ValueError, match="invalid bit"):
        repeat_bits([2], 3)
    with pytest.raises(ValueError, match="multiple"):
        majority_vote_bits([0, 1, 0], 2)
    with pytest.raises(ValueError, match="invalid bit"):
        majority_vote_bits([0, 2], 2)
