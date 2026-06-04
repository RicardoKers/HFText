"""Sweep experimental interleaving geometries for one FEC mode."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

from fec_sweep import (
    FecAggregateResult,
    _encode_for_mode,
    aggregate_results,
    print_summary,
    run_fec_sweep,
    write_summary,
    write_trials,
)
from hftext.frame import build_transmission
from hftext.interleaving import choose_interleave_shape
from interleaving_sweep import candidate_interleave_shapes, parse_rows
from tx_wav import build_payload


def best_results_by_snr(results: list[FecAggregateResult]) -> list[FecAggregateResult]:
    """Pick the best aggregate FEC result for each SNR level."""
    best: list[FecAggregateResult] = []
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


def write_best_summary(path: str | Path, results: list[FecAggregateResult]) -> None:
    """Write the best FEC result for each SNR level."""
    with Path(path).open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(
            [
                "snr_db",
                "best_label",
                "mode",
                "duration_multiplier",
                "trials",
                "crc_success_rate",
                "payload_success_rate",
                "avg_recovered_ber",
                "max_recovered_ber",
                "avg_corrected_codewords",
                "avg_decoder_distance",
                "avg_confidence",
                "min_confidence",
                "interleave_rows",
                "interleave_columns",
            ]
        )
        for result in best_results_by_snr(results):
            writer.writerow(
                [
                    "" if result.snr_db is None else result.snr_db,
                    result.label,
                    result.mode,
                    f"{result.duration_multiplier:.6f}",
                    result.trials,
                    f"{result.crc_success_rate:.6f}",
                    f"{result.payload_success_rate:.6f}",
                    f"{result.avg_recovered_ber:.6f}",
                    f"{result.max_recovered_ber:.6f}",
                    f"{result.avg_corrected_codewords:.6f}",
                    f"{result.avg_decoder_distance:.6f}",
                    f"{result.avg_confidence:.6f}",
                    f"{result.min_confidence:.6f}",
                    "" if result.interleave_rows is None else result.interleave_rows,
                    "" if result.interleave_columns is None else result.interleave_columns,
                ]
            )


def print_best_summary(results: list[FecAggregateResult]) -> None:
    """Print the best FEC result for each SNR level."""
    print()
    print(
        "best_by_snr,snr_db,best_label,mode,duration_multiplier,trials,"
        "crc_success_rate,payload_success_rate,avg_recovered_ber,max_recovered_ber,"
        "avg_corrected_codewords,avg_decoder_distance,avg_confidence,min_confidence,"
        "interleave_rows,interleave_columns"
    )
    for result in best_results_by_snr(results):
        snr = "clean" if result.snr_db is None else f"{result.snr_db:g}"
        rows = "" if result.interleave_rows is None else result.interleave_rows
        columns = "" if result.interleave_columns is None else result.interleave_columns
        print(
            f"best_by_snr,{snr},{result.label},{result.mode},{result.duration_multiplier:.3f},"
            f"{result.trials},{result.crc_success_rate:.3f},{result.payload_success_rate:.3f},"
            f"{result.avg_recovered_ber:.6f},{result.max_recovered_ber:.6f},"
            f"{result.avg_corrected_codewords:.3f},{result.avg_decoder_distance:.3f},"
            f"{result.avg_confidence:.6f},{result.min_confidence:.6f},{rows},{columns}"
        )


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("message", help="message text to transmit")
    parser.add_argument("--callsign", default=None, help="optional callsign prefix")
    parser.add_argument("--output-dir", default="generated/fec_interleaving_sweep")
    parser.add_argument("--mode", default="conv_k3", choices=["hamming74", "conv_k3"], help="FEC mode to test")
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
    parser.add_argument("--auto-shape", action="store_true", help="test only the deterministic recommended shape")
    parser.add_argument("--preferred-rows", type=int, default=6)
    parser.add_argument("--no-baseline", action="store_true", help="skip selected FEC mode without interleaving")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    rows = parse_rows(args.rows)
    payload = build_payload(args.message, args.callsign)
    encoded_bit_count = len(_encode_for_mode(build_transmission(payload), args.mode))
    if args.auto_shape:
        shapes = [
            choose_interleave_shape(
                encoded_bit_count,
                preferred_rows=args.preferred_rows,
                min_rows=args.min_rows,
                max_rows=args.max_rows,
            )
        ]
    elif rows is None:
        shapes = candidate_interleave_shapes(encoded_bit_count, args.min_rows, args.max_rows)
    else:
        shapes = [(row, encoded_bit_count // row) for row in rows if encoded_bit_count % row == 0]

    if not shapes:
        raise ValueError("no interleaving shapes exactly fit the encoded bit count")

    all_results = []
    if not args.no_baseline:
        baseline_results = run_fec_sweep(
            args.message,
            callsign=args.callsign,
            snrs_db=args.snr,
            output_dir=args.output_dir,
            modes=[args.mode],
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
        results = run_fec_sweep(
            args.message,
            callsign=args.callsign,
            snrs_db=args.snr,
            output_dir=args.output_dir,
            modes=[args.mode],
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
