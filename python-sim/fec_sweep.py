"""Compare experimental FEC modes under AWGN and optional block fading."""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from hftext.channel import add_awgn, apply_block_fading, bit_error_count, bit_error_rate
from hftext.demodulator import demodulate_bit_decisions_2fsk
from hftext.fec import (
    convolutional_k3_decode_bits,
    convolutional_k3_encode_bits,
    hamming74_decode_bits,
    hamming74_encode_bits,
)
from hftext.frame import FrameResult, build_transmission, parse_frame_from_stream
from hftext.interleaving import deinterleave_bits, interleave_bits
from hftext.modulator import DEFAULT_F0, DEFAULT_F1, DEFAULT_SAMPLE_RATE, DEFAULT_SYMBOL_DURATION, modulate_bits_2fsk
from noise_sweep import mean_confidence, snr_label
from tx_wav import build_payload


@dataclass(frozen=True)
class FecTrialResult:
    label: str
    mode: str
    snr_db: float | None
    trial: int
    seed: int
    duration_multiplier: float
    channel_bit_errors: int
    channel_ber: float
    recovered_bit_errors: int
    recovered_ber: float
    corrected_codewords: int
    invalid_codewords: int
    confidence: float
    fading_block_symbols: int | None
    fading_min_gain: float | None
    fading_max_gain: float | None
    interleave_rows: int | None
    interleave_columns: int | None
    frame_result: FrameResult
    decoder_distance: int = 0


@dataclass(frozen=True)
class FecAggregateResult:
    label: str
    mode: str
    snr_db: float | None
    trials: int
    duration_multiplier: float
    crc_successes: int
    payload_successes: int
    avg_channel_ber: float
    avg_recovered_ber: float
    max_recovered_ber: float
    avg_corrected_codewords: float
    avg_invalid_codewords: float
    avg_confidence: float
    min_confidence: float
    interleave_rows: int | None
    interleave_columns: int | None
    avg_decoder_distance: float = 0.0

    @property
    def crc_success_rate(self) -> float:
        return self.crc_successes / self.trials if self.trials else 0.0

    @property
    def payload_success_rate(self) -> float:
        return self.payload_successes / self.trials if self.trials else 0.0


def _validate_modes(modes: list[str]) -> None:
    valid_modes = {"raw", "hamming74", "conv_k3"}
    for mode in modes:
        if mode not in valid_modes:
            raise ValueError(f"invalid FEC mode: {mode}")


def _encode_for_mode(frame_bits: list[int], mode: str) -> list[int]:
    if mode == "raw":
        return frame_bits
    if mode == "hamming74":
        return hamming74_encode_bits(frame_bits)
    if mode == "conv_k3":
        return convolutional_k3_encode_bits(frame_bits)
    raise ValueError(f"invalid FEC mode: {mode}")


def _decode_for_mode(bits: list[int], mode: str, original_bit_count: int) -> tuple[list[int], int, int, int]:
    if mode == "raw":
        return bits, 0, 0, 0
    if mode == "hamming74":
        decoded, corrected_count, invalid_count = hamming74_decode_bits(bits, original_bit_count)
        return decoded, corrected_count, invalid_count, 0
    if mode == "conv_k3":
        decoded, distance = convolutional_k3_decode_bits(bits, original_bit_count)
        return decoded, 0, 0, distance
    raise ValueError(f"invalid FEC mode: {mode}")


