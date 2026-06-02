#include "hftext_encoder.h"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expectSymbolsEqual(const std::vector<std::uint8_t>& actual, const std::vector<std::uint8_t>& expected) {
    assert(actual == expected);
}

}  // namespace

int main() {
    expectSymbolsEqual(hftext::encodeTextToSymbols("abc 123"), {1, 2, 3, 0, 28, 29, 30});

    expectSymbolsEqual(hftext::encodeTextToSymbols("AbZ"), {hftext::kShiftSymbol, 1, 2, hftext::kShiftSymbol, 26});
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols("AbZ")) == "AbZ");

    const std::string symbols = ".,?!/-+:;@#$%&*()_=<>\\|";
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols(symbols)) == symbols);

    assert(hftext::sanitizeText("a~b") == "a?b");
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols("a~b")) == "a?b");

    const std::string alphabet = " abcdefghijklmnopqrstuvwxyz0123456789.,?!/-+:;@#$%&*()_=<>\\|";
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols(alphabet)) == alphabet);

    assert(hftext::frameLength("") == 0);
    assert(hftext::frameLength(std::string(hftext::kMaxPayloadSymbols, 'a')) == hftext::kMaxPayloadSymbols);
    assert(hftext::frameLength("Aa") == 3);

    bool lengthRejected = false;
    try {
        (void)hftext::encodeTextToSymbols(std::string(64, 'A'));
    } catch (const std::length_error&) {
        lengthRejected = true;
    }
    assert(lengthRejected);

    assert(hftext::decodeSymbolsToText({hftext::kShiftSymbol, 27}) == "0");
    assert(hftext::decodeSymbolsToText({1, hftext::kShiftSymbol}) == "a");

    bool reservedRejected = false;
    try {
        (void)hftext::decodeSymbolsToText({61});
    } catch (const std::invalid_argument&) {
        reservedRejected = true;
    }
    assert(reservedRejected);
}
