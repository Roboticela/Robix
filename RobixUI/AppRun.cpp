#include "AppRun.hpp"

#include "Document.hpp"
#include "Web/WebFoundation.hpp"
#include "Web/WebLanguage.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace fs = std::filesystem;

namespace Robix::UI {
namespace {

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

fs::path BuildRoot(const std::string& toolDirectory) {
    const fs::path tool(toolDirectory);
    if (Lower(tool.filename().string()) == "bin") {
        const fs::path build = tool.parent_path();
        if (Lower(build.filename().string()) == "build") {
            return build;
        }
    }
    return fs::current_path() / "Build";
}

std::string AppNameOf(const fs::path& input) {
    if (fs::is_directory(input)) {
        return input.filename().string();
    }
    const std::string stem = input.stem().string();
    return stem.empty() ? "App" : stem;
}

fs::path FindDocument(const fs::path& input) {
    std::error_code error;
    if (fs::is_regular_file(input, error)) {
        const std::string ext = Lower(input.extension().string());
        if (ext == ".rbxu" || ext == ".xml") {
            return input;
        }
    }
    const fs::path root = fs::is_directory(input, error) ? input : input.parent_path();
    const char* names[] = {"Main.rbxu", "Main.xml", "MainWindow.rbxu"};
    for (const char* name : names) {
        const fs::path candidate = root / name;
        if (fs::is_regular_file(candidate, error)) {
            return candidate;
        }
    }
    return {};
}

void WriteStamp(const fs::path& path, const std::string& body) {
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    std::ofstream out(path, std::ios::binary);
    out << body;
}

std::filesystem::file_time_type StampOf(const fs::path& path) {
    std::error_code error;
    return fs::last_write_time(path, error);
}

bool ScreenChanged(WindowRunOptions& options, void* user) {
    auto* path = static_cast<const fs::path*>(user);
    if (path == nullptr) {
        return false;
    }
    static bool ready = false;
    static std::filesystem::file_time_type seen{};
    const auto now = StampOf(*path);
    if (!ready) {
        seen = now;
        ready = true;
        return false;
    }
    if (now == seen) {
        return false;
    }
    seen = now;
    LoadScreenOptions(path->string(), options);
    return true;
}

#ifdef _WIN32
std::string Mime(const fs::path& path) {
    const std::string ext = Lower(path.extension().string());
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "text/javascript; charset=utf-8";
    if (ext == ".wasm") return "application/wasm";
    if (ext == ".ttf") return "font/ttf";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".webp") return "image/webp";
    if (ext == ".gif") return "image/gif";
    if (ext == ".json") return "application/json";
    return "application/octet-stream";
}

bool WriteWebPage(const fs::path& outRoot, const fs::path& document, WebForm form, const std::string& toolDirectory, std::string& error,
                  bool compile = false) {
    std::string script;
    const fs::path source = FindWebSource(document);
    if (!source.empty()) {
        const WebBuildResult built = BuildWebProgram(source, form, toolDirectory);
        if (!built.ok) {
            error = built.error.empty() ? "the web program was not written" : built.error;
            return false;
        }
        script = built.script;
        if (!built.wasm.empty()) {
            std::ofstream wasm(outRoot / "app.wasm", std::ios::binary);
            wasm.write(reinterpret_cast<const char*>(built.wasm.data()), static_cast<std::streamsize>(built.wasm.size()));
            if (!wasm) {
                error = "failed to write app.wasm";
                return false;
            }
            std::cout << "program: " << (outRoot / "app.wasm").string() << std::endl;
        }
    } else {
        std::cerr << "warning: web: no Robix program was found beside the screen.\n";
    }
    return WriteWebFoundation(outRoot, document, error, script, compile);
}

void ServeWeb(const fs::path& root, const fs::path& document, int port, bool watch, const std::string& toolDirectory) {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "robix: the web interpreter could not open a socket\n";
        return;
    }
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<u_short>(port));
    if (listener == INVALID_SOCKET || bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(listener, 8) != 0) {
        std::cerr << "robix: could not listen on http://127.0.0.1:" << port << "/\n";
        if (listener != INVALID_SOCKET) {
            closesocket(listener);
        }
        WSACleanup();
        return;
    }
    const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/";
    std::cout << "interpreter: web " << url << "\n";
    std::cout << "interpreter: cache " << root.string() << "\n";
    if (watch) {
        std::cout << "The page stays open. Press Ctrl+C to stop.\n";
        std::wstring wide(url.begin(), url.end());
        ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    auto seen = StampOf(document);
    u_long nonblock = 1;
    ioctlsocket(listener, FIONBIO, &nonblock);
    const auto started = std::chrono::steady_clock::now();
    while (true) {
        if (watch) {
            const auto now = StampOf(document);
            if (now != seen) {
                seen = now;
                std::string error;
                if (WriteWebPage(root, document, WebForm::Script, toolDirectory, error)) {
                    std::cout << "interpreter: refreshed\n";
                } else {
                    std::cerr << "robix: " << error << "\n";
                }
            }
        } else if (std::chrono::steady_clock::now() - started > std::chrono::seconds(2)) {
            break;
        }
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            std::this_thread::sleep_for(std::chrono::milliseconds(watch ? 200 : 50));
            continue;
        }
        char request[1024];
        const int count = recv(client, request, sizeof(request) - 1, 0);
        std::string relative = "index.html";
        if (count > 0) {
            request[count] = 0;
            std::string line(request, request + count);
            const auto start = line.find(' ');
            const auto end = line.find(' ', start == std::string::npos ? 0 : start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                relative = line.substr(start + 1, end - start - 1);
            }
        }
        if (relative.empty() || relative == "/") {
            relative = "index.html";
        }
        while (!relative.empty() && relative.front() == '/') {
            relative.erase(relative.begin());
        }
        const fs::path full = (root / relative).lexically_normal();
        const fs::path base = root.lexically_normal();
        std::string body;
        int status = 404;
        std::string type = "text/plain; charset=utf-8";
        std::error_code error;
        if (full.string().rfind(base.string(), 0) == 0 && fs::is_regular_file(full, error)) {
            std::ifstream in(full, std::ios::binary);
            body.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            status = 200;
            type = Mime(full);
        } else {
            body = "Not found";
        }
        std::ostringstream response;
        response << "HTTP/1.1 " << status << (status == 200 ? " OK" : " Not Found") << "\r\n";
        response << "Content-Type: " << type << "\r\n";
        response << "Content-Length: " << body.size() << "\r\nConnection: close\r\n\r\n";
        const std::string head = response.str();
        send(client, head.data(), static_cast<int>(head.size()), 0);
        send(client, body.data(), static_cast<int>(body.size()), 0);
        closesocket(client);
        if (!watch) {
            break;
        }
    }
    closesocket(listener);
    WSACleanup();
}
#endif

}  // namespace

