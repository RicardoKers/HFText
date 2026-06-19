#include "hftext_c_api.h"

#include <jni.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

jstring toJString(JNIEnv* env, const char* value) {
    return env->NewStringUTF(value == nullptr ? "" : value);
}

jstring toJString(JNIEnv* env, const std::string& value) {
    return env->NewStringUTF(value.c_str());
}

class JniString {
public:
    JniString(JNIEnv* env, jstring value)
        : env_(env),
          value_(value),
          chars_(value == nullptr ? nullptr : env->GetStringUTFChars(value, nullptr)) {}

    JniString(const JniString&) = delete;
    JniString& operator=(const JniString&) = delete;

    ~JniString() {
        if (chars_ != nullptr) {
            env_->ReleaseStringUTFChars(value_, chars_);
        }
    }

    const char* c_str() const {
        return chars_ == nullptr ? "" : chars_;
    }

private:
    JNIEnv* env_;
    jstring value_;
    const char* chars_;
};

std::string intString(std::int32_t value) {
    return std::to_string(value);
}

std::string sizeString(std::size_t value) {
    return std::to_string(value);
}

std::string boolString(bool value) {
    return value ? "1" : "0";
}

std::string doubleString(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << value;
    return out.str();
}

std::string joinMessages(const std::vector<std::string>& messages) {
    std::ostringstream out;
    for (std::size_t index = 0; index < messages.size(); ++index) {
        if (index > 0) {
            out << '\n';
        }
        out << messages[index];
    }
    return out.str();
}

jobjectArray stringArray(JNIEnv* env, const std::vector<std::string>& values) {
    jclass stringClass = env->FindClass("java/lang/String");
    jstring empty = env->NewStringUTF("");
    auto array = env->NewObjectArray(
        static_cast<jsize>(values.size()),
        stringClass,
        empty
    );
    env->DeleteLocalRef(empty);
    if (array == nullptr) {
        return nullptr;
    }

    for (jsize index = 0; index < static_cast<jsize>(values.size()); ++index) {
        jstring item = env->NewStringUTF(values[static_cast<std::size_t>(index)].c_str());
        env->SetObjectArrayElement(array, index, item);
        env->DeleteLocalRef(item);
    }
    return array;
}

std::string cApiError(const char* context, int32_t status, const char* error) {
    std::ostringstream out;
    out << context << " failed (" << status << ")";
    if (error != nullptr && error[0] != '\0') {
        out << ": " << error;
    }
    return out.str();
}

bool loadProfiles(HFTextAppModemProfiles& profiles, std::string& errorText) {
    const auto status = hftext_c_default_app_modem_profiles(&profiles);
    if (status != HFTEXT_STATUS_OK) {
        errorText = cApiError("default profiles", status, "");
        return false;
    }
    return true;
}

bool speedProfileFromInt(jint value, HFTextSpeedProfile& profile, std::string& errorText) {
    if (value == static_cast<jint>(HFTEXT_SPEED_PROFILE_SLOW)) {
        profile = HFTEXT_SPEED_PROFILE_SLOW;
        return true;
    }
    if (value == static_cast<jint>(HFTEXT_SPEED_PROFILE_FAST)) {
        profile = HFTEXT_SPEED_PROFILE_FAST;
        return true;
    }

    errorText = "invalid speed profile";
    return false;
}

void throwJavaException(JNIEnv* env, const char* className, const std::string& message) {
    jclass exceptionClass = env->FindClass(className);
    if (exceptionClass != nullptr) {
        env->ThrowNew(exceptionClass, message.c_str());
        env->DeleteLocalRef(exceptionClass);
    }
}

const char* modulationName(HFTextModulationMode mode) {
    switch (mode) {
    case HFTEXT_MODULATION_2FSK:
        return "2-FSK";
    case HFTEXT_MODULATION_4FSK:
        return "4-FSK";
    case HFTEXT_MODULATION_8FSK:
        return "8-FSK";
    }
    return "unknown";
}

