#include "NetworkRuntime.hpp"

#define SECURITY_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <schannel.h>
#include <security.h>
#include <wincrypt.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

extern "C" __declspec(dllexport) const char rbx_network_marker[] = "rbx-network-runtime-marker";

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

bool WriteFile(const fs::path& path, const std::string& text) {
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

std::string JoinEnv(const std::map<std::string, std::string>& values) {
    std::ostringstream out;
    for (const auto& [key, value] : values) out << key << "=" << value << "\n";
    return out.str();
}

struct Method {
    std::string name;
    std::string argument;
    std::string returns;
};

bool LoadMethods(const fs::path& path, std::vector<Method>& methods) {
    std::ifstream in(path);
    std::string name, argument, returns;
    while (in >> name >> argument >> returns) {
        methods.push_back({name, argument, returns});
    }
    return !methods.empty();
}

bool Startup() {
    WSADATA data{};
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

bool SendAll(SOCKET sock, const char* data, int size) {
    int sent = 0;
    while (sent < size) {
        const int n = send(sock, data + sent, size - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

bool RecvMore(SOCKET sock, std::vector<char>& buffer) {
    char temp[1024];
    const int n = recv(sock, temp, sizeof(temp), 0);
    if (n <= 0) return false;
    buffer.insert(buffer.end(), temp, temp + n);
    return true;
}

std::string RecvLine(SOCKET sock) {
    std::string line;
    char ch = 0;
    while (recv(sock, &ch, 1, 0) == 1) {
        if (ch == '\n') break;
        if (ch != '\r') line.push_back(ch);
    }
    return line;
}

SOCKET ListenOn(std::atomic<int>& port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) return INVALID_SOCKET;
    if (listen(sock, 4) != 0) return INVALID_SOCKET;
    int length = sizeof(address);
    getsockname(sock, reinterpret_cast<sockaddr*>(&address), &length);
    port.store(ntohs(address.sin_port));
    return sock;
}

SOCKET ConnectTo(int port) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<u_short>(port));
    if (connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        closesocket(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

const Method* FindMethod(const std::vector<Method>& methods, const std::string& name) {
    for (const auto& method : methods) {
        if (method.name == name) return &method;
    }
    return nullptr;
}

std::string Answer(const std::vector<Method>& methods, const std::string& line, bool& hang) {
    hang = line == "hang";
    if (hang) return "reply Int 42";
    std::istringstream in(line);
    std::string kind, name, type, value;
    in >> kind >> name >> type >> value;
    const Method* method = FindMethod(methods, name);
    if (kind != "call" || method == nullptr || method->returns == "Void") return "error unknown call";
    if (type != method->argument) return "error expected " + method->argument;
    try {
        (void)std::stoi(value);
    } catch (...) {
        return "error expected " + method->argument;
    }
    if (name == "GetStatus") return "reply Int 42";
    return "reply " + method->returns + " 0";
}

void ServePlain(std::atomic<int>& port, const std::vector<Method>& methods, int connections) {
    SOCKET listener = ListenOn(port);
    for (int i = 0; i < connections; ++i) {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;
        while (true) {
            const std::string line = RecvLine(client);
            if (line.empty()) break;
            bool hang = false;
            const std::string reply = Answer(methods, line, hang);
            if (hang) Sleep(800);
            const std::string wire = reply + "\n";
            if (!SendAll(client, wire.data(), static_cast<int>(wire.size()))) break;
            if (reply.rfind("reply ", 0) == 0) {
                const Method* event = FindMethod(methods, "BatteryLow");
                if (event != nullptr) {
                    const std::string message = "event BatteryLow " + event->argument + " 7\n";
                    if (!SendAll(client, message.data(), static_cast<int>(message.size()))) break;
                }
            }
        }
        closesocket(client);
    }
    closesocket(listener);
}

struct TlsSession {
    CredHandle credential{};
    CtxtHandle context{};
    bool haveCredential = false;
    bool haveContext = false;
    SecPkgContext_StreamSizes sizes{};

    ~TlsSession() {
        if (haveContext) DeleteSecurityContext(&context);
        if (haveCredential) FreeCredentialsHandle(&credential);
    }
};

bool SendToken(SOCKET sock, const SecBuffer& buffer) {
    if (buffer.cbBuffer == 0 || buffer.pvBuffer == nullptr) return true;
    const bool ok = SendAll(sock, static_cast<char*>(buffer.pvBuffer), static_cast<int>(buffer.cbBuffer));
    FreeContextBuffer(buffer.pvBuffer);
    return ok;
}

bool Handshake(SOCKET sock, TlsSession& session, bool server, const char* target) {
    DWORD receiveTimeout = 5000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&receiveTimeout), sizeof(receiveTimeout));
    std::vector<char> incoming;
    DWORD attributes = 0;
    if (server && !RecvMore(sock, incoming)) return false;
    const DWORD request = ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY | ISC_REQ_STREAM |
                          ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT;
    while (true) {
        SecBuffer input[2]{};
        input[0].BufferType = SECBUFFER_TOKEN;
        input[0].pvBuffer = incoming.empty() ? nullptr : incoming.data();
        input[0].cbBuffer = static_cast<unsigned long>(incoming.size());
        input[1].BufferType = SECBUFFER_EMPTY;
        SecBufferDesc inputDesc{SECBUFFER_VERSION, 2, input};
        SecBuffer output{};
        output.BufferType = SECBUFFER_TOKEN;
        SecBufferDesc outputDesc{SECBUFFER_VERSION, 1, &output};
        SECURITY_STATUS status;
        if (server) {
            status = AcceptSecurityContext(&session.credential, session.haveContext ? &session.context : nullptr,
                                           incoming.empty() ? nullptr : &inputDesc, request, 0, &session.context,
                                           &outputDesc, &attributes, nullptr);
        } else {
            status = InitializeSecurityContextA(&session.credential, session.haveContext ? &session.context : nullptr,
                                                const_cast<char*>(target), request, 0, 0,
                                                incoming.empty() ? nullptr : &inputDesc, 0, &session.context,
                                                &outputDesc, &attributes, nullptr);
        }
        if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED || status == SEC_E_INCOMPLETE_MESSAGE) session.haveContext = true;
        if (!SendToken(sock, output)) return false;
        std::vector<char> extra;
        SecBuffer* extraBuffer = &input[1];
        if ((status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED) && extraBuffer->BufferType == SECBUFFER_EXTRA &&
            extraBuffer->pvBuffer != nullptr && extraBuffer->cbBuffer > 0) {
            const char* bytes = static_cast<char*>(extraBuffer->pvBuffer);
            extra.assign(bytes, bytes + extraBuffer->cbBuffer);
        }
        if (status == SEC_E_OK || status == SEC_I_CONTINUE_NEEDED) incoming = std::move(extra);
        if (status == SEC_E_OK) {
            QueryContextAttributes(&session.context, SECPKG_ATTR_STREAM_SIZES, &session.sizes);
            return true;
        }
        if (status == SEC_I_CONTINUE_NEEDED && !incoming.empty()) continue;
        if (status == SEC_E_INCOMPLETE_MESSAGE || status == SEC_I_CONTINUE_NEEDED) {
            if (!RecvMore(sock, incoming)) return false;
            continue;
        }
        std::cerr << "tls: handshake status " << std::hex << status << std::dec << "\n";
        return false;
    }
}

bool EncryptSend(SOCKET sock, TlsSession& session, const std::string& message) {
    std::vector<char> buffer(session.sizes.cbHeader + message.size() + session.sizes.cbTrailer);
    std::memcpy(buffer.data() + session.sizes.cbHeader, message.data(), message.size());
    SecBuffer parts[4]{};
    parts[0] = {session.sizes.cbHeader, SECBUFFER_STREAM_HEADER, buffer.data()};
    parts[1] = {static_cast<unsigned long>(message.size()), SECBUFFER_DATA, buffer.data() + session.sizes.cbHeader};
    parts[2] = {session.sizes.cbTrailer, SECBUFFER_STREAM_TRAILER, buffer.data() + session.sizes.cbHeader + message.size()};
    parts[3] = {0, SECBUFFER_EMPTY, nullptr};
    SecBufferDesc desc{SECBUFFER_VERSION, 4, parts};
    const SECURITY_STATUS encrypted = EncryptMessage(&session.context, 0, &desc, 0);
    if (encrypted != SEC_E_OK) {
        std::cerr << "tls: encrypt " << std::hex << encrypted << std::dec << "\n";
        return false;
    }
    const int size = static_cast<int>(parts[0].cbBuffer + parts[1].cbBuffer + parts[2].cbBuffer);
    return SendAll(sock, buffer.data(), size);
}

bool DecryptRecv(SOCKET sock, TlsSession& session, std::string& message) {
    std::vector<char> incoming;
    SECURITY_STATUS status = SEC_E_INCOMPLETE_MESSAGE;
    while (incoming.size() < 64 * 1024) {
        if (!RecvMore(sock, incoming)) return false;
        SecBuffer parts[4]{};
        parts[0].BufferType = SECBUFFER_DATA;
        parts[0].pvBuffer = incoming.data();
        parts[0].cbBuffer = static_cast<unsigned long>(incoming.size());
        parts[1].BufferType = SECBUFFER_EMPTY;
        parts[2].BufferType = SECBUFFER_EMPTY;
        parts[3].BufferType = SECBUFFER_EMPTY;
        SecBufferDesc desc{SECBUFFER_VERSION, 4, parts};
        status = DecryptMessage(&session.context, &desc, 0, nullptr);
        if (status == SEC_E_INCOMPLETE_MESSAGE) continue;
        if (status != SEC_E_OK) {
            std::cerr << "tls: decrypt " << std::hex << status << std::dec << "\n";
            return false;
        }
        for (const auto& part : parts) {
            if (part.BufferType == SECBUFFER_DATA && part.pvBuffer != nullptr) {
                message.assign(static_cast<char*>(part.pvBuffer), part.cbBuffer);
                return true;
            }
        }
        return false;
    }
    return false;
}

bool LoadServerCredential(const fs::path& pfx, const std::string& password, TlsSession& session, PCCERT_CONTEXT& cert, HCERTSTORE& store) {
    const std::string bytes = ReadFile(pfx);
    if (bytes.empty()) return false;
    CRYPT_DATA_BLOB blob{};
    blob.cbData = static_cast<DWORD>(bytes.size());
    blob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data()));
    store = PFXImportCertStore(&blob, std::wstring(password.begin(), password.end()).c_str(), CRYPT_USER_KEYSET | PKCS12_INCLUDE_EXTENDED_PROPERTIES);
    if (store == nullptr) return false;
    cert = CertFindCertificateInStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HAS_PRIVATE_KEY, nullptr, nullptr);
    if (cert == nullptr) return false;
    SCHANNEL_CRED credential{};
    credential.dwVersion = SCHANNEL_CRED_VERSION;
    credential.cCreds = 1;
    credential.paCred = &cert;
    credential.grbitEnabledProtocols = SP_PROT_TLS1_2_SERVER;
    const SECURITY_STATUS status = AcquireCredentialsHandleA(nullptr, const_cast<char*>(UNISP_NAME_A), SECPKG_CRED_INBOUND,
                                                             nullptr, &credential, nullptr, nullptr, &session.credential, nullptr);
    if (status != SEC_E_OK) std::cerr << "tls: server credential " << std::hex << status << std::dec << "\n";
    session.haveCredential = status == SEC_E_OK;
    return session.haveCredential;
}

