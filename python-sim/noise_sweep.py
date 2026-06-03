"""Run an HFText AWGN SNR sweep and save WAV examples."""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from hftext.channel import add_awgn, bit_error_count, bit_error_rate
from hftext.demodulator import BitDecision, demodulate_bit_decisions_2fsk
from hftext.frame import FrameResult, build_transmission, parse_frame_from_stream
from hftext.modulator import (
    DEFAULT_F0,
    DEFAULT_F1,
    DEFAULT_SAMPLE_RATE,
    DEFAULT_SYMBOL_DURATION,
    modulate_bits_2fsk,
    save_wav,
)
from tx_wav import build_payload


@dataclass(frozen=True)
class SweepResult:
    label: str
    snr_db: float | None
    trial: int
    seed: int
    wav_path: Path
    bit_errors: int
    ber: float
    confidence: float
    frame_result: FrameResult


@dataclass(frozen=True)
class AggregateResult:
    label: str
    snr_db: float | None
    trials: int
    crc_successes: int
    payload_successes: int
    avg_ber: float
    max_ber: float
    avg_confidence: float
    min_confidence: float
    min_bit_errors: int
    max_bit_errors: int

    @property
    def crc_success_rate(self) -> float:
        return self.crc_successes / self.trials if self.trials else 0.0

    @property
    def payload_success_rate(self) -> float:
        return self.payload_successes / self.trials if self.trials else 0.0


def snr_label(snr_db: float | None) -> str:
    """Return a filesystem-friendly SNR label."""
    if snr_db is None:
        return "clean"
    sign = "p" if snr_db >= 0 else "m"
    value = str(abs(snr_db)).replace(".", "p")
    return f"snr_{sign}{value}db"


def mean_confidence(decisions: list[BitDecision]) -> float:
    """Return the average symbol confidence for a demodulated trial."""
    if not decisions:
        return 0.0
    return sum(decision.confidence for decision in decisions) / len(decisions)


def run_sweep(
    message: str,
    callsign: str | None,
    snrs_db: list[float],
    output_dir: str | Path,
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
    seed: int = 12345,
    trials: int = 1,
    include_clean: bool = True,
    save_wavs: bool = True,
) -> list[SweepResult]:
    """Generate clean/noisy WAVs and return demodulation results."""
    if trials <= 0:
        raise ValueError("trials must be positive")

    payload = build_payload(message, callsign)
    bits = build_transmission(payload)
    clean_audio = modulate_bits_2fsk(bits, sample_rate, symbol_duration, f0, f1)

    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    levels: list[float | None] = []
    if include_clean:
        levels.append(None)
    levels.extend(snrs_db)

    results = []
    for level_index, snr_db in enumerate(levels):
        label = snr_label(snr_db)
        level_trials = 1 if snr_db is None else trials
        for trial in range(level_trials):
            trial_seed = seed + level_index * trials + trial
            rng = np.random.default_rng(trial_seed)
            audio = clean_audio if snr_db is None else add_awgn(clean_audio, snr_db, rng=rng)
            wav_path = output_path / f"{label}.wav"
            if level_trials > 1 and trial != 0:
                wav_path = output_path / f"{label}_trial_{trial + 1:03d}.wav"
            if save_wavs and (trial == 0 or snr_db is None):
                save_wav(wav_path, audio, sample_rate)

            decisions = demodulate_bit_decisions_2fsk(audio, sample_rate, symbol_duration, f0, f1)
            decoded_bits = [decision.bit for decision in decisions]
            frame_result = parse_frame_from_stream(decoded_bits)
            results.append(
                SweepResult(
                    label=label,
                    snr_db=snr_db,
                    trial=trial + 1,
                    seed=trial_seed,
                    wav_path=wav_path if save_wavs and (trial == 0 or snr_db is None) else Path(""),
                    bit_errors=bit_error_count(bits, decoded_bits),
                    ber=bit_error_rate(bits, decoded_bits),
                    confidence=mean_confidence(decisions),
                    frame_result=frame_result,
                )
            )

    write_trials(output_path / "trials.csv", results)
    write_summary(output_path / "summary.csv", aggregate_results(results))
    return results


