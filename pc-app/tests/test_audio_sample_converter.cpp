#include "AudioSampleConverter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testFloatStereoDownmix() {
    const std::vector<float> interleaved{
        1.0F, -1.0F,
        0.5F, 0.25F,
        -0.25F, -0.75F,
    };
    const InterleavedAudioFormat format{
        AudioSampleEncoding::Float32,
        2,
        32,
        32,
        8,
    };

    const auto mono = interleavedAudioToMono(
        reinterpret_cast<const std::uint8_t*>(interleaved.data()),
        3,
        format
    );

    require(mono.size() == 3, "float downmix size mismatch");
    require(std::abs(mono[0]) < 1.0e-6F, "float downmix cancellation failed");
    require(std::abs(mono[1] - 0.375F) < 1.0e-6F, "float downmix average failed");
    require(std::abs(mono[2] + 0.5F) < 1.0e-6F, "float downmix negative average failed");
}

void testPcm16AndSilence() {
    const std::vector<std::int16_t> pcm{
        32767, 32767,
        -32768, -32768,
    };
    const InterleavedAudioFormat format{
        AudioSampleEncoding::SignedPcm,
        2,
        16,
        16,
        4,
    };

    const auto mono = interleavedAudioToMono(
        reinterpret_cast<const std::uint8_t*>(pcm.data()),
        2,
        format
    );
    require(mono.size() == 2, "PCM16 downmix size mismatch");
    require(mono[0] > 0.999F, "PCM16 positive full scale failed");
    require(mono[1] == -1.0F, "PCM16 negative full scale failed");

    const auto silence = interleavedAudioToMono(nullptr, 4, format, true);
    require(silence == std::vector<float>(4, 0.0F), "silent packet conversion failed");
}

void testInvalidFormatRejected() {
    bool rejected = false;
    try {
        (void)interleavedAudioToMono(
            nullptr,
            1,
            InterleavedAudioFormat{AudioSampleEncoding::SignedPcm, 0, 16, 16, 2},
            true
        );
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "invalid channel count was accepted");
}

void testStreamingResample44100To48000() {
    constexpr int sourceRate = 44100;
    constexpr int destinationRate = 48000;
    constexpr double frequency = 1200.0;

    std::vector<float> source(sourceRate);
    for (int index = 0; index < sourceRate; ++index) {
        source[static_cast<std::size_t>(index)] = static_cast<float>(
            std::sin(2.0 * kPi * frequency * static_cast<double>(index) / sourceRate)
        );
    }

    StreamingLinearResampler resampler(sourceRate, destinationRate);
    std::vector<float> output;
    for (std::size_t offset = 0; offset < source.size(); offset += 317) {
        const auto end = std::min(source.size(), offset + 317);
        const std::vector<float> chunk(source.begin() + static_cast<std::ptrdiff_t>(offset), source.begin() + static_cast<std::ptrdiff_t>(end));
        auto converted = resampler.process(chunk);
        output.insert(output.end(), converted.begin(), converted.end());
    }

    require(output.size() >= 47998 && output.size() <= 48000, "resampled duration mismatch");
    double squaredError = 0.0;
    for (std::size_t index = 0; index < output.size(); ++index) {
        const double expected = std::sin(
            2.0 * kPi * frequency * static_cast<double>(index) / destinationRate
        );
        const double error = static_cast<double>(output[index]) - expected;
        squaredError += error * error;
    }
    const double rootMeanSquareError = std::sqrt(squaredError / static_cast<double>(output.size()));
    require(rootMeanSquareError < 0.01, "resampled tone error is too large");
}

void testMatchingSampleRateIsExact() {
    const std::vector<float> input{0.1F, -0.2F, 0.3F};
    StreamingLinearResampler resampler(48000, 48000);
    require(resampler.process(input) == input, "matching sample rate changed samples");
}

void testFixedAudioBlockBuffer() {
    FixedAudioBlockBuffer buffer(4);

    require(buffer.process({1.0F, 2.0F}).empty(), "partial block was emitted");
    const auto first = buffer.process({3.0F, 4.0F, 5.0F});
    require(first.size() == 1, "completed block was not emitted");
    require(
        first[0] == std::vector<float>({1.0F, 2.0F, 3.0F, 4.0F}),
        "completed block contents changed"
    );
    require(buffer.pendingSampleCount() == 1, "block remainder size mismatch");

    const auto next = buffer.process({6.0F, 7.0F, 8.0F, 9.0F, 10.0F, 11.0F, 12.0F});
    require(next.size() == 2, "multiple completed blocks were not emitted");
    require(next[0] == std::vector<float>({5.0F, 6.0F, 7.0F, 8.0F}), "first queued block mismatch");
    require(next[1] == std::vector<float>({9.0F, 10.0F, 11.0F, 12.0F}), "second queued block mismatch");
    require(buffer.pendingSampleCount() == 0, "unexpected samples remained after complete blocks");

    buffer.process({13.0F});
    buffer.reset();
    require(buffer.pendingSampleCount() == 0, "block buffer reset failed");
}

void testInvalidFixedAudioBlockSizeRejected() {
    bool rejected = false;
    try {
        FixedAudioBlockBuffer buffer(0);
        (void)buffer;
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "zero audio block size was accepted");
}

}  // namespace

int main() {
    testFloatStereoDownmix();
    testPcm16AndSilence();
    testInvalidFormatRejected();
    testStreamingResample44100To48000();
    testMatchingSampleRateIsExact();
    testFixedAudioBlockBuffer();
    testInvalidFixedAudioBlockSizeRejected();
    return 0;
}
