#include "LowLevelSystems.hpp"

#include <algorithm>
#include <cctype>
#include <exception>

namespace Robix::Runtime {

namespace {

std::string ToUpperASCII(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return out;
}

bool ContainsToken(const std::string& upperText, std::string_view token) {
    return upperText.find(token) != std::string::npos;
}

} // namespace

bool InlineASMValidator::ValidateForTarget(const InlineASMStatementDesc& asmDesc,
                                           Spec::TargetArch targetArch,
                                           Frontend::DiagnosticBag& diags) {
    if (asmDesc.instructionText.empty()) {
        diags.ReportError(
            "A5001",
            "Inline assembly instruction string cannot be empty.",
            asmDesc.location);
        return false;
    }

    const std::string upperInst = ToUpperASCII(asmDesc.instructionText);

    // Check architecture-specific instruction and register compatibility
    const bool usesX86Only = ContainsToken(upperInst, "RDTSC") ||
                             ContainsToken(upperInst, "CPUID") ||
                             ContainsToken(upperInst, "RAX") ||
                             ContainsToken(upperInst, "RBX") ||
                             ContainsToken(upperInst, "RCX") ||
                             ContainsToken(upperInst, "RDX") ||
                             ContainsToken(upperInst, "MFENCE");

    const bool usesARMOnly = ContainsToken(upperInst, "DMB") ||
                             ContainsToken(upperInst, "DSB") ||
                             ContainsToken(upperInst, "ISB") ||
                             ContainsToken(upperInst, "WFI") ||
                             ContainsToken(upperInst, "MRS ");

    const bool usesRISCVOnly = ContainsToken(upperInst, "CSRR") ||
                               ContainsToken(upperInst, "CSRW") ||
                               ContainsToken(upperInst, "EBREAK") ||
                               ContainsToken(upperInst, "FENCE.I");

    const bool isTargetX86   = (targetArch == Spec::TargetArch::X86_64_SysV ||
                                targetArch == Spec::TargetArch::X86_64_Windows);
    const bool isTargetARM   = (targetArch == Spec::TargetArch::ARM64 ||
                                targetArch == Spec::TargetArch::ARM32);
    const bool isTargetRISCV = (targetArch == Spec::TargetArch::RISCV64);

    if (usesX86Only && !isTargetX86) {
        diags.ReportError(
            "A5001",
            "x86-64 specific inline assembly instruction '" + asmDesc.instructionText +
                "' is invalid for non-x86 target architecture.",
            asmDesc.location,
            "Guard architecture-specific ASM blocks with target checks or use portable hardware intrinsics.");
        return false;
    }

    if (usesARMOnly && !isTargetARM) {
        diags.ReportError(
            "A5001",
            "ARM specific inline assembly instruction '" + asmDesc.instructionText +
                "' is invalid for non-ARM target architecture.",
            asmDesc.location);
        return false;
    }

    if (usesRISCVOnly && !isTargetRISCV) {
        diags.ReportError(
            "A5001",
            "RISC-V specific inline assembly instruction '" + asmDesc.instructionText +
                "' is invalid for non-RISC-V target architecture.",
            asmDesc.location);
        return false;
    }

    // Validate output constraints start with '=' or '+'
    for (const auto& outC : asmDesc.outputConstraints) {
        if (outC.empty() || (outC[0] != '=' && outC[0] != '+')) {
            diags.ReportError(
                "A5001",
                "Invalid inline assembly output constraint '" + outC + "': output constraints must begin with '=' or '+'.",
                asmDesc.location);
            return false;
        }
    }

    return true;
}

// ============================================================================
// MEMORY-SAFE C / NATIVE INTEROPERABILITY (FFI BOUNDARY GUARD)
// ============================================================================

FFIBoundaryGuard::FFIBoundaryGuard(Frontend::DiagnosticBag& diagnostics) noexcept
    : m_diagnostics(diagnostics) {}

void FFIBoundaryGuard::PinBufferForFFI(const void* bufferPtr, std::size_t sizeBytes) {
    (void)sizeBytes;
    if (bufferPtr) {
        m_pinnedBuffers.insert(bufferPtr);
    }
}

void FFIBoundaryGuard::UnpinBufferAfterFFI(const void* bufferPtr) noexcept {
    if (bufferPtr) {
        m_pinnedBuffers.erase(bufferPtr);
    }
}

bool FFIBoundaryGuard::IsBufferPinned(const void* bufferPtr) const noexcept {
    return bufferPtr != nullptr && m_pinnedBuffers.find(bufferPtr) != m_pinnedBuffers.end();
}

bool FFIBoundaryGuard::ValidateCFFIPointerArg(const void* ptr,
                                              bool allowNull,
                                              bool requirePinned,
                                              std::string_view externFnName,
                                              Frontend::SourceLocation loc) {
    if (!ptr) {
        if (!allowNull) {
            m_diagnostics.ReportError(
                "F5001",
                "Null pointer passed to non-nullable C FFI parameter in '" + std::string(externFnName) + "'.",
                loc,
                "Verify the pointer is non-null before crossing the C FFI boundary.");
            return false;
        }
        return true;
    }

    if (requirePinned && !IsBufferPinned(ptr)) {
        m_diagnostics.ReportError(
            "F5001",
            "Unpinned Robix buffer passed to C FFI function '" + std::string(externFnName) + "'.",
            loc,
            "Pin the buffer before calling C so it cannot be freed or relocated during the FFI call.");
        return false;
    }

    return true;
}

CFFIResult FFIBoundaryGuard::ExecuteIsolatedCCall(std::string_view externFnName,
                                                  const std::function<std::int64_t()>& cCallback,
                                                  Frontend::SourceLocation loc) {
    try {
        const std::int64_t ret = cCallback();
        return CFFIResult{ true, ret, {} };
    } catch (const std::exception& ex) {
        m_diagnostics.ReportError(
            "F5001",
            "Exception caught and isolated at C FFI boundary in '" + std::string(externFnName) + "': " + ex.what(),
            loc);
        return CFFIResult{ false, -1, ex.what() };
    } catch (...) {
        m_diagnostics.ReportError(
            "F5001",
            "Unknown native fault isolated at C FFI boundary in '" + std::string(externFnName) + "'.",
            loc);
        return CFFIResult{ false, -1, "Unknown C FFI fault" };
    }
}

} // namespace Robix::Runtime

