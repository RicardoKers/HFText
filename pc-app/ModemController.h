#pragma once

#include "hftext_config.h"
#include "hftext_result.h"

#include <string>
#include <vector>

class ModemController {
public:
    void setConfig(const hftext::ModemConfig& config);
    const hftext::ModemConfig& config() const;

    std::string buildPayload(const std::string& callsign, const std::string& message) const;
    void generateWav(const std::string& callsign, const std::string& message, const std::string& outputPath) const;
    hftext::DecodeResult decodeWav(const std::string& inputPath) const;

private:
    hftext::ModemConfig config_;
};
