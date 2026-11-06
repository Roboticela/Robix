#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

static char* EmptyCopy() {
    char* text = static_cast<char*>(std::malloc(1));
    if (text != nullptr) text[0] = '\0';
    return text;
}

extern "C" char* rbx_network_send_text(const char* text) {
    const std::string body = text != nullptr ? text : "";
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return EmptyCopy();
    std::string reply;
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (listener != INVALID_SOCKET &&
        bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 &&
        listen(listener, 1) == 0) {
        sockaddr_in bound{};
        int boundLength = sizeof(bound);
        getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &boundLength);
        const unsigned short port = ntohs(bound.sin_port);
        std::thread server([&] {
            SOCKET accepted = accept(listener, nullptr, nullptr);
            if (accepted == INVALID_SOCKET) return;
            char buffer[4096];
            const int got = recv(accepted, buffer, static_cast<int>(sizeof buffer) - 1, 0);
            std::string inbound = got > 0 ? std::string(buffer, buffer + got) : std::string();
            while (!inbound.empty() && (inbound.back() == '\n' || inbound.back() == '\r')) inbound.pop_back();
            const std::string outbound = "Hello, " + inbound + "\n";
            send(accepted, outbound.data(), static_cast<int>(outbound.size()), 0);
            closesocket(accepted);
        });
        SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in destination{};
        destination.sin_family = AF_INET;
        destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        destination.sin_port = htons(port);
        if (client != INVALID_SOCKET &&
            connect(client, reinterpret_cast<sockaddr*>(&destination), sizeof(destination)) == 0) {
            const std::string outbound = body + "\n";
            send(client, outbound.data(), static_cast<int>(outbound.size()), 0);
            char buffer[4096];
            const int got = recv(client, buffer, static_cast<int>(sizeof buffer) - 1, 0);
            if (got > 0) reply.assign(buffer, buffer + got);
            while (!reply.empty() && (reply.back() == '\n' || reply.back() == '\r')) reply.pop_back();
        }
        if (client != INVALID_SOCKET) closesocket(client);
        server.join();
    }
    if (listener != INVALID_SOCKET) closesocket(listener);
    WSACleanup();
    char* copy = static_cast<char*>(std::malloc(reply.size() + 1));
    if (copy == nullptr) return EmptyCopy();
    std::memcpy(copy, reply.data(), reply.size());
    copy[reply.size()] = '\0';
    return copy;
}
