#include "ModemController.h"

#include "hftext_core.h"
#include "wav_io.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

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
