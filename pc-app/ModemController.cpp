#include "ModemController.h"

#include "hftext_core.h"
#include "hftext_encoder.h"
#include "hftext_frame.h"
#include "hftext_robust.h"
#include "wav_io.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

int packedPayloadBytes(int payloadSymbols) {
    return (payloadSymbols * hftext::kBitsPerSymbol + hftext::kBitsPerByte - 1) / hftext::kBitsPerByte;
}

std::vector<std::uint8_t> preambleBits(int bitCount) {
    if (bitCount < 0) {
        throw std::invalid_argument("preambulo deve ser nao negativo");
    }

    std::vector<std::uint8_t> bits;
    bits.reserve(static_cast<std::size_t>(bitCount));
    for (int index = 0; index < bitCount; ++index) {
        bits.push_back(static_cast<std::uint8_t>(index % 2 == 0 ? 1 : 0));
    }
    return bits;
}

}  // namespace

double ModemController::WavStats::durationSeconds() const {
    if (sampleRate <= 0) {
        return 0.0;
    }
    return static_cast<double>(sampleCount) / static_cast<double>(sampleRate);
}

void ModemController::setConfig(const hftext::ModemConfig& config) {
    config_ = config;
}

const hftext::ModemConfig& ModemController::config() const {
    return config_;
}

std::string ModemController::buildPayload(const std::string& callsign, const std::string& message) const {
    if (message.empty()) {
        throw std::invalid_argument("mensagem vazia");
    }
    if (callsign.empty()) {
        return message;
    }
    return callsign + " " + message;
}

ModemController::TransmissionEstimate ModemController::estimateTransmission(
    const std::string& callsign,
    const std::string& message
) const {
    TransmissionEstimate estimate;
    estimate.maxPayloadSymbols = hftext::kMaxPayloadSymbols;
    if (message.empty()) {
        return estimate;
    }

    estimate.messageEmpty = false;
    estimate.payload = buildPayload(callsign, message);
    estimate.payloadSymbols = hftext::encodedSymbolCount(estimate.payload);
    estimate.payloadTooLong = estimate.payloadSymbols > hftext::kMaxPayloadSymbols;
    estimate.frameBits = (hftext::kHeaderBytes + packedPayloadBytes(estimate.payloadSymbols) + hftext::kCrcBytes)
        * hftext::kBitsPerByte;
    if (!estimate.payloadTooLong) {
        const auto bits = hftext::buildRobustTransmission(estimate.payload, preambleBits(config_.preambleBits));
        estimate.transmissionBits = static_cast<int>(bits.size());
        estimate.frameBits = estimate.transmissionBits - config_.preambleBits;
    } else {
        estimate.transmissionBits = config_.preambleBits + estimate.frameBits;
    }
    estimate.durationSeconds = static_cast<double>(estimate.transmissionBits)
        * static_cast<double>(config_.symbolDurationSec);
    return estimate;
}

void ModemController::generateWav(
    const std::string& callsign,
    const std::string& message,
    const std::string& outputPath
) const {
    const std::string payload = buildPayload(callsign, message);
    const auto audio = hftext::modulateText(payload, config_);
    hftext::tools::writeMonoPcm16Wav(outputPath, audio, config_.sampleRate);
}

ModemController::WavStats ModemController::analyzeWav(const std::string& inputPath) const {
    const auto wav = hftext::tools::readPcm16Wav(inputPath);
    WavStats stats;
    stats.sampleRate = wav.sampleRate;
    stats.sampleCount = wav.samples.size();

    for (float sample : wav.samples) {
        const float absValue = std::abs(sample);
        stats.peak = (std::max)(stats.peak, absValue);
        if (absValue >= 0.98F) {
            ++stats.clippedSamples;
        }
    }

    return stats;
}

hftext::DecodeResult ModemController::decodeWav(const std::string& inputPath) const {
    const auto wav = hftext::tools::readPcm16Wav(inputPath);
    hftext::ModemConfig decodeConfig = config_;
    decodeConfig.sampleRate = wav.sampleRate;
    return hftext::demodulateSamples(wav.samples, decodeConfig);
}
