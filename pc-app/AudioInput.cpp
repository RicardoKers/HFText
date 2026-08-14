#include "AudioInput.h"

#include "AudioSampleConverter.h"
#include "wav_io.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#include <audioclient.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <wrl/client.h>
#endif

namespace {

constexpr int kRecentCaptureSeconds = 30;

#ifdef _WIN32
using Microsoft::WRL::ComPtr;

constexpr unsigned int kDefaultDeviceId = WAVE_MAPPER;
constexpr int kChannelCount = 1;
constexpr int kBitsPerSample = 16;
constexpr int kBufferCount = 4;
constexpr int kBufferMilliseconds = 100;

class ComRuntime {
public:
    ComRuntime() {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(result) && result != RPC_E_CHANGED_MODE) {
            throw std::runtime_error("failed to initialize Windows audio services");
        }
        shouldUninitialize_ = SUCCEEDED(result);
    }

    ~ComRuntime() {
        if (shouldUninitialize_) {
            CoUninitialize();
        }
    }

private:
    bool shouldUninitialize_ = false;
};

std::string wideToUtf8(const wchar_t* text) {
    if (text == nullptr || *text == L'\0') {
        return {};
    }
    const int required = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), required, nullptr, nullptr);
    result.resize(static_cast<std::size_t>(required - 1));
    return result;
}

std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0
    );
    if (required <= 0) {
        throw std::runtime_error("invalid UTF-8 audio endpoint identifier");
    }
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        required
    );
    return result;
}

void checkMmResult(MMRESULT result, const char* message) {
    if (result != MMSYSERR_NOERROR) {
        throw std::runtime_error(message);
    }
}

void checkHResult(HRESULT result, const char* message) {
    if (SUCCEEDED(result)) {
        return;
    }
    std::ostringstream detail;
    detail << message << " (HRESULT 0x" << std::hex << std::uppercase
           << static_cast<unsigned long>(result) << ')';
    throw std::runtime_error(detail.str());
}

float pcm16ToFloat(std::int16_t sample) {
    if (sample < 0) {
        return static_cast<float>(sample) / 32768.0F;
    }
    return static_cast<float>(sample) / 32767.0F;
}

std::string endpointFriendlyName(IMMDevice* device) {
    ComPtr<IPropertyStore> properties;
    checkHResult(
        device->OpenPropertyStore(STGM_READ, &properties),
        "failed to read output device properties"
    );

    PROPVARIANT value;
    PropVariantInit(&value);
    const HRESULT result = properties->GetValue(PKEY_Device_FriendlyName, &value);
    std::string name;
    if (SUCCEEDED(result) && value.vt == VT_LPWSTR) {
        name = wideToUtf8(value.pwszVal);
    }
    PropVariantClear(&value);
    return name;
}

std::vector<AudioInput::DeviceInfo> loopbackDevices() {
    ComRuntime com;
    ComPtr<IMMDeviceEnumerator> enumerator;
    checkHResult(
        CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            IID_PPV_ARGS(&enumerator)
        ),
        "failed to create the Windows audio device enumerator"
    );

    std::vector<AudioInput::DeviceInfo> result;
    result.push_back(AudioInput::DeviceInfo{
        AudioInput::DeviceKind::OutputLoopback,
        0,
        {},
        "loopback:default",
        "Loopback: Windows default output",
    });

    ComPtr<IMMDeviceCollection> collection;
    checkHResult(
        enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection),
        "failed to enumerate Windows output devices"
    );
    UINT count = 0;
    checkHResult(collection->GetCount(&count), "failed to count Windows output devices");
    for (UINT index = 0; index < count; ++index) {
        ComPtr<IMMDevice> device;
        if (FAILED(collection->Item(index, &device))) {
            continue;
        }

        LPWSTR rawId = nullptr;
        if (FAILED(device->GetId(&rawId)) || rawId == nullptr) {
            continue;
        }
        const std::string endpointId = wideToUtf8(rawId);
        CoTaskMemFree(rawId);

        std::string name;
        try {
            name = endpointFriendlyName(device.Get());
        } catch (...) {
        }
        if (name.empty()) {
            name = "Output device " + std::to_string(index + 1);
        }
        result.push_back(AudioInput::DeviceInfo{
            AudioInput::DeviceKind::OutputLoopback,
            0,
            endpointId,
            "loopback:" + endpointId,
            "Loopback: " + name,
        });
    }
    return result;
}

