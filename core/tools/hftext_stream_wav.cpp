#include "hftext_config.h"
#include "hftext_app_settings.h"
#include "hftext_streaming_receiver.h"
#include "hftext_version.h"
#include "cli_args.h"
#include "wav_io.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void printUsage(const std::string& program) {
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
        << "  --metrics-json <path>       write machine-readable receiver performance metrics\n"
        << "  --verbose                   print streaming diagnostics\n";
}

std::string jsonEscape(const std::string& value) {
    constexpr char hex[] = "0123456789abcdef";
    std::string escaped;
    escaped.reserve(value.size() + 8U);
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (character < 0x20U) {
                escaped += "\\u00";
                escaped.push_back(hex[(character >> 4U) & 0x0FU]);
                escaped.push_back(hex[character & 0x0FU]);
            } else {
                escaped.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    return escaped;
}

void writeBenchmarkJson(
    const std::string& outputPath,
    const std::string& inputPath,
    const hftext::ModemConfig& config,
    int chunkMilliseconds,
    std::size_t inputSamples,
    double replayWallSeconds,
    const std::vector<hftext::DecodeResult>& decoded,
    const hftext::StreamingReceiverMetrics& metrics
) {
    std::ofstream output(std::filesystem::u8path(outputPath), std::ios::binary);
    if (!output) {
        throw std::runtime_error("could not open metrics JSON: " + outputPath);
    }

    const double inputAudioSeconds = config.sampleRate <= 0
        ? 0.0
        : static_cast<double>(inputSamples) / static_cast<double>(config.sampleRate);
    const double realtimeFactor = replayWallSeconds <= 0.0 ? 0.0 : inputAudioSeconds / replayWallSeconds;
    output << std::setprecision(10);
    output << "{\n";
    output << "  \"schema_version\": 1,\n";
    output << "  \"hftext_version\": \"" << jsonEscape(hftext::kVersion) << "\",\n";
    output << "  \"release_track\": \"" << jsonEscape(hftext::kReleaseTrack) << "\",\n";
    output << "  \"protocol\": \"" << jsonEscape(hftext::kProtocolVersion) << "\",\n";
    output << "  \"input_path\": \"" << jsonEscape(inputPath) << "\",\n";
    output << "  \"input_samples\": " << inputSamples << ",\n";
    output << "  \"input_audio_s\": " << inputAudioSeconds << ",\n";
    output << "  \"replay_wall_s\": " << replayWallSeconds << ",\n";
    output << "  \"realtime_factor\": " << realtimeFactor << ",\n";
    output << "  \"chunk_ms\": " << chunkMilliseconds << ",\n";
    output << "  \"frames_decoded\": " << decoded.size() << ",\n";
    output << "  \"messages\": [\n";
    for (std::size_t index = 0; index < decoded.size(); ++index) {
        const auto& result = decoded[index];
        output << "    {\"text\": \"" << jsonEscape(result.text)
               << "\", \"length\": " << result.length
               << ", \"confidence\": " << result.confidence
               << ", \"crc_ok\": " << (result.crcOk ? "true" : "false")
               << ", \"payload_valid\": " << (result.payloadValid ? "true" : "false") << "}";
        output << (index + 1U == decoded.size() ? "\n" : ",\n");
    }
    output << "  ],\n";
    output << "  \"config\": {\n";
    output << "    \"sample_rate_hz\": " << config.sampleRate << ",\n";
    output << "    \"symbol_duration_s\": " << config.symbolDurationSec << ",\n";
    output << "    \"mode\": \"" << jsonEscape(hftext::modulationModeKey(config.modulationMode)) << "\",\n";
    output << "    \"base_frequency_hz\": " << config.frequency0Hz << ",\n";
    output << "    \"second_tone_hz\": " << config.frequency1Hz << "\n";
    output << "  },\n";
    output << "  \"receiver\": {\n";
    output << "    \"timing_enabled\": " << (metrics.timingEnabled ? "true" : "false") << ",\n";
    output << "    \"phase_count\": " << metrics.phaseCount << ",\n";
    output << "    \"push_calls\": " << metrics.pushCalls << ",\n";
    output << "    \"samples_pushed\": " << metrics.samplesPushed << ",\n";
    output << "    \"phase_symbols_processed\": " << metrics.phaseSymbolsProcessed << ",\n";
    output << "    \"bit_decisions_produced\": " << metrics.bitDecisionsProduced << ",\n";
    output << "    \"sync_positions_examined\": " << metrics.syncPositionsExamined << ",\n";
    output << "    \"sync_pattern_matches\": " << metrics.syncPatternMatches << ",\n";
    output << "    \"rejected_sync_cache_hits\": " << metrics.rejectedSyncCacheHits << ",\n";
    output << "    \"physical_length_attempts\": " << metrics.physicalLengthAttempts << ",\n";
    output << "    \"physical_length_valid\": " << metrics.physicalLengthValid << ",\n";
    output << "    \"physical_length_invalid\": " << metrics.physicalLengthInvalid << ",\n";
    output << "    \"frame_waiting_checks\": " << metrics.frameWaitingChecks << ",\n";
    output << "    \"robust_decode_attempts\": " << metrics.robustDecodeAttempts << ",\n";
    output << "    \"valid_frame_candidates\": " << metrics.validFrameCandidates << ",\n";
    output << "    \"rejected_frame_candidates\": " << metrics.rejectedFrameCandidates << ",\n";
    output << "    \"frames_decoded\": " << metrics.framesDecoded << ",\n";
    output << "    \"demodulation_time_ns\": " << metrics.demodulationTimeNs << ",\n";
    output << "    \"frame_search_time_ns\": " << metrics.frameSearchTimeNs << ",\n";
    output << "    \"robust_decode_time_ns\": " << metrics.robustDecodeTimeNs << ",\n";
    output << "    \"total_push_time_ns\": " << metrics.totalPushTimeNs << ",\n";
    output << "    \"max_push_time_ns\": " << metrics.maxPushTimeNs << "\n";
    output << "  }\n";
    output << "}\n";
}

}  // namespace

