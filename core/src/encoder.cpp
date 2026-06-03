#include "hftext_encoder.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace hftext {
namespace {

constexpr char kInvalidReplacement = '?';
constexpr char kAlphabet[] = " abcdefghijklmnopqrstuvwxyz0123456789.,?!/-+:;@#$%&*()_=<>\\|";
constexpr std::size_t kAlphabetSize = sizeof(kAlphabet) - 1;
constexpr char kAcute = 1;
constexpr char kTilde = 2;

std::array<std::int16_t, 256> makeCharToSymbol() {
    std::array<std::int16_t, 256> table{};
    table.fill(-1);
    for (std::size_t index = 0; index < kAlphabetSize; ++index) {
        table[static_cast<unsigned char>(kAlphabet[index])] = static_cast<std::int16_t>(index);
    }
    return table;
}

const std::array<std::int16_t, 256> kCharToSymbol = makeCharToSymbol();

bool isAsciiUpper(char ch) {
    return ch >= 'A' && ch <= 'Z';
}

bool isAsciiLower(char ch) {
    return ch >= 'a' && ch <= 'z';
}

char asciiToLower(char ch) {
    return static_cast<char>(ch - 'A' + 'a');
}

bool startsWith(const std::string& text, std::size_t offset, const char* token) {
    const std::string value(token);
    return text.compare(offset, value.size(), value) == 0;
}

std::size_t utf8SequenceLength(unsigned char lead) {
    if ((lead & 0x80U) == 0) {
        return 1;
    }
    if ((lead & 0xE0U) == 0xC0U) {
        return 2;
    }
    if ((lead & 0xF0U) == 0xE0U) {
        return 3;
    }
    if ((lead & 0xF8U) == 0xF0U) {
        return 4;
    }
    return 1;
}

struct TextToken {
    std::string text;
    char ascii = '\0';
    char base = '\0';
    char accent = '\0';
    bool uppercase = false;
    bool cedilla = false;
    bool supported = false;
};

TextToken readToken(const std::string& text, std::size_t& offset) {
    const unsigned char lead = static_cast<unsigned char>(text[offset]);
    if (lead <= 0x7FU) {
        const char ch = static_cast<char>(lead);
        ++offset;
        return TextToken{std::string(1, ch), ch, '\0', '\0', false, false, isSupportedPresentationChar(ch)};
    }

    struct Utf8Mapping {
        const char* text;
        char base;
        char accent;
        bool uppercase;
        bool cedilla;
    };

    static constexpr Utf8Mapping kMappings[] = {
        {"\xC3\xA1", 'a', kAcute, false, false},  // á
        {"\xC3\x81", 'a', kAcute, true, false},   // Á
        {"\xC3\xA9", 'e', kAcute, false, false},  // é
        {"\xC3\x89", 'e', kAcute, true, false},   // É
        {"\xC3\xAD", 'i', kAcute, false, false},  // í
        {"\xC3\x8D", 'i', kAcute, true, false},   // Í
        {"\xC3\xB3", 'o', kAcute, false, false},  // ó
        {"\xC3\x93", 'o', kAcute, true, false},   // Ó
        {"\xC3\xBA", 'u', kAcute, false, false},  // ú
        {"\xC3\x9A", 'u', kAcute, true, false},   // Ú
        {"\xC3\xA3", 'a', kTilde, false, false},  // ã
        {"\xC3\x83", 'a', kTilde, true, false},   // Ã
        {"\xC3\xB5", 'o', kTilde, false, false},  // õ
        {"\xC3\x95", 'o', kTilde, true, false},   // Õ
        {"\xC3\xA7", '\0', '\0', false, true},    // ç
        {"\xC3\x87", '\0', '\0', true, true},     // Ç
    };

    for (const auto& mapping : kMappings) {
        if (startsWith(text, offset, mapping.text)) {
            const std::string token(mapping.text);
            offset += token.size();
            return TextToken{token, '\0', mapping.base, mapping.accent, mapping.uppercase, mapping.cedilla, true};
        }
    }

    const std::size_t length = (std::min)(utf8SequenceLength(lead), text.size() - offset);
    offset += length;
    return TextToken{std::string(1, kInvalidReplacement), kInvalidReplacement, '\0', '\0', false, false, true};
}

void appendTokenSymbols(std::vector<std::uint8_t>& symbols, const TextToken& token) {
    if (token.cedilla) {
        if (token.uppercase) {
            symbols.push_back(kShiftSymbol);
        }
        symbols.push_back(kCedillaSymbol);
        return;
    }

    if (token.accent == kAcute || token.accent == kTilde) {
        symbols.push_back(token.accent == kAcute ? kAcuteSymbol : kTildeSymbol);
        if (token.uppercase) {
            symbols.push_back(kShiftSymbol);
        }
        symbols.push_back(static_cast<std::uint8_t>(kCharToSymbol[static_cast<unsigned char>(token.base)]));
        return;
    }

    const char ch = token.ascii;
    if (isAsciiUpper(ch)) {
        symbols.push_back(kShiftSymbol);
        symbols.push_back(static_cast<std::uint8_t>(kCharToSymbol[static_cast<unsigned char>(asciiToLower(ch))]));
    } else {
        symbols.push_back(static_cast<std::uint8_t>(kCharToSymbol[static_cast<unsigned char>(ch)]));
    }
}

std::string accentedUtf8(char base, char accent, bool uppercase) {
    if (accent == kAcute) {
        if (base == 'a') {
            return uppercase ? "\xC3\x81" : "\xC3\xA1";
        }
        if (base == 'e') {
            return uppercase ? "\xC3\x89" : "\xC3\xA9";
        }
        if (base == 'i') {
            return uppercase ? "\xC3\x8D" : "\xC3\xAD";
        }
        if (base == 'o') {
            return uppercase ? "\xC3\x93" : "\xC3\xB3";
        }
        if (base == 'u') {
            return uppercase ? "\xC3\x9A" : "\xC3\xBA";
        }
    }
    if (accent == kTilde) {
        if (base == 'a') {
            return uppercase ? "\xC3\x83" : "\xC3\xA3";
        }
        if (base == 'o') {
            return uppercase ? "\xC3\x95" : "\xC3\xB5";
        }
    }
    return {};
}

}  // namespace

