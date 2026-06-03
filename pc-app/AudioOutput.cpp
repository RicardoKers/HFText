#include "AudioOutput.h"

#include "wav_io.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#ifdef _WIN32
#include <limits>
#endif

namespace {

std::int16_t floatToPcm16(float sample) {
    const float clipped = std::clamp(sample, -1.0F, 1.0F);
    if (clipped <= -1.0F) {
        return -32768;
    }
    return static_cast<std::int16_t>(std::lround(clipped * 32767.0F));
}

#ifdef _WIN32
constexpr unsigned int kDefaultDeviceId = WAVE_MAPPER;

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
#endif

}  // namespace

AudioOutput::~AudioOutput() {
    stop();
}

std::vector<AudioOutput::DeviceInfo> AudioOutput::devices() const {
#ifdef _WIN32
    std::vector<DeviceInfo> result;
    result.push_back(DeviceInfo{kDefaultDeviceId, "Dispositivo padrao do Windows"});

    const UINT count = waveOutGetNumDevs();
    for (UINT id = 0; id < count; ++id) {
        WAVEOUTCAPSW caps = {};
        if (waveOutGetDevCapsW(id, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            result.push_back(DeviceInfo{id, wideToUtf8(caps.szPname)});
        }
    }
    return result;
#else
    return {};
#endif
}

void AudioOutput::playWavAsync(const std::string& path, unsigned int deviceId) {
    if (path.empty()) {
        throw std::invalid_argument("caminho do WAV vazio");
    }

    stop();
    durationSeconds_ = 0.0;
    positionSeconds_ = 0.0;
    playing_ = false;
    thread_ = std::thread(&AudioOutput::playThread, this, path, deviceId);
}

void AudioOutput::stop() {
#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (currentHandle_ != nullptr) {
            waveOutReset(currentHandle_);
        }
    }
#endif

    if (thread_.joinable()) {
        thread_.join();
    }
    playing_ = false;
    positionSeconds_ = 0.0;
}

bool AudioOutput::isPlaying() const {
    return playing_.load();
}

double AudioOutput::durationSeconds() const {
    return durationSeconds_.load();
}

double AudioOutput::positionSeconds() const {
    return positionSeconds_.load();
}

void AudioOutput::playThread(std::string path, unsigned int deviceId) {
#ifdef _WIN32
    HWAVEOUT handle = nullptr;
    HANDLE event = nullptr;
    WAVEHDR header = {};
    bool headerPrepared = false;

    try {
        const auto wav = hftext::tools::readPcm16Wav(path);
        if (wav.sampleRate <= 0 || wav.samples.empty()) {
            throw std::runtime_error("WAV sem audio valido");
        }
        durationSeconds_ = static_cast<double>(wav.samples.size()) / static_cast<double>(wav.sampleRate);
        positionSeconds_ = 0.0;

        std::vector<std::int16_t> pcm;
        pcm.reserve(wav.samples.size());
        for (float sample : wav.samples) {
            pcm.push_back(floatToPcm16(sample));
        }

        WAVEFORMATEX format = {};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 1;
        format.nSamplesPerSec = static_cast<DWORD>(wav.sampleRate);
        format.wBitsPerSample = 16;
        format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

        event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (event == nullptr) {
            throw std::runtime_error("falha ao criar evento de audio");
        }

        checkMmResult(
            waveOutOpen(&handle, deviceId, &format, reinterpret_cast<DWORD_PTR>(event), 0, CALLBACK_EVENT),
            "falha ao abrir dispositivo de saida"
        );

        {
            std::lock_guard<std::mutex> lock(mutex_);
            currentHandle_ = handle;
        }

        header.lpData = reinterpret_cast<LPSTR>(pcm.data());
        header.dwBufferLength = static_cast<DWORD>(pcm.size() * sizeof(std::int16_t));

        checkMmResult(waveOutPrepareHeader(handle, &header, sizeof(header)), "falha ao preparar buffer de audio");
        headerPrepared = true;
        checkMmResult(waveOutWrite(handle, &header, sizeof(header)), "falha ao iniciar audio");
        playing_ = true;
        const auto startedAt = std::chrono::steady_clock::now();

        while ((header.dwFlags & WHDR_DONE) == 0) {
            WaitForSingleObject(event, 100);
            const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startedAt).count();
            positionSeconds_ = std::clamp(elapsed, 0.0, durationSeconds_.load());
        }
        positionSeconds_ = durationSeconds_.load();
        playing_ = false;

        waveOutUnprepareHeader(handle, &header, sizeof(header));
        headerPrepared = false;
        waveOutClose(handle);
        handle = nullptr;
        CloseHandle(event);
        event = nullptr;

        std::lock_guard<std::mutex> lock(mutex_);
        currentHandle_ = nullptr;
    } catch (...) {
        playing_ = false;
        positionSeconds_ = 0.0;
        if (handle != nullptr) {
            waveOutReset(handle);
            if (headerPrepared) {
                waveOutUnprepareHeader(handle, &header, sizeof(header));
            }
            waveOutClose(handle);
        }
        if (event != nullptr) {
            CloseHandle(event);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        currentHandle_ = nullptr;
    }
#else
    (void)path;
    (void)deviceId;
    throw std::runtime_error("reproducao de audio ainda nao suportada nesta plataforma");
#endif
}
