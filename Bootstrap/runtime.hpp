// Runtime for programs the C++ bootstrap transpiles from Robix.
#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <future>
#include <thread>
#include <utility>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
extern "C" {
__declspec(dllimport) void* __stdcall GetStdHandle(unsigned long);
__declspec(dllimport) int __stdcall GetConsoleMode(void*, unsigned long*);
__declspec(dllimport) int __stdcall SetConsoleMode(void*, unsigned long);
__declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void*, char*, unsigned long);
__declspec(dllimport) unsigned int __stdcall GetConsoleOutputCP(void);
}
#endif

namespace rx {

struct Bool {
    unsigned char value = 0;
    Bool() = default;
    Bool(bool bit) : value(bit ? 1 : 0) {}
    operator bool() const { return value != 0; }
    Bool& operator=(bool bit) {
        value = bit ? 1 : 0;
        return *this;
    }
};

inline bool operator==(Bool left, Bool right) { return left.value == right.value; }
inline bool operator!=(Bool left, Bool right) { return left.value != right.value; }

struct String {
    bool null = false;
    std::string text;

    static String Null() {
        String value;
        value.null = true;
        return value;
    }

    static String Lit(const std::string& text) {
        String value;
        value.text = text;
        return value;
    }

    static String From(int64_t code) {
        String value;
        uint32_t point = static_cast<uint32_t>(code);
        if (point < 0x80) {
            value.text.push_back(static_cast<char>(point));
        } else if (point < 0x800) {
            value.text.push_back(static_cast<char>(0xC0 | (point >> 6)));
            value.text.push_back(static_cast<char>(0x80 | (point & 0x3F)));
        } else if (point < 0x10000) {
            value.text.push_back(static_cast<char>(0xE0 | (point >> 12)));
            value.text.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3F)));
            value.text.push_back(static_cast<char>(0x80 | (point & 0x3F)));
        } else {
            value.text.push_back(static_cast<char>(0xF0 | (point >> 18)));
            value.text.push_back(static_cast<char>(0x80 | ((point >> 12) & 0x3F)));
            value.text.push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3F)));
            value.text.push_back(static_cast<char>(0x80 | (point & 0x3F)));
        }
        return value;
    }

    bool IsNull() const { return null; }
    int64_t Length() const { return null ? 0 : static_cast<int64_t>(text.size()); }

    int64_t GetChar(int64_t index) const {
        if (null || index < 0 || static_cast<size_t>(index) >= text.size()) return 0;
        return static_cast<unsigned char>(text[static_cast<size_t>(index)]);
    }

    String Substring(int64_t start) const {
        if (null) return Lit("");
        if (start < 0) start = 0;
        if (static_cast<size_t>(start) >= text.size()) return Lit("");
        return Lit(text.substr(static_cast<size_t>(start)));
    }

    String Substring(int64_t start, int64_t count) const {
        if (null) return Lit("");
        if (start < 0) start = 0;
        if (count < 0) count = 0;
        if (static_cast<size_t>(start) >= text.size()) return Lit("");
        return Lit(text.substr(static_cast<size_t>(start), static_cast<size_t>(count)));
    }

    bool StartsWith(const String& prefix) const {
        if (null || prefix.null) return false;
        return text.size() >= prefix.text.size() && text.compare(0, prefix.text.size(), prefix.text) == 0;
    }

    bool EndsWith(const String& suffix) const {
        if (null || suffix.null) return false;
        return text.size() >= suffix.text.size() &&
            text.compare(text.size() - suffix.text.size(), suffix.text.size(), suffix.text) == 0;
    }

    bool Contains(const String& part) const {
        if (null || part.null) return false;
        return text.find(part.text) != std::string::npos;
    }

    int64_t IndexOf(const String& part) const {
        if (null || part.null) return -1;
        auto found = text.find(part.text);
        return found == std::string::npos ? -1 : static_cast<int64_t>(found);
    }

    int64_t LastIndexOf(const String& part) const {
        if (null || part.null) return -1;
        auto found = text.rfind(part.text);
        return found == std::string::npos ? -1 : static_cast<int64_t>(found);
    }

    String Replace(const String& from, const String& to) const {
        if (null) return Lit("");
        if (from.null || from.text.empty()) return *this;
        std::string out;
        size_t cursor = 0;
        while (cursor <= text.size()) {
            auto found = text.find(from.text, cursor);
            if (found == std::string::npos) {
                out.append(text, cursor, std::string::npos);
                break;
            }
            out.append(text, cursor, found - cursor);
            if (!to.null) out.append(to.text);
            cursor = found + from.text.size();
        }
        return Lit(out);
    }

    String ToLower() const {
        if (null) return Lit("");
        std::string out = text;
        for (char& character : out) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        return Lit(out);
    }

    String ToUpper() const {
        if (null) return Lit("");
        std::string out = text;
        for (char& character : out) character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        return Lit(out);
    }

    String Trim() const {
        if (null) return Lit("");
        size_t start = 0;
        while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) start++;
        size_t end = text.size();
        while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) end--;
        return Lit(text.substr(start, end - start));
    }

    int64_t ToInt() const {
        if (null || text.empty()) return 0;
        try {
            size_t used = 0;
            int64_t value = std::stoll(text, &used, 10);
            (void)used;
            return value;
        } catch (...) {
            return 0;
        }
    }

    String ToString() const { return null ? Lit("") : *this; }
};

