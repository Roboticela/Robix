#pragma once

#include <string>
#include <string_view>

namespace Robix::Tools {

enum class NativeOutputKind : std::uint8_t {
    HostedExecutable,
    BootImage,
    ObjectFile,
    HostedObject,
    StaticLibrary,
    SharedLibrary,
    PackageArchive,
    LinuxExecutable
};

struct NativeBuildOptions {
    std::string outputPath;       // Executable path (default: <input stem>.exe)
    bool release        = true;   // true => clang -O2, false => clang -O0
    bool keepLLVMIR     = false;  // Keep the generated .ll file next to the output
    bool emitLLVMOnly   = false;  // Stop after writing the .ll file
    bool freestanding   = false;  // No hosted CRT and no generated main
    NativeOutputKind outputKind = NativeOutputKind::HostedExecutable;
    std::string clangPath;        // Explicit clang path (else auto-detected)
    std::string runtimeObjectPath;// Explicit RobixNativeRuntime.obj path (else auto-detected)
    std::string toolDirectory;    // Directory containing robix.exe / robixc.exe (for auto-detection)
    std::string targetTriple;     // LLVM triple written into the IR. Empty keeps the hosted Windows triple
    std::string clangTarget;      // Assembler triple. Empty uses targetTriple, or x86_64-elf for a boot image
    std::string dataLayout;
    std::string entrySymbol;      // Empty means Main, or Start when freestanding
    std::string asmArch = "x86_64";
    std::string imageRole = "Kernel"; // .rbxb Output: Kernel or Firmware
    bool linkNetwork = false;
    bool debugLines = false;
    bool omitMain = false;
    std::string profileName;
    std::string networkObjectPath;
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

    // Locates the clang shipped inside the compiler install: <toolDir>/toolchain/clang.exe.
    [[nodiscard]] static std::string LocateClang(const std::string& hint, const std::string& toolDirectory);

    // Locates RobixNativeRuntime.obj: explicit hint, <toolDir>/RobixNativeRuntime.obj, <cwd>/Build/.
    [[nodiscard]] static std::string LocateRuntimeObject(const std::string& hint, const std::string& toolDirectory);
};

} // namespace Robix::Tools

