#include "NativeCompilerDriver.hpp"
#include "RobixCLIAndTestGenerator.hpp"
#include "ExamplesAndDocumentationCatalog.hpp"
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
        << "Robix CLI (robix) - Unified Language Toolchain & Package Manager\n"
        << "Roboticela Software Architecture\n\n"
        << "USAGE:\n"
        << "    robix <command> [arguments]\n\n"
        << "COMMANDS:\n"
        << "    run <file.rbx> [--native | --vm]   Execute a Robix source program\n"
        << "    build <file.rbx> [-o <out.exe>]     Compile directly to a native executable\n"
        << "    test [path] [--generate]            Run test suite (scaffolds missing tests if requested)\n"
        << "    generate-tests [path]               Generate companion .test.rbx scaffolds for all .rbx files\n"
        << "    verify [path]                       Run full semantic and static verification on files\n"
        << "    new <ProjectName>                   Initialize a new Robix project\n"
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

        auto res = Robix::Tools::NativeCompilerDriver::CompileSource(inputFile, sourceText, opts);
        if (!res.diagnosticsText.empty()) std::cerr << res.diagnosticsText;
        if (!res.toolOutput.empty()) std::cout << res.toolOutput;
        if (!res.success) {
            std::cerr << "robix build: compilation failed.\n";
            return 1;
        }
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

