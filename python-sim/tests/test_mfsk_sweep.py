from mfsk_sweep import aggregate_results, run_mfsk_sweep


def test_run_mfsk_sweep_compares_2fsk_and_4fsk(tmp_path):
    results = run_mfsk_sweep(
        "Teste",
        callsign="pu5lrk",
        output_dir=tmp_path,
        snrs_db=[12.0],
        sample_rate=8_000,
        symbol_duration=0.01,
        seed=7,
        trials=2,
    )

    modes = {result.mode for result in results}
    assert modes == {"2fsk-v0.1", "4fsk-v0.2-exp"}
    clean = [result for result in results if result.snr_db is None]
    assert len(clean) == 2
    assert all(result.frame_result.payload_valid for result in clean)

    aggregates = aggregate_results(results)
    clean_2fsk = next(result for result in aggregates if result.mode == "2fsk-v0.1" and result.snr_db is None)
    clean_4fsk = next(result for result in aggregates if result.mode == "4fsk-v0.2-exp" and result.snr_db is None)
    assert clean_4fsk.duration_s < clean_2fsk.duration_s
    assert (tmp_path / "trials.csv").exists()
    assert (tmp_path / "summary.csv").exists()
