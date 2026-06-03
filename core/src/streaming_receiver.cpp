#include "hftext_streaming_receiver.h"

#include "hftext_core.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hftext {
namespace {

constexpr int kBitsPerByte = 8;
constexpr int kBitsPerSymbol = 6;
constexpr int kSyncBits = 16;
constexpr int kLengthBits = 8;
constexpr int kCrcBits = 16;
constexpr int kMaxRetainedBits = 1200;

std::size_t packedPayloadBytes(int symbolCount) {
    if (symbolCount <= 0) {
        return 0;
    }
    return static_cast<std::size_t>((symbolCount * kBitsPerSymbol + kBitsPerByte - 1) / kBitsPerByte);
}

}  // namespace

StreamingReceiver::StreamingReceiver(const ModemConfig& config)
    : config_(config) {
}

void StreamingReceiver::setConfig(const ModemConfig& config) {
    config_ = config;
    reset();
}

const ModemConfig& StreamingReceiver::config() const {
    return config_;
}

void StreamingReceiver::reset() {
    buffer_.clear();
}

std::vector<DecodeResult> StreamingReceiver::pushSamples(const std::vector<float>& samples) {
    if (samples.empty()) {
        return {};
    }

    buffer_.insert(buffer_.end(), samples.begin(), samples.end());
    trimBuffer();

    std::vector<DecodeResult> results;
    while (!buffer_.empty()) {
        const auto result = demodulateSamples(buffer_, config_);
        if (!result.crcOk || !result.payloadValid) {
            break;
        }

        results.push_back(result);

        const auto consumedSamples = static_cast<std::size_t>(
            result.startOffset + static_cast<int>((result.syncIndex + static_cast<int>(frameBitCount(result))) * samplesPerSymbol())
        );
        if (consumedSamples == 0 || consumedSamples >= buffer_.size()) {
            buffer_.clear();
            break;
        }
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(consumedSamples));
    }

    return results;
}

int StreamingReceiver::samplesPerSymbol() const {
    const auto samples = static_cast<int>(
        std::lround(static_cast<double>(config_.sampleRate) * config_.symbolDurationSec)
    );
    if (samples <= 0) {
        throw std::invalid_argument("symbol duration is too short for sample_rate");
    }
    return samples;
}

std::size_t StreamingReceiver::frameBitCount(const DecodeResult& result) const {
    return static_cast<std::size_t>(kSyncBits + kLengthBits + kCrcBits)
        + packedPayloadBytes(result.length) * kBitsPerByte;
}

void StreamingReceiver::trimBuffer() {
    const auto maxSamples = static_cast<std::size_t>(samplesPerSymbol()) * kMaxRetainedBits;
    if (buffer_.size() > maxSamples) {
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_.size() - maxSamples));
    }
}

}  // namespace hftext
