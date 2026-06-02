#include "hftext_crc16.h"

#include <stdexcept>

namespace hftext {

std::uint16_t crc16CcittFalse(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr && size != 0) {
        throw std::invalid_argument("data must not be null when size is non-zero");
    }

    std::uint16_t crc = kCrc16CcittFalseInitial;

    for (std::size_t index = 0; index < size; ++index) {
        crc ^= static_cast<std::uint16_t>(data[index]) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000U) != 0) {
                crc = static_cast<std::uint16_t>((crc << 1) ^ kCrc16CcittFalsePoly);
            } else {
                crc = static_cast<std::uint16_t>(crc << 1);
            }
        }
    }

    return static_cast<std::uint16_t>(crc ^ kCrc16CcittFalseXorOut);
}

std::uint16_t crc16CcittFalse(const std::vector<std::uint8_t>& data) {
    return crc16CcittFalse(data.data(), data.size());
}

}  // namespace hftext
