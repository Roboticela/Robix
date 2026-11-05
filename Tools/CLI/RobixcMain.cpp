#include "NativeCompilerDriver.hpp"
#include "RobixCLIAndTestGenerator.hpp"
#include "../../StdLib/Core/CoreTypesAndRegex.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void PrintHelp() {
    std::cout
        << "Robix Native Compiler (robixc) - Production Native Toolchain\n"
        << "Roboticela Software Architecture\n\n"
        << "USAGE:\n"
        << "    robixc <file.rbx> [options]\n"
        << "    robixc build <file.rbx> [-o <out.exe>]\n"
        << "    robixc run <file.rbx> [--native | --vm]\n"
        << "    robixc test [file.test.rbx]\n"
        << "    robixc --emit-llvm <file.rbx>\n\n"
        << "OPTIONS:\n"
        << "    -o <path>             Specify output executable path\n"
        << "    -O0, --debug          Build with optimizations disabled\n"
        << "    -O2, -O3, --release   Build with release optimizations (default: -O2)\n"
        << "    --emit-llvm           Generate LLVM IR (.ll) and exit\n"
        << "    --keep-ir             Retain generated .ll file next to executable\n"
        << "    --clang <path>        Path to clang.exe\n"
        << "    --runtime-obj <path>  Path to RobixNativeRuntime.obj\n"
        << "    --generate-tests      Generate missing .test.rbx scaffolds\n"
        << "    --version, -v         Display robixc version information\n"
        << "    --help, -h            Show this help documentation\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintHelp();
        return 0;
    }

    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const std::string& first = args[0];
    if (first == "--help" || first == "-h" || first == "help") {
        PrintHelp();
        return 0;
    }
    if (first == "--version" || first == "-v" || first == "version") {
        std::cout << "robixc version 1.0.0 (Windows x64 Native LLVM Toolchain)\n";
        return 0;
    }

    // Determine tool directory (where robixc.exe is running from)
    std::string toolDir;
    try {
        toolDir = fs::absolute(argv[0]).parent_path().string();
    } catch (...) {
        toolDir = ".";
    }

    // Parse options
    std::string inputFilePath;
    Robix::Tools::NativeBuildOptions options;
    options.toolDirectory = toolDir;
    bool executeAfterBuild = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "-o" && i + 1 < args.size()) {
            options.outputPath = args[++i];
        } else if (a == "--emit-llvm") {
            options.emitLLVMOnly = true;
        } else if (a == "--keep-ir") {
            options.keepLLVMIR = true;
        } else if (a == "-O0" || a == "--debug") {
            options.release = false;
        } else if (a == "-O2" || a == "-O3" || a == "--release") {
            options.release = true;
        } else if (a == "--clang" && i + 1 < args.size()) {
            options.clangPath = args[++i];
        } else if (a == "--runtime-obj" && i + 1 < args.size()) {
            options.runtimeObjectPath = args[++i];
        } else if (a == "run") {
            executeAfterBuild = true;
        } else if (a == "build") {
            // subcommand verb
        } else if (!a.empty() && a[0] == '-') {
            // other flags (e.g. --generate-tests)
            if (a == "--generate-tests") {
                auto res = Robix::Tools::RobixCLIDispatcher::ExecuteCommand(
                    Robix::Tools::CLIBinaryKind::RobixNativeCompiler, args);
                std::cout << res.stdoutText;
                if (!res.stderrText.empty()) std::cerr << res.stderrText;
                return res.exitCode;
            }
        } else if (inputFilePath.empty()) {
            inputFilePath = a;
        }
    }

    // If dispatching to standard subcommands (test, verify, etc.)
    if (first == "test" || first == "verify" || first == "check") {
        auto res = Robix::Tools::RobixCLIDispatcher::ExecuteCommand(
            Robix::Tools::CLIBinaryKind::RobixNativeCompiler, args);
        std::cout << res.stdoutText;
        if (!res.stderrText.empty()) std::cerr << res.stderrText;
        return res.exitCode;
    }

    if (inputFilePath.empty()) {
        std::cerr << "robixc: error: no input file specified.\n";
        return 1;
    }

    // Read source file
    std::ifstream srcFile(inputFilePath, std::ios::binary);
    if (!srcFile.is_open()) {
        std::cerr << "robixc: error: could not open source file '" << inputFilePath << "'\n";
        return 1;
    }
    std::string sourceText((std::istreambuf_iterator<char>(srcFile)),
                           std::istreambuf_iterator<char>());
    srcFile.close();

    // Default output path if not specified
    if (options.outputPath.empty() && !options.emitLLVMOnly) {
        fs::path p(inputFilePath);
        p.replace_extension(".exe");
        options.outputPath = p.string();
    }

    auto buildResult = Robix::Tools::NativeCompilerDriver::CompileSource(
        inputFilePath, sourceText, options);

    if (!buildResult.diagnosticsText.empty()) {
        std::cerr << buildResult.diagnosticsText;
    }
    if (!buildResult.toolOutput.empty()) {
        std::cout << buildResult.toolOutput;
    }

    if (!buildResult.success) {
        std::cerr << "robixc: build failed.\n";
        return 1;
    }

    if (options.emitLLVMOnly) {
        std::cout << "robixc: generated LLVM IR: " << buildResult.llvmIRPath << "\n";
        return 0;
    }

    std::cout << "robixc: compiled successfully -> " << buildResult.executablePath << "\n";

    if (executeAfterBuild) {
        int runCode = std::system(buildResult.executablePath.c_str());
        return runCode;
    }

    return 0;
}

