#include "hftext_app_tx.h"

#include "hftext_app_settings.h"
#include "hftext_core.h"
#include "hftext_encoder.h"
#include "hftext_frame.h"
#include "hftext_robust.h"

#include <cstddef>
#include <stdexcept>

namespace hftext {
namespace {

int packedPayloadBytesUnchecked(int payloadSymbols) {
    return (payloadSymbols * kBitsPerSymbol + kBitsPerByte - 1) / kBitsPerByte;
}

}  // namespace

std::string buildTransmitPayload(const std::string& callsign, const std::string& message) {
    if (message.empty()) {
        throw std::invalid_argument("message is empty");
    }
    if (callsign.empty()) {
        return message;
    }
    return callsign + " " + message;
}

std::vector<std::uint8_t> preambleBitsForConfig(const ModemConfig& config) {
    if (config.preambleBits < 0) {
        throw std::invalid_argument("preamble_bits must be non-negative");
    }

    if (config.modulationMode == ModulationMode::Fsk2 && config.preambleBits == kDefaultPreambleBits) {
        return defaultPreambleBits();
    }

    std::vector<std::uint8_t> bits;
    bits.reserve(static_cast<std::size_t>(config.preambleBits));
    if (toneCount(config.modulationMode) > 2) {
        const int bitsPerTone = bitsPerModulationSymbol(config.modulationMode);
        const int tones = toneCount(config.modulationMode);
        for (int index = 0; index < config.preambleBits; ++index) {
            const auto tone = static_cast<std::uint8_t>((index / bitsPerTone) % tones);
            const int bitIndex = bitsPerTone - 1 - (index % bitsPerTone);
            bits.push_back(static_cast<std::uint8_t>((tone >> bitIndex) & 0x01U));
        }
    } else {
        for (int index = 0; index < config.preambleBits; ++index) {
            bits.push_back(static_cast<std::uint8_t>(index % 2 == 0 ? 1 : 0));
        }
    }
    return bits;
}

TransmissionEstimate estimateTransmission(
    const std::string& callsign,
    const std::string& message,
    const ModemConfig& config
) {
    validateAppModemConfig(config);

    TransmissionEstimate estimate;
    estimate.maxPayloadSymbols = kMaxPayloadSymbols;
    if (message.empty()) {
        return estimate;
    }

    estimate.messageEmpty = false;
    estimate.payload = buildTransmitPayload(callsign, message);
    estimate.payloadSymbols = encodedSymbolCount(estimate.payload);
    estimate.payloadTooLong = estimate.payloadSymbols > kMaxPayloadSymbols;
    estimate.frameBits = (kHeaderBytes + packedPayloadBytesUnchecked(estimate.payloadSymbols) + kCrcBytes)
        * kBitsPerByte;
    if (!estimate.payloadTooLong) {
        const auto bits = buildRobustTransmission(estimate.payload, preambleBitsForConfig(config));
        estimate.transmissionBits = static_cast<int>(bits.size());
        estimate.frameBits = estimate.transmissionBits - config.preambleBits;
    } else {
        estimate.transmissionBits = config.preambleBits + estimate.frameBits;
    }

    const int bitsPerAudioSymbol = bitsPerModulationSymbol(config.modulationMode);
    const int physicalSymbols = (estimate.transmissionBits + bitsPerAudioSymbol - 1) / bitsPerAudioSymbol;
    estimate.durationSeconds = static_cast<double>(physicalSymbols) * static_cast<double>(config.symbolDurationSec);
    return estimate;
}

std::vector<float> generateTransmitAudio(
    const std::string& callsign,
    const std::string& message,
    const ModemConfig& config
) {
    validateAppModemConfig(config);
    return modulateText(buildTransmitPayload(callsign, message), config);
}

}  // namespace hftext
