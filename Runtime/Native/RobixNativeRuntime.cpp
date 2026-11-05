// Robix native runtime support library.
//
// Linked into every native executable produced by `robixc`. It provides the tiny C ABI
// used by compiler-generated code for Print(...). It deliberately has no dependencies
// beyond the C standard library, performs no allocation, and keeps output formatting
// identical to the Bytecode VM (decimal integers, `%g` floating point, true/false).

#include <cstdint>
#include <cstdio>

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

} // extern "C"

