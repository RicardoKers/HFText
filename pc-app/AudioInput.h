#pragma once

#include <atomic>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

class AudioInput {
public:
    using SamplesCallback = std::function<void(const std::vector<float>& samples)>;

    struct DeviceInfo {
        unsigned int id = 0;
        std::string name;
    };

    struct CaptureStats {
        int sampleRate = 0;
        std::size_t sampleCount = 0;
        float peak = 0.0F;
        std::size_t clippedSamples = 0;

        double durationSeconds() const;
    };

    AudioInput() = default;
    ~AudioInput();

    AudioInput(const AudioInput&) = delete;
    AudioInput& operator=(const AudioInput&) = delete;

    std::vector<DeviceInfo> devices() const;
    void setSamplesCallback(SamplesCallback callback);
    void start(unsigned int deviceId, int sampleRate);
    CaptureStats stopAndSave(const std::string& path);
    bool isRecording() const;
    float level() const;
    std::string lastError() const;

private:
    void recordThread(unsigned int deviceId, int sampleRate);

    mutable std::mutex mutex_;
    SamplesCallback samplesCallback_;
    std::thread thread_;
    std::deque<float> samples_;
    std::string lastError_;
    int sampleRate_ = 48000;
    std::size_t sampleCount_ = 0;
    float peak_ = 0.0F;
    std::size_t clippedSamples_ = 0;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> recording_{false};
    std::atomic<float> level_{0.0F};
#ifdef _WIN32
    HWAVEIN currentHandle_ = nullptr;
#endif
};
