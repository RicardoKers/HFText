#pragma once

#include "hftext_config.h"

#include <cstdint>
#include <string>
#include <vector>

namespace hftext {

struct TransmissionEstimate {
    bool messageEmpty = true;
    bool payloadTooLong = false;
    int payloadSymbols = 0;
    int maxPayloadSymbols = 0;
    int frameBits = 0;
    int transmissionBits = 0;
    double durationSeconds = 0.0;
    std::string payload;
};

std::string buildTransmitPayload(const std::string& callsign, const std::string& message);
std::vector<std::uint8_t> preambleBitsForConfig(const ModemConfig& config);
TransmissionEstimate estimateTransmission(
    const std::string& callsign,
    const std::string& message,
    const ModemConfig& config
);
std::vector<float> generateTransmitAudio(
    const std::string& callsign,
    const std::string& message,
    const ModemConfig& config
);

}  // namespace hftext
