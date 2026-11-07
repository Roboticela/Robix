#include "Window.hpp"
#include "WebFoundation.hpp"
#include "../Foundation.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>

namespace Robix::UI::Web {
namespace {

std::string Escape(const std::string& text) {
    std::string out;
    for (char ch : text) {
        if (ch == '&') out += "&amp;";
        else if (ch == '<') out += "&lt;";
        else if (ch == '>') out += "&gt;";
        else if (ch == '"') out += "&quot;";
        else out.push_back(ch);
    }
    return out;
}

std::string Hex(std::uint32_t color) {
    std::ostringstream text;
    text << std::hex;
    text.width(6);
    text.fill('0');
    text << (color & 0xFFFFFFu);
    return text.str();
}

} // namespace

std::string RenderWindow(const std::string& name, const std::string& title, int width, int height,
                         const std::string& theme, std::uint32_t background, std::uint32_t foreground,
                         const std::string& body, bool live, const std::string& fontPrefix) {
    const std::string safeName = Escape(name.empty() ? "Window" : name);
    const std::string safeTitle = Escape(title.empty() ? safeName : title);
    const std::string themeName = (theme == "dark" || theme == "Dark") ? "dark" : "light";
    std::ostringstream out;
    out << "<!DOCTYPE html>\n<html lang=\"en\" data-theme=\"" << themeName << "\">\n<head>\n<meta charset=\"utf-8\">\n"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        << "<title>" << safeTitle << "</title>\n<style>\n"
        << Robix::UI::FoundationCss(fontPrefix);
    const bool screen = body.find("robo-screen") != std::string::npos;
    if (screen) {
        out << "html,body{margin:0;height:100%;overflow:hidden;background:var(--bg-primary);border:0;outline:none;box-shadow:none}\n"
            << "html[data-theme=\"dark\"]{color-scheme:dark}\n"
            << "html[data-theme=\"light\"]{color-scheme:light}\n"
            << "html:focus,html:focus-visible,body:focus,body:focus-visible{outline:none;box-shadow:none}\n"
            << "body{color:var(--text-primary);font-family:var(--font-sans,\"Inter\",system-ui,sans-serif)}\n"
            << ".robo-screen{position:relative;min-height:100%;box-sizing:border-box;z-index:1;background:var(--bg-primary);border:0;outline:none;box-shadow:none}\n"
            << ".robo-page{height:100%;overflow:auto;scrollbar-width:none;background:var(--bg-primary);color:var(--text-primary);border:0;outline:none;box-shadow:none}\n"
            << ".robo-screen:focus,.robo-screen:focus-visible,.robo-page:focus,.robo-page:focus-visible{outline:none;box-shadow:none}\n"
            << ".robo-page::-webkit-scrollbar{width:0;height:0}\n"
            << ".robo-window{position:absolute;z-index:1;box-sizing:border-box;color:var(--text-primary);background:var(--bg-secondary);border:0;outline:none;overflow:auto}\n"
            << ".robo-screen{background:var(--bg-primary);color:var(--text-primary)}\n"
            << ".robo-window:not(.is-open){display:none}\n"
            << ".robo-page .robo-window,.robo-page .robo-window:not(.is-open){position:relative;display:flex;width:100%;height:auto;left:auto;top:auto;max-height:none;z-index:auto}\n"
            << ".robo-board{box-sizing:border-box;min-width:0;border:0;outline:none}\n"
            << ".robo-board:focus,.robo-board:focus-visible{outline:none}\n"
            << "p{margin:0;}\n"
            << "button{border:0;background:var(--brand-primary);color:var(--text-on-brand);font-family:var(--font-sans-medium,\"Inter\",system-ui,sans-serif);font-size:var(--body-size);font-weight:500;}\n";
    } else {
        out << "body{margin:0;background:var(--bg-secondary);color:var(--text-primary);font-family:var(--font-sans,\"Inter\",system-ui,sans-serif);font-size:var(--body-size);}\n"
        << "main{box-sizing:border-box;width:100%;max-width:" << width << "px;min-height:" << height
        << "px;padding:var(--space-4);background:#" << Hex(background) << ";color:#" << Hex(foreground) << ";}\n"
        << "h1{margin:0;font-size:var(--title-size);font-weight:600;}\n"
        << "p{margin:12px 0 0;}\n"
        << "button{margin:12px 12px 0 0;padding:8px 12px;border:0;border-radius:var(--control);background:var(--brand-primary);color:var(--text-on-brand);font:var(--body-size) sans-serif;}\n";
    }
    out << "</style>\n</head>\n<body>\n";
    if (screen) out << body;
    else out << "<main id=\"" << safeName << "\" data-component=\"Window\">\n<h1>" << safeTitle << "</h1>\n" << body << "\n</main>\n";
    if (live) {
        out << "<script>\nlet stamp='';\nsetInterval(async function(){\n"
            << "try{const next=await fetch('/robix-dev',{cache:'no-store'}).then(function(r){return r.text();});\n"
            << "if(stamp && next!==stamp) location.reload();\nstamp=next;}catch(e){}\n},400);\n</script>\n";
    }
    out << "</body>\n</html>\n";
    return out.str();
}

std::string FontPrefixFor(const std::string& htmlPath) {
    namespace fs = std::filesystem;
    fs::path fonts;
    fs::path cursor = fs::current_path();
    for (int depth = 0; depth < 8 && !cursor.empty(); ++depth) {
        if (fs::is_directory(cursor / "Assets" / "Fonts")) {
            fonts = cursor / "Assets" / "Fonts";
            break;
        }
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor) break;
        cursor = cursor.parent_path();
    }
    if (fonts.empty() || htmlPath.empty()) return "Assets/Fonts";
    std::error_code ec;
    const fs::path relative = fs::relative(fonts, fs::path(htmlPath).parent_path(), ec);
    if (ec) return "Assets/Fonts";
    return relative.generic_string();
}

