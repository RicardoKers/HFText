import numpy as np

from hftext.channel import (
    add_awgn,
    add_dc_offset,
    attenuate,
    bit_error_count,
    bit_error_rate,
    clip,
    signal_power,
)
from hftext.demodulator import demodulate_bits_2fsk
from hftext.frame import build_frame, parse_frame
from hftext.modulator import modulate_bits_2fsk


def test_signal_power_returns_mean_square():
    samples = np.array([1.0, -1.0, 0.0, 0.0], dtype=np.float32)

    assert signal_power(samples) == 0.5


def test_add_awgn_is_deterministic_with_seeded_rng():
    samples = np.ones(8, dtype=np.float32)

    noisy_a = add_awgn(samples, snr_db=10.0, rng=np.random.default_rng(123))
    noisy_b = add_awgn(samples, snr_db=10.0, rng=np.random.default_rng(123))

    np.testing.assert_allclose(noisy_a, noisy_b)
    assert not np.allclose(noisy_a, samples)


def test_add_awgn_leaves_silence_unchanged():
    samples = np.zeros(8, dtype=np.float32)

    noisy = add_awgn(samples, snr_db=0.0, rng=np.random.default_rng(123))

    np.testing.assert_array_equal(noisy, samples)


def test_bit_error_metrics_count_mismatches_and_length_differences():
    assert bit_error_count([0, 1, 1], [0, 0]) == 2
    assert bit_error_rate([0, 1, 1], [0, 0]) == 2 / 3


def test_attenuate_scales_audio():
    samples = np.array([1.0, -0.5], dtype=np.float32)

    np.testing.assert_allclose(attenuate(samples, 0.25), np.array([0.25, -0.125], dtype=np.float32))


def test_attenuate_rejects_negative_gain():
    samples = np.array([1.0], dtype=np.float32)

    try:
        attenuate(samples, -1.0)
    except ValueError as exc:
        assert "gain" in str(exc)
    else:
        raise AssertionError("expected ValueError")


def test_add_dc_offset_shifts_audio():
    samples = np.array([0.25, -0.25], dtype=np.float32)

    np.testing.assert_allclose(add_dc_offset(samples, 0.1), np.array([0.35, -0.15], dtype=np.float32))


def test_clip_limits_audio_symmetrically():
    samples = np.array([-2.0, -0.5, 0.25, 2.0], dtype=np.float32)

    np.testing.assert_allclose(clip(samples, 0.75), np.array([-0.75, -0.5, 0.25, 0.75], dtype=np.float32))


def test_clip_rejects_non_positive_limit():
    samples = np.array([1.0], dtype=np.float32)

    try:
        clip(samples, 0.0)
    except ValueError as exc:
        assert "limit" in str(exc)
    else:
        raise AssertionError("expected ValueError")


def test_frame_survives_moderate_awgn_channel():
    bits = build_frame("pu5lrk cq cq")
    audio = modulate_bits_2fsk(
        bits,
        sample_rate=8_000,
        symbol_duration=0.02,
        f0=1_000.0,
        f1=2_000.0,
    )
    noisy = add_awgn(audio, snr_db=6.0, rng=np.random.default_rng(42))
    decoded_bits = demodulate_bits_2fsk(
        noisy,
        sample_rate=8_000,
        symbol_duration=0.02,
        f0=1_000.0,
        f1=2_000.0,
    )
    result = parse_frame(decoded_bits)

    assert bit_error_rate(bits, decoded_bits) == 0.0
    assert result.crc_ok
    assert result.payload_valid
    assert result.text == "pu5lrk cq cq"


def test_frame_survives_attenuation_dc_offset_and_mild_clipping():
    bits = build_frame("pu5lrk cq cq")
    audio = modulate_bits_2fsk(
        bits,
        sample_rate=8_000,
        symbol_duration=0.02,
        f0=1_000.0,
        f1=2_000.0,
    )
    impaired = attenuate(audio, 0.5)
    impaired = add_dc_offset(impaired, 0.1)
    impaired = clip(impaired, 0.45)
    decoded_bits = demodulate_bits_2fsk(
        impaired,
        sample_rate=8_000,
        symbol_duration=0.02,
        f0=1_000.0,
        f1=2_000.0,
    )
    result = parse_frame(decoded_bits)

    assert bit_error_rate(bits, decoded_bits) == 0.0
    assert result.crc_ok
    assert result.payload_valid
    assert result.text == "pu5lrk cq cq"


def test_bit_error_never_presents_wrong_message_as_valid():
    bits = build_frame("pu5lrk cq cq")
    corrupted = bits.copy()
    corrupted[-1] ^= 1

    result = parse_frame(corrupted)

    assert not result.crc_ok
    assert not result.payload_valid
    assert result.text == ""
