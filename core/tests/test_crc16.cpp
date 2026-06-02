#include "hftext_crc16.h"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> bytesFromString(const std::string& text) {
    return {text.begin(), text.end()};
}

}  // namespace

int main() {
    const auto known = bytesFromString("123456789");
    assert(hftext::crc16CcittFalse(known) == 0x29B1);

    const std::vector<std::uint8_t> empty;
    assert(hftext::crc16CcittFalse(empty) == 0xFFFF);

    const auto original = bytesFromString("hftext");
    const auto changed = bytesFromString("hftexu");
    assert(hftext::crc16CcittFalse(original) != hftext::crc16CcittFalse(changed));

    const std::vector<std::uint8_t> payloadBytes = {0x04, 0x20, 0xC0};
    assert(hftext::crc16CcittFalse(payloadBytes) == hftext::crc16CcittFalse(payloadBytes));

    auto modifiedPayload = payloadBytes;
    modifiedPayload.push_back(0);
    assert(hftext::crc16CcittFalse(payloadBytes) != hftext::crc16CcittFalse(modifiedPayload));

    bool nullRejected = false;
    try {
        (void)hftext::crc16CcittFalse(nullptr, 1);
    } catch (const std::invalid_argument&) {
        nullRejected = true;
    }
    assert(nullRejected);
}
