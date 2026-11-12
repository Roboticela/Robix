#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Robix::Tools {

enum class RoboTone : std::uint8_t {
    Plain,
    Title,
    Heading,
    Command,
    Option,
    Success,
    Error,
    Time,
    Dim
};

inline bool& RoboColorEnabledFlag() {
    static bool enabled = false;
    return enabled;
}

inline void RoboSetColorEnabled(bool enabled) {
    RoboColorEnabledFlag() = enabled;
}

inline bool RoboColorEnabled() {
    return RoboColorEnabledFlag();
}

inline bool RoboColorDisabledByEnvironment() {
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, "NO_COLOR") != 0 || value == nullptr) return false;
    const bool disabled = value[0] != '\0';
    std::free(value);
    return disabled;
#else
    const char* noColor = std::getenv("NO_COLOR");
    return noColor != nullptr && noColor[0] != '\0';
#endif
}

inline void RoboEnableConsoleColor() {
    if (RoboColorDisabledByEnvironment()) {
        RoboSetColorEnabled(false);
        return;
    }
#if defined(_WIN32)
    auto enableHandle = [](DWORD standardHandle) {
        HANDLE handle = GetStdHandle(standardHandle);
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return false;
        DWORD mode = 0;
        if (!GetConsoleMode(handle, &mode)) return false;
        const DWORD kVirtualTerminal = 0x0004;
        SetConsoleMode(handle, mode | kVirtualTerminal);
        return true;
    };
    const bool stdoutReady = enableHandle(STD_OUTPUT_HANDLE);
    const bool stderrReady = enableHandle(STD_ERROR_HANDLE);
    SetConsoleOutputCP(65001);
    RoboSetColorEnabled(stdoutReady || stderrReady);
#else
    RoboSetColorEnabled(::isatty(1) != 0 || ::isatty(2) != 0);
#endif
}

inline const char* RoboToneCode(RoboTone tone) {
    switch (tone) {
        case RoboTone::Title: return "\033[1;96m";
        case RoboTone::Heading: return "\033[1;36m";
        case RoboTone::Command: return "\033[32m";
        case RoboTone::Option: return "\033[33m";
        case RoboTone::Success: return "\033[32m";
        case RoboTone::Error: return "\033[31m";
        case RoboTone::Time: return "\033[1;96m";
        case RoboTone::Dim: return "\033[2;90m";
        case RoboTone::Plain: return "";
    }
    return "";
}

inline std::string RoboPaint(std::string_view text, RoboTone tone) {
    if (!RoboColorEnabled() || tone == RoboTone::Plain || text.empty()) {
        return std::string(text);
    }
    return std::string(RoboToneCode(tone)) + std::string(text) + "\033[0m";
}

inline std::string RoboTrimmedNumber(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    std::string text = out.str();
    const std::size_t dot = text.find('.');
    if (dot == std::string::npos) return text;
    while (!text.empty() && text.back() == '0') text.pop_back();
    if (!text.empty() && text.back() == '.') text.pop_back();
    return text;
}

inline std::string RoboFormatDuration(std::int64_t nanoseconds) {
    if (nanoseconds < 0) nanoseconds = 0;
    if (nanoseconds < 1000) {
        return std::to_string(nanoseconds) + " ns";
    }
    if (nanoseconds < 1000000) {
        return RoboTrimmedNumber(static_cast<double>(nanoseconds) / 1000.0, 2) + " us";
    }
    if (nanoseconds < 1000000000LL) {
        return RoboTrimmedNumber(static_cast<double>(nanoseconds) / 1000000.0, 2) + " ms";
    }
    if (nanoseconds < 60000000000LL) {
        return RoboTrimmedNumber(static_cast<double>(nanoseconds) / 1000000000.0, 3) + " s";
    }
    const std::int64_t totalMilliseconds = nanoseconds / 1000000LL;
    const std::int64_t minutes = totalMilliseconds / 60000LL;
    const double seconds = static_cast<double>(totalMilliseconds % 60000LL) / 1000.0;
    return std::to_string(minutes) + "m " + RoboTrimmedNumber(seconds, 1) + "s";
}

inline bool RoboShowExecutionTime(const std::vector<std::string>& args) {
    bool show = true;
    for (const std::string& arg : args) {
        if (arg == "--no-time") show = false;
        else if (arg == "--time") show = true;
    }
    return show;
}

inline void RoboPrintDuration(std::string_view label, std::int64_t nanoseconds) {
    std::cout.flush();
    std::cout << RoboPaint("-- ", RoboTone::Dim)
              << RoboPaint(label, RoboTone::Dim)
              << RoboPaint(RoboFormatDuration(nanoseconds), RoboTone::Time)
              << "\n";
    std::cout.flush();
}

inline void RoboPrintCompileTime(std::int64_t nanoseconds) {
    RoboPrintDuration("Compiled in ", nanoseconds);
}

inline void RoboPrintExecutionTime(std::int64_t nanoseconds) {
    RoboPrintDuration("Ran in ", nanoseconds);
}

} // namespace Robix::Tools
