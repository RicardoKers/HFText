import numpy as np
import pytest
import soundfile as sf

from hftext.modulator import modulate_bits_2fsk, save_wav


def tone_power(samples: np.ndarray, sample_rate: int, frequency: float) -> float:
    t = np.arange(len(samples), dtype=np.float64) / sample_rate
    sine = np.sin(2.0 * np.pi * frequency * t)
    cosine = np.cos(2.0 * np.pi * frequency * t)
    return float(np.dot(samples, sine) ** 2 + np.dot(samples, cosine) ** 2)


def test_modulate_bits_returns_float32_audio_with_correct_duration():
    audio = modulate_bits_2fsk([0, 1, 0], sample_rate=8_000, symbol_duration=0.1)

    assert audio.dtype == np.float32
    assert len(audio) == 2_400


def test_modulate_bits_audio_is_normalized():
    audio = modulate_bits_2fsk([0, 1], sample_rate=8_000, symbol_duration=0.1)

    assert np.max(audio) <= 1.0
    assert np.min(audio) >= -1.0


def test_modulate_bits_maps_zero_to_f0_and_one_to_f1():
    sample_rate = 8_000
    symbol_duration = 0.1
    samples_per_symbol = int(sample_rate * symbol_duration)
    audio = modulate_bits_2fsk(
        [0, 1],
        sample_rate=sample_rate,
        symbol_duration=symbol_duration,
        f0=1_000.0,
        f1=2_000.0,
    )

    first = audio[:samples_per_symbol]
    second = audio[samples_per_symbol:]

    assert tone_power(first, sample_rate, 1_000.0) > tone_power(first, sample_rate, 2_000.0)
    assert tone_power(second, sample_rate, 2_000.0) > tone_power(second, sample_rate, 1_000.0)


def test_modulate_bits_empty_input_returns_empty_audio():
    audio = modulate_bits_2fsk([], sample_rate=8_000, symbol_duration=0.1)

    assert audio.dtype == np.float32
    assert len(audio) == 0


def test_modulate_bits_rejects_invalid_bit():
    with pytest.raises(ValueError, match="invalid bit"):
        modulate_bits_2fsk([0, 2, 1])


def test_modulate_bits_rejects_invalid_parameters():
    with pytest.raises(ValueError, match="sample_rate"):
        modulate_bits_2fsk([0], sample_rate=0)

    with pytest.raises(ValueError, match="symbol_duration"):
        modulate_bits_2fsk([0], symbol_duration=0)

    with pytest.raises(ValueError, match="frequencies"):
        modulate_bits_2fsk([0], f0=0)

    with pytest.raises(ValueError, match="amplitude"):
        modulate_bits_2fsk([0], amplitude=1.1)


def test_save_wav_writes_readable_mono_file(tmp_path):
    path = tmp_path / "generated" / "tone.wav"
    audio = modulate_bits_2fsk([0, 1], sample_rate=8_000, symbol_duration=0.01)

    save_wav(path, audio, sample_rate=8_000)
    loaded, sample_rate = sf.read(path, dtype="float32")

    assert sample_rate == 8_000
    assert loaded.dtype == np.float32
    assert loaded.shape == audio.shape