InterleavedAudioFormat audioFormatFromWaveFormat(const WAVEFORMATEX* waveFormat) {
    if (waveFormat == nullptr || waveFormat->nChannels == 0 || waveFormat->nSamplesPerSec == 0) {
        throw std::runtime_error("output device returned an invalid audio format");
    }

    AudioSampleEncoding encoding;
    int validBits = waveFormat->wBitsPerSample;
    if (waveFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        encoding = AudioSampleEncoding::Float32;
    } else if (waveFormat->wFormatTag == WAVE_FORMAT_PCM) {
        encoding = AudioSampleEncoding::SignedPcm;
    } else if (waveFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE
               && waveFormat->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
        const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(waveFormat);
        validBits = extensible->Samples.wValidBitsPerSample;
        if (IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            encoding = AudioSampleEncoding::Float32;
        } else if (IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) {
            encoding = AudioSampleEncoding::SignedPcm;
        } else {
            throw std::runtime_error("output device uses an unsupported sample encoding");
        }
    } else {
        throw std::runtime_error("output device uses an unsupported audio format");
    }

    return InterleavedAudioFormat{
        encoding,
        static_cast<int>(waveFormat->nChannels),
        static_cast<int>(waveFormat->wBitsPerSample),
        validBits,
        static_cast<int>(waveFormat->nBlockAlign),
    };
}
#endif

}  // namespace

double AudioInput::CaptureStats::durationSeconds() const {
    if (sampleRate <= 0) {
        return 0.0;
    }
    return static_cast<double>(sampleCount) / static_cast<double>(sampleRate);
}

AudioInput::~AudioInput() {
    try {
        (void)stopAndSave({});
    } catch (...) {
    }
}

std::vector<AudioInput::DeviceInfo> AudioInput::devices() const {
#ifdef _WIN32
    std::vector<DeviceInfo> result;
    result.push_back(DeviceInfo{
        DeviceKind::RecordingInput,
        kDefaultDeviceId,
        {},
        "capture:default",
        "Input: Windows default recording device",
    });

    const UINT count = waveInGetNumDevs();
    for (UINT id = 0; id < count; ++id) {
        WAVEINCAPSW caps = {};
        if (waveInGetDevCapsW(id, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            const std::string name = wideToUtf8(caps.szPname);
            result.push_back(DeviceInfo{
                DeviceKind::RecordingInput,
                id,
                {},
                "capture:" + name,
                "Input: " + name,
            });
        }
    }

    try {
        auto outputs = loopbackDevices();
        result.insert(result.end(), outputs.begin(), outputs.end());
    } catch (...) {
    }
    return result;
#else
    return {};
#endif
}

void AudioInput::setSamplesCallback(SamplesCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    samplesCallback_ = std::move(callback);
}

void AudioInput::start(const DeviceInfo& device, int sampleRate) {
    if (sampleRate <= 0) {
        throw std::invalid_argument("invalid sample rate");
    }
    if (device.persistentKey.empty()) {
        throw std::invalid_argument("invalid audio input source");
    }
    (void)stopAndSave({});

    {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.clear();
        lastError_.clear();
        sampleRate_ = sampleRate;
        sampleCount_ = 0;
        peak_ = 0.0F;
        clippedSamples_ = 0;
    }
    level_ = 0.0F;
    stopRequested_ = false;
    recording_ = true;
    thread_ = std::thread(&AudioInput::recordThread, this, device, sampleRate);
}

AudioInput::CaptureStats AudioInput::stopAndSave(const std::string& path) {
    stopRequested_ = true;
#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (currentHandle_ != nullptr) {
            waveInReset(currentHandle_);
        }
        if (currentWakeEvent_ != nullptr) {
            SetEvent(currentWakeEvent_);
        }
    }
