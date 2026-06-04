#pragma once

#include "hftext_result.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace hftext {

constexpr std::uint16_t kSyncValue = 0x2DD4;
constexpr int kBitsPerByte = 8;
constexpr int kBitsPerSymbol = 6;
constexpr int kCrcBytes = 2;
constexpr int kHeaderBytes = 3;
constexpr int kDefaultPreambleBits = 64;
constexpr int kPhysicalLengthRepeat = 3;

std::vector<std::uint8_t> syncBytes();
std::vector<std::uint8_t> syncBits();
std::vector<std::uint8_t> startSyncBits();
std::vector<std::uint8_t> physicalLengthBits(int payloadLength);
int decodePhysicalLengthBits(const std::vector<std::uint8_t>& bits, std::size_t start);
std::vector<std::uint8_t> defaultPreambleBits();

std::vector<std::uint8_t> bytesToBits(const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> bitsToBytes(const std::vector<std::uint8_t>& bits);

int payloadByteCount(int length);
std::vector<std::uint8_t> packSymbolsToBytes(const std::vector<std::uint8_t>& symbols);
std::vector<std::uint8_t> unpackSymbolsFromBytes(const std::vector<std::uint8_t>& data, int symbolCount);

std::vector<std::uint8_t> buildFrameBytes(const std::string& payloadText);
std::vector<std::uint8_t> buildFrame(const std::string& payloadText);
std::vector<std::uint8_t> buildTransmission(const std::string& payloadText);
std::vector<std::uint8_t> buildTransmission(
    const std::string& payloadText,
    const std::vector<std::uint8_t>& preambleBits
);

DecodeResult parseFrameBytes(const std::vector<std::uint8_t>& frame);
DecodeResult parseFrame(const std::vector<std::uint8_t>& bits);
int findSync(const std::vector<std::uint8_t>& bits);
DecodeResult parseFrameFromStream(const std::vector<std::uint8_t>& bits);

}  // namespace hftext
