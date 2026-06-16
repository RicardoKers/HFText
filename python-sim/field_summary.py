"""Aggregate HFText field evidence TXT files into one CSV table."""

from __future__ import annotations

import argparse
import csv
import sys
from dataclasses import dataclass
from io import StringIO
from pathlib import Path
from typing import Iterable, TextIO


SUMMARY_MARKERS = ("--- Summary CSV ---", "--- Resumo CSV ---")
FRAMES_MARKERS = ("--- Accepted Frames CSV ---", "--- Quadros aceitos CSV ---")
DEFAULT_LOG_DIR = Path(__file__).resolve().parent.parent / "logs"
DEFAULT_GROUP_BY = [
    "modulation",
    "symbol_duration_s",
    "f0_hz",
    "f1_hz",
    "amplitude",
    "preamble_bits",
    "detailed_log",
]
FRAME_DEDUP_COLUMNS = [
    "accepted_at",
    "modulation",
    "symbol_duration_s",
    "sample_rate_hz",
    "f0_hz",
    "f1_hz",
    "amplitude",
    "preamble_bits",
    "length",
    "text",
]


@dataclass(frozen=True)
class EvidenceSummary:
    source_path: Path
    row: dict[str, str]
    frame_rows: list[dict[str, str]]


def _read_csv_records(text: str, markers: str | tuple[str, ...]) -> list[list[str]] | None:
    """Return CSV records from a marked evidence TXT section, if present."""
    marker_set = {markers} if isinstance(markers, str) else set(markers)
    lines = text.splitlines()
    for index, line in enumerate(lines):
        if line.strip() not in marker_set:
            continue

        csv_lines = []
        for candidate in lines[index + 1 :]:
            if candidate.startswith("--- ") and csv_lines:
                break
            if candidate.strip() or csv_lines:
                csv_lines.append(candidate)
        if not csv_lines:
            return None

        reader = csv.reader(StringIO("\n".join(csv_lines)))
        records = [record for record in reader if record]
        if not records:
            return None
        return records

    return None


def _read_summary_records(text: str) -> tuple[list[str], list[str]] | None:
    """Return header/data CSV records from an evidence TXT, if present."""
    records = _read_csv_records(text, SUMMARY_MARKERS)
    if records is None or len(records) < 2:
        return None
    return records[0], records[1]


def _read_frame_rows(text: str) -> list[dict[str, str]]:
    """Return one row per accepted RX frame from an evidence TXT, if present."""
    records = _read_csv_records(text, FRAMES_MARKERS)
    if records is None:
        return []
    header, *values = records
    rows = []
    for record in values:
        if len(record) == len(header):
            rows.append(dict(zip(header, record, strict=True)))
    return rows


def parse_evidence_summary(path: str | Path) -> EvidenceSummary | None:
    """Parse the `Summary CSV` block from one field evidence TXT file."""
    evidence_path = Path(path)
    text = evidence_path.read_text(encoding="utf-8-sig", errors="replace")
    summary_records = _read_summary_records(text)
    if summary_records is None:
        return None

    header, values = summary_records
    if not header or len(values) != len(header):
        return None

    return EvidenceSummary(
        source_path=evidence_path,
        row=dict(zip(header, values, strict=True)),
        frame_rows=_read_frame_rows(text),
    )


def iter_evidence_files(input_dir: str | Path) -> list[Path]:
    """Return evidence TXT files in deterministic order."""
    root = Path(input_dir)
    if not root.exists():
        return []
    if root.is_file():
        return [root]
    return sorted(path for path in root.rglob("*.txt") if path.is_file())


def collect_summaries(input_dir: str | Path) -> list[EvidenceSummary]:
    """Collect all valid evidence summaries under an input directory."""
    summaries = []
    for path in iter_evidence_files(input_dir):
        summary = parse_evidence_summary(path)
        if summary is not None:
            summaries.append(summary)
    return summaries


def summary_columns(summaries: Iterable[EvidenceSummary]) -> list[str]:
    """Build a stable union of columns from evidence summaries."""
    columns = ["source_txt"]
    for summary in summaries:
        for column in summary.row:
            if column not in columns:
                columns.append(column)
    return columns


def write_summary_csv(path: str | Path, summaries: list[EvidenceSummary]) -> None:
    """Write an aggregate CSV containing all evidence summaries."""
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    columns = summary_columns(summaries)

    with output_path.open("w", newline="", encoding="utf-8") as file:
        write_summaries(file, summaries, columns)


def write_summaries(file: TextIO, summaries: list[EvidenceSummary], columns: list[str] | None = None) -> None:
    """Write summaries to an already-open text stream."""
    fieldnames = columns if columns is not None else summary_columns(summaries)
    writer = csv.DictWriter(file, fieldnames=fieldnames, extrasaction="ignore")
    writer.writeheader()
    for summary in summaries:
        row = {"source_txt": str(summary.source_path)}
        row.update(summary.row)
        writer.writerow(row)


