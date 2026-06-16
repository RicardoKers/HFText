"""Generate an HFText 2-FSK WAV file."""

from __future__ import annotations

import argparse
from pathlib import Path

from hftext.frame import build_transmission
from hftext.modulator import (
    DEFAULT_F0,
    DEFAULT_F1,
    DEFAULT_SAMPLE_RATE,
    DEFAULT_SYMBOL_DURATION,
    modulate_bits_2fsk,
    save_wav,
)


def build_payload(message: str, callsign: str | None = None) -> str:
    """Return the transmitted payload text."""
    if callsign is None or callsign == "":
        return message
    return f"{callsign} {message}"


def generate_wav(
    message: str,
    output_path: str | Path,
    callsign: str | None = None,
    sample_rate: int = DEFAULT_SAMPLE_RATE,
    symbol_duration: float = DEFAULT_SYMBOL_DURATION,
    f0: float = DEFAULT_F0,
    f1: float = DEFAULT_F1,
) -> str:
    """Build TX bits, modulate them, save WAV, and return payload text."""
    payload = build_payload(message, callsign)
    bits = build_transmission(payload)
    audio = modulate_bits_2fsk(bits, sample_rate, symbol_duration, f0, f1)
    save_wav(output_path, audio, sample_rate)
    return payload


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("message", help="message text to transmit")
    parser.add_argument("output_wav", help="output WAV path")
    parser.add_argument("--callsign", default=None, help="optional callsign prefix")
    parser.add_argument("--sample-rate", type=int, default=DEFAULT_SAMPLE_RATE)
    parser.add_argument("--symbol-duration", type=float, default=DEFAULT_SYMBOL_DURATION)
    parser.add_argument("--f0", type=float, default=DEFAULT_F0)
    parser.add_argument("--f1", type=float, default=DEFAULT_F1)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    payload = generate_wav(
        args.message,
        args.output_wav,
        callsign=args.callsign,
        sample_rate=args.sample_rate,
        symbol_duration=args.symbol_duration,
        f0=args.f0,
        f1=args.f1,
    )
    print(f"WAV generated: {args.output_wav}")
    print(f"Payload: {payload}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