bool isSupportedPresentationChar(char ch) {
    return isAsciiUpper(ch) || kCharToSymbol[static_cast<unsigned char>(ch)] >= 0;
}

std::string sanitizeText(const std::string& text) {
    std::string output;
    output.reserve(text.size());

    for (std::size_t offset = 0; offset < text.size();) {
        const auto token = readToken(text, offset);
        output += token.supported ? token.text : std::string(1, kInvalidReplacement);
    }

    return output;
}

int encodedSymbolCount(const std::string& text) {
    std::vector<std::uint8_t> symbols;
    symbols.reserve(text.size());

    const auto sanitized = sanitizeText(text);
    for (std::size_t offset = 0; offset < sanitized.size();) {
        appendTokenSymbols(symbols, readToken(sanitized, offset));
    }

    return static_cast<int>(symbols.size());
}

std::vector<std::uint8_t> encodeTextToSymbols(const std::string& text) {
    std::vector<std::uint8_t> symbols;
    symbols.reserve(text.size());

    const auto sanitized = sanitizeText(text);
    for (std::size_t offset = 0; offset < sanitized.size();) {
        appendTokenSymbols(symbols, readToken(sanitized, offset));
        if (symbols.size() > kMaxPayloadSymbols) {
            throw std::length_error("payload must be at most 127 symbols");
        }
    }

    return symbols;
}

std::string decodeSymbolsToText(const std::vector<std::uint8_t>& symbols) {
    std::string output;
    output.reserve(symbols.size());
    bool shiftNext = false;
    char accentNext = '\0';

    for (std::uint8_t symbol : symbols) {
        if (symbol == kShiftSymbol) {
            shiftNext = true;
            continue;
        }
        if (symbol == kAcuteSymbol || symbol == kTildeSymbol) {
            if (accentNext != '\0') {
                output.push_back(kInvalidReplacement);
            }
            accentNext = symbol == kAcuteSymbol ? kAcute : kTilde;
            continue;
        }
        if (symbol == kCedillaSymbol) {
            if (accentNext != '\0') {
                output.push_back(kInvalidReplacement);
            }
            output += shiftNext ? "\xC3\x87" : "\xC3\xA7";
            shiftNext = false;
            accentNext = '\0';
            continue;
        }
        if (symbol >= kAlphabetSize) {
            throw std::invalid_argument("invalid symbol");
        }

        char ch = kAlphabet[symbol];
        const auto accented = accentedUtf8(ch, accentNext, shiftNext);
        if (!accented.empty()) {
            output += accented;
        } else {
            if (accentNext != '\0') {
                output.push_back(kInvalidReplacement);
            }
            if (shiftNext && isAsciiLower(ch)) {
                output.push_back(static_cast<char>(ch - 'a' + 'A'));
            } else {
                output.push_back(ch);
            }
        }
        shiftNext = false;
        accentNext = '\0';
    }

    return output;
}

int frameLength(const std::string& payloadText) {
    return static_cast<int>(encodeTextToSymbols(payloadText).size());
}

}  // namespace hftext
