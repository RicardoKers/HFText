#pragma once

#include "hftext_config.h"
#include "hftext_demodulator.h"
#include "hftext_result.h"

#include <cstddef>
#include <cstdint>
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
    struct PhaseState {
        int offsetSamples = 0;
        std::size_t nextStartSample = 0;
        std::size_t firstBitSample = 0;
        std::vector<BitDecision> decisions;
        std::vector<std::uint8_t> bits;
    };

    int samplesPerSymbol() const;
    std::size_t frameBitCount(const DecodeResult& result) const;
    std::vector<int> phaseOffsets() const;
    void resetPhaseStates();
    void processPhase(PhaseState& phase);
    bool findBestFrame(DecodeResult& result, std::size_t& frameEndSample) const;
    void resetAfterFrame(std::size_t frameEndSample);
    void trimBuffer();
    void trimBitBuffers();

    ModemConfig config_;
    std::vector<float> buffer_;
    std::size_t sampleCursor_ = 0;
    std::vector<PhaseState> phases_;
};

}  // namespace hftext
