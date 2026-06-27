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
    const std::string international =
        std::string("\xC3\xA0") + "\xC3\xA2" + "\xC3\xAA" + "\xC3\xAD" + "\xC3\xB3" + "\xC3\xB4" +
        "\xC3\xB5" + "\xC3\xBA" + "\xC3\xBC" + "\xC3\xA7" + "\xC3\xB1" + "\xC3\x81" +
        "\xC3\x82" + "\xC3\x83" + "\xC3\x89" + "\xC3\x8A" + "\xC3\x8D" + "\xC3\x93" +
        "\xC3\x94" + "\xC3\x95" + "\xC3\x9A" + "\xC3\x9C" + "\xC3\x87" + "\xC3\x91";

    expectSymbolsEqual(hftext::encodeTextToSymbols("abc 123"), {1, 2, 3, 0, 28, 29, 30});

    expectSymbolsEqual(hftext::encodeTextToSymbols("AbZ"), {37, 2, 62});
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols("AbZ")) == "AbZ");

    const std::string symbols = ".,?!:;'\"-_/\\+=*%&#@$<>()[]{}|`~^\xC2\xB0";
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols(symbols)) == symbols);

    assert(hftext::sanitizeText(std::string("a") + "\xE2\x82\xAC" + "b") == "a?b");
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols(std::string("a") + "\xE2\x82\xAC" + "b")) == "a?b");

    expectSymbolsEqual(hftext::encodeTextToSymbols(ola), {15, 12, hftext::kShiftSymbol, 1});
    expectSymbolsEqual(
        hftext::encodeTextToSymbols(atencao),
        {1, 20, 5, 14, hftext::kShiftSymbol, 13, hftext::kShiftSymbol, 4, 15}
    );
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols(olaAtencao)) == olaAtencao);

    expectSymbolsEqual(
        hftext::encodeTextToSymbols(accentedUpper),
        {
            hftext::kShiftSymbol,
            44,
            hftext::kShiftSymbol,
            47,
            hftext::kShiftSymbol,
            46,
            hftext::kShiftSymbol,
            52,
            hftext::kShiftSymbol,
            55,
        }
    );
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols(accentedUpper)) == accentedUpper);
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols(international)) == international);
    assert(hftext::decodeSymbolsToText({hftext::kShiftSymbol, 61}) == "?");
    assert(hftext::decodeSymbolsToText({hftext::kShiftSymbol, 62}) == "?");
    assert(hftext::decodeSymbolsToText({hftext::kShiftSymbol, hftext::kShiftSymbol}) == "?");

    const std::string alphabet = " abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    assert(hftext::decodeSymbolsToText(hftext::encodeTextToSymbols(alphabet)) == alphabet);

    assert(hftext::frameLength("") == 0);
    assert(hftext::frameLength(std::string(hftext::kMaxPayloadSymbols, 'a')) == hftext::kMaxPayloadSymbols);
    assert(hftext::frameLength("Aa") == 2);
    assert(hftext::encodedSymbolCount(std::string("\xC3\xA1") + "\xC3\xA3" + "\xC3\xA7") == 6);

    bool lengthRejected = false;
    try {
        (void)hftext::encodeTextToSymbols(std::string(64, '!'));
    } catch (const std::length_error&) {
        lengthRejected = true;
    }
    assert(lengthRejected);

    assert(hftext::decodeSymbolsToText({hftext::kShiftSymbol, 27}) == "+");
    assert(hftext::decodeSymbolsToText({1, hftext::kShiftSymbol}) == "a?");

    bool invalidRejected = false;
    try {
        (void)hftext::decodeSymbolsToText({64});
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);
}
