#pragma once

#include "hftext_result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hftext {

struct InterleaveShape {
    int rows = 0;
    int columns = 0;
};

struct ViterbiResult {
    std::vector<std::uint8_t> bits;
    int distance = 0;
};

struct SoftBitDecision {
    std::uint8_t bit = 0;
    float confidence = 1.0F;
};

struct RobustDecodeResult {
    DecodeResult frame;
    InterleaveShape shape;
    int viterbiDistance = 0;
};

InterleaveShape chooseInterleaveShape(
    std::size_t bitCount,
    int preferredRows = 6,
    int minRows = 2,
    int maxRows = 16
);

std::vector<std::uint8_t> interleaveBits(
    const std::vector<std::uint8_t>& bits,
    int rows,
    int columns
);

std::vector<std::uint8_t> deinterleaveBits(
    const std::vector<std::uint8_t>& bits,
    int rows,
    int columns
);

std::vector<std::uint8_t> convolutionalK3EncodeBits(
    const std::vector<std::uint8_t>& bits,
    bool tail = true
);

ViterbiResult convolutionalK3DecodeBits(
    const std::vector<std::uint8_t>& bits,
    int originalBitCount = -1,
    bool tail = true
);

ViterbiResult convolutionalK3DecodeSoftBits(
    const std::vector<SoftBitDecision>& decisions,
    int originalBitCount = -1,
    bool tail = true
);

std::vector<std::uint8_t> buildRobustFrameBits(const std::string& payloadText);

std::vector<std::uint8_t> buildRobustTransmission(const std::string& payloadText);

std::vector<std::uint8_t> buildRobustTransmission(
    const std::string& payloadText,
    const std::vector<std::uint8_t>& preambleBits
);

RobustDecodeResult parseRobustFrameBits(
    const std::vector<std::uint8_t>& bits,
    int originalFrameBitCount
);

RobustDecodeResult parseRobustFrameSoftBits(
    const std::vector<SoftBitDecision>& decisions,
    int originalFrameBitCount
);

RobustDecodeResult parseRobustFrameFromStream(const std::vector<std::uint8_t>& bits);

}  // namespace hftext
