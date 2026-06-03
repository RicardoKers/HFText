#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hftext {

constexpr std::uint8_t kShiftSymbol = 60;
constexpr std::uint8_t kAcuteSymbol = 61;
constexpr std::uint8_t kTildeSymbol = 62;
constexpr std::uint8_t kCedillaSymbol = 63;
constexpr int kMaxPayloadSymbols = 127;

std::string sanitizeText(const std::string& text);
std::vector<std::uint8_t> encodeTextToSymbols(const std::string& text);
std::string decodeSymbolsToText(const std::vector<std::uint8_t>& symbols);
int encodedSymbolCount(const std::string& text);
int frameLength(const std::string& payloadText);
bool isSupportedPresentationChar(char ch);

}  // namespace hftext