def run_fec_sweep(
    message: str,
    callsign: str | None,
    snrs_db: list[float],
    output_dir: str | Path,
    modes: list[str] | None = None,
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
    seed: int = 12345,
    trials: int = 1,
    include_clean: bool = True,
    fading_block_symbols: int | None = None,
    fading_min_gain: float | None = None,
    fading_max_gain: float | None = None,
    interleave_rows: int | None = None,
    interleave_columns: int | None = None,
) -> list[FecTrialResult]:
    """Run an experimental FEC sweep."""
    if trials <= 0:
        raise ValueError("trials must be positive")
    selected_modes = ["raw", "hamming74", "conv_k3"] if modes is None else modes
    _validate_modes(selected_modes)
    if fading_block_symbols is not None and fading_block_symbols <= 0:
        raise ValueError("fading_block_symbols must be positive")
    if fading_min_gain is not None and fading_min_gain < 0:
        raise ValueError("fading_min_gain must be non-negative")
    if fading_min_gain is not None and fading_max_gain is not None and fading_max_gain < fading_min_gain:
        raise ValueError("fading_max_gain must be greater than or equal to fading_min_gain")
    if (interleave_rows is None) != (interleave_columns is None):
        raise ValueError("interleave_rows and interleave_columns must be set together")
    if interleave_rows is not None and interleave_rows <= 0:
        raise ValueError("interleave_rows must be positive")
    if interleave_columns is not None and interleave_columns <= 0:
        raise ValueError("interleave_columns must be positive")

    payload = build_payload(message, callsign)
    frame_bits = build_transmission(payload)
    samples_per_symbol = int(round(sample_rate * symbol_duration))
    if samples_per_symbol <= 0:
        raise ValueError("symbol duration is too short for sample_rate")

    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    levels: list[float | None] = []
    if include_clean:
        levels.append(None)
    levels.extend(snrs_db)

    results: list[FecTrialResult] = []
    for mode_index, mode in enumerate(selected_modes):
        encoded_bits = _encode_for_mode(frame_bits, mode)
        tx_bits = encoded_bits
        if interleave_rows is not None and interleave_columns is not None:
            block_size = interleave_rows * interleave_columns
            if len(encoded_bits) % block_size:
                raise ValueError("encoded bit count must be a multiple of interleave rows * columns")
            tx_bits = interleave_bits(encoded_bits, interleave_rows, interleave_columns)
        clean_audio = modulate_bits_2fsk(tx_bits, sample_rate, symbol_duration, f0, f1)
        duration_multiplier = len(tx_bits) / len(frame_bits) if frame_bits else 1.0
        for level_index, snr_db in enumerate(levels):
            label = f"{snr_label(snr_db)}_{mode}"
            if interleave_rows is not None and interleave_columns is not None:
                label += f"_int{interleave_rows}x{interleave_columns}"
            if fading_block_symbols is not None:
                label += "_fade"
            level_trials = 1 if snr_db is None else trials
            for trial in range(level_trials):
                trial_seed = seed + mode_index * 100_000 + level_index * trials + trial
                rng = np.random.default_rng(trial_seed)
                audio = clean_audio
                if fading_block_symbols is not None:
                    min_gain = 0.0 if fading_min_gain is None else fading_min_gain
                    max_gain = 1.0 if fading_max_gain is None else fading_max_gain
                    audio = apply_block_fading(
                        audio,
                        block_size=samples_per_symbol * fading_block_symbols,
                        min_gain=min_gain,
                        max_gain=max_gain,
                        rng=rng,
                    )
                if snr_db is not None:
                    audio = add_awgn(audio, snr_db, rng=rng)

                decisions = demodulate_bit_decisions_2fsk(audio, sample_rate, symbol_duration, f0, f1)
                rx_bits = [decision.bit for decision in decisions]
                encoded_rx_bits = rx_bits
                if interleave_rows is not None and interleave_columns is not None:
                    encoded_rx_bits = deinterleave_bits(rx_bits, interleave_rows, interleave_columns)
                recovered_bits, corrected_count, invalid_count, decoder_distance = _decode_for_mode(
                    encoded_rx_bits, mode, len(frame_bits)
                )
                frame_result = parse_frame_from_stream(recovered_bits)

                results.append(
                    FecTrialResult(
                        label=label,
                        mode=mode,
                        snr_db=snr_db,
                        trial=trial + 1,
                        seed=trial_seed,
                        duration_multiplier=duration_multiplier,
                        channel_bit_errors=bit_error_count(tx_bits, rx_bits),
                        channel_ber=bit_error_rate(tx_bits, rx_bits),
                        recovered_bit_errors=bit_error_count(frame_bits, recovered_bits),
                        recovered_ber=bit_error_rate(frame_bits, recovered_bits),
                        corrected_codewords=corrected_count,
                        invalid_codewords=invalid_count,
                        confidence=mean_confidence(decisions),
                        fading_block_symbols=fading_block_symbols,
                        fading_min_gain=fading_min_gain,
                        fading_max_gain=fading_max_gain,
                        interleave_rows=interleave_rows,
                        interleave_columns=interleave_columns,
                        frame_result=frame_result,
                        decoder_distance=decoder_distance,
                    )
                )

    write_trials(output_path / "trials.csv", results)
    write_summary(output_path / "summary.csv", aggregate_results(results))
    return results


