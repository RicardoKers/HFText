"""Run named HFText channel impairment scenarios."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

import numpy as np

from hftext.channel import (
    add_awgn,
    add_dc_offset,
    apply_block_fading,
    apply_frequency_offset,
    attenuate,
    bit_error_count,
    bit_error_rate,
    clip,
)
from hftext.demodulator import demodulate_bits_2fsk
from hftext.frame import build_transmission, parse_frame_from_stream
from hftext.modulator import (
    DEFAULT_F0,
    DEFAULT_F1,
    DEFAULT_SAMPLE_RATE,
    DEFAULT_SYMBOL_DURATION,
    modulate_bits_2fsk,
    save_wav,
)
from noise_sweep import SweepResult, aggregate_results, write_summary, write_trials
from tx_wav import build_payload


@dataclass(frozen=True)
class ChannelScenario:
    label: str
    awgn_snr_db: float | None = None
    gain: float | None = None
    dc_offset: float | None = None
    clip_limit: float | None = None
    frequency_offset_hz: float | None = None
    fading_block_symbols: int | None = None
    fading_min_gain: float | None = None
    fading_max_gain: float | None = None


DEFAULT_SCENARIOS = [
    ChannelScenario("clean"),
    ChannelScenario("awgn_6db", awgn_snr_db=6.0),
    ChannelScenario("attenuated", gain=0.25),
    ChannelScenario("dc_offset", dc_offset=0.15),
    ChannelScenario("mild_clipping", clip_limit=0.45),
    ChannelScenario("freq_offset_20hz", frequency_offset_hz=20.0),
    ChannelScenario("fading_light", fading_block_symbols=4, fading_min_gain=0.25, fading_max_gain=1.0),
    ChannelScenario(
        "combined_moderate",
        awgn_snr_db=6.0,
        gain=0.6,
        dc_offset=0.05,
        clip_limit=0.8,
        frequency_offset_hz=10.0,
        fading_block_symbols=8,
        fading_min_gain=0.5,
        fading_max_gain=1.0,
    ),
]


def apply_scenario(
    samples: np.ndarray,
    scenario: ChannelScenario,
    sample_rate: int,
    samples_per_symbol: int,
    rng: np.random.Generator,
) -> np.ndarray:
    """Apply one channel scenario to samples."""
    audio = np.asarray(samples, dtype=np.float32)
    if scenario.gain is not None:
        audio = attenuate(audio, scenario.gain)
    if scenario.dc_offset is not None:
        audio = add_dc_offset(audio, scenario.dc_offset)
    if scenario.frequency_offset_hz is not None:
        audio = apply_frequency_offset(audio, sample_rate, scenario.frequency_offset_hz)
    if scenario.fading_block_symbols is not None:
        min_gain = 0.0 if scenario.fading_min_gain is None else scenario.fading_min_gain
        max_gain = 1.0 if scenario.fading_max_gain is None else scenario.fading_max_gain
        audio = apply_block_fading(
            audio,
            block_size=samples_per_symbol * scenario.fading_block_symbols,
            min_gain=min_gain,
            max_gain=max_gain,
            rng=rng,
        )
    if scenario.awgn_snr_db is not None:
        audio = add_awgn(audio, scenario.awgn_snr_db, rng=rng)
    if scenario.clip_limit is not None:
        audio = clip(audio, scenario.clip_limit)
    return audio.astype(np.float32)


def run_channel_sweep(
    message: str,
    callsign: str | None,
    output_dir: str | Path,
    scenarios: list[ChannelScenario] | None = None,
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
    seed: int = 12345,
    trials: int = 1,
    save_wavs: bool = False,
) -> list[SweepResult]:
    """Run named channel scenarios and return per-trial results."""
    if trials <= 0:
        raise ValueError("trials must be positive")

    selected = DEFAULT_SCENARIOS if scenarios is None else scenarios
    samples_per_symbol = int(round(sample_rate * symbol_duration))
    if samples_per_symbol <= 0:
        raise ValueError("symbol duration is too short for sample_rate")

    payload = build_payload(message, callsign)
    bits = build_transmission(payload)
    clean_audio = modulate_bits_2fsk(bits, sample_rate, symbol_duration, f0, f1)
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    results = []
    for scenario_index, scenario in enumerate(selected):
        for trial in range(trials):
            trial_seed = seed + scenario_index * trials + trial
            rng = np.random.default_rng(trial_seed)
            audio = apply_scenario(clean_audio, scenario, sample_rate, samples_per_symbol, rng)
            wav_path = output_path / f"{scenario.label}.wav"
            if trials > 1 and trial != 0:
                wav_path = output_path / f"{scenario.label}_trial_{trial + 1:03d}.wav"
            if save_wavs and trial == 0:
                save_wav(wav_path, audio, sample_rate)

            decoded_bits = demodulate_bits_2fsk(audio, sample_rate, symbol_duration, f0, f1)
            frame_result = parse_frame_from_stream(decoded_bits)
            results.append(
                SweepResult(
                    label=scenario.label,
                    snr_db=None,
                    trial=trial + 1,
                    seed=trial_seed,
                    wav_path=wav_path if save_wavs and trial == 0 else Path(""),
                    bit_errors=bit_error_count(bits, decoded_bits),
                    ber=bit_error_rate(bits, decoded_bits),
                    frame_result=frame_result,
                )
            )

    write_trials(output_path / "trials.csv", results)
    write_summary(output_path / "summary.csv", aggregate_results(results))
    return results


def print_summary(results: list[SweepResult]) -> None:
    """Print aggregate scenario results."""
    print("label,trials,crc_success_rate,payload_success_rate,avg_ber,max_ber,min_errors,max_errors")
    for aggregate in aggregate_results(results):
        print(
            f"{aggregate.label},{aggregate.trials},"
            f"{aggregate.crc_success_rate:.3f},{aggregate.payload_success_rate:.3f},"
            f"{aggregate.avg_ber:.6f},{aggregate.max_ber:.6f},"
            f"{aggregate.min_bit_errors},{aggregate.max_bit_errors}"
        )


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("message", help="message text to transmit")
    parser.add_argument("--callsign", default=None, help="optional callsign prefix")
    parser.add_argument("--output-dir", default="generated/channel_sweep")
    parser.add_argument("--seed", type=int, default=12345)
    parser.add_argument("--trials", type=int, default=10)
    parser.add_argument("--sample-rate", type=int, default=DEFAULT_SAMPLE_RATE)
    parser.add_argument("--symbol-duration", type=float, default=DEFAULT_SYMBOL_DURATION)
    parser.add_argument("--f0", type=float, default=DEFAULT_F0)
    parser.add_argument("--f1", type=float, default=DEFAULT_F1)
    parser.add_argument("--save-wavs", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    results = run_channel_sweep(
        args.message,
        callsign=args.callsign,
        output_dir=args.output_dir,
        sample_rate=args.sample_rate,
        symbol_duration=args.symbol_duration,
        f0=args.f0,
        f1=args.f1,
        seed=args.seed,
        trials=args.trials,
        save_wavs=args.save_wavs,
    )
    print_summary(results)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
