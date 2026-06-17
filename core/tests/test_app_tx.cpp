#include "hftext_app_tx.h"

#include "hftext_app_settings.h"
#include "hftext_core.h"
#include "hftext_encoder.h"
#include "hftext_frame.h"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    auto profiles = hftext::defaultAppModemProfiles();
    profiles.rxSampleRate = 8000;
    profiles.txSampleRate = 8000;
    profiles.baseFrequencyHz = 1000.0F;
    profiles.toneSpacingHz = 120.0F;
    profiles.slow.symbolDurationSec = 0.01F;
    profiles.fast.symbolDurationSec = 0.01F;
    const auto config = hftext::modemConfigForProfile(profiles, hftext::SpeedProfile::Slow, profiles.txSampleRate);

    assert(hftext::buildTransmitPayload("", "Test") == "Test");
    assert(hftext::buildTransmitPayload("pu5lrk", "Test") == "pu5lrk Test");

    bool rejected = false;
    try {
        (void)hftext::buildTransmitPayload("pu5lrk", "");
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    const auto emptyEstimate = hftext::estimateTransmission("pu5lrk", "", config);
    assert(emptyEstimate.messageEmpty);
    assert(emptyEstimate.maxPayloadSymbols == hftext::kMaxPayloadSymbols);
    assert(emptyEstimate.durationSeconds == 0.0);

    const auto estimate = hftext::estimateTransmission("pu5lrk", "Test", config);
    assert(!estimate.messageEmpty);
    assert(!estimate.payloadTooLong);
    assert(estimate.payload == "pu5lrk Test");
    assert(estimate.payloadSymbols == hftext::encodedSymbolCount(estimate.payload));
    assert(estimate.frameBits > 0);
    assert(estimate.transmissionBits > estimate.frameBits);
    assert(estimate.durationSeconds > 0.0);

    const std::string tooLong(static_cast<std::size_t>(hftext::kMaxPayloadSymbols + 1), 'a');
    const auto longEstimate = hftext::estimateTransmission("", tooLong, config);
    assert(!longEstimate.messageEmpty);
    assert(longEstimate.payloadTooLong);
    assert(longEstimate.payloadSymbols == hftext::kMaxPayloadSymbols + 1);

    hftext::ModemConfig fsk2Default;
    assert(hftext::preambleBitsForConfig(fsk2Default) == hftext::defaultPreambleBits());

    auto fsk8Config = config;
    fsk8Config.preambleBits = 24;
    fsk8Config.modulationMode = hftext::ModulationMode::Fsk8;
    const std::vector<std::uint8_t> expectedFsk8PreamblePrefix{
        0, 0, 0,
        0, 0, 1,
        0, 1, 0,
        0, 1, 1,
        1, 0, 0,
        1, 0, 1,
        1, 1, 0,
        1, 1, 1,
    };
    assert(hftext::preambleBitsForConfig(fsk8Config) == expectedFsk8PreamblePrefix);

    auto audio = hftext::generateTransmitAudio("pu5lrk", "Test", config);
    assert(!audio.empty());
    const auto result = hftext::demodulateSamples(audio, config);
    assert(result.frameDetected);
    assert(result.crcOk);
    assert(result.payloadValid);
    assert(result.text == "pu5lrk Test");
}
