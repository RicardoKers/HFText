from pathlib import Path

import numpy as np

from channel_sweep import ChannelScenario, apply_scenario, main, run_channel_sweep


def test_apply_scenario_can_combine_effects():
    samples = np.ones(64, dtype=np.float32) * 0.5
    scenario = ChannelScenario(
        "combo",
        awgn_snr_db=20.0,
        gain=0.8,
        dc_offset=0.01,
        clip_limit=0.9,
        frequency_offset_hz=5.0,
        fading_block_symbols=1,
        fading_min_gain=0.9,
        fading_max_gain=1.0,
    )

    output = apply_scenario(
        samples,
        scenario,
        sample_rate=8_000,
        samples_per_symbol=16,
        rng=np.random.default_rng(7),
    )

    assert output.dtype == np.float32
    assert output.shape == samples.shape


def test_run_channel_sweep_writes_summary(tmp_path):
    scenarios = [
        ChannelScenario("clean"),
        ChannelScenario("awgn_12db", awgn_snr_db=12.0),
    ]

    results = run_channel_sweep(
        "Teste",
        callsign="pu5lrk",
        output_dir=tmp_path,
        scenarios=scenarios,
        sample_rate=8_000,
        symbol_duration=0.01,
        f0=1_000.0,
        f1=2_000.0,
        seed=7,
        trials=2,
        save_wavs=True,
    )

    assert [result.label for result in results] == ["clean", "clean", "awgn_12db", "awgn_12db"]
    assert (tmp_path / "summary.csv").exists()
    assert (tmp_path / "trials.csv").exists()
    assert Path(tmp_path / "clean.wav").exists()
    assert Path(tmp_path / "awgn_12db.wav").exists()
    assert results[0].frame_result.crc_ok
    assert results[0].frame_result.text == "pu5lrk Teste"
    assert results[0].confidence > 0.9
    assert "avg_confidence" in (tmp_path / "summary.csv").read_text(encoding="utf-8")


def test_main_prints_channel_summary(tmp_path, capsys):
    code = main(
        [
            "--callsign",
            "pu5lrk",
            "--output-dir",
            str(tmp_path),
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
    assert "label,trials,crc_success_rate" in output
    assert "avg_confidence" in output
    assert (tmp_path / "summary.csv").exists()
