"""Text codec for the HFText Basic v0.1 protocol."""

from __future__ import annotations

MAX_PAYLOAD_SYMBOLS = 127
MIN_LENGTH = 0
MAX_LENGTH = MAX_PAYLOAD_SYMBOLS

SHIFT_SYMBOL = 60
ACUTE_SYMBOL = 61
TILDE_SYMBOL = 62
CEDILLA_SYMBOL = 63

ALPHABET = " abcdefghijklmnopqrstuvwxyz0123456789.,?!/-+:;@#$%&*()_=<>\\|"
INVALID_REPLACEMENT = "?"
ACUTE_VOWELS = {
    "á": "a",
    "é": "e",
    "í": "i",
    "ó": "o",
    "ú": "u",
}
TILDE_VOWELS = {
    "ã": "a",
    "õ": "o",
}
ACCENTED_TO_BASE = ACUTE_VOWELS | TILDE_VOWELS

CHAR_TO_SYMBOL = {char: symbol for symbol, char in enumerate(ALPHABET)}
SYMBOL_TO_CHAR = {symbol: char for symbol, char in enumerate(ALPHABET)}
BITS_PER_SYMBOL = 6
BITS_PER_BYTE = 8


def _is_supported_text_char(char: str) -> bool:
    lower = char.lower()
    return (
        "A" <= char <= "Z"
        or char in CHAR_TO_SYMBOL
        or lower in ACCENTED_TO_BASE
        or lower == "ç"
    )


def sanitize_text(text: str) -> str:
    """Return protocol-compatible presentation text."""
    sanitized = []
    for char in text:
        if _is_supported_text_char(char):
            sanitized.append(char)
        else:
            sanitized.append(INVALID_REPLACEMENT)
    return "".join(sanitized)


def _append_encoded_char(symbols: list[int], char: str) -> None:
    lower = char.lower()
    is_upper = char != lower

    if lower in ACUTE_VOWELS:
        symbols.append(ACUTE_SYMBOL)
        if is_upper:
            symbols.append(SHIFT_SYMBOL)
        symbols.append(CHAR_TO_SYMBOL[ACUTE_VOWELS[lower]])
    elif lower in TILDE_VOWELS:
        symbols.append(TILDE_SYMBOL)
        if is_upper:
            symbols.append(SHIFT_SYMBOL)
        symbols.append(CHAR_TO_SYMBOL[TILDE_VOWELS[lower]])
    elif lower == "ç":
        if is_upper:
            symbols.append(SHIFT_SYMBOL)
        symbols.append(CEDILLA_SYMBOL)
    elif "A" <= char <= "Z":
        symbols.append(SHIFT_SYMBOL)
        symbols.append(CHAR_TO_SYMBOL[char.lower()])
    else:
        symbols.append(CHAR_TO_SYMBOL[char])


def encoded_symbol_count(text: str) -> int:
    """Return the number of 6-bit symbols after text encoding."""
    symbols = []
    for char in sanitize_text(text):
        _append_encoded_char(symbols, char)
    return len(symbols)


def encode_text_to_symbols(text: str) -> list[int]:
    """Encode payload text as 6-bit symbol values."""
    symbols = []
    for char in sanitize_text(text):
        _append_encoded_char(symbols, char)

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
    accent_next: str | None = None

    for symbol in symbols:
        if symbol == SHIFT_SYMBOL:
            shift_next = True
            continue
        if symbol in (ACUTE_SYMBOL, TILDE_SYMBOL):
            if accent_next is not None:
                chars.append(INVALID_REPLACEMENT)
            accent_next = "acute" if symbol == ACUTE_SYMBOL else "tilde"
            continue
        if symbol == CEDILLA_SYMBOL:
            if accent_next is not None:
                chars.append(INVALID_REPLACEMENT)
            chars.append("Ç" if shift_next else "ç")
            shift_next = False
            accent_next = None
            continue
        if symbol not in SYMBOL_TO_CHAR:
            raise ValueError(f"invalid symbol: {symbol}")

        char = SYMBOL_TO_CHAR[symbol]
        if accent_next == "acute" and char in "aeiou":
            accented = {"a": "á", "e": "é", "i": "í", "o": "ó", "u": "ú"}[char]
            chars.append(accented.upper() if shift_next else accented)
        elif accent_next == "tilde" and char in "ao":
            accented = {"a": "ã", "o": "õ"}[char]
            chars.append(accented.upper() if shift_next else accented)
        else:
            if accent_next is not None:
                chars.append(INVALID_REPLACEMENT)
            if shift_next and "a" <= char <= "z":
                chars.append(char.upper())
            else:
                chars.append(char)
        shift_next = False
        accent_next = None

    return "".join(chars)
