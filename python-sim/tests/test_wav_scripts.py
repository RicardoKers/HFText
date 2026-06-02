import numpy as np
import soundfile as sf

from rx_wav import decode_wav, load_wav_mono, main as rx_main
from tx_wav import build_payload, generate_wav, main as tx_main


def test_build_payload_prefixes_callsign_with_single_space():
    assert build_payload("Teste", callsign="pu5lrk") == "pu5lrk Teste"
    assert build_payload("Teste", callsign="") == "Teste"
    assert build_payload("Teste") == "Teste"


def test_generate_and_decode_wav_round_trip(tmp_path):
    path = tmp_path / "tx.wav"

    payload = generate_wav(
        "Teste",
        path,
        callsign="pu5lrk",
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
    )
    result = decode_wav(path, symbol_duration=0.01, f0=1_000.0, f1=2_000.0)

    assert payload == "pu5lrk Teste"
    assert result.frame_detected
    assert result.crc_ok
    assert result.payload_valid
    assert result.text == payload


def test_generated_wav_decodes_with_leading_silence(tmp_path):
    path = tmp_path / "tx.wav"
    payload = generate_wav(
        "Teste",
        path,
        callsign="pu5lrk",
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
    )
    samples, sample_rate = sf.read(path, dtype="float32")
    silence = np.zeros(int(sample_rate * 0.083), dtype=np.float32)
    sf.write(path, np.concatenate([silence, samples]), sample_rate)

    result = decode_wav(path, symbol_duration=0.01, f0=1_000.0, f1=2_000.0)

    assert result.crc_ok
    assert result.payload_valid
    assert result.text == payload


def test_load_wav_mono_averages_stereo(tmp_path):
    path = tmp_path / "stereo.wav"
    stereo = np.array([[0.5, -0.5], [0.25, 0.75]], dtype=np.float32)
    sf.write(path, stereo, 8_000)

    samples, sample_rate = load_wav_mono(path)

    assert sample_rate == 8_000
    np.testing.assert_allclose(samples, np.array([0.0, 0.5], dtype=np.float32), atol=1e-4)


def test_tx_and_rx_main_functions_round_trip(tmp_path, capsys):
    path = tmp_path / "cli.wav"

    tx_code = tx_main(
        [
            "--callsign",
            "pu5lrk",
            "--sample-rate",
            "8000",
            "--symbol-duration",
            "0.01",
            "--f0",
            "1000",
            "--f1",
            "2000",
            "Teste",
            str(path),
        ]
    )
    rx_code = rx_main(
        [
            "--symbol-duration",
            "0.01",
            "--f0",
            "1000",
            "--f1",
            "2000",
            str(path),
        ]
    )
    output = capsys.readouterr().out

    assert tx_code == 0
    assert rx_code == 0
    assert "Payload: pu5lrk Teste" in output
    assert output.rstrip().endswith("pu5lrk Teste")
