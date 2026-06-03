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
    const std::string ola = std::string("ol") + "\xC3\xA1";
    const std::string atencao = std::string("aten") + "\xC3\xA7" + "\xC3\xA3" + "o";
    const std::string olaAtencao = ola + " " + atencao;
    const std::string accentedUpper = std::string("\xC3\x81") + "\xC3\x89" + "\xC3\x83" + "\xC3\x95" + "\xC3\x87";

    expectSymbolsEqual(hftext::encodeTextToSymbols("abc 123"), {1, 2, 3, 0, 28, 29, 30});

    expectSymbolsEqual(hftext::encodeTextToSymbols("AbZ"), {hftext::kShiftSymbol, 1, 2, hftext::kShiftSymbol, 26});
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols("AbZ")) == "AbZ");

    const std::string symbols = ".,?!/-+:;@#$%&*()_=<>\\|";
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols(symbols)) == symbols);

    assert(hftext::sanitizeText("a~b") == "a?b");
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols("a~b")) == "a?b");

    expectSymbolsEqual(hftext::encodeTextToSymbols(ola), {15, 12, hftext::kAcuteSymbol, 1});
    expectSymbolsEqual(
        hftext::encodeTextToSymbols(atencao),
        {1, 20, 5, 14, hftext::kCedillaSymbol, hftext::kTildeSymbol, 1, 15}
    );
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols(olaAtencao)) == olaAtencao);

    expectSymbolsEqual(
        hftext::encodeTextToSymbols(accentedUpper),
        {
            hftext::kAcuteSymbol,
            hftext::kShiftSymbol,
            1,
            hftext::kAcuteSymbol,
            hftext::kShiftSymbol,
            5,
            hftext::kTildeSymbol,
            hftext::kShiftSymbol,
            1,
            hftext::kTildeSymbol,
            hftext::kShiftSymbol,
            15,
            hftext::kShiftSymbol,
            hftext::kCedillaSymbol,
        }
    );
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols(accentedUpper)) == accentedUpper);
    assert(hftext::decodeSymbolsToText({hftext::kAcuteSymbol, 2}) == "?b");
    assert(hftext::decodeSymbolsToText({hftext::kTildeSymbol, 5}) == "?e");

    const std::string alphabet = " abcdefghijklmnopqrstuvwxyz0123456789.,?!/-+:;@#$%&*()_=<>\\|";
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols(alphabet)) == alphabet);

    assert(hftext::frameLength("") == 0);
    assert(hftext::frameLength(std::string(hftext::kMaxPayloadSymbols, 'a')) == hftext::kMaxPayloadSymbols);
    assert(hftext::frameLength("Aa") == 3);
    assert(hftext::encodedSymbolCount(std::string("\xC3\xA1") + "\xC3\xA3" + "\xC3\xA7") == 5);

    bool lengthRejected = false;
    try {
        (void)hftext::encodeTextToSymbols(std::string(64, 'A'));
    } catch (const std::length_error&) {
        lengthRejected = true;
    }
    assert(lengthRejected);

    assert(hftext::decodeSymbolsToText({hftext::kShiftSymbol, 27}) == "0");
    assert(hftext::decodeSymbolsToText({1, hftext::kShiftSymbol}) == "a");

    bool invalidRejected = false;
    try {
        (void)hftext::decodeSymbolsToText({64});
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);
}
