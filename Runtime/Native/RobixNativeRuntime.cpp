// Robix native runtime support library.
//
// Linked into every native executable produced by `robixc`. Print does not allocate.
// Input and File.ReadText return a malloc'd C string. File.WriteText uses the host disk.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

std::int64_t ArrayCount(const std::int64_t* values) {
    if (values == nullptr || values[0] < 0) return 0;
    return values[0];
}

void ArrayBounds(std::int64_t index, std::int64_t count) {
    if (index < 0 || index >= count) std::abort();
}

std::int64_t* AllocIntArray(std::int64_t count, std::int64_t fill) {
    if (count < 0 || count > 10000000) std::abort();
    auto* data = static_cast<std::int64_t*>(std::malloc(static_cast<std::size_t>(count + 1) * sizeof(std::int64_t)));
    if (data == nullptr) std::abort();
    data[0] = count;
    for (std::int64_t i = 0; i < count; ++i) data[i + 1] = fill;
    return data;
}

std::int64_t* CopyIntArray(const std::int64_t* values) {
    const std::int64_t count = ArrayCount(values);
    std::int64_t* copy = AllocIntArray(count, 0);
    for (std::int64_t i = 0; i < count; ++i) copy[i + 1] = values[i + 1];
    return copy;
}

void StableSortInts(std::int64_t* items, std::int64_t count) {
    std::vector<std::int64_t> aux(static_cast<std::size_t>(count));
    for (std::int64_t width = 1; width < count; width *= 2) {
        for (std::int64_t start = 0; start < count; start += width * 2) {
            const std::int64_t mid = (std::min)(start + width, count);
            const std::int64_t end = (std::min)(start + width * 2, count);
            std::int64_t left = start;
            std::int64_t right = mid;
            std::int64_t cursor = start;
            while (left < mid && right < end) {
                if (items[right] < items[left]) aux[static_cast<std::size_t>(cursor++)] = items[right++];
                else aux[static_cast<std::size_t>(cursor++)] = items[left++];
            }
            while (left < mid) aux[static_cast<std::size_t>(cursor++)] = items[left++];
            while (right < end) aux[static_cast<std::size_t>(cursor++)] = items[right++];
            for (std::int64_t i = start; i < end; ++i) items[i] = aux[static_cast<std::size_t>(i)];
        }
    }
}

std::int64_t RotateShift(std::int64_t steps, std::int64_t count) {
    if (count <= 0) return 0;
    std::int64_t shift = steps % count;
    if (shift < 0) shift += count;
    return shift;
}

std::FILE* OpenFile(const char* path, const char* mode) {
    std::FILE* file = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&file, path, mode) != 0) return nullptr;
#else
    file = std::fopen(path, mode);
#endif
    return file;
}

}

