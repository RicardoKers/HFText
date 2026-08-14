#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum class AudioSampleEncoding {
    SignedPcm,
    Float32,
};

struct InterleavedAudioFormat {
    AudioSampleEncoding encoding = AudioSampleEncoding::SignedPcm;
    int channelCount = 0;
    int bitsPerSample = 0;
    int validBitsPerSample = 0;
    int blockAlign = 0;
};

std::vector<float> interleavedAudioToMono(
    const std::uint8_t* data,
    std::size_t frameCount,
    const InterleavedAudioFormat& format,
    bool silent = false
);

class StreamingLinearResampler {
public:
    StreamingLinearResampler(int sourceSampleRate, int destinationSampleRate);

    std::vector<float> process(const std::vector<float>& input);
    void reset();

private:
    int sourceSampleRate_ = 0;
    int destinationSampleRate_ = 0;
    double nextPosition_ = 0.0;
    std::vector<float> pending_;
};

class FixedAudioBlockBuffer {
public:
    explicit FixedAudioBlockBuffer(std::size_t blockSize);

    std::vector<std::vector<float>> process(const std::vector<float>& input);
    std::size_t pendingSampleCount() const;
    void reset();

private:
    std::size_t blockSize_ = 0;
    std::vector<float> pending_;
};
