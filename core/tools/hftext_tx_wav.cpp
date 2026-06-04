#include "hftext_config.h"
#include "hftext_core.h"
#include "wav_io.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void printUsage(const char* program) {
    std::cerr
        << "Uso: " << program << " [opcoes] <mensagem> <saida.wav>\n"
        << "\n"
        << "Opcoes:\n"
        << "  --callsign <texto>          prefixa indicativo ao payload\n"
        << "  --sample-rate <Hz>          padrao: 48000\n"
        << "  --symbol-duration <s>       padrao: 0.5\n"
        << "  --f0 <Hz>                   padrao: 1200\n"
        << "  --f1 <Hz>                   padrao: 1600\n"
        << "  --amplitude <0..1>          padrao: 0.8\n"
        << "  --robust                    usa modo robusto experimental\n";
}

std::string buildPayload(const std::string& message, const std::string& callsign) {
    if (callsign.empty()) {
        return message;
    }
    return callsign + " " + message;
}

}  // namespace

int main(int argc, char** argv) {
    hftext::ModemConfig config;
    std::string callsign;
    std::string message;
    std::string outputPath;
    bool robust = false;

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
            if (arg == "--callsign") {
                callsign = requireValue(arg);
            } else if (arg == "--sample-rate") {
                config.sampleRate = std::stoi(requireValue(arg));
            } else if (arg == "--symbol-duration") {
                config.symbolDurationSec = std::stof(requireValue(arg));
            } else if (arg == "--f0") {
                config.frequency0Hz = std::stof(requireValue(arg));
            } else if (arg == "--f1") {
                config.frequency1Hz = std::stof(requireValue(arg));
            } else if (arg == "--amplitude") {
                config.amplitude = std::stof(requireValue(arg));
            } else if (arg == "--robust") {
                robust = true;
            } else if (message.empty()) {
                message = arg;
            } else if (outputPath.empty()) {
                outputPath = arg;
            } else {
                throw std::invalid_argument("argumento inesperado: " + arg);
            }
        }

        if (message.empty() || outputPath.empty()) {
            printUsage(argv[0]);
            return 2;
        }

        const std::string payload = buildPayload(message, callsign);
        const auto audio = robust
            ? hftext::modulateTextRobust(payload, config)
            : hftext::modulateText(payload, config);
        hftext::tools::writeMonoPcm16Wav(outputPath, audio, config.sampleRate);

        std::cout << "WAV gerado: " << outputPath << "\n";
        std::cout << "Modo: " << (robust ? "robust" : "basic") << "\n";
        std::cout << "Payload: " << payload << "\n";
        return 0;
    } catch (const std::exception& exc) {
        std::cerr << "Erro: " << exc.what() << "\n";
        return 1;
    }
}
