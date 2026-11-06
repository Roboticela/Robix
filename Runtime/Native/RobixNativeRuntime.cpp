// Robix native runtime support library.
//
// Linked into every native executable produced by `robixc`. Print does not allocate.
// Input and File.ReadText return a malloc'd C string. File.WriteText uses the host disk.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

extern "C" {

void rbx_debug_line(std::int64_t line) {
    std::printf("native %lld\n", static_cast<long long>(line));
    std::fflush(stdout);
}

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

std::int64_t rbx_device_unavailable(std::int64_t code, std::int64_t id) {
    const char* name = "PWM.SetDuty";
    if (code == 2) name = "UART.Write";
    else if (code == 3) name = "SPI.Transfer";
    else if (code == 4) name = "I2C.Write";
    std::fprintf(stderr, "%s is not available on this target (%lld)\n", name, static_cast<long long>(id));
    return 1;
}

std::int64_t rbx_gpio_read(std::int64_t pin) {
    std::fprintf(stderr, "GPIO.Read is not available on this target (pin %lld)\n",
                 static_cast<long long>(pin));
    return 1;
}

struct PairAdd {
    std::int64_t* cell;
    std::int64_t amount;
};

DWORD WINAPI PairAddProc(LPVOID raw) {
    auto* job = static_cast<PairAdd*>(raw);
    InterlockedExchangeAdd64(reinterpret_cast<LONG64*>(job->cell), job->amount);
    return 0;
}

std::int64_t rbx_add_pair(std::int64_t* cell, std::int64_t left, std::int64_t right) {
    if (cell == nullptr) return 0;
    PairAdd jobs[2] = { { cell, left }, { cell, right } };
    HANDLE threads[2] = { nullptr, nullptr };
    for (int i = 0; i < 2; ++i) {
        threads[i] = CreateThread(nullptr, 0, PairAddProc, &jobs[i], 0, nullptr);
        if (threads[i] == nullptr) {
            std::fputs("Thread.AddPair could not start an operating-system thread\n", stderr);
            for (int j = 0; j < i; ++j) {
                WaitForSingleObject(threads[j], INFINITE);
                CloseHandle(threads[j]);
            }
            return -1;
        }
    }
    WaitForSingleObject(threads[0], INFINITE);
    WaitForSingleObject(threads[1], INFINITE);
    CloseHandle(threads[0]);
    CloseHandle(threads[1]);
    return *cell;
}

} // extern "C"

