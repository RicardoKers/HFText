"""Sweep experimental interleaving geometries with bit repetition."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

from hftext.frame import build_transmission
from repetition_sweep import (
    RepetitionAggregateResult,
    aggregate_results,
    print_summary,
    run_repetition_sweep,
    write_summary,
    write_trials,
)
from tx_wav import build_payload


def candidate_interleave_shapes(bit_count: int, min_rows: int = 2, max_rows: int = 16) -> list[tuple[int, int]]:
    """Return rectangular interleaver shapes that exactly fit bit_count."""
    if bit_count <= 0:
        raise ValueError("bit_count must be positive")
    if min_rows <= 0:
        raise ValueError("min_rows must be positive")
    if max_rows < min_rows:
        raise ValueError("max_rows must be greater than or equal to min_rows")

    shapes: list[tuple[int, int]] = []
    for rows in range(min_rows, max_rows + 1):
        if bit_count % rows == 0:
            shapes.append((rows, bit_count // rows))
    return shapes


def parse_rows(values: list[int] | None) -> list[int] | None:
    """Validate optional explicit row counts."""
    if values is None:
        return None
    if any(value <= 0 for value in values):
        raise ValueError("rows must be positive")
    return values


def best_results_by_snr(results: list[RepetitionAggregateResult]) -> list[RepetitionAggregateResult]:
    """Pick the best aggregate result for each SNR level."""
    best: list[RepetitionAggregateResult] = []
    snrs = list(dict.fromkeys(result.snr_db for result in results))
    for snr_db in snrs:
        group = [result for result in results if result.snr_db == snr_db]
        best.append(
            max(
                group,
                key=lambda result: (
                    result.crc_success_rate,
                    result.payload_success_rate,
                    -result.avg_recovered_ber,
                    -result.max_recovered_ber,
                    result.avg_confidence,
                ),
            )
        )
    return best


def write_best_summary(path: str | Path, results: list[RepetitionAggregateResult]) -> None:
    """Write the best aggregate result for each SNR level."""
    with Path(path).open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(
            [
                "snr_db",
                "best_label",
                "factor",
                "duration_multiplier",
                "trials",
                "crc_success_rate",
                "payload_success_rate",
                "avg_recovered_ber",
                "max_recovered_ber",
                "avg_confidence",
                "min_confidence",
            ]
        )
        for result in best_results_by_snr(results):
            writer.writerow(
                [
                    "" if result.snr_db is None else result.snr_db,
                    result.label,
                    result.factor,
                    result.duration_multiplier,
                    result.trials,
                    f"{result.crc_success_rate:.6f}",
                    f"{result.payload_success_rate:.6f}",
                    f"{result.avg_recovered_ber:.6f}",
                    f"{result.max_recovered_ber:.6f}",
                    f"{result.avg_confidence:.6f}",
                    f"{result.min_confidence:.6f}",
                ]
            )


def print_best_summary(results: list[RepetitionAggregateResult]) -> None:
    """Print the best aggregate result for each SNR level."""
    print()
    print(
        "best_by_snr,snr_db,best_label,factor,duration_multiplier,trials,"
        "crc_success_rate,payload_success_rate,avg_recovered_ber,max_recovered_ber,"
        "avg_confidence,min_confidence"
    )
    for result in best_results_by_snr(results):
        snr = "clean" if result.snr_db is None else f"{result.snr_db:g}"
        print(
            f"best_by_snr,{snr},{result.label},{result.factor},{result.duration_multiplier},"
            f"{result.trials},{result.crc_success_rate:.3f},{result.payload_success_rate:.3f},"
            f"{result.avg_recovered_ber:.6f},{result.max_recovered_ber:.6f},"
            f"{result.avg_confidence:.6f},{result.min_confidence:.6f}"
        )


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("message", help="message text to transmit")
    parser.add_argument("--callsign", default=None, help="optional callsign prefix")
    parser.add_argument("--output-dir", default="generated/interleaving_sweep")
    parser.add_argument("--factor", type=int, default=3, help="bit repetition factor")
    parser.add_argument("--snr", type=float, nargs="+", default=[-12.0])
    parser.add_argument("--seed", type=int, default=12345)
    parser.add_argument("--trials", type=int, default=5)
    parser.add_argument("--sample-rate", type=int, default=48_000)
    parser.add_argument("--symbol-duration", type=float, default=0.5)
    parser.add_argument("--f0", type=float, default=1_200.0)
    parser.add_argument("--f1", type=float, default=1_600.0)
    parser.add_argument("--no-clean", action="store_true")
    parser.add_argument("--fading-block-symbols", type=int, default=None)
    parser.add_argument("--fading-min-gain", type=float, default=None)
    parser.add_argument("--fading-max-gain", type=float, default=None)
    parser.add_argument("--min-rows", type=int, default=2)
    parser.add_argument("--max-rows", type=int, default=16)
    parser.add_argument("--rows", type=int, nargs="+", default=None, help="explicit row counts to test")
    parser.add_argument("--no-baseline", action="store_true", help="skip repeated transmission without interleaving")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    rows = parse_rows(args.rows)
    payload = build_payload(args.message, args.callsign)
    repeated_bit_count = len(build_transmission(payload)) * args.factor
    if rows is None:
        shapes = candidate_interleave_shapes(repeated_bit_count, args.min_rows, args.max_rows)
    else:
        shapes = [(row, repeated_bit_count // row) for row in rows if repeated_bit_count % row == 0]

    if not shapes:
        raise ValueError("no interleaving shapes exactly fit the repeated bit count")

    all_results = []
    if not args.no_baseline:
        baseline_results = run_repetition_sweep(
            args.message,
            callsign=args.callsign,
            snrs_db=args.snr,
            output_dir=args.output_dir,
            factors=[args.factor],
            sample_rate=args.sample_rate,
            symbol_duration=args.symbol_duration,
            f0=args.f0,
            f1=args.f1,
            seed=args.seed,
            trials=args.trials,
            include_clean=not args.no_clean,
            fading_block_symbols=args.fading_block_symbols,
            fading_min_gain=args.fading_min_gain,
            fading_max_gain=args.fading_max_gain,
        )
        all_results.extend(baseline_results)

    for shape_index, (shape_rows, shape_columns) in enumerate(shapes):
        results = run_repetition_sweep(
            args.message,
            callsign=args.callsign,
            snrs_db=args.snr,
            output_dir=args.output_dir,
            factors=[args.factor],
            sample_rate=args.sample_rate,
            symbol_duration=args.symbol_duration,
            f0=args.f0,
            f1=args.f1,
            seed=args.seed + (shape_index + 1) * 1_000_000,
            trials=args.trials,
            include_clean=not args.no_clean,
            fading_block_symbols=args.fading_block_symbols,
            fading_min_gain=args.fading_min_gain,
            fading_max_gain=args.fading_max_gain,
            interleave_rows=shape_rows,
            interleave_columns=shape_columns,
        )
        all_results.extend(results)

    output_path = Path(args.output_dir)
    aggregate = aggregate_results(all_results)
    write_trials(output_path / "trials.csv", all_results)
    write_summary(output_path / "summary.csv", aggregate)
    write_best_summary(output_path / "best_summary.csv", aggregate)
    print_summary(all_results)
    print_best_summary(aggregate)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
