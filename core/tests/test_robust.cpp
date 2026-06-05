#include "hftext_frame.h"
#include "hftext_encoder.h"
#include "hftext_robust.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

bool throwsInvalidArgument(void (*fn)()) {
    try {
        fn();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

}  // namespace

int main() {
    assert(hftext::interleaveBits({0, 1, 1, 0, 1, 0}, 2, 3) == std::vector<std::uint8_t>({0, 0, 1, 1, 1, 0}));

    const std::vector<std::uint8_t> bits = {0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1};
    const auto interleaved = hftext::interleaveBits(bits, 3, 2);
    assert(hftext::deinterleaveBits(interleaved, 3, 2) == bits);

    auto burst = std::vector<std::uint8_t>(16, 0);
    auto spread = hftext::interleaveBits(burst, 4, 4);
    spread[0] = 1;
    spread[1] = 1;
    assert(
        hftext::deinterleaveBits(spread, 4, 4)
        == std::vector<std::uint8_t>({1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0})
    );

    assert(hftext::chooseInterleaveShape(340).rows == 5);
    assert(hftext::chooseInterleaveShape(340).columns == 68);
    assert(hftext::chooseInterleaveShape(372).rows == 6);
    assert(hftext::chooseInterleaveShape(372).columns == 62);
    assert(hftext::chooseInterleaveShape(612).rows == 6);
    assert(hftext::chooseInterleaveShape(612).columns == 102);
    assert(hftext::chooseInterleaveShape(372, 4).rows == 4);
    assert(hftext::chooseInterleaveShape(372, 4).columns == 93);

    assert(
        hftext::convolutionalK3EncodeBits({1, 0, 1})
        == std::vector<std::uint8_t>({1, 1, 1, 0, 0, 0, 1, 0, 1, 1})
    );

    const std::vector<std::uint8_t> sourceBits = {1, 0, 1, 1, 0, 0, 1};
    auto encoded = hftext::convolutionalK3EncodeBits(sourceBits);
    auto decoded = hftext::convolutionalK3DecodeBits(encoded, static_cast<int>(sourceBits.size()));
    assert(decoded.bits == sourceBits);
    assert(decoded.distance == 0);

    std::vector<std::uint8_t> noisy = hftext::convolutionalK3EncodeBits({1, 0, 1, 1, 0, 0, 1, 0, 1});
    noisy[1] ^= 1;
    noisy[8] ^= 1;
    decoded = hftext::convolutionalK3DecodeBits(noisy, 9);
    assert(decoded.bits == std::vector<std::uint8_t>({1, 0, 1, 1, 0, 0, 1, 0, 1}));
    assert(decoded.distance == 2);

    const std::vector<std::uint8_t> softSourceBits = {0, 0, 0, 0, 0, 1};
    auto softEncoded = hftext::convolutionalK3EncodeBits(softSourceBits);
    for (std::size_t index : {0U, 1U, 2U}) {
        softEncoded[index] = static_cast<std::uint8_t>(softEncoded[index] ^ 1U);
    }
    decoded = hftext::convolutionalK3DecodeBits(softEncoded, static_cast<int>(softSourceBits.size()));
    assert(decoded.bits != softSourceBits);

    std::vector<hftext::SoftBitDecision> softDecisions;
    softDecisions.reserve(softEncoded.size());
    for (std::size_t index = 0; index < softEncoded.size(); ++index) {
        const bool weakDecision = index == 0U || index == 1U || index == 2U;
        softDecisions.push_back({softEncoded[index], weakDecision ? 0.025F : 1.0F});
    }
    decoded = hftext::convolutionalK3DecodeSoftBits(softDecisions, static_cast<int>(softSourceBits.size()));
    assert(decoded.bits == softSourceBits);
    assert(decoded.distance > 0);

    const auto frameBits = hftext::buildFrame("pu5lrk cq");
    noisy = hftext::convolutionalK3EncodeBits(frameBits);
    for (std::size_t index = 3; index < noisy.size(); index += 31) {
        noisy[index] ^= 1;
    }
    decoded = hftext::convolutionalK3DecodeBits(noisy, static_cast<int>(frameBits.size()));
    const auto result = hftext::parseFrame(decoded.bits);
    assert(decoded.distance > 0);
    assert(result.crcOk);
    assert(result.payloadValid);
    assert(result.text == "pu5lrk cq");

    const auto robustBits = hftext::buildRobustFrameBits("pu5lrk Teste");
    const auto logicalFrameBits = hftext::buildFrame("pu5lrk Teste");
    assert(robustBits.size() == (logicalFrameBits.size() + 2) * 2);
    auto robustResult = hftext::parseRobustFrameBits(robustBits, static_cast<int>(logicalFrameBits.size()));
    const auto expectedShape = hftext::chooseInterleaveShape((logicalFrameBits.size() + 2) * 2);
    assert(robustResult.frame.crcOk);
    assert(robustResult.frame.payloadValid);
    assert(robustResult.frame.text == "pu5lrk Teste");
    assert(robustResult.shape.rows == expectedShape.rows);
    assert(robustResult.shape.columns == expectedShape.columns);
    assert(robustResult.viterbiDistance == 0);

    auto noisyRobustBits = robustBits;
    for (std::size_t index = 5; index < noisyRobustBits.size(); index += 47) {
        noisyRobustBits[index] ^= 1;
    }
    robustResult = hftext::parseRobustFrameBits(noisyRobustBits, static_cast<int>(logicalFrameBits.size()));
    assert(robustResult.frame.crcOk);
    assert(robustResult.frame.payloadValid);
    assert(robustResult.frame.text == "pu5lrk Teste");
    assert(robustResult.viterbiDistance > 0);

    std::vector<hftext::SoftBitDecision> softRobustBits;
    softRobustBits.reserve(robustBits.size());
    for (std::size_t index = 0; index < robustBits.size(); ++index) {
        softRobustBits.push_back({robustBits[index], 1.0F});
    }
    for (std::size_t index : {5U, 6U, 7U}) {
        softRobustBits[index].bit = static_cast<std::uint8_t>(softRobustBits[index].bit ^ 1U);
        softRobustBits[index].confidence = 0.025F;
    }
    robustResult = hftext::parseRobustFrameSoftBits(softRobustBits, static_cast<int>(logicalFrameBits.size()));
    assert(robustResult.frame.crcOk);
    assert(robustResult.frame.payloadValid);
    assert(robustResult.frame.text == "pu5lrk Teste");
    assert(robustResult.viterbiDistance > 0);

    const auto robustTransmission = hftext::buildRobustTransmission("pu5lrk Teste");
    const auto preamble = hftext::defaultPreambleBits();
    const auto startSync = hftext::startSyncBits();
    const auto physicalLength = hftext::physicalLengthBits(static_cast<int>(hftext::encodeTextToSymbols("pu5lrk Teste").size()));
    assert(
        std::vector<std::uint8_t>(robustTransmission.begin(), robustTransmission.begin() + preamble.size())
        == preamble
    );
    assert(
        std::vector<std::uint8_t>(
            robustTransmission.begin() + static_cast<std::ptrdiff_t>(preamble.size()),
            robustTransmission.begin() + static_cast<std::ptrdiff_t>(preamble.size() + startSync.size())
        )
        == startSync
    );
    assert(
        std::vector<std::uint8_t>(
            robustTransmission.begin() + static_cast<std::ptrdiff_t>(preamble.size() + startSync.size()),
            robustTransmission.begin() + static_cast<std::ptrdiff_t>(preamble.size() + startSync.size() + physicalLength.size())
        )
        == physicalLength
    );
    assert(
        std::vector<std::uint8_t>(
            robustTransmission.begin() + static_cast<std::ptrdiff_t>(preamble.size() + startSync.size() + physicalLength.size()),
            robustTransmission.end()
        )
        == robustBits
    );

    robustResult = hftext::parseRobustFrameFromStream(robustTransmission);
    assert(robustResult.frame.crcOk);
    assert(robustResult.frame.payloadValid);
    assert(robustResult.frame.text == "pu5lrk Teste");
    assert(robustResult.frame.syncIndex == static_cast<int>(preamble.size() + startSync.size() + physicalLength.size()));

    const std::vector<std::uint8_t> customPreamble = {1, 0, 0, 1, 1, 0};
    const auto customTransmission = hftext::buildRobustTransmission("Ok", customPreamble);
    const auto customPhysicalLength = hftext::physicalLengthBits(static_cast<int>(hftext::encodeTextToSymbols("Ok").size()));
    robustResult = hftext::parseRobustFrameFromStream(customTransmission);
    assert(robustResult.frame.crcOk);
    assert(robustResult.frame.payloadValid);
    assert(robustResult.frame.text == "Ok");
    assert(robustResult.frame.syncIndex == static_cast<int>(customPreamble.size() + startSync.size() + customPhysicalLength.size()));

    std::vector<std::uint8_t> noisyStream = {0, 0, 1, 1, 0};
    noisyStream.insert(noisyStream.end(), robustTransmission.begin(), robustTransmission.end());
    noisyStream.insert(noisyStream.end(), {1, 1, 0});
    robustResult = hftext::parseRobustFrameFromStream(noisyStream);
    assert(robustResult.frame.crcOk);
    assert(robustResult.frame.payloadValid);
    assert(robustResult.frame.text == "pu5lrk Teste");
    assert(robustResult.frame.syncIndex == static_cast<int>(5 + preamble.size() + startSync.size() + physicalLength.size()));

    auto damagedStartSyncStream = robustTransmission;
    for (std::size_t index : {preamble.size(), preamble.size() + 3, preamble.size() + 8, preamble.size() + 15, preamble.size() + 24}) {
        damagedStartSyncStream[index] = static_cast<std::uint8_t>(damagedStartSyncStream[index] ^ 1U);
    }
    robustResult = hftext::parseRobustFrameFromStream(damagedStartSyncStream);
    assert(robustResult.frame.crcOk);
    assert(robustResult.frame.payloadValid);
    assert(robustResult.frame.text == "pu5lrk Teste");
    assert(robustResult.frame.syncIndex == static_cast<int>(preamble.size() + startSync.size() + physicalLength.size()));

    robustResult = hftext::parseRobustFrameFromStream({0, 1, 0, 1});
    assert(!robustResult.frame.frameDetected);
    assert(!robustResult.frame.crcOk);
    assert(!robustResult.frame.payloadValid);
    assert(robustResult.frame.error == "robust frame not found");

    assert(throwsInvalidArgument([] { (void)hftext::interleaveBits({0}, 0, 1); }));
    assert(throwsInvalidArgument([] { (void)hftext::deinterleaveBits({0}, 1, 0); }));
    assert(throwsInvalidArgument([] { (void)hftext::interleaveBits({0, 2}, 1, 2); }));
    assert(throwsInvalidArgument([] { (void)hftext::deinterleaveBits({0, 1, 0}, 2, 2); }));
    assert(throwsInvalidArgument([] { (void)hftext::chooseInterleaveShape(0); }));
    assert(throwsInvalidArgument([] { (void)hftext::chooseInterleaveShape(17, 6, 2, 4); }));
    assert(throwsInvalidArgument([] { (void)hftext::convolutionalK3EncodeBits({2}); }));
    assert(throwsInvalidArgument([] { (void)hftext::convolutionalK3DecodeBits({0, 2}); }));
    assert(throwsInvalidArgument([] { (void)hftext::convolutionalK3DecodeBits({0}); }));
    assert(throwsInvalidArgument([] { (void)hftext::convolutionalK3DecodeBits({0, 0}, -2); }));
    assert(throwsInvalidArgument([] { (void)hftext::convolutionalK3DecodeSoftBits({{0, 1.5F}, {0, 1.0F}}); }));
    assert(throwsInvalidArgument([] { (void)hftext::convolutionalK3DecodeSoftBits({{0, 1.0F}}); }));
    assert(throwsInvalidArgument([] { (void)hftext::parseRobustFrameBits({0, 0}, -1); }));
    assert(throwsInvalidArgument([] { (void)hftext::parseRobustFrameBits({0, 2}, 1); }));
    assert(throwsInvalidArgument([] { (void)hftext::parseRobustFrameSoftBits({{0, 1.0F}, {0, 1.0F}}, -1); }));
    assert(throwsInvalidArgument([] { (void)hftext::buildRobustTransmission("Ok", {0, 2}); }));
    assert(throwsInvalidArgument([] { (void)hftext::parseRobustFrameFromStream({0, 2}); }));
}
