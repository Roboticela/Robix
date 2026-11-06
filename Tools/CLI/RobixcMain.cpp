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
        << "    --profile <name>      release, debug, or embedded (no hosted CRT)\n"
        << "    --target <triple>     x86_64-pc-windows-msvc, x86_64-unknown-linux-gnu,\n"
        << "                          x86_64-unknown-none, aarch64-unknown-none,\n"
        << "                          arm-unknown-none, riscv64-unknown-none\n"
        << "    --entry <symbol>      Freestanding entry symbol (default: Start)\n"
        << "    --emit-llvm           Generate LLVM IR (.ll) and exit\n"
        << "    --keep-ir             Retain generated .ll file next to executable\n"
        << "    --clang <path>        Path to clang.exe\n"
        << "    --runtime-obj <path>  Path to RobixNativeRuntime.obj\n"
        << "    --generate-tests      Generate missing .test.rbx scaffolds\n"
        << "    --version, -v         Display robixc version information\n"
        << "    --help, -h            Show this help documentation\n";
}

static bool ApplyTarget(const std::string& triple, Robix::Tools::NativeBuildOptions& options, std::string& error) {
    options.asmArch = "x86_64";
    options.clangTarget = triple;
    options.targetTriple = triple;
    const std::string elfLayout =
        "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
    if (triple == "x86_64-pc-windows-msvc") {
        options.freestanding = false;
        options.outputKind = Robix::Tools::NativeOutputKind::HostedExecutable;
        options.targetTriple.clear();
        options.clangTarget.clear();
        options.dataLayout.clear();
        return true;
    }
    options.freestanding = true;
    if (triple == "x86_64-unknown-linux-gnu") {
        options.outputKind = Robix::Tools::NativeOutputKind::ObjectFile;
        options.dataLayout = elfLayout;
        return true;
    }
    if (triple == "x86_64-unknown-none") {
        options.outputKind = Robix::Tools::NativeOutputKind::BootImage;
        options.dataLayout = elfLayout;
        options.clangTarget = "x86_64-elf";
        return true;
    }
    if (triple == "aarch64-unknown-none") {
        options.outputKind = Robix::Tools::NativeOutputKind::ObjectFile;
        options.asmArch = "aarch64";
        options.dataLayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128";
        options.clangTarget = "aarch64-elf";
        return true;
    }
    if (triple == "arm-unknown-none") {
        options.outputKind = Robix::Tools::NativeOutputKind::ObjectFile;
        options.asmArch = "arm";
        options.dataLayout = "e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64";
        options.clangTarget = "arm-none-eabi";
        return true;
    }
    if (triple == "riscv64-unknown-none") {
        options.outputKind = Robix::Tools::NativeOutputKind::ObjectFile;
        options.asmArch = "riscv64";
        options.dataLayout = "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128";
        options.clangTarget = "riscv64-unknown-elf";
        return true;
    }
    error = "robixc: unknown target '" + triple + "'.\n";
    return false;
}

static bool LoadRobixBuild(const fs::path& rbxb, std::string& sourcePath, Robix::Tools::NativeBuildOptions& options, std::string& error) {
    std::ifstream in(rbxb);
    if (!in) {
        error = "robixc: could not open '" + rbxb.string() + "'.\n";
        return false;
    }
    std::string outputRole;
    std::string target;
    std::string profile;
    std::string source;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        auto trim = [](std::string text) {
            const auto first = text.find_first_not_of(" \t");
            if (first == std::string::npos) return std::string{};
            const auto last = text.find_last_not_of(" \t");
            return text.substr(first, last - first + 1);
        };
        const std::string key = trim(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        if (key == "Output") outputRole = value;
        else if (key == "Target") target = value;
        else if (key == "Entry") options.entrySymbol = value;
        else if (key == "Source") source = value;
        else if (key == "Profile") profile = value;
    }
    if (source.empty()) {
        error = "robixc: " + rbxb.string() + " has no Source: entry.\n";
        return false;
    }
    if (!target.empty() && !ApplyTarget(target, options, error)) return false;
    if (outputRole == "Kernel" || outputRole == "Firmware") {
        options.imageRole = outputRole;
        options.freestanding = true;
        if (target.empty() || target == "x86_64-unknown-none" || target == "aarch64-unknown-none" ||
            target == "arm-unknown-none" || target == "riscv64-unknown-none") {
            options.outputKind = Robix::Tools::NativeOutputKind::BootImage;
        }
    }
    if (profile == "embedded") {
        options.freestanding = true;
        if (options.outputKind == Robix::Tools::NativeOutputKind::HostedExecutable) {
            options.outputKind = Robix::Tools::NativeOutputKind::ObjectFile;
        }
    } else if (profile == "debug") {
        options.release = false;
    }
    sourcePath = (rbxb.parent_path() / source).string();
    return true;
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
        } else if (a == "--target" && i + 1 < args.size()) {
            std::string targetError;
            if (!ApplyTarget(args[++i], options, targetError)) {
                std::cerr << targetError;
                return 1;
            }
        } else if (a == "--entry" && i + 1 < args.size()) {
            options.entrySymbol = args[++i];
        } else if (a == "--profile" && i + 1 < args.size()) {
            const std::string profile = args[++i];
            if (profile == "debug") {
                options.release = false;
            } else if (profile == "release") {
                options.release = true;
            } else if (profile == "embedded") {
                options.freestanding = true;
                options.release = true;
                if (options.outputKind == Robix::Tools::NativeOutputKind::HostedExecutable) {
                    options.outputKind = Robix::Tools::NativeOutputKind::ObjectFile;
                }
            } else {
                std::cerr << "robixc: unknown profile '" << profile << "'.\n";
                return 1;
            }
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
    if (fs::path(inputFilePath).extension() == ".rbxb") {
        std::string sourcePath;
        std::string rbxbError;
        if (!LoadRobixBuild(inputFilePath, sourcePath, options, rbxbError)) {
            std::cerr << rbxbError;
            return 1;
        }
        inputFilePath = sourcePath;
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
        if (options.outputKind == Robix::Tools::NativeOutputKind::BootImage) {
            p.replace_extension(".elf");
        } else if (options.outputKind == Robix::Tools::NativeOutputKind::ObjectFile) {
            p.replace_extension(".o");
        } else {
            p.replace_extension(".exe");
        }
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

