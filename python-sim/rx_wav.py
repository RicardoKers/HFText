"""Decode an HFText 2-FSK WAV file."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import soundfile as sf

from hftext.frame import FrameResult
from hftext.modulator import DEFAULT_F0, DEFAULT_F1, DEFAULT_SYMBOL_DURATION
from hftext.receiver import receive_samples_2fsk


def load_wav_mono(path: str | Path) -> tuple[np.ndarray, int]:
    """Read WAV audio as mono float32 samples."""
    samples, sample_rate = sf.read(str(path), dtype="float32")
    audio = np.asarray(samples, dtype=np.float32)
    if audio.ndim == 2:
        audio = np.mean(audio, axis=1, dtype=np.float32)
    return audio, sample_rate


def decode_wav(
    input_path: str | Path,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
    sync_search: bool = True,
    offset_step: int | None = None,
) -> FrameResult:
    """Read a WAV file, find SYNC, and decode one HFText frame."""
    samples, sample_rate = load_wav_mono(input_path)
    return receive_samples_2fsk(
        samples,
        sample_rate,
        symbol_duration,
        f0,
        f1,
        sync_search=sync_search,
        offset_step=offset_step,
    ).frame_result


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_wav", help="input WAV path")
    parser.add_argument("--symbol-duration", type=float, default=DEFAULT_SYMBOL_DURATION)
    parser.add_argument("--f0", type=float, default=DEFAULT_F0)
    parser.add_argument("--f1", type=float, default=DEFAULT_F1)
    parser.add_argument("--no-sync-search", action="store_true")
    parser.add_argument("--offset-step", type=int, default=None)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    result = decode_wav(
        args.input_wav,
        symbol_duration=args.symbol_duration,
        f0=args.f0,
        f1=args.f1,
        sync_search=not args.no_sync_search,
        offset_step=args.offset_step,
    )

    if not result.frame_detected:
        print(f"Quadro não detectado: {result.error}")
        return 1
    if not result.crc_ok:
        print("Quadro detectado, mas CRC inválido.")
        if result.error:
            print(f"Erro: {result.error}")
        return 2
    if not result.payload_valid:
        print("Quadro detectado, CRC válido, mas payload inválido.")
        if result.error:
            print(f"Erro: {result.error}")
        return 3

    print(result.text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
