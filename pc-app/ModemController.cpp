#include "ModemController.h"

#include "hftext_core.h"
#include "wav_io.h"

#include <stdexcept>

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

void ModemController::generateWav(
    const std::string& callsign,
    const std::string& message,
    const std::string& outputPath
) const {
    const std::string payload = buildPayload(callsign, message);
    const auto audio = hftext::modulateText(payload, config_);
    hftext::tools::writeMonoPcm16Wav(outputPath, audio, config_.sampleRate);
}

hftext::DecodeResult ModemController::decodeWav(const std::string& inputPath) const {
    const auto wav = hftext::tools::readPcm16Wav(inputPath);
    hftext::ModemConfig decodeConfig = config_;
    decodeConfig.sampleRate = wav.sampleRate;
    return hftext::demodulateSamples(wav.samples, decodeConfig);
}
