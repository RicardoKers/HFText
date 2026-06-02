#pragma once

#include <atomic>
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
    struct DeviceInfo {
        unsigned int id = 0;
        std::string name;
    };

    AudioInput() = default;
    ~AudioInput();

    AudioInput(const AudioInput&) = delete;
    AudioInput& operator=(const AudioInput&) = delete;

    std::vector<DeviceInfo> devices() const;
    void start(unsigned int deviceId, int sampleRate);
    void stopAndSave(const std::string& path);
    bool isRecording() const;
    float level() const;
    std::string lastError() const;

private:
    void recordThread(unsigned int deviceId, int sampleRate);

    mutable std::mutex mutex_;
    std::thread thread_;
    std::vector<float> samples_;
    std::string lastError_;
    int sampleRate_ = 48000;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> recording_{false};
    std::atomic<float> level_{0.0F};
#ifdef _WIN32
    HWAVEIN currentHandle_ = nullptr;
#endif
};
