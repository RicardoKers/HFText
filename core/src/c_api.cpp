#include "hftext_c_api.h"

#include "hftext_app_rx.h"
#include "hftext_app_settings.h"
#include "hftext_app_tx.h"
#include "hftext_audio_stats.h"
#include "hftext_encoder.h"
#include "hftext_streaming_receiver.h"
#include "hftext_version.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

struct HFTextStreamingReceiver {
    explicit HFTextStreamingReceiver(const hftext::ModemConfig& config)
        : receiver(config) {}

    hftext::StreamingReceiver receiver;
};

namespace {

void clearError(char* errorMessage, std::size_t errorMessageSize) {
    if (errorMessage != nullptr && errorMessageSize > 0) {
        errorMessage[0] = '\0';
    }
}

void writeError(char* errorMessage, std::size_t errorMessageSize, const std::string& message) {
    if (errorMessage == nullptr || errorMessageSize == 0) {
        return;
    }

    const auto count = (std::min)(message.size(), errorMessageSize - 1);
    std::memcpy(errorMessage, message.data(), count);
    errorMessage[count] = '\0';
}

bool copyStringToBuffer(const std::string& value, char* buffer, std::size_t bufferSize) {
    if (buffer == nullptr || bufferSize == 0) {
        return !value.empty();
    }

    const auto count = (std::min)(value.size(), bufferSize - 1);
    std::memcpy(buffer, value.data(), count);
    buffer[count] = '\0';
    return count < value.size();
}

hftext::ModulationMode toCppModulation(HFTextModulationMode mode) {
    switch (mode) {
    case HFTEXT_MODULATION_2FSK:
        return hftext::ModulationMode::Fsk2;
    case HFTEXT_MODULATION_4FSK:
        return hftext::ModulationMode::Fsk4;
    case HFTEXT_MODULATION_8FSK:
        return hftext::ModulationMode::Fsk8;
    }
    throw std::invalid_argument("invalid modulation mode");
}

HFTextModulationMode fromCppModulation(hftext::ModulationMode mode) {
    switch (mode) {
    case hftext::ModulationMode::Fsk8:
        return HFTEXT_MODULATION_8FSK;
    case hftext::ModulationMode::Fsk4:
        return HFTEXT_MODULATION_4FSK;
    case hftext::ModulationMode::Fsk2:
    default:
        return HFTEXT_MODULATION_2FSK;
    }
}

hftext::SpeedProfile toCppSpeedProfile(HFTextSpeedProfile profile) {
    switch (profile) {
    case HFTEXT_SPEED_PROFILE_SLOW:
        return hftext::SpeedProfile::Slow;
    case HFTEXT_SPEED_PROFILE_FAST:
        return hftext::SpeedProfile::Fast;
    }
    throw std::invalid_argument("invalid speed profile");
}

HFTextSpeedProfileConfig fromCppProfileConfig(const hftext::SpeedProfileConfig& config) {
    return {
        fromCppModulation(config.modulationMode),
        config.symbolDurationSec,
    };
}

hftext::SpeedProfileConfig toCppProfileConfig(const HFTextSpeedProfileConfig& config) {
    return {
        toCppModulation(config.modulation_mode),
        config.symbol_duration_sec,
    };
}

HFTextAppModemProfiles fromCppProfiles(const hftext::AppModemProfiles& profiles) {
    return {
        profiles.txSampleRate,
        profiles.rxSampleRate,
        profiles.baseFrequencyHz,
        profiles.toneSpacingHz,
        profiles.amplitude,
        profiles.preambleBits,
        fromCppProfileConfig(profiles.slow),
        fromCppProfileConfig(profiles.fast),
    };
}

hftext::AppModemProfiles toCppProfiles(const HFTextAppModemProfiles& profiles) {
    return {
        profiles.tx_sample_rate,
        profiles.rx_sample_rate,
        profiles.base_frequency_hz,
        profiles.tone_spacing_hz,
        profiles.amplitude,
        profiles.preamble_bits,
        toCppProfileConfig(profiles.slow),
        toCppProfileConfig(profiles.fast),
    };
}

HFTextModemConfig fromCppConfig(const hftext::ModemConfig& config) {
    return {
        config.sampleRate,
        config.symbolDurationSec,
        config.frequency0Hz,
        config.frequency1Hz,
        config.amplitude,
        config.preambleBits,
        config.syncSearch ? 1 : 0,
        fromCppModulation(config.modulationMode),
    };
}

hftext::ModemConfig toCppConfig(const HFTextModemConfig& config) {
    hftext::ModemConfig output;
    output.sampleRate = config.sample_rate;
    output.symbolDurationSec = config.symbol_duration_sec;
    output.frequency0Hz = config.frequency0_hz;
    output.frequency1Hz = config.frequency1_hz;
    output.amplitude = config.amplitude;
    output.preambleBits = config.preamble_bits;
    output.syncSearch = config.sync_search != 0;
    output.modulationMode = toCppModulation(config.modulation_mode);
    return output;
}

HFTextTransmissionEstimate fromCppEstimate(const hftext::TransmissionEstimate& estimate) {
    return {
        estimate.messageEmpty ? 1 : 0,
        estimate.payloadTooLong ? 1 : 0,
        estimate.payloadSymbols,
        estimate.maxPayloadSymbols,
        estimate.frameBits,
        estimate.transmissionBits,
        estimate.durationSeconds,
    };
}

HFTextStreamingReceiverEventType fromCppEventType(hftext::StreamingReceiverEventType type) {
    switch (type) {
    case hftext::StreamingReceiverEventType::SyncFound:
        return HFTEXT_RX_EVENT_SYNC_FOUND;
    case hftext::StreamingReceiverEventType::PhysicalLengthRecovered:
        return HFTEXT_RX_EVENT_PHYSICAL_LENGTH_RECOVERED;
    case hftext::StreamingReceiverEventType::PhysicalLengthInvalid:
        return HFTEXT_RX_EVENT_PHYSICAL_LENGTH_INVALID;
    case hftext::StreamingReceiverEventType::FrameWaiting:
        return HFTEXT_RX_EVENT_FRAME_WAITING;
    case hftext::StreamingReceiverEventType::FrameRejected:
        return HFTEXT_RX_EVENT_FRAME_REJECTED;
    case hftext::StreamingReceiverEventType::FrameDecoded:
    default:
        return HFTEXT_RX_EVENT_FRAME_DECODED;
    }
}

hftext::StreamingReceiverEventType toCppEventType(HFTextStreamingReceiverEventType type) {
    switch (type) {
    case HFTEXT_RX_EVENT_SYNC_FOUND:
        return hftext::StreamingReceiverEventType::SyncFound;
    case HFTEXT_RX_EVENT_PHYSICAL_LENGTH_RECOVERED:
        return hftext::StreamingReceiverEventType::PhysicalLengthRecovered;
    case HFTEXT_RX_EVENT_PHYSICAL_LENGTH_INVALID:
        return hftext::StreamingReceiverEventType::PhysicalLengthInvalid;
    case HFTEXT_RX_EVENT_FRAME_WAITING:
        return hftext::StreamingReceiverEventType::FrameWaiting;
    case HFTEXT_RX_EVENT_FRAME_REJECTED:
        return hftext::StreamingReceiverEventType::FrameRejected;
    case HFTEXT_RX_EVENT_FRAME_DECODED:
        return hftext::StreamingReceiverEventType::FrameDecoded;
    }
    throw std::invalid_argument("invalid receiver event type");
}

void fillDecodeResult(const hftext::DecodeResult& source, HFTextDecodeResult& target) {
    char* textBuffer = target.text_utf8;
    const std::size_t textBufferSize = target.text_size;

    target.frame_detected = source.frameDetected ? 1 : 0;
    target.crc_ok = source.crcOk ? 1 : 0;
    target.payload_valid = source.payloadValid ? 1 : 0;
    target.length = source.length;
    target.sync_index = source.syncIndex;
    target.start_offset = source.startOffset;
    target.offsets_tried = source.offsetsTried;
    target.confidence = source.confidence;
    target.text_utf8 = textBuffer;
    target.text_size = textBufferSize;
    target.text_bytes = source.text.size();
    target.text_truncated = copyStringToBuffer(source.text, textBuffer, textBufferSize) ? 1 : 0;
}

HFTextStreamingReceiverEvent fromCppEvent(const hftext::StreamingReceiverEvent& event) {
    return {
        fromCppEventType(event.type),
        event.phaseOffsetSamples,
        event.syncSample,
        event.syncBitIndex,
        event.syncMismatches,
        event.payloadLength,
        event.decodedLength,
        event.bitsAvailable,
        event.bitsExpected,
        event.crcOk ? 1 : 0,
        event.payloadValid ? 1 : 0,
        event.confidence,
        event.latencySeconds,
    };
}

hftext::StreamingReceiverEvent toCppEvent(const HFTextStreamingReceiverEvent& event) {
    hftext::StreamingReceiverEvent output;
    output.type = toCppEventType(event.type);
    output.phaseOffsetSamples = event.phase_offset_samples;
    output.syncSample = event.sync_sample;
    output.syncBitIndex = event.sync_bit_index;
    output.syncMismatches = event.sync_mismatches;
    output.payloadLength = event.payload_length;
    output.decodedLength = event.decoded_length;
    output.bitsAvailable = event.bits_available;
    output.bitsExpected = event.bits_expected;
    output.crcOk = event.crc_ok != 0;
    output.payloadValid = event.payload_valid != 0;
    output.confidence = event.confidence;
    output.latencySeconds = event.latency_seconds;
    return output;
}

void resetPreparedText(HFTextPreparedText* text) {
    if (text == nullptr) {
        return;
    }
    text->message_empty = 1;
    text->payload_too_long = 0;
    text->message_symbols = 0;
    text->payload_symbols = 0;
    text->max_payload_symbols = hftext::kMaxPayloadSymbols;
    text->sanitized_message_bytes = 0;
    text->payload_bytes = 0;
    text->sanitized_message_truncated = 0;
    text->payload_truncated = 0;
}

void resetAudio(HFTextFloatAudio* audio) {
    if (audio == nullptr) {
        return;
    }
    audio->samples = nullptr;
    audio->sample_count = 0;
    audio->sample_rate = 0;
    audio->duration_seconds = 0.0;
}

void resetToneFrequencies(HFTextToneFrequencies* frequencies) {
    if (frequencies == nullptr) {
        return;
    }
    std::fill(std::begin(frequencies->frequencies_hz), std::end(frequencies->frequencies_hz), 0.0F);
    frequencies->tone_count = 0;
}

void resetAudioStats(HFTextAudioStats* stats) {
    if (stats == nullptr) {
        return;
    }
    stats->sample_count = 0;
    stats->peak = 0.0F;
    stats->clipped_samples = 0;
    stats->clipping_percent = 0.0;
    stats->duration_seconds = 0.0;
}

void resetRxEventSummary(HFTextRxEventSummary* summary) {
    if (summary == nullptr) {
        return;
    }
    summary->best_decoded_index = -1;
    summary->best_length_index = -1;
    summary->best_waiting_index = -1;
    summary->best_sync_index = -1;
    summary->best_rejected_index = -1;
    summary->rejected_count = 0;
    summary->has_invalid_length = 0;
    summary->sync_count = 0;
    summary->length_count = 0;
    summary->rejected_event_count = 0;
    summary->quality_permille = -1;
    summary->has_terminal_candidate = 0;
}

int32_t exceptionStatus(const std::exception& exc, char* errorMessage, std::size_t errorMessageSize) {
    writeError(errorMessage, errorMessageSize, exc.what());
    return dynamic_cast<const std::invalid_argument*>(&exc) != nullptr
        ? HFTEXT_STATUS_INVALID_ARGUMENT
        : HFTEXT_STATUS_EXCEPTION;
}

}  // namespace

