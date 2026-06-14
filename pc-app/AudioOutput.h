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

class AudioOutput {
public:
    struct DeviceInfo {
        unsigned int id = 0;
        std::string name;
    };

    AudioOutput() = default;
    ~AudioOutput();

    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    std::vector<DeviceInfo> devices() const;
    void playWavAsync(const std::string& path, unsigned int deviceId);
    void playSamplesAsync(std::vector<float> samples, int sampleRate, unsigned int deviceId);
    void stop();
    bool isPlaying() const;
    double durationSeconds() const;
    double positionSeconds() const;

private:
    void playThread(std::string path, unsigned int deviceId);
    void playSamplesThread(std::vector<float> samples, int sampleRate, unsigned int deviceId);
    void playSamplesBlocking(std::vector<float> samples, int sampleRate, unsigned int deviceId);

    std::mutex mutex_;
    std::thread thread_;
    std::atomic<bool> playing_{false};
    std::atomic<double> durationSeconds_{0.0};
    std::atomic<double> positionSeconds_{0.0};
#ifdef _WIN32
    HWAVEOUT currentHandle_ = nullptr;
#endif
};