bool WriteWindow(const std::string& path, const std::string& name, const std::string& title,
                 int width, int height, const std::string& theme, std::uint32_t background, std::uint32_t foreground,
                 const std::string& body, bool live) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << RenderWindow(name, title, width, height, theme, background, foreground, body, live, FontPrefixFor(path));
    return static_cast<bool>(out);
}

void SendResponse(SOCKET client, const std::string& type, const std::string& body) {
    std::ostringstream head;
    head << "HTTP/1.1 200 OK\r\nContent-Type: " << type << "\r\nCache-Control: no-store\r\nContent-Length: "
         << body.size() << "\r\nConnection: close\r\n\r\n";
    const std::string header = head.str();
    send(client, header.data(), static_cast<int>(header.size()), 0);
    send(client, body.data(), static_cast<int>(body.size()), 0);
}

bool ServeDev(void (*provider)(DevPage& page, void* user), void* user) {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    int port = 4173;
    bool bound = false;
    for (; port < 4183; ++port) {
        address.sin_port = htons(static_cast<unsigned short>(port));
        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
            bound = true;
            break;
        }
    }
    if (!bound || listen(listener, 8) != 0) {
        if (listener != INVALID_SOCKET) closesocket(listener);
        WSACleanup();
        return false;
    }
    const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/";
    std::cout << "web: " << url << "\n";
    std::cout << "web: development watching\n";
    std::cout.flush();
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    DevPage page;
    provider(page, user);
    while (true) {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        char buffer[2048];
        const int got = recv(client, buffer, static_cast<int>(sizeof buffer) - 1, 0);
        const std::string request = got > 0 ? std::string(buffer, buffer + got) : std::string();
        provider(page, user);
        const auto fontAt = request.find("GET /Assets/Fonts/");
        if (request.find("GET /robix-dev") != std::string::npos) {
            SendResponse(client, "text/plain; charset=utf-8", std::to_string(page.generation));
        } else if (fontAt != std::string::npos) {
            const auto pathStart = fontAt + 5;
            const auto pathEnd = request.find(' ', pathStart);
            const std::string url = pathEnd == std::string::npos ? std::string() : request.substr(pathStart, pathEnd - pathStart);
            namespace fs = std::filesystem;
            fs::path root;
            fs::path cursor = fs::current_path();
            for (int depth = 0; depth < 8 && root.empty() && !cursor.empty(); ++depth) {
                if (fs::is_directory(cursor / "Assets" / "Fonts")) root = cursor;
                else cursor = cursor.parent_path();
            }
            const fs::path file = root / fs::path(url);
            const std::string bytes = [&] {
                if (url.find("..") != std::string::npos || !fs::is_regular_file(file)) return std::string();
                std::ifstream in(file, std::ios::binary);
                std::ostringstream data;
                data << in.rdbuf();
                return data.str();
            }();
            if (bytes.empty()) SendResponse(client, "text/plain", "missing font");
            else SendResponse(client, "font/ttf", bytes);
        } else {
            SendResponse(client, "text/html; charset=utf-8", page.html);
        }
        closesocket(client);
    }
    return true;
}

} // namespace Robix::UI::Web