std::string profileSummary(HFTextSpeedProfile profile) {
    HFTextAppModemProfiles profiles{};
    std::string errorText;
    if (!loadProfiles(profiles, errorText)) {
        return errorText;
    }

    HFTextModemConfig config{};
    char error[256] = {};
    auto status = hftext_c_modem_config_for_profile(
        &profiles,
        profile,
        profiles.rx_sample_rate,
        &config,
        error,
        sizeof(error)
    );
    if (status != HFTEXT_STATUS_OK) {
        return cApiError("profile config", status, error);
    }

    HFTextToneFrequencies tones{};
    status = hftext_c_tone_frequencies(&config, &tones, error, sizeof(error));
    if (status != HFTEXT_STATUS_OK) {
        return cApiError("tone list", status, error);
    }

    std::ostringstream out;
    out << modulationName(config.modulation_mode)
        << ", " << std::fixed << std::setprecision(3)
        << config.symbol_duration_sec << " s/symbol";

    if (tones.tone_count > 0) {
        const auto first = static_cast<int>(std::lround(tones.frequencies_hz[0]));
        const auto last = static_cast<int>(
            std::lround(tones.frequencies_hz[tones.tone_count - 1])
        );
        out << ", " << first << "-" << last << " Hz";
    }

    return out.str();
}

bool estimateForProfile(
    const HFTextAppModemProfiles& profiles,
    HFTextSpeedProfile profile,
    const char* callsign,
    const char* message,
    HFTextTransmissionEstimate& estimate,
    std::string& errorText
) {
    HFTextModemConfig config{};
    char error[256] = {};
    auto status = hftext_c_modem_config_for_profile(
        &profiles,
        profile,
        profiles.tx_sample_rate,
        &config,
        error,
        sizeof(error)
    );
    if (status != HFTEXT_STATUS_OK) {
        errorText = cApiError("profile config", status, error);
        return false;
    }

    status = hftext_c_estimate_transmission(
        callsign,
        message,
        &config,
        &estimate,
        error,
        sizeof(error)
    );
    if (status != HFTEXT_STATUS_OK) {
        errorText = cApiError("estimate", status, error);
        return false;
    }

    return true;
}

bool txConfigForProfile(
    jint profileValue,
    HFTextModemConfig& config,
    std::string& errorText
) {
    HFTextSpeedProfile profile{};
    if (!speedProfileFromInt(profileValue, profile, errorText)) {
        return false;
    }

    HFTextAppModemProfiles profiles{};
    if (!loadProfiles(profiles, errorText)) {
        return false;
    }

    char error[256] = {};
    const auto status = hftext_c_modem_config_for_profile(
        &profiles,
        profile,
        profiles.tx_sample_rate,
        &config,
        error,
        sizeof(error)
    );
    if (status != HFTEXT_STATUS_OK) {
        errorText = cApiError("profile config", status, error);
        return false;
    }

    return true;
}

bool rxConfigForProfile(
    jint profileValue,
    HFTextModemConfig& config,
    std::string& errorText
) {
    HFTextSpeedProfile profile{};
    if (!speedProfileFromInt(profileValue, profile, errorText)) {
        return false;
    }

    HFTextAppModemProfiles profiles{};
    if (!loadProfiles(profiles, errorText)) {
        return false;
    }

    char error[256] = {};
    const auto status = hftext_c_modem_config_for_profile(
        &profiles,
        profile,
        profiles.rx_sample_rate,
        &config,
        error,
        sizeof(error)
    );
    if (status != HFTEXT_STATUS_OK) {
        errorText = cApiError("profile config", status, error);
        return false;
    }

    return true;
}

std::string eventSummary(const HFTextStreamingReceiverEvent& event) {
    std::ostringstream out;
    switch (event.type) {
    case HFTEXT_RX_EVENT_SYNC_FOUND:
        out << "sync found";
        break;
    case HFTEXT_RX_EVENT_PHYSICAL_LENGTH_RECOVERED:
        out << "PHYS_LENGTH " << event.payload_length << " symbols";
        break;
    case HFTEXT_RX_EVENT_PHYSICAL_LENGTH_INVALID:
        out << "invalid PHYS_LENGTH";
        break;
    case HFTEXT_RX_EVENT_FRAME_WAITING:
        out << "receiving frame " << event.bits_available << "/" << event.bits_expected << " bits";
        break;
    case HFTEXT_RX_EVENT_FRAME_REJECTED:
        out << "candidate rejected";
        break;
    case HFTEXT_RX_EVENT_FRAME_DECODED:
        if (event.crc_ok != 0 && event.payload_valid != 0) {
            out << "frame decoded CRC OK";
        } else {
            out << "frame decoded but rejected";
        }
        break;
    }
    return out.str();
}

