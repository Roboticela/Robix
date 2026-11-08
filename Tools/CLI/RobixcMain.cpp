#include "NativeCompilerDriver.hpp"
#include "StageIBuild.hpp"
#include "UiProjectBuild.hpp"
#include "../../RobixUI/AppRun.hpp"
#include "RobixCLIAndTestGenerator.hpp"
#include "ExecutionGate.hpp"
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
        << "    --target windows|linux|macos|android|ios|web\n"
        << "                          Application or Canvas platform. linux, macos, android, and ios\n"
        << "                          compile only on that machine. web needs a .rbxu screen.\n"
        << "    --image               Machine image. Refuses platform code. Bedrock is always an image.\n"
        << "    --target <triple>     x86_64-pc-windows-msvc, x86_64-unknown-linux-gnu,\n"
        << "                          x86_64-unknown-none, aarch64-unknown-none,\n"
        << "                          arm-unknown-none, riscv64-unknown-none\n"
        << "    --entry <symbol>      Freestanding entry symbol (default: Start)\n"
        << "    --emit-llvm           Generate LLVM IR (.ll) and exit\n"
        << "    --keep-ir             Retain generated .ll file next to executable\n"
        << "    --clang <path>        Path to clang.exe\n"
        << "    --runtime-obj <path>  Path to RobixNativeRuntime.obj\n"
        << "    --explain, --dry-run  Print the .rbxb plan and do not compile\n"
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
        else if (key == "Config") profile = profile.empty() && value == "debug" ? "debug" : profile;
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
    } else if (outputRole == "Executable") {
        if (target == "x86_64-unknown-linux-gnu") {
            options.outputKind = Robix::Tools::NativeOutputKind::LinuxExecutable;
            options.freestanding = true;
            if (options.entrySymbol.empty()) options.entrySymbol = "Start";
        } else {
            options.outputKind = Robix::Tools::NativeOutputKind::HostedExecutable;
            options.freestanding = false;
        }
    } else if (outputRole == "Library" || outputRole == "StaticLibrary") {
        options.outputKind = Robix::Tools::NativeOutputKind::StaticLibrary;
        options.omitMain = true;
        options.freestanding = false;
    } else if (outputRole == "SharedLibrary") {
        options.outputKind = Robix::Tools::NativeOutputKind::SharedLibrary;
        options.omitMain = true;
        options.freestanding = false;
    } else if (outputRole == "Object") {
        options.outputKind = Robix::Tools::NativeOutputKind::HostedObject;
        options.omitMain = true;
        options.freestanding = false;
    } else if (outputRole == "Package") {
        options.outputKind = Robix::Tools::NativeOutputKind::PackageArchive;
        options.omitMain = true;
        options.freestanding = false;
    } else if (!outputRole.empty()) {
        error = "robixc: unknown Output '" + outputRole + "'.\n";
        return false;
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
    bool explainOnly = false;
    bool uiDevelopment = false;
    bool cliImage = false;
    std::string cliTarget;
    std::vector<std::string> uiTargets;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "-o" && i + 1 < args.size()) {
            options.outputPath = args[++i];
        } else if (a == "--explain" || a == "--dry-run") {
            explainOnly = true;
        } else if (a == "--emit-llvm") {
            options.emitLLVMOnly = true;
        } else if (a == "--keep-ir") {
            options.keepLLVMIR = true;
        } else if (a == "-O0" || a == "--debug") {
            options.release = false;
        } else if (a == "-O2" || a == "-O3" || a == "--release") {
            options.release = true;
        } else if (a == "--mode" && i + 1 < args.size()) {
            const std::string mode = args[++i];
            if (mode == "development" || mode == "dev") {
                uiDevelopment = true;
            } else if (mode == "production" || mode == "prod") {
                uiDevelopment = false;
            } else {
                std::cerr << "robixc: mode must be development or production.\n";
                return 1;
            }
        } else if (a == "--image") {
            cliImage = true;
        } else if (a == "--target" && i + 1 < args.size()) {
            const std::string target = args[++i];
            cliTarget = target;
            const std::string key = Robix::UI::NormalizeTarget(target);
            if (Robix::Tools::IsPlatformName(key)) {
                if (key == "web" || key == "windows") uiTargets.push_back(key);
                continue;
            }
            std::string targetError;
            if (!ApplyTarget(target, options, targetError)) {
                std::cerr << targetError;
                return 1;
            }
        } else if (a == "--entry" && i + 1 < args.size()) {
            options.entrySymbol = args[++i];
        } else if (a == "--profile" && i + 1 < args.size()) {
            const std::string profile = args[++i];
            options.profileName = profile;
            if (profile == "debug" || profile == "safety") {
                options.release = false;
            } else if (profile == "release" || profile == "performance" || profile == "realtime" || profile == "size") {
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
        } else if (a == "--no-main") {
            options.omitMain = true;
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
    {
        std::error_code inputError;
        const fs::path inputPath(inputFilePath);
        const bool uiInput = fs::is_directory(inputPath, inputError) || inputPath.extension() == ".rbxu" ||
                             inputPath.extension() == ".xml" || inputPath.extension() == ".rbxproj";
        const bool sourceFile = inputPath.extension() == ".rbx" || inputPath.extension() == ".rbxb";
        if ((uiInput || !uiTargets.empty() || uiDevelopment) && !sourceFile) {
            if (uiDevelopment) {
                std::cerr << "robixc: development runs with robix, not robixc.\n";
                std::cerr << "  robix run " << inputFilePath << " --target "
                          << (uiTargets.empty() ? Robix::UI::HostTarget() : uiTargets.front()) << " --watch\n";
                return 1;
            }
            if (uiTargets.empty()) {
                uiTargets.push_back(Robix::UI::HostTarget());
            }
            return Robix::UI::CompileProduction(inputFilePath, uiTargets, toolDir);
        }
    }
    if (explainOnly) {
        Robix::Tools::RbxbPlan plan;
        if (fs::path(inputFilePath).extension() == ".rbxb") {
            plan = Robix::Tools::ReadRbxb(inputFilePath);
            if (!plan.ok) {
                std::cerr << plan.error;
                return 1;
            }
        } else {
            plan.ok = true;
            plan.source = inputFilePath;
        }
        std::cout << Robix::Tools::ExplainRbxb(plan);
        return 0;
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

    const std::string category = Robix::Tools::ProjectCategory(inputFilePath);
    const Robix::Tools::CodeShape shape = Robix::Tools::ScanCodeShape(sourceText);
    const std::string cliKey = Robix::UI::NormalizeTarget(cliTarget);
    Robix::Tools::RunSurface surface = options.freestanding ? Robix::Tools::RunSurface::Direct
                                                           : Robix::Tools::RunSurface::Platform;
    std::string surfaceName = options.freestanding ? "a machine image" : "windows";
    if (cliImage || Robix::Tools::IsDirectName(cliKey) || Robix::Tools::IsDirectName(cliTarget)) {
        surface = Robix::Tools::RunSurface::Direct;
        surfaceName = "a machine image";
        options.freestanding = true;
        if (options.outputKind == Robix::Tools::NativeOutputKind::HostedExecutable) {
            options.outputKind = Robix::Tools::NativeOutputKind::BootImage;
        }
        if (options.entrySymbol.empty()) options.entrySymbol = "Start";
        if (options.targetTriple.empty()) {
            options.targetTriple = "x86_64-unknown-none";
            options.clangTarget = "x86_64-elf";
            options.dataLayout =
                "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
        }
    }
    if (Robix::Tools::IsPlatformName(cliKey)) {
        surface = Robix::Tools::RunSurface::Platform;
        surfaceName = cliKey;
        options.freestanding = false;
        options.outputKind = Robix::Tools::NativeOutputKind::HostedExecutable;
    }
    if (category == "Bedrock" && !cliImage && !Robix::Tools::IsDirectName(cliKey) && !Robix::Tools::IsPlatformName(cliKey) &&
        !options.freestanding) {
        surface = Robix::Tools::RunSurface::Direct;
        surfaceName = "a machine image";
        options.freestanding = true;
        options.outputKind = Robix::Tools::NativeOutputKind::BootImage;
        if (options.entrySymbol.empty()) options.entrySymbol = "Start";
    }
    const std::string mismatch = Robix::Tools::ExecutionError(category, surface, surfaceName, shape);
    if (!mismatch.empty()) {
        std::cerr << mismatch;
        return 1;
    }
    if (surface == Robix::Tools::RunSurface::Platform) {
        const Robix::Tools::WebDecision web = Robix::Tools::DecideWeb(
            category, Robix::Tools::ProjectKind(inputFilePath), sourceText, inputFilePath, cliKey == "web");
        if (!web.warning.empty()) std::cerr << web.warning;
        if (!web.error.empty()) {
            std::cerr << web.error;
            return 1;
        }
        if (cliKey == "web") {
            return Robix::UI::CompileProduction(web.screen, { "web" }, toolDir);
        }
    }
    if (Robix::Tools::IsPlatformName(cliKey) && cliKey != "windows" && cliKey != "web") {
        std::cerr << "error: this machine compiles windows. '" << cliKey
                  << "' was not compiled. Compile it on a " << cliKey << " machine.\n";
        return 1;
    }

    // Default output path if not specified
    if (options.outputPath.empty() && !options.emitLLVMOnly) {
        fs::path p(inputFilePath);
        if (options.outputKind == Robix::Tools::NativeOutputKind::BootImage ||
            options.outputKind == Robix::Tools::NativeOutputKind::LinuxExecutable) {
            p.replace_extension(".elf");
        } else if (options.outputKind == Robix::Tools::NativeOutputKind::ObjectFile ||
                   options.outputKind == Robix::Tools::NativeOutputKind::HostedObject) {
            p.replace_extension(".o");
        } else if (options.outputKind == Robix::Tools::NativeOutputKind::StaticLibrary) {
            p.replace_extension(".a");
        } else if (options.outputKind == Robix::Tools::NativeOutputKind::SharedLibrary) {
            p.replace_extension(".dll");
        } else if (options.outputKind == Robix::Tools::NativeOutputKind::PackageArchive) {
            p.replace_extension(".rbxlib");
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

