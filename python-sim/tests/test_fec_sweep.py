from pathlib import Path

import pytest

from fec_sweep import aggregate_results, main, run_fec_sweep


def test_run_fec_sweep_compares_raw_and_hamming_and_writes_csv(tmp_path):
    results = run_fec_sweep(
        "Teste",
        callsign="pu5lrk",
        snrs_db=[6.0],
        output_dir=tmp_path,
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
        seed=7,
        trials=1,
    )

    assert [result.label for result in results] == [
        "clean_raw",
        "snr_p6p0db_raw",
        "clean_hamming74",
        "snr_p6p0db_hamming74",
        "clean_conv_k3",
        "snr_p6p0db_conv_k3",
    ]
    assert results[0].duration_multiplier == 1.0
    assert results[2].duration_multiplier > 1.0
    assert results[4].duration_multiplier > results[2].duration_multiplier
    assert results[0].frame_result.crc_ok
    assert results[2].frame_result.crc_ok
    assert results[4].frame_result.crc_ok
    assert "duration_multiplier" in (tmp_path / "summary.csv").read_text(encoding="utf-8")
    assert "corrected_codewords" in (tmp_path / "trials.csv").read_text(encoding="utf-8")
    assert "decoder_distance" in (tmp_path / "trials.csv").read_text(encoding="utf-8")


def test_fec_sweep_aggregate_reports_hamming_overhead(tmp_path):
    results = run_fec_sweep(
        "Teste",
        callsign="pu5lrk",
        snrs_db=[6.0],
        output_dir=tmp_path,
        modes=["hamming74"],
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
        seed=7,
        trials=2,
        include_clean=False,
    )

    aggregate = aggregate_results(results)

    assert len(aggregate) == 1
    assert aggregate[0].mode == "hamming74"
    assert aggregate[0].duration_multiplier > 1.0
    assert aggregate[0].crc_success_rate == 1.0


def test_fec_sweep_aggregate_reports_convolutional_overhead(tmp_path):
    results = run_fec_sweep(
        "Teste",
        callsign="pu5lrk",
        snrs_db=[6.0],
        output_dir=tmp_path,
        modes=["conv_k3"],
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
        seed=7,
        trials=2,
        include_clean=False,
    )

    aggregate = aggregate_results(results)

    assert len(aggregate) == 1
    assert aggregate[0].mode == "conv_k3"
    assert aggregate[0].duration_multiplier > 2.0
    assert aggregate[0].crc_success_rate == 1.0
    assert aggregate[0].avg_decoder_distance >= 0.0


def test_fec_sweep_main_prints_summary(tmp_path, capsys):
    code = main(
        [
            "--callsign",
            "pu5lrk",
            "--output-dir",
            str(tmp_path),
            "--mode",
            "raw",
            "hamming74",
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
            "Teste",
        ]
    )
    output = capsys.readouterr().out

    assert code == 0
    assert "duration_multiplier" in output
    assert "clean_raw" in output
    assert "clean_hamming74" in output
    assert "avg_decoder_distance" in output
    assert Path(tmp_path / "summary.csv").exists()


def test_fec_sweep_can_apply_block_fading(tmp_path):
    results = run_fec_sweep(
        "Teste",
        callsign="pu5lrk",
        snrs_db=[6.0],
        output_dir=tmp_path,
        modes=["hamming74"],
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
        seed=7,
        trials=1,
        include_clean=False,
        fading_block_symbols=2,
        fading_min_gain=0.5,
        fading_max_gain=1.0,
    )

    assert results[0].label == "snr_p6p0db_hamming74_fade"
    assert results[0].fading_block_symbols == 2
    assert results[0].fading_min_gain == 0.5
    assert results[0].fading_max_gain == 1.0


def test_fec_sweep_can_apply_interleaving_after_hamming(tmp_path):
    results = run_fec_sweep(
        "Teste",
        callsign="pu5lrk",
        snrs_db=[6.0],
        output_dir=tmp_path,
        modes=["hamming74"],
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
        seed=7,
        trials=1,
        include_clean=False,
        interleave_rows=7,
        interleave_columns=46,
    )

    assert results[0].label == "snr_p6p0db_hamming74_int7x46"
    assert results[0].interleave_rows == 7
    assert results[0].interleave_columns == 46
    assert results[0].frame_result.crc_ok
    trials_csv = (tmp_path / "trials.csv").read_text(encoding="utf-8")
    assert "interleave_rows" in trials_csv
    assert "interleave_columns" in trials_csv


def test_fec_sweep_rejects_invalid_parameters(tmp_path):
    with pytest.raises(ValueError, match="invalid FEC mode"):
        run_fec_sweep("Teste", None, [6.0], tmp_path, modes=["unknown"])

    with pytest.raises(ValueError, match="trials"):
        run_fec_sweep("Teste", None, [6.0], tmp_path, trials=0)

    with pytest.raises(ValueError, match="fading_block_symbols"):
        run_fec_sweep("Teste", None, [6.0], tmp_path, fading_block_symbols=0)

    with pytest.raises(ValueError, match="fading_min_gain"):
        run_fec_sweep("Teste", None, [6.0], tmp_path, fading_min_gain=-0.1)

    with pytest.raises(ValueError, match="fading_max_gain"):
        run_fec_sweep("Teste", None, [6.0], tmp_path, fading_min_gain=1.0, fading_max_gain=0.5)

    with pytest.raises(ValueError, match="set together"):
        run_fec_sweep("Teste", None, [6.0], tmp_path, interleave_rows=7)

    with pytest.raises(ValueError, match="interleave_rows"):
        run_fec_sweep("Teste", None, [6.0], tmp_path, interleave_rows=0, interleave_columns=7)

    with pytest.raises(ValueError, match="interleave_columns"):
        run_fec_sweep("Teste", None, [6.0], tmp_path, interleave_rows=7, interleave_columns=0)

    with pytest.raises(ValueError, match="encoded bit count"):
        run_fec_sweep(
            "Teste",
            None,
            [6.0],
            tmp_path,
            modes=["hamming74"],
            interleave_rows=5,
            interleave_columns=5,
        )
