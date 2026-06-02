#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hftext {

constexpr std::uint8_t kShiftSymbol = 60;
constexpr int kMaxPayloadSymbols = 127;

std::string sanitizeText(const std::string& text);
std::vector<std::uint8_t> encodeTextToSymbols(const std::string& text);
std::string decodeSymbolsToText(const std::vector<std::uint8_t>& symbols);
int frameLength(const std::string& payloadText);
bool isSupportedPresentationChar(char ch);

}  // namespace hftext