const char* hftext_c_application_name(void) {
    return hftext::kApplicationName;
}

const char* hftext_c_version(void) {
    return hftext::kVersion;
}

const char* hftext_c_version_label(void) {
    return hftext::kVersionLabel;
}

const char* hftext_c_release_track(void) {
    return hftext::kReleaseTrack;
}

const char* hftext_c_protocol_version(void) {
    return hftext::kProtocolVersion;
}

int32_t hftext_c_default_app_modem_profiles(HFTextAppModemProfiles* outProfiles) {
    if (outProfiles == nullptr) {
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }

    *outProfiles = fromCppProfiles(hftext::defaultAppModemProfiles());
    return HFTEXT_STATUS_OK;
}

int32_t hftext_c_modem_config_for_profile(
    const HFTextAppModemProfiles* profiles,
    HFTextSpeedProfile profile,
    int32_t sampleRate,
    HFTextModemConfig* outConfig,
    char* errorMessage,
    size_t errorMessageSize
) {
    clearError(errorMessage, errorMessageSize);
    if (profiles == nullptr || outConfig == nullptr) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }

    try {
        const auto config = hftext::modemConfigForProfile(
            toCppProfiles(*profiles),
            toCppSpeedProfile(profile),
            sampleRate
        );
        *outConfig = fromCppConfig(config);
        return HFTEXT_STATUS_OK;
    } catch (const std::exception& exc) {
        return exceptionStatus(exc, errorMessage, errorMessageSize);
    }
}

