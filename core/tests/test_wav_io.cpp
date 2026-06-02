#include "wav_io.h"

#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace {

std::uint32_t readU32Le(const std::vector<unsigned char>& data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset])
        | (static_cast<std::uint32_t>(data[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(data[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

std::uint16_t readU16Le(const std::vector<unsigned char>& data, std::size_t offset) {
    return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

bool check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

}  // namespace

int main() {
    try {
        const char* path = "hftext_wav_io_test.wav";
        hftext::tools::writeMonoPcm16Wav(path, {-1.0F, 0.0F, 1.0F}, 8000);
        const auto wav = hftext::tools::readPcm16Wav(path);

        std::ifstream input(path, std::ios::binary);
        const std::vector<unsigned char> bytes{
            std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()
        };

        if (!check(bytes.size() == 50, "unexpected wav size")
            || !check(wav.sampleRate == 8000, "unexpected read sample rate")
            || !check(wav.samples.size() == 3, "unexpected read sample count")
            || !check(wav.samples[0] <= -0.999F, "unexpected read negative sample")
            || !check(wav.samples[1] == 0.0F, "unexpected read zero sample")
            || !check(wav.samples[2] >= 0.999F, "unexpected read positive sample")
            || !check(bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F', "missing RIFF")
            || !check(bytes[8] == 'W' && bytes[9] == 'A' && bytes[10] == 'V' && bytes[11] == 'E', "missing WAVE")
            || !check(readU16Le(bytes, 20) == 1, "unexpected format")
            || !check(readU16Le(bytes, 22) == 1, "unexpected channel count")
            || !check(readU32Le(bytes, 24) == 8000, "unexpected sample rate")
            || !check(readU16Le(bytes, 34) == 16, "unexpected bit depth")
            || !check(bytes[36] == 'd' && bytes[37] == 'a' && bytes[38] == 't' && bytes[39] == 'a', "missing data chunk")
            || !check(readU32Le(bytes, 40) == 6, "unexpected data size")
            || !check(readU16Le(bytes, 44) == 0x8000, "unexpected negative sample")
            || !check(readU16Le(bytes, 46) == 0x0000, "unexpected zero sample")
            || !check(readU16Le(bytes, 48) == 0x7FFF, "unexpected positive sample")) {
            std::remove(path);
            return 1;
        }

        std::remove(path);
        return 0;
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << "\n";
        return 1;
    }
}
