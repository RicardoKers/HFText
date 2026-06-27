from pathlib import Path

import pytest

from interleaving_sweep import best_results_by_snr, candidate_interleave_shapes, main, parse_rows
from repetition_sweep import RepetitionAggregateResult


def test_candidate_interleave_shapes_returns_exact_rectangles():
    assert candidate_interleave_shapes(552, min_rows=2, max_rows=6) == [
        (2, 276),
        (3, 184),
        (4, 138),
        (6, 92),
    ]


def test_candidate_interleave_shapes_rejects_invalid_parameters():
    with pytest.raises(ValueError, match="bit_count"):
        candidate_interleave_shapes(0)

    with pytest.raises(ValueError, match="min_rows"):
        candidate_interleave_shapes(10, min_rows=0)

    with pytest.raises(ValueError, match="max_rows"):
        candidate_interleave_shapes(10, min_rows=5, max_rows=4)


def test_parse_rows_rejects_invalid_rows():
    with pytest.raises(ValueError, match="rows"):
        parse_rows([4, 0])


def test_best_results_by_snr_prefers_crc_then_ber():
    weak_crc_low_ber = RepetitionAggregateResult(
        label="low_ber",
        snr_db=-12.0,
        factor=3,
        trials=10,
        crc_successes=1,
        payload_successes=1,
        avg_recovered_ber=0.01,
        max_recovered_ber=0.02,
        avg_confidence=0.7,
        min_confidence=0.6,
    )
    strong_crc_high_ber = RepetitionAggregateResult(
        label="high_crc",
        snr_db=-12.0,
        factor=3,
        trials=10,
        crc_successes=2,
        payload_successes=2,
        avg_recovered_ber=0.03,
        max_recovered_ber=0.04,
        avg_confidence=0.7,
        min_confidence=0.6,
    )
    clean = RepetitionAggregateResult(
        label="clean",
        snr_db=None,
        factor=3,
        trials=1,
        crc_successes=1,
        payload_successes=1,
        avg_recovered_ber=0.0,
        max_recovered_ber=0.0,
        avg_confidence=1.0,
        min_confidence=1.0,
    )

    best = best_results_by_snr([weak_crc_low_ber, strong_crc_high_ber, clean])

    assert [result.label for result in best] == ["high_crc", "clean"]


def test_interleaving_sweep_main_prints_geometry_summary(tmp_path, capsys):
    code = main(
        [
            "--callsign",
            "pu5lrk",
            "--output-dir",
            str(tmp_path),
            "--factor",
            "3",
            "--rows",
            "4",
            "6",
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
    assert "snr_p6p0db_rep3" in output
    assert "snr_p6p0db_rep3_int4x132" in output
    assert "snr_p6p0db_rep3_int6x88" in output
    assert "best_by_snr" in output
    assert Path(tmp_path / "summary.csv").exists()
    assert Path(tmp_path / "trials.csv").exists()
    assert Path(tmp_path / "best_summary.csv").exists()


def test_interleaving_sweep_main_can_skip_baseline(tmp_path, capsys):
    code = main(
        [
            "--callsign",
            "pu5lrk",
            "--output-dir",
            str(tmp_path),
            "--factor",
            "3",
            "--rows",
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
            "--no-baseline",
            "Teste",
        ]
    )
    output = capsys.readouterr().out

    assert code == 0
    assert "snr_p6p0db_rep3_int4x132" in output
    assert "snr_p6p0db_rep3," not in output
