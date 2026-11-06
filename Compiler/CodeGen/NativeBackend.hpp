#pragma once

#include "../IR/SSAIR.hpp"
#include "../Semantic/Diagnostics.hpp"
#include "LLVMIRGenerator.hpp"

#include <optional>
#include <string>

namespace Robix::CodeGen {

struct NativeEmitOptions {
    BuildProfile profile  = BuildProfile::Release;
    TargetPlatform target = TargetPlatform::WindowsX64;
    // Freestanding images have no hosted main, no CRT, and one exported entry symbol.
    bool freestanding = false;
    std::string entryName;
    std::string tripleOverride;
    std::string layoutOverride;
    std::string asmArch = "x86_64";
};

// Production LLVM IR emitter.
//
// Unlike LLVMIRGenerator::GenerateLLVMIR (a legacy textual preview used by the
// Stage 4 conformance tests), this emitter produces COMPLETE, VERIFIABLE LLVM IR
// that links into a working native executable:
//   - typed locals (alloca slots promoted to SSA by LLVM mem2reg),
//   - Int / Float / Bool / String scalar semantics with implicit widening,
//   - full control flow (if / while / do-while / for / repeat / loop / break / continue),
//   - direct calls with arguments and return values, recursion,
//   - Print lowering to the Robix native runtime, and a C `main` entry wrapper.
//
// Constructs the native backend cannot yet compile (classes, strings beyond Print,
// collections, defer, assertions outside tests, virtual calls) are reported as
// N9xxx errors. They are never silently skipped or miscompiled.
class NativeLLVMEmitter {
public:
    explicit NativeLLVMEmitter(Frontend::DiagnosticBag& diagnostics, NativeEmitOptions options = {}) noexcept;

    // Returns the textual LLVM IR module, or std::nullopt if any error was reported.
    [[nodiscard]] std::optional<std::string> Emit(const IR::IRModule& mod);

private:
    Frontend::DiagnosticBag& m_diagnostics;
    NativeEmitOptions m_options;
};

} // namespace Robix::CodeGen

