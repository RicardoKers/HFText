#include "hftext_demodulator.h"
#include "hftext_frame.h"
#include "hftext_modulator.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

float deterministicNoise(std::uint32_t& state, float amplitude) {
    state = state * 1664525U + 1013904223U;
    const float normalized = static_cast<float>((state >> 8) & 0xFFFFU) / 32767.5F - 1.0F;
    return normalized * amplitude;
}

std::vector<float> sineTone(int sampleRate, int sampleCount, float frequencyHz) {
    std::vector<float> samples;
    samples.reserve(static_cast<std::size_t>(sampleCount));
    for (int index = 0; index < sampleCount; ++index) {
        const double t = static_cast<double>(index) / sampleRate;
        samples.push_back(static_cast<float>(std::sin(2.0 * kPi * frequencyHz * t)));
    }
    return samples;
}

}  // namespace

int main() {
    const auto tone = sineTone(8000, 800, 1000.0F);
    assert(hftext::toneEnergy(tone, 8000, 1000.0F) > hftext::toneEnergy(tone, 8000, 2000.0F));

    const std::vector<std::uint8_t> bits = {0, 1, 1, 0, 1, 0};
    auto audio = hftext::modulateBits2Fsk(bits, 8000, 0.05F, 1000.0F, 2000.0F, 0.8F);
    auto decoded = hftext::demodulateBits2Fsk(audio, 8000, 0.05F, 1000.0F, 2000.0F);
    assert(decoded == bits);

    const auto decisions = hftext::demodulateBitDecisions2Fsk(audio, 8000, 0.05F, 1000.0F, 2000.0F);
    assert(decisions.size() == bits.size());
    for (std::size_t index = 0; index < decisions.size(); ++index) {
        assert(decisions[index].bit == bits[index]);
        assert(decisions[index].confidence > 0.9F);
        assert(decisions[index].quality > 0.9F);
    }
    const auto silentDecisions = hftext::demodulateBitDecisions2Fsk(
        std::vector<float>(audio.size(), 0.0F),
        8000,
        0.05F,
        1000.0F,
        2000.0F
    );
    for (const auto& decision : silentDecisions) {
        assert(decision.confidence == 0.0F);
        assert(decision.quality == 0.0F);
    }

    const std::vector<std::uint8_t> fsk4Bits = {0, 0, 0, 1, 1, 0, 1, 1};
    audio = hftext::modulateBits4Fsk(fsk4Bits, 8000, 0.05F, 1000.0F, 1200.0F, 0.8F);
    decoded = hftext::demodulateBits4Fsk(audio, 8000, 0.05F, 1000.0F, 1200.0F);
    assert(decoded == fsk4Bits);
    const auto fsk4Decisions = hftext::demodulateBitDecisions4Fsk(audio, 8000, 0.05F, 1000.0F, 1200.0F);
    assert(fsk4Decisions.size() == fsk4Bits.size());
    for (std::size_t index = 0; index < fsk4Decisions.size(); ++index) {
        assert(fsk4Decisions[index].bit == fsk4Bits[index]);
        assert(fsk4Decisions[index].confidence > 0.9F);
        assert(fsk4Decisions[index].quality > 0.9F);
    }

    const std::vector<std::uint8_t> fsk8Bits = {
        0, 0, 0,
        0, 0, 1,
        0, 1, 0,
        0, 1, 1,
        1, 0, 0,
        1, 0, 1,
        1, 1, 0,
        1, 1, 1,
    };
    audio = hftext::modulateBits8Fsk(fsk8Bits, 8000, 0.05F, 1000.0F, 1200.0F, 0.8F);
    decoded = hftext::demodulateBits8Fsk(audio, 8000, 0.05F, 1000.0F, 1200.0F);
    assert(decoded == fsk8Bits);
    const auto fsk8Decisions = hftext::demodulateBitDecisions8Fsk(audio, 8000, 0.05F, 1000.0F, 1200.0F);
    assert(fsk8Decisions.size() == fsk8Bits.size());
    for (std::size_t index = 0; index < fsk8Decisions.size(); ++index) {
        assert(fsk8Decisions[index].bit == fsk8Bits[index]);
        assert(fsk8Decisions[index].confidence > 0.9F);
        assert(fsk8Decisions[index].quality > 0.9F);
    }

    std::uint32_t noiseState = 0xA53C19D2U;
    std::vector<float> noiseOnly(800);
    for (auto& sample : noiseOnly) {
        sample = deterministicNoise(noiseState, 0.5F);
    }
    const auto noiseDecisions = hftext::demodulateBitDecisions2Fsk(
        noiseOnly,
        8000,
        0.1F,
        1000.0F,
        2000.0F
    );
    assert(noiseDecisions.size() == 1);
    assert(noiseDecisions.front().quality < 0.1F);

    audio = hftext::modulateBits2Fsk({0, 1}, 8000, 0.05F, 1200.0F, 1600.0F, 0.8F);
    audio.insert(audio.end(), 10, 0.0F);
    decoded = hftext::demodulateBits2Fsk(audio, 8000, 0.05F, 1200.0F, 1600.0F);
    assert(decoded == std::vector<std::uint8_t>({0, 1}));

    audio = hftext::modulateBits2Fsk({1, 0, 1}, 8000, 0.01F, 1200.0F, 1600.0F, 0.8F);
    std::vector<float> shifted(13, 0.0F);
    shifted.insert(shifted.end(), audio.begin(), audio.end());
    decoded = hftext::demodulateBits2Fsk(shifted, 8000, 0.01F, 1200.0F, 1600.0F, 13);
    assert(decoded == std::vector<std::uint8_t>({1, 0, 1}));

    assert(hftext::demodulateBits2Fsk({}, 48000, 0.5F, 1200.0F, 1600.0F).empty());
    assert(hftext::demodulateBits2Fsk(audio, 8000, 0.01F, 1200.0F, 1600.0F, 999999).empty());

    bool invalidRejected = false;
    try {
        (void)hftext::demodulateBits2Fsk(audio, 0, 0.01F, 1200.0F, 1600.0F);
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);

    invalidRejected = false;
    try {
        (void)hftext::demodulateBits2Fsk(audio, 8000, 0.0F, 1200.0F, 1600.0F);
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);

    invalidRejected = false;
    try {
        (void)hftext::demodulateBits2Fsk(audio, 8000, 0.01F, 1200.0F, 0.0F);
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);

    invalidRejected = false;
    try {
        (void)hftext::demodulateBits2Fsk(audio, 8000, 0.01F, 1200.0F, 1600.0F, -1);
    } catch (const std::invalid_argument&) {
        invalidRejected = true;
    }
    assert(invalidRejected);

    const auto frameBits = hftext::buildFrame("pu5lrk Teste");
    audio = hftext::modulateBits2Fsk(frameBits, 8000, 0.01F, 1000.0F, 2000.0F, 0.8F);
    decoded = hftext::demodulateBits2Fsk(audio, 8000, 0.01F, 1000.0F, 2000.0F);
    const auto result = hftext::parseFrame(decoded);
    assert(decoded == frameBits);
    assert(result.crcOk);
    assert(result.payloadValid);
    assert(result.text == "pu5lrk Teste");
}