def aggregate_results(results: list[FecTrialResult]) -> list[FecAggregateResult]:
    """Aggregate FEC trials by label."""
    aggregates: list[FecAggregateResult] = []
    labels = list(dict.fromkeys(result.label for result in results))
    for label in labels:
        group = [result for result in results if result.label == label]
        channel_bers = [result.channel_ber for result in group]
        recovered_bers = [result.recovered_ber for result in group]
        corrected_counts = [result.corrected_codewords for result in group]
        invalid_counts = [result.invalid_codewords for result in group]
        decoder_distances = [result.decoder_distance for result in group]
        confidences = [result.confidence for result in group]
        aggregates.append(
            FecAggregateResult(
                label=label,
                mode=group[0].mode,
                snr_db=group[0].snr_db,
                trials=len(group),
                duration_multiplier=group[0].duration_multiplier,
                crc_successes=sum(1 for result in group if result.frame_result.crc_ok),
                payload_successes=sum(1 for result in group if result.frame_result.payload_valid),
                avg_channel_ber=sum(channel_bers) / len(channel_bers),
                avg_recovered_ber=sum(recovered_bers) / len(recovered_bers),
                max_recovered_ber=max(recovered_bers),
                avg_corrected_codewords=sum(corrected_counts) / len(corrected_counts),
                avg_invalid_codewords=sum(invalid_counts) / len(invalid_counts),
                avg_confidence=sum(confidences) / len(confidences),
                min_confidence=min(confidences),
                interleave_rows=group[0].interleave_rows,
                interleave_columns=group[0].interleave_columns,
                avg_decoder_distance=sum(decoder_distances) / len(decoder_distances),
            )
        )
    return aggregates


def write_trials(path: str | Path, results: list[FecTrialResult]) -> None:
    """Write per-trial FEC results."""
    with Path(path).open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(
            [
                "label",
                "mode",
                "snr_db",
                "duration_multiplier",
                "trial",
                "seed",
                "channel_bit_errors",
                "channel_ber",
                "recovered_bit_errors",
                "recovered_ber",
                "corrected_codewords",
                "invalid_codewords",
                "decoder_distance",
                "confidence",
                "fading_block_symbols",
                "fading_min_gain",
                "fading_max_gain",
                "interleave_rows",
                "interleave_columns",
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
                    result.mode,
                    "" if result.snr_db is None else result.snr_db,
                    f"{result.duration_multiplier:.6f}",
                    result.trial,
                    result.seed,
                    result.channel_bit_errors,
                    f"{result.channel_ber:.6f}",
                    result.recovered_bit_errors,
                    f"{result.recovered_ber:.6f}",
                    result.corrected_codewords,
                    result.invalid_codewords,
                    result.decoder_distance,
                    f"{result.confidence:.6f}",
                    "" if result.fading_block_symbols is None else result.fading_block_symbols,
                    "" if result.fading_min_gain is None else result.fading_min_gain,
                    "" if result.fading_max_gain is None else result.fading_max_gain,
                    "" if result.interleave_rows is None else result.interleave_rows,
                    "" if result.interleave_columns is None else result.interleave_columns,
                    result.frame_result.crc_ok,
                    result.frame_result.payload_valid,
                    result.frame_result.text,
                    result.frame_result.error or "",
                ]
            )