inline bool operator==(const String& left, const String& right) {
    if (left.null || right.null) return left.null && right.null;
    return left.text == right.text;
}

inline bool operator!=(const String& left, const String& right) { return !(left == right); }

inline String Show(const String& value) { return value.null ? String::Lit("") : value; }
inline String Show(int64_t value) { return String::Lit(std::to_string(value)); }
inline String Show(bool value) { return String::Lit(value ? "True" : "False"); }
inline String Show(double value) { return String::Lit(std::to_string(value)); }

inline String Concat(const String& left, const String& right) {
    return String::Lit(Show(left).text + Show(right).text);
}

template <typename T>
struct List {
    std::shared_ptr<std::vector<T>> data;

    static List Make() {
        List value;
        value.data = std::make_shared<std::vector<T>>();
        return value;
    }

    bool IsNull() const { return data == nullptr; }
    int64_t Count() const { return data ? static_cast<int64_t>(data->size()) : 0; }

    int64_t Add(const T& item) {
        if (!data) data = std::make_shared<std::vector<T>>();
        data->push_back(item);
        return static_cast<int64_t>(data->size());
    }

    int64_t Truncate(int64_t count) {
        if (!data) return 0;
        if (count < 0) count = 0;
        if (static_cast<size_t>(count) < data->size()) data->resize(static_cast<size_t>(count));
        return static_cast<int64_t>(data->size());
    }
};

inline int64_t& ObjectLedger() {
    static int64_t count = 0;
    return count;
}

inline int64_t LiveObjects() { return ObjectLedger(); }

inline std::vector<void*>& RecycleBin(std::size_t bytes) {
    static std::vector<std::vector<void*>> bins(8192);
    static std::vector<void*> unused;
    if (bytes == 0 || bytes >= bins.size()) return unused;
    return bins[bytes];
}

template <typename T>
struct PoolAlloc {
    using value_type = T;
    PoolAlloc() = default;
    template <typename U>
    PoolAlloc(const PoolAlloc<U>&) {}

    T* allocate(std::size_t count) {
        std::size_t bytes = sizeof(T) * count;
        std::vector<void*>& bin = RecycleBin(bytes);
        if (bytes < 8192 && !bin.empty()) {
            void* memory = bin.back();
            bin.pop_back();
            return static_cast<T*>(memory);
        }
        return static_cast<T*>(::operator new(bytes));
    }

    void deallocate(T* pointer, std::size_t count) noexcept {
        if (pointer == nullptr) return;
        std::size_t bytes = sizeof(T) * count;
        if (bytes == 0 || bytes >= 8192) {
            ::operator delete(pointer);
            return;
        }
        RecycleBin(bytes).push_back(pointer);
    }
};

