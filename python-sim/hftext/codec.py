"""Text codec for HFText Basic v0.1 with text codec v0.2."""

from __future__ import annotations

MAX_PAYLOAD_SYMBOLS = 127
MIN_LENGTH = 0
MAX_LENGTH = MAX_PAYLOAD_SYMBOLS

SHIFT_SYMBOL = 63

BASE_ALPHABET = " abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
SHIFT_ALPHABET = {
    0: "\n",
    1: "á",
    2: "à",
    3: "â",
    4: "ã",
    5: "é",
    6: "ê",
    7: "í",
    8: "ó",
    9: "ô",
    10: "õ",
    11: "ú",
    12: "ü",
    13: "ç",
    14: "ñ",
    15: ".",
    16: ",",
    17: "?",
    18: "!",
    19: ":",
    20: ";",
    21: "'",
    22: '"',
    23: "-",
    24: "_",
    25: "/",
    26: "\\",
    27: "+",
    28: "=",
    29: "*",
    30: "%",
    31: "&",
    32: "#",
    33: "@",
    34: "$",
    35: "<",
    36: ">",
    37: "(",
    38: ")",
    39: "[",
    40: "]",
    41: "{",
    42: "}",
    43: "|",
    44: "Á",
    45: "Â",
    46: "Ã",
    47: "É",
    48: "Ê",
    49: "Í",
    50: "Ó",
    51: "Ô",
    52: "Õ",
    53: "Ú",
    54: "Ü",
    55: "Ç",
    56: "Ñ",
    57: "`",
    58: "~",
    59: "^",
    60: "°",
}
ALPHABET = BASE_ALPHABET
INVALID_REPLACEMENT = "?"

BASE_CHAR_TO_SYMBOL = {char: symbol for symbol, char in enumerate(BASE_ALPHABET)}
SHIFT_CHAR_TO_SYMBOL = {char: symbol for symbol, char in SHIFT_ALPHABET.items()}
CHAR_TO_SYMBOLS = {
    **{char: [symbol] for char, symbol in BASE_CHAR_TO_SYMBOL.items()},
    **{char: [SHIFT_SYMBOL, symbol] for char, symbol in SHIFT_CHAR_TO_SYMBOL.items()},
}
SYMBOL_TO_CHAR = {symbol: char for symbol, char in enumerate(BASE_ALPHABET)}
BITS_PER_SYMBOL = 6
BITS_PER_BYTE = 8


def _is_supported_text_char(char: str) -> bool:
    return char in CHAR_TO_SYMBOLS


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
    symbols.extend(CHAR_TO_SYMBOLS[char])


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

    for symbol in symbols:
        if symbol < 0 or symbol >= 2**BITS_PER_SYMBOL:
            raise ValueError(f"invalid symbol: {symbol}")
        if shift_next:
            chars.append(SHIFT_ALPHABET.get(symbol, INVALID_REPLACEMENT))
            shift_next = False
            continue
        if symbol == SHIFT_SYMBOL:
            shift_next = True
            continue
        if symbol not in SYMBOL_TO_CHAR:
            raise ValueError(f"invalid symbol: {symbol}")

        chars.append(SYMBOL_TO_CHAR[symbol])

    if shift_next:
        chars.append(INVALID_REPLACEMENT)

    return "".join(chars)