int32_t hftext_c_prepare_text(
    const char* callsignUtf8,
    const char* messageUtf8,
    char* sanitizedMessageUtf8,
    size_t sanitizedMessageSize,
    char* payloadUtf8,
    size_t payloadSize,
    HFTextPreparedText* outText,
    char* errorMessage,
    size_t errorMessageSize
) {
    clearError(errorMessage, errorMessageSize);
    resetPreparedText(outText);
    if (messageUtf8 == nullptr || outText == nullptr) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }

    try {
        const std::string callsign = callsignUtf8 == nullptr ? std::string() : std::string(callsignUtf8);
        const std::string sanitizedCallsign = hftext::sanitizeText(callsign);
        const std::string sanitizedMessage = hftext::sanitizeText(messageUtf8);
        const std::string payload = sanitizedMessage.empty()
            ? std::string()
            : hftext::buildTransmitPayload(sanitizedCallsign, sanitizedMessage);

        outText->message_empty = sanitizedMessage.empty() ? 1 : 0;
        outText->message_symbols = hftext::encodedSymbolCount(sanitizedMessage);
        outText->payload_symbols = payload.empty() ? 0 : hftext::encodedSymbolCount(payload);
        outText->payload_too_long = outText->payload_symbols > hftext::kMaxPayloadSymbols ? 1 : 0;
        outText->max_payload_symbols = hftext::kMaxPayloadSymbols;
        outText->sanitized_message_bytes = sanitizedMessage.size();
        outText->payload_bytes = payload.size();
        outText->sanitized_message_truncated = copyStringToBuffer(
            sanitizedMessage,
            sanitizedMessageUtf8,
            sanitizedMessageSize
        ) ? 1 : 0;
        outText->payload_truncated = copyStringToBuffer(payload, payloadUtf8, payloadSize) ? 1 : 0;
        return HFTEXT_STATUS_OK;
    } catch (const std::exception& exc) {
        resetPreparedText(outText);
        return exceptionStatus(exc, errorMessage, errorMessageSize);
    }
}