template <typename T, typename U>
inline bool operator==(const PoolAlloc<T>&, const PoolAlloc<U>&) { return true; }

template <typename T, typename U>
inline bool operator!=(const PoolAlloc<T>&, const PoolAlloc<U>&) { return false; }

inline String JoinLines(List<String> lines) {
    std::string text;
    if (!lines.data) return String::Lit("");
    size_t bytes = 0;
    for (size_t index = 0; index < lines.data->size(); index++) {
        if (index > 0) bytes++;
        const String& line = (*lines.data)[index];
        if (!line.null) bytes += line.text.size();
    }
    text.reserve(bytes);
    for (size_t index = 0; index < lines.data->size(); index++) {
        if (index > 0) text.push_back('\n');
        const String& line = (*lines.data)[index];
        if (!line.null) text += line.text;
    }
    return String::Lit(text);
}

inline String JoinText(List<String> lines) { return JoinLines(lines); }

template <typename T>
T& At(List<T>& list, int64_t index) {
    if (!list.data) list.data = std::make_shared<std::vector<T>>();
    if (index < 0) index = 0;
    if (static_cast<size_t>(index) >= list.data->size()) {
        list.data->resize(static_cast<size_t>(index) + 1);
    }
    return (*list.data)[static_cast<size_t>(index)];
}

template <typename T>
T AtValue(List<T> list, int64_t index) {
    return At(list, index);
}

inline String FromChars(List<int64_t> chars) {
    String value;
    if (!chars.data) return value;
    for (int64_t code : *chars.data) value.text += String::From(code).text;
    return value;
}

inline List<String> SplitString(const String& text, const String& separator) {
    List<String> parts = List<String>::Make();
    if (text.null) {
        parts.Add(String::Lit(""));
        return parts;
    }
    if (separator.null || separator.text.empty()) {
        parts.Add(text);
        return parts;
    }
    size_t cursor = 0;
    while (true) {
        size_t found = text.text.find(separator.text, cursor);
        if (found == std::string::npos) {
            parts.Add(String::Lit(text.text.substr(cursor)));
            break;
        }
        parts.Add(String::Lit(text.text.substr(cursor, found - cursor)));
        cursor = found + separator.text.size();
    }
    return parts;
}

struct ExecutionResult {
    int64_t Code = 0;
    String Message;
    String Details;

    static ExecutionResult Make(int64_t code, const String& reason) {
        ExecutionResult value;
        value.Code = code;
        value.Message = reason;
        return value;
    }

    static ExecutionResult Make(int64_t code, const String& reason, const String& details) {
        ExecutionResult value = Make(code, reason);
        value.Details = details;
        return value;
    }
};

namespace ExitCode {
constexpr int64_t Success = 0;
constexpr int64_t SuccessfulExecution = 0;
constexpr int64_t WarningExecution = 1;
constexpr int64_t ErrorExecution = 2;
constexpr int64_t FatalErrorExecution = 3;
constexpr int64_t InvalidArguments = 4;
constexpr int64_t PermissionDenied = 5;
constexpr int64_t ResourceUnavailable = 6;
}  // namespace ExitCode

inline bool TerminalColor() {
    static int enabled = -1;
    if (enabled >= 0) return enabled == 1;
    if (const char* noColor = std::getenv("NO_COLOR")) {
        if (*noColor) {
            enabled = 0;
            return false;
        }
    }
#ifdef _WIN32
    void* handle = GetStdHandle(0xFFFFFFF5u);
    unsigned long mode = 0;
    if (handle == nullptr || handle == reinterpret_cast<void*>(-1) || !GetConsoleMode(handle, &mode)) {
        enabled = 0;
        return false;
    }
    mode |= 0x0004u;
    if (!SetConsoleMode(handle, mode)) {
        enabled = 0;
        return false;
    }
    enabled = 1;
    return true;
#else
    enabled = ::isatty(1) ? 1 : 0;
    return enabled == 1;
#endif
}