bool LoadClientCredential(TlsSession& session) {
    SCHANNEL_CRED credential{};
    credential.dwVersion = SCHANNEL_CRED_VERSION;
    credential.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT;
    credential.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_SERVERNAME_CHECK;
    const SECURITY_STATUS status = AcquireCredentialsHandleA(nullptr, const_cast<char*>(UNISP_NAME_A), SECPKG_CRED_OUTBOUND,
                                                             nullptr, &credential, nullptr, nullptr, &session.credential, nullptr);
    if (status != SEC_E_OK) std::cerr << "tls: client credential " << std::hex << status << std::dec << "\n";
    session.haveCredential = status == SEC_E_OK;
    return session.haveCredential;
}

std::string CertificateSubject(PCCERT_CONTEXT cert) {
    DWORD size = CertGetNameStringA(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr, 0);
    std::string name(size == 0 ? 0 : size - 1, '\0');
    if (size > 1) CertGetNameStringA(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name.data(), size);
    return name;
}

} // namespace

extern "C" __declspec(dllexport) int rbx_network_run(const char* serverEnvPath, const char* methodsPath) {
    std::vector<Method> methods;
    if (!LoadMethods(methodsPath, methods)) {
        std::cerr << "network: the contract method list is empty\n";
        return 1;
    }
    const auto server = ReadEnv(serverEnvPath);
    if (!Startup()) {
        std::cerr << "network: Winsock did not start\n";
        return 1;
    }
    const int timeoutMs = 200;
    std::atomic<int> port{0};
    std::thread plain([&] { ServePlain(port, methods, 2); });
    while (port.load() == 0) Sleep(1);
    SOCKET call = ConnectTo(port.load());
    if (call == INVALID_SOCKET) {
        plain.join();
        std::cerr << "network: could not connect to 127.0.0.1\n";
        return 1;
    }
    const std::string request = "call GetStatus Int 7\n";
    SendAll(call, request.data(), static_cast<int>(request.size()));
    const std::string reply = RecvLine(call);
    const std::string event = RecvLine(call);
    const std::string bad = "call GetStatus String fast\n";
    SendAll(call, bad.data(), static_cast<int>(bad.size()));
    const std::string rejected = RecvLine(call);
    closesocket(call);
    std::cout << "call " << reply << "\n";
    std::cout << "event " << event << "\n";
    std::cout << "type " << rejected << "\n";
    if (reply != "reply Int 42" || event != "event BatteryLow Int 7" || rejected != "error expected Int") {
        plain.join();
        WSACleanup();
        return 1;
    }

    SOCKET slow = ConnectTo(port.load());
    DWORD wait = static_cast<DWORD>(timeoutMs);
    setsockopt(slow, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&wait), sizeof(wait));
    const std::string hang = "hang\n";
    const auto started = GetTickCount64();
    SendAll(slow, hang.data(), static_cast<int>(hang.size()));
    char probe = 0;
    const int received = recv(slow, &probe, 1, 0);
    const auto elapsed = GetTickCount64() - started;
    const int socketError = WSAGetLastError();
    closesocket(slow);
    plain.join();
    if (received != SOCKET_ERROR || socketError != WSAETIMEDOUT || elapsed >= 700) {
        std::cerr << "timeout: expected a " << timeoutMs << " ms receive timeout, elapsed " << elapsed
                  << " error " << socketError << "\n";
        WSACleanup();
        return 1;
    }
    std::cout << "timeout: " << elapsed << " ms on 127.0.0.1\n";

    const auto certPath = server.find("CERT_PFX");
    const auto certPassword = server.find("CERT_PASSWORD");
    if (certPath == server.end() || certPassword == server.end()) {
        std::cerr << "tls: .env.server has no CERT_PFX or CERT_PASSWORD\n";
        WSACleanup();
        return 1;
    }
    PCCERT_CONTEXT cert = nullptr;
    HCERTSTORE store = nullptr;
    std::atomic<int> tlsPort{0};
    std::atomic<bool> serverReady{false};
    std::string tlsServerError;
    std::thread tlsServer([&] {
        TlsSession session;
        if (!LoadServerCredential(certPath->second, certPassword->second, session, cert, store)) {
            tlsServerError = "credential";
            serverReady = true;
            return;
        }
        SOCKET listener = ListenOn(tlsPort);
        serverReady = true;
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET || !Handshake(client, session, true, nullptr)) {
            tlsServerError = "handshake";
            if (client != INVALID_SOCKET) closesocket(client);
            closesocket(listener);
            return;
        }
        std::string requestLine;
        if (!DecryptRecv(client, session, requestLine)) tlsServerError = "read";
        else {
            bool hang = false;
            const std::string body = Answer(methods, Trim(requestLine), hang) + "\n";
            if (!EncryptSend(client, session, body)) tlsServerError = "write";
        }
        closesocket(client);
        closesocket(listener);
    });
    while (!serverReady.load()) Sleep(1);
    if (!tlsServerError.empty() && tlsPort.load() == 0) {
        tlsServer.join();
        std::cerr << "tls: server " << tlsServerError << "\n";
        if (cert) CertFreeCertificateContext(cert);
        if (store) CertCloseStore(store, 0);
        WSACleanup();
        return 1;
    }
    while (tlsPort.load() == 0 && tlsServerError.empty()) Sleep(1);
    SOCKET secure = ConnectTo(tlsPort.load());
    TlsSession clientSession;
    std::string tlsReply;
    std::string subject;
    const bool handshook = secure != INVALID_SOCKET && LoadClientCredential(clientSession) &&
                           Handshake(secure, clientSession, false, "robix-stagek");
    const bool tlsOk = handshook && EncryptSend(secure, clientSession, "call GetStatus Int 7\n") &&
                       DecryptRecv(secure, clientSession, tlsReply);
    if (tlsOk) {
        PCCERT_CONTEXT remote = nullptr;
        if (QueryContextAttributes(&clientSession.context, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &remote) == SEC_E_OK && remote != nullptr) {
            subject = CertificateSubject(remote);
            CertFreeCertificateContext(remote);
        }
    }
    if (secure != INVALID_SOCKET) closesocket(secure);
    tlsServer.join();
    if (cert) CertFreeCertificateContext(cert);
    if (store) CertCloseStore(store, 0);
    if (!tlsOk || Trim(tlsReply) != "reply Int 42") {
        std::cerr << "tls: handshake or typed call failed";
        if (!tlsServerError.empty()) std::cerr << " (" << tlsServerError << ")";
        std::cerr << "\n";
        WSACleanup();
        return 1;
    }
    std::cout << "tls: schannel handshake subject " << subject << "\n";
    std::cout << "tls-call " << Trim(tlsReply) << "\n";

    const Method* move = FindMethod(methods, "Move");
    if (move == nullptr || move->returns != "Void" || move->argument != "Int,Int,Int,Int") {
        std::cerr << "realtime: Move(Int, Int, Int, Int) -> Void is not in the contract\n";
        WSACleanup();
        return 1;
    }
    struct World {
        int sequence[2] = {-1, -1};
        int x[2] = {0, 0};
        int y[2] = {0, 0};
        int applied = 0;
        int stale = 0;
        int rejected = 0;
    } world;
    std::atomic<int> udpPort{0};
    std::thread host([&] {
        SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        if (bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            udpPort = -1;
            closesocket(sock);
            return;
        }
        int length = sizeof(address);
        getsockname(sock, reinterpret_cast<sockaddr*>(&address), &length);
        DWORD wait = 500;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&wait), sizeof(wait));
        udpPort = ntohs(address.sin_port);
        while (true) {
            char buffer[256];
            sockaddr_in from{};
            int fromLength = sizeof(from);
            const int n = recvfrom(sock, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);
            if (n == SOCKET_ERROR) break;
            std::string line(buffer, buffer + n);
            if (Trim(line) == "end") break;
            std::istringstream in(line);
            std::string kind, playerText, sequenceText, xText, yText;
            in >> kind >> playerText >> sequenceText >> xText >> yText;
            int player = 0;
            int sequence = 0;
            int x = 0;
            int y = 0;
            try {
                player = std::stoi(playerText);
                sequence = std::stoi(sequenceText);
                x = std::stoi(xText);
                y = std::stoi(yText);
            } catch (...) {
                ++world.rejected;
                continue;
            }
            if (kind != "state" || player < 1 || player > 2) {
                ++world.rejected;
                continue;
            }
            const int index = player - 1;
            if (sequence <= world.sequence[index]) {
                ++world.stale;
                continue;
            }
            world.sequence[index] = sequence;
            world.x[index] = x;
            world.y[index] = y;
            ++world.applied;
        }
        closesocket(sock);
    });
    while (udpPort.load() == 0) Sleep(1);
    if (udpPort.load() < 0) {
        host.join();
        std::cerr << "realtime: UDP socket did not bind\n";
        WSACleanup();
        return 1;
    }
    SOCKET client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dest.sin_port = htons(static_cast<u_short>(udpPort.load()));
    auto sendState = [&](const std::string& text) {
        sendto(client, text.data(), static_cast<int>(text.size()), 0, reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
    };
    const auto tickStart = GetTickCount64();
    std::thread second([&] {
        SOCKET other = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        for (int sequence = 1; sequence <= 20; ++sequence) {
            const std::string text = "state 2 " + std::to_string(sequence) + " " + std::to_string(100 + sequence) + " " + std::to_string(sequence);
            sendto(other, text.data(), static_cast<int>(text.size()), 0, reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
            Sleep(1);
        }
        closesocket(other);
    });
    int sent = 0;
    for (int sequence = 1; sequence <= 20; ++sequence) {
        if (sequence == 7) continue;
        sendState("state 1 " + std::to_string(sequence) + " " + std::to_string(sequence * 3) + " " + std::to_string(sequence * 2));
        ++sent;
        Sleep(1);
    }
    second.join();
    sent += 20;
    Sleep(30);
    sendState("state 1 2 99 99");
    sendState("state 1 late no 1");
    Sleep(30);
    sendState("end");
    const auto tickElapsed = GetTickCount64() - tickStart;
    host.join();
    closesocket(client);
    WSACleanup();
    if (world.x[0] != 60 || world.y[0] != 40 || world.sequence[0] != 20 || world.x[1] != 120 || world.y[1] != 20 ||
        world.stale < 1 || world.rejected < 1) {
        std::cerr << "realtime: player1 " << world.x[0] << "," << world.y[0] << " seq " << world.sequence[0]
                  << " player2 " << world.x[1] << "," << world.y[1]
                  << " applied " << world.applied << " stale " << world.stale << " rejected " << world.rejected << "\n";
        return 1;
    }
    std::cout << "realtime: udp 127.0.0.1 sent " << sent << " applied " << world.applied
              << " elapsed " << tickElapsed << " ms"
              << " player1 " << world.x[0] << "," << world.y[0]
              << " player2 " << world.x[1] << "," << world.y[1]
              << " stale " << world.stale << " rejected " << world.rejected << "\n";
    return 0;
}
