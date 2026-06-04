#include "hftext_robust.h"

#include "hftext_encoder.h"
#include "hftext_frame.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

namespace hftext {
namespace {

void validateBit(std::uint8_t bit) {
    if (bit != 0 && bit != 1) {
        throw std::invalid_argument("invalid bit");
    }
}

void validateBits(const std::vector<std::uint8_t>& bits) {
    for (std::uint8_t bit : bits) {
        validateBit(bit);
    }
}

void validateDimensions(int rows, int columns) {
    if (rows <= 0) {
        throw std::invalid_argument("rows must be positive");
    }
    if (columns <= 0) {
        throw std::invalid_argument("columns must be positive");
    }
}

std::uint8_t convolutionalK3OutputBit(int reg, int generator) {
    int masked = reg & generator;
    std::uint8_t parity = 0;
    while (masked != 0) {
        parity = static_cast<std::uint8_t>(parity ^ (masked & 1));
        masked >>= 1;
    }
    return parity;
}

std::pair<int, std::vector<std::uint8_t>> convolutionalK3EncodeStep(int state, std::uint8_t bit) {
    const int reg = (static_cast<int>(bit) << 2) | state;
    return {
        reg >> 1,
        {
            convolutionalK3OutputBit(reg, 0b111),
            convolutionalK3OutputBit(reg, 0b101),
        }
    };
}

struct Path {
    int distance = std::numeric_limits<int>::max() / 4;
    std::vector<std::uint8_t> bits;
    bool valid = false;
};

int logicalFrameBitCountForPayloadLength(int payloadLength) {
    return (kHeaderBytes + payloadByteCount(payloadLength) + kCrcBytes) * kBitsPerByte;
}

std::size_t encodedBitCountForLogicalFrameBits(int logicalFrameBitCount) {
    constexpr int kConvTailBits = 2;
    constexpr int kConvOutputBitsPerInputBit = 2;
    return static_cast<std::size_t>(logicalFrameBitCount + kConvTailBits) * kConvOutputBitsPerInputBit;
}

}  // namespace

InterleaveShape chooseInterleaveShape(std::size_t bitCount, int preferredRows, int minRows, int maxRows) {
    if (bitCount == 0) {
        throw std::invalid_argument("bit_count must be positive");
    }
    if (preferredRows <= 0) {
        throw std::invalid_argument("preferred_rows must be positive");
    }
    if (minRows <= 0) {
        throw std::invalid_argument("min_rows must be positive");
    }
    if (maxRows < minRows) {
        throw std::invalid_argument("max_rows must be greater than or equal to min_rows");
    }

    int bestRows = 0;
    for (int rows = minRows; rows <= maxRows; ++rows) {
        if (bitCount % static_cast<std::size_t>(rows) != 0) {
            continue;
        }
        if (
            bestRows == 0
            || std::abs(rows - preferredRows) < std::abs(bestRows - preferredRows)
            || (
                std::abs(rows - preferredRows) == std::abs(bestRows - preferredRows)
                && rows < bestRows
            )
        ) {
            bestRows = rows;
        }
    }

    if (bestRows == 0) {
        throw std::invalid_argument("no interleaving shape exactly fits bit_count");
    }

    return {bestRows, static_cast<int>(bitCount / static_cast<std::size_t>(bestRows))};
}

std::vector<std::uint8_t> interleaveBits(const std::vector<std::uint8_t>& bits, int rows, int columns) {
    validateDimensions(rows, columns);
    validateBits(bits);

    const std::size_t blockSize = static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns);
    if (bits.size() % blockSize != 0) {
        throw std::invalid_argument("bit count must be a multiple of rows * columns");
    }

    std::vector<std::uint8_t> output;
    output.reserve(bits.size());
    for (std::size_t blockStart = 0; blockStart < bits.size(); blockStart += blockSize) {
        for (int column = 0; column < columns; ++column) {
            for (int row = 0; row < rows; ++row) {
                output.push_back(bits[blockStart + static_cast<std::size_t>(row * columns + column)]);
            }
        }
    }
    return output;
}