int32_t hftext_c_tone_frequencies(
    const HFTextModemConfig* config,
    HFTextToneFrequencies* outFrequencies,
    char* errorMessage,
    size_t errorMessageSize
) {
    clearError(errorMessage, errorMessageSize);
    resetToneFrequencies(outFrequencies);
    if (config == nullptr || outFrequencies == nullptr) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }

    try {
        const auto frequencies = hftext::modulationToneFrequenciesHz(toCppConfig(*config));
        if (frequencies.size() > std::size(outFrequencies->frequencies_hz)) {
            throw std::invalid_argument("too many tones");
        }
        outFrequencies->tone_count = static_cast<std::int32_t>(frequencies.size());
        std::copy(frequencies.begin(), frequencies.end(), outFrequencies->frequencies_hz);
        return HFTEXT_STATUS_OK;
    } catch (const std::exception& exc) {
        resetToneFrequencies(outFrequencies);
        return exceptionStatus(exc, errorMessage, errorMessageSize);
    }
}

int32_t hftext_c_analyze_audio_samples(
    const float* samples,
    size_t sampleCount,
    int32_t sampleRate,
    float clippingThreshold,
    HFTextAudioStats* outStats,
    char* errorMessage,
    size_t errorMessageSize
) {
    clearError(errorMessage, errorMessageSize);
    resetAudioStats(outStats);
    if (outStats == nullptr || (samples == nullptr && sampleCount > 0)) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }
    if (sampleRate <= 0) {
        writeError(errorMessage, errorMessageSize, "sample rate must be positive");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }

    try {
        std::vector<float> sampleVector;
        if (sampleCount > 0) {
            sampleVector.assign(samples, samples + sampleCount);
        }
        const auto stats = hftext::analyzeAudioSamples(sampleVector, clippingThreshold);
        outStats->sample_count = stats.sampleCount;
        outStats->peak = stats.peak;
        outStats->clipped_samples = stats.clippedSamples;
        outStats->clipping_percent = hftext::clippingPercent(stats.clippedSamples, stats.sampleCount);
        outStats->duration_seconds = hftext::audioDurationSeconds(stats.sampleCount, sampleRate);
        return HFTEXT_STATUS_OK;
    } catch (const std::exception& exc) {
        resetAudioStats(outStats);
        return exceptionStatus(exc, errorMessage, errorMessageSize);
    }
}

