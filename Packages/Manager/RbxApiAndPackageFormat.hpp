#pragma once

#include "../../Compiler/AST/ASTNodes.hpp"
#include "../../Compiler/Semantic/Diagnostics.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Robix::Packages {

// ============================================================================
// 1. PUBLIC API DEFINITION (.rbxapi) GENERATOR & VALIDATOR
// ============================================================================
struct RbxApiValidationResult {
    bool                     valid = false;
    std::string              packageName;
    std::string              apiSha256Hex;
    std::size_t              publicTypeCount     = 0;
    std::size_t              publicFunctionCount = 0;
    std::vector<std::string> requiredPermissions;
};

class RbxApiEngine {
public:
    // Extracts public declarations + Explain blocks from .rx source code into a .rbxapi specification
    // (Strips private/internal members and all function bodies so private implementation is hidden)
    [[nodiscard]] static std::string GenerateRbxApiFromSource(
        std::string_view packageName,
        std::string_view version,
        std::string_view rbxSourceCode,
        Frontend::DiagnosticBag& diagnostics);

    // Parses and validates a .rbxapi file, enforcing mandatory Explain blocks on all public functions/methods (P8001)
    [[nodiscard]] static RbxApiValidationResult ValidateRbxApi(
        std::string_view fileName,
        std::string_view rbxApiContent,
        Frontend::DiagnosticBag& diagnostics);
};

// ============================================================================
// 2. COMPILED BINARY PACKAGE ARCHIVE (.rbxp)
// ============================================================================
struct TargetBinaryArtifact {
    std::string              targetPlatform; // e.g. "windows-x64", "linux-x64", "macos-arm64", "embedded-arm64"
    std::string              artifactFileName; // e.g. "RobixMotors.dll", "libRobixMotors.so"
    std::vector<std::uint8_t> binaryPayload;
    std::string              sha256Hex;
};

struct RbxPackageManifest {
    std::string                        name;
    std::string                        version             = "1.0.0";
    std::string                        compilerVersion     = "1.0";
    std::string                        abiVersion          = "RobixABI_v1";
    std::string                        license             = "Proprietary";
    bool                               hasInstallScript    = false;
    std::vector<std::string>           permissions;
    std::map<std::string, std::string> dependencies; // pkgName -> versionConstraint
};

struct RbxPackageArchive {
    RbxPackageManifest                manifest;
    std::string                       rbxApiContent;
    std::string                       rbxApiSha256Hex;
    std::vector<TargetBinaryArtifact> binaries;
    std::string                       documentationMarkdown;
    std::string                       sbomJson;
    std::string                       publisherSignatureHex;
};

struct PackageVerificationOptions {
    std::string expectedABIVersion      = "RobixABI_v1";
    std::string requiredTargetPlatform  = "windows-x64";
    std::string trustedSigningKey       = "RoboticelaOfficialPublisherKey";
    bool        allowInstallScripts     = false; // Default false: blocks arbitrary install scripts (P8006)
};

class RbxPackagePacker {
public:
    // Packs a .rbxapi interface + target binaries into a signed .rbxp archive
    [[nodiscard]] static RbxPackageArchive CreateSignedPackage(
        RbxPackageManifest manifest,
        std::string rbxApiContent,
        std::vector<TargetBinaryArtifact> targetBinaries,
        std::string_view publisherSigningKey,
        Frontend::DiagnosticBag& diagnostics);

    // Serializes .rbxp archive to a deterministic JSON/Binary container string
    [[nodiscard]] static std::string SerializeArchive(const RbxPackageArchive& archive);

    // Deserializes .rbxp archive from container string
    [[nodiscard]] static std::optional<RbxPackageArchive> DeserializeArchive(
        std::string_view serialized,
        Frontend::DiagnosticBag& diagnostics);

    // Performs full security, SHA-256, signature, target OS/CPU, and ABI verification before linking
    [[nodiscard]] static bool VerifyPackageArchive(
        const RbxPackageArchive& archive,
        const PackageVerificationOptions& options,
        Frontend::DiagnosticBag& diagnostics);
};

} // namespace Robix::Packages