std::vector<std::uint8_t> deinterleaveBits(const std::vector<std::uint8_t>& bits, int rows, int columns) {
    validateDimensions(rows, columns);
    validateBits(bits);

    const std::size_t blockSize = static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns);
    if (bits.size() % blockSize != 0) {
        throw std::invalid_argument("bit count must be a multiple of rows * columns");
    }

    std::vector<std::uint8_t> output;
    output.reserve(bits.size());
    for (std::size_t blockStart = 0; blockStart < bits.size(); blockStart += blockSize) {
        std::vector<std::uint8_t> restored(blockSize, 0);
        std::size_t index = 0;
        for (int column = 0; column < columns; ++column) {
            for (int row = 0; row < rows; ++row) {
                restored[static_cast<std::size_t>(row * columns + column)] = bits[blockStart + index];
                ++index;
            }
        }
        output.insert(output.end(), restored.begin(), restored.end());
    }
    return output;
}

std::vector<std::uint8_t> convolutionalK3EncodeBits(const std::vector<std::uint8_t>& bits, bool tail) {
    validateBits(bits);

    int state = 0;
    std::vector<std::uint8_t> inputBits = bits;
    if (tail) {
        inputBits.push_back(0);
        inputBits.push_back(0);
    }

    std::vector<std::uint8_t> output;
    output.reserve(inputBits.size() * 2);
    for (std::uint8_t bit : inputBits) {
        const auto step = convolutionalK3EncodeStep(state, bit);
        state = step.first;
        output.insert(output.end(), step.second.begin(), step.second.end());
    }
    return output;
}

ViterbiResult convolutionalK3DecodeBits(const std::vector<std::uint8_t>& bits, int originalBitCount, bool tail) {
    validateBits(bits);
    if (bits.size() % 2 != 0) {
        throw std::invalid_argument("bit count must be a multiple of 2");
    }
    if (originalBitCount < -1) {
        throw std::invalid_argument("original_bit_count must be non-negative");
    }

    std::vector<Path> paths(4);
    paths[0].distance = 0;
    paths[0].valid = true;

    for (std::size_t offset = 0; offset < bits.size(); offset += 2) {
        std::vector<Path> nextPaths(4);
        for (int state = 0; state < 4; ++state) {
            if (!paths[state].valid) {
                continue;
            }
            for (std::uint8_t bit : {std::uint8_t{0}, std::uint8_t{1}}) {
                const auto step = convolutionalK3EncodeStep(state, bit);
                const int nextState = step.first;
                const int branchDistance =
                    (bits[offset] ^ step.second[0]) + (bits[offset + 1] ^ step.second[1]);
                const int candidateDistance = paths[state].distance + branchDistance;
                if (!nextPaths[nextState].valid || candidateDistance < nextPaths[nextState].distance) {
                    nextPaths[nextState].distance = candidateDistance;
                    nextPaths[nextState].bits = paths[state].bits;
                    nextPaths[nextState].bits.push_back(bit);
                    nextPaths[nextState].valid = true;
                }
            }
        }
        paths = std::move(nextPaths);
    }

    Path bestPath;
    if (tail && paths[0].valid) {
        bestPath = paths[0];
    } else {
        const auto best = std::min_element(paths.begin(), paths.end(), [](const Path& left, const Path& right) {
            if (!left.valid) {
                return false;
            }
            if (!right.valid) {
                return true;
            }
            return left.distance < right.distance;
        });
        if (best == paths.end() || !best->valid) {
            throw std::invalid_argument("no valid viterbi path");
        }
        bestPath = *best;
    }

    if (tail && bestPath.bits.size() >= 2) {
        bestPath.bits.resize(bestPath.bits.size() - 2);
    }
    if (originalBitCount >= 0 && static_cast<std::size_t>(originalBitCount) < bestPath.bits.size()) {
        bestPath.bits.resize(static_cast<std::size_t>(originalBitCount));
    }

    return {bestPath.bits, bestPath.distance};
}

