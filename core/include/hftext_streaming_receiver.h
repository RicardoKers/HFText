#pragma once

#include "hftext_config.h"
#include "hftext_demodulator.h"
#include "hftext_result.h"

#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>

namespace hftext {

enum class StreamingReceiverEventType {
    SyncFound,
    PhysicalLengthRecovered,
    PhysicalLengthInvalid,
    FrameWaiting,
    FrameRejected,
    FrameDecoded,
};

struct StreamingReceiverEvent {
    StreamingReceiverEventType type = StreamingReceiverEventType::SyncFound;
    std::int32_t phaseOffsetSamples = 0;
    std::int64_t syncSample = -1;
    std::int32_t syncBitIndex = -1;
    std::int32_t syncMismatches = 0;
    std::int32_t payloadLength = -1;
    std::int32_t decodedLength = -1;
    std::int32_t bitsAvailable = 0;
    std::int32_t bitsExpected = 0;
    bool crcOk = false;
    bool payloadValid = false;
    float confidence = 0.0F;
    float latencySeconds = 0.0F;
};

struct StreamingReceiverMetrics {
    std::uint64_t pushCalls = 0;
    std::uint64_t samplesPushed = 0;
    std::uint64_t phaseSymbolsProcessed = 0;
    std::uint64_t bitDecisionsProduced = 0;
    std::uint64_t syncPositionsExamined = 0;
    std::uint64_t syncPatternMatches = 0;
    std::uint64_t rejectedSyncCacheHits = 0;
    std::uint64_t physicalLengthAttempts = 0;
    std::uint64_t physicalLengthValid = 0;
    std::uint64_t physicalLengthInvalid = 0;
    std::uint64_t frameWaitingChecks = 0;
    std::uint64_t robustDecodeAttempts = 0;
    std::uint64_t validFrameCandidates = 0;
    std::uint64_t rejectedFrameCandidates = 0;
    std::uint64_t framesDecoded = 0;
    std::uint64_t demodulationTimeNs = 0;
    std::uint64_t frameSearchTimeNs = 0;
    std::uint64_t robustDecodeTimeNs = 0;
    std::uint64_t totalPushTimeNs = 0;
    std::uint64_t maxPushTimeNs = 0;
    std::uint64_t phaseCount = 0;
    bool timingEnabled = false;
};

class StreamingReceiver {
public:
    explicit StreamingReceiver(const ModemConfig& config = ModemConfig{});

    void setConfig(const ModemConfig& config);
    const ModemConfig& config() const;

    void reset();
    std::vector<DecodeResult> pushSamples(const std::vector<float>& samples);
    std::vector<StreamingReceiverEvent> takeEvents();
    void setPerformanceTimingEnabled(bool enabled);
    bool performanceTimingEnabled() const;
    void resetMetrics();
    StreamingReceiverMetrics metrics() const;

private:
    struct EventKey {
        StreamingReceiverEventType type = StreamingReceiverEventType::SyncFound;
        int phaseOffsetSamples = 0;
        std::int64_t syncSample = -1;
        std::int32_t syncBitIndex = -1;
        int bucket = 0;

        bool operator<(const EventKey& other) const;
    };

    struct PhaseState {
        int offsetSamples = 0;
        float frequencyOffsetHz = 0.0F;
        ModemConfig config;
        std::size_t nextStartSample = 0;
        std::size_t firstBitSample = 0;
        std::vector<BitDecision> decisions;
        std::vector<std::uint8_t> bits;
        std::set<std::int64_t> rejectedSyncBitKeys;
        std::size_t nextSyncSearchBit = 0;
    };

    int samplesPerSymbol() const;
    int samplesPerSymbol(const ModemConfig& config) const;
    std::size_t frameBitCount(const DecodeResult& result) const;
    std::vector<int> phaseOffsets() const;
    void resetPhaseStates();
    void processPhase(PhaseState& phase);
    bool findBestFrame(DecodeResult& result, std::size_t& frameEndSample);
    void emitEvent(const StreamingReceiverEvent& event, int bucket = 0);
    void resetAfterFrame(std::size_t frameEndSample);
    void trimBuffer();
    void trimBitBuffers();

    ModemConfig config_;
    std::vector<float> buffer_;
    std::size_t sampleCursor_ = 0;
    std::vector<PhaseState> phases_;
    std::vector<StreamingReceiverEvent> events_;
    std::set<EventKey> reportedEvents_;
    StreamingReceiverMetrics metrics_;
    bool performanceTimingEnabled_ = false;
};

}  // namespace hftext
