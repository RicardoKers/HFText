#include "hftext_crc16.h"
#include "hftext_frame.h"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void assertValidText(const hftext::DecodeResult& result, const std::string& expectedText, int expectedLength) {
    assert(result.frameDetected);
    assert(result.crcOk);
    assert(result.payloadValid);
    assert(result.text == expectedText);
    assert(result.length == expectedLength);
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

int main() {
    auto result = hftext::parseFrame(hftext::buildFrame("pu5lrk Teste"));
    assertValidText(result, "pu5lrk Teste", 13);

    const auto frameBits = hftext::buildFrame("abc");
    const auto txBits = hftext::buildTransmission("abc");
    const auto preamble = hftext::defaultPreambleBits();
    assert(std::vector<std::uint8_t>(txBits.begin(), txBits.begin() + preamble.size()) == preamble);
    assert(std::vector<std::uint8_t>(txBits.begin() + preamble.size(), txBits.end()) == frameBits);

    std::vector<std::uint8_t> stream = {0, 0, 0, 1};
    const auto transmission = hftext::buildTransmission("pu5lrk Teste");
    stream.insert(stream.end(), transmission.begin(), transmission.end());
    stream.insert(stream.end(), {1, 1, 1});
    result = hftext::parseFrameFromStream(stream);
    assertValidText(result, "pu5lrk Teste", 13);
    assert(result.syncIndex == static_cast<int>(4 + preamble.size()));

    assert(hftext::findSync({1, 0, 1, 0, 1, 0}) == -1);

    bool invalidBitRejected = false;
    try {
        auto badBits = std::vector<std::uint8_t>{0, 1, 2};
        const auto sync = hftext::syncBits();
        badBits.insert(badBits.end(), sync.begin(), sync.end());
        (void)hftext::findSync(badBits);
    } catch (const std::invalid_argument&) {
        invalidBitRejected = true;
    }
    assert(invalidBitRejected);

    result = hftext::parseFrameFromStream(hftext::syncBits());
    assert(result.frameDetected);
    assert(!result.crcOk);
    assert(result.error == "stream ended before length");

    const auto oneSymbolPayload = hftext::packSymbolsToBytes({1});
    auto expectedFrame = hftext::syncBytes();
    expectedFrame.push_back(1);
    expectedFrame.insert(expectedFrame.end(), oneSymbolPayload.begin(), oneSymbolPayload.end());
    const auto expectedCrc = hftext::crc16CcittFalse(oneSymbolPayload);
    expectedFrame.push_back(static_cast<std::uint8_t>((expectedCrc >> 8) & 0xFF));
    expectedFrame.push_back(static_cast<std::uint8_t>(expectedCrc & 0xFF));
    assert(hftext::buildFrameBytes("a") == expectedFrame);

    result = hftext::parseFrame(hftext::buildFrame(""));
    assertValidText(result, "", 0);

    auto badSync = hftext::buildFrameBytes("abc");
    badSync[0] ^= 0xFF;
    result = hftext::parseFrameBytes(badSync);
    assert(!result.frameDetected);
    assert(result.error == "sync not found");

    auto badLength = hftext::buildFrameBytes("abc");
    badLength[2] = 0x80;
    result = hftext::parseFrameBytes(badLength);
    assert(result.frameDetected);
    assert(!result.crcOk);
    assert(result.error == "length bit 7 set");

    auto shortFrame = hftext::buildFrameBytes("abc");
    shortFrame.pop_back();
    result = hftext::parseFrameBytes(shortFrame);
    assert(result.frameDetected);
    assert(!result.crcOk);
    assert(contains(result.error, "frame size mismatch"));

    auto badCrc = hftext::buildFrameBytes("abc");
    badCrc.back() ^= 0x01;
    result = hftext::parseFrameBytes(badCrc);
    assert(result.frameDetected);
    assert(!result.crcOk);
    assert(!result.payloadValid);
    assert(result.text.empty());
    assert(result.error == "crc mismatch");

    const auto reservedPayload = hftext::packSymbolsToBytes({61});
    auto reservedFrame = hftext::syncBytes();
    reservedFrame.push_back(1);
    reservedFrame.insert(reservedFrame.end(), reservedPayload.begin(), reservedPayload.end());
    const auto reservedCrc = hftext::crc16CcittFalse(reservedPayload);
    reservedFrame.push_back(static_cast<std::uint8_t>((reservedCrc >> 8) & 0xFF));
    reservedFrame.push_back(static_cast<std::uint8_t>(reservedCrc & 0xFF));
    result = hftext::parseFrameBytes(reservedFrame);
    assert(result.frameDetected);
    assert(result.crcOk);
    assert(!result.payloadValid);
    assert(result.text.empty());
    assert(contains(result.error, "invalid symbol"));

    const std::vector<std::uint8_t> syncBytes = {0x2D, 0xD4};
    const auto convertedSyncBits = hftext::bytesToBits(syncBytes);
    assert(std::vector<std::uint8_t>(convertedSyncBits.begin(), convertedSyncBits.begin() + 8)
        == std::vector<std::uint8_t>({0, 0, 1, 0, 1, 1, 0, 1}));
    assert(hftext::bitsToBytes(convertedSyncBits) == syncBytes);

    bool bitCountRejected = false;
    try {
        (void)hftext::bitsToBytes({1, 0, 1});
    } catch (const std::invalid_argument&) {
        bitCountRejected = true;
    }
    assert(bitCountRejected);

    bool bitValueRejected = false;
    try {
        (void)hftext::bitsToBytes({0, 0, 0, 0, 0, 0, 0, 2});
    } catch (const std::invalid_argument&) {
        bitValueRejected = true;
    }
    assert(bitValueRejected);

    assert(hftext::payloadByteCount(0) == 0);
    assert(hftext::payloadByteCount(1) == 1);
    assert(hftext::payloadByteCount(4) == 3);
    assert(hftext::payloadByteCount(127) == 96);

    assert(hftext::packSymbolsToBytes({1, 2, 3, 0}) == std::vector<std::uint8_t>({0x04, 0x20, 0xC0}));
    assert(hftext::unpackSymbolsFromBytes({0x04}, 1) == std::vector<std::uint8_t>({1}));
}
