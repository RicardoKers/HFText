import numpy as np
import pytest

from hftext.demodulator import demodulate_bits_2fsk, tone_energy
from hftext.frame import build_frame, parse_frame
from hftext.modulator import modulate_bits_2fsk


def test_tone_energy_detects_matching_tone():
    sample_rate = 8_000
    t = np.arange(800, dtype=np.float64) / sample_rate
    tone = np.sin(2.0 * np.pi * 1_000.0 * t).astype(np.float32)

    assert tone_energy(tone, sample_rate, 1_000.0) > tone_energy(tone, sample_rate, 2_000.0)


def test_demodulate_bits_recovers_clean_modulated_bits():
    bits = [0, 1, 1, 0, 1, 0]
    audio = modulate_bits_2fsk(
        bits,
        sample_rate=8_000,
        symbol_duration=0.05,
        f0=1_000.0,
        f1=2_000.0,
    )

    decoded = demodulate_bits_2fsk(
        audio,
        sample_rate=8_000,
        symbol_duration=0.05,
        f0=1_000.0,
        f1=2_000.0,
    )

    assert decoded == bits


def test_demodulate_bits_ignores_trailing_partial_symbol():
    bits = [0, 1]
    audio = modulate_bits_2fsk(bits, sample_rate=8_000, symbol_duration=0.05)
    audio = np.concatenate([audio, np.zeros(10, dtype=np.float32)])

    decoded = demodulate_bits_2fsk(audio, sample_rate=8_000, symbol_duration=0.05)

    assert decoded == bits


def test_demodulate_bits_supports_start_offset():
    bits = [1, 0, 1]
    audio = modulate_bits_2fsk(bits, sample_rate=8_000, symbol_duration=0.01)
    shifted = np.concatenate([np.zeros(13, dtype=np.float32), audio])

    decoded = demodulate_bits_2fsk(
        shifted,
        sample_rate=8_000,
        symbol_duration=0.01,
        start_offset=13,
    )

    assert decoded == bits


def test_demodulate_bits_empty_audio_returns_empty_bits():
    assert demodulate_bits_2fsk(np.array([], dtype=np.float32)) == []


def test_demodulate_bits_rejects_invalid_parameters():
    audio = np.zeros(10, dtype=np.float32)

    with pytest.raises(ValueError, match="sample_rate"):
        demodulate_bits_2fsk(audio, sample_rate=0)

    with pytest.raises(ValueError, match="symbol_duration"):
        demodulate_bits_2fsk(audio, symbol_duration=0)

    with pytest.raises(ValueError, match="frequencies"):
        demodulate_bits_2fsk(audio, f1=0)

    with pytest.raises(ValueError, match="start_offset"):
        demodulate_bits_2fsk(audio, start_offset=-1)


def test_frame_bits_survive_modulate_demodulate_round_trip():
    bits = build_frame("pu5lrk Teste")
    audio = modulate_bits_2fsk(
        bits,
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
    )
    decoded_bits = demodulate_bits_2fsk(
        audio,
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
    )

    result = parse_frame(decoded_bits)

    assert decoded_bits == bits
    assert result.crc_ok
    assert result.payload_valid
    assert result.text == "pu5lrk Teste"
