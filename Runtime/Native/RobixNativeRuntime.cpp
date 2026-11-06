// Robix native runtime support library.
//
// Linked into every native executable produced by `robixc`. Print does not allocate.
// Input and File.ReadText return a malloc'd C string. File.WriteText uses the host disk.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {

void rbx_print_i64(std::int64_t value) {
    std::printf("%lld\n", static_cast<long long>(value));
}

void rbx_print_f64(double value) {
    std::printf("%g\n", value);
}

void rbx_print_bool(std::int32_t value) {
    std::puts(value != 0 ? "true" : "false");
}

void rbx_print_str(const char* text) {
    std::puts(text != nullptr ? text : "null");
}

static char* EmptyString() {
    char* text = static_cast<char*>(std::malloc(1));
    if (text != nullptr) text[0] = '\0';
    return text;
}

char* rbx_input_line(const char* prompt) {
    if (prompt != nullptr && prompt[0] != '\0') {
        std::fputs(prompt, stdout);
        std::fflush(stdout);
    }
    char buffer[4096];
    if (std::fgets(buffer, static_cast<int>(sizeof buffer), stdin) == nullptr) {
        return EmptyString();
    }
    std::size_t length = std::strlen(buffer);
    while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
        buffer[--length] = '\0';
    }
    char* text = static_cast<char*>(std::malloc(length + 1));
    if (text == nullptr) return EmptyString();
    std::memcpy(text, buffer, length + 1);
    return text;
}

std::int64_t rbx_input_i64(const char* prompt) {
    char* line = rbx_input_line(prompt);
    char* end = nullptr;
    const long long value = std::strtoll(line != nullptr ? line : "", &end, 10);
    std::free(line);
    return static_cast<std::int64_t>(value);
}

int rbx_write_text(const char* path, const char* text) {
    if (path == nullptr) return 0;
    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) return 0;
    if (text != nullptr) {
        const std::size_t length = std::strlen(text);
        if (length > 0) std::fwrite(text, 1, length, file);
    }
    std::fclose(file);
    return 1;
}

char* rbx_read_text(const char* path) {
    if (path == nullptr) return EmptyString();
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) return EmptyString();
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return EmptyString();
    }
    const long size = std::ftell(file);
    if (size < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return EmptyString();
    }
    char* text = static_cast<char*>(std::malloc(static_cast<std::size_t>(size) + 1));
    if (text == nullptr) {
        std::fclose(file);
        return EmptyString();
    }
    const std::size_t got = std::fread(text, 1, static_cast<std::size_t>(size), file);
    text[got] = '\0';
    std::fclose(file);
    return text;
}

} // extern "C"

