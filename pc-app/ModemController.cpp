#include "ModemController.h"

#include "hftext_audio_stats.h"
#include "hftext_core.h"
#include "wav_io.h"

double ModemController::WavStats::durationSeconds() const {
    return hftext::audioDurationSeconds(sampleCount, sampleRate);
}

void ModemController::setConfig(const hftext::ModemConfig& config) {
    config_ = config;
}

const hftext::ModemConfig& ModemController::config() const {
    return config_;
}

std::string ModemController::buildPayload(const std::string& callsign, const std::string& message) const {
    return hftext::buildTransmitPayload(callsign, message);
}

hftext::TransmissionEstimate ModemController::estimateTransmission(
    const std::string& callsign,
    const std::string& message
) const {
    return hftext::estimateTransmission(callsign, message, config_);
}

void ModemController::generateWav(
    const std::string& callsign,
    const std::string& message,
    const std::string& outputPath
) const {
    const auto audio = generateAudio(callsign, message);
    hftext::tools::writeMonoPcm16Wav(outputPath, audio, config_.sampleRate);
}

std::vector<float> ModemController::generateAudio(const std::string& callsign, const std::string& message) const {
    return hftext::generateTransmitAudio(callsign, message, config_);
}

ModemController::WavStats ModemController::analyzeWav(const std::string& inputPath) const {
    const auto wav = hftext::tools::readPcm16Wav(inputPath);
    const auto audioStats = hftext::analyzeAudioSamples(wav.samples);
    WavStats stats;
    stats.sampleRate = wav.sampleRate;
    stats.sampleCount = audioStats.sampleCount;
    stats.peak = audioStats.peak;
    stats.clippedSamples = audioStats.clippedSamples;
    return stats;
}

hftext::DecodeResult ModemController::decodeWav(const std::string& inputPath) const {
    const auto wav = hftext::tools::readPcm16Wav(inputPath);
    hftext::ModemConfig decodeConfig = config_;
    decodeConfig.sampleRate = wav.sampleRate;
    return hftext::demodulateSamples(wav.samples, decodeConfig);
}