#endif

    if (thread_.joinable()) {
        thread_.join();
    }
    recording_ = false;
    level_ = 0.0F;

    if (!path.empty()) {
        std::vector<float> samples;
        int savedSampleRate = 48000;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            samples.assign(samples_.begin(), samples_.end());
            savedSampleRate = sampleRate_;
        }
        hftext::tools::writeMonoPcm16Wav(path, samples, savedSampleRate);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    return CaptureStats{
        sampleRate_,
        sampleCount_,
        peak_,
        clippedSamples_,
    };
}

bool AudioInput::isRecording() const {
    return recording_;
}

float AudioInput::level() const {
    return level_.load();
}

std::string AudioInput::lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

void AudioInput::recordThread(DeviceInfo device, int sampleRate) {
#ifdef _WIN32
    try {
        if (device.kind == DeviceKind::OutputLoopback) {
            recordOutputLoopback(device.endpointId, sampleRate);
        } else {
            recordWaveInput(device.waveDeviceId, sampleRate);
        }
    } catch (const std::exception& exc) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = exc.what();
    }
#else
    (void)device;
    (void)sampleRate;
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_ = "audio capture is not supported on this platform yet";
#endif
    recording_ = false;
}

void AudioInput::deliverSamples(const std::vector<float>& samples, int sampleRate) {
    if (samples.empty()) {
        return;
    }

    float peak = 0.0F;
    std::size_t clippedSamples = 0;
    for (const float value : samples) {
        const float absValue = std::abs(value);
        peak = (std::max)(peak, absValue);
        if (absValue >= 0.98F) {
            ++clippedSamples;
        }
    }

    SamplesCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.insert(samples_.end(), samples.begin(), samples.end());
        const auto maxStoredSamples = static_cast<std::size_t>(
            (std::max)(1, sampleRate * kRecentCaptureSeconds)
        );
        if (samples_.size() > maxStoredSamples) {
            const auto excess = samples_.size() - maxStoredSamples;
            samples_.erase(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(excess));
        }
        sampleCount_ += samples.size();
        peak_ = (std::max)(peak_, peak);
        clippedSamples_ += clippedSamples;
        callback = samplesCallback_;
    }
    level_ = peak;

    if (callback) {
        callback(samples);
    }
}

void AudioInput::recordWaveInput(unsigned int deviceId, int sampleRate) {
#ifdef _WIN32
    HWAVEIN handle = nullptr;
    std::vector<std::vector<std::int16_t>> buffers;
    std::vector<WAVEHDR> headers;

    auto cleanup = [&]() {
        if (handle != nullptr) {
            waveInStop(handle);
            waveInReset(handle);
            for (auto& header : headers) {
                if ((header.dwFlags & WHDR_PREPARED) != 0) {
                    waveInUnprepareHeader(handle, &header, sizeof(WAVEHDR));
                }
            }
            waveInClose(handle);
            handle = nullptr;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        currentHandle_ = nullptr;
    };

    try {
        WAVEFORMATEX format = {};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = kChannelCount;
        format.nSamplesPerSec = static_cast<DWORD>(sampleRate);
        format.wBitsPerSample = kBitsPerSample;
        format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

        checkMmResult(waveInOpen(&handle, deviceId, &format, 0, 0, CALLBACK_NULL), "failed to open audio input");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            currentHandle_ = handle;
        }

        const std::size_t samplesPerBuffer = static_cast<std::size_t>(
            (std::max)(1, sampleRate * kBufferMilliseconds / 1000)
        );
        buffers.resize(kBufferCount);
        headers.resize(kBufferCount);

        for (int index = 0; index < kBufferCount; ++index) {
            buffers[index].resize(samplesPerBuffer);
            headers[index].lpData = reinterpret_cast<LPSTR>(buffers[index].data());
            headers[index].dwBufferLength = static_cast<DWORD>(buffers[index].size() * sizeof(std::int16_t));
            checkMmResult(waveInPrepareHeader(handle, &headers[index], sizeof(WAVEHDR)), "failed to prepare RX buffer");
            checkMmResult(waveInAddBuffer(handle, &headers[index], sizeof(WAVEHDR)), "failed to add RX buffer");
        }

        checkMmResult(waveInStart(handle), "failed to start RX");

        while (!stopRequested_) {
            for (int index = 0; index < kBufferCount; ++index) {
                auto& header = headers[index];
                if ((header.dwFlags & WHDR_DONE) == 0) {
                    continue;
                }

                const auto* raw = reinterpret_cast<const std::int16_t*>(header.lpData);
                const std::size_t sampleCount = header.dwBytesRecorded / sizeof(std::int16_t);
                std::vector<float> chunk;
                chunk.reserve(sampleCount);
                for (std::size_t sample = 0; sample < sampleCount; ++sample) {
                    chunk.push_back(pcm16ToFloat(raw[sample]));
                }

                header.dwBytesRecorded = 0;
                header.dwFlags &= ~WHDR_DONE;
                checkMmResult(waveInAddBuffer(handle, &header, sizeof(WAVEHDR)), "failed to recycle RX buffer");
                deliverSamples(chunk, sampleRate);
            }
            Sleep(20);
        }
        cleanup();
    } catch (...) {
        cleanup();
        throw;
    }
#else
    (void)deviceId;
    (void)sampleRate;
#endif
}

