import numpy as np
import pytest

from hftext.frame import build_transmission
from hftext.modulator import modulate_bits_2fsk
from hftext.receiver import default_offset_step, receive_samples_2fsk


def test_receive_samples_recovers_frame_with_symbol_offset():
    bits = build_transmission("pu5lrk Teste")
    audio = modulate_bits_2fsk(
        bits,
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
    )
    shifted = np.concatenate([np.zeros(37, dtype=np.float32), audio])

    result = receive_samples_2fsk(
        shifted,
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
        offset_step=1,
    )

    assert result.frame_result.crc_ok
    assert result.frame_result.payload_valid
    assert result.frame_result.text == "pu5lrk Teste"
    assert result.start_offset >= 0
    assert result.offsets_tried >= 1


def test_receive_samples_without_sync_search_uses_zero_offset_only():
    bits = build_transmission("pu5lrk Teste")
    audio = modulate_bits_2fsk(bits, sample_rate=8_000, symbol_duration=0.01)
    shifted = np.concatenate([np.zeros(37, dtype=np.float32), audio])

    result = receive_samples_2fsk(
        shifted,
        sample_rate=8_000,
        symbol_duration=0.01,
        sync_search=False,
    )

    assert result.offsets_tried == 1
    assert result.start_offset == 0


def test_default_offset_step_scales_with_symbol_size():
    assert default_offset_step(8_000, 0.01) == 4
    assert default_offset_step(8_000, 0.0001) == 1


def test_receive_samples_rejects_invalid_offset_step():
    with pytest.raises(ValueError, match="offset_step"):
        receive_samples_2fsk(np.zeros(10, dtype=np.float32), offset_step=0)
