#include "hftext_c_api.h"

#include "hftext_app_settings.h"
#include "hftext_app_tx.h"
#include "hftext_version.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

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

void resetAudio(HFTextFloatAudio* audio) {
    if (audio == nullptr) {
        return;
    }
    audio->samples = nullptr;
    audio->sample_count = 0;
    audio->sample_rate = 0;
    audio->duration_seconds = 0.0;
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
