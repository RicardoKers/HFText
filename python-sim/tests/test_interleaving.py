import pytest

from hftext.interleaving import deinterleave_bits, interleave_bits


def test_interleave_bits_reads_columns_from_row_major_block():
    bits = [0, 1, 1, 0, 1, 0]

    assert interleave_bits(bits, rows=2, columns=3) == [0, 0, 1, 1, 1, 0]


def test_deinterleave_bits_reverses_interleaving():
    bits = [0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1]

    interleaved = interleave_bits(bits, rows=3, columns=2)

    assert deinterleave_bits(interleaved, rows=3, columns=2) == bits


def test_interleaving_spreads_adjacent_bits_after_deinterleave():
    bits = [0] * 16
    interleaved = interleave_bits(bits, rows=4, columns=4)
    interleaved[0] = 1
    interleaved[1] = 1
    restored = deinterleave_bits(interleaved, rows=4, columns=4)

    assert restored == [1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]


def test_interleaving_helpers_reject_invalid_inputs():
    with pytest.raises(ValueError, match="rows"):
        interleave_bits([0], rows=0, columns=1)
    with pytest.raises(ValueError, match="columns"):
        deinterleave_bits([0], rows=1, columns=0)
    with pytest.raises(ValueError, match="invalid bit"):
        interleave_bits([0, 2], rows=1, columns=2)
    with pytest.raises(ValueError, match="multiple"):
        deinterleave_bits([0, 1, 0], rows=2, columns=2)
