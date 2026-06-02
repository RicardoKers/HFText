"""Text codec for the HFText Basic v0.1 protocol."""

from __future__ import annotations

MAX_PAYLOAD_SYMBOLS = 127
MIN_LENGTH = 0
MAX_LENGTH = MAX_PAYLOAD_SYMBOLS

SHIFT_SYMBOL = 60

ALPHABET = " abcdefghijklmnopqrstuvwxyz0123456789.,?!/-+:;@#$%&*()_=<>\\|"
INVALID_REPLACEMENT = "?"

CHAR_TO_SYMBOL = {char: symbol for symbol, char in enumerate(ALPHABET)}
SYMBOL_TO_CHAR = {symbol: char for symbol, char in enumerate(ALPHABET)}
BITS_PER_SYMBOL = 6
BITS_PER_BYTE = 8


def sanitize_text(text: str) -> str:
    """Return protocol-compatible presentation text."""
    sanitized = []
    for char in text:
        if "A" <= char <= "Z" or char in CHAR_TO_SYMBOL:
            sanitized.append(char)
        else:
            sanitized.append(INVALID_REPLACEMENT)
    return "".join(sanitized)


def encode_text_to_symbols(text: str) -> list[int]:
    """Encode payload text as 6-bit symbol values."""
    symbols = []
    for char in sanitize_text(text):
        if "A" <= char <= "Z":
            symbols.append(SHIFT_SYMBOL)
            symbols.append(CHAR_TO_SYMBOL[char.lower()])
        else:
            symbols.append(CHAR_TO_SYMBOL[char])

    if len(symbols) > MAX_PAYLOAD_SYMBOLS:
        raise ValueError(f"payload must be at most {MAX_PAYLOAD_SYMBOLS} symbols")

    return symbols


def frame_length(payload_text: str) -> int:
    """Return the LENGTH field value for the encoded payload."""
    return len(encode_text_to_symbols(payload_text))


def pack_symbols_to_bytes(symbols: list[int]) -> bytes:
    """Pack 6-bit symbols into bytes using MSB-first bit order."""
    bit_buffer = 0
    bit_count = 0
    output = bytearray()

    for symbol in symbols:
        if symbol < 0 or symbol >= 2**BITS_PER_SYMBOL:
            raise ValueError(f"invalid 6-bit symbol: {symbol}")

        bit_buffer = (bit_buffer << BITS_PER_SYMBOL) | symbol
        bit_count += BITS_PER_SYMBOL

        while bit_count >= BITS_PER_BYTE:
            bit_count -= BITS_PER_BYTE
            output.append((bit_buffer >> bit_count) & 0xFF)

    if bit_count:
        output.append((bit_buffer << (BITS_PER_BYTE - bit_count)) & 0xFF)

    return bytes(output)


def unpack_symbols_from_bytes(data: bytes, symbol_count: int) -> list[int]:
    """Unpack symbol_count 6-bit symbols from MSB-first packed bytes."""
    if symbol_count < 0:
        raise ValueError("symbol_count must be non-negative")

    available_bits = len(data) * BITS_PER_BYTE
    required_bits = symbol_count * BITS_PER_SYMBOL
    if required_bits > available_bits:
        raise ValueError("not enough data for requested symbols")

    bit_buffer = 0
    bit_count = 0
    symbols = []

    for byte in data:
        bit_buffer = (bit_buffer << BITS_PER_BYTE) | byte
        bit_count += BITS_PER_BYTE

        while bit_count >= BITS_PER_SYMBOL and len(symbols) < symbol_count:
            bit_count -= BITS_PER_SYMBOL
            symbols.append((bit_buffer >> bit_count) & 0x3F)

    return symbols


def decode_symbols_to_text(symbols: list[int]) -> str:
    """Decode 6-bit symbol values back to presentation text."""
    chars = []
    shift_next = False

    for symbol in symbols:
        if symbol == SHIFT_SYMBOL:
            shift_next = True
            continue
        if symbol not in SYMBOL_TO_CHAR:
            raise ValueError(f"invalid symbol: {symbol}")

        char = SYMBOL_TO_CHAR[symbol]
        if shift_next and "a" <= char <= "z":
            chars.append(char.upper())
        else:
            chars.append(char)
        shift_next = False

    return "".join(chars)
