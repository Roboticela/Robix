#pragma once

#include "../../Compiler/Semantic/Diagnostics.hpp"
#include "../../Compiler/Types/SpecValidator.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace Robix::Runtime {

// ============================================================================
// 1. SAFE BOUNDS-CHECKED SPAN<T> SLICE
// ============================================================================

template <typename T>
class Span {
public:
    constexpr Span() noexcept = default;
    constexpr Span(T* data, std::size_t length) noexcept : m_data(data), m_length(length) {}

    [[nodiscard]] constexpr T* Data() const noexcept { return m_data; }
    [[nodiscard]] constexpr std::size_t Length() const noexcept { return m_length; }
    [[nodiscard]] constexpr bool IsEmpty() const noexcept { return m_length == 0; }

    // Safe bounds-checked element access
    [[nodiscard]] T* At(std::size_t index, Frontend::DiagnosticBag& diags, Frontend::SourceLocation loc = {}) const {
        if (!m_data || index >= m_length) {
            diags.ReportError(
                "M5003",
                "Span bounds check failed: index " + std::to_string(index) +
                    " is out of bounds for Span of length " + std::to_string(m_length) + ".",
                loc,
                "Check 'index < span.Length()' before indexing.");
            return nullptr;
        }
        return &m_data[index];
    }

    // Safe bounds-checked sub-slice
    [[nodiscard]] Span<T> SubSpan(std::size_t offset,
                                  std::size_t count,
                                  Frontend::DiagnosticBag& diags,
                                  Frontend::SourceLocation loc = {}) const {
        if (offset + count > m_length) {
            diags.ReportError(
                "M5003",
                "SubSpan range [" + std::to_string(offset) + ".." + std::to_string(offset + count) +
                    "] exceeds Span length " + std::to_string(m_length) + ".",
                loc);
            return {};
        }
        return Span<T>(m_data + offset, count);
    }

    // Zero-cost unchecked indexing for compiler-verified inner loops
    [[nodiscard]] constexpr T& operator[](std::size_t index) const noexcept { return m_data[index]; }

private:
    T* m_data            = nullptr;
    std::size_t m_length = 0;
};

// ============================================================================
// 2. TYPED RAW POINTER<T> (RESTRICTED TO UNSAFE CONTEXTS)
// ============================================================================

template <typename T>
class RawPointer {
public:
    constexpr RawPointer() noexcept = default;
    constexpr explicit RawPointer(T* ptr) noexcept : m_ptr(ptr) {}

    [[nodiscard]] constexpr bool IsNull() const noexcept { return m_ptr == nullptr; }
    [[nodiscard]] constexpr T* Get() const noexcept { return m_ptr; }
    [[nodiscard]] std::uintptr_t Address() const noexcept { return reinterpret_cast<std::uintptr_t>(m_ptr); }

    [[nodiscard]] constexpr RawPointer<T> AddOffset(std::ptrdiff_t elementOffset) const noexcept {
        return RawPointer<T>(m_ptr ? (m_ptr + elementOffset) : nullptr);
    }

    [[nodiscard]] constexpr std::ptrdiff_t Diff(RawPointer<T> other) const noexcept {
        return m_ptr - other.m_ptr;
    }

    template <typename U>
    [[nodiscard]] RawPointer<U> Cast() const noexcept {
        return RawPointer<U>(reinterpret_cast<U*>(m_ptr));
    }

    [[nodiscard]] T* Dereference(Frontend::DiagnosticBag& diags, Frontend::SourceLocation loc = {}) const {
        if (!m_ptr) {
            diags.ReportError(
                "M5004",
                "Null pointer dereference detected in Unsafe pointer operation.",
                loc,
                "Check 'ptr != null' before dereferencing.");
            return nullptr;
        }
        return m_ptr;
    }

private:
    T* m_ptr = nullptr;
};

// ============================================================================
// 3. TARGET-CHECKED INLINE ASSEMBLY (ASM) VALIDATOR
// ============================================================================

struct InlineASMStatementDesc {
    std::string instructionText;
    std::vector<std::string> outputConstraints; // e.g. "=r"
    std::vector<std::string> inputConstraints;  // e.g. "r", "m"
    std::vector<std::string> clobbers;          // e.g. "memory", "cc", "rax"
    bool isVolatile = true;
    Frontend::SourceLocation location{};
};

class InlineASMValidator {
public:
    static bool ValidateForTarget(const InlineASMStatementDesc& asmDesc,
                                  Spec::TargetArch targetArch,
                                  Frontend::DiagnosticBag& diags);
};

// ============================================================================
// 4. MEMORY-SAFE C / NATIVE INTEROPERABILITY (FFI BOUNDARY GUARD)
// ============================================================================

struct CFFIResult {
    bool succeeded      = false;
    std::int64_t retVal = 0;
    std::string errorMessage;
};

class FFIBoundaryGuard {
public:
    explicit FFIBoundaryGuard(Frontend::DiagnosticBag& diagnostics) noexcept;

    // Pin a Robix memory buffer before passing its pointer across a C FFI boundary
    void PinBufferForFFI(const void* bufferPtr, std::size_t sizeBytes);
    void UnpinBufferAfterFFI(const void* bufferPtr) noexcept;
    [[nodiscard]] bool IsBufferPinned(const void* bufferPtr) const noexcept;

    // Validate a pointer argument before invoking an Extern C function
    bool ValidateCFFIPointerArg(const void* ptr,
                                bool allowNull,
                                bool requirePinned,
                                std::string_view externFnName,
                                Frontend::SourceLocation loc = {});

    // Execute a C FFI callback inside a panic/exception isolation barrier
    CFFIResult ExecuteIsolatedCCall(std::string_view externFnName,
                                    const std::function<std::int64_t()>& cCallback,
                                    Frontend::SourceLocation loc = {});

private:
    Frontend::DiagnosticBag& m_diagnostics;
    std::unordered_set<const void*> m_pinnedBuffers;
};

} // namespace Robix::Runtime