std::string HostTarget() {
#ifdef _WIN32
    return "windows";
#else
    return "host";
#endif
}

std::string NormalizeTarget(const std::string& target) {
    const std::string key = Lower(target);
    if (key == "web") return "web";
    if (key == "windows" || key == "win32" || key == "x86_64-pc-windows-msvc") return "windows";
    if (key == "linux" || key == "x86_64-unknown-linux-gnu") return "linux";
    if (key == "macos" || key == "darwin" || key == "osx") return "macos";
    if (key == "android") return "android";
    if (key == "ios") return "ios";
    if (key == "aarch64" || key == "arm64" || key == "aarch64-unknown-none") return "aarch64";
    if (key == "arm" || key == "arm-unknown-none") return "arm";
    if (key == "riscv64" || key == "riscv64-unknown-none") return "riscv64";
    return key;
}

bool InterpreterCanRun(const std::string& target) {
    const std::string key = NormalizeTarget(target);
    return key == "web" || key == HostTarget();
}

bool CompilerCanBuild(const std::string& target) {
    const std::string key = NormalizeTarget(target);
    if (key == "web") {
        return true;
    }
    return key == HostTarget();
}

int RunInterpreter(const std::string& input, const std::string& target, bool watch, int port, const std::string& toolDirectory, WebForm form) {
    const std::string key = NormalizeTarget(target);
    if (!InterpreterCanRun(key)) {
        std::cerr << "robix: the interpreter runs the host platform (" << HostTarget() << ") and web.\n";
        std::cerr << "'" << key << "' was not started. Run it on a " << key
                  << " machine, or use --target web on this machine.\n";
        return 1;
    }
    const fs::path document = FindDocument(fs::absolute(input));
    if (document.empty()) {
        std::cerr << "robix: no screen document in '" << input << "'\n";
        return 1;
    }
    const fs::path named = (document.stem() == "Main" || document.stem() == "MainWindow") ? document.parent_path() : document;
    const fs::path cache = BuildRoot(toolDirectory) / "Cache" / (key == "web" ? "Web" : "Windows") / AppNameOf(named);
    std::cout << "interpreter: " << key << " " << document.string() << "\n";
    if (key == "web") {
        if (form == WebForm::Assembly) {
            std::cerr << "warning: web: interpretation runs as HTML, CSS, and JS. Assembly is compiled with robixc --web assembly.\n";
            form = WebForm::Script;
        }
        std::cout << "web form: " << WebFormName(form) << std::endl;
        std::string error;
        if (!WriteWebPage(cache, document, form, toolDirectory, error)) {
            std::cerr << "robix: " << error << "\n";
            return 1;
        }
        std::cout << "interpreter: cache only, not a production app\n";
#ifdef _WIN32
        ServeWeb(cache, document, port, watch, toolDirectory);
#else
        std::cout << "interpreter: " << (cache / "index.html").string() << "\n";
        (void)watch;
        (void)port;
#endif
        return 0;
    }
    WriteStamp(cache / "Session.txt", "interpreter\n" + document.string() + "\n");
    std::cout << "interpreter: cache " << cache.string() << "\n";
    WindowRunOptions options;
    options.stayOpen = watch;
    LoadScreenOptions(document.string(), options);
    options.document = document.string();
    fs::path watched = document;
    if (watch) {
        options.watch = ScreenChanged;
        options.watchUser = &watched;
        std::cout << "interpreter: watching " << document.string() << "\n";
    }
    return RunWindowsFoundation(options);
}

