#include "hftext_encoder.h"

#include <array>
#include <stdexcept>

namespace hftext {
namespace {

constexpr char kInvalidReplacement = '?';
constexpr char kAlphabet[] = " abcdefghijklmnopqrstuvwxyz0123456789.,?!/-+:;@#$%&*()_=<>\\|";
constexpr std::size_t kAlphabetSize = sizeof(kAlphabet) - 1;

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

}  // namespace

bool isSupportedPresentationChar(char ch) {
    return isAsciiUpper(ch) || kCharToSymbol[static_cast<unsigned char>(ch)] >= 0;
}

std::string sanitizeText(const std::string& text) {
    std::string output;
    output.reserve(text.size());

    for (char ch : text) {
        output.push_back(isSupportedPresentationChar(ch) ? ch : kInvalidReplacement);
    }

    return output;
}

std::vector<std::uint8_t> encodeTextToSymbols(const std::string& text) {
    std::vector<std::uint8_t> symbols;
    symbols.reserve(text.size());

    for (char ch : sanitizeText(text)) {
        if (isAsciiUpper(ch)) {
            symbols.push_back(kShiftSymbol);
            symbols.push_back(static_cast<std::uint8_t>(kCharToSymbol[static_cast<unsigned char>(asciiToLower(ch))]));
        } else {
            symbols.push_back(static_cast<std::uint8_t>(kCharToSymbol[static_cast<unsigned char>(ch)]));
        }

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
        if (symbol == kShiftSymbol) {
            shiftNext = true;
            continue;
        }
        if (symbol >= kAlphabetSize) {
            throw std::invalid_argument("invalid symbol");
        }

        char ch = kAlphabet[symbol];
        if (shiftNext && isAsciiLower(ch)) {
            output.push_back(static_cast<char>(ch - 'a' + 'A'));
        } else {
            output.push_back(ch);
        }
        shiftNext = false;
    }

    return output;
}

int frameLength(const std::string& payloadText) {
    return static_cast<int>(encodeTextToSymbols(payloadText).size());
}

}  // namespace hftext
