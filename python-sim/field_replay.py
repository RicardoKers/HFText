"""Replay HFText field evidence WAVs through the C++ WAV decoder CLI."""

from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import TextIO

from field_summary import EvidenceSummary, collect_summaries


REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_LOG_DIR = REPO_ROOT / "logs"
DEFAULT_TIMEOUT_SECONDS = 120.0


@dataclass(frozen=True)
class ReplayCase:
    source_txt: Path
    wav_path: Path
    expected_lines: list[str]
    symbol_duration: str
    f0: str
    f1: str


@dataclass(frozen=True)
class ReplayResult:
    source_txt: Path
    wav_path: Path
    expected_text: str
    decoded_text: str
    passed: bool
    return_code: int | None
    status: str
    stdout: str
    stderr: str


def candidate_rx_executables() -> list[Path]:
    """Return likely locations for the C++ WAV decoder executable."""
    names = ["hftext_rx_wav.exe", "hftext_rx_wav"]
    dirs = [
        REPO_ROOT / "build-qt14" / "core" / "Release",
        REPO_ROOT / "build-qt14" / "core" / "Debug",
        REPO_ROOT / "build" / "core" / "Release",
        REPO_ROOT / "build" / "core" / "Debug",
        REPO_ROOT / "core" / "build" / "Release",
        REPO_ROOT / "core" / "build" / "Debug",
    ]
    return [directory / name for directory in dirs for name in names]


def find_rx_executable() -> Path | None:
    """Find a likely C++ WAV decoder executable, if it has been built."""
    for path in candidate_rx_executables():
        if path.exists():
            return path
    return None


def _split_expected_lines(text: str) -> list[str]:
    return [line.strip() for line in text.splitlines() if line.strip()]


def _resolve_wav_path(summary: EvidenceSummary) -> Path:
    wav_text = summary.row.get("wav_path", "").strip()
    if wav_text:
        wav_path = Path(wav_text)
        if wav_path.is_absolute():
            return wav_path
        return (summary.source_path.parent / wav_path).resolve()
    return summary.source_path.with_suffix(".wav")


def build_replay_cases(summaries: list[EvidenceSummary], include_failures: bool = False) -> list[ReplayCase]:
    """Build replay cases from evidence summaries."""
    cases = []
    for summary in summaries:
        accepted_text = summary.row.get("rx_accepted", "0").strip()
        accepted = accepted_text.isdigit() and int(accepted_text) > 0
        if not include_failures and not accepted:
            continue

        expected_lines = _split_expected_lines(summary.row.get("received_text", ""))
        if not include_failures and not expected_lines:
            continue

        cases.append(
            ReplayCase(
                source_txt=summary.source_path,
                wav_path=_resolve_wav_path(summary),
                expected_lines=expected_lines,
                symbol_duration=summary.row.get("symbol_duration_s", "0.5") or "0.5",
                f0=summary.row.get("f0_hz", "1200.0") or "1200.0",
                f1=summary.row.get("f1_hz", "1600.0") or "1600.0",
            )
        )
    return cases


def _compact_output(text: str, max_length: int = 500) -> str:
    stripped = text.strip()
    if len(stripped) <= max_length:
        return stripped
    return stripped[: max_length - 3] + "..."


def _decoded_text_from_stdout(stdout: str, return_code: int) -> str:
    if return_code != 0:
        return ""
    for line in stdout.splitlines():
        text = line.strip()
        if text:
            return text
    return ""