std::vector<std::uint8_t> buildRobustFrameBits(const std::string& payloadText) {
    const auto frameBits = buildFrame(payloadText);
    const auto encodedBits = convolutionalK3EncodeBits(frameBits);
    const auto shape = chooseInterleaveShape(encodedBits.size());
    return interleaveBits(encodedBits, shape.rows, shape.columns);
}

std::vector<std::uint8_t> buildRobustTransmission(const std::string& payloadText) {
    return buildRobustTransmission(payloadText, defaultPreambleBits());
}

std::vector<std::uint8_t> buildRobustTransmission(
    const std::string& payloadText,
    const std::vector<std::uint8_t>& preambleBits
) {
    validateBits(preambleBits);

    auto transmission = preambleBits;
    const auto startSync = syncBits();
    transmission.insert(transmission.end(), startSync.begin(), startSync.end());
    const auto frameBits = buildRobustFrameBits(payloadText);
    transmission.insert(transmission.end(), frameBits.begin(), frameBits.end());
    return transmission;
}

RobustDecodeResult parseRobustFrameBits(const std::vector<std::uint8_t>& bits, int originalFrameBitCount) {
    if (originalFrameBitCount < 0) {
        throw std::invalid_argument("original_frame_bit_count must be non-negative");
    }

    validateBits(bits);
    const auto shape = chooseInterleaveShape(bits.size());
    const auto deinterleaved = deinterleaveBits(bits, shape.rows, shape.columns);
    const auto decoded = convolutionalK3DecodeBits(deinterleaved, originalFrameBitCount);

    RobustDecodeResult result;
    result.frame = parseFrame(decoded.bits);
    result.shape = shape;
    result.viterbiDistance = decoded.distance;
    return result;
}

RobustDecodeResult parseRobustFrameFromStream(const std::vector<std::uint8_t>& bits) {
    validateBits(bits);

    RobustDecodeResult firstCandidate;
    bool hasCandidate = false;
    const auto startSync = syncBits();
    if (bits.size() < startSync.size()) {
        DecodeResult result;
        result.error = "robust frame not found";
        return {result, {}, 0};
    }

    const std::size_t lastSyncStart = bits.size() - startSync.size();
    for (std::size_t syncStart = 0; syncStart <= lastSyncStart; ++syncStart) {
        if (!std::equal(startSync.begin(), startSync.end(), bits.begin() + static_cast<std::ptrdiff_t>(syncStart))) {
            continue;
        }

        const std::size_t offset = syncStart + startSync.size();
        for (int payloadLength = 0; payloadLength <= kMaxPayloadSymbols; ++payloadLength) {
            const int logicalFrameBitCount = logicalFrameBitCountForPayloadLength(payloadLength);
            const std::size_t encodedBitCount = encodedBitCountForLogicalFrameBits(logicalFrameBitCount);
            if (offset + encodedBitCount > bits.size()) {
                continue;
            }

            const std::vector<std::uint8_t> candidateBits(
                bits.begin() + static_cast<std::ptrdiff_t>(offset),
                bits.begin() + static_cast<std::ptrdiff_t>(offset + encodedBitCount)
            );
            auto result = parseRobustFrameBits(candidateBits, logicalFrameBitCount);
            result.frame.syncIndex = static_cast<int>(offset);
            if (!hasCandidate) {
                firstCandidate = result;
                hasCandidate = true;
            }
            if (result.frame.crcOk && result.frame.payloadValid && result.frame.length == payloadLength) {
                return result;
            }
        }
    }

    if (hasCandidate) {
        firstCandidate.frame.frameDetected = false;
        firstCandidate.frame.crcOk = false;
        firstCandidate.frame.payloadValid = false;
        firstCandidate.frame.text.clear();
        firstCandidate.frame.error = "robust frame not found";
        firstCandidate.frame.syncIndex = -1;
        return firstCandidate;
    }

    RobustDecodeResult result;
    result.frame.error = "robust frame not found";
    result.frame.syncIndex = -1;
    return result;
}

}  // namespace hftext