bool shouldDisplayEvent(const HFTextStreamingReceiverEvent& event) {
    if (event.type == HFTEXT_RX_EVENT_FRAME_DECODED) {
        return true;
    }
    return event.confidence >= 0.20f;
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeApplicationName(JNIEnv* env, jclass) {
    return toJString(env, hftext_c_application_name());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeVersionLabel(JNIEnv* env, jclass) {
    return toJString(env, hftext_c_version_label());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeReleaseTrack(JNIEnv* env, jclass) {
    return toJString(env, hftext_c_release_track());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeProtocolVersion(JNIEnv* env, jclass) {
    return toJString(env, hftext_c_protocol_version());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeSlowProfileSummary(JNIEnv* env, jclass) {
    return toJString(env, profileSummary(HFTEXT_SPEED_PROFILE_SLOW));
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeFastProfileSummary(JNIEnv* env, jclass) {
    return toJString(env, profileSummary(HFTEXT_SPEED_PROFILE_FAST));
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeTextAnalysis(
    JNIEnv* env,
    jclass,
    jstring callsign,
    jstring message
) {
    JniString callsignChars(env, callsign);
    JniString messageChars(env, message);

    char sanitized[2048] = {};
    char payload[2048] = {};
    char error[256] = {};
    HFTextPreparedText prepared{};
    auto status = hftext_c_prepare_text(
        callsignChars.c_str(),
        messageChars.c_str(),
        sanitized,
        sizeof(sanitized),
        payload,
        sizeof(payload),
        &prepared,
        error,
        sizeof(error)
    );
    if (status != HFTEXT_STATUS_OK) {
        return stringArray(env, {
            "error",
            cApiError("prepare text", status, error),
            "",
            "",
            "0",
            "0",
            "127",
            "0",
            "1",
            "0.000",
            "0",
            "1",
            "0.000",
            "0",
            "1",
        });
    }

    HFTextAppModemProfiles profiles{};
    std::string errorText;
    if (!loadProfiles(profiles, errorText)) {
        return stringArray(env, {
            "error",
            errorText,
            sanitized,
            payload,
            intString(prepared.message_symbols),
            intString(prepared.payload_symbols),
            intString(prepared.max_payload_symbols),
            boolString(prepared.payload_too_long != 0),
            boolString(prepared.message_empty != 0),
            "0.000",
            "0",
            "1",
            "0.000",
            "0",
            "1",
        });
    }

    HFTextTransmissionEstimate slowEstimate{};
    if (!estimateForProfile(
            profiles,
            HFTEXT_SPEED_PROFILE_SLOW,
            callsignChars.c_str(),
            messageChars.c_str(),
            slowEstimate,
            errorText
        )) {
        return stringArray(env, {
            "error",
            errorText,
            sanitized,
            payload,
            intString(prepared.message_symbols),
            intString(prepared.payload_symbols),
            intString(prepared.max_payload_symbols),
            boolString(prepared.payload_too_long != 0),
            boolString(prepared.message_empty != 0),
            "0.000",
            "0",
            "1",
            "0.000",
            "0",
            "1",
        });
    }

    HFTextTransmissionEstimate fastEstimate{};
    if (!estimateForProfile(
            profiles,
            HFTEXT_SPEED_PROFILE_FAST,
            callsignChars.c_str(),
            messageChars.c_str(),
            fastEstimate,
            errorText
        )) {
        return stringArray(env, {
            "error",
            errorText,
            sanitized,
            payload,
            intString(prepared.message_symbols),
            intString(prepared.payload_symbols),
            intString(prepared.max_payload_symbols),
            boolString(prepared.payload_too_long != 0),
            boolString(prepared.message_empty != 0),
            "0.000",
            "0",
            "1",
            "0.000",
            "0",
            "1",
        });
    }

    return stringArray(env, {
        "ok",
        "",
        sanitized,
        payload,
        intString(prepared.message_symbols),
        intString(prepared.payload_symbols),
        intString(prepared.max_payload_symbols),
        boolString(prepared.payload_too_long != 0),
        boolString(prepared.message_empty != 0),
        doubleString(slowEstimate.duration_seconds),
        intString(slowEstimate.transmission_bits),
        boolString(slowEstimate.payload_too_long != 0),
        doubleString(fastEstimate.duration_seconds),
        intString(fastEstimate.transmission_bits),
        boolString(fastEstimate.payload_too_long != 0),
    });
}

extern "C" JNIEXPORT jint JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeTransmitSampleRate(
    JNIEnv* env,
    jclass,
    jint profile
) {
    HFTextModemConfig config{};
    std::string errorText;
    if (!txConfigForProfile(profile, config, errorText)) {
        throwJavaException(env, "java/lang/IllegalArgumentException", errorText);
        return 0;
    }
    return static_cast<jint>(config.sample_rate);
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeTransmitAudioSamples(
    JNIEnv* env,
    jclass,
    jstring callsign,
    jstring message,
    jint profile
) {
    HFTextModemConfig config{};
    std::string errorText;
    if (!txConfigForProfile(profile, config, errorText)) {
        throwJavaException(env, "java/lang/IllegalArgumentException", errorText);
        return nullptr;
    }

    JniString callsignChars(env, callsign);
    JniString messageChars(env, message);

    HFTextFloatAudio audio{};
    char error[256] = {};
    const auto status = hftext_c_generate_transmit_audio(
        callsignChars.c_str(),
        messageChars.c_str(),
        &config,
        &audio,
        error,
        sizeof(error)
    );
    if (status != HFTEXT_STATUS_OK) {
        hftext_c_free_audio(&audio);
        throwJavaException(
            env,
            "java/lang/IllegalArgumentException",
            cApiError("generate transmit audio", status, error)
        );
        return nullptr;
    }

    if (audio.sample_count > static_cast<std::size_t>(std::numeric_limits<jsize>::max())) {
        hftext_c_free_audio(&audio);
        throwJavaException(env, "java/lang/IllegalStateException", "generated audio is too large");
        return nullptr;
    }

    jfloatArray out = env->NewFloatArray(static_cast<jsize>(audio.sample_count));
    if (out != nullptr && audio.sample_count > 0) {
        env->SetFloatArrayRegion(
            out,
            0,
            static_cast<jsize>(audio.sample_count),
            audio.samples
        );
    }

    hftext_c_free_audio(&audio);
    return out;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeReceiveSampleRate(
    JNIEnv* env,
    jclass,
    jint profile
) {
    HFTextModemConfig config{};
    std::string errorText;
    if (!rxConfigForProfile(profile, config, errorText)) {
        throwJavaException(env, "java/lang/IllegalArgumentException", errorText);
        return 0;
    }
    return static_cast<jint>(config.sample_rate);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeAnalyzeAudioSamples(
    JNIEnv* env,
    jclass,
    jfloatArray samples,
    jint sampleRate
) {
    const jsize sampleCount = samples == nullptr ? 0 : env->GetArrayLength(samples);
    jfloat* sampleData = nullptr;
    if (sampleCount > 0) {
        sampleData = env->GetFloatArrayElements(samples, nullptr);
        if (sampleData == nullptr) {
            return stringArray(env, {
                "error",
                "could not access sample buffer",
                "0",
                "0.000",
                "0",
                "0.000",
                "0.000",
            });
        }
    }

    HFTextAudioStats stats{};
    char error[256] = {};
    const auto status = hftext_c_analyze_audio_samples(
        sampleData,
        static_cast<std::size_t>(sampleCount),
        static_cast<std::int32_t>(sampleRate),
        0.98f,
        &stats,
        error,
        sizeof(error)
    );

    if (sampleData != nullptr) {
        env->ReleaseFloatArrayElements(samples, sampleData, JNI_ABORT);
    }

    if (status != HFTEXT_STATUS_OK) {
        return stringArray(env, {
            "error",
            cApiError("analyze audio", status, error),
            "0",
            "0.000",
            "0",
            "0.000",
            "0.000",
        });
    }

    return stringArray(env, {
        "ok",
        "",
        sizeString(stats.sample_count),
        doubleString(stats.peak),
        sizeString(stats.clipped_samples),
        doubleString(stats.clipping_percent),
        doubleString(stats.duration_seconds),
    });
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeReceiverCreate(
    JNIEnv* env,
    jclass,
    jint profile
) {
    HFTextModemConfig config{};
    std::string errorText;
    if (!rxConfigForProfile(profile, config, errorText)) {
        throwJavaException(env, "java/lang/IllegalArgumentException", errorText);
        return 0;
    }

    HFTextStreamingReceiver* receiver = nullptr;
    char error[256] = {};
    const auto status = hftext_c_streaming_receiver_create(
        &config,
        &receiver,
        error,
        sizeof(error)
    );
    if (status != HFTEXT_STATUS_OK || receiver == nullptr) {
        throwJavaException(
            env,
            "java/lang/IllegalStateException",
            cApiError("receiver create", status, error)
        );
        return 0;
    }

    return reinterpret_cast<jlong>(receiver);
}

extern "C" JNIEXPORT void JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeReceiverFree(
    JNIEnv*,
    jclass,
    jlong handle
) {
    auto* receiver = reinterpret_cast<HFTextStreamingReceiver*>(handle);
    hftext_c_streaming_receiver_free(receiver);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_hftext_android_HFTextNativeBridge_nativeReceiverPushSamples(
    JNIEnv* env,
    jclass,
    jlong handle,
    jfloatArray samples
) {
    auto* receiver = reinterpret_cast<HFTextStreamingReceiver*>(handle);
    if (receiver == nullptr) {
        return stringArray(env, {
            "error",
            "receiver is not active",
            "",
            "",
            "-1.000",
            "-1.000",
            "0",
            "0",
            "0",
            "0",
        });
    }

    const jsize sampleCount = samples == nullptr ? 0 : env->GetArrayLength(samples);
    jfloat* sampleData = nullptr;
    if (sampleCount > 0) {
        sampleData = env->GetFloatArrayElements(samples, nullptr);
        if (sampleData == nullptr) {
            return stringArray(env, {
                "error",
                "could not access sample buffer",
                "",
                "",
                "-1.000",
                "-1.000",
                "0",
                "0",
                "0",
                "0",
            });
        }
    }

    constexpr std::size_t kResultCapacity = 4;
    constexpr std::size_t kTextCapacity = 256;
    HFTextDecodeResult results[kResultCapacity] = {};
    char textBuffers[kResultCapacity][kTextCapacity] = {};
    for (std::size_t index = 0; index < kResultCapacity; ++index) {
        results[index].text_utf8 = textBuffers[index];
        results[index].text_size = kTextCapacity;
    }

    constexpr std::size_t kEventCapacity = 128;
    HFTextStreamingReceiverEvent events[kEventCapacity] = {};
    std::size_t resultCount = 0;
    std::size_t eventCount = 0;
    char error[256] = {};
    const auto status = hftext_c_streaming_receiver_push_samples(
        receiver,
        sampleData,
        static_cast<std::size_t>(sampleCount),
        results,
        kResultCapacity,
        &resultCount,
        events,
        kEventCapacity,
        &eventCount,
        error,
        sizeof(error)
    );

    if (sampleData != nullptr) {
        env->ReleaseFloatArrayElements(samples, sampleData, JNI_ABORT);
    }

    if (status != HFTEXT_STATUS_OK) {
        return stringArray(env, {
            "error",
            cApiError("receiver push", status, error),
            "",
            "",
            "-1.000",
            "-1.000",
            "0",
            "0",
            "0",
            sizeString(eventCount),
        });
    }

    std::vector<std::string> messages;
    std::size_t accepted = 0;
    const auto copiedResultCount = std::min(resultCount, kResultCapacity);
    for (std::size_t index = 0; index < copiedResultCount; ++index) {
        const auto& result = results[index];
        if (result.frame_detected != 0 &&
            result.crc_ok != 0 &&
            result.payload_valid != 0 &&
            result.text_truncated == 0 &&
            result.text_utf8 != nullptr) {
            messages.emplace_back(result.text_utf8);
            ++accepted;
        }
    }

    std::string state;
    double progress = -1.0;
    double quality = -1.0;
    std::size_t rejected = 0;
    std::size_t sync = 0;
    const auto copiedEventCount = std::min(eventCount, kEventCapacity);
    for (std::size_t index = 0; index < copiedEventCount; ++index) {
        const auto& event = events[index];
        if (!shouldDisplayEvent(event)) {
            continue;
        }
        state = eventSummary(event);
        if (event.confidence >= 0.0f) {
            quality = static_cast<double>(event.confidence);
        }
        if (event.type == HFTEXT_RX_EVENT_SYNC_FOUND) {
            ++sync;
        } else if (event.type == HFTEXT_RX_EVENT_FRAME_REJECTED) {
            ++rejected;
        } else if (event.type == HFTEXT_RX_EVENT_FRAME_WAITING && event.bits_expected > 0) {
            progress = static_cast<double>(event.bits_available) /
                static_cast<double>(event.bits_expected);
        } else if (event.type == HFTEXT_RX_EVENT_FRAME_DECODED) {
            progress = 1.0;
        }
    }

    return stringArray(env, {
        "ok",
        "",
        joinMessages(messages),
        state,
        doubleString(progress),
        doubleString(quality),
        sizeString(accepted),
        sizeString(rejected),
        sizeString(sync),
        sizeString(eventCount),
    });
}
