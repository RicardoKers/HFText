#include "hftext_config.h"
#include "hftext_app_settings.h"
#include "hftext_streaming_receiver.h"
#include "hftext_version.h"
#include "wav_io.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void printUsage(const char* program) {
    std::cerr
        << "Usage: " << program << " [options] <input.wav>\n"
        << "\n"
        << "Options:\n"
        << "  --version                  print version information\n"
        << "  --symbol-duration <s>       default: 0.5\n"
        << "  --mode <2fsk|4fsk|8fsk>     default: 2fsk; 4fsk/8fsk are experimental\n"
        << "  --f0 <Hz>                   default: 1200\n"
        << "  --f1 <Hz>                   default: 1600; in MFSK defines the second tone and spacing\n"
        << "  --chunk-ms <ms>             default: 500\n"
        << "  --verbose                   print streaming diagnostics\n";
}

}  // namespace

int main(int argc, char** argv) {
    hftext::ModemConfig config;
    std::string inputPath;
    int chunkMilliseconds = 500;
    bool verbose = false;

    try {
        for (int index = 1; index < argc; ++index) {
            const std::string arg = argv[index];
            auto requireValue = [&](const std::string& option) -> std::string {
                if (index + 1 >= argc) {
                    throw std::invalid_argument("missing value for option: " + option);
                }
                return argv[++index];
            };

            if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
                return 0;
            }
            if (arg == "--version") {
                std::cout << hftext::kVersionLabel << " (" << hftext::kReleaseTrack << ")\n";
                std::cout << "Protocol: " << hftext::kProtocolVersion << "\n";
                return 0;
            }
            if (arg == "--symbol-duration") {
                config.symbolDurationSec = std::stof(requireValue(arg));
            } else if (arg == "--mode") {
                config.modulationMode = hftext::parseModulationModeKey(requireValue(arg));
            } else if (arg == "--f0") {
                config.frequency0Hz = std::stof(requireValue(arg));
            } else if (arg == "--f1") {
                config.frequency1Hz = std::stof(requireValue(arg));
            } else if (arg == "--chunk-ms") {
                chunkMilliseconds = std::stoi(requireValue(arg));
            } else if (arg == "--verbose" || arg == "-v") {
                verbose = true;
            } else if (inputPath.empty()) {
                inputPath = arg;
            } else {
                throw std::invalid_argument("unexpected argument: " + arg);
            }
        }

        if (inputPath.empty()) {
            printUsage(argv[0]);
            return 2;
        }
        if (chunkMilliseconds <= 0) {
            throw std::invalid_argument("chunk-ms must be positive");
        }

        const auto wav = hftext::tools::readPcm16Wav(inputPath);
        config.sampleRate = wav.sampleRate;

        hftext::StreamingReceiver receiver(config);
        std::vector<hftext::DecodeResult> decoded;
        const auto chunkSamples = static_cast<std::size_t>(
            std::max(1, config.sampleRate * chunkMilliseconds / 1000)
        );
        for (std::size_t offset = 0; offset < wav.samples.size(); offset += chunkSamples) {
            const auto end = std::min(wav.samples.size(), offset + chunkSamples);
            std::vector<float> chunk(
                wav.samples.begin() + static_cast<std::ptrdiff_t>(offset),
                wav.samples.begin() + static_cast<std::ptrdiff_t>(end)
            );
            const auto results = receiver.pushSamples(chunk);
            decoded.insert(decoded.end(), results.begin(), results.end());
        }

        if (verbose) {
            const auto events = receiver.takeEvents();
            std::cout << "HFText version: " << hftext::kVersion << "\n";
            std::cout << "Mode: " << hftext::modulationModeProtocolName(config.modulationMode) << " streaming\n";
            std::cout << "Sample rate: " << config.sampleRate << " Hz\n";
            std::cout << "Chunk: " << chunkMilliseconds << " ms\n";
            std::cout << "Frames: " << decoded.size() << "\n";
            std::cout << "Pending events: " << events.size() << "\n";
        }

        for (const auto& result : decoded) {
            if (result.crcOk && result.payloadValid) {
                std::cout << result.text << "\n";
                if (verbose) {
                    std::cout << "Start offset: " << result.startOffset << " samples\n";
                    std::cout << "Offsets tried: " << result.offsetsTried << "\n";
                    std::cout << "Sync index: " << result.syncIndex << " bits\n";
                    std::cout << "Length: " << result.length << " symbols\n";
                    std::cout << "Confidence: " << result.confidence * 100.0F << "%\n";
                }
            }
        }

        return decoded.empty() ? 1 : 0;
    } catch (const std::exception& exc) {
        std::cerr << "Error: " << exc.what() << "\n";
        return 1;
    }
}
