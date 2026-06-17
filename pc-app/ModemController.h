#pragma once

#include "hftext_app_tx.h"
#include "hftext_result.h"

#include <cstddef>
#include <string>
#include <vector>

class ModemController {
public:
    struct WavStats {
        int sampleRate = 0;
        std::size_t sampleCount = 0;
        float peak = 0.0F;
        std::size_t clippedSamples = 0;

        double durationSeconds() const;
    };

    void setConfig(const hftext::ModemConfig& config);
    const hftext::ModemConfig& config() const;

    std::string buildPayload(const std::string& callsign, const std::string& message) const;
    hftext::TransmissionEstimate estimateTransmission(const std::string& callsign, const std::string& message) const;
    std::vector<float> generateAudio(const std::string& callsign, const std::string& message) const;
    void generateWav(const std::string& callsign, const std::string& message, const std::string& outputPath) const;
    WavStats analyzeWav(const std::string& inputPath) const;
    hftext::DecodeResult decodeWav(const std::string& inputPath) const;

private:
    hftext::ModemConfig config_;
};
