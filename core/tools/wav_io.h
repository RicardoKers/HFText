#pragma once

#include <string>
#include <vector>

namespace hftext::tools {

struct WavData {
    std::vector<float> samples;
    int sampleRate = 0;
};

void writeMonoPcm16Wav(const std::string& path, const std::vector<float>& samples, int sampleRate);
WavData readPcm16Wav(const std::string& path);

}  // namespace hftext::tools
