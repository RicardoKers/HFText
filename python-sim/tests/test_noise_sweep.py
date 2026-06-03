from pathlib import Path

from noise_sweep import aggregate_results, main, run_sweep, snr_label


def test_snr_label_is_filesystem_friendly():
    assert snr_label(None) == "clean"
    assert snr_label(6.0) == "snr_p6p0db"
    assert snr_label(-12.5) == "snr_m12p5db"


def test_run_sweep_writes_wavs_and_summary(tmp_path):
    results = run_sweep(
        "Teste",
        callsign="pu5lrk",
        snrs_db=[12.0, -20.0],
        output_dir=tmp_path,
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
        seed=7,
        trials=2,
    )

    assert [result.label for result in results] == [
        "clean",
        "snr_p12p0db",
        "snr_p12p0db",
        "snr_m20p0db",
        "snr_m20p0db",
    ]
    assert all(result.wav_path.exists() for result in results if result.wav_path)
    assert (tmp_path / "summary.csv").exists()
    assert (tmp_path / "trials.csv").exists()
    assert results[0].frame_result.crc_ok
    assert results[0].frame_result.text == "pu5lrk Teste"
    assert results[0].confidence > 0.9
    assert "confidence" in (tmp_path / "trials.csv").read_text(encoding="utf-8")
    assert "avg_confidence" in (tmp_path / "summary.csv").read_text(encoding="utf-8")


def test_aggregate_results_groups_trials_by_snr(tmp_path):
    results = run_sweep(
        "Teste",
        callsign="pu5lrk",
        snrs_db=[12.0],
        output_dir=tmp_path,
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
        seed=7,
        trials=3,
        include_clean=False,
        save_wavs=False,
    )
    aggregate = aggregate_results(results)

    assert len(aggregate) == 1
    assert aggregate[0].label == "snr_p12p0db"
    assert aggregate[0].trials == 3
    assert aggregate[0].crc_success_rate == 1.0
    assert aggregate[0].avg_confidence > 0.0
    assert aggregate[0].min_confidence > 0.0


def test_main_prints_summary_and_returns_success(tmp_path, capsys):
    code = main(
        [
            "--callsign",
            "pu5lrk",
            "--output-dir",
            str(tmp_path),
            "--snr",
            "12",
            "--trials",
            "2",
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
    assert "label,snr_db,trials" in output
    assert "avg_confidence" in output
    assert Path(tmp_path / "clean.wav").exists()
    assert Path(tmp_path / "summary.csv").exists()
    assert Path(tmp_path / "trials.csv").exists()
