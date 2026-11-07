#include "Document.hpp"

#include "Platform/Windows/WindowsHost.hpp"
#include "Scene.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

namespace Robix::UI {
namespace {

std::string ReadFile(const std::string& path, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "could not open '" + path + "'";
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool Load(const std::string& path, Scene& scene, std::string& error) {
    const std::string source = ReadFile(path, error);
    if (!error.empty()) {
        return false;
    }
    return LoadScene(source, scene, error);
}

int RenderBmp(const std::string& input, const std::string& output) {
    Scene scene;
    std::string error;
    if (!Load(input, scene, error)) {
        std::cerr << error << "\n";
        return 1;
    }
    Robo::FrameBuffer buffer;
    PaintScene(scene, buffer);
    if (!WriteBmp(output, buffer, error)) {
        std::cerr << error << "\n";
        return 1;
    }
    std::cout << scene.foundationLine << "\n";
    std::cout << "renderer platform-framebuffer\n";
    std::cout << "framebuffer " << buffer.GetWidth() << "x" << buffer.GetHeight() << "\n";
    for (const std::string& line : scene.layoutLines) {
        std::cout << line << "\n";
    }
    std::cout << "wrote " << output << "\n";
    return 0;
}

int ServeWeb(const Scene& scene, bool once) {
    const std::string page = SceneHtml(scene);
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "web: winsock failed\n";
        return 1;
    }
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (listener == INVALID_SOCKET || bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(listener, 1) != 0) {
        std::cerr << "web: could not listen\n";
        WSACleanup();
        return 1;
    }
    int length = sizeof(address);
    getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length);
    const unsigned port = ntohs(address.sin_port);
    std::cout << "web: running http://127.0.0.1:" << port << "/" << std::endl;
    std::cout << scene.foundationLine << "\n";
    std::cout << "component Screen\n";
    std::cout << "class " << "robo-screen" << " theme " << scene.screen.theme << "\n";
    for (const std::string& line : scene.layoutLines) {
        std::cout << line << "\n";
    }
    const int visits = once ? 1 : 64;
    for (int visit = 0; visit < visits; ++visit) {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            break;
        }
        char request[1024];
        recv(client, request, sizeof(request), 0);
        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " << page.size()
                 << "\r\nConnection: close\r\n\r\n"
                 << page;
        const std::string bytes = response.str();
        send(client, bytes.data(), static_cast<int>(bytes.size()), 0);
        closesocket(client);
        if (once) {
            break;
        }
    }
    closesocket(listener);
    WSACleanup();
    std::cout << "web: closed\n";
    return 0;
}

}  // namespace

int RunUiCommand(const std::vector<std::string>& args) {
    if (args.empty() || args[0] != "ui" || args.size() < 2) {
        return -1;
    }
    const std::string action = args[1];
    if (action != "window" && action != "web" && action != "render" && action != "validate") {
        return -1;
    }
    std::string input;
    std::string output;
    bool stayOpen = false;
    bool trace = false;
    bool once = false;
    for (std::size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--dev") {
            stayOpen = true;
        } else if (args[i] == "--trace") {
            trace = true;
        } else if (args[i] == "--once") {
            once = true;
        } else if ((args[i] == "-o" || args[i] == "--out") && i + 1 < args.size()) {
            output = args[++i];
        } else if (!args[i].empty() && args[i][0] != '-') {
            input = args[i];
        }
    }
    if (input.empty()) {
        std::cerr << "robix ui: no .rbxu file\n";
        return 1;
    }
    Scene scene;
    std::string error;
    if (!Load(input, scene, error)) {
        std::cerr << "robix ui: " << error << "\n";
        return 1;
    }
    if (action == "validate") {
        std::cout << "component Screen\n";
        std::cout << scene.foundationLine << "\n";
        for (const std::string& line : scene.layoutLines) {
            std::cout << line << "\n";
        }
        std::cout << std::flush;
        return 0;
    }
    if (action == "render") {
        if (output.empty()) {
            output = "framebuffer.bmp";
        }
        return RenderBmp(input, output);
    }
    if (action == "web") {
        if (!output.empty()) {
            std::ofstream out(output, std::ios::binary);
            if (!out) {
                std::cerr << "robix ui: could not write " << output << "\n";
                return 1;
            }
            out << SceneHtml(scene);
            std::cout << "web " << output << " " << scene.screen.width << "x" << scene.screen.height << "\n";
            std::cout << scene.foundationLine << "\n";
            return 0;
        }
        return ServeWeb(scene, once || !stayOpen);
    }
    WindowRunOptions options;
    options.stayOpen = stayOpen;
    options.trace = trace;
    return RunWindowsScreen(scene, options);
}

}  // namespace Robix::UI