def write_summary(path: str | Path, results: list[FecAggregateResult]) -> None:
    """Write aggregate FEC results."""
    with Path(path).open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(
            [
                "label",
                "mode",
                "snr_db",
                "duration_multiplier",
                "trials",
                "crc_success_rate",
                "payload_success_rate",
                "avg_channel_ber",
                "avg_recovered_ber",
                "max_recovered_ber",
                "avg_corrected_codewords",
                "avg_invalid_codewords",
                "avg_decoder_distance",
                "avg_confidence",
                "min_confidence",
                "interleave_rows",
                "interleave_columns",
            ]
        )
        for result in results:
            writer.writerow(
                [
                    result.label,
                    result.mode,
                    "" if result.snr_db is None else result.snr_db,
                    f"{result.duration_multiplier:.6f}",
                    result.trials,
                    f"{result.crc_success_rate:.6f}",
                    f"{result.payload_success_rate:.6f}",
                    f"{result.avg_channel_ber:.6f}",
                    f"{result.avg_recovered_ber:.6f}",
                    f"{result.max_recovered_ber:.6f}",
                    f"{result.avg_corrected_codewords:.6f}",
                    f"{result.avg_invalid_codewords:.6f}",
                    f"{result.avg_decoder_distance:.6f}",
                    f"{result.avg_confidence:.6f}",
                    f"{result.min_confidence:.6f}",
                    "" if result.interleave_rows is None else result.interleave_rows,
                    "" if result.interleave_columns is None else result.interleave_columns,
                ]
            )


def print_summary(results: list[FecTrialResult]) -> None:
    """Print aggregate FEC comparison."""
    print(
        "label,mode,snr_db,duration_multiplier,trials,crc_success_rate,"
        "payload_success_rate,avg_channel_ber,avg_recovered_ber,max_recovered_ber,"
        "avg_corrected_codewords,avg_invalid_codewords,avg_decoder_distance,avg_confidence,min_confidence"
    )
    for result in aggregate_results(results):
        snr = "clean" if result.snr_db is None else f"{result.snr_db:g}"
        print(
            f"{result.label},{result.mode},{snr},{result.duration_multiplier:.3f},"
            f"{result.trials},{result.crc_success_rate:.3f},{result.payload_success_rate:.3f},"
            f"{result.avg_channel_ber:.6f},{result.avg_recovered_ber:.6f},"
            f"{result.max_recovered_ber:.6f},{result.avg_corrected_codewords:.3f},"
            f"{result.avg_invalid_codewords:.3f},{result.avg_decoder_distance:.3f},"
            f"{result.avg_confidence:.6f},"
            f"{result.min_confidence:.6f}"
        )


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("message", help="message text to transmit")
    parser.add_argument("--callsign", default=None, help="optional callsign prefix")
    parser.add_argument("--output-dir", default="generated/fec_sweep")
    parser.add_argument("--mode", nargs="+", default=["raw", "hamming74", "conv_k3"], help="FEC modes to compare")
    parser.add_argument("--snr", type=float, nargs="+", default=[6.0, 0.0, -6.0, -12.0])
    parser.add_argument("--seed", type=int, default=12345)
    parser.add_argument("--trials", type=int, default=5)
    parser.add_argument("--sample-rate", type=int, default=DEFAULT_SAMPLE_RATE)
    parser.add_argument("--symbol-duration", type=float, default=DEFAULT_SYMBOL_DURATION)
    parser.add_argument("--f0", type=float, default=DEFAULT_F0)
    parser.add_argument("--f1", type=float, default=DEFAULT_F1)
    parser.add_argument("--no-clean", action="store_true")
    parser.add_argument("--fading-block-symbols", type=int, default=None)
    parser.add_argument("--fading-min-gain", type=float, default=None)
    parser.add_argument("--fading-max-gain", type=float, default=None)
    parser.add_argument("--interleave-rows", type=int, default=None)
    parser.add_argument("--interleave-columns", type=int, default=None)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    results = run_fec_sweep(
        args.message,
        callsign=args.callsign,
        snrs_db=args.snr,
        output_dir=args.output_dir,
        modes=args.mode,
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
        interleave_rows=args.interleave_rows,
        interleave_columns=args.interleave_columns,
    )
    print_summary(results)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
