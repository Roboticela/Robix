#pragma once

#include "../../Compiler/Semantic/Diagnostics.hpp"
#include "../Manager/RxApiAndPackageFormat.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Robix::Packages {

// ============================================================================
// 1. SEMANTIC VERSIONING (SemVer 2.0 + Constraint Matching)
// ============================================================================
struct SemVer {
    int major = 1;
    int minor = 0;
    int patch = 0;

    [[nodiscard]] static std::optional<SemVer> Parse(std::string_view versionStr) noexcept;
    [[nodiscard]] std::string ToString() const;

    constexpr bool operator==(const SemVer& rhs) const noexcept {
        return major == rhs.major && minor == rhs.minor && patch == rhs.patch;
    }
    constexpr bool operator<(const SemVer& rhs) const noexcept {
        if (major != rhs.major) return major < rhs.major;
        if (minor != rhs.minor) return minor < rhs.minor;
        return patch < rhs.patch;
    }
    constexpr bool operator>=(const SemVer& rhs) const noexcept { return !(*this < rhs); }

    // Supports "^1.2.0" (caret), "~1.2.0" (tilde), ">=1.0.0", and exact "1.2.3" / "=1.2.3"
    [[nodiscard]] bool SatisfiesConstraint(std::string_view constraint) const noexcept;
};

// ============================================================================
// 2. PROJECT MANIFEST (.rxproj) & DETERMINISTIC LOCKFILE (.rxlock)
// ============================================================================
struct RxProjectManifest {
    std::string                        projectName;
    std::string                        version         = "1.0.0";
    std::string                        projectTemplate = "robotics"; // robotics|desktop|ai|embedded|library
    std::map<std::string, std::string> dependencies;                 // pkgName -> versionConstraint

    [[nodiscard]] std::string SerializeToRxProj() const;
    [[nodiscard]] static std::optional<RxProjectManifest> ParseFromRxProj(std::string_view content);
};

struct LockedPackageEntry {
    std::string              name;
    std::string              resolvedVersion;
    std::string              rxApiSha256Hex;
    std::string              signatureHex;
    std::vector<std::string> directDependencies;
};

struct RxLockfile {
    std::string                     lockfileFormat = "RobixLock-1.0";
    std::vector<LockedPackageEntry> resolvedPackages; // Ordered deterministically (topological + lexical)

    [[nodiscard]] std::string SerializeToRxLock() const;
    [[nodiscard]] static std::optional<RxLockfile> ParseFromRxLock(std::string_view content);
};

// ============================================================================
// 3. DETERMINISTIC DEPENDENCY RESOLVER (DAG, Cycle & Conflict Detection)
// ============================================================================
class DependencyResolver {
public:
    void RegisterAvailablePackage(RxPackageArchive pkg);

    [[nodiscard]] std::optional<RxLockfile> ResolveProjectDependencies(
        const RxProjectManifest& project,
        Frontend::DiagnosticBag& diagnostics) const;

    [[nodiscard]] const std::vector<RxPackageArchive>& AvailablePackages() const noexcept {
        return m_availablePackages;
    }

private:
    std::vector<RxPackageArchive> m_availablePackages;

    [[nodiscard]] const RxPackageArchive* FindBestMatchingPackage(
        const std::string& pkgName,
        std::string_view constraint) const noexcept;
};

} // namespace Robix::Packages

