import numpy as np

from hftext.channel import (
    add_awgn,
    add_dc_offset,
    apply_block_fading,
    apply_frequency_offset,
    attenuate,
    bit_error_count,
    bit_error_rate,
    clip,
    signal_power,
)
from hftext.demodulator import demodulate_bits_2fsk, tone_energy
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


def test_apply_frequency_offset_shifts_tone_energy():
    sample_rate = 8_000
    t = np.arange(8_000, dtype=np.float64) / sample_rate
    tone = np.sin(2.0 * np.pi * 1_000.0 * t).astype(np.float32)

    shifted = apply_frequency_offset(tone, sample_rate, 100.0)

    assert tone_energy(shifted, sample_rate, 1_100.0) > tone_energy(shifted, sample_rate, 1_000.0)


def test_apply_frequency_offset_rejects_invalid_sample_rate():
    try:
        apply_frequency_offset(np.zeros(4, dtype=np.float32), 0, 10.0)
    except ValueError as exc:
        assert "sample_rate" in str(exc)
    else:
        raise AssertionError("expected ValueError")


def test_apply_block_fading_is_deterministic_with_seeded_rng():
    samples = np.ones(12, dtype=np.float32)

    faded_a = apply_block_fading(samples, 4, 0.2, 0.8, rng=np.random.default_rng(123))
    faded_b = apply_block_fading(samples, 4, 0.2, 0.8, rng=np.random.default_rng(123))

    np.testing.assert_allclose(faded_a, faded_b)
    assert not np.allclose(faded_a, samples)
    assert np.all(faded_a >= 0.2)
    assert np.all(faded_a <= 0.8)


def test_apply_block_fading_rejects_invalid_parameters():
    samples = np.ones(4, dtype=np.float32)

    for kwargs, expected in [
        ({"block_size": 0, "min_gain": 0.1, "max_gain": 1.0}, "block_size"),
        ({"block_size": 2, "min_gain": -0.1, "max_gain": 1.0}, "min_gain"),
        ({"block_size": 2, "min_gain": 1.0, "max_gain": 0.5}, "max_gain"),
    ]:
        try:
            apply_block_fading(samples, **kwargs)
        except ValueError as exc:
            assert expected in str(exc)
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


def test_frame_survives_small_frequency_offset():
    bits = build_frame("pu5lrk cq cq")
    audio = modulate_bits_2fsk(
        bits,
        sample_rate=8_000,
        symbol_duration=0.02,
        f0=1_000.0,
        f1=2_000.0,
    )
    shifted = apply_frequency_offset(audio, sample_rate=8_000, offset_hz=20.0)
    decoded_bits = demodulate_bits_2fsk(
        shifted,
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


def test_frame_survives_light_block_fading():
    bits = build_frame("pu5lrk cq cq")
    audio = modulate_bits_2fsk(
        bits,
        sample_rate=8_000,
        symbol_duration=0.02,
        f0=1_000.0,
        f1=2_000.0,
    )
    faded = apply_block_fading(audio, 160, 0.25, 1.0, rng=np.random.default_rng(123))
    decoded_bits = demodulate_bits_2fsk(
        faded,
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
