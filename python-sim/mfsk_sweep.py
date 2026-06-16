"""Compare experimental HFText 2-FSK, 4-FSK and 8-FSK physical modulation under AWGN."""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from hftext.channel import add_awgn, bit_error_count, bit_error_rate
from hftext.demodulator import BitDecision, demodulate_bit_decisions_fsk
from hftext.frame import FrameResult, build_transmission, parse_frame_from_stream
from hftext.modulator import (
    DEFAULT_AMPLITUDE,
    DEFAULT_SAMPLE_RATE,
    DEFAULT_SYMBOL_DURATION,
    fsk_tones,
    modulate_bits_fsk,
)
from tx_wav import build_payload


@dataclass(frozen=True)
class MfskTrialResult:
    mode: str
    bits_per_symbol: int
    snr_db: float | None
    trial: int
    seed: int
    duration_s: float
    bit_errors: int
    ber: float
    confidence: float
    frame_result: FrameResult


@dataclass(frozen=True)
class MfskAggregateResult:
    mode: str
    bits_per_symbol: int
    snr_db: float | None
    trials: int
    duration_s: float
    crc_successes: int
    payload_successes: int
    avg_ber: float
    max_ber: float
    avg_confidence: float
    min_confidence: float

    @property
    def crc_success_rate(self) -> float:
        return self.crc_successes / self.trials if self.trials else 0.0

    @property
    def payload_success_rate(self) -> float:
        return self.payload_successes / self.trials if self.trials else 0.0


def mean_confidence(decisions: list[BitDecision]) -> float:
    if not decisions:
        return 0.0
    return sum(decision.confidence for decision in decisions) / len(decisions)


def mode_label(bits_per_symbol: int) -> str:
    if bits_per_symbol == 1:
        return "2fsk-v0.1"
    if bits_per_symbol == 2:
        return "4fsk-v0.2-exp"
    if bits_per_symbol == 3:
        return "8fsk-v0.3-exp"
    raise ValueError("bits_per_symbol must be 1, 2 or 3")


def snr_label(snr_db: float | None) -> str:
    if snr_db is None:
        return "clean"
    return f"{snr_db:g}"


def run_mfsk_sweep(
    message: str,
    callsign: str | None,
    output_dir: str | Path,
    snrs_db: list[float],
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0_2fsk: float = 1200.0,
    f1_2fsk: float = 1600.0,
    f0_4fsk: float = 1000.0,
    f1_4fsk: float = 1200.0,
    f0_8fsk: float = 1000.0,
    f1_8fsk: float = 1200.0,
    amplitude: float = DEFAULT_AMPLITUDE,
    seed: int = 12345,
    trials: int = 10,
    include_clean: bool = True,
) -> list[MfskTrialResult]:
    """Run 2-FSK, 4-FSK and 8-FSK AWGN trials and write CSV files."""
    if trials <= 0:
        raise ValueError("trials must be positive")

    payload = build_payload(message, callsign)
    bits = build_transmission(payload)
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    levels: list[float | None] = []
    if include_clean:
        levels.append(None)
    levels.extend(snrs_db)

    configs = [
        (1, f0_2fsk, f1_2fsk),
        (2, f0_4fsk, f1_4fsk),
        (3, f0_8fsk, f1_8fsk),
    ]

    results: list[MfskTrialResult] = []
    for config_index, (bits_per_symbol, f0, f1) in enumerate(configs):
        tones = fsk_tones(f0, f1, 1 << bits_per_symbol)
        if max(tones) >= sample_rate / 2:
            raise ValueError(f"{mode_label(bits_per_symbol)} tone above Nyquist")
        clean_audio = modulate_bits_fsk(
            bits,
            sample_rate=sample_rate,
            symbol_duration=symbol_duration,
            f0=f0,
            f1=f1,
            amplitude=amplitude,
            bits_per_symbol=bits_per_symbol,
        )
        duration_s = len(clean_audio) / sample_rate

        for level_index, snr_db in enumerate(levels):
            level_trials = 1 if snr_db is None else trials
            for trial in range(level_trials):
                trial_seed = seed + config_index * 100_000 + level_index * trials + trial
                rng = np.random.default_rng(trial_seed)
                audio = clean_audio if snr_db is None else add_awgn(clean_audio, snr_db, rng=rng)
                decisions = demodulate_bit_decisions_fsk(
                    audio,
                    sample_rate=sample_rate,
                    symbol_duration=symbol_duration,
                    f0=f0,
                    f1=f1,
                    bits_per_symbol=bits_per_symbol,
                )
                decoded_bits = [decision.bit for decision in decisions][: len(bits)]
                frame_result = parse_frame_from_stream(decoded_bits)
                results.append(
                    MfskTrialResult(
                        mode=mode_label(bits_per_symbol),
                        bits_per_symbol=bits_per_symbol,
                        snr_db=snr_db,
                        trial=trial + 1,
                        seed=trial_seed,
                        duration_s=duration_s,
                        bit_errors=bit_error_count(bits, decoded_bits),
                        ber=bit_error_rate(bits, decoded_bits),
                        confidence=mean_confidence(decisions),
                        frame_result=frame_result,
                    )
                )

    write_trials(output_path / "trials.csv", results)
    write_summary(output_path / "summary.csv", aggregate_results(results))
    return results


