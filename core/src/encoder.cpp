#include "hftext_encoder.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace hftext {
namespace {

constexpr char kInvalidReplacement = '?';
constexpr char kBaseAlphabet[] = " abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr std::size_t kBaseAlphabetSize = sizeof(kBaseAlphabet) - 1;

struct ShiftMapping {
    std::uint8_t symbol;
    const char* text;
};

static constexpr ShiftMapping kShiftMappings[] = {
    {0, "\n"},
    {1, "\xC3\xA1"},
    {2, "\xC3\xA0"},
    {3, "\xC3\xA2"},
    {4, "\xC3\xA3"},
    {5, "\xC3\xA9"},
    {6, "\xC3\xAA"},
    {7, "\xC3\xAD"},
    {8, "\xC3\xB3"},
    {9, "\xC3\xB4"},
    {10, "\xC3\xB5"},
    {11, "\xC3\xBA"},
    {12, "\xC3\xBC"},
    {13, "\xC3\xA7"},
    {14, "\xC3\xB1"},
    {15, "."},
    {16, ","},
    {17, "?"},
    {18, "!"},
    {19, ":"},
    {20, ";"},
    {21, "'"},
    {22, "\""},
    {23, "-"},
    {24, "_"},
    {25, "/"},
    {26, "\\"},
    {27, "+"},
    {28, "="},
    {29, "*"},
    {30, "%"},
    {31, "&"},
    {32, "#"},
    {33, "@"},
    {34, "$"},
    {35, "<"},
    {36, ">"},
    {37, "("},
    {38, ")"},
    {39, "["},
    {40, "]"},
    {41, "{"},
    {42, "}"},
    {43, "|"},
    {44, "\xC3\x81"},
    {45, "\xC3\x82"},
    {46, "\xC3\x83"},
    {47, "\xC3\x89"},
    {48, "\xC3\x8A"},
    {49, "\xC3\x8D"},
    {50, "\xC3\x93"},
    {51, "\xC3\x94"},
    {52, "\xC3\x95"},
    {53, "\xC3\x9A"},
    {54, "\xC3\x9C"},
    {55, "\xC3\x87"},
    {56, "\xC3\x91"},
    {57, "`"},
    {58, "~"},
    {59, "^"},
    {60, "\xC2\xB0"},
};

std::array<std::int16_t, 256> makeCharToSymbol() {
    std::array<std::int16_t, 256> table{};
    table.fill(-1);
    for (std::size_t index = 0; index < kBaseAlphabetSize; ++index) {
        table[static_cast<unsigned char>(kBaseAlphabet[index])] = static_cast<std::int16_t>(index);
    }
    return table;
}

const std::array<std::int16_t, 256> kCharToSymbol = makeCharToSymbol();

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

std::string readToken(const std::string& text, std::size_t& offset) {
    const unsigned char lead = static_cast<unsigned char>(text[offset]);
    if (lead <= 0x7FU) {
        ++offset;
        return std::string(1, static_cast<char>(lead));
    }

    const std::size_t length = (std::min)(utf8SequenceLength(lead), text.size() - offset);
    const auto token = text.substr(offset, length);
    offset += length;
    return token;
}

const ShiftMapping* shiftMappingForToken(const std::string& token) {
    for (const auto& mapping : kShiftMappings) {
        if (token == mapping.text) {
            return &mapping;
        }
    }
    return nullptr;
}

const ShiftMapping* shiftMappingForSymbol(std::uint8_t symbol) {
    for (const auto& mapping : kShiftMappings) {
        if (mapping.symbol == symbol) {
            return &mapping;
        }
    }
    return nullptr;
}

bool isSupportedToken(const std::string& token) {
    if (token.size() == 1 && kCharToSymbol[static_cast<unsigned char>(token[0])] >= 0) {
        return true;
    }
    return shiftMappingForToken(token) != nullptr;
}

void appendReplacementSymbols(std::vector<std::uint8_t>& symbols) {
    symbols.push_back(kShiftSymbol);
    symbols.push_back(17);
}

void appendTokenSymbols(std::vector<std::uint8_t>& symbols, const std::string& token) {
    if (token.size() == 1) {
        const auto symbol = kCharToSymbol[static_cast<unsigned char>(token[0])];
        if (symbol >= 0) {
            symbols.push_back(static_cast<std::uint8_t>(symbol));
            return;
        }
    }

    if (const auto* mapping = shiftMappingForToken(token)) {
        symbols.push_back(kShiftSymbol);
        symbols.push_back(mapping->symbol);
        return;
    }

    appendReplacementSymbols(symbols);
}

}  // namespace

bool isSupportedPresentationChar(char ch) {
    if (kCharToSymbol[static_cast<unsigned char>(ch)] >= 0) {
        return true;
    }
    return shiftMappingForToken(std::string(1, ch)) != nullptr;
}

std::string sanitizeText(const std::string& text) {
    std::string output;
    output.reserve(text.size());

    for (std::size_t offset = 0; offset < text.size();) {
        const auto token = readToken(text, offset);
        output += isSupportedToken(token) ? token : std::string(1, kInvalidReplacement);
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

    for (std::uint8_t symbol : symbols) {
        if (symbol > kShiftSymbol) {
            throw std::invalid_argument("invalid symbol");
        }
        if (shiftNext) {
            if (const auto* mapping = shiftMappingForSymbol(symbol)) {
                output += mapping->text;
            } else {
                output.push_back(kInvalidReplacement);
            }
            shiftNext = false;
            continue;
        }
        if (symbol == kShiftSymbol) {
            shiftNext = true;
            continue;
        }
        if (symbol >= kBaseAlphabetSize) {
            throw std::invalid_argument("invalid symbol");
        }
        output.push_back(kBaseAlphabet[symbol]);
    }

    if (shiftNext) {
        output.push_back(kInvalidReplacement);
    }

    return output;
}

int frameLength(const std::string& payloadText) {
    return static_cast<int>(encodeTextToSymbols(payloadText).size());
}

}  // namespace hftext