int runMain(const std::vector<std::string>& args) {
    hftext::ModemConfig config;
    std::string inputPath;
    int chunkMilliseconds = 500;
    bool verbose = false;
    std::string metricsJsonPath;

    try {
        for (std::size_t index = 1; index < args.size(); ++index) {
            const std::string arg = args[index];
            auto requireValue = [&](const std::string& option) -> std::string {
                if (index + 1 >= args.size()) {
                    throw std::invalid_argument("missing value for option: " + option);
                }
                return args[++index];
            };

            if (arg == "--help" || arg == "-h") {
                printUsage(args[0]);
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
            } else if (arg == "--metrics-json") {
                metricsJsonPath = requireValue(arg);
            } else if (arg == "--verbose" || arg == "-v") {
                verbose = true;
            } else if (inputPath.empty()) {
                inputPath = arg;
            } else {
                throw std::invalid_argument("unexpected argument: " + arg);
            }
        }

        if (inputPath.empty()) {
            printUsage(args[0]);
            return 2;
        }
        if (chunkMilliseconds <= 0) {
            throw std::invalid_argument("chunk-ms must be positive");
        }

        const auto wav = hftext::tools::readPcm16Wav(inputPath);
        config.sampleRate = wav.sampleRate;

        hftext::StreamingReceiver receiver(config);
        receiver.setPerformanceTimingEnabled(!metricsJsonPath.empty());
        std::vector<hftext::DecodeResult> decoded;
        const auto chunkSamples = static_cast<std::size_t>(
            std::max(1, config.sampleRate * chunkMilliseconds / 1000)
        );
        const auto replayStart = std::chrono::steady_clock::now();
        for (std::size_t offset = 0; offset < wav.samples.size(); offset += chunkSamples) {
            const auto end = std::min(wav.samples.size(), offset + chunkSamples);
            std::vector<float> chunk(
                wav.samples.begin() + static_cast<std::ptrdiff_t>(offset),
                wav.samples.begin() + static_cast<std::ptrdiff_t>(end)
            );
            const auto results = receiver.pushSamples(chunk);
            decoded.insert(decoded.end(), results.begin(), results.end());
        }
        const double replayWallSeconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - replayStart
        ).count();

        if (!metricsJsonPath.empty()) {
            writeBenchmarkJson(
                metricsJsonPath,
                inputPath,
                config,
                chunkMilliseconds,
                wav.samples.size(),
                replayWallSeconds,
                decoded,
                receiver.metrics()
            );
        }

        if (verbose) {
            const auto events = receiver.takeEvents();
            std::cout << "HFText version: " << hftext::kVersion << "\n";
            std::cout << "Mode: " << hftext::modulationModeProtocolName(config.modulationMode) << " streaming\n";
            std::cout << "Sample rate: " << config.sampleRate << " Hz\n";
            std::cout << "Chunk: " << chunkMilliseconds << " ms\n";
            std::cout << "Frames: " << decoded.size() << "\n";
            std::cout << "Pending events: " << events.size() << "\n";
            std::cout << "Replay wall time: " << replayWallSeconds << " s\n";
            if (replayWallSeconds > 0.0) {
                const double audioSeconds = static_cast<double>(wav.samples.size()) / config.sampleRate;
                std::cout << "Real-time factor: " << audioSeconds / replayWallSeconds << "x\n";
            }
            if (!metricsJsonPath.empty()) {
                std::cout << "Metrics JSON: " << metricsJsonPath << "\n";
            }
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

HFTEXT_CLI_MAIN(runMain)
