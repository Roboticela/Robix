#include "SecurityAndNetwork.hpp"

#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Robix::StdLib {

// ============================================================================
// 1. CAPABILITY SANDBOX IMPLEMENTATION
// ============================================================================

void CapabilitySandbox::GrantCapability(std::string capability) {
    m_grantedCapabilities.insert(std::move(capability));
}

void CapabilitySandbox::RevokeCapability(const std::string& capability) {
    m_grantedCapabilities.erase(capability);
}

bool CapabilitySandbox::HasCapability(const std::string& capability) const noexcept {
    return m_grantedCapabilities.find(capability) != m_grantedCapabilities.end();
}

bool CapabilitySandbox::EnforceCapability(const std::string& requiredCapability,
                                          std::string_view callerFunction,
                                          Frontend::SourceLocation loc) {
    if (HasCapability(requiredCapability)) {
        return true;
    }

    m_diagnostics.ReportError(
        "SEC7001",
        "ROBIX SECURITY VIOLATION SEC7001: Function '" + std::string(callerFunction) +
            "' attempted an operation requiring capability '" + requiredCapability +
            "', which is not granted in the active sandbox profile.",
        loc,
        "Declare 'requires " + requiredCapability + "' on the function and grant '" +
            requiredCapability + "' in the package/project Permissions manifest.");
    return false;
}

// ============================================================================
// 2. ZERO-ON-DROP SECRET BUFFER IMPLEMENTATION
// ============================================================================

SecretBuffer::SecretBuffer(std::string_view secretData, bool* outZeroizedFlag)
    : m_bytes(secretData.begin(), secretData.end())
    , m_externalZeroizedTracker(outZeroizedFlag) {
    if (m_externalZeroizedTracker) {
        *m_externalZeroizedTracker = false;
    }
    if (!m_bytes.empty()) {
#ifdef _WIN32
        m_isPageLocked = (VirtualLock(m_bytes.data(), m_bytes.size()) != 0);
#else
        m_isPageLocked = true;
#endif
    }
}

SecretBuffer::~SecretBuffer() {
    ZeroizeNow();
}

void SecretBuffer::ZeroizeNow() noexcept {
    if (!m_zeroized) {
        volatile std::uint8_t* p = m_bytes.data();
        for (std::size_t i = 0; i < m_bytes.size(); ++i) {
            p[i] = 0;
        }
#ifdef _WIN32
        if (m_isPageLocked && !m_bytes.empty()) {
            (void)VirtualUnlock(m_bytes.data(), m_bytes.size());
        }
#endif
        m_zeroized = true;
        if (m_externalZeroizedTracker) {
            *m_externalZeroizedTracker = true;
        }
    }
}

bool SecretBuffer::ConstantTimeEquals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    volatile unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

// ============================================================================
// 3. CRYPTO ENGINE (RFC 6234 SHA-256, HMAC-SHA256, CSPRNG, AUTH CIPHER)
// ============================================================================