int32_t hftext_c_generate_transmit_audio(
    const char* callsignUtf8,
    const char* messageUtf8,
    const HFTextModemConfig* config,
    HFTextFloatAudio* outAudio,
    char* errorMessage,
    size_t errorMessageSize
) {
    clearError(errorMessage, errorMessageSize);
    resetAudio(outAudio);
    if (messageUtf8 == nullptr || config == nullptr || outAudio == nullptr) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }

    try {
        const std::string callsign = callsignUtf8 == nullptr ? std::string() : std::string(callsignUtf8);
        const std::string message(messageUtf8);
        const auto cppConfig = toCppConfig(*config);
        const auto audio = hftext::generateTransmitAudio(callsign, message, cppConfig);
        if (!audio.empty()) {
            if (audio.size() > (std::numeric_limits<std::size_t>::max)() / sizeof(float)) {
                throw std::bad_alloc();
            }
            const auto byteCount = audio.size() * sizeof(float);
            auto* samples = static_cast<float*>(std::malloc(byteCount));
            if (samples == nullptr) {
                throw std::bad_alloc();
            }
            std::memcpy(samples, audio.data(), byteCount);
            outAudio->samples = samples;
        }
        outAudio->sample_count = audio.size();
        outAudio->sample_rate = cppConfig.sampleRate;
        outAudio->duration_seconds = cppConfig.sampleRate > 0
            ? static_cast<double>(audio.size()) / static_cast<double>(cppConfig.sampleRate)
            : 0.0;
        return HFTEXT_STATUS_OK;
    } catch (const std::exception& exc) {
        resetAudio(outAudio);
        return exceptionStatus(exc, errorMessage, errorMessageSize);
    }
}

void hftext_c_free_audio(HFTextFloatAudio* audio) {
    if (audio == nullptr) {
        return;
    }
    std::free(audio->samples);
    resetAudio(audio);
}

int32_t hftext_c_streaming_receiver_create(
    const HFTextModemConfig* config,
    HFTextStreamingReceiver** outReceiver,
    char* errorMessage,
    size_t errorMessageSize
) {
    clearError(errorMessage, errorMessageSize);
    if (outReceiver != nullptr) {
        *outReceiver = nullptr;
    }
    if (config == nullptr || outReceiver == nullptr) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }

    try {
        *outReceiver = new HFTextStreamingReceiver(toCppConfig(*config));
        return HFTEXT_STATUS_OK;
    } catch (const std::exception& exc) {
        *outReceiver = nullptr;
        return exceptionStatus(exc, errorMessage, errorMessageSize);
    }
}

void hftext_c_streaming_receiver_free(HFTextStreamingReceiver* receiver) {
    delete receiver;
}

int32_t hftext_c_streaming_receiver_reset(
    HFTextStreamingReceiver* receiver,
    char* errorMessage,
    size_t errorMessageSize
) {
    clearError(errorMessage, errorMessageSize);
    if (receiver == nullptr) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }

    try {
        receiver->receiver.reset();
        return HFTEXT_STATUS_OK;
    } catch (const std::exception& exc) {
        return exceptionStatus(exc, errorMessage, errorMessageSize);
    }
}

int32_t hftext_c_streaming_receiver_set_config(
    HFTextStreamingReceiver* receiver,
    const HFTextModemConfig* config,
    char* errorMessage,
    size_t errorMessageSize
) {
    clearError(errorMessage, errorMessageSize);
    if (receiver == nullptr || config == nullptr) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }

    try {
        receiver->receiver.setConfig(toCppConfig(*config));
        return HFTEXT_STATUS_OK;
    } catch (const std::exception& exc) {
        return exceptionStatus(exc, errorMessage, errorMessageSize);
    }
}

