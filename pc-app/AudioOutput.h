#pragma once

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
    void stop();

private:
    void playThread(std::string path, unsigned int deviceId);

    std::mutex mutex_;
    std::thread thread_;
#ifdef _WIN32
    HWAVEOUT currentHandle_ = nullptr;
#endif
};
