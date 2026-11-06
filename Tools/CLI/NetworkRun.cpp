#include "NetworkRun.hpp"

#include "../../Compiler/Frontend/ImportResolver.hpp"
#include "../../Compiler/IR/SSAIR.hpp"
#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Optimizer/IRPassManager.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Semantic/SemanticAnalyzer.hpp"
#include "../../Compiler/Types/TypeChecker.hpp"
#include "../../Interpreter/Bytecode/BytecodeModule.hpp"
#include "../../Interpreter/VM/BytecodeVM.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

namespace fs = std::filesystem;

namespace Robix::Tools {
namespace {

std::mutex g_print;

std::string Trim(std::string text) {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) text.pop_back();
    std::size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) ++start;
    return text.substr(start);
}

std::map<std::string, std::string> ReadEnv(const fs::path& path) {
    std::map<std::string, std::string> values;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        values[Trim(line.substr(0, eq))] = Trim(line.substr(eq + 1));
    }
    return values;
}

bool IsNetworkProject(const fs::path& directory, fs::path& project) {
    std::error_code ec;
    if (!fs::is_directory(directory, ec)) return false;
    for (const auto& entry : fs::directory_iterator(directory, ec)) {
        if (entry.path().extension() != ".rbxproj") continue;
        std::ifstream in(entry.path());
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (text.find("Kind: Network") == std::string::npos && text.find("Network: true") == std::string::npos) continue;
        project = entry.path();
        return true;
    }
    return false;
}

struct Loaded {
    Robix::VM::BytecodeModule module;
    std::string error;
};

Loaded LoadModule(const fs::path& file) {
    Loaded loaded;
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        loaded.error = "robix run: could not read " + file.string() + "\n";
        return loaded;
    }
    const std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    Robix::Frontend::DiagnosticBag diags;
    Robix::Frontend::Lexer lexer(source, file.string(), diags);
    Robix::Frontend::Parser parser(lexer.TokenizeAll(), file.string(), diags);
    auto ast = parser.ParseCompilationUnit();
    Robix::Frontend::ResolveImports(ast, file.string(), diags);
    Robix::Frontend::TypeChecker checker(diags);
    checker.CheckCompilationUnit(ast);
    Robix::Frontend::SemanticAnalyzer semantic(diags);
    semantic.AnalyzeCompilationUnit(ast);
    if (diags.HasErrors()) {
        loaded.error = diags.FormatHumanReadable();
        return loaded;
    }
    Robix::IR::SSAIRBuilder ir(semantic, diags);
    auto irModule = ir.LowerCompilationUnit(ast);
    Robix::IR::IRPassManager passes(semantic, diags);
    passes.RunOptimizationPipeline(irModule, true);
    if (diags.HasErrors()) {
        loaded.error = diags.FormatHumanReadable();
        return loaded;
    }
    Robix::VM::BytecodeGenerator generator(diags);
    loaded.module = generator.GenerateFromIR(irModule);
    if (diags.HasErrors()) loaded.error = diags.FormatHumanReadable();
    return loaded;
}

std::vector<std::string> ServiceNames(const fs::path& root) {
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (!entry.is_directory()) continue;
        const std::string name = entry.path().filename().string();
        if (name == "Common" || name == "Packages") continue;
        if (fs::is_regular_file(entry.path() / "Source" / (name + ".rbx"), ec)) names.push_back(name);
    }
    return names;
}

void PrintLine(const std::string& name, const std::string& text) {
    std::lock_guard<std::mutex> lock(g_print);
    std::cout << "[" << name << "] " << text << "\n";
    std::cout.flush();
}

struct Listener {
    std::string name;
    SOCKET socket = INVALID_SOCKET;
    std::thread thread;
    std::atomic<bool> stop{ false };
    Robix::VM::BytecodeModule module;
};

void Serve(Listener& listener, const std::string& secret) {
    Robix::VM::BytecodeVM vm(listener.module);
    while (!listener.stop.load()) {
        fd_set ready;
        FD_ZERO(&ready);
        FD_SET(listener.socket, &ready);
        timeval wait{};
        wait.tv_sec = 0;
        wait.tv_usec = 200000;
        const int selected = select(0, &ready, nullptr, nullptr, &wait);
        if (selected <= 0) continue;
        SOCKET accepted = accept(listener.socket, nullptr, nullptr);
        if (accepted == INVALID_SOCKET) continue;
        char buffer[4096];
        const int got = recv(accepted, buffer, static_cast<int>(sizeof buffer) - 1, 0);
        std::string request = got > 0 ? std::string(buffer, buffer + got) : std::string();
        std::vector<std::string> lines;
        std::string line;
        for (char ch : request) {
            if (ch == '\n') {
                lines.push_back(Trim(line));
                line.clear();
            } else {
                line.push_back(ch);
            }
        }
        if (!line.empty()) lines.push_back(Trim(line));
        std::string reply = "missing";
        if (lines.size() < 3 || lines[0] != secret) {
            reply = "denied";
        } else if (listener.module.FindChunk(lines[1]) != nullptr) {
            Robix::VM::VMValue result = vm.ExecuteFunction(lines[1], { Robix::VM::VMValue{ std::string(lines[2]) } });
            reply = result.ToString();
            PrintLine(listener.name, lines[1] + " " + lines[2] + " -> " + reply);
        }
        reply.push_back('\n');
        send(accepted, reply.data(), static_cast<int>(reply.size()), 0);
        closesocket(accepted);
    }
}

} // namespace

