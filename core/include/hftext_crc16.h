#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace hftext {

constexpr std::uint16_t kCrc16CcittFalsePoly = 0x1021;
constexpr std::uint16_t kCrc16CcittFalseInitial = 0xFFFF;
constexpr std::uint16_t kCrc16CcittFalseXorOut = 0x0000;

std::uint16_t crc16CcittFalse(const std::uint8_t* data, std::size_t size);
std::uint16_t crc16CcittFalse(const std::vector<std::uint8_t>& data);

}  // namespace hftext
