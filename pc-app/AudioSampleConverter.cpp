#include "AudioSampleConverter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {

void validateFormat(const InterleavedAudioFormat& format) {
    if (format.channelCount <= 0) {
        throw std::invalid_argument("audio format has no channels");
    }
    if (format.bitsPerSample != 16 && format.bitsPerSample != 24 && format.bitsPerSample != 32) {
        throw std::invalid_argument("unsupported audio sample size");
    }
    if (format.encoding == AudioSampleEncoding::Float32 && format.bitsPerSample != 32) {
        throw std::invalid_argument("unsupported floating-point audio format");
    }

    const int bytesPerSample = format.bitsPerSample / 8;
    if (format.blockAlign < format.channelCount * bytesPerSample) {
        throw std::invalid_argument("invalid audio block alignment");
    }

    const int validBits = format.validBitsPerSample > 0
        ? format.validBitsPerSample
        : format.bitsPerSample;
    if (validBits <= 0 || validBits > format.bitsPerSample) {
        throw std::invalid_argument("invalid audio valid-bit count");
    }
}

std::int64_t readSignedPcm(const std::uint8_t* data, int bitsPerSample) {
    if (bitsPerSample == 16) {
        std::int16_t value = 0;
        std::memcpy(&value, data, sizeof(value));
        return value;
    }
    if (bitsPerSample == 24) {
        std::int32_t value = static_cast<std::int32_t>(data[0])
            | (static_cast<std::int32_t>(data[1]) << 8)
            | (static_cast<std::int32_t>(data[2]) << 16);
        if ((value & 0x00800000) != 0) {
            value |= static_cast<std::int32_t>(0xFF000000);
        }
        return value;
    }

    std::int32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

float readSample(const std::uint8_t* data, const InterleavedAudioFormat& format) {
    if (format.encoding == AudioSampleEncoding::Float32) {
        float value = 0.0F;
        std::memcpy(&value, data, sizeof(value));
        if (!std::isfinite(value)) {
            return 0.0F;
        }
        return std::clamp(value, -1.0F, 1.0F);
    }

    const int validBits = format.validBitsPerSample > 0
        ? format.validBitsPerSample
        : format.bitsPerSample;
    std::int64_t value = readSignedPcm(data, format.bitsPerSample);
    if (validBits < format.bitsPerSample) {
        value /= std::int64_t{1} << (format.bitsPerSample - validBits);
    }

    const double scale = std::ldexp(1.0, validBits - 1);
    return std::clamp(static_cast<float>(static_cast<double>(value) / scale), -1.0F, 1.0F);
}

}  // namespace

std::vector<float> interleavedAudioToMono(
    const std::uint8_t* data,
    std::size_t frameCount,
    const InterleavedAudioFormat& format,
    bool silent
) {
    validateFormat(format);
    if (frameCount == 0) {
        return {};
    }
    if (!silent && data == nullptr) {
        throw std::invalid_argument("audio data is null");
    }
    if (silent) {
        return std::vector<float>(frameCount, 0.0F);
    }

    const int bytesPerSample = format.bitsPerSample / 8;
    std::vector<float> mono;
    mono.reserve(frameCount);
    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        const auto* frameData = data + frame * static_cast<std::size_t>(format.blockAlign);
        double sum = 0.0;
        for (int channel = 0; channel < format.channelCount; ++channel) {
            sum += readSample(frameData + channel * bytesPerSample, format);
        }
        mono.push_back(static_cast<float>(sum / static_cast<double>(format.channelCount)));
    }
    return mono;
}

StreamingLinearResampler::StreamingLinearResampler(int sourceSampleRate, int destinationSampleRate)
    : sourceSampleRate_(sourceSampleRate),
      destinationSampleRate_(destinationSampleRate) {
    if (sourceSampleRate_ <= 0 || destinationSampleRate_ <= 0) {
        throw std::invalid_argument("invalid resampler sample rate");
    }
}

std::vector<float> StreamingLinearResampler::process(const std::vector<float>& input) {
    if (input.empty()) {
        return {};
    }
    if (sourceSampleRate_ == destinationSampleRate_) {
        return input;
    }

    pending_.insert(pending_.end(), input.begin(), input.end());
    const double step = static_cast<double>(sourceSampleRate_)
        / static_cast<double>(destinationSampleRate_);
    std::vector<float> output;
    output.reserve(static_cast<std::size_t>(
        std::ceil(static_cast<double>(input.size()) / step)
    ));

    while (nextPosition_ + 1.0 < static_cast<double>(pending_.size())) {
        const auto index = static_cast<std::size_t>(nextPosition_);
        const double fraction = nextPosition_ - static_cast<double>(index);
        const double value = static_cast<double>(pending_[index])
            + (static_cast<double>(pending_[index + 1]) - static_cast<double>(pending_[index])) * fraction;
        output.push_back(static_cast<float>(value));
        nextPosition_ += step;
    }

    const auto consumed = static_cast<std::size_t>(nextPosition_);
    if (consumed > 0) {
        pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(consumed));
        nextPosition_ -= static_cast<double>(consumed);
    }
    return output;
}

void StreamingLinearResampler::reset() {
    nextPosition_ = 0.0;
    pending_.clear();
}

FixedAudioBlockBuffer::FixedAudioBlockBuffer(std::size_t blockSize)
    : blockSize_(blockSize) {
    if (blockSize_ == 0) {
        throw std::invalid_argument("audio block size must be positive");
    }
}

std::vector<std::vector<float>> FixedAudioBlockBuffer::process(
    const std::vector<float>& input
) {
    pending_.insert(pending_.end(), input.begin(), input.end());

    std::vector<std::vector<float>> blocks;
    std::size_t consumed = 0;
    while (pending_.size() - consumed >= blockSize_) {
        const auto begin = pending_.begin() + static_cast<std::ptrdiff_t>(consumed);
        blocks.emplace_back(begin, begin + static_cast<std::ptrdiff_t>(blockSize_));
        consumed += blockSize_;
    }
    if (consumed > 0) {
        pending_.erase(
            pending_.begin(),
            pending_.begin() + static_cast<std::ptrdiff_t>(consumed)
        );
    }
    return blocks;
}

std::size_t FixedAudioBlockBuffer::pendingSampleCount() const {
    return pending_.size();
}

void FixedAudioBlockBuffer::reset() {
    pending_.clear();
}