inline int HexDigit(char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

inline int ColorLevel() {
    const char* colorterm = std::getenv("COLORTERM");
    if (colorterm != nullptr) {
        std::string value(colorterm);
        if (value.find("truecolor") != std::string::npos || value.find("24bit") != std::string::npos) return 2;
    }
    const char* session = std::getenv("WT_SESSION");
    if (session != nullptr && session[0] != '\0') return 2;
    const char* term = std::getenv("TERM");
    if (term != nullptr && std::string(term).find("256") != std::string::npos) return 1;
    return 0;
}

inline int Approx256(int red, int green, int blue) {
    int scaledRed = red * 5 / 255;
    int scaledGreen = green * 5 / 255;
    int scaledBlue = blue * 5 / 255;
    return 16 + 36 * scaledRed + 6 * scaledGreen + scaledBlue;
}

inline int Approx16(int red, int green, int blue) {
    int best = 37;
    int bestDistance = 1 << 30;
    const int sampleRed[8] = {239, 22, 234, 59, 6, 168, 229, 148};
    const int sampleGreen[8] = {68, 163, 179, 130, 182, 85, 231, 163};
    const int sampleBlue[8] = {68, 74, 8, 246, 212, 247, 235, 184};
    const int sampleCode[8] = {31, 32, 33, 34, 36, 35, 37, 90};
    for (int index = 0; index < 8; index++) {
        int deltaRed = red - sampleRed[index];
        int deltaGreen = green - sampleGreen[index];
        int deltaBlue = blue - sampleBlue[index];
        int distance = deltaRed * deltaRed + deltaGreen * deltaGreen + deltaBlue * deltaBlue;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = sampleCode[index];
        }
    }
    return best;
}

inline std::string PaintColor(int red, int green, int blue, int index256, int code16) {
    int level = ColorLevel();
    if (level >= 2) return "\033[38;2;" + std::to_string(red) + ";" + std::to_string(green) + ";" + std::to_string(blue) + "m";
    if (level == 1) {
        int index = index256 >= 0 ? index256 : Approx256(red, green, blue);
        return "\033[38;5;" + std::to_string(index) + "m";
    }
    int code = code16 >= 0 ? code16 : Approx16(red, green, blue);
    return "\033[" + std::to_string(code) + "m";
}

inline bool RoleColor(const std::string& tag, int& red, int& green, int& blue, int& index256, int& code16) {
    index256 = -1;
    code16 = -1;
    if (tag == "red" || tag == "error") {
        red = 239;
        green = 68;
        blue = 68;
        index256 = 203;
        code16 = 31;
        return true;
    }
    if (tag == "green" || tag == "success" || tag == "accent" || tag == "primary") {
        red = 22;
        green = 163;
        blue = 74;
        index256 = 34;
        code16 = 32;
        return true;
    }
    if (tag == "yellow" || tag == "warning") {
        red = 234;
        green = 179;
        blue = 8;
        index256 = 178;
        code16 = 33;
        return true;
    }
    if (tag == "blue") {
        red = 59;
        green = 130;
        blue = 246;
        index256 = 69;
        code16 = 34;
        return true;
    }
    if (tag == "cyan" || tag == "info") {
        red = 6;
        green = 182;
        blue = 212;
        index256 = 38;
        code16 = 36;
        return true;
    }
    if (tag == "magenta" || tag == "package") {
        red = 168;
        green = 85;
        blue = 247;
        index256 = 141;
        code16 = 35;
        return true;
    }
    if (tag == "orange" || tag == "debug") {
        red = 249;
        green = 115;
        blue = 22;
        index256 = 208;
        code16 = 33;
        return true;
    }
    if (tag == "white" || tag == "text") {
        red = 229;
        green = 231;
        blue = 235;
        index256 = 254;
        code16 = 37;
        return true;
    }
    if (tag == "gray" || tag == "grey" || tag == "muted" || tag == "secondary") {
        red = 148;
        green = 163;
        blue = 184;
        index256 = 248;
        code16 = 90;
        return true;
    }
    if (tag.size() == 7 && tag[0] == '#') {
        int channels[3] = {};
        for (int channel = 0; channel < 3; channel++) {
            int high = HexDigit(tag[1 + channel * 2]);
            int low = HexDigit(tag[2 + channel * 2]);
            if (high < 0 || low < 0) return false;
            channels[channel] = high * 16 + low;
        }
        red = channels[0];
        green = channels[1];
        blue = channels[2];
        return true;
    }
    if (tag.rfind("rgb:", 0) == 0) {
        int channels[3] = {};
        int slot = 0;
        int value = 0;
        bool seen = false;
        for (size_t index = 4; index <= tag.size() && slot < 3; index++) {
            char mark = index < tag.size() ? tag[index] : ',';
            if (mark >= '0' && mark <= '9') {
                value = value * 10 + (mark - '0');
                if (value > 255) value = 255;
                seen = true;
            } else if (mark == ',' && seen) {
                channels[slot++] = value;
                value = 0;
                seen = false;
            } else {
                return false;
            }
        }
        if (slot != 3) return false;
        red = channels[0];
        green = channels[1];
        blue = channels[2];
        return true;
    }
    return false;
}

