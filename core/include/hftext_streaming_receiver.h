#pragma once

#include "hftext_config.h"
#include "hftext_result.h"

#include <vector>

namespace hftext {

class StreamingReceiver {
public:
    explicit StreamingReceiver(const ModemConfig& config = ModemConfig{});

    void setConfig(const ModemConfig& config);
    const ModemConfig& config() const;

    void reset();
    std::vector<DecodeResult> pushSamples(const std::vector<float>& samples);

private:
    int samplesPerSymbol() const;
    std::size_t frameBitCount(const DecodeResult& result) const;
    void trimBuffer();

    ModemConfig config_;
    std::vector<float> buffer_;
};

}  // namespace hftext
