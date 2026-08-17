import csv
import json
import sys
from io import StringIO

from field_summary import collect_summaries
from hftext.receiver_benchmark import (
    ReceiverBenchmarkCase,
    run_benchmark_case,
    write_benchmark_results,
)
from receiver_benchmark import build_benchmark_cases


def fake_stream_command(tmp_path, messages=None, return_code=0):
    messages = ["pu5lrk test"] if messages is None else messages
    script = tmp_path / "fake_stream.py"
    script.write_text(
        "import json\n"
        "import sys\n"
        "path = sys.argv[sys.argv.index('--metrics-json') + 1]\n"
        f"messages = {messages!r}\n"
        "report = {\n"
        "    'schema_version': 1,\n"
        "    'hftext_version': 'test',\n"
        "    'input_audio_s': 10.0,\n"
        "    'replay_wall_s': 2.0,\n"
        "    'realtime_factor': 5.0,\n"
        "    'frames_decoded': len(messages),\n"
        "    'messages': [{'text': text} for text in messages],\n"
        "    'config': {'mode': '8fsk', 'symbol_duration_s': 0.1},\n"
        "    'receiver': {\n"
        "        'phase_count': 50,\n"
        "        'push_calls': 20,\n"
        "        'samples_pushed': 480000,\n"
        "        'phase_symbols_processed': 5000,\n"
        "        'bit_decisions_produced': 15000,\n"
        "        'sync_positions_examined': 1000,\n"
        "        'sync_pattern_matches': 5,\n"
        "        'rejected_sync_cache_hits': 2,\n"
        "        'physical_length_attempts': 3,\n"
        "        'physical_length_valid': 2,\n"
        "        'physical_length_invalid': 1,\n"
        "        'frame_waiting_checks': 4,\n"
        "        'robust_decode_attempts': 2,\n"
        "        'valid_frame_candidates': 1,\n"
        "        'rejected_frame_candidates': 1,\n"
        "        'demodulation_time_ns': 1000000000,\n"
        "        'frame_search_time_ns': 500000000,\n"
        "        'robust_decode_time_ns': 250000000,\n"
        "        'total_push_time_ns': 2000000000,\n"
        "        'max_push_time_ns': 200000000,\n"
        "    },\n"
        "}\n"
        "with open(path, 'w', encoding='utf-8') as file:\n"
        "    json.dump(report, file)\n"
        f"raise SystemExit({return_code})\n",
        encoding="utf-8",
    )
    return [sys.executable, str(script)]


def benchmark_case(tmp_path, expected_texts=("pu5lrk test",)):
    wav_path = tmp_path / "capture.wav"
    wav_path.write_bytes(b"fake")
    return ReceiverBenchmarkCase(
        name="capture",
        wav_path=wav_path,
        mode="8fsk",
        symbol_duration="0.1",
        f0="1050",
        f1="1180",
        expected_texts=expected_texts,
    )


def test_run_benchmark_case_accepts_matching_machine_readable_result(tmp_path):
    case = benchmark_case(tmp_path)

    result = run_benchmark_case(
        case,
        fake_stream_command(tmp_path),
        tmp_path / "metrics.json",
        timeout=5.0,
    )

    assert result.passed
    assert result.status == "ok"
    assert result.decoded_texts == ("pu5lrk test",)
    assert result.metrics["realtime_factor"] == 5.0


def test_run_benchmark_case_rejects_payload_mismatch(tmp_path):
    case = benchmark_case(tmp_path, expected_texts=("different",))

    result = run_benchmark_case(
        case,
        fake_stream_command(tmp_path),
        tmp_path / "metrics.json",
        timeout=5.0,
    )

    assert not result.passed
    assert result.status == "failed"


def test_run_benchmark_case_accepts_no_frame_for_failure_fixture(tmp_path):
    case = benchmark_case(tmp_path, expected_texts=())

    result = run_benchmark_case(
        case,
        fake_stream_command(tmp_path, messages=[], return_code=1),
        tmp_path / "metrics.json",
        timeout=5.0,
    )

    assert result.passed
    assert result.return_code == 1
    assert result.decoded_texts == ()


def test_write_benchmark_results_flattens_stage_metrics(tmp_path):
    case = benchmark_case(tmp_path)
    result = run_benchmark_case(
        case,
        fake_stream_command(tmp_path),
        tmp_path / "metrics.json",
        timeout=5.0,
    )
    output = StringIO()

    write_benchmark_results(output, [result])

    row = next(csv.DictReader(StringIO(output.getvalue())))
    assert row["passed"] == "1"
    assert row["realtime_factor"] == "5.0"
    assert row["sync_positions_examined"] == "1000"
    assert row["demodulation_s"] == "1.0"
    assert row["max_push_ms"] == "200.0"


def test_metrics_json_fixture_is_valid_json(tmp_path):
    case = benchmark_case(tmp_path)
    metrics_path = tmp_path / "metrics.json"

    run_benchmark_case(case, fake_stream_command(tmp_path), metrics_path, timeout=5.0)

    assert json.loads(metrics_path.read_text(encoding="utf-8"))["schema_version"] == 1


def test_build_benchmark_cases_prefers_companion_wav_from_copied_evidence(tmp_path):
    report_path = tmp_path / "capture.txt"
    companion_wav = report_path.with_suffix(".wav")
    companion_wav.write_bytes(b"fake")
    report_path.write_text(
        "HFText RX evidence\n\n"
        "--- Summary CSV ---\n"
        "modulation,symbol_duration_s,f0_hz,f1_hz,rx_accepted,received_text,wav_path\n"
        '"8-FSK exp v0.3",0.100,1050.0,1180.0,1,"pu5lrk test","C:/old/computer/capture.wav"\n',
        encoding="utf-8",
    )

    cases = build_benchmark_cases(collect_summaries(tmp_path))

    assert len(cases) == 1
    assert cases[0].wav_path == companion_wav
    assert cases[0].mode == "8fsk"
    assert cases[0].expected_texts == ("pu5lrk test",)