inline std::string FormatConsole(const std::string& text) {
    bool color = TerminalColor();
    std::string built;
    built.reserve(text.size());
    for (size_t index = 0; index < text.size();) {
        if (text[index] != '{') {
            built.push_back(text[index]);
            index++;
            continue;
        }
        size_t close = text.find('}', index + 1);
        if (close == std::string::npos) {
            built.append(text, index, std::string::npos);
            break;
        }
        std::string tag = text.substr(index + 1, close - index - 1);
        if (tag.empty() || tag == "/" || tag == "reset") {
            if (color) built += "\033[0m";
        } else if (tag == "bold") {
            if (color) built += "\033[1m";
        } else {
            int red = 0;
            int green = 0;
            int blue = 0;
            int index256 = -1;
            int code16 = -1;
            if (!RoleColor(tag, red, green, blue, index256, code16)) built.append(text, index, close - index + 1);
            else if (color) built += PaintColor(red, green, blue, index256, code16);
        }
        index = close + 1;
    }
    if (color) built += "\033[0m";
    return built;
}

inline void Print(const String& text) {
    std::cout << FormatConsole(text.null ? "" : text.text) << std::endl;
}

inline void SleepMillis(int64_t millis) {
    if (millis < 0) millis = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<uint64_t>(millis)));
}

inline int64_t ClockMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

inline String ReadLine() {
    std::string line;
    if (!std::getline(std::cin, line)) return String::Lit("");
    return String::Lit(line);
}

inline String Input() {
    return ReadLine();
}

inline String ReadConsoleLine() {
    return Input();
}

inline void ClearScreen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

inline String HostSystem() {
#ifdef _WIN32
    return String::Lit("Windows");
#else
    return String::Lit("Linux");
#endif
}

inline String HostArchitecture() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return String::Lit("ARM64");
#else
    return String::Lit("x64");
#endif
}

inline String ToolchainPath() {
    return String::Lit(std::filesystem::current_path().string());
}

inline String UserConfigPath() {
    const char* home = std::getenv("USERPROFILE");
    if (home == nullptr || home[0] == '\0') home = std::getenv("HOME");
    if (home == nullptr || home[0] == '\0') return String::Lit(".robix/config");
    return String::Lit(std::string(home) + "/.robix/config");
}

inline bool UnicodeTerminal() {
    const char* language = std::getenv("LANG");
    if (language != nullptr && std::string(language).find("UTF") != std::string::npos) return true;
    const char* session = std::getenv("WT_SESSION");
    if (session != nullptr && session[0] != '\0') return true;
#ifdef _WIN32
    if (GetConsoleOutputCP() == 65001) return true;
#endif
    return false;
}

inline std::string ExecutablePath() {
#ifdef _WIN32
    char buffer[4096];
    unsigned long count = GetModuleFileNameA(nullptr, buffer, 4096);
    if (count == 0 || count >= 4096) return "Robix.exe";
    return std::string(buffer, count);
#else
    char buffer[4096];
    ssize_t count = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (count <= 0) return "Robix";
    buffer[count] = '\0';
    return std::string(buffer);
#endif
}

