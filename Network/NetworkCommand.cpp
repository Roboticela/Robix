#include "NetworkRuntime.hpp"

#include "../Compiler/Lexer/Lexer.hpp"
#include "../Compiler/Parser/Parser.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>
#include <iphlpapi.h>
#endif

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

#ifdef _WIN32
int ProbeBeyondLoopback() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "network probe: Winsock did not start\n";
        return 1;
    }
    ULONG size = 32 * 1024;
    std::vector<unsigned char> buffer(size);
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG result = GetAdaptersAddresses(AF_INET, flags, nullptr,
                                        reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(size);
        result = GetAdaptersAddresses(AF_INET, flags, nullptr,
                                      reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
    }
    std::string chosen;
    if (result == NO_ERROR) {
        for (auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()); adapter != nullptr; adapter = adapter->Next) {
            for (auto* address = adapter->FirstUnicastAddress; address != nullptr; address = address->Next) {
                auto* ipv4 = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
                if (ipv4 == nullptr || ipv4->sin_family != AF_INET) continue;
                char text[64]{};
                if (!inet_ntop(AF_INET, &ipv4->sin_addr, text, sizeof(text))) continue;
                const std::string ip = text;
                if (ip == "127.0.0.1" || ip.rfind("169.254.", 0) == 0) continue;
                chosen = ip;
                break;
            }
            if (!chosen.empty()) break;
        }
    }
    if (chosen.empty()) {
        std::cerr << "network probe: no address other than 127.0.0.1\n";
        WSACleanup();
        return 1;
    }

    DWORD encoded = 0;
    const char* subject = "CN=robix-stagen";
    if (!CertStrToNameA(X509_ASN_ENCODING, subject, CERT_X500_NAME_STR, nullptr, nullptr, &encoded, nullptr)) {
        std::cerr << "network probe: could not encode the certificate name\n";
        WSACleanup();
        return 1;
    }
    std::vector<BYTE> nameBytes(encoded);
    CertStrToNameA(X509_ASN_ENCODING, subject, CERT_X500_NAME_STR, nullptr, nameBytes.data(), &encoded, nullptr);
    CERT_NAME_BLOB name{};
    name.cbData = encoded;
    name.pbData = nameBytes.data();
    PCCERT_CONTEXT cert = CertCreateSelfSignCertificate(0, &name, 0, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (cert == nullptr) {
        std::cerr << "network probe: could not create a certificate\n";
        WSACleanup();
        return 1;
    }
    char display[256]{};
    CertGetNameStringA(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, display, sizeof(display));
    const std::string shown = display;
    if (shown.empty() || shown == "robix-stagek") {
        std::cerr << "network probe: certificate subject was not a new name\n";
        CertFreeCertificateContext(cert);
        WSACleanup();
        return 1;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in any{};
    any.sin_family = AF_INET;
    any.sin_addr.s_addr = htonl(INADDR_ANY);
    any.sin_port = 0;
    if (listener == INVALID_SOCKET || bind(listener, reinterpret_cast<sockaddr*>(&any), sizeof(any)) != 0 ||
        listen(listener, 1) != 0) {
        std::cerr << "network probe: could not listen\n";
        if (listener != INVALID_SOCKET) closesocket(listener);
        CertFreeCertificateContext(cert);
        WSACleanup();
        return 1;
    }
    sockaddr_in bound{};
    int boundLength = sizeof(bound);
    getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &boundLength);
    const unsigned short port = ntohs(bound.sin_port);
    std::thread server([&] {
        SOCKET accepted = accept(listener, nullptr, nullptr);
        if (accepted == INVALID_SOCKET) return;
        char request[8]{};
        recv(accepted, request, 4, 0);
        send(accepted, "pong", 4, 0);
        closesocket(accepted);
    });

    SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    inet_pton(AF_INET, chosen.c_str(), &destination.sin_addr);
    destination.sin_port = htons(port);
    const bool connected = client != INVALID_SOCKET &&
                           connect(client, reinterpret_cast<sockaddr*>(&destination), sizeof(destination)) == 0;
    char reply[8]{};
    int received = 0;
    if (connected) {
        send(client, "ping", 4, 0);
        received = recv(client, reply, 4, 0);
    }
    if (client != INVALID_SOCKET) closesocket(client);
    server.join();
    closesocket(listener);
    CertFreeCertificateContext(cert);
    WSACleanup();
    if (!connected || received != 4 || std::string(reply, 4) != "pong") {
        std::cerr << "network probe: no reply from " << chosen << "\n";
        return 1;
    }
    std::cout << "probe: connected " << chosen << " reply pong\n";
    std::cout << "probe: certificate " << shown << "\n";
    return 0;
}
#endif

int RunNetworkCommand(const std::vector<std::string>& args) {
    if (args.empty() || args[0] != "network") return -1;
#ifdef _WIN32
    if (args.size() >= 2 && args[1] == "probe") return ProbeBeyondLoopback();
#endif
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
