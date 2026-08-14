#include "AudioInput.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    AudioInput input;
    const auto devices = input.devices();
    require(!devices.empty(), "Windows audio source catalog is empty");

    bool foundDefaultInput = false;
    bool foundDefaultLoopback = false;
    for (const auto& device : devices) {
        std::cout << device.name << " [" << device.persistentKey << "]\n";
        require(!device.name.empty(), "audio source has no display name");
        require(!device.persistentKey.empty(), "audio source has no persistent key");
        foundDefaultInput = foundDefaultInput || device.persistentKey == "capture:default";
        foundDefaultLoopback = foundDefaultLoopback || device.persistentKey == "loopback:default";
    }

    require(foundDefaultInput, "default recording input is missing");
    require(foundDefaultLoopback, "default output loopback is missing");

    if (argc >= 2 && std::string(argv[1]) == "--capture-default-loopback") {
        const auto defaultLoopback = std::find_if(
            devices.begin(),
            devices.end(),
            [](const AudioInput::DeviceInfo& device) {
                return device.persistentKey == "loopback:default";
            }
        );
        require(defaultLoopback != devices.end(), "default output loopback was not found");

        std::size_t callbackSamples = 0;
        input.setSamplesCallback([&callbackSamples](const std::vector<float>& samples) {
            callbackSamples += samples.size();
        });

        const int captureMilliseconds = argc >= 3 ? std::stoi(argv[2]) : 3250;
        require(captureMilliseconds >= 1000, "loopback diagnostic duration is too short");
        const bool keepOutput = argc >= 4;
        const auto outputPath = keepOutput
            ? std::filesystem::path(argv[3])
            : std::filesystem::temp_directory_path() / "hftext-loopback-continuity-test.wav";
        input.start(*defaultLoopback, 48000);
        std::this_thread::sleep_for(std::chrono::milliseconds(captureMilliseconds));
        const auto stats = input.stopAndSave(outputPath.string());
        if (!keepOutput) {
            std::filesystem::remove(outputPath);
        }

        std::cout << "Loopback duration: " << stats.durationSeconds() << " s" << std::endl;
        std::cout << "Loopback callback samples: " << callbackSamples << std::endl;
        std::cout << "Loopback error: " << input.lastError() << std::endl;
        const double minimumExpectedSeconds = (
            static_cast<double>(captureMilliseconds) / 1000.0
        ) - 0.75;
        require(
            stats.durationSeconds() >= minimumExpectedSeconds,
            "loopback timeline stopped during silence"
        );
        require(callbackSamples == stats.sampleCount, "loopback callback and evidence timelines differ");
    }
#endif
    return 0;
}