inline std::string RunLines(const std::string& executable, const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char mark : text) {
        if (mark == '\n') {
            if (!current.empty()) lines.push_back(current);
            current.clear();
        } else if (mark != '\r') {
            current.push_back(mark);
        }
    }
    if (!current.empty()) lines.push_back(current);
    std::vector<std::future<std::pair<int, std::string>>> jobs;
    for (const std::string& line : lines) {
        jobs.push_back(std::async(std::launch::async, [executable, line]() {
            std::string command = "\"" + executable + "\" " + line;
            int code = std::system(command.c_str());
            return std::make_pair(code, line);
        }));
    }
    std::string output;
    for (std::future<std::pair<int, std::string>>& job : jobs) {
        std::pair<int, std::string> result = job.get();
        output += result.first == 0 ? "Passed " : "Failed ";
        output += result.second;
        output += "\n";
    }
    return output;
}

inline String ParallelRun(String script) {
    return String::Lit(RunLines(ExecutablePath(), script.null ? "" : script.text));
}

inline String ToolPath(String name) {
    std::string tool = name.null ? "" : name.text;
#ifdef _WIN32
    std::string tail = tool.size() >= 4 ? tool.substr(tool.size() - 4) : "";
    for (char& mark : tail) mark = static_cast<char>(std::tolower(static_cast<unsigned char>(mark)));
    if (tail != ".exe") tool += ".exe";
#endif
    std::string executable = ExecutablePath();
    std::string::size_type slash = executable.find_last_of("\\/");
    std::string directory = slash == std::string::npos ? "" : executable.substr(0, slash + 1);
    return String::Lit(directory + tool);
}

inline String ParallelTool(String tool, String script) {
    std::string executable = tool.null ? "" : tool.text;
    if (executable.empty() || !std::filesystem::exists(executable)) return String::Lit("Failed missing tool\n");
    return String::Lit(RunLines(executable, script.null ? "" : script.text));
}

inline int64_t BitAnd(int64_t left, int64_t right) {
    return static_cast<int64_t>(static_cast<uint64_t>(left) & static_cast<uint64_t>(right));
}
inline int64_t BitOr(int64_t left, int64_t right) {
    return static_cast<int64_t>(static_cast<uint64_t>(left) | static_cast<uint64_t>(right));
}
inline int64_t BitXor(int64_t left, int64_t right) {
    return static_cast<int64_t>(static_cast<uint64_t>(left) ^ static_cast<uint64_t>(right));
}
inline int64_t Shl(int64_t left, int64_t right) {
    return static_cast<int64_t>(static_cast<uint64_t>(left) << (static_cast<uint64_t>(right) & 63));
}
inline int64_t Shr(int64_t left, int64_t right) {
    return static_cast<int64_t>(static_cast<uint64_t>(left) >> (static_cast<uint64_t>(right) & 63));
}

