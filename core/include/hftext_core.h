#pragma once

#include "hftext_config.h"
#include "hftext_result.h"

#include <string>
#include <vector>

namespace hftext {

std::vector<float> modulateText(const std::string& text, const ModemConfig& config = ModemConfig{});
DecodeResult demodulateSamples(const std::vector<float>& samples, const ModemConfig& config = ModemConfig{});

}  // namespace hftext
