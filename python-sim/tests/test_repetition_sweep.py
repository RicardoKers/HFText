from pathlib import Path

import pytest

from repetition_sweep import aggregate_results, main, run_repetition_sweep


def test_run_repetition_sweep_compares_factors_and_writes_csv(tmp_path):
    results = run_repetition_sweep(
        "Teste",
        callsign="pu5lrk",
        snrs_db=[6.0],
        output_dir=tmp_path,
        factors=[1, 3],
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
        seed=7,
        trials=2,
    )

    assert [result.label for result in results] == [
        "clean_rep1",
        "snr_p6p0db_rep1",
        "snr_p6p0db_rep1",
        "clean_rep3",
        "snr_p6p0db_rep3",
        "snr_p6p0db_rep3",
    ]
    assert results[0].factor == 1
    assert results[3].factor == 3
    assert results[0].frame_result.crc_ok
    assert results[3].frame_result.crc_ok
    assert "duration_multiplier" in (tmp_path / "summary.csv").read_text(encoding="utf-8")
    assert "recovered_ber" in (tmp_path / "trials.csv").read_text(encoding="utf-8")


def test_repetition_sweep_aggregate_reports_duration_multiplier(tmp_path):
    results = run_repetition_sweep(
        "Teste",
        callsign="pu5lrk",
        snrs_db=[6.0],
        output_dir=tmp_path,
        factors=[3],
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
    assert aggregate[0].factor == 3
    assert aggregate[0].duration_multiplier == 3
    assert aggregate[0].crc_success_rate == 1.0


def test_repetition_sweep_main_prints_summary(tmp_path, capsys):
    code = main(
        [
            "--callsign",
            "pu5lrk",
            "--output-dir",
            str(tmp_path),
            "--factor",
            "1",
            "3",
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
    assert "clean_rep1" in output
    assert Path(tmp_path / "summary.csv").exists()


def test_repetition_sweep_can_apply_block_fading(tmp_path):
    results = run_repetition_sweep(
        "Teste",
        callsign="pu5lrk",
        snrs_db=[6.0],
        output_dir=tmp_path,
        factors=[3],
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

    assert results[0].label == "snr_p6p0db_rep3_fade"
    assert results[0].fading_block_symbols == 2
    assert results[0].fading_min_gain == 0.5
    assert results[0].fading_max_gain == 1.0
    assert "fading_block_symbols" in (tmp_path / "trials.csv").read_text(encoding="utf-8")


def test_repetition_sweep_can_apply_interleaving(tmp_path):
    results = run_repetition_sweep(
        "Teste",
        callsign="pu5lrk",
        snrs_db=[6.0],
        output_dir=tmp_path,
        factors=[3],
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
        seed=7,
        trials=1,
        include_clean=False,
        interleave_rows=4,
        interleave_columns=138,
    )

    assert results[0].label == "snr_p6p0db_rep3_int4x138"
    assert results[0].interleave_rows == 4
    assert results[0].interleave_columns == 138
    assert results[0].frame_result.crc_ok
    trials_csv = (tmp_path / "trials.csv").read_text(encoding="utf-8")
    assert "interleave_rows" in trials_csv
    assert "interleave_columns" in trials_csv


def test_repetition_sweep_rejects_invalid_fading_parameters(tmp_path):
    with pytest.raises(ValueError, match="fading_block_symbols"):
        run_repetition_sweep("Teste", None, [6.0], tmp_path, fading_block_symbols=0)

    with pytest.raises(ValueError, match="fading_min_gain"):
        run_repetition_sweep("Teste", None, [6.0], tmp_path, fading_min_gain=-0.1)

    with pytest.raises(ValueError, match="fading_max_gain"):
        run_repetition_sweep("Teste", None, [6.0], tmp_path, fading_min_gain=1.0, fading_max_gain=0.5)


def test_repetition_sweep_rejects_invalid_interleaving_parameters(tmp_path):
    with pytest.raises(ValueError, match="set together"):
        run_repetition_sweep("Teste", None, [6.0], tmp_path, interleave_rows=4)

    with pytest.raises(ValueError, match="interleave_rows"):
        run_repetition_sweep("Teste", None, [6.0], tmp_path, interleave_rows=0, interleave_columns=4)

    with pytest.raises(ValueError, match="interleave_columns"):
        run_repetition_sweep("Teste", None, [6.0], tmp_path, interleave_rows=4, interleave_columns=0)

    with pytest.raises(ValueError, match="repeated bit count"):
        run_repetition_sweep("Teste", None, [6.0], tmp_path, interleave_rows=5, interleave_columns=5)