def accepted_frame_columns(summaries: Iterable[EvidenceSummary]) -> list[str]:
    """Build a stable union of columns from accepted-frame evidence rows."""
    columns = ["source_txt"]
    for summary in summaries:
        for frame in summary.frame_rows:
            for column in frame:
                if column not in columns:
                    columns.append(column)
    return columns


def _accepted_frame_key(frame: dict[str, str]) -> tuple[str, ...]:
    """Return a stable key for one accepted frame across cumulative evidence saves."""
    return tuple(frame.get(column, "") for column in FRAME_DEDUP_COLUMNS)


def unique_accepted_frame_rows(summaries: list[EvidenceSummary]) -> list[tuple[EvidenceSummary, dict[str, str]]]:
    """Return accepted frames, deduplicating frames repeated by later evidence saves."""
    rows = []
    seen: set[tuple[str, ...]] = set()
    for summary in sorted(summaries, key=lambda item: str(item.source_path)):
        for frame in summary.frame_rows:
            key = _accepted_frame_key(frame)
            if key in seen:
                continue
            seen.add(key)
            rows.append((summary, frame))
    return rows


def write_accepted_frames(
    file: TextIO,
    summaries: list[EvidenceSummary],
    columns: list[str] | None = None,
    *,
    keep_duplicates: bool = False,
) -> None:
    """Write one CSV row per accepted frame from evidence files."""
    fieldnames = columns if columns is not None else accepted_frame_columns(summaries)
    writer = csv.DictWriter(file, fieldnames=fieldnames, extrasaction="ignore")
    writer.writeheader()
    if keep_duplicates:
        rows = [(summary, frame) for summary in summaries for frame in summary.frame_rows]
    else:
        rows = unique_accepted_frame_rows(summaries)
    for summary, frame in rows:
        row = {"source_txt": str(summary.source_path)}
        row.update(frame)
        writer.writerow(row)


def write_accepted_frames_csv(
    path: str | Path,
    summaries: list[EvidenceSummary],
    *,
    keep_duplicates: bool = False,
) -> None:
    """Write accepted RX frames as a flat CSV table."""
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="", encoding="utf-8") as file:
        write_accepted_frames(file, summaries, keep_duplicates=keep_duplicates)


def _parse_percent(value: str) -> float | None:
    text = value.strip().replace(",", ".")
    if not text.endswith("%"):
        return None
    try:
        return float(text[:-1])
    except ValueError:
        return None


def _parse_float(value: str) -> float | None:
    text = value.strip().replace(",", ".")
    if not text or text == "--":
        return None
    try:
        return float(text)
    except ValueError:
        return None


def _parse_int(value: str) -> int:
    try:
        return int(value)
    except ValueError:
        return 0


def _format_optional_float(value: float | None, digits: int = 2) -> str:
    if value is None:
        return ""
    return f"{value:.{digits}f}"


def grouped_summary_rows(summaries: list[EvidenceSummary], group_by: list[str]) -> list[dict[str, str]]:
    """Return aggregate rows grouped by selected evidence columns."""
    groups: dict[tuple[str, ...], list[EvidenceSummary]] = {}
    for summary in summaries:
        key = tuple(summary.row.get(column, "") for column in group_by)
        groups.setdefault(key, []).append(summary)

    rows = []
    for key in sorted(groups):
        group = groups[key]
        accepted = sum(_parse_int(summary.row.get("rx_accepted", "0")) for summary in group)
        qualities = [
            quality
            for summary in group
            if (quality := _parse_percent(summary.row.get("rx_quality", ""))) is not None
        ]
        elapsed = [
            value
            for summary in group
            if (value := _parse_float(summary.row.get("rx_elapsed_s", ""))) is not None
        ]
        rejected = [_parse_int(summary.row.get("rx_rejected_strong", "0")) for summary in group]
        phys_lengths = [_parse_int(summary.row.get("rx_phys_length", "0")) for summary in group]
        syncs = [_parse_int(summary.row.get("rx_sync", "0")) for summary in group]

        row = dict(zip(group_by, key, strict=True))
        row.update(
            {
                "evidences": str(len(group)),
                "rx_accepted": str(accepted),
                "accept_rate": f"{accepted / len(group):.3f}" if group else "0.000",
                "avg_quality_pct": _format_optional_float(
                    sum(qualities) / len(qualities) if qualities else None,
                    digits=1,
                ),
                "min_quality_pct": _format_optional_float(min(qualities) if qualities else None, digits=1),
                "avg_rx_elapsed_s": _format_optional_float(sum(elapsed) / len(elapsed) if elapsed else None),
                "avg_rejected_strong": _format_optional_float(sum(rejected) / len(rejected) if rejected else None),
                "avg_rx_phys_length": _format_optional_float(
                    sum(phys_lengths) / len(phys_lengths) if phys_lengths else None
                ),
                "avg_rx_sync": _format_optional_float(sum(syncs) / len(syncs) if syncs else None),
            }
        )
        rows.append(row)

    return rows