int RunProjectServices(const std::vector<std::string>& args, const std::string& toolDir) {
    if (args.empty() || args[0] != "run") return -1;
    bool separate = false;
    bool listenOnly = false;
    std::vector<std::string> positionals;
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--separate") separate = true;
        else if (args[i] == "--listen") listenOnly = true;
        else if (!args[i].empty() && args[i][0] == '-') return -1;
        else positionals.push_back(args[i]);
    }
    if (!positionals.empty()) {
        const fs::path file(positionals[0]);
        std::error_code ec;
        if (file.extension() == ".rbx" || fs::is_regular_file(file, ec)) return -1;
    }

    fs::path project;
    if (!IsNetworkProject(fs::current_path(), project)) return -1;
    const fs::path root = project.parent_path();
    const auto env = ReadEnv(root / ".env.common");
    const auto secretIt = env.find("SECRET");
    if (secretIt == env.end() || secretIt->second.empty()) {
        std::cerr << "robix run: .env.common has no SECRET.\n";
        return 1;
    }
    std::vector<std::string> names = ServiceNames(root);
    if (positionals.empty()) {
        std::cerr << "robix run: name a client, a server, or all.\n";
        return 1;
    }
    if (positionals[0] == "client" || positionals[0] == "server") {
        if (positionals.size() < 2) {
            std::cerr << "robix run: " << positionals[0] << " needs a name.\n";
            return 1;
        }
        positionals = { positionals[1] };
    }
    if (positionals[0] != "all") {
        bool found = false;
        for (const auto& name : names) found = found || name == positionals[0];
        if (!found) {
            std::cerr << "robix run: '" << positionals[0] << "' is not a client or server in this project.\n";
            return 1;
        }
        names = { positionals[0] };
    }

    if (separate && !listenOnly) {
        for (const auto& name : names) {
            std::string command = "\"" + (fs::path(toolDir) / "robix.exe").string() + "\" run " + name + " --listen";
            STARTUPINFOA startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            std::vector<char> mutableCommand(command.begin(), command.end());
            mutableCommand.push_back('\0');
            if (!CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE,
                                nullptr, root.string().c_str(), &startup, &process)) {
                std::cerr << "robix run: could not open a console for " << name << ".\n";
                return 1;
            }
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            std::cout << "robix run: separate " << name << "\n";
        }
    }

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "robix run: network startup failed.\n";
        return 1;
    }
    std::vector<std::unique_ptr<Listener>> listeners;
    if (!separate || listenOnly) {
        for (const auto& name : names) {
            const auto host = env.find(name + "_HOST");
            const auto port = env.find(name + "_PORT");
            if (host == env.end() || port == env.end()) {
                std::cerr << "robix run: .env.common has no address for " << name << ".\n";
                WSACleanup();
                return 1;
            }
            auto listener = std::make_unique<Listener>();
            listener->name = name;
            Loaded loaded = LoadModule(root / name / "Source" / (name + ".rbx"));
            if (!loaded.error.empty()) {
                std::cerr << loaded.error;
                WSACleanup();
                return 1;
            }
            listener->module = std::move(loaded.module);
            listener->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            sockaddr_in address{};
            address.sin_family = AF_INET;
            inet_pton(AF_INET, host->second.c_str(), &address.sin_addr);
            address.sin_port = htons(static_cast<unsigned short>(std::atoi(port->second.c_str())));
            if (listener->socket == INVALID_SOCKET ||
                bind(listener->socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
                listen(listener->socket, 4) != 0) {
                std::cerr << "robix run: could not listen for " << name << " on " << host->second << ":" << port->second << ".\n";
                WSACleanup();
                return 1;
            }
            PrintLine(name, "listening " + host->second + ":" + port->second);
            listeners.push_back(std::move(listener));
        }
        for (auto& listener : listeners) {
            Listener* raw = listener.get();
            raw->thread = std::thread([raw, secret = secretIt->second] { Serve(*raw, secret); });
        }
    }
    if (listenOnly) {
        for (auto& listener : listeners) {
            if (listener->thread.joinable()) listener->thread.join();
        }
        WSACleanup();
        return 0;
    }

    if (separate) {
        const auto started = std::chrono::steady_clock::now();
        for (const auto& name : names) {
            const auto port = env.find(name + "_PORT");
            const auto host = env.find(name + "_HOST");
            bool open = false;
            while (std::chrono::steady_clock::now() - started < std::chrono::seconds(3)) {
                SOCKET probe = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                sockaddr_in destination{};
                destination.sin_family = AF_INET;
                inet_pton(AF_INET, host->second.c_str(), &destination.sin_addr);
                destination.sin_port = htons(static_cast<unsigned short>(std::atoi(port->second.c_str())));
                open = probe != INVALID_SOCKET &&
                       connect(probe, reinterpret_cast<sockaddr*>(&destination), sizeof(destination)) == 0;
                if (probe != INVALID_SOCKET) closesocket(probe);
                if (open) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            if (!open) {
                std::cerr << "robix run: " << name << " did not start listening.\n";
                WSACleanup();
                return 1;
            }
        }
    }

    int status = 0;
    for (const auto& name : names) {
        Loaded loaded = LoadModule(root / name / "Source" / (name + ".rbx"));
        if (!loaded.error.empty()) {
            std::cerr << loaded.error;
            status = 1;
            break;
        }
        Robix::VM::BytecodeVM vm(loaded.module);
        if (vm.ExecuteMainAndGetExitCode() != 0 || !vm.Fault().empty()) status = 1;
        std::istringstream output(vm.GetStdoutBuffer());
        std::string line;
        while (std::getline(output, line)) PrintLine(name, Trim(line));
    }

    for (auto& listener : listeners) {
        listener->stop.store(true);
        if (listener->thread.joinable()) listener->thread.join();
        if (listener->socket != INVALID_SOCKET) closesocket(listener->socket);
    }
    WSACleanup();
    std::cout << "robix run: stopped\n";
    return status;
}

} // namespace Robix::Tools
