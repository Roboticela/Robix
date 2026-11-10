#pragma once

#include "../../Compiler/Semantic/Diagnostics.hpp"
#include "../Manager/RbxApiAndPackageFormat.hpp"
#include "../Resolver/DependencyResolver.hpp"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Robix::Packages {

// ============================================================================
// 1. PACKAGE REGISTRY CLIENT/SERVER WORKFLOWS (publish, search, info, verify)
// ============================================================================
struct RegistrySearchHit {
    std::string name;
    std::string latestVersion;
    std::string license;
};

class RobixPackageRegistry {
public:
    explicit RobixPackageRegistry(std::string trustedPublisherKey = "RoboticelaOfficialPublisherKey")
        : m_trustedPublisherKey(std::move(trustedPublisherKey)) {}

    // `robix publish` — verifies signature, .rxapi Explain blocks, and SHA-256 before accepting
    bool PublishPackage(const RbxPackageArchive& archive,
                        const std::string& requiredTargetPlatform,
                        Frontend::DiagnosticBag& diagnostics);

    // `robix search <query>`
    [[nodiscard]] std::vector<RegistrySearchHit> Search(std::string_view query) const;

    // `robix info <name>[@version]`
    [[nodiscard]] const RbxPackageArchive* GetPackageInfo(
        std::string_view name,
        std::string_view versionConstraint = "*") const noexcept;

    [[nodiscard]] const DependencyResolver& Resolver() const noexcept { return m_resolver; }
    [[nodiscard]] const std::string& TrustedPublisherKey() const noexcept { return m_trustedPublisherKey; }

private:
    std::string        m_trustedPublisherKey;
    DependencyResolver m_resolver;
};

// ============================================================================
// 2. PROJECT-LOCAL PACKAGE MANAGER (`robix` CLI Engine)
// ============================================================================
struct InstalledPackageLayout {
    std::string              packageName;
    std::string              version;
    std::string              installDirectory; // <ProjectRoot>/.robix/packages/<Name>/<Version>/
    std::string              rbxApiPath;       // .../<Name>.rxapi
    std::vector<std::string> binaryPaths;      // .../binaries/<target>/<file>
    std::string              packageJsonPath;  // .../package.json
};

class RobixPackageManager {
public:
    explicit RobixPackageManager(std::string projectRoot, RobixPackageRegistry& registry)
        : m_projectRoot(std::move(projectRoot)), m_registry(registry) {}

    // `robix new <Name> --template robotics|desktop|ai|embedded|library`
    bool NewProject(std::string_view projectName, std::string_view templateKind = "robotics");

    // `robix init`
    bool InitProject(std::string_view projectName);

    // `robix add <Pkg>[@version]`
    bool AddDependency(const std::string& packageName, const std::string& versionConstraint = "*");

    // `robix remove <Pkg>`
    bool RemoveDependency(const std::string& packageName);

    // `robix install` & `robix update` -> resolves dependencies, verifies signatures/ABI,
    // installs into `<ProjectRoot>/.robix/packages/<Name>/<Version>/`, and writes `Robix.Lock`
    bool InstallPackages(const std::string& targetPlatform, Frontend::DiagnosticBag& diagnostics);

    // `robix restore` -> verifies Installed packages match deterministic `Robix.Lock`
    bool RestoreFromLockfile(const std::string& targetPlatform, Frontend::DiagnosticBag& diagnostics);

    // `robix list`
    [[nodiscard]] const std::vector<InstalledPackageLayout>& ListInstalled() const noexcept {
        return m_installedPackages;
    }

    // `robix tree`
    [[nodiscard]] std::string RenderDependencyTree() const;

    [[nodiscard]] const RbxProjectManifest& ProjectManifest() const noexcept { return m_manifest; }
    [[nodiscard]] const RbxLockfile& Lockfile() const noexcept { return m_lockfile; }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& VirtualFS() const noexcept {
        return m_virtualProjectFS;
    }

private:
    std::string                                  m_projectRoot;
    RobixPackageRegistry&                        m_registry;
    RbxProjectManifest                           m_manifest;
    RbxLockfile                                  m_lockfile;
    std::vector<InstalledPackageLayout>          m_installedPackages;
    std::unordered_map<std::string, std::string> m_virtualProjectFS;
};

} // namespace Robix::Packages

