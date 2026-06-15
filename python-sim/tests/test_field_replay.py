import csv
import os
import sys
from io import StringIO

from field_summary import collect_summaries
from field_replay import build_replay_cases, replay_case, replay_cases, write_replay_results


def evidence_text(wav_path: str, received_text: str = "pu5lrk Ola!") -> str:
    return (
        "HFText evidencia RX\n"
        "\n"
        "--- Resumo CSV ---\n"
        "generated_at,modulation,symbol_duration_s,f0_hz,f1_hz,rx_accepted,received_text,wav_path\n"
        f'"2026-06-06T17:59:11","4-FSK exp v0.2",0.300,1200.0,1600.0,1,"{received_text}","{wav_path}"\n'
        "\n"
        "--- Log ---\n"
    )


def fake_decoder_command(tmp_path, decoded_text: str = "pu5lrk Ola!", return_code: int = 0):
    script = tmp_path / "fake_rx.py"
    script.write_text(
        "import sys\n"
        f"print({decoded_text!r})\n"
        f"raise SystemExit({return_code})\n",
        encoding="utf-8",
    )

    if os.name == "nt":
        wrapper = tmp_path / "fake_rx.cmd"
        wrapper.write_text(f'@echo off\r\n"{sys.executable}" "{script}" %*\r\n', encoding="utf-8")
        return [str(wrapper)]

    wrapper = tmp_path / "fake_rx"
    wrapper.write_text(f"#! /bin/sh\nexec {sys.executable!r} {str(script)!r} \"$@\"\n", encoding="utf-8")
    wrapper.chmod(0o755)
    return [str(wrapper)]


def test_build_replay_cases_uses_accepted_evidence(tmp_path):
    wav_path = tmp_path / "capture.wav"
    wav_path.write_bytes(b"fake")
    evidence = tmp_path / "evidence.txt"
    evidence.write_text(evidence_text(str(wav_path)), encoding="utf-8")

    cases = build_replay_cases(collect_summaries(tmp_path))

    assert len(cases) == 1
    assert cases[0].wav_path == wav_path
    assert cases[0].expected_lines == ["pu5lrk Ola!"]
    assert cases[0].mode == "4fsk"
    assert cases[0].symbol_duration == "0.300"
    assert cases[0].f0 == "1200.0"
    assert cases[0].f1 == "1600.0"


def test_replay_case_passes_when_decoder_matches_expected_text(tmp_path):
    wav_path = tmp_path / "capture.wav"
    wav_path.write_bytes(b"fake")
    evidence = tmp_path / "evidence.txt"
    evidence.write_text(evidence_text(str(wav_path)), encoding="utf-8")
    case = build_replay_cases(collect_summaries(tmp_path))[0]

    result = replay_case(case, fake_decoder_command(tmp_path))

    assert result.passed
    assert result.status == "ok"
    assert result.return_code == 0
    assert result.decoded_text == "pu5lrk Ola!"


def test_replay_case_fails_when_decoder_text_differs(tmp_path):
    wav_path = tmp_path / "capture.wav"
    wav_path.write_bytes(b"fake")
    evidence = tmp_path / "evidence.txt"
    evidence.write_text(evidence_text(str(wav_path)), encoding="utf-8")
    case = build_replay_cases(collect_summaries(tmp_path))[0]

    result = replay_case(case, fake_decoder_command(tmp_path, decoded_text="pu5lrk Outro"))

    assert not result.passed
    assert result.status == "failed"
    assert result.decoded_text == "pu5lrk Outro"


def test_replay_case_reports_missing_wav(tmp_path):
    evidence = tmp_path / "evidence.txt"
    evidence.write_text(evidence_text(str(tmp_path / "missing.wav")), encoding="utf-8")
    case = build_replay_cases(collect_summaries(tmp_path))[0]

    result = replay_case(case, fake_decoder_command(tmp_path))

    assert not result.passed
    assert result.status == "missing_wav"
    assert result.return_code is None


def test_write_replay_results_csv(tmp_path):
    wav_path = tmp_path / "capture.wav"
    wav_path.write_bytes(b"fake")
    evidence = tmp_path / "evidence.txt"
    evidence.write_text(evidence_text(str(wav_path)), encoding="utf-8")
    case = build_replay_cases(collect_summaries(tmp_path))[0]
    results = replay_cases([case], fake_decoder_command(tmp_path))
    output = StringIO()

    write_replay_results(output, results)

    rows = list(csv.DictReader(StringIO(output.getvalue())))
    assert rows[0]["passed"] == "1"
    assert rows[0]["decoded_text"] == "pu5lrk Ola!"
