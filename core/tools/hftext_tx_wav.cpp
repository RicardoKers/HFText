#include "hftext_config.h"
#include "hftext_core.h"
#include "hftext_version.h"
#include "wav_io.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void printUsage(const char* program) {
    std::cerr
        << "Usage: " << program << " [options] <message> <output.wav>\n"
        << "\n"
        << "Options:\n"
        << "  --version                  print version information\n"
        << "  --callsign <text>           prefix callsign to the payload\n"
        << "  --sample-rate <Hz>          default: 48000\n"
        << "  --symbol-duration <s>       default: 0.5\n"
        << "  --mode <2fsk|4fsk|8fsk>     default: 2fsk; 4fsk/8fsk are experimental\n"
        << "  --f0 <Hz>                   default: 1200\n"
        << "  --f1 <Hz>                   default: 1600; in MFSK defines the second tone and spacing\n"
        << "  --amplitude <0..1>          default: 0.8\n";
}

std::string buildPayload(const std::string& message, const std::string& callsign) {
    if (callsign.empty()) {
        return message;
    }
    return callsign + " " + message;
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
    std::string callsign;
    std::string message;
    std::string outputPath;

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
            if (arg == "--callsign") {
                callsign = requireValue(arg);
            } else if (arg == "--sample-rate") {
                config.sampleRate = std::stoi(requireValue(arg));
            } else if (arg == "--symbol-duration") {
                config.symbolDurationSec = std::stof(requireValue(arg));
            } else if (arg == "--mode") {
                setMode(config, requireValue(arg));
            } else if (arg == "--f0") {
                config.frequency0Hz = std::stof(requireValue(arg));
            } else if (arg == "--f1") {
                config.frequency1Hz = std::stof(requireValue(arg));
            } else if (arg == "--amplitude") {
                config.amplitude = std::stof(requireValue(arg));
            } else if (message.empty()) {
                message = arg;
            } else if (outputPath.empty()) {
                outputPath = arg;
            } else {
                throw std::invalid_argument("unexpected argument: " + arg);
            }
        }

        if (message.empty() || outputPath.empty()) {
            printUsage(argv[0]);
            return 2;
        }

        const std::string payload = buildPayload(message, callsign);
        const auto audio = hftext::modulateText(payload, config);
        hftext::tools::writeMonoPcm16Wav(outputPath, audio, config.sampleRate);

        std::cout << "WAV generated: " << outputPath << "\n";
        std::cout << "HFText version: " << hftext::kVersion << "\n";
        std::cout << "Mode: " << modeName(config.modulationMode) << "\n";
        std::cout << "Payload: " << payload << "\n";
        return 0;
    } catch (const std::exception& exc) {
        std::cerr << "Error: " << exc.what() << "\n";
        return 1;
    }
}
