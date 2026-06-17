#include "hftext_config.h"
#include "hftext_app_settings.h"
#include "hftext_core.h"
#include "hftext_version.h"
#include "wav_io.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

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
        << "  --no-sync-search            do not try initial sample offsets\n"
        << "  --verbose                   print synchronization diagnostics\n";
}

}  // namespace

int main(int argc, char** argv) {
    hftext::ModemConfig config;
    std::string inputPath;
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
            } else if (arg == "--no-sync-search") {
                config.syncSearch = false;
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

        const auto wav = hftext::tools::readPcm16Wav(inputPath);
        config.sampleRate = wav.sampleRate;
        const auto result = hftext::demodulateSamples(wav.samples, config);
        auto printDiagnostics = [&]() {
            if (!verbose) {
                return;
            }
            std::cout << "HFText version: " << hftext::kVersion << "\n";
            std::cout << "Mode: " << hftext::modulationModeProtocolName(config.modulationMode) << "\n";
            std::cout << "Sample rate: " << config.sampleRate << " Hz\n";
            std::cout << "Start offset: " << result.startOffset << " samples\n";
            std::cout << "Offsets tried: " << result.offsetsTried << "\n";
            std::cout << "Sync index: " << result.syncIndex << " bits\n";
            std::cout << "Length: " << result.length << " symbols\n";
            std::cout << "Confidence: " << result.confidence * 100.0F << "%\n";
        };

        if (!result.frameDetected) {
            std::cout << "Frame not detected: " << result.error << "\n";
            printDiagnostics();
            return 1;
        }
        if (!result.crcOk) {
            std::cout << "Frame detected, but CRC is invalid.\n";
            if (!result.error.empty()) {
                std::cout << "Error: " << result.error << "\n";
            }
            printDiagnostics();
            return 2;
        }
        if (!result.payloadValid) {
            std::cout << "Frame detected, CRC is valid, but payload is invalid.\n";
            if (!result.error.empty()) {
                std::cout << "Error: " << result.error << "\n";
            }
            printDiagnostics();
            return 3;
        }

        std::cout << result.text << "\n";
        printDiagnostics();
        return 0;
    } catch (const std::exception& exc) {
        std::cerr << "Error: " << exc.what() << "\n";
        return 1;
    }
}