def replay_case(case: ReplayCase, rx_command: list[str], timeout: float = DEFAULT_TIMEOUT_SECONDS) -> ReplayResult:
    """Replay one evidence WAV through a decoder command."""
    if not case.wav_path.exists():
        return ReplayResult(
            source_txt=case.source_txt,
            wav_path=case.wav_path,
            expected_text="\n".join(case.expected_lines),
            decoded_text="",
            passed=False,
            return_code=None,
            status="missing_wav",
            stdout="",
            stderr="",
        )

    command = [
        *rx_command,
        "--symbol-duration",
        case.symbol_duration,
        "--f0",
        case.f0,
        "--f1",
        case.f1,
        str(case.wav_path),
    ]
    try:
        completed = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as exc:
        return ReplayResult(
            source_txt=case.source_txt,
            wav_path=case.wav_path,
            expected_text="\n".join(case.expected_lines),
            decoded_text="",
            passed=False,
            return_code=None,
            status="timeout",
            stdout=_compact_output(exc.stdout or ""),
            stderr=_compact_output(exc.stderr or ""),
        )

    decoded_text = _decoded_text_from_stdout(completed.stdout, completed.returncode)
    passed = completed.returncode == 0 and decoded_text in case.expected_lines
    return ReplayResult(
        source_txt=case.source_txt,
        wav_path=case.wav_path,
        expected_text="\n".join(case.expected_lines),
        decoded_text=decoded_text,
        passed=passed,
        return_code=completed.returncode,
        status="ok" if passed else "failed",
        stdout=_compact_output(completed.stdout),
        stderr=_compact_output(completed.stderr),
    )


def replay_cases(
    cases: list[ReplayCase],
    rx_command: list[str],
    timeout: float = DEFAULT_TIMEOUT_SECONDS,
) -> list[ReplayResult]:
    """Replay several evidence WAVs through a decoder command."""
    return [replay_case(case, rx_command, timeout=timeout) for case in cases]


def write_replay_csv(path: str | Path, results: list[ReplayResult]) -> None:
    """Write replay results to CSV."""
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="", encoding="utf-8") as file:
        write_replay_results(file, results)


def write_replay_results(file: TextIO, results: list[ReplayResult]) -> None:
    """Write replay results to an already-open text stream."""
    writer = csv.DictWriter(
        file,
        fieldnames=[
            "source_txt",
            "wav_path",
            "expected_text",
            "decoded_text",
            "passed",
            "return_code",
            "status",
            "stdout",
            "stderr",
        ],
    )
    writer.writeheader()
    for result in results:
        writer.writerow(
            {
                "source_txt": str(result.source_txt),
                "wav_path": str(result.wav_path),
                "expected_text": result.expected_text,
                "decoded_text": result.decoded_text,
                "passed": "1" if result.passed else "0",
                "return_code": "" if result.return_code is None else result.return_code,
                "status": result.status,
                "stdout": result.stdout,
                "stderr": result.stderr,
            }
        )


def print_report(results: list[ReplayResult], output_path: Path | None, stream: TextIO = sys.stdout) -> None:
    """Print a compact replay report."""
    passed = sum(1 for result in results if result.passed)
    print(f"replays,{len(results)}", file=stream)
    print(f"passou,{passed}", file=stream)
    print(f"falhou,{len(results) - passed}", file=stream)
    if output_path is not None:
        print(f"csv,{output_path}", file=stream)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", default=str(DEFAULT_LOG_DIR), help="directory containing evidence TXT files")
    parser.add_argument("--output", default=None, help="replay CSV path")
    parser.add_argument("--rx-exe", default=None, help="path to hftext_rx_wav executable")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT_SECONDS, help="seconds per replay")
    parser.add_argument("--include-failures", action="store_true", help="also replay evidences without accepted text")
    parser.add_argument("--stdout", action="store_true", help="write replay CSV to stdout instead of a file")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    rx_exe = Path(args.rx_exe) if args.rx_exe else find_rx_executable()
    if rx_exe is None:
        print("Erro: hftext_rx_wav nao encontrado. Use --rx-exe.", file=sys.stderr)
        return 2

    summaries = collect_summaries(args.input_dir)
    cases = build_replay_cases(summaries, include_failures=args.include_failures)
    results = replay_cases(cases, [str(rx_exe)], timeout=args.timeout)

    output_path = None
    if args.stdout:
        write_replay_results(sys.stdout, results)
    else:
        output_path = Path(args.output) if args.output else Path(args.input_dir) / "field_replay.csv"
        write_replay_csv(output_path, results)

    print_report(results, output_path, stream=sys.stderr if args.stdout else sys.stdout)
    return 0 if all(result.passed for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
