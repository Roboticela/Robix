#include "NetworkRuntime.hpp"

#include "../Compiler/Lexer/Lexer.hpp"
#include "../Compiler/Parser/Parser.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string Trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

std::string ReadFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

bool WriteText(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
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

struct Method {
    std::string name;
    std::string argument;
    std::string returns;
};

void Collect(const Robix::Frontend::DeclNode& decl, std::vector<Method>& out) {
    if (decl.kind == Robix::Frontend::DeclKind::Function && !decl.parameters.empty()) {
        Method method;
        method.name = decl.name;
        for (std::size_t i = 0; i < decl.parameters.size(); ++i) {
            if (i > 0) method.argument += ",";
            method.argument += decl.parameters[i].type.baseName;
        }
        method.returns = decl.returnType.baseName.empty() ? "Void" : decl.returnType.baseName;
        out.push_back(method);
    }
    for (const auto& member : decl.members) {
        if (member) Collect(*member, out);
    }
}

bool LoadContract(const fs::path& commonDir, std::vector<Method>& methods, std::string& error) {
    std::error_code ec;
    if (!fs::is_directory(commonDir, ec)) {
        error = "network: Common/ is missing\n";
        return false;
    }
    for (const auto& entry : fs::directory_iterator(commonDir, ec)) {
        if (entry.path().extension() != ".rbx") continue;
        const std::string source = ReadFile(entry.path());
        Robix::Frontend::DiagnosticBag diagnostics;
        Robix::Frontend::Lexer lexer(source, entry.path().string(), diagnostics);
        const auto tokens = lexer.TokenizeAll();
        Robix::Frontend::Parser parser(tokens, entry.path().string(), diagnostics);
        const auto unit = parser.ParseCompilationUnit();
        if (diagnostics.HasErrors()) {
            error = diagnostics.FormatHumanReadable();
            return false;
        }
        for (const auto& decl : unit.declarations) {
            if (decl) Collect(*decl, methods);
        }
    }
    if (methods.empty()) {
        error = "network: Common/ has no typed call or event\n";
        return false;
    }
    return true;
}

} // namespace

namespace Robix::Network {

int RunNetworkCommand(const std::vector<std::string>& args) {
    if (args.empty() || args[0] != "network") return -1;
    if (args.size() < 3 || (args[1] != "check" && args[1] != "run")) {
        std::cerr << "robix network: use check <project> or run <project>\n";
        return 1;
    }
    const fs::path root = args[2];
    fs::path projectPath;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (entry.path().extension() == ".rbxproj") projectPath = entry.path();
    }
    if (ReadFile(projectPath).find("Network: true") == std::string::npos) {
        std::cerr << "network: " << root.string() << " has no Network block\n";
        return 1;
    }
    std::vector<Method> methods;
    std::string error;
    if (!LoadContract(root / "Common", methods, error)) {
        std::cerr << error;
        return 1;
    }
    std::ostringstream methodText;
    for (const auto& method : methods) {
        std::cout << "contract " << method.name << "(" << method.argument << ") -> " << method.returns << "\n";
        methodText << method.name << " " << method.argument << " " << method.returns << "\n";
    }
    const auto shared = ReadEnv(root / ".env");
    const auto client = ReadEnv(root / ".env.client");
    const auto server = ReadEnv(root / ".env.server");
    for (const auto& [key, value] : server) {
        if (client.find(key) != client.end()) {
            std::cerr << "network: server key " << key << " is also in .env.client\n";
            return 1;
        }
    }
    std::map<std::string, std::string> clientBundle = shared;
    clientBundle.insert(client.begin(), client.end());
    std::map<std::string, std::string> serverBundle = shared;
    serverBundle.insert(server.begin(), server.end());
    std::ostringstream clientText;
    std::ostringstream serverText;
    for (const auto& [key, value] : clientBundle) clientText << key << "=" << value << "\n";
    for (const auto& [key, value] : serverBundle) serverText << key << "=" << value << "\n";
    const fs::path buildDir = "Build/StageK";
    if (!WriteText(buildDir / "client.env", clientText.str()) || !WriteText(buildDir / "server.env", serverText.str()) ||
        !WriteText(buildDir / "methods.txt", methodText.str())) {
        std::cerr << "network: could not write the split environment\n";
        return 1;
    }
    for (const auto& [key, value] : server) {
        if (!value.empty() && clientText.str().find(value) != std::string::npos) {
            std::cerr << "network: server value for " << key << " would ship in the client environment\n";
            return 1;
        }
    }
    std::cout << "env: client keys " << clientBundle.size() << ", server keys " << serverBundle.size() << "\n";
    std::cout << "client-secret: absent\n";
    if (args[1] == "check") {
        std::cout << "network-check: ok\n";
        return 0;
    }
    const int code = rbx_network_run((buildDir / "server.env").string().c_str(), (buildDir / "methods.txt").string().c_str());
    if (code != 0) return code;
    std::cout << "network: ok\n";
    return 0;
}

} // namespace Robix::Network
