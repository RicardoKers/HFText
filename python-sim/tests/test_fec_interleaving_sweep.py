from pathlib import Path

from fec_interleaving_sweep import best_results_by_snr, main
from fec_sweep import FecAggregateResult


def test_best_results_by_snr_prefers_crc_then_ber():
    lower_crc = FecAggregateResult(
        label="lower_crc",
        mode="hamming74",
        snr_db=-12.0,
        trials=10,
        duration_multiplier=1.75,
        crc_successes=1,
        payload_successes=1,
        avg_channel_ber=0.08,
        avg_recovered_ber=0.01,
        max_recovered_ber=0.02,
        avg_corrected_codewords=10.0,
        avg_invalid_codewords=0.0,
        avg_confidence=0.7,
        min_confidence=0.6,
        interleave_rows=2,
        interleave_columns=10,
    )
    higher_crc = FecAggregateResult(
        label="higher_crc",
        mode="hamming74",
        snr_db=-12.0,
        trials=10,
        duration_multiplier=1.75,
        crc_successes=2,
        payload_successes=2,
        avg_channel_ber=0.08,
        avg_recovered_ber=0.03,
        max_recovered_ber=0.04,
        avg_corrected_codewords=12.0,
        avg_invalid_codewords=0.0,
        avg_confidence=0.7,
        min_confidence=0.6,
        interleave_rows=4,
        interleave_columns=5,
    )

    best = best_results_by_snr([lower_crc, higher_crc])

    assert best == [higher_crc]


def test_fec_interleaving_sweep_main_prints_geometry_summary(tmp_path, capsys):
    code = main(
        [
            "--callsign",
            "pu5lrk",
            "--output-dir",
            str(tmp_path),
            "--mode",
            "hamming74",
            "--rows",
            "7",
            "14",
            "--snr",
            "6",
            "--trials",
            "1",
            "--sample-rate",
            "8000",
            "--symbol-duration",
            "0.01",
            "--f0",
            "1000",
            "--f1",
            "2000",
            "--no-clean",
            "Teste",
        ]
    )
    output = capsys.readouterr().out

    assert code == 0
    assert "snr_p6p0db_hamming74" in output
    assert "snr_p6p0db_hamming74_int7x44" in output
    assert "snr_p6p0db_hamming74_int14x22" in output
    assert "best_by_snr" in output
    assert "avg_decoder_distance" in output
    assert Path(tmp_path / "summary.csv").exists()
    assert Path(tmp_path / "trials.csv").exists()
    assert Path(tmp_path / "best_summary.csv").exists()
    assert "avg_decoder_distance" in (tmp_path / "best_summary.csv").read_text(encoding="utf-8")


def test_fec_interleaving_sweep_main_can_skip_baseline(tmp_path, capsys):
    code = main(
        [
            "--callsign",
            "pu5lrk",
            "--output-dir",
            str(tmp_path),
            "--mode",
            "hamming74",
            "--rows",
            "7",
            "--snr",
            "6",
            "--trials",
            "1",
            "--sample-rate",
            "8000",
            "--symbol-duration",
            "0.01",
            "--f0",
            "1000",
            "--f1",
            "2000",
            "--no-clean",
            "--no-baseline",
            "Teste",
        ]
    )
    output = capsys.readouterr().out

    assert code == 0
    assert "snr_p6p0db_hamming74_int7x44" in output
    assert "snr_p6p0db_hamming74," not in output


def test_fec_interleaving_sweep_main_can_test_convolutional_mode(tmp_path, capsys):
    code = main(
        [
            "--callsign",
            "pu5lrk",
            "--output-dir",
            str(tmp_path),
            "--mode",
            "conv_k3",
            "--rows",
            "2",
            "4",
            "--snr",
            "6",
            "--trials",
            "1",
            "--sample-rate",
            "8000",
            "--symbol-duration",
            "0.01",
            "--f0",
            "1000",
            "--f1",
            "2000",
            "--no-clean",
            "Teste",
        ]
    )
    output = capsys.readouterr().out

    assert code == 0
    assert "snr_p6p0db_conv_k3" in output
    assert "snr_p6p0db_conv_k3_int2x178" in output
    assert "snr_p6p0db_conv_k3_int4x89" in output
    assert "best_by_snr" in output


def test_fec_interleaving_sweep_main_can_use_auto_shape(tmp_path, capsys):
    code = main(
        [
            "--callsign",
            "pu5lrk",
            "--output-dir",
            str(tmp_path),
            "--mode",
            "conv_k3",
            "--auto-shape",
            "--snr",
            "6",
            "--trials",
            "1",
            "--sample-rate",
            "8000",
            "--symbol-duration",
            "0.01",
            "--f0",
            "1000",
            "--f1",
            "2000",
            "--no-clean",
            "Teste",
        ]
    )
    output = capsys.readouterr().out

    assert code == 0
    assert "snr_p6p0db_conv_k3_int4x89" in output
    assert "snr_p6p0db_conv_k3_int2x178" not in output