def aggregate_results(results: list[MfskTrialResult]) -> list[MfskAggregateResult]:
    aggregates: list[MfskAggregateResult] = []
    keys = list(dict.fromkeys((result.mode, result.snr_db) for result in results))
    for mode, snr_db in keys:
        group = [result for result in results if result.mode == mode and result.snr_db == snr_db]
        bers = [result.ber for result in group]
        confidences = [result.confidence for result in group]
        aggregates.append(
            MfskAggregateResult(
                mode=mode,
                bits_per_symbol=group[0].bits_per_symbol,
                snr_db=snr_db,
                trials=len(group),
                duration_s=group[0].duration_s,
                crc_successes=sum(1 for result in group if result.frame_result.crc_ok),
                payload_successes=sum(1 for result in group if result.frame_result.payload_valid),
                avg_ber=sum(bers) / len(bers),
                max_ber=max(bers),
                avg_confidence=sum(confidences) / len(confidences),
                min_confidence=min(confidences),
            )
        )
    return aggregates


def write_trials(path: str | Path, results: list[MfskTrialResult]) -> None:
    with Path(path).open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(
            [
                "mode",
                "bits_per_symbol",
                "snr_db",
                "trial",
                "seed",
                "duration_s",
                "bit_errors",
                "ber",
                "confidence",
                "frame_detected",
                "crc_ok",
                "payload_valid",
                "text",
                "error",
            ]
        )
        for result in results:
            writer.writerow(
                [
                    result.mode,
                    result.bits_per_symbol,
                    "" if result.snr_db is None else result.snr_db,
                    result.trial,
                    result.seed,
                    f"{result.duration_s:.6f}",
                    result.bit_errors,
                    f"{result.ber:.6f}",
                    f"{result.confidence:.6f}",
                    result.frame_result.frame_detected,
                    result.frame_result.crc_ok,
                    result.frame_result.payload_valid,
                    result.frame_result.text,
                    result.frame_result.error or "",
                ]
            )


def write_summary(path: str | Path, results: list[MfskAggregateResult]) -> None:
    with Path(path).open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(
            [
                "mode",
                "bits_per_symbol",
                "snr_db",
                "trials",
                "duration_s",
                "crc_success_rate",
                "payload_success_rate",
                "avg_ber",
                "max_ber",
                "avg_confidence",
                "min_confidence",
            ]
        )
        for result in results:
            writer.writerow(
                [
                    result.mode,
                    result.bits_per_symbol,
                    "" if result.snr_db is None else result.snr_db,
                    result.trials,
                    f"{result.duration_s:.6f}",
                    f"{result.crc_success_rate:.6f}",
                    f"{result.payload_success_rate:.6f}",
                    f"{result.avg_ber:.6f}",
                    f"{result.max_ber:.6f}",
                    f"{result.avg_confidence:.6f}",
                    f"{result.min_confidence:.6f}",
                ]
            )


def print_summary(results: list[MfskTrialResult]) -> None:
    print("mode,snr_db,trials,duration_s,crc_success_rate,payload_success_rate,avg_ber,max_ber,avg_confidence,min_confidence")
    for result in aggregate_results(results):
        print(
            f"{result.mode},{snr_label(result.snr_db)},{result.trials},"
            f"{result.duration_s:.3f},{result.crc_success_rate:.3f},"
            f"{result.payload_success_rate:.3f},{result.avg_ber:.6f},"
            f"{result.max_ber:.6f},{result.avg_confidence:.6f},{result.min_confidence:.6f}"
        )


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("message", help="message text to transmit")
    parser.add_argument("--callsign", default=None, help="optional callsign prefix")
    parser.add_argument("--output-dir", default="generated/mfsk_sweep")
    parser.add_argument("--snr", type=float, nargs="+", default=[18.0, 12.0, 6.0, 0.0, -6.0])
    parser.add_argument("--seed", type=int, default=12345)
    parser.add_argument("--trials", type=int, default=10)
    parser.add_argument("--sample-rate", type=int, default=DEFAULT_SAMPLE_RATE)
    parser.add_argument("--symbol-duration", type=float, default=DEFAULT_SYMBOL_DURATION)
    parser.add_argument("--f0-2fsk", type=float, default=1200.0)
    parser.add_argument("--f1-2fsk", type=float, default=1600.0)
    parser.add_argument("--f0-4fsk", type=float, default=1000.0)
    parser.add_argument("--f1-4fsk", type=float, default=1200.0)
    parser.add_argument("--f0-8fsk", type=float, default=1000.0)
    parser.add_argument("--f1-8fsk", type=float, default=1200.0)
    parser.add_argument("--amplitude", type=float, default=DEFAULT_AMPLITUDE)
    parser.add_argument("--no-clean", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    results = run_mfsk_sweep(
        args.message,
        callsign=args.callsign,
        output_dir=args.output_dir,
        snrs_db=args.snr,
        sample_rate=args.sample_rate,
        symbol_duration=args.symbol_duration,
        f0_2fsk=args.f0_2fsk,
        f1_2fsk=args.f1_2fsk,
        f0_4fsk=args.f0_4fsk,
        f1_4fsk=args.f1_4fsk,
        f0_8fsk=args.f0_8fsk,
        f1_8fsk=args.f1_8fsk,
        amplitude=args.amplitude,
        seed=args.seed,
        trials=args.trials,
        include_clean=not args.no_clean,
    )
    print_summary(results)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
