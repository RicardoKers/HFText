"""Benchmark field evidence with the C++ HFText streaming receiver."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from field_summary import EvidenceSummary, collect_summaries
from hftext.receiver_benchmark import (
    ReceiverBenchmarkCase,
    run_benchmark_cases,
    write_benchmark_csv,
    write_benchmark_results,
)


REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_INPUT_DIR = REPO_ROOT / "Evidence"
DEFAULT_OUTPUT = REPO_ROOT / "logs" / "receiver_benchmark.csv"
DEFAULT_METRICS_DIR = REPO_ROOT / "logs" / "receiver_benchmark_metrics"


def candidate_stream_executables() -> list[Path]:
    names = ["hftext_stream_wav.exe", "hftext_stream_wav"]
    directories = [
        REPO_ROOT / "build-performance" / "core" / "Release",
        REPO_ROOT / "build-qt15" / "core" / "Release",
        REPO_ROOT / "build-loopback" / "core" / "Release",
        REPO_ROOT / "core" / "build-performance" / "Release",
        REPO_ROOT / "build" / "core" / "Release",
        REPO_ROOT / "core" / "build" / "Release",
    ]
    return [directory / name for directory in directories for name in names]


def find_stream_executable() -> Path | None:
    return next((path for path in candidate_stream_executables() if path.exists()), None)


def _mode_from_summary(summary: EvidenceSummary) -> str:
    modulation = summary.row.get("modulation", "").lower()
    if "8" in modulation:
        return "8fsk"
    return "4fsk" if "4" in modulation else "2fsk"


def _expected_texts(summary: EvidenceSummary) -> tuple[str, ...]:
    return tuple(line.strip() for line in summary.row.get("received_text", "").splitlines() if line.strip())


def _local_wav_path(summary: EvidenceSummary) -> Path:
    companion = summary.source_path.with_suffix(".wav")
    if companion.exists():
        return companion
    configured = Path(summary.row.get("wav_path", "").strip())
    if configured.is_absolute():
        return configured
    return (summary.source_path.parent / configured).resolve() if configured else companion


def build_benchmark_cases(
    summaries: list[EvidenceSummary],
    include_failures: bool = False,
) -> list[ReceiverBenchmarkCase]:
    cases = []
    for summary in summaries:
        expected_texts = _expected_texts(summary)
        if not include_failures and not expected_texts:
            continue
        cases.append(
            ReceiverBenchmarkCase(
                name=summary.source_path.stem,
                wav_path=_local_wav_path(summary),
                mode=_mode_from_summary(summary),
                symbol_duration=summary.row.get("symbol_duration_s", "0.5") or "0.5",
                f0=summary.row.get("f0_hz", "1200.0") or "1200.0",
                f1=summary.row.get("f1_hz", "1600.0") or "1600.0",
                expected_texts=expected_texts,
            )
        )
    return cases


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", default=str(DEFAULT_INPUT_DIR), help="evidence directory")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT), help="flattened benchmark CSV path")
    parser.add_argument("--metrics-dir", default=str(DEFAULT_METRICS_DIR), help="per-case JSON output directory")
    parser.add_argument("--stream-exe", default=None, help="path to hftext_stream_wav executable")
    parser.add_argument("--timeout", type=float, default=900.0, help="seconds allowed per WAV")
    parser.add_argument("--include-failures", action="store_true", help="include evidence without accepted text")
    parser.add_argument("--stdout", action="store_true", help="write flattened CSV to stdout")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    stream_exe = Path(args.stream_exe) if args.stream_exe else find_stream_executable()
    if stream_exe is None:
        print("Error: hftext_stream_wav not found. Use --stream-exe.", file=sys.stderr)
        return 2

    cases = build_benchmark_cases(
        collect_summaries(args.input_dir),
        include_failures=args.include_failures,
    )
    results = run_benchmark_cases(
        cases,
        [str(stream_exe)],
        Path(args.metrics_dir),
        args.timeout,
    )

    if args.stdout:
        write_benchmark_results(sys.stdout, results)
    else:
        write_benchmark_csv(Path(args.output), results)

    passed = sum(result.passed for result in results)
    report_stream = sys.stderr if args.stdout else sys.stdout
    print(f"benchmarks,{len(results)}", file=report_stream)
    print(f"passed,{passed}", file=report_stream)
    print(f"failed,{len(results) - passed}", file=report_stream)
    if not args.stdout:
        print(f"csv,{Path(args.output)}", file=report_stream)
        print(f"json_dir,{Path(args.metrics_dir)}", file=report_stream)
    return 0 if results and all(result.passed for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())

