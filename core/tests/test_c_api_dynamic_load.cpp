#include "hftext_c_api.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

class SharedLibrary {
public:
    explicit SharedLibrary(const char* path) {
#if defined(_WIN32)
        handle_ = LoadLibraryA(path);
#else
        handle_ = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
        if (handle_ == nullptr) {
            std::cerr << "Could not load shared library: " << path << "\n";
#if !defined(_WIN32)
            const char* error = dlerror();
            if (error != nullptr) {
                std::cerr << error << "\n";
            }
#endif
        }
    }

    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;

    ~SharedLibrary() {
        if (handle_ != nullptr) {
#if defined(_WIN32)
            FreeLibrary(handle_);
#else
            dlclose(handle_);
#endif
        }
    }

    bool loaded() const {
        return handle_ != nullptr;
    }

    template <typename Function>
    Function symbol(const char* name) const {
#if defined(_WIN32)
        auto* address = reinterpret_cast<void*>(GetProcAddress(handle_, name));
#else
        dlerror();
        void* address = dlsym(handle_, name);
#endif
        if (address == nullptr) {
            std::cerr << "Missing exported symbol: " << name << "\n";
#if !defined(_WIN32)
            const char* error = dlerror();
            if (error != nullptr) {
                std::cerr << error << "\n";
            }
#endif
            return nullptr;
        }
        return reinterpret_cast<Function>(address);
    }

private:
#if defined(_WIN32)
    HMODULE handle_ = nullptr;
#else
    void* handle_ = nullptr;
#endif
};

bool require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <hftext_c_api shared library>\n";
        return 2;
    }

    SharedLibrary library(argv[1]);
    if (!library.loaded()) {
        return 1;
    }

    using StringFn = const char* (*)();
    using DefaultProfilesFn = int32_t (*)(HFTextAppModemProfiles*);
    using ModemConfigForProfileFn = int32_t (*)(
        const HFTextAppModemProfiles*,
        HFTextSpeedProfile,
        int32_t,
        HFTextModemConfig*,
        char*,
        size_t
    );
    using StreamingReceiverCreateFn = int32_t (*)(
        const HFTextModemConfig*,
        HFTextStreamingReceiver**,
        char*,
        size_t
    );
    using StreamingReceiverFreeFn = void (*)(HFTextStreamingReceiver*);

    auto applicationName = library.symbol<StringFn>("hftext_c_application_name");
    auto version = library.symbol<StringFn>("hftext_c_version");
    auto defaultProfiles =
        library.symbol<DefaultProfilesFn>("hftext_c_default_app_modem_profiles");
    auto modemConfigForProfile =
        library.symbol<ModemConfigForProfileFn>("hftext_c_modem_config_for_profile");
    auto streamingReceiverCreate =
        library.symbol<StreamingReceiverCreateFn>("hftext_c_streaming_receiver_create");
    auto streamingReceiverFree =
        library.symbol<StreamingReceiverFreeFn>("hftext_c_streaming_receiver_free");

    if (applicationName == nullptr ||
        version == nullptr ||
        defaultProfiles == nullptr ||
        modemConfigForProfile == nullptr ||
        streamingReceiverCreate == nullptr ||
        streamingReceiverFree == nullptr) {
        return 1;
    }

    if (!require(std::strcmp(applicationName(), "HFText") == 0, "Unexpected application name")) {
        return 1;
    }
    if (!require(std::strlen(version()) > 0, "Version string is empty")) {
        return 1;
    }

    HFTextAppModemProfiles profiles{};
    if (!require(
            defaultProfiles(&profiles) == HFTEXT_STATUS_OK,
            "Could not load default modem profiles"
        )) {
        return 1;
    }
    if (!require(
            profiles.slow.modulation_mode == HFTEXT_MODULATION_8FSK,
            "Unexpected slow-profile modulation"
        )) {
        return 1;
    }

    HFTextModemConfig config{};
    char error[128] = {};
    if (!require(
            modemConfigForProfile(
                &profiles,
                HFTEXT_SPEED_PROFILE_FAST,
                profiles.rx_sample_rate,
                &config,
                error,
                sizeof(error)
            ) == HFTEXT_STATUS_OK,
            "Could not derive fast modem config"
        )) {
        std::cerr << error << "\n";
        return 1;
    }

    HFTextStreamingReceiver* receiver = nullptr;
    if (!require(
            streamingReceiverCreate(&config, &receiver, error, sizeof(error)) == HFTEXT_STATUS_OK,
            "Could not create streaming receiver through dynamic C ABI"
        )) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!require(receiver != nullptr, "Streaming receiver handle is null")) {
        return 1;
    }

    streamingReceiverFree(receiver);
    return 0;
}
