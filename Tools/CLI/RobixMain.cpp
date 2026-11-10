#include "NativeCompilerDriver.hpp"
#include "ProjectFiles.hpp"
#include "UiProjectBuild.hpp"
#include "../../RobixUI/AppRun.hpp"
#include "../../RobixUI/Document.hpp"
#include "../../Network/NetworkRuntime.hpp"
#include "DeveloperCommands.hpp"
#include "ProjectScaffold.hpp"
#include "NetworkRun.hpp"
#include "RobixCLIAndTestGenerator.hpp"
#include "ExecutionGate.hpp"
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
        << "    run                                 Run the project in this folder\n"
        << "    run <file.rbx> [more.rbx] [--target windows|linux|macos|android|ios|web] [--web script|assembly] [--image]\n"
        << "                                    Run only the named file or files.\n"
        << "    run all                             Run every program in the folder, one by one\n"
        << "    run <folder>                        Run that project folder\n"
        << "    run <screen.rbxu> [--target web|windows] [--watch] [--port N]\n"
        << "                                    Interpret a screen on the host platform or web.\n"
        << "    run all [--separate]                Run every network client and server\n"
        << "    run client <Name> | server <Name>   Run one service in this console\n"
        << "    build [folder] [-o <out.exe>]       Compile each source to its own file. Skip unchanged files.\n"
        << "    build <file.rbx> [-o <out.exe>]     Compile that file. Skip it when nothing changed.\n"
        << "    test [path] [--generate]            Run .test.rbx on the interpreter. Test native also compiles.\n"
        << "    generate-tests [path]               Generate companion .test.rbx scaffolds for all .rbx files\n"
        << "    verify [path]                       Run full semantic and static verification on files\n"
        << "    new [.] [Name] [Application|Canvas|Bedrock] [kind]\n"
        << "                                    Create a project. . uses this directory. No name asks.\n"
        << "                                    Application: Console, Window, Network.\n"
        << "                                    Canvas: an empty project.\n"
        << "                                    Bedrock: Kernel, OS, BIOS, Driver, Bootloader, Firmware.\n"
        << "    new Name Network --client Name CLI|GUI --server Name CLI|GUI\n"
        << "    new file <Name> [hello|empty|class|package]   Add a source file and its test\n"
        << "    add <name> <file.rbx>               Lock a local package with SHA-256 and an HMAC signature\n"
        << "    remove <name>                       Remove a package from the lock\n"
        << "    update <name>                       Rehash a locked package file\n"
        << "    install                             Copy locked packages into Packages/ when the hash matches\n"
        << "    restore                             Check every locked hash and signature\n"
        << "    ui validate <file.rbxu> --code <file.rbx>   Parse, lay out, and type-check event handlers\n"
        << "    ui render <file.rbxu> -o <file.bmp>         Draw one software frame as a BMP\n"
        << "    ui window <file.rbxu> [--dev]               Open the Screen on Windows. --dev keeps it open\n"
        << "    ui web <file.rbxu> [--dev]                  Run the page directly. --dev does not write an HTML file\n"
        << "    ui render <file.rbxu> -o <file.bmp>         Draw the framebuffer\n"
        << "    network check <project>             Check a Network block, contract, and env split\n"
        << "    network run <project>               Run the local contract call, timeout, and TLS handshake\n"
        << "    format <file.rbx> [--check]         Rewrite source with 4-space indentation\n"
        << "    lint <file.rbx> [--strict]          Type-check and enforce style\n"
        << "    check <file.rbx>                    Type-check without running the program\n"
        << "    doc <file.rbx> [-o <file.md>]       Write markdown from Explain blocks\n"
        << "    debug <file.rbx>                    Step the interpreter and a debug native build by source line\n"
        << "    lsp                                 Speak Language Server Protocol on stdin\n"
        << "    examples [topic]                    List or extract official Robix code examples\n"
        << "    repl                                Start the interactive Robix REPL\n"
        << "    version                             Display version and toolchain info\n\n"
        << "OPTIONS:\n"
        << "    -v, --version                       Display version information\n"
        << "    -h, --help                          Show this help documentation\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        const fs::path content = fs::absolute(argv[0]).parent_path() / "Content";
        const char* names[] = {"Main.rbxu", "Main.xml", "MainWindow.rbxu"};
        for (const char* name : names) {
            const fs::path document = content / name;
            if (!fs::is_regular_file(document)) {
                continue;
            }
            Robix::UI::WindowRunOptions options;
            options.stayOpen = true;
            Robix::UI::LoadScreenOptions(document.string(), options);
            options.document = document.string();
            return Robix::UI::RunWindowsFoundation(options);
        }
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
    const int uiStatus = Robix::UI::RunUiCommand(args);
    if (uiStatus >= 0) return uiStatus;
    const int networkStatus = Robix::Network::RunNetworkCommand(args);
    if (networkStatus >= 0) return networkStatus;
    const int developerStatus = Robix::Tools::RunDeveloperCommand(args, toolDir);
    if (developerStatus >= 0) return developerStatus;

    // `robix build` compiles each project source to its own file.
    // `robix build <file.rbx>` compiles that one file.
    if (cmd == "build") {
        std::string inputFile;
        Robix::Tools::NativeBuildOptions base;
        base.toolDirectory = toolDir;

        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "-o" && i + 1 < args.size()) {
                base.outputPath = args[++i];
            } else if (args[i] == "-O0" || args[i] == "--debug") {
                base.release = false;
            } else if (args[i] == "-O2" || args[i] == "-O3" || args[i] == "--release") {
                base.release = true;
            } else if (args[i] == "--keep-ir") {
                base.keepLLVMIR = true;
            } else if (args[i] == "--clang" && i + 1 < args.size()) {
                base.clangPath = args[++i];
            } else if (args[i] == "--runtime-obj" && i + 1 < args.size()) {
                base.runtimeObjectPath = args[++i];
            } else if (!args[i].empty() && args[i][0] != '-') {
                if (inputFile.empty()) inputFile = args[i];
            }
        }

        struct BuildUnit {
            std::string file;
            bool entry = true;
        };
        std::vector<BuildUnit> units;
        std::error_code inputKind;
        const bool projectBuild = inputFile.empty() || fs::is_directory(inputFile, inputKind);
        fs::path projectAnchor = inputFile.empty() ? fs::current_path() : fs::path(inputFile);
        if (projectBuild) {
            const Robix::Tools::ProjectRun project = Robix::Tools::ChooseProjectRun(
                inputFile.empty() ? fs::path{} : fs::path(inputFile), false, false);
            if (project.mode != Robix::Tools::ProjectRun::Mode::Project) {
                std::cerr << (project.error.empty() ? "robix build: this folder has no program.\n" : project.error);
                return 1;
            }
            projectAnchor = project.root;
            bool sawEntry = false;
            for (const fs::path& source : Robix::Tools::CompilerBuildSources(project)) {
                const bool entry = Robix::Tools::CompilerBuildIsEntry(project, source);
                sawEntry = sawEntry || entry;
                units.push_back(BuildUnit{ source.string(), entry });
            }
            if (!units.empty() && !sawEntry) units.back().entry = true;
        } else {
            units.push_back(BuildUnit{ inputFile, true });
        }
        if (units.empty()) {
            std::cerr << "robix build: error: no input file specified.\n";
            return 1;
        }

        Robix::Tools::ProjectFile projectFile;
        if (Robix::Tools::FindProject(projectAnchor, projectFile) && projectFile.network) {
            base.linkNetwork = true;
            const fs::path installed = fs::path(toolDir) / ".." / "Lib" / "Robix" / "1.0.0" / "NetworkRuntime.obj";
            const fs::path fromRoot = fs::current_path() / "Build" / "Lib" / "Robix" / "1.0.0" / "NetworkRuntime.obj";
            if (fs::exists(installed)) {
                base.networkObjectPath = installed.string();
            } else {
                base.networkObjectPath = fromRoot.string();
            }
        }
        const std::string missingTest = Robix::Tools::MissingTestMessage(projectAnchor);
        if (!missingTest.empty()) {
            std::cerr << missingTest;
            return 1;
        }

        const std::string requestedOutput = base.outputPath;
        for (const BuildUnit& unit : units) {
            std::ifstream srcFile(unit.file, std::ios::binary);
            if (!srcFile.is_open()) {
                std::cerr << "robix build: error: could not open file '" << unit.file << "'\n";
                return 1;
            }
            std::string sourceText((std::istreambuf_iterator<char>(srcFile)),
                                   std::istreambuf_iterator<char>());
            srcFile.close();

            Robix::Tools::NativeBuildOptions opts = base;
            if (!unit.entry) {
                opts.omitMain = true;
                opts.outputKind = Robix::Tools::NativeOutputKind::HostedObject;
                fs::path objectPath(unit.file);
                objectPath.replace_extension(".o");
                opts.outputPath = objectPath.string();
            } else if (units.size() > 1) {
                opts.outputPath = requestedOutput;
            }
            if (opts.outputPath.empty()) {
                fs::path p(unit.file);
                p.replace_extension(".exe");
                opts.outputPath = p.string();
            }
            if (units.size() > 1) std::cout << unit.file << std::endl;
            if (Robix::Tools::IncrementalHit(unit.file, opts.outputPath)) {
                std::cout << "incremental: unchanged " << opts.outputPath << "\n";
                continue;
            }

            auto res = Robix::Tools::NativeCompilerDriver::CompileSource(unit.file, sourceText, opts);
            if (!res.diagnosticsText.empty()) std::cerr << res.diagnosticsText;
            if (!res.toolOutput.empty()) std::cout << res.toolOutput;
            if (!res.success) {
                std::cerr << "robix build: compilation failed.\n";
                return 1;
            }
            Robix::Tools::WriteIncrementalStamp(unit.file, res.executablePath);
            std::cout << "robix build: created " << (unit.entry ? "executable" : "object")
                      << " -> " << res.executablePath << "\n";
        }
        return 0;
    }

    // `robix run <file.rbx> --native`
    bool wantsNativeRun = false;
    for (const auto& a : args) {
        if (a == "--native") wantsNativeRun = true;
    }

    if (cmd == "run" && wantsNativeRun) {
        std::cerr << "robix: --native compiles an executable. Use robixc to build the production app.\n";
        return 1;
    }

    if (cmd == "new") return Robix::Tools::RunNewCommand(args);
    if (cmd == "run") {
        std::string runInput;
        std::vector<std::string> runFiles;
        std::string runTarget = Robix::UI::HostTarget();
        bool targetSet = false;
        bool image = false;
        bool watch = false;
        int port = 4173;
        Robix::UI::WebForm webForm = Robix::UI::WebForm::Script;
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--target" && i + 1 < args.size()) {
                runTarget = args[++i];
                targetSet = true;
            } else if (args[i] == "--web" && i + 1 < args.size()) {
                bool recognized = false;
                webForm = Robix::UI::ParseWebForm(args[++i], recognized);
                if (!recognized) {
                    std::cerr << "robix: --web must be script or assembly.\n";
                    return 1;
                }
            } else if (args[i] == "--image") {
                image = true;
            } else if (args[i] == "--watch") {
                watch = true;
            } else if (args[i] == "--port" && i + 1 < args.size()) {
                port = std::stoi(args[++i]);
            } else if (!args[i].empty() && args[i][0] != '-') {
                if (runInput.empty()) runInput = args[i];
                if (fs::path(args[i]).extension() == ".rbx") runFiles.push_back(args[i]);
            }
        }
        const std::string targetKey = Robix::UI::NormalizeTarget(runTarget);
        const bool direct = image || Robix::Tools::IsDirectName(targetKey) || Robix::Tools::IsDirectName(runTarget);
        bool resolvedProject = false;
        if (runFiles.empty() && targetKey != "web") {
            const int services = Robix::Tools::RunProjectServices(args, toolDir);
            if (services >= 0) return services;
        }
        std::error_code runError;
        const bool runFolder = runInput.empty() || runInput == "all" || fs::is_directory(runInput, runError);
        if (runFiles.empty() && runFolder) {
            const Robix::Tools::ProjectRun choice =
                Robix::Tools::ChooseProjectRun(runInput == "all" ? fs::path{} : fs::path(runInput), runInput == "all");
            if (choice.mode == Robix::Tools::ProjectRun::Mode::Network && targetKey == "web") {
                std::cout << "platform: web" << std::endl;
                if (!choice.buildFile.empty()) std::cout << "rbxb: " << choice.buildFile.string() << std::endl;
                if (choice.ui.empty()) {
                    std::cerr << "error: the build file has no UI screen, so this project cannot run for web.\n";
                    return 1;
                }
                std::cout << "screen: " << choice.ui.front().string() << std::endl;
                return Robix::UI::RunInterpreter(choice.ui.front().string(), "web", true, port, toolDir, webForm);
            }
            if (choice.mode == Robix::Tools::ProjectRun::Mode::Network) {
                std::vector<std::string> serviceArgs = { "run", "all" };
                for (const std::string& arg : args) {
                    if (arg == "--separate" || arg == "--listen") serviceArgs.push_back(arg);
                }
                const fs::path previous = fs::current_path();
                if (!choice.root.empty()) fs::current_path(choice.root);
                const int services = Robix::Tools::RunProjectServices(serviceArgs, toolDir);
                fs::current_path(previous);
                return services < 0 ? 1 : services;
            }
            if (choice.mode == Robix::Tools::ProjectRun::Mode::Absent) {
                const bool folderScreen = !runInput.empty() && runInput != "all" && fs::is_directory(runInput, runError);
                if (!folderScreen) {
                    std::cerr << choice.error;
                    return 1;
                }
            } else {
                runFiles.clear();
                for (const fs::path& file : choice.files) runFiles.push_back(file.string());
                runInput = runFiles.front();
                resolvedProject = true;
                if (choice.mode == Robix::Tools::ProjectRun::Mode::Project) {
                    const std::string platform = targetSet ? targetKey : Robix::UI::HostTarget();
                    std::cout << "platform: " << platform << std::endl;
                    if (!choice.buildFile.empty()) {
                        std::cout << "rbxb: " << choice.buildFile.string() << std::endl;
                        std::cout << "order:";
                        const std::vector<fs::path>& listed = choice.order.empty() ? choice.files : choice.order;
                        for (const fs::path& file : listed) std::cout << " " << file.lexically_relative(choice.root).string();
                        std::cout << std::endl;
                    }
                    if (!choice.ui.empty() && (platform == "web" || platform == Robix::UI::HostTarget())) {
                        std::cout << "screen: " << choice.ui.front().string() << std::endl;
                        if (platform == "web") {
                            return Robix::UI::RunInterpreter(choice.ui.front().string(), "web", true, port, toolDir, webForm);
                        }
                        std::cout << "The window stays open until you close it." << std::endl;
                        return Robix::UI::RunInterpreter(choice.ui.front().string(), platform, true, port, toolDir);
                    }
                    if (platform == "web") {
                        std::cerr << "error: the build file has no UI screen, so this project cannot run for web.\n";
                        return 1;
                    }
                    std::cout << "project: " << runInput << std::endl;
                }
            }
        }
        const std::string extension = fs::path(runInput).extension().string();
        const bool screen = extension == ".rbxu" || extension == ".xml" || extension == ".r3d" ||
                            (fs::is_directory(runInput) && runInput != "all");
        if (!runFiles.empty()) {
            const std::string category = Robix::Tools::ProjectCategory(runFiles.front());
            const bool gated = !category.empty() || targetSet || image;
            const Robix::Tools::RunSurface surface = direct ? Robix::Tools::RunSurface::Direct
                                                            : Robix::Tools::RunSurface::Platform;
            if (gated) {
                for (const std::string& file : runFiles) {
                    std::ifstream in(file, std::ios::binary);
                    if (!in) {
                        std::cerr << "error: could not read '" << file << "'.\n";
                        return 1;
                    }
                    const std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                    const std::string fileCategory = Robix::Tools::ProjectCategory(file);
                    const std::string mismatch = Robix::Tools::ExecutionError(
                        fileCategory.empty() ? category : fileCategory,
                        surface,
                        direct ? "a machine image" : targetKey,
                        Robix::Tools::ScanCodeShape(source));
                    if (!mismatch.empty()) {
                        std::cerr << file << ": " << mismatch;
                        return 1;
                    }
                    if (!direct) {
                        const std::string kind = Robix::Tools::ProjectKind(file);
                        const Robix::Tools::WebDecision web = Robix::Tools::DecideWeb(
                            fileCategory.empty() ? category : fileCategory,
                            kind,
                            source,
                            file,
                            targetKey == "web");
                        if (!web.warning.empty()) std::cerr << file << ": " << web.warning;
                        if (!web.error.empty()) {
                            std::cerr << file << ": " << web.error;
                            return 1;
                        }
                        if (targetKey == "web") {
                            return Robix::UI::RunInterpreter(web.screen, runTarget, watch, port, toolDir, webForm);
                        }
                    }
                }
            }
            if (direct) {
                if (!image && category != "Bedrock" && category != "Canvas" && !targetSet) {
                    std::cerr << "error: a machine image is built with robixc --image.\n";
                    return 1;
                }
                int status = 0;
                for (const std::string& file : runFiles) {
                    std::ifstream in(file, std::ios::binary);
                    const std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                    Robix::Tools::NativeBuildOptions imageOptions;
                    imageOptions.toolDirectory = toolDir;
                    imageOptions.release = true;
                    imageOptions.freestanding = true;
                    imageOptions.outputKind = Robix::Tools::NativeOutputKind::BootImage;
                    imageOptions.targetTriple = "x86_64-unknown-none";
                    imageOptions.clangTarget = "x86_64-elf";
                    imageOptions.dataLayout =
                        "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
                    imageOptions.entrySymbol = "Start";
                    imageOptions.profileName = "embedded";
                    fs::path out = fs::path("Build") / "Images" / (fs::path(file).stem().string() + ".elf");
                    imageOptions.outputPath = out.string();
                    auto built = Robix::Tools::NativeCompilerDriver::CompileSource(file, source, imageOptions);
                    if (!built.diagnosticsText.empty()) std::cerr << built.diagnosticsText;
                    if (!built.toolOutput.empty()) std::cout << built.toolOutput;
                    if (!built.success) {
                        std::cerr << "error: image was not built for '" << file << "'.\n";
                        return 1;
                    }
                    std::cout << "image: " << built.executablePath << "\n";
                    status = 0;
                }
                return status;
            }
            if (category == "Bedrock") {
                std::cerr << "error: Bedrock does not run in the interpreter. Build the image with robixc --image.\n";
                return 1;
            }
        }
        if (screen && runInput != "all" && runInput != "client" && runInput != "server") {
            if (direct) {
                std::cerr << "error: a screen runs on a platform. It does not build a machine image.\n";
                return 1;
            }
            const std::string screenCategory = Robix::Tools::ProjectCategory(runInput);
            if (screenCategory == "Bedrock") {
                std::cerr << "error: Bedrock does not run on " << targetKey << ". It builds a machine image.\n";
                return 1;
            }
            if (!Robix::UI::InterpreterCanRun(targetKey)) {
                std::cerr << "error: this machine interprets windows and web. '" << targetKey << "' was not started.\n";
                return 1;
            }
            return Robix::UI::RunInterpreter(runInput, runTarget, watch, port, toolDir, webForm);
        }
        if (direct) {
            std::cerr << "error: a machine image does not run in the interpreter. Build it with robixc --image.\n";
            return 1;
        }
        if (targetSet && !Robix::UI::InterpreterCanRun(targetKey)) {
            std::cerr << "error: this machine interprets windows and web. '" << targetKey << "' was not started.\n";
            return 1;
        }
        if (!runFiles.empty() && (runFiles.size() > 1 || resolvedProject)) {
            int status = 0;
            for (const std::string& file : runFiles) {
                if (runFiles.size() > 1) std::cout << file << "\n";
                auto interpreted = Robix::Tools::RobixCLIDispatcher::ExecuteCommand(
                    Robix::Tools::CLIBinaryKind::RobixUnifiedCLI, { "run", file });
                if (!interpreted.stdoutText.empty()) std::cout << interpreted.stdoutText;
                if (!interpreted.stderrText.empty()) std::cerr << interpreted.stderrText;
                if (interpreted.exitCode != 0) status = interpreted.exitCode;
            }
            return status;
        }
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

    if (cmd == "run") {
        std::vector<std::string> programArgs;
        bool seenFile = false;
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--native" || args[i] == "--vm" || args[i] == "--engine") continue;
            if (!seenFile) {
                seenFile = true;
                continue;
            }
            if (!args[i].empty() && args[i][0] == '-') continue;
            programArgs.push_back(args[i]);
        }
        Robix::VM::BytecodeVM::SetProgramArguments(programArgs);
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

