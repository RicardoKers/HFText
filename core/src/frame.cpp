#include "hftext_frame.h"

#include "hftext_crc16.h"
#include "hftext_encoder.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace hftext {
namespace {

void validateBit(std::uint8_t bit) {
    if (bit != 0 && bit != 1) {
        throw std::invalid_argument("invalid bit");
    }
}

int bitsToInt(const std::vector<std::uint8_t>& bits, std::size_t start, std::size_t count) {
    int value = 0;
    for (std::size_t index = start; index < start + count; ++index) {
        validateBit(bits[index]);
        value = (value << 1) | bits[index];
    }
    return value;
}

std::uint16_t readBigEndianU16(const std::vector<std::uint8_t>& bytes, std::size_t start) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[start]) << 8) | bytes[start + 1]);
}

void appendBigEndianU16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

DecodeResult makeResult(
    bool frameDetected,
    bool crcOk,
    bool payloadValid,
    std::string text,
    int length = 0,
    std::vector<std::uint8_t> payloadSymbols = {},
    std::string error = {},
    int syncIndex = -1
) {
    DecodeResult result;
    result.frameDetected = frameDetected;
    result.crcOk = crcOk;
    result.payloadValid = payloadValid;
    result.text = std::move(text);
    result.length = length;
    result.payloadSymbols = std::move(payloadSymbols);
    result.error = std::move(error);
    result.syncIndex = syncIndex;
    return result;
}

}  // namespace

std::vector<std::uint8_t> syncBytes() {
    return {0x2D, 0xD4};
}

std::vector<std::uint8_t> syncBits() {
    return bytesToBits(syncBytes());
}

std::vector<std::uint8_t> startSyncBits() {
    auto bits = syncBits();
    const auto repeated = syncBits();
    bits.insert(bits.end(), repeated.begin(), repeated.end());
    return bits;
}

std::vector<std::uint8_t> physicalLengthBits(int payloadLength) {
    if (payloadLength < 0 || payloadLength > kMaxPayloadSymbols) {
        throw std::invalid_argument("payload length must be between 0 and 127");
    }

    std::vector<std::uint8_t> bits;
    bits.reserve(kBitsPerByte * kPhysicalLengthRepeat);
    const auto byte = static_cast<std::uint8_t>(payloadLength & 0x7F);
    for (int repeat = 0; repeat < kPhysicalLengthRepeat; ++repeat) {
        for (int shift = kBitsPerByte - 1; shift >= 0; --shift) {
            bits.push_back(static_cast<std::uint8_t>((byte >> shift) & 1U));
        }
    }
    return bits;
}

int decodePhysicalLengthBits(const std::vector<std::uint8_t>& bits, std::size_t start) {
    const auto requiredBits = static_cast<std::size_t>(kBitsPerByte * kPhysicalLengthRepeat);
    if (start + requiredBits > bits.size()) {
        return -1;
    }

    int value = 0;
    for (int bitIndex = 0; bitIndex < kBitsPerByte; ++bitIndex) {
        int ones = 0;
        for (int repeat = 0; repeat < kPhysicalLengthRepeat; ++repeat) {
            const auto bit = bits[start + static_cast<std::size_t>(repeat * kBitsPerByte + bitIndex)];
            validateBit(bit);
            ones += bit;
        }
        value = (value << 1) | (ones >= 2 ? 1 : 0);
    }

    if ((value & 0x80) != 0 || value > kMaxPayloadSymbols) {
        return -1;
    }
    return value;
}

std::vector<std::uint8_t> defaultPreambleBits() {
    std::vector<std::uint8_t> bits;
    bits.reserve(kDefaultPreambleBits);
    for (int index = 0; index < kDefaultPreambleBits; ++index) {
        bits.push_back(static_cast<std::uint8_t>(index % 2 == 0 ? 1 : 0));
    }
    return bits;
}

std::vector<std::uint8_t> bytesToBits(const std::vector<std::uint8_t>& data) {
    std::vector<std::uint8_t> bits;
    bits.reserve(data.size() * kBitsPerByte);
    for (std::uint8_t byte : data) {
        for (int shift = kBitsPerByte - 1; shift >= 0; --shift) {
            bits.push_back(static_cast<std::uint8_t>((byte >> shift) & 1U));
        }
    }
    return bits;
}

