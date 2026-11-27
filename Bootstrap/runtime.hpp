// Runtime for programs the C++ bootstrap transpiles from Robix.
#pragma once

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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
};

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

inline void Print(const String& text) {
    std::cout << (text.null ? "" : text.text) << std::endl;
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
