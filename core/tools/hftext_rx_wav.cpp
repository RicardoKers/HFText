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
        << "Uso: " << program << " [opcoes] <entrada.wav>\n"
        << "\n"
        << "Opcoes:\n"
        << "  --symbol-duration <s>       padrao: 0.5\n"
        << "  --f0 <Hz>                   padrao: 1200\n"
        << "  --f1 <Hz>                   padrao: 1600\n"
        << "  --no-sync-search            interpreta o fluxo desde o primeiro bit\n";
}

}  // namespace

int main(int argc, char** argv) {
    hftext::ModemConfig config;
    std::string inputPath;

    try {
        for (int index = 1; index < argc; ++index) {
            const std::string arg = argv[index];
            auto requireValue = [&](const std::string& option) -> std::string {
                if (index + 1 >= argc) {
                    throw std::invalid_argument("opcao sem valor: " + option);
                }
                return argv[++index];
            };

            if (arg == "--help" || arg == "-h") {
                printUsage(argv[0]);
                return 0;
            }
            if (arg == "--symbol-duration") {
                config.symbolDurationSec = std::stof(requireValue(arg));
            } else if (arg == "--f0") {
                config.frequency0Hz = std::stof(requireValue(arg));
            } else if (arg == "--f1") {
                config.frequency1Hz = std::stof(requireValue(arg));
            } else if (arg == "--no-sync-search") {
                config.syncSearch = false;
            } else if (inputPath.empty()) {
                inputPath = arg;
            } else {
                throw std::invalid_argument("argumento inesperado: " + arg);
            }
        }

        if (inputPath.empty()) {
            printUsage(argv[0]);
            return 2;
        }

        const auto wav = hftext::tools::readPcm16Wav(inputPath);
        config.sampleRate = wav.sampleRate;
        const auto result = hftext::demodulateSamples(wav.samples, config);

        if (!result.frameDetected) {
            std::cout << "Quadro nao detectado: " << result.error << "\n";
            return 1;
        }
        if (!result.crcOk) {
            std::cout << "Quadro detectado, mas CRC invalido.\n";
            if (!result.error.empty()) {
                std::cout << "Erro: " << result.error << "\n";
            }
            return 2;
        }
        if (!result.payloadValid) {
            std::cout << "Quadro detectado, CRC valido, mas payload invalido.\n";
            if (!result.error.empty()) {
                std::cout << "Erro: " << result.error << "\n";
            }
            return 3;
        }

        std::cout << result.text << "\n";
        return 0;
    } catch (const std::exception& exc) {
        std::cerr << "Erro: " << exc.what() << "\n";
        return 1;
    }
}
