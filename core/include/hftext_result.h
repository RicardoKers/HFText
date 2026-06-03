#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hftext {

struct DecodeResult {
    bool frameDetected = false;
    bool crcOk = false;
    bool payloadValid = false;
    std::string text;
    std::string error;
    std::int32_t length = 0;
    std::int32_t syncIndex = -1;
    std::int32_t startOffset = 0;
    std::int32_t offsetsTried = 1;
    std::vector<std::uint8_t> payloadSymbols;
    float confidence = 0.0F;
};

}  // namespace hftext