std::vector<std::uint8_t> bitsToBytes(const std::vector<std::uint8_t>& bits) {
    if (bits.size() % kBitsPerByte != 0) {
        throw std::invalid_argument("bit count must be a multiple of 8");
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(bits.size() / kBitsPerByte);
    for (std::size_t offset = 0; offset < bits.size(); offset += kBitsPerByte) {
        std::uint8_t byte = 0;
        for (std::size_t index = offset; index < offset + kBitsPerByte; ++index) {
            validateBit(bits[index]);
            byte = static_cast<std::uint8_t>((byte << 1) | bits[index]);
        }
        bytes.push_back(byte);
    }
    return bytes;
}

int payloadByteCount(int length) {
    if (length < 0 || length > kMaxPayloadSymbols) {
        throw std::invalid_argument("length must be between 0 and 127");
    }
    return (length * kBitsPerSymbol + kBitsPerByte - 1) / kBitsPerByte;
}

std::vector<std::uint8_t> packSymbolsToBytes(const std::vector<std::uint8_t>& symbols) {
    std::uint32_t bitBuffer = 0;
    int bitCount = 0;
    std::vector<std::uint8_t> bytes;
    bytes.reserve(payloadByteCount(static_cast<int>(symbols.size())));

    for (std::uint8_t symbol : symbols) {
        if (symbol >= (1U << kBitsPerSymbol)) {
            throw std::invalid_argument("invalid 6-bit symbol");
        }

        bitBuffer = (bitBuffer << kBitsPerSymbol) | symbol;
        bitCount += kBitsPerSymbol;

        while (bitCount >= kBitsPerByte) {
            bitCount -= kBitsPerByte;
            bytes.push_back(static_cast<std::uint8_t>((bitBuffer >> bitCount) & 0xFF));
            bitBuffer &= (1U << bitCount) - 1U;
        }
    }

    if (bitCount != 0) {
        bytes.push_back(static_cast<std::uint8_t>((bitBuffer << (kBitsPerByte - bitCount)) & 0xFF));
    }

    return bytes;
}

std::vector<std::uint8_t> unpackSymbolsFromBytes(const std::vector<std::uint8_t>& data, int symbolCount) {
    if (symbolCount < 0) {
        throw std::invalid_argument("symbol_count must be non-negative");
    }

    const int availableBits = static_cast<int>(data.size()) * kBitsPerByte;
    const int requiredBits = symbolCount * kBitsPerSymbol;
    if (requiredBits > availableBits) {
        throw std::invalid_argument("not enough data for requested symbols");
    }

    std::uint32_t bitBuffer = 0;
    int bitCount = 0;
    std::vector<std::uint8_t> symbols;
    symbols.reserve(static_cast<std::size_t>(symbolCount));

    for (std::uint8_t byte : data) {
        bitBuffer = (bitBuffer << kBitsPerByte) | byte;
        bitCount += kBitsPerByte;

        while (bitCount >= kBitsPerSymbol && static_cast<int>(symbols.size()) < symbolCount) {
            bitCount -= kBitsPerSymbol;
            symbols.push_back(static_cast<std::uint8_t>((bitBuffer >> bitCount) & 0x3F));
            bitBuffer &= (1U << bitCount) - 1U;
        }
    }

    return symbols;
}

std::vector<std::uint8_t> buildFrameBytes(const std::string& payloadText) {
    const auto payloadSymbols = encodeTextToSymbols(payloadText);
    const auto payload = packSymbolsToBytes(payloadSymbols);
    const std::uint16_t crc = crc16CcittFalse(payload);

    std::vector<std::uint8_t> frame = syncBytes();
    frame.push_back(static_cast<std::uint8_t>(payloadSymbols.size()));
    frame.insert(frame.end(), payload.begin(), payload.end());
    appendBigEndianU16(frame, crc);
    return frame;
}

std::vector<std::uint8_t> buildFrame(const std::string& payloadText) {
    return bytesToBits(buildFrameBytes(payloadText));
}

std::vector<std::uint8_t> buildTransmission(const std::string& payloadText) {
    return buildTransmission(payloadText, defaultPreambleBits());
}

std::vector<std::uint8_t> buildTransmission(
    const std::string& payloadText,
    const std::vector<std::uint8_t>& preambleBits
) {
    for (std::uint8_t bit : preambleBits) {
        validateBit(bit);
    }

    auto transmission = preambleBits;
    const auto frame = buildFrame(payloadText);
    transmission.insert(transmission.end(), frame.begin(), frame.end());
    return transmission;
}

DecodeResult parseFrameBytes(const std::vector<std::uint8_t>& frame) {
    if (frame.size() < kHeaderBytes + kCrcBytes) {
        return makeResult(false, false, false, "", 0, {}, "frame too short");
    }

    const auto expectedSync = syncBytes();
    if (!std::equal(expectedSync.begin(), expectedSync.end(), frame.begin())) {
        return makeResult(false, false, false, "", 0, {}, "sync not found");
    }

    const int length = frame[2];
    if ((length & 0x80) != 0) {
        return makeResult(true, false, false, "", length, {}, "length bit 7 set");
    }
    if (length > kMaxPayloadSymbols) {
        return makeResult(true, false, false, "", length, {}, "length too large");
    }

    const int packedSize = payloadByteCount(length);
    const std::size_t expectedSize = static_cast<std::size_t>(kHeaderBytes + packedSize + kCrcBytes);
    if (frame.size() != expectedSize) {
        return makeResult(
            true,
            false,
            false,
            "",
            length,
            {},
            "frame size mismatch: expected " + std::to_string(expectedSize) + ", got " + std::to_string(frame.size())
        );
    }

    const auto payloadStart = frame.begin() + kHeaderBytes;
    const auto payloadEnd = payloadStart + packedSize;
    const std::vector<std::uint8_t> payload(payloadStart, payloadEnd);
    const std::uint16_t receivedCrc = readBigEndianU16(frame, kHeaderBytes + packedSize);
    const std::uint16_t calculatedCrc = crc16CcittFalse(payload);
    if (receivedCrc != calculatedCrc) {
        return makeResult(true, false, false, "", length, {}, "crc mismatch");
    }

    const auto payloadSymbols = unpackSymbolsFromBytes(payload, length);
    try {
        const auto text = decodeSymbolsToText(payloadSymbols);
        return makeResult(true, true, true, text, length, payloadSymbols);
    } catch (const std::invalid_argument& exc) {
        return makeResult(true, true, false, "", length, payloadSymbols, exc.what());
    }
}

DecodeResult parseFrame(const std::vector<std::uint8_t>& bits) {
    return parseFrameBytes(bitsToBytes(bits));
}

DecodeResult parseFrameAtSync(const std::vector<std::uint8_t>& bits, int syncIndex) {
    const std::size_t lengthStart = static_cast<std::size_t>(syncIndex) + syncBits().size();
    const std::size_t lengthEnd = lengthStart + kBitsPerByte;
    if (bits.size() < lengthEnd) {
        return makeResult(true, false, false, "", 0, {}, "stream ended before length", syncIndex);
    }

    const int length = bitsToInt(bits, lengthStart, kBitsPerByte);
    if ((length & 0x80) != 0) {
        return makeResult(true, false, false, "", length, {}, "length bit 7 set", syncIndex);
    }
    if (length > kMaxPayloadSymbols) {
        return makeResult(true, false, false, "", length, {}, "length too large", syncIndex);
    }

    const std::size_t frameBitCount = static_cast<std::size_t>(kHeaderBytes + payloadByteCount(length) + kCrcBytes)
        * kBitsPerByte;
    const std::size_t frameEnd = static_cast<std::size_t>(syncIndex) + frameBitCount;
    if (bits.size() < frameEnd) {
        return makeResult(true, false, false, "", length, {}, "stream ended before complete frame", syncIndex);
    }

    const std::vector<std::uint8_t> frameBits(
        bits.begin() + syncIndex,
        bits.begin() + static_cast<std::ptrdiff_t>(frameEnd)
    );
    auto result = parseFrame(frameBits);
    result.syncIndex = syncIndex;
    return result;
}

int findSync(const std::vector<std::uint8_t>& bits) {
    const auto sync = syncBits();
    if (bits.size() < sync.size()) {
        return -1;
    }

    for (std::uint8_t bit : bits) {
        validateBit(bit);
    }

    const std::size_t lastStart = bits.size() - sync.size();
    for (std::size_t index = 0; index <= lastStart; ++index) {
        if (std::equal(sync.begin(), sync.end(), bits.begin() + static_cast<std::ptrdiff_t>(index))) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

DecodeResult parseFrameFromStream(const std::vector<std::uint8_t>& bits) {
    const auto sync = syncBits();
    if (bits.size() < sync.size()) {
        return makeResult(false, false, false, "", 0, {}, "sync not found");
    }

    for (std::uint8_t bit : bits) {
        validateBit(bit);
    }

    DecodeResult firstCandidate;
    bool hasCandidate = false;
    const std::size_t lastStart = bits.size() - sync.size();
    for (std::size_t index = 0; index <= lastStart; ++index) {
        if (!std::equal(sync.begin(), sync.end(), bits.begin() + static_cast<std::ptrdiff_t>(index))) {
            continue;
        }

        auto result = parseFrameAtSync(bits, static_cast<int>(index));
        if (!hasCandidate) {
            firstCandidate = result;
            hasCandidate = true;
        }
        if (result.crcOk && result.payloadValid) {
            return result;
        }
    }

    if (hasCandidate) {
        return firstCandidate;
    }

    return makeResult(false, false, false, "", 0, {}, "sync not found");
}

}  // namespace hftext
