"""Reusable runner for C++ streaming receiver performance benchmarks."""

from __future__ import annotations

import csv
import json
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any, TextIO


@dataclass(frozen=True)
class ReceiverBenchmarkCase:
    name: str
    wav_path: Path
    mode: str
    symbol_duration: str
    f0: str
    f1: str
    expected_texts: tuple[str, ...] = ()


@dataclass(frozen=True)
class ReceiverBenchmarkResult:
    case: ReceiverBenchmarkCase
    passed: bool
    status: str
    return_code: int | None
    decoded_texts: tuple[str, ...]
    metrics: dict[str, Any]
    stdout: str
    stderr: str


def _compact_output(text: str, max_length: int = 500) -> str:
    stripped = text.strip()
    if len(stripped) <= max_length:
        return stripped
    return stripped[: max_length - 3] + "..."


def _decoded_texts(metrics: dict[str, Any]) -> tuple[str, ...]:
    messages = metrics.get("messages", [])
    return tuple(
        str(message.get("text", ""))
        for message in messages
        if isinstance(message, dict) and message.get("text")
    )


def run_benchmark_case(
    case: ReceiverBenchmarkCase,
    stream_command: list[str],
    metrics_path: Path,
    timeout: float,
) -> ReceiverBenchmarkResult:
    """Run one WAV through hftext_stream_wav and validate its JSON report."""
    if not case.wav_path.exists():
        return ReceiverBenchmarkResult(case, False, "missing_wav", None, (), {}, "", "")

    metrics_path.parent.mkdir(parents=True, exist_ok=True)
    metrics_path.unlink(missing_ok=True)
    command = [
        *stream_command,
        "--mode",
        case.mode,
        "--symbol-duration",
        case.symbol_duration,
        "--f0",
        case.f0,
        "--f1",
        case.f1,
        "--metrics-json",
        str(metrics_path),
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
        return ReceiverBenchmarkResult(
            case,
            False,
            "timeout",
            None,
            (),
            {},
            _compact_output(exc.stdout or ""),
            _compact_output(exc.stderr or ""),
        )

    if not metrics_path.exists():
        return ReceiverBenchmarkResult(
            case,
            False,
            "missing_metrics",
            completed.returncode,
            (),
            {},
            _compact_output(completed.stdout),
            _compact_output(completed.stderr),
        )

    try:
        metrics = json.loads(metrics_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return ReceiverBenchmarkResult(
            case,
            False,
            "invalid_metrics",
            completed.returncode,
            (),
            {},
            _compact_output(completed.stdout),
            _compact_output(completed.stderr),
        )

    decoded_texts = _decoded_texts(metrics)
    expected_found = all(expected in decoded_texts for expected in case.expected_texts)
    allowed_return_code = completed.returncode == 0 or (
        completed.returncode == 1 and not case.expected_texts
    )
    passed = metrics.get("schema_version") == 1 and expected_found and allowed_return_code
    return ReceiverBenchmarkResult(
        case,
        passed,
        "ok" if passed else "failed",
        completed.returncode,
        decoded_texts,
        metrics,
        _compact_output(completed.stdout),
        _compact_output(completed.stderr),
    )


def run_benchmark_cases(
    cases: list[ReceiverBenchmarkCase],
    stream_command: list[str],
    metrics_dir: Path,
    timeout: float,
) -> list[ReceiverBenchmarkResult]:
    """Run a benchmark corpus and retain one JSON report per case."""
    results = []
    for index, case in enumerate(cases):
        metrics_path = metrics_dir / f"{index:03d}-{case.name}.json"
        results.append(run_benchmark_case(case, stream_command, metrics_path, timeout))
    return results


def _nested(metrics: dict[str, Any], section: str, key: str, default: Any = "") -> Any:
    value = metrics.get(section, {})
    return value.get(key, default) if isinstance(value, dict) else default


def _nanoseconds_to_seconds(value: Any) -> float | str:
    try:
        return float(value) / 1.0e9
    except (TypeError, ValueError):
        return ""


def _nanoseconds_to_milliseconds(value: Any) -> float | str:
    try:
        return float(value) / 1.0e6
    except (TypeError, ValueError):
        return ""


BENCHMARK_FIELDS = [
    "case",
    "wav_path",
    "expected_text",
    "decoded_text",
    "passed",
    "status",
    "return_code",
    "hftext_version",
    "mode",
    "symbol_duration_s",
    "input_audio_s",
    "replay_wall_s",
    "realtime_factor",
    "frames_decoded",
    "phase_count",
    "push_calls",
    "samples_pushed",
    "phase_symbols_processed",
    "bit_decisions_produced",
    "sync_positions_examined",
    "sync_pattern_matches",
    "rejected_sync_cache_hits",
    "physical_length_attempts",
    "physical_length_valid",
    "physical_length_invalid",
    "frame_waiting_checks",
    "robust_decode_attempts",
    "valid_frame_candidates",
    "rejected_frame_candidates",
    "demodulation_s",
    "frame_search_s",
    "robust_decode_s",
    "total_push_s",
    "max_push_ms",
    "stdout",
    "stderr",
]


def write_benchmark_results(file: TextIO, results: list[ReceiverBenchmarkResult]) -> None:
    """Write flattened benchmark results to CSV."""
    writer = csv.DictWriter(file, fieldnames=BENCHMARK_FIELDS)
    writer.writeheader()
    for result in results:
        metrics = result.metrics
        writer.writerow(
            {
                "case": result.case.name,
                "wav_path": str(result.case.wav_path),
                "expected_text": "\n".join(result.case.expected_texts),
                "decoded_text": "\n".join(result.decoded_texts),
                "passed": "1" if result.passed else "0",
                "status": result.status,
                "return_code": "" if result.return_code is None else result.return_code,
                "hftext_version": metrics.get("hftext_version", ""),
                "mode": _nested(metrics, "config", "mode"),
                "symbol_duration_s": _nested(metrics, "config", "symbol_duration_s"),
                "input_audio_s": metrics.get("input_audio_s", ""),
                "replay_wall_s": metrics.get("replay_wall_s", ""),
                "realtime_factor": metrics.get("realtime_factor", ""),
                "frames_decoded": metrics.get("frames_decoded", ""),
                "phase_count": _nested(metrics, "receiver", "phase_count"),
                "push_calls": _nested(metrics, "receiver", "push_calls"),
                "samples_pushed": _nested(metrics, "receiver", "samples_pushed"),
                "phase_symbols_processed": _nested(metrics, "receiver", "phase_symbols_processed"),
                "bit_decisions_produced": _nested(metrics, "receiver", "bit_decisions_produced"),
                "sync_positions_examined": _nested(metrics, "receiver", "sync_positions_examined"),
                "sync_pattern_matches": _nested(metrics, "receiver", "sync_pattern_matches"),
                "rejected_sync_cache_hits": _nested(metrics, "receiver", "rejected_sync_cache_hits"),
                "physical_length_attempts": _nested(metrics, "receiver", "physical_length_attempts"),
                "physical_length_valid": _nested(metrics, "receiver", "physical_length_valid"),
                "physical_length_invalid": _nested(metrics, "receiver", "physical_length_invalid"),
                "frame_waiting_checks": _nested(metrics, "receiver", "frame_waiting_checks"),
                "robust_decode_attempts": _nested(metrics, "receiver", "robust_decode_attempts"),
                "valid_frame_candidates": _nested(metrics, "receiver", "valid_frame_candidates"),
                "rejected_frame_candidates": _nested(metrics, "receiver", "rejected_frame_candidates"),
                "demodulation_s": _nanoseconds_to_seconds(
                    _nested(metrics, "receiver", "demodulation_time_ns")
                ),
                "frame_search_s": _nanoseconds_to_seconds(
                    _nested(metrics, "receiver", "frame_search_time_ns")
                ),
                "robust_decode_s": _nanoseconds_to_seconds(
                    _nested(metrics, "receiver", "robust_decode_time_ns")
                ),
                "total_push_s": _nanoseconds_to_seconds(
                    _nested(metrics, "receiver", "total_push_time_ns")
                ),
                "max_push_ms": _nanoseconds_to_milliseconds(
                    _nested(metrics, "receiver", "max_push_time_ns")
                ),
                "stdout": result.stdout,
                "stderr": result.stderr,
            }
        )


def write_benchmark_csv(path: Path, results: list[ReceiverBenchmarkResult]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as file:
        write_benchmark_results(file, results)