inline int64_t BitsOf(double value) {
    int64_t bits = 0;
    static_assert(sizeof(double) == sizeof(int64_t), "double must be 64 bits");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

inline double DoubleOf(int64_t bits) {
    double value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline int64_t ParseF64(const String& text) {
    if (text.null || text.text.empty()) return BitsOf(0);
    try {
        return BitsOf(std::stod(text.text));
    } catch (...) {
        return BitsOf(0);
    }
}

inline int64_t F32Parse(const String& text) { return ParseF64(text); }
inline int64_t F16Parse(const String& text) { return ParseF64(text); }
inline String TextF64(int64_t bits) { return String::Lit(std::to_string(DoubleOf(bits))); }
inline int64_t FloatFromInt(int64_t value) { return BitsOf(static_cast<double>(value)); }
inline int64_t F64Add(int64_t left, int64_t right) { return BitsOf(DoubleOf(left) + DoubleOf(right)); }
inline int64_t F64Sub(int64_t left, int64_t right) { return BitsOf(DoubleOf(left) - DoubleOf(right)); }
inline int64_t F64Mul(int64_t left, int64_t right) { return BitsOf(DoubleOf(left) * DoubleOf(right)); }
inline int64_t F64Div(int64_t left, int64_t right) {
    double divisor = DoubleOf(right);
    if (divisor == 0) return BitsOf(0);
    return BitsOf(DoubleOf(left) / divisor);
}
inline int64_t F64Neg(int64_t bits) { return BitsOf(-DoubleOf(bits)); }
inline int64_t F64Cmp(int64_t left, int64_t right) {
    double a = DoubleOf(left);
    double b = DoubleOf(right);
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}
inline int64_t FloatToInt(int64_t bits) { return static_cast<int64_t>(DoubleOf(bits)); }
inline int64_t F64ToIntMode(int64_t bits, int64_t) { return FloatToInt(bits); }
inline int64_t F64ToIntChecked(int64_t bits) { return FloatToInt(bits); }
inline String F64Format(int64_t bits, const String&) { return TextF64(bits); }
inline int64_t U64Parse(const String& text) {
    if (text.null || text.text.empty()) return 0;
    try {
        return static_cast<int64_t>(std::stoull(text.text));
    } catch (...) {
        return 0;
    }
}

namespace Files {
inline bool Exists(const String& path) {
    if (path.null || path.text.empty()) return false;
    std::error_code error;
    return std::filesystem::is_regular_file(path.text, error);
}

inline String ReadText(const String& path) {
    if (path.null) return String::Lit("");
    std::ifstream input(path.text, std::ios::binary);
    if (!input) return String::Lit("");
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return String::Lit(buffer.str());
}

inline void WriteText(const String& path, const String& text) {
    if (path.null) return;
    std::filesystem::path file(path.text);
    std::error_code error;
    if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), error);
    std::ofstream output(path.text, std::ios::binary);
    if (!text.null) output << text.text;
}

inline void WriteBytes(const String& path, List<int64_t> bytes) {
    if (path.null) return;
    std::filesystem::path file(path.text);
    std::error_code error;
    if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), error);
    std::ofstream output(path.text, std::ios::binary);
    if (!bytes.data) return;
    for (int64_t value : *bytes.data) output.put(static_cast<char>(value & 0xFF));
}

inline void Delete(const String& path) {
    if (path.null) return;
    std::error_code error;
    std::filesystem::remove(path.text, error);
}

inline String Name(const String& path) {
    if (path.null) return String::Lit("");
    auto slash = path.text.find_last_of("/\\");
    if (slash == std::string::npos) return path;
    return String::Lit(path.text.substr(slash + 1));
}

inline String Join(const String& directory, const String& file) {
    if (directory.null || directory.text.empty()) return file.null ? String::Lit("") : file;
    if (file.null) return directory;
    if (!directory.text.empty() && (directory.text.back() == '/' || directory.text.back() == '\\')) {
        return String::Lit(directory.text + file.text);
    }
    return String::Lit(directory.text + "/" + file.text);
}
}  // namespace Files

namespace Directory {
inline bool Exists(const String& path) {
    if (path.null || path.text.empty()) return false;
    std::error_code error;
    return std::filesystem::is_directory(path.text, error);
}

inline void Create(const String& path) {
    if (path.null || path.text.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(path.text, error);
}

inline void Delete(const String& path) {
    if (path.null || path.text.empty()) return;
    std::error_code error;
    std::filesystem::remove_all(path.text, error);
}

inline List<String> Files(const String& path) {
    List<String> files = List<String>::Make();
    if (path.null || path.text.empty()) return files;
    std::error_code error;
    std::filesystem::path root(path.text == "." ? std::filesystem::path(".") : std::filesystem::path(path.text));
    if (!std::filesystem::exists(root, error)) return files;
    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, error);
    std::filesystem::recursive_directory_iterator end;
    std::vector<std::string> names;
    for (; !error && it != end; it.increment(error)) {
        std::error_code status;
        if (!it->is_regular_file(status)) continue;
        std::error_code relativeError;
        auto relative = std::filesystem::relative(it->path(), root, relativeError);
        if (relativeError) continue;
        names.push_back(relative.generic_string());
    }
    std::sort(names.begin(), names.end());
    for (const std::string& name : names) files.Add(String::Lit(name));
    return files;
}
}  // namespace Directory

}  // namespace rx
