#include "AudioInput.h"

#include "wav_io.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace {

#ifdef _WIN32
constexpr unsigned int kDefaultDeviceId = WAVE_MAPPER;
constexpr int kChannelCount = 1;
constexpr int kBitsPerSample = 16;
constexpr int kBufferCount = 4;
constexpr int kBufferMilliseconds = 100;

std::string wideToUtf8(const wchar_t* text) {
    const int required = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), required, nullptr, nullptr);
    result.resize(static_cast<std::size_t>(required - 1));
    return result;
}

void checkMmResult(MMRESULT result, const char* message) {
    if (result != MMSYSERR_NOERROR) {
        throw std::runtime_error(message);
    }
}

float pcm16ToFloat(std::int16_t sample) {
    if (sample < 0) {
        return static_cast<float>(sample) / 32768.0F;
    }
    return static_cast<float>(sample) / 32767.0F;
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
    result.push_back(DeviceInfo{kDefaultDeviceId, "Dispositivo padrao do Windows"});

    const UINT count = waveInGetNumDevs();
    for (UINT id = 0; id < count; ++id) {
        WAVEINCAPSW caps = {};
        if (waveInGetDevCapsW(id, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            result.push_back(DeviceInfo{id, wideToUtf8(caps.szPname)});
        }
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

void AudioInput::start(unsigned int deviceId, int sampleRate) {
    if (sampleRate <= 0) {
        throw std::invalid_argument("sample rate invalido");
    }
    (void)stopAndSave({});

    {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.clear();
        lastError_.clear();
        sampleRate_ = sampleRate;
        peak_ = 0.0F;
        clippedSamples_ = 0;
    }
    level_ = 0.0F;
    stopRequested_ = false;
    recording_ = true;
    thread_ = std::thread(&AudioInput::recordThread, this, deviceId, sampleRate);
}

AudioInput::CaptureStats AudioInput::stopAndSave(const std::string& path) {
    stopRequested_ = true;
#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (currentHandle_ != nullptr) {
            waveInReset(currentHandle_);
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
            samples = samples_;
            savedSampleRate = sampleRate_;
        }
        hftext::tools::writeMonoPcm16Wav(path, samples, savedSampleRate);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    return CaptureStats{
        sampleRate_,
        samples_.size(),
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

void AudioInput::recordThread(unsigned int deviceId, int sampleRate) {
#ifdef _WIN32
    HWAVEIN handle = nullptr;
    std::vector<std::vector<std::int16_t>> buffers;
    std::vector<WAVEHDR> headers;

    try {
        WAVEFORMATEX format = {};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = kChannelCount;
        format.nSamplesPerSec = static_cast<DWORD>(sampleRate);
        format.wBitsPerSample = kBitsPerSample;
        format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

        checkMmResult(waveInOpen(&handle, deviceId, &format, 0, 0, CALLBACK_NULL), "falha ao abrir entrada de audio");
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
            checkMmResult(waveInPrepareHeader(handle, &headers[index], sizeof(WAVEHDR)), "falha ao preparar buffer RX");
            checkMmResult(waveInAddBuffer(handle, &headers[index], sizeof(WAVEHDR)), "falha ao adicionar buffer RX");
        }

        checkMmResult(waveInStart(handle), "falha ao iniciar RX");

        while (!stopRequested_) {
            for (int index = 0; index < kBufferCount; ++index) {
                auto& header = headers[index];
                if ((header.dwFlags & WHDR_DONE) == 0) {
                    continue;
                }

                const auto* raw = reinterpret_cast<const std::int16_t*>(header.lpData);
                const std::size_t sampleCount = header.dwBytesRecorded / sizeof(std::int16_t);
                float peak = 0.0F;
                std::size_t clippedSamples = 0;
                std::vector<float> chunk;
                chunk.reserve(sampleCount);
                for (std::size_t sample = 0; sample < sampleCount; ++sample) {
                    const float value = pcm16ToFloat(raw[sample]);
                    const float absValue = std::abs(value);
                    peak = (std::max)(peak, absValue);
                    if (absValue >= 0.98F) {
                        ++clippedSamples;
                    }
                    chunk.push_back(value);
                }

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    samples_.insert(samples_.end(), chunk.begin(), chunk.end());
                    peak_ = (std::max)(peak_, peak);
                    clippedSamples_ += clippedSamples;
                }
                level_ = peak;

                SamplesCallback callback;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    callback = samplesCallback_;
                }
                if (callback) {
                    callback(chunk);
                }

                header.dwBytesRecorded = 0;
                header.dwFlags &= ~WHDR_DONE;
                checkMmResult(waveInAddBuffer(handle, &header, sizeof(WAVEHDR)), "falha ao reciclar buffer RX");
            }
            Sleep(20);
        }

        waveInStop(handle);
        waveInReset(handle);
        for (auto& header : headers) {
            waveInUnprepareHeader(handle, &header, sizeof(WAVEHDR));
        }
        waveInClose(handle);
        handle = nullptr;
    } catch (const std::exception& exc) {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_ = exc.what();
    }

    if (handle != nullptr) {
        waveInReset(handle);
        for (auto& header : headers) {
            waveInUnprepareHeader(handle, &header, sizeof(WAVEHDR));
        }
        waveInClose(handle);
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentHandle_ = nullptr;
    }
    recording_ = false;
#else
    (void)deviceId;
    (void)sampleRate;
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_ = "captura de audio ainda nao suportada nesta plataforma";
    recording_ = false;
#endif
}