void AudioInput::recordOutputLoopback(const std::string& endpointId, int sampleRate) {
#ifdef _WIN32
    ComRuntime com;
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    ComPtr<IAudioClient> audioClient;
    ComPtr<IAudioCaptureClient> captureClient;
    ComPtr<IAudioClient> keepAliveAudioClient;
    ComPtr<IAudioRenderClient> keepAliveRenderClient;
    WAVEFORMATEX* mixFormat = nullptr;
    HANDLE wakeEvent = nullptr;
    HANDLE keepAliveEvent = nullptr;
    bool captureStarted = false;
    bool keepAliveStarted = false;

    auto cleanup = [&]() {
        if (captureStarted && audioClient != nullptr) {
            audioClient->Stop();
            captureStarted = false;
        }
        if (keepAliveStarted && keepAliveAudioClient != nullptr) {
            keepAliveAudioClient->Stop();
            keepAliveStarted = false;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            currentWakeEvent_ = nullptr;
        }
        if (wakeEvent != nullptr) {
            CloseHandle(wakeEvent);
            wakeEvent = nullptr;
        }
        if (keepAliveEvent != nullptr) {
            CloseHandle(keepAliveEvent);
            keepAliveEvent = nullptr;
        }
        if (mixFormat != nullptr) {
            CoTaskMemFree(mixFormat);
            mixFormat = nullptr;
        }
    };

    try {
        checkHResult(
            CoCreateInstance(
                __uuidof(MMDeviceEnumerator),
                nullptr,
                CLSCTX_ALL,
                IID_PPV_ARGS(&enumerator)
            ),
            "failed to create the Windows audio device enumerator"
        );

        if (endpointId.empty()) {
            checkHResult(
                enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device),
                "failed to open the Windows default output"
            );
        } else {
            const std::wstring wideEndpointId = utf8ToWide(endpointId);
            checkHResult(
                enumerator->GetDevice(wideEndpointId.c_str(), &device),
                "failed to open the selected output device"
            );
        }

        checkHResult(
            device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audioClient),
            "failed to activate output loopback capture"
        );
        checkHResult(audioClient->GetMixFormat(&mixFormat), "failed to read the output mix format");
        const auto inputFormat = audioFormatFromWaveFormat(mixFormat);
        StreamingLinearResampler resampler(
            static_cast<int>(mixFormat->nSamplesPerSec),
            sampleRate
        );
        FixedAudioBlockBuffer blockBuffer(static_cast<std::size_t>(
            (std::max)(1, sampleRate * kBufferMilliseconds / 1000)
        ));

        const DWORD streamFlags = AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
        checkHResult(
            audioClient->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                streamFlags,
                0,
                0,
                mixFormat,
                nullptr
            ),
            "failed to initialize output loopback capture"
        );

        wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (wakeEvent == nullptr) {
            throw std::runtime_error("failed to create output loopback event");
        }
        checkHResult(audioClient->SetEventHandle(wakeEvent), "failed to configure output loopback event");
        checkHResult(
            audioClient->GetService(__uuidof(IAudioCaptureClient), &captureClient),
            "failed to create output loopback reader"
        );

        checkHResult(
            device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &keepAliveAudioClient),
            "failed to activate output loopback keep-alive"
        );
        checkHResult(
            keepAliveAudioClient->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                0,
                0,
                mixFormat,
                nullptr
            ),
            "failed to initialize output loopback keep-alive"
        );
        keepAliveEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (keepAliveEvent == nullptr) {
            throw std::runtime_error("failed to create output loopback keep-alive event");
        }
        checkHResult(
            keepAliveAudioClient->SetEventHandle(keepAliveEvent),
            "failed to configure output loopback keep-alive event"
        );
        checkHResult(
            keepAliveAudioClient->GetService(__uuidof(IAudioRenderClient), &keepAliveRenderClient),
            "failed to create output loopback keep-alive writer"
        );

        UINT32 keepAliveBufferFrames = 0;
        checkHResult(
            keepAliveAudioClient->GetBufferSize(&keepAliveBufferFrames),
            "failed to read output loopback keep-alive buffer size"
        );
        BYTE* keepAliveData = nullptr;
        checkHResult(
            keepAliveRenderClient->GetBuffer(keepAliveBufferFrames, &keepAliveData),
            "failed to reserve output loopback keep-alive buffer"
        );
        checkHResult(
            keepAliveRenderClient->ReleaseBuffer(
                keepAliveBufferFrames,
                AUDCLNT_BUFFERFLAGS_SILENT
            ),
            "failed to prime output loopback keep-alive buffer"
        );
        {
            std::lock_guard<std::mutex> lock(mutex_);
            currentWakeEvent_ = wakeEvent;
        }
        checkHResult(audioClient->Start(), "failed to start output loopback capture");
        captureStarted = true;
        checkHResult(
            keepAliveAudioClient->Start(),
            "failed to start output loopback keep-alive"
        );
        keepAliveStarted = true;

        while (!stopRequested_) {
            const HANDLE events[] = {wakeEvent, keepAliveEvent};
            const DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, 200);
            if (waitResult == WAIT_FAILED) {
                throw std::runtime_error("failed while waiting for output loopback audio");
            }
            if (stopRequested_) {
                break;
            }

            UINT32 packetFrames = 0;
            checkHResult(captureClient->GetNextPacketSize(&packetFrames), "failed to inspect output loopback audio");
            while (packetFrames > 0) {
                BYTE* data = nullptr;
                UINT32 frameCount = 0;
                DWORD flags = 0;
                checkHResult(
                    captureClient->GetBuffer(&data, &frameCount, &flags, nullptr, nullptr),
                    "failed to read output loopback audio"
                );

                std::vector<float> mono;
                try {
                    mono = interleavedAudioToMono(
                        data,
                        frameCount,
                        inputFormat,
                        (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0
                    );
                } catch (...) {
                    captureClient->ReleaseBuffer(frameCount);
                    throw;
                }
                checkHResult(captureClient->ReleaseBuffer(frameCount), "failed to release output loopback audio");
                const auto converted = resampler.process(mono);
                for (const auto& block : blockBuffer.process(converted)) {
                    deliverSamples(block, sampleRate);
                }
                checkHResult(captureClient->GetNextPacketSize(&packetFrames), "failed to inspect output loopback audio");
            }

            UINT32 keepAlivePaddingFrames = 0;
            checkHResult(
                keepAliveAudioClient->GetCurrentPadding(&keepAlivePaddingFrames),
                "failed to inspect output loopback keep-alive audio"
            );
            const UINT32 availableKeepAliveFrames = keepAliveBufferFrames - keepAlivePaddingFrames;
            if (availableKeepAliveFrames > 0) {
                keepAliveData = nullptr;
                checkHResult(
                    keepAliveRenderClient->GetBuffer(availableKeepAliveFrames, &keepAliveData),
                    "failed to reserve output loopback keep-alive audio"
                );
                checkHResult(
                    keepAliveRenderClient->ReleaseBuffer(
                        availableKeepAliveFrames,
                        AUDCLNT_BUFFERFLAGS_SILENT
                    ),
                    "failed to submit output loopback keep-alive audio"
                );
            }
        }
        cleanup();
    } catch (...) {
        cleanup();
        throw;
    }
#else
    (void)endpointId;
    (void)sampleRate;
#endif
}