def aggregate_results(results: list[SweepResult]) -> list[AggregateResult]:
    """Aggregate trial results by SNR label."""
    aggregates = []
    labels = list(dict.fromkeys(result.label for result in results))
    for label in labels:
        group = [result for result in results if result.label == label]
        bit_errors = [result.bit_errors for result in group]
        bers = [result.ber for result in group]
        confidences = [result.confidence for result in group]
        aggregates.append(
            AggregateResult(
                label=label,
                snr_db=group[0].snr_db,
                trials=len(group),
                crc_successes=sum(1 for result in group if result.frame_result.crc_ok),
                payload_successes=sum(1 for result in group if result.frame_result.payload_valid),
                avg_ber=sum(bers) / len(bers),
                max_ber=max(bers),
                avg_confidence=sum(confidences) / len(confidences),
                min_confidence=min(confidences),
                min_bit_errors=min(bit_errors),
                max_bit_errors=max(bit_errors),
            )
        )
    return aggregates


def write_trials(path: str | Path, results: list[SweepResult]) -> None:
    """Write per-trial sweep results to CSV."""
    with Path(path).open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(
            [
                "label",
                "snr_db",
                "trial",
                "seed",
                "wav_path",
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
                    result.label,
                    "" if result.snr_db is None else result.snr_db,
                    result.trial,
                    result.seed,
                    result.wav_path,
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


def write_summary(path: str | Path, results: list[AggregateResult]) -> None:
    """Write aggregate sweep results to CSV."""
    with Path(path).open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(
            [
                "label",
                "snr_db",
                "trials",
                "crc_successes",
                "crc_success_rate",
                "payload_successes",
                "payload_success_rate",
                "avg_ber",
                "max_ber",
                "avg_confidence",
                "min_confidence",
                "min_bit_errors",
                "max_bit_errors",
            ]
        )
        for result in results:
            writer.writerow(
                [
                    result.label,
                    "" if result.snr_db is None else result.snr_db,
                    result.trials,
                    result.crc_successes,
                    f"{result.crc_success_rate:.6f}",
                    result.payload_successes,
                    f"{result.payload_success_rate:.6f}",
                    f"{result.avg_ber:.6f}",
                    f"{result.max_ber:.6f}",
                    f"{result.avg_confidence:.6f}",
                    f"{result.min_confidence:.6f}",
                    result.min_bit_errors,
                    result.max_bit_errors,
                ]
            )


def print_summary(results: list[SweepResult]) -> None:
    """Print a compact human-readable sweep table."""
    print("label,snr_db,trials,crc_success_rate,payload_success_rate,avg_ber,max_ber,avg_confidence,min_confidence,min_errors,max_errors")
    for result in aggregate_results(results):
        snr = "clean" if result.snr_db is None else f"{result.snr_db:g}"
        print(
            f"{result.label},{snr},{result.trials},"
            f"{result.crc_success_rate:.3f},{result.payload_success_rate:.3f},"
            f"{result.avg_ber:.6f},{result.max_ber:.6f},"
            f"{result.avg_confidence:.6f},{result.min_confidence:.6f},"
            f"{result.min_bit_errors},{result.max_bit_errors}"
        )


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("message", help="message text to transmit")
    parser.add_argument("--callsign", default=None, help="optional callsign prefix")
    parser.add_argument("--output-dir", default="generated/noise_sweep")
    parser.add_argument("--snr", type=float, nargs="+", default=[12.0, 6.0, 0.0, -6.0, -12.0])
    parser.add_argument("--seed", type=int, default=12345)
    parser.add_argument("--trials", type=int, default=1)
    parser.add_argument("--sample-rate", type=int, default=DEFAULT_SAMPLE_RATE)
    parser.add_argument("--symbol-duration", type=float, default=DEFAULT_SYMBOL_DURATION)
    parser.add_argument("--f0", type=float, default=DEFAULT_F0)
    parser.add_argument("--f1", type=float, default=DEFAULT_F1)
    parser.add_argument("--no-clean", action="store_true")
    parser.add_argument("--no-wavs", action="store_true", help="do not save WAV examples")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    results = run_sweep(
        args.message,
        callsign=args.callsign,
        snrs_db=args.snr,
        output_dir=args.output_dir,
        sample_rate=args.sample_rate,
        symbol_duration=args.symbol_duration,
        f0=args.f0,
        f1=args.f1,
        seed=args.seed,
        trials=args.trials,
        include_clean=not args.no_clean,
        save_wavs=not args.no_wavs,
    )
    print_summary(results)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