def write_grouped_summary_csv(path: str | Path, summaries: list[EvidenceSummary], group_by: list[str]) -> None:
    """Write a grouped aggregate CSV for field evidence comparisons."""
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = group_by + [
        "evidences",
        "rx_accepted",
        "accept_rate",
        "avg_quality_pct",
        "min_quality_pct",
        "avg_rx_elapsed_s",
        "avg_rejected_strong",
        "avg_rx_phys_length",
        "avg_rx_sync",
    ]
    with output_path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(grouped_summary_rows(summaries, group_by))


def print_report(
    summaries: list[EvidenceSummary],
    output_path: Path | None,
    group_output_path: Path | None,
    frames_output_path: Path | None,
    stream: TextIO = sys.stdout,
) -> None:
    """Print a compact human-readable aggregate report."""
    accepted = 0
    accepted_frame_rows = len(unique_accepted_frame_rows(summaries))
    accepted_frame_rows_raw = 0
    qualities = []
    for summary in summaries:
        try:
            accepted += int(summary.row.get("rx_accepted", "0"))
        except ValueError:
            pass
        accepted_frame_rows_raw += len(summary.frame_rows)
        quality = _parse_percent(summary.row.get("rx_quality", ""))
        if quality is not None:
            qualities.append(quality)

    print(f"evidence_files,{len(summaries)}", file=stream)
    print(f"accepted_frames,{accepted}", file=stream)
    print(f"unique_accepted_frames,{accepted_frame_rows}", file=stream)
    if accepted_frame_rows_raw != accepted_frame_rows:
        print(f"raw_accepted_frame_rows,{accepted_frame_rows_raw}", file=stream)
    if qualities:
        print(f"avg_quality_pct,{sum(qualities) / len(qualities):.1f}", file=stream)
        print(f"min_quality_pct,{min(qualities):.1f}", file=stream)
    if output_path is not None:
        print(f"csv,{output_path}", file=stream)
    if group_output_path is not None:
        print(f"groups_csv,{group_output_path}", file=stream)
    if frames_output_path is not None:
        print(f"frames_csv,{frames_output_path}", file=stream)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", default=str(DEFAULT_LOG_DIR), help="directory containing evidence TXT files")
    parser.add_argument("--output", default=None, help="aggregate CSV path")
    parser.add_argument("--group-output", default=None, help="grouped aggregate CSV path")
    parser.add_argument("--frames-output", default=None, help="accepted frames CSV path")
    parser.add_argument("--group-by", nargs="+", default=DEFAULT_GROUP_BY, help="columns used for grouped summary")
    parser.add_argument("--no-groups", action="store_true", help="do not write grouped aggregate CSV")
    parser.add_argument("--no-frames", action="store_true", help="do not write accepted-frame CSV")
    parser.add_argument(
        "--keep-duplicate-frames",
        action="store_true",
        help="keep accepted frames repeated by later cumulative evidence saves",
    )
    parser.add_argument("--stdout", action="store_true", help="write aggregate CSV to stdout instead of a file")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    summaries = collect_summaries(args.input_dir)

    output_path = None
    group_output_path = None
    frames_output_path = None
    if args.stdout:
        write_summaries(sys.stdout, summaries)
        if args.group_output:
            group_output_path = Path(args.group_output)
            write_grouped_summary_csv(group_output_path, summaries, args.group_by)
        if args.frames_output:
            frames_output_path = Path(args.frames_output)
            write_accepted_frames_csv(
                frames_output_path,
                summaries,
                keep_duplicates=args.keep_duplicate_frames,
            )
    else:
        output_path = Path(args.output) if args.output else Path(args.input_dir) / "field_summary.csv"
        write_summary_csv(output_path, summaries)
        if not args.no_groups:
            group_output_path = Path(args.group_output) if args.group_output else Path(args.input_dir) / "field_summary_groups.csv"
            write_grouped_summary_csv(group_output_path, summaries, args.group_by)
        if not args.no_frames:
            frames_output_path = Path(args.frames_output) if args.frames_output else Path(args.input_dir) / "field_frames.csv"
            write_accepted_frames_csv(
                frames_output_path,
                summaries,
                keep_duplicates=args.keep_duplicate_frames,
            )

    print_report(
        summaries,
        output_path,
        group_output_path,
        frames_output_path,
        stream=sys.stderr if args.stdout else sys.stdout,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
