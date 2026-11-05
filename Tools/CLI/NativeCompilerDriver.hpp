#pragma once

#include <string>
#include <string_view>

namespace Robix::Tools {

struct NativeBuildOptions {
    std::string outputPath;       // Executable path (default: <input stem>.exe)
    bool release        = true;   // true => clang -O2, false => clang -O0
    bool keepLLVMIR     = false;  // Keep the generated .ll file next to the output
    bool emitLLVMOnly   = false;  // Stop after writing the .ll file
    std::string clangPath;        // Explicit clang path (else auto-detected)
    std::string runtimeObjectPath;// Explicit RobixNativeRuntime.obj path (else auto-detected)
    std::string toolDirectory;    // Directory containing robix.exe / robixc.exe (for auto-detection)
};

struct NativeBuildResult {
    bool success = false;
    std::string executablePath;
    std::string llvmIRPath;
    std::string diagnosticsText;  // Compiler diagnostics (errors/warnings)
    std::string toolOutput;       // clang output (if any)
};

class NativeCompilerDriver {
public:
    // Full Robix -> native executable pipeline:
    //   lex -> parse -> type check -> semantic analysis -> SSA IR -> optimize -> LLVM IR -> clang -> .exe
    [[nodiscard]] static NativeBuildResult CompileSource(std::string_view fileName,
                                                         std::string_view sourceText,
                                                         const NativeBuildOptions& options);

    // Locates clang.exe: explicit hint, ROBIX_CLANG env var, <toolDir>/toolchain/clang.exe, PATH, default LLVM install.
    [[nodiscard]] static std::string LocateClang(const std::string& hint, const std::string& toolDirectory);

    // Locates RobixNativeRuntime.obj: explicit hint, <toolDir>/RobixNativeRuntime.obj, <cwd>/Build/.
    [[nodiscard]] static std::string LocateRuntimeObject(const std::string& hint, const std::string& toolDirectory);
};

} // namespace Robix::Tools

