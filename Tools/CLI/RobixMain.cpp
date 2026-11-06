#include "NativeCompilerDriver.hpp"
#include "StageIBuild.hpp"
#include "RobixCLIAndTestGenerator.hpp"
#include "ExamplesAndDocumentationCatalog.hpp"
#include "../../Interpreter/Bytecode/BytecodeModule.hpp"
#include "../../Interpreter/VM/BytecodeVM.hpp"
#include "../../StdLib/Core/CoreTypesAndRegex.hpp"

#include <algorithm>
#include <cstdlib>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void PrintHelp() {
    std::cout
        << "Robix CLI (robix) - Unified Language Toolchain & Package Manager\n"
        << "Roboticela Software Architecture\n\n"
        << "USAGE:\n"
        << "    robix <command> [arguments]\n\n"
        << "COMMANDS:\n"
        << "    run <file.rbx> [--native | --vm]   Execute a Robix source program\n"
        << "    build <file.rbx> [-o <out.exe>]     Compile directly to a native executable\n"
        << "    test [path] [--generate]            Run .test.rbx on the interpreter. Test native also compiles.\n"
        << "    generate-tests [path]               Generate companion .test.rbx scaffolds for all .rbx files\n"
        << "    verify [path]                       Run full semantic and static verification on files\n"
        << "    new <ProjectName>                   Initialize a new Robix project\n"
        << "    add <name> <file.rbx>               Lock a local package with SHA-256 and an HMAC signature\n"
        << "    remove <name>                       Remove a package from the lock\n"
        << "    update <name>                       Rehash a locked package file\n"
        << "    install                             Copy locked packages into Packages/ when the hash matches\n"
        << "    restore                             Check every locked hash and signature\n"
        << "    examples [topic]                    List or extract official Robix code examples\n"
        << "    repl                                Start the interactive Robix REPL\n"
        << "    version                             Display version and toolchain info\n\n"
        << "OPTIONS:\n"
        << "    -v, --version                       Display version information\n"
        << "    -h, --help                          Show this help documentation\n";
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

    const std::string& cmd = args[0];
    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        PrintHelp();
        return 0;
    }
    if (cmd == "--version" || cmd == "-v" || cmd == "version") {
        std::cout << "robix version 1.0.0 (Unified CLI & Toolchain, Windows x64)\n";
        return 0;
    }

    // Determine tool directory
    std::string toolDir;
    try {
        toolDir = fs::absolute(argv[0]).parent_path().string();
    } catch (...) {
        toolDir = ".";
    }

    const int packageStatus = Robix::Tools::PackageCommand(args);
    if (packageStatus >= 0) return packageStatus;

    // `robix build <file.rbx>` delegates to NativeCompilerDriver
    if (cmd == "build" && args.size() >= 2) {
        std::string inputFile;
        Robix::Tools::NativeBuildOptions opts;
        opts.toolDirectory = toolDir;

        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "-o" && i + 1 < args.size()) {
                opts.outputPath = args[++i];
            } else if (args[i] == "-O0" || args[i] == "--debug") {
                opts.release = false;
            } else if (args[i] == "-O2" || args[i] == "-O3" || args[i] == "--release") {
                opts.release = true;
            } else if (args[i] == "--keep-ir") {
                opts.keepLLVMIR = true;
            } else if (args[i] == "--clang" && i + 1 < args.size()) {
                opts.clangPath = args[++i];
            } else if (args[i] == "--runtime-obj" && i + 1 < args.size()) {
                opts.runtimeObjectPath = args[++i];
            } else if (!args[i].empty() && args[i][0] != '-') {
                if (inputFile.empty()) inputFile = args[i];
            }
        }

        if (inputFile.empty()) {
            std::cerr << "robix build: error: no input file specified.\n";
            return 1;
        }

        std::ifstream srcFile(inputFile, std::ios::binary);
        if (!srcFile.is_open()) {
            std::cerr << "robix build: error: could not open file '" << inputFile << "'\n";
            return 1;
        }
        std::string sourceText((std::istreambuf_iterator<char>(srcFile)),
                               std::istreambuf_iterator<char>());
        srcFile.close();

        if (opts.outputPath.empty()) {
            fs::path p(inputFile);
            p.replace_extension(".exe");
            opts.outputPath = p.string();
        }

        const std::string missingTest = Robix::Tools::MissingTestMessage(inputFile);
        if (!missingTest.empty()) {
            std::cerr << missingTest;
            return 1;
        }
        if (Robix::Tools::IncrementalHit(inputFile, opts.outputPath)) {
            std::cout << "incremental: unchanged " << opts.outputPath << "\n";
            return 0;
        }

        auto res = Robix::Tools::NativeCompilerDriver::CompileSource(inputFile, sourceText, opts);
        if (!res.diagnosticsText.empty()) std::cerr << res.diagnosticsText;
        if (!res.toolOutput.empty()) std::cout << res.toolOutput;
        if (!res.success) {
            std::cerr << "robix build: compilation failed.\n";
            return 1;
        }
        Robix::Tools::WriteIncrementalStamp(inputFile, res.executablePath);
        std::cout << "robix build: created executable -> " << res.executablePath << "\n";
        return 0;
    }

    // `robix run <file.rbx> --native`
    bool wantsNativeRun = false;
    for (const auto& a : args) {
        if (a == "--native") wantsNativeRun = true;
    }

    if (cmd == "run" && wantsNativeRun && args.size() >= 2) {
        std::string inputFile;
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty() && args[i][0] != '-') {
                inputFile = args[i];
                break;
            }
        }
        if (inputFile.empty()) inputFile = "Source/Main.rbx";

        std::ifstream srcFile(inputFile, std::ios::binary);
        if (!srcFile.is_open()) {
            std::cerr << "robix run: error: could not open file '" << inputFile << "'\n";
            return 1;
        }
        std::string sourceText((std::istreambuf_iterator<char>(srcFile)),
                               std::istreambuf_iterator<char>());
        srcFile.close();

        fs::path tempExe = fs::temp_directory_path() / (fs::path(inputFile).stem().string() + "_run.exe");
        Robix::Tools::NativeBuildOptions opts;
        opts.toolDirectory = toolDir;
        opts.outputPath = tempExe.string();
        opts.release = true;

        auto res = Robix::Tools::NativeCompilerDriver::CompileSource(inputFile, sourceText, opts);
        if (!res.success) {
            if (!res.diagnosticsText.empty()) std::cerr << res.diagnosticsText;
            std::cerr << "robix run: native compilation failed.\n";
            return 1;
        }
        int code = std::system(tempExe.string().c_str());
        std::error_code ec;
        fs::remove(tempExe, ec);
        return code;
    }

    if (cmd == "new") {
        std::string name = "App";
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty() && args[i][0] != '-') {
                name = args[i];
                break;
            }
        }
        if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos ||
            name.find("..") != std::string::npos) {
            std::cerr << "robix new: project name must be a single directory name.\n";
            return 1;
        }
        const fs::path dir = fs::path(name);
        const fs::path file = dir / "Main.rbx";
        std::error_code ec;
        if (fs::exists(file)) {
            std::cerr << "robix new: '" << file.string() << "' already exists.\n";
            return 1;
        }
        fs::create_directories(dir, ec);
        if (ec) {
            std::cerr << "robix new: could not create '" << dir.string() << "'.\n";
            return 1;
        }
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        if (!out) {
            std::cerr << "robix new: could not write '" << file.string() << "'.\n";
            return 1;
        }
        out << "package " << name << ";\n\n"
            << "public Function Main() -> Void {\n"
            << "    Print(\"Hello from Robix\");\n"
            << "}\n";
        std::cout << "robix new: wrote " << file.string() << "\n";
        return 0;
    }

    if (cmd == "test") {
        bool generateOnly = false;
        std::string target;
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--generate" || args[i] == "--init" || args[i] == "--auto-scaffold") {
                generateOnly = args[i] == "--generate" && args.size() == 2;
                continue;
            }
            if (!args[i].empty() && args[i][0] != '-') target = args[i];
        }
        if (target.empty()) target = "Tests/Main.test.rbx";
        const bool wantGenerate = false;
        (void)wantGenerate;
        if (!generateOnly) {
            auto versionProbe = []() {
                Robix::VM::BytecodeModule bad;
                bad.version = 99;
                Robix::VM::BytecodeChunk chunk;
                chunk.name = "Main";
                chunk.returnTypeName = "Void";
                Robix::VM::BCInstruction ret;
                ret.opcode = Robix::VM::BCOpcode::ReturnVoid;
                chunk.code.push_back(ret);
                bad.chunks.push_back(std::move(chunk));
                Robix::VM::BytecodeVM vm(bad);
                const int code = vm.ExecuteMainAndGetExitCode();
                const bool refused = code != 0 && vm.Fault().find("version") != std::string::npos;
                std::cout << (refused ? "bytecode version 99 refused\n" : "bytecode version check did not refuse\n");
                return refused ? 0 : 1;
            };
            auto runOne = [&](const fs::path& file) {
                std::ifstream in(file, std::ios::binary);
                if (!in) {
                    std::cerr << file.string() << ": could not read\n";
                    return 1;
                }
                const std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                const bool expectDiagnostic = source.find("expect: interpreter-diagnostic") != std::string::npos;
                const bool runNative = source.find("Test native") != std::string::npos;
                std::vector<std::string> one = { "test", file.string() };
                auto interpreted = Robix::Tools::RobixCLIDispatcher::ExecuteCommand(
                    Robix::Tools::CLIBinaryKind::RobixUnifiedCLI, one);
                if (!interpreted.stdoutText.empty()) std::cout << interpreted.stdoutText;
                if (!interpreted.stderrText.empty()) std::cerr << interpreted.stderrText;
                if (expectDiagnostic) {
                    const bool saw = interpreted.exitCode != 0 &&
                                     (interpreted.stdoutText.find("native freestanding") != std::string::npos ||
                                      interpreted.stdoutText.find("InlineASM") != std::string::npos);
                    std::cout << file.string() << (saw ? ": interpreter diagnostic\n" : ": expected an interpreter diagnostic\n");
                    return saw ? 0 : 1;
                }
                if (interpreted.exitCode != 0) {
                    std::cout << file.string() << ": interpreter failed\n";
                    return 1;
                }
                if (!runNative) {
                    std::cout << file.string() << ": interpreter ok\n";
                    return 0;
                }
                fs::create_directories("Build/Conformance");
                const fs::path exe = fs::path("Build/Conformance") / (file.stem().string() + ".exe");
                Robix::Tools::NativeBuildOptions opts;
                opts.toolDirectory = toolDir;
                opts.outputPath = exe.string();
                opts.release = true;
                auto built = Robix::Tools::NativeCompilerDriver::CompileSource(file.string(), source, opts);
                if (!built.success) {
                    std::cerr << file.string() << ": native compilation failed\n";
                    if (!built.diagnosticsText.empty()) std::cerr << built.diagnosticsText;
                    if (!built.toolOutput.empty()) std::cerr << built.toolOutput;
                    std::cout << file.string() << ": interpreter passed, native failed\n";
                    return 1;
                }
                STARTUPINFOA si{};
                si.cb = sizeof(si);
                PROCESS_INFORMATION pi{};
                std::string exePath = built.executablePath;
                for (char& ch : exePath) {
                    if (ch == '/') ch = '\\';
                }
                int nativeCode = 1;
                if (CreateProcessA(exePath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
                    WaitForSingleObject(pi.hProcess, INFINITE);
                    DWORD processCode = 1;
                    GetExitCodeProcess(pi.hProcess, &processCode);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    nativeCode = static_cast<int>(processCode);
                }
                if (nativeCode != 0) {
                    std::cout << file.string() << ": interpreter passed, native exit " << nativeCode << "\n";
                    return 1;
                }
                std::cout << file.string() << ": interpreter ok, native ok\n";
                return 0;
            };

            int status = 0;
            std::error_code ec;
            if (fs::is_directory(target, ec)) {
                const std::string missingTest = Robix::Tools::MissingTestMessage(target);
                if (!missingTest.empty()) {
                    std::cerr << missingTest;
                    return 1;
                }
                status |= versionProbe();
                std::vector<fs::path> files;
                auto collect = [&](const fs::path& dir) {
                    for (const auto& entry : fs::directory_iterator(dir, ec)) {
                        if (entry.path().extension() == ".rbx" &&
                            entry.path().stem().string().size() > 5 &&
                            entry.path().stem().string().ends_with(".test")) {
                            files.push_back(entry.path());
                        }
                    }
                };
                collect(target);
                if (fs::is_directory(fs::path(target) / "Source", ec)) collect(fs::path(target) / "Source");
                std::sort(files.begin(), files.end());
                if (files.empty()) {
                    std::cerr << "robix test: no .test.rbx files in " << target << "\n";
                    return 1;
                }
                for (const auto& file : files) status |= runOne(file);
                std::cout << (status == 0 ? "conformance: both engines passed\n" : "conformance: failed\n");
                return status == 0 ? 0 : 1;
            }
            return runOne(target);
        }
    }

    // General commands dispatch via RobixCLIDispatcher
    auto cliResult = Robix::Tools::RobixCLIDispatcher::ExecuteCommand(
        Robix::Tools::CLIBinaryKind::RobixUnifiedCLI, args);

    if (!cliResult.stdoutText.empty()) {
        std::cout << cliResult.stdoutText;
    }
    if (!cliResult.stderrText.empty()) {
        std::cerr << cliResult.stderrText;
    }

    return cliResult.exitCode;
}