int32_t hftext_c_streaming_receiver_push_samples(
    HFTextStreamingReceiver* receiver,
    const float* samples,
    size_t sampleCount,
    HFTextDecodeResult* results,
    size_t resultCapacity,
    size_t* outResultCount,
    HFTextStreamingReceiverEvent* events,
    size_t eventCapacity,
    size_t* outEventCount,
    char* errorMessage,
    size_t errorMessageSize
) {
    clearError(errorMessage, errorMessageSize);
    if (outResultCount != nullptr) {
        *outResultCount = 0;
    }
    if (outEventCount != nullptr) {
        *outEventCount = 0;
    }
    if (receiver == nullptr || outResultCount == nullptr || outEventCount == nullptr) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }
    if (samples == nullptr && sampleCount > 0) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }
    if ((results == nullptr && resultCapacity > 0) || (events == nullptr && eventCapacity > 0)) {
        writeError(errorMessage, errorMessageSize, "null output buffer");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }

    try {
        std::vector<float> sampleVector;
        if (sampleCount > 0) {
            sampleVector.assign(samples, samples + sampleCount);
        }
        const auto decoded = receiver->receiver.pushSamples(sampleVector);
        const auto receiverEvents = receiver->receiver.takeEvents();

        *outResultCount = decoded.size();
        *outEventCount = receiverEvents.size();

        const auto resultCopyCount = (std::min)(decoded.size(), resultCapacity);
        for (std::size_t index = 0; index < resultCopyCount; ++index) {
            fillDecodeResult(decoded[index], results[index]);
        }

        const auto eventCopyCount = (std::min)(receiverEvents.size(), eventCapacity);
        for (std::size_t index = 0; index < eventCopyCount; ++index) {
            events[index] = fromCppEvent(receiverEvents[index]);
        }

        return HFTEXT_STATUS_OK;
    } catch (const std::exception& exc) {
        *outResultCount = 0;
        *outEventCount = 0;
        return exceptionStatus(exc, errorMessage, errorMessageSize);
    }
}

int32_t hftext_c_summarize_rx_events(
    const HFTextStreamingReceiverEvent* events,
    size_t eventCount,
    HFTextRxEventSummary* outSummary,
    char* errorMessage,
    size_t errorMessageSize
) {
    clearError(errorMessage, errorMessageSize);
    resetRxEventSummary(outSummary);
    if (outSummary == nullptr) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }
    if (events == nullptr && eventCount > 0) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }

    try {
        std::vector<hftext::StreamingReceiverEvent> cppEvents;
        cppEvents.reserve(eventCount);
        for (std::size_t index = 0; index < eventCount; ++index) {
            cppEvents.push_back(toCppEvent(events[index]));
        }

        const auto selection = hftext::selectRxEvents(cppEvents);
        const auto counts = hftext::rxSessionEventCounts(cppEvents);

        outSummary->best_decoded_index = selection.bestDecoded;
        outSummary->best_length_index = selection.bestLength;
        outSummary->best_waiting_index = selection.bestWaiting;
        outSummary->best_sync_index = selection.bestSync;
        outSummary->best_rejected_index = selection.bestRejected;
        outSummary->rejected_count = selection.rejectedCount;
        outSummary->has_invalid_length = selection.hasInvalidLength ? 1 : 0;
        outSummary->sync_count = counts.sync;
        outSummary->length_count = counts.length;
        outSummary->rejected_event_count = counts.rejected;
        outSummary->quality_permille = hftext::rxQualityPermille(cppEvents);
        outSummary->has_terminal_candidate = hftext::hasTerminalRxCandidate(cppEvents) ? 1 : 0;
        return HFTEXT_STATUS_OK;
    } catch (const std::exception& exc) {
        resetRxEventSummary(outSummary);
        return exceptionStatus(exc, errorMessage, errorMessageSize);
    }
}

int32_t hftext_c_estimate_transmission(
    const char* callsignUtf8,
    const char* messageUtf8,
    const HFTextModemConfig* config,
    HFTextTransmissionEstimate* outEstimate,
    char* errorMessage,
    size_t errorMessageSize
) {
    clearError(errorMessage, errorMessageSize);
    if (messageUtf8 == nullptr || config == nullptr || outEstimate == nullptr) {
        writeError(errorMessage, errorMessageSize, "null argument");
        return HFTEXT_STATUS_INVALID_ARGUMENT;
    }

    try {
        const std::string callsign = callsignUtf8 == nullptr ? std::string() : std::string(callsignUtf8);
        const std::string message(messageUtf8);
        const auto estimate = hftext::estimateTransmission(callsign, message, toCppConfig(*config));
        *outEstimate = fromCppEstimate(estimate);
        return HFTEXT_STATUS_OK;
    } catch (const std::exception& exc) {
        return exceptionStatus(exc, errorMessage, errorMessageSize);
    }
}
