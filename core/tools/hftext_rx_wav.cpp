#include "hftext_config.h"
#include "hftext_core.h"
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
        << "  --symbol-duration <s>       default: 0.5\n"
        << "  --mode <2fsk|4fsk|8fsk>     default: 2fsk; 4fsk/8fsk are experimental\n"
        << "  --f0 <Hz>                   default: 1200\n"
        << "  --f1 <Hz>                   default: 1600; in MFSK defines the second tone and spacing\n"
        << "  --no-sync-search            do not try initial sample offsets\n"
        << "  --verbose                   print synchronization diagnostics\n";
}

void setMode(hftext::ModemConfig& config, const std::string& value) {
    if (value == "2fsk") {
        config.modulationMode = hftext::ModulationMode::Fsk2;
        return;
    }
    if (value == "4fsk") {
        config.modulationMode = hftext::ModulationMode::Fsk4;
        return;
    }
    if (value == "8fsk") {
        config.modulationMode = hftext::ModulationMode::Fsk8;
        return;
    }
    throw std::invalid_argument("invalid mode: " + value);
}

const char* modeName(hftext::ModulationMode mode) {
    switch (mode) {
    case hftext::ModulationMode::Fsk8:
        return "robust-v0.3-exp-8fsk";
    case hftext::ModulationMode::Fsk4:
        return "robust-v0.2-exp-4fsk";
    case hftext::ModulationMode::Fsk2:
    default:
        return "robust-v0.1-2fsk";
    }
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
            if (arg == "--symbol-duration") {
                config.symbolDurationSec = std::stof(requireValue(arg));
            } else if (arg == "--mode") {
                setMode(config, requireValue(arg));
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
            std::cout << "Mode: " << modeName(config.modulationMode) << "\n";
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