int CompileProduction(const std::string& input, const std::vector<std::string>& targets, const std::string& toolDirectory, WebForm form) {
    const fs::path document = FindDocument(fs::absolute(input));
    if (document.empty()) {
        std::cerr << "robixc: no screen document in '" << input << "'\n";
        return 1;
    }
    const std::string app = AppNameOf(fs::is_directory(fs::absolute(input)) ? fs::absolute(input) : document.parent_path());
    int status = 0;
    for (const std::string& raw : targets) {
        const std::string key = NormalizeTarget(raw);
        if (!CompilerCanBuild(key)) {
            std::cerr << "robixc: this machine is " << HostTarget() << ", so it cannot compile '" << key << "'.\n";
            std::cerr << "Get on a " << key << " machine and run robixc there.\n";
            std::cerr << "robixc compiles a native target only on that target's machine. Web can be compiled here.\n";
            status = 1;
            continue;
        }
        const fs::path out = BuildRoot(toolDirectory) / "Targets" / (key == "web" ? "Web" : "Windows") / app;
        std::error_code error;
        fs::create_directories(out / "Content", error);
        fs::copy_file(document, out / "Content" / document.filename(), fs::copy_options::overwrite_existing, error);
        if (key == "web") {
            std::string webError;
            std::cout << "web form: " << WebFormName(form) << std::endl;
            if (!WriteWebPage(out, document, form, toolDirectory, webError, true)) {
                std::cerr << "robixc: " << webError << "\n";
                status = 1;
                continue;
            }
            const char* entry = form == WebForm::Assembly ? "app.wasm" : "app.js";
            WriteStamp(out / "AppManifest.json",
                       "{\n  \"name\": \"" + app + "\",\n  \"target\": \"web\",\n  \"form\": \"" + std::string(WebFormName(form)) +
                           "\",\n  \"entry\": \"index.html\",\n  \"program\": \"" + entry + "\"\n}\n");
            std::cout << "compiled web app " << (out / "index.html").string() << "\n";
            continue;
        }
        const fs::path host = fs::path(toolDirectory) / "Robix.exe";
        if (!fs::is_regular_file(host, error)) {
            std::cerr << "robixc: Robix.exe is not beside robixc, so the windows app cannot be packaged\n";
            status = 1;
            continue;
        }
        fs::copy_file(host, out / (app + ".exe"), fs::copy_options::overwrite_existing, error);
        WriteStamp(out / "AppManifest.json",
                   "{\n  \"name\": \"" + app + "\",\n  \"target\": \"windows\",\n  \"entry\": \"" + app + ".exe\"\n}\n");
        std::cout << "compiled windows app " << (out / (app + ".exe")).string() << "\n";
    }
    return status;
}

}  // namespace Robix::UI