extern "C" {

void rx_debug_line(std::int64_t line) {
    std::printf("native %lld\n", static_cast<long long>(line));
    std::fflush(stdout);
}

void rx_print_i64(std::int64_t value) {
    std::printf("%lld\n", static_cast<long long>(value));
}

void rx_print_f64(double value) {
    std::printf("%g\n", value);
}

void rx_print_bool(std::int32_t value) {
    std::puts(value != 0 ? "true" : "false");
}

void rx_print_str(const char* text) {
    std::puts(text != nullptr ? text : "null");
}

static char* EmptyString() {
    char* text = static_cast<char*>(std::malloc(1));
    if (text != nullptr) text[0] = '\0';
    return text;
}

char* rx_input_line(const char* prompt) {
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

std::int64_t rx_input_i64(const char* prompt) {
    char* line = rx_input_line(prompt);
    char* end = nullptr;
    const long long value = std::strtoll(line != nullptr ? line : "", &end, 10);
    std::free(line);
    return static_cast<std::int64_t>(value);
}

int rx_write_text(const char* path, const char* text) {
    if (path == nullptr) return 0;
    std::FILE* file = OpenFile(path, "wb");
    if (file == nullptr) return 0;
    if (text != nullptr) {
        const std::size_t length = std::strlen(text);
        if (length > 0) std::fwrite(text, 1, length, file);
    }
    std::fclose(file);
    return 1;
}

char* rx_read_text(const char* path) {
    if (path == nullptr) return EmptyString();
    std::FILE* file = OpenFile(path, "rb");
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

std::int64_t rx_device_unavailable(std::int64_t code, std::int64_t id) {
    const char* name = "PWM.SetDuty";
    if (code == 2) name = "UART.Write";
    else if (code == 3) name = "SPI.Transfer";
    else if (code == 4) name = "I2C.Write";
    else if (code == 5) name = "CAN.Send";
    else if (code == 6) name = "USB.Write";
    else if (code == 7) name = "PCIe.Read";
    else if (code == 8) name = "DMA.Start";
    else if (code == 9) name = "Sensor.Read";
    else if (code == 10) name = "Motor.SetSpeed";
    else if (code == 11) name = "Interrupt.Enable";
    else if (code == 12) name = "HardwareTimer.Read";
    std::fprintf(stderr, "%s is not available on this target (%lld)\n", name, static_cast<long long>(id));
    return 1;
}

std::int64_t rx_gpio_read(std::int64_t pin) {
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

std::int64_t rx_add_pair(std::int64_t* cell, std::int64_t left, std::int64_t right) {
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

static std::vector<std::string> g_arguments;
static bool g_argumentsReady = false;

static std::string WideToUtf8(const wchar_t* text, int length) {
    if (text == nullptr || length <= 0) return {};
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
    std::string out(bytes > 0 ? static_cast<std::size_t>(bytes) : 0, '\0');
    if (bytes > 0) WideCharToMultiByte(CP_UTF8, 0, text, length, out.data(), bytes, nullptr, nullptr);
    return out;
}

static void LoadProcessArguments() {
    if (g_argumentsReady) return;
    g_argumentsReady = true;
    const wchar_t* command = GetCommandLineW();
    if (command == nullptr) return;
    std::vector<std::string> tokens;
    std::wstring current;
    bool quoted = false;
    bool token = false;
    for (const wchar_t* cursor = command; *cursor != L'\0'; ++cursor) {
        if (*cursor == L'"') {
            quoted = !quoted;
            token = true;
            continue;
        }
        if (!quoted && (*cursor == L' ' || *cursor == L'\t')) {
            if (token) {
                tokens.push_back(WideToUtf8(current.c_str(), static_cast<int>(current.size())));
                current.clear();
                token = false;
            }
            continue;
        }
        current.push_back(*cursor);
        token = true;
    }
    if (token) tokens.push_back(WideToUtf8(current.c_str(), static_cast<int>(current.size())));
    if (!tokens.empty()) tokens.erase(tokens.begin());
    g_arguments = std::move(tokens);
}

std::int64_t rx_args_count() {
    LoadProcessArguments();
    return static_cast<std::int64_t>(g_arguments.size());
}

char* rx_args_get(std::int64_t index) {
    LoadProcessArguments();
    if (index < 0 || static_cast<std::size_t>(index) >= g_arguments.size()) return EmptyString();
    const std::string& text = g_arguments[static_cast<std::size_t>(index)];
    char* copy = static_cast<char*>(std::malloc(text.size() + 1));
    if (copy == nullptr) return EmptyString();
    std::memcpy(copy, text.data(), text.size() + 1);
    return copy;
}

void rx_print_array(const std::int64_t* values) {
    if (values == nullptr) {
        std::puts("null");
        return;
    }
    const std::int64_t count = ArrayCount(values);
    std::fputc('[', stdout);
    for (std::int64_t i = 0; i < count; ++i) {
        if (i > 0) std::fputs(", ", stdout);
        std::printf("%lld", static_cast<long long>(values[i + 1]));
    }
    std::fputs("]\n", stdout);
}

void rx_array_swap(std::int64_t* values, std::int64_t left, std::int64_t right) {
    const std::int64_t count = ArrayCount(values);
    ArrayBounds(left, count);
    ArrayBounds(right, count);
    std::swap(values[left + 1], values[right + 1]);
}

void rx_array_reverse(std::int64_t* values) {
    const std::int64_t count = ArrayCount(values);
    for (std::int64_t i = 0; i < count / 2; ++i) std::swap(values[i + 1], values[count - i]);
}

void rx_array_sort(std::int64_t* values) {
    if (values == nullptr) return;
    StableSortInts(values + 1, ArrayCount(values));
}

void rx_array_rotate(std::int64_t* values, std::int64_t steps) {
    const std::int64_t count = ArrayCount(values);
    const std::int64_t shift = RotateShift(steps, count);
    if (shift == 0) return;
    std::vector<std::int64_t> saved(static_cast<std::size_t>(count));
    for (std::int64_t i = 0; i < count; ++i) saved[static_cast<std::size_t>(i)] = values[i + 1];
    for (std::int64_t i = 0; i < count; ++i) values[i + 1] = saved[static_cast<std::size_t>((i + shift) % count)];
}

void rx_array_fill(std::int64_t* values, std::int64_t fill) {
    const std::int64_t count = ArrayCount(values);
    for (std::int64_t i = 0; i < count; ++i) values[i + 1] = fill;
}

void rx_array_shuffle(std::int64_t* values, std::int64_t seed) {
    const std::int64_t count = ArrayCount(values);
    std::uint64_t state = static_cast<std::uint64_t>(seed);
    for (std::int64_t i = count - 1; i > 0; --i) {
        state = state * 1664525ull + 1013904223ull;
        const std::int64_t slot = static_cast<std::int64_t>(state % static_cast<std::uint64_t>(i + 1));
        std::swap(values[i + 1], values[slot + 1]);
    }
}

std::int64_t rx_array_sum(const std::int64_t* values) {
    std::int64_t total = 0;
    const std::int64_t count = ArrayCount(values);
    for (std::int64_t i = 0; i < count; ++i) total += values[i + 1];
    return total;
}

std::int64_t rx_array_product(const std::int64_t* values) {
    std::int64_t total = 1;
    const std::int64_t count = ArrayCount(values);
    for (std::int64_t i = 0; i < count; ++i) total *= values[i + 1];
    return total;
}

std::int64_t rx_array_min(const std::int64_t* values) {
    const std::int64_t count = ArrayCount(values);
    if (count == 0) return 0;
    std::int64_t best = values[1];
    for (std::int64_t i = 1; i < count; ++i) if (values[i + 1] < best) best = values[i + 1];
    return best;
}

std::int64_t rx_array_max(const std::int64_t* values) {
    const std::int64_t count = ArrayCount(values);
    if (count == 0) return 0;
    std::int64_t best = values[1];
    for (std::int64_t i = 1; i < count; ++i) if (values[i + 1] > best) best = values[i + 1];
    return best;
}

std::int64_t rx_array_index_of(const std::int64_t* values, std::int64_t target) {
    const std::int64_t count = ArrayCount(values);
    for (std::int64_t i = 0; i < count; ++i) if (values[i + 1] == target) return i;
    return -1;
}

std::int64_t rx_array_last_index_of(const std::int64_t* values, std::int64_t target) {
    const std::int64_t count = ArrayCount(values);
    for (std::int64_t i = count - 1; i >= 0; --i) if (values[i + 1] == target) return i;
    return -1;
}

std::int64_t rx_array_count_of(const std::int64_t* values, std::int64_t target) {
    std::int64_t count = 0;
    const std::int64_t length = ArrayCount(values);
    for (std::int64_t i = 0; i < length; ++i) if (values[i + 1] == target) ++count;
    return count;
}

std::int64_t rx_array_contains(const std::int64_t* values, std::int64_t target) {
    return rx_array_index_of(values, target) >= 0 ? 1 : 0;
}

std::int64_t rx_array_is_sorted(const std::int64_t* values) {
    const std::int64_t count = ArrayCount(values);
    for (std::int64_t i = 1; i < count; ++i) if (values[i + 1] < values[i]) return 0;
    return 1;
}

std::int64_t rx_array_is_empty(const std::int64_t* values) {
    return ArrayCount(values) == 0 ? 1 : 0;
}

std::int64_t rx_array_binary_search(const std::int64_t* values, std::int64_t target) {
    std::int64_t low = 0;
    std::int64_t high = ArrayCount(values) - 1;
    std::int64_t found = -1;
    while (low <= high) {
        const std::int64_t mid = low + (high - low) / 2;
        const std::int64_t value = values[mid + 1];
        if (value == target) {
            found = mid;
            high = mid - 1;
        } else if (value < target) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return found;
}

std::int64_t* rx_array_copy(const std::int64_t* values) { return CopyIntArray(values); }

std::int64_t* rx_array_reversed(const std::int64_t* values) {
    std::int64_t* copy = CopyIntArray(values);
    rx_array_reverse(copy);
    return copy;
}

std::int64_t* rx_array_sorted(const std::int64_t* values) {
    std::int64_t* copy = CopyIntArray(values);
    rx_array_sort(copy);
    return copy;
}

std::int64_t* rx_array_rotated(const std::int64_t* values, std::int64_t steps) {
    std::int64_t* copy = CopyIntArray(values);
    rx_array_rotate(copy, steps);
    return copy;
}

std::int64_t* rx_array_unique(const std::int64_t* values) {
    const std::int64_t count = ArrayCount(values);
    std::vector<std::int64_t> kept;
    for (std::int64_t i = 0; i < count; ++i) {
        bool seen = false;
        for (std::int64_t item : kept) {
            if (item == values[i + 1]) {
                seen = true;
                break;
            }
        }
        if (!seen) kept.push_back(values[i + 1]);
    }
    std::int64_t* out = AllocIntArray(static_cast<std::int64_t>(kept.size()), 0);
    for (std::size_t i = 0; i < kept.size(); ++i) out[i + 1] = kept[i];
    return out;
}

std::int64_t* rx_array_concat(const std::int64_t* left, const std::int64_t* right) {
    const std::int64_t a = ArrayCount(left);
    const std::int64_t b = ArrayCount(right);
    std::int64_t* out = AllocIntArray(a + b, 0);
    for (std::int64_t i = 0; i < a; ++i) out[i + 1] = left[i + 1];
    for (std::int64_t i = 0; i < b; ++i) out[a + i + 1] = right[i + 1];
    return out;
}

std::int64_t* rx_array_filled(std::int64_t count, std::int64_t fill) {
    return AllocIntArray(count, fill);
}

std::int64_t* rx_array_range(std::int64_t start, std::int64_t end) {
    if (end < start) return AllocIntArray(0, 0);
    if (end - start > 10000000) std::abort();
    std::int64_t* out = AllocIntArray(end - start, 0);
    for (std::int64_t i = start; i < end; ++i) out[(i - start) + 1] = i;
    return out;
}

} // extern "C"

