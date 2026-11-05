#pragma once

#include "../../Compiler/Semantic/Diagnostics.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Robix::StdLib {

// ============================================================================
// 1. FORMAL THREAT MODELING & CAPABILITY PERMISSION SANDBOX (SEC7001)
// ============================================================================
enum class TrustZone : std::uint8_t {
    SafeApplication,
    UnsafeSystemBlock,
    ForeignFFIBoundary,
    ThirdPartyPackage,
    ExternalNetwork,
    HardwarePeripheral
};

class CapabilitySandbox {
public:
    explicit CapabilitySandbox(Frontend::DiagnosticBag& diagnostics,
                               TrustZone zone = TrustZone::SafeApplication) noexcept
        : m_diagnostics(diagnostics), m_zone(zone) {}

    void GrantCapability(std::string capability);
    void RevokeCapability(const std::string& capability);

    [[nodiscard]] bool HasCapability(const std::string& capability) const noexcept;

    // Enforces `requires ...` capability check; reports SEC7001 on violation
    bool EnforceCapability(const std::string& requiredCapability,
                           std::string_view callerFunction,
                           Frontend::SourceLocation loc = {});

    [[nodiscard]] TrustZone CurrentTrustZone() const noexcept { return m_zone; }

private:
    Frontend::DiagnosticBag&        m_diagnostics;
    TrustZone                       m_zone;
    std::unordered_set<std::string> m_grantedCapabilities;
};

// ============================================================================
// 2. ZERO-ON-DROP SECRET<T> STORAGE (VirtualLock / mlock + Constant-Time Wipe)
// ============================================================================
class SecretBuffer {
public:
    explicit SecretBuffer(std::string_view secretData, bool* outZeroizedFlag = nullptr);
    ~SecretBuffer();

    SecretBuffer(const SecretBuffer&) = delete;
    SecretBuffer& operator=(const SecretBuffer&) = delete;

    [[nodiscard]] std::string_view RevealScoped() const noexcept {
        return std::string_view(reinterpret_cast<const char*>(m_bytes.data()), m_bytes.size());
    }

    [[nodiscard]] std::size_t SizeBytes() const noexcept { return m_bytes.size(); }
    [[nodiscard]] bool IsPageLockedInRAM() const noexcept { return m_isPageLocked; }

    void ZeroizeNow() noexcept;

    // Constant-time byte comparison immune to timing side-channels
    [[nodiscard]] static bool ConstantTimeEquals(std::string_view a, std::string_view b) noexcept;

private:
    std::vector<std::uint8_t> m_bytes;
    bool                      m_isPageLocked = false;
    bool                      m_zeroized     = false;
    bool*                     m_externalZeroizedTracker = nullptr;
};

// ============================================================================
// 3. CRYPTOGRAPHY (RFC 6234 SHA-256, HMAC-SHA256, CSPRNG, AUTHENTICATED CIPHER)
// ============================================================================
class CryptoEngine {
public:
    [[nodiscard]] static std::array<std::uint8_t, 32> ComputeSHA256Bytes(std::string_view data) noexcept;
    [[nodiscard]] static std::string                  ComputeSHA256Hex(std::string_view data);
    [[nodiscard]] static std::string                  ComputeHMACSHA256Hex(std::string_view key,
                                                                           std::string_view message);
    [[nodiscard]] static std::vector<std::uint8_t>    GenerateCSPRNGBytes(std::size_t count);

    // Authenticated encryption (stream keystream + 256-bit HMAC-SHA256 authentication tag)
    [[nodiscard]] static std::vector<std::uint8_t>    SealAuthenticated(std::string_view key,
                                                                        std::string_view nonce,
                                                                        std::string_view plaintext);
    [[nodiscard]] static std::optional<std::string>   OpenAuthenticated(std::string_view key,
                                                                        std::string_view nonce,
                                                                        const std::vector<std::uint8_t>& sealedPacket);
};

// ============================================================================
// 4. SUPPLY-CHAIN SECURITY, PACKAGE SIGNATURE & VULNERABILITY AUDITOR
// ============================================================================
struct PackageDependencyDescriptor {
    std::string packageName;
    std::string version;
    std::string sha256DigestHex;
    std::string signatureHex;
};

struct SecurityAuditResult {
    bool                     passed = true;
    std::size_t              packagesAudited = 0;
    std::vector<std::string> vulnerabilitiesFound;
    std::string              generatedSBOMJson;
};

class SupplyChainAuditor {
public:
    static SecurityAuditResult AuditDependencies(
        const std::vector<PackageDependencyDescriptor>& dependencies,
        std::string_view trustedSigningKey,
        const std::unordered_map<std::string, std::string>& knownVulnerablePkgVersions,
        Frontend::DiagnosticBag& diagnostics);
};

// ============================================================================
// 5. OS-INDEPENDENT NETWORKING (HTTP, WebSocket, MQTT Pub/Sub)
// ============================================================================
struct HTTPRequest {
    std::string method = "GET";
    std::string path   = "/";
    std::string body;
    std::map<std::string, std::string> headers;
};

struct HTTPResponse {
    int         statusCode = 200;
    std::string body;
    std::map<std::string, std::string> headers;
};

class HTTPEndpointRouter {
public:
    using HandlerFn = std::function<HTTPResponse(const HTTPRequest&)>;

    void RegisterRoute(const std::string& method, const std::string& path, HandlerFn handler);
    [[nodiscard]] HTTPResponse Dispatch(const HTTPRequest& req) const;

private:
    std::map<std::string, HandlerFn> m_routes;
};

class MQTTBroker {
public:
    using SubscriberFn = std::function<void(std::string_view topic, std::string_view payload)>;

    void Subscribe(const std::string& topic, SubscriberFn callback);
    std::size_t Publish(const std::string& topic, std::string_view payload) const;

private:
    std::unordered_map<std::string, std::vector<SubscriberFn>> m_subscribers;
};

} // namespace Robix::StdLib

