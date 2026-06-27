#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace hftext::tools {

#ifdef _WIN32
inline std::string wideToUtf8(const wchar_t* text) {
    if (text == nullptr) {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }

    std::string output(static_cast<std::size_t>(required - 1), '\0');
    if (!output.empty()) {
        WideCharToMultiByte(CP_UTF8, 0, text, -1, output.data(), required, nullptr, nullptr);
    }
    return output;
}

inline std::vector<std::string> makeUtf8Args(int argc, wchar_t** argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        args.push_back(wideToUtf8(argv[index]));
    }
    return args;
}
#else
inline std::vector<std::string> makeUtf8Args(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        args.emplace_back(argv[index]);
    }
    return args;
}
#endif

}  // namespace hftext::tools

#ifdef _WIN32
#define HFTEXT_CLI_MAIN(runMainFunction) \
    int wmain(int argc, wchar_t** argv) { \
        return runMainFunction(hftext::tools::makeUtf8Args(argc, argv)); \
    }
#else
#define HFTEXT_CLI_MAIN(runMainFunction) \
    int main(int argc, char** argv) { \
        return runMainFunction(hftext::tools::makeUtf8Args(argc, argv)); \
    }
#endif