namespace {

constexpr std::uint32_t RoRight(std::uint32_t x, unsigned int n) noexcept {
    return (x >> n) | (x << (32U - n));
}

constexpr std::uint32_t K_SHA256[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
    0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
    0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
    0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
    0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

std::string BytesToHex(const std::uint8_t* data, std::size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    return oss.str();
}

} // namespace

std::array<std::uint8_t, 32> CryptoEngine::ComputeSHA256Bytes(std::string_view data) noexcept {
    std::uint32_t h[8] = {
        0x6a09e667UL, 0xbb67ae85UL, 0x3c6ef372UL, 0xa54ff53aUL,
        0x510e527fUL, 0x9b05688cUL, 0x1f83d9abUL, 0x5be0cd19UL
    };

    const std::uint64_t bitLen = static_cast<std::uint64_t>(data.size()) * 8ULL;
    std::vector<std::uint8_t> padded(data.begin(), data.end());
    padded.push_back(0x80U);
    while ((padded.size() % 64) != 56) {
        padded.push_back(0x00U);
    }
    for (int i = 7; i >= 0; --i) {
        padded.push_back(static_cast<std::uint8_t>((bitLen >> (i * 8)) & 0xFFU));
    }

    for (std::size_t chunkOffset = 0; chunkOffset < padded.size(); chunkOffset += 64) {
        std::uint32_t w[64]{};
        for (int i = 0; i < 16; ++i) {
            const std::size_t idx = chunkOffset + static_cast<std::size_t>(i * 4);
            w[i] = (static_cast<std::uint32_t>(padded[idx])     << 24U) |
                   (static_cast<std::uint32_t>(padded[idx + 1]) << 16U) |
                   (static_cast<std::uint32_t>(padded[idx + 2]) << 8U)  |
                   (static_cast<std::uint32_t>(padded[idx + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = RoRight(w[i - 15], 7) ^ RoRight(w[i - 15], 18) ^ (w[i - 15] >> 3U);
            const std::uint32_t s1 = RoRight(w[i - 2], 17) ^ RoRight(w[i - 2], 19) ^ (w[i - 2] >> 10U);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i) {
            const std::uint32_t S1    = RoRight(e, 6) ^ RoRight(e, 11) ^ RoRight(e, 25);
            const std::uint32_t ch    = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = hh + S1 + ch + K_SHA256[i] + w[i];
            const std::uint32_t S0    = RoRight(a, 2) ^ RoRight(a, 13) ^ RoRight(a, 22);
            const std::uint32_t maj   = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = S0 + maj;

            hh = g;
            g  = f;
            f  = e;
            e  = d + temp1;
            d  = c;
            c  = b;
            b  = a;
            a  = temp1 + temp2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    std::array<std::uint8_t, 32> digest{};
    for (int i = 0; i < 8; ++i) {
        digest[static_cast<std::size_t>(i * 4)]     = static_cast<std::uint8_t>((h[i] >> 24U) & 0xFFU);
        digest[static_cast<std::size_t>(i * 4 + 1)] = static_cast<std::uint8_t>((h[i] >> 16U) & 0xFFU);
        digest[static_cast<std::size_t>(i * 4 + 2)] = static_cast<std::uint8_t>((h[i] >> 8U)  & 0xFFU);
        digest[static_cast<std::size_t>(i * 4 + 3)] = static_cast<std::uint8_t>(h[i] & 0xFFU);
    }
    return digest;
}

std::string CryptoEngine::ComputeSHA256Hex(std::string_view data) {
    const auto digest = ComputeSHA256Bytes(data);
    return BytesToHex(digest.data(), digest.size());
}

std::string CryptoEngine::ComputeHMACSHA256Hex(std::string_view key, std::string_view message) {
    std::array<std::uint8_t, 64> keyBlock{};
    if (key.size() > 64) {
        const auto hashedKey = ComputeSHA256Bytes(key);
        std::memcpy(keyBlock.data(), hashedKey.data(), hashedKey.size());
    } else {
        std::memcpy(keyBlock.data(), key.data(), key.size());
    }

    std::string iKeyPad(64, '\0');
    std::string oKeyPad(64, '\0');
    for (std::size_t i = 0; i < 64; ++i) {
        iKeyPad[i] = static_cast<char>(keyBlock[i] ^ 0x36U);
        oKeyPad[i] = static_cast<char>(keyBlock[i] ^ 0x5cU);
    }

    const auto innerHash = ComputeSHA256Bytes(iKeyPad + std::string(message));
    std::string outerInput = oKeyPad;
    outerInput.append(reinterpret_cast<const char*>(innerHash.data()), innerHash.size());
    return ComputeSHA256Hex(outerInput);
}

std::vector<std::uint8_t> CryptoEngine::GenerateCSPRNGBytes(std::size_t count) {
    std::random_device rd;
    std::vector<std::uint8_t> out(count);
    for (std::size_t i = 0; i < count; ++i) {
        out[i] = static_cast<std::uint8_t>(rd() & 0xFFU);
    }
    return out;
}

std::vector<std::uint8_t> CryptoEngine::SealAuthenticated(std::string_view key,
                                                          std::string_view nonce,
                                                          std::string_view plaintext) {
    std::vector<std::uint8_t> ciphertext(plaintext.size());
    for (std::size_t blockIdx = 0; blockIdx * 32 < plaintext.size(); ++blockIdx) {
        const auto keystream = ComputeSHA256Bytes(
            std::string(key) + ":" + std::string(nonce) + ":" + std::to_string(blockIdx));
        for (std::size_t j = 0; j < 32 && (blockIdx * 32 + j) < plaintext.size(); ++j) {
            ciphertext[blockIdx * 32 + j] =
                static_cast<std::uint8_t>(plaintext[blockIdx * 32 + j]) ^ keystream[j];
        }
    }

    std::string macInput(nonce);
    macInput.append(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
    const auto tag = ComputeSHA256Bytes( ComputeHMACSHA256Hex(key, macInput) );

    std::vector<std::uint8_t> packet = std::move(ciphertext);
    packet.insert(packet.end(), tag.begin(), tag.end());
    return packet;
}

std::optional<std::string> CryptoEngine::OpenAuthenticated(std::string_view key,
                                                           std::string_view nonce,
                                                           const std::vector<std::uint8_t>& sealedPacket) {
    if (sealedPacket.size() < 32) {
        return std::nullopt;
    }
    const std::size_t cipherLen = sealedPacket.size() - 32;
    std::string macInput(nonce);
    macInput.append(reinterpret_cast<const char*>(sealedPacket.data()), cipherLen);
    const auto expectedTag = ComputeSHA256Bytes( ComputeHMACSHA256Hex(key, macInput) );

    std::string actualTagView(reinterpret_cast<const char*>(sealedPacket.data() + cipherLen), 32);
    std::string expectedTagView(reinterpret_cast<const char*>(expectedTag.data()), 32);
    if (!SecretBuffer::ConstantTimeEquals(actualTagView, expectedTagView)) {
        return std::nullopt; // Tampered packet rejected!
    }

    std::string plaintext(cipherLen, '\0');
    for (std::size_t blockIdx = 0; blockIdx * 32 < cipherLen; ++blockIdx) {
        const auto keystream = ComputeSHA256Bytes(
            std::string(key) + ":" + std::string(nonce) + ":" + std::to_string(blockIdx));
        for (std::size_t j = 0; j < 32 && (blockIdx * 32 + j) < cipherLen; ++j) {
            plaintext[blockIdx * 32 + j] =
                static_cast<char>(sealedPacket[blockIdx * 32 + j] ^ keystream[j]);
        }
    }
    return plaintext;
}

// ============================================================================
// 4. SUPPLY-CHAIN AUDITOR & SBOM GENERATOR IMPLEMENTATION
// ============================================================================

SecurityAuditResult SupplyChainAuditor::AuditDependencies(
    const std::vector<PackageDependencyDescriptor>& dependencies,
    std::string_view trustedSigningKey,
    const std::unordered_map<std::string, std::string>& knownVulnerablePkgVersions,
    Frontend::DiagnosticBag& diagnostics) {

    SecurityAuditResult res;
    res.packagesAudited = dependencies.size();

    std::ostringstream sbom;
    sbom << "{\"sbomFormat\":\"RobixSBOM-1.0\",\"components\":[";

    for (std::size_t i = 0; i < dependencies.size(); ++i) {
        const auto& dep = dependencies[i];
        if (i > 0) sbom << ",";
        sbom << "{\"name\":\"" << dep.packageName
             << "\",\"version\":\"" << dep.version
             << "\",\"sha256\":\"" << dep.sha256DigestHex << "\"}";

        // 1. Verify cryptographic signature over packageName@version:sha256
        const std::string expectedSig = CryptoEngine::ComputeHMACSHA256Hex(
            trustedSigningKey,
            dep.packageName + "@" + dep.version + ":" + dep.sha256DigestHex);

        if (!SecretBuffer::ConstantTimeEquals(dep.signatureHex, expectedSig)) {
            res.passed = false;
            res.vulnerabilitiesFound.push_back(
                "Invalid cryptographic package signature for '" + dep.packageName + "@" + dep.version + "'");
            diagnostics.ReportError(
                "SEC7002",
                "Supply-chain signature verification failed for package '" + dep.packageName + "@" + dep.version + "'.",
                Frontend::SourceLocation{ dep.packageName + ".rxp", 1, 1, 1 },
                "Re-fetch the signed package from a trusted registry.");
        }

        // 2. Check against vulnerability advisory database
        auto vIt = knownVulnerablePkgVersions.find(dep.packageName + "@" + dep.version);
        if (vIt != knownVulnerablePkgVersions.end()) {
            res.passed = false;
            res.vulnerabilitiesFound.push_back(vIt->second);
            diagnostics.ReportError(
                "SEC7003",
                "Known security vulnerability in dependency '" + dep.packageName + "@" + dep.version + "': " + vIt->second,
                Frontend::SourceLocation{ "Robix.Lock", 1, 1, 1 },
                "Upgrade '" + dep.packageName + "' to a patched version.");
        }
    }

    sbom << "]}";
    res.generatedSBOMJson = sbom.str();
    return res;
}

// ============================================================================
// 5. NETWORKING ROUTER & MQTT BROKER IMPLEMENTATION
// ============================================================================

void HTTPEndpointRouter::RegisterRoute(const std::string& method,
                                       const std::string& path,
                                       HandlerFn handler) {
    m_routes[method + " " + path] = std::move(handler);
}

HTTPResponse HTTPEndpointRouter::Dispatch(const HTTPRequest& req) const {
    auto it = m_routes.find(req.method + " " + req.path);
    if (it == m_routes.end() || !it->second) {
        return HTTPResponse{ 404, "{\"error\":\"Not Found\"}", {} };
    }
    return it->second(req);
}

void MQTTBroker::Subscribe(const std::string& topic, SubscriberFn callback) {
    m_subscribers[topic].push_back(std::move(callback));
}

std::size_t MQTTBroker::Publish(const std::string& topic, std::string_view payload) const {
    auto it = m_subscribers.find(topic);
    if (it == m_subscribers.end()) return 0;
    std::size_t delivered = 0;
    for (const auto& cb : it->second) {
        if (cb) {
            cb(topic, payload);
            ++delivered;
        }
    }
    return delivered;
}

} // namespace Robix::StdLib

