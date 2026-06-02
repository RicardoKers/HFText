#include "wav_io.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>

namespace hftext::tools {
namespace {

constexpr std::uint16_t kPcmFormat = 1;
constexpr std::uint16_t kMonoChannels = 1;
constexpr std::uint16_t kBitsPerSample = 16;
constexpr std::uint16_t kBytesPerSample = kBitsPerSample / 8;

void writeBytes(std::ofstream& output, const char* text, std::size_t size) {
    output.write(text, static_cast<std::streamsize>(size));
}

void writeU16(std::ofstream& output, std::uint16_t value) {
    const char bytes[] = {
        static_cast<char>(value & 0xFF),
        static_cast<char>((value >> 8) & 0xFF),
    };
    writeBytes(output, bytes, sizeof(bytes));
}

void writeU32(std::ofstream& output, std::uint32_t value) {
    const char bytes[] = {
        static_cast<char>(value & 0xFF),
        static_cast<char>((value >> 8) & 0xFF),
        static_cast<char>((value >> 16) & 0xFF),
        static_cast<char>((value >> 24) & 0xFF),
    };
    writeBytes(output, bytes, sizeof(bytes));
}

std::uint16_t readU16(std::istream& input) {
    unsigned char bytes[2] = {};
    input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (!input) {
        throw std::runtime_error("unexpected end of wav file");
    }
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

std::uint32_t readU32(std::istream& input) {
    unsigned char bytes[4] = {};
    input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (!input) {
        throw std::runtime_error("unexpected end of wav file");
    }
    return static_cast<std::uint32_t>(bytes[0])
        | (static_cast<std::uint32_t>(bytes[1]) << 8)
        | (static_cast<std::uint32_t>(bytes[2]) << 16)
        | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::string readFourCc(std::istream& input) {
    char text[4] = {};
    input.read(text, sizeof(text));
    if (!input) {
        throw std::runtime_error("unexpected end of wav file");
    }
    return std::string(text, sizeof(text));
}

void skipBytes(std::istream& input, std::uint32_t size) {
    input.seekg(size, std::ios::cur);
    if (!input) {
        throw std::runtime_error("failed to skip wav chunk");
    }
}

std::int16_t floatToPcm16(float sample) {
    const float clipped = std::clamp(sample, -1.0F, 1.0F);
    if (clipped <= -1.0F) {
        return -32768;
    }
    return static_cast<std::int16_t>(std::lround(clipped * 32767.0F));
}

float pcm16ToFloat(std::int16_t sample) {
    if (sample < 0) {
        return static_cast<float>(sample) / 32768.0F;
    }
    return static_cast<float>(sample) / 32767.0F;
}

}  // namespace

void writeMonoPcm16Wav(const std::string& path, const std::vector<float>& samples, int sampleRate) {
    if (sampleRate <= 0) {
        throw std::invalid_argument("sample_rate must be positive");
    }
    if (path.empty()) {
        throw std::invalid_argument("path must not be empty");
    }

    const std::filesystem::path outputPath(path);
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }

    const auto dataBytes = static_cast<std::uint32_t>(samples.size() * kBytesPerSample);
    const auto byteRate = static_cast<std::uint32_t>(sampleRate * kMonoChannels * kBytesPerSample);
    const auto blockAlign = static_cast<std::uint16_t>(kMonoChannels * kBytesPerSample);
    const auto riffSize = static_cast<std::uint32_t>(36 + dataBytes);

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        throw std::runtime_error("failed to open wav file for writing");
    }

    writeBytes(output, "RIFF", 4);
    writeU32(output, riffSize);
    writeBytes(output, "WAVE", 4);
    writeBytes(output, "fmt ", 4);
    writeU32(output, 16);
    writeU16(output, kPcmFormat);
    writeU16(output, kMonoChannels);
    writeU32(output, static_cast<std::uint32_t>(sampleRate));
    writeU32(output, byteRate);
    writeU16(output, blockAlign);
    writeU16(output, kBitsPerSample);
    writeBytes(output, "data", 4);
    writeU32(output, dataBytes);

    for (float sample : samples) {
        writeU16(output, static_cast<std::uint16_t>(floatToPcm16(sample)));
    }
}

WavData readPcm16Wav(const std::string& path) {
    if (path.empty()) {
        throw std::invalid_argument("path must not be empty");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("failed to open wav file for reading");
    }

    if (readFourCc(input) != "RIFF") {
        throw std::runtime_error("missing RIFF header");
    }
    (void)readU32(input);
    if (readFourCc(input) != "WAVE") {
        throw std::runtime_error("missing WAVE header");
    }

    bool fmtFound = false;
    bool dataFound = false;
    std::uint16_t audioFormat = 0;
    std::uint16_t channelCount = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
    std::vector<unsigned char> rawData;

    while (input && (!fmtFound || !dataFound)) {
        const std::string chunkId = readFourCc(input);
        const std::uint32_t chunkSize = readU32(input);

        if (chunkId == "fmt ") {
            audioFormat = readU16(input);
            channelCount = readU16(input);
            sampleRate = readU32(input);
            (void)readU32(input);
            (void)readU16(input);
            bitsPerSample = readU16(input);
            if (chunkSize > 16) {
                skipBytes(input, chunkSize - 16);
            }
            fmtFound = true;
        } else if (chunkId == "data") {
            rawData.resize(chunkSize);
            input.read(reinterpret_cast<char*>(rawData.data()), static_cast<std::streamsize>(rawData.size()));
            if (!input) {
                throw std::runtime_error("unexpected end of wav data");
            }
            dataFound = true;
        } else {
            skipBytes(input, chunkSize);
        }

        if ((chunkSize % 2U) != 0) {
            skipBytes(input, 1);
        }
    }

    if (!fmtFound || !dataFound) {
        throw std::runtime_error("missing fmt or data chunk");
    }
    if (audioFormat != kPcmFormat || bitsPerSample != kBitsPerSample) {
        throw std::runtime_error("only PCM16 WAV is supported");
    }
    if (channelCount == 0) {
        throw std::runtime_error("invalid channel count");
    }

    const auto bytesPerFrame = static_cast<std::size_t>(channelCount) * kBytesPerSample;
    if (rawData.size() % bytesPerFrame != 0) {
        throw std::runtime_error("invalid wav data size");
    }

    std::vector<float> samples;
    samples.reserve(rawData.size() / bytesPerFrame);
    for (std::size_t offset = 0; offset < rawData.size(); offset += bytesPerFrame) {
        float mixed = 0.0F;
        for (std::uint16_t channel = 0; channel < channelCount; ++channel) {
            const std::size_t sampleOffset = offset + static_cast<std::size_t>(channel) * kBytesPerSample;
            const auto raw = static_cast<std::uint16_t>(rawData[sampleOffset] | (rawData[sampleOffset + 1] << 8));
            mixed += pcm16ToFloat(static_cast<std::int16_t>(raw));
        }
        samples.push_back(mixed / static_cast<float>(channelCount));
    }

    return WavData{samples, static_cast<int>(sampleRate)};
}

}  // namespace hftext::tools
