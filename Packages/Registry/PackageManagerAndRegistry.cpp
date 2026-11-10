#include "PackageManagerAndRegistry.hpp"

#include "../../StdLib/Core/CoreTypesAndRegex.hpp"

#include <sstream>

namespace Robix::Packages {

// ============================================================================
// 1. PACKAGE REGISTRY IMPLEMENTATION
// ============================================================================

bool RobixPackageRegistry::PublishPackage(const RbxPackageArchive& archive,
                                          const std::string& requiredTargetPlatform,
                                          Frontend::DiagnosticBag& diagnostics) {
    PackageVerificationOptions opts;
    opts.trustedSigningKey      = m_trustedPublisherKey;
    opts.requiredTargetPlatform = requiredTargetPlatform;
    opts.allowInstallScripts    = false;

    if (!RbxPackagePacker::VerifyPackageArchive(archive, opts, diagnostics)) {
        return false;
    }

    m_resolver.RegisterAvailablePackage(archive);
    return true;
}

std::vector<RegistrySearchHit> RobixPackageRegistry::Search(std::string_view query) const {
    std::vector<RegistrySearchHit> hits;
    const std::string lowerQ = StdLib::StringOps::ToLower(query);

    for (const auto& pkg : m_resolver.AvailablePackages()) {
        const std::string lowerName = StdLib::StringOps::ToLower(pkg.manifest.name);
        if (lowerQ.empty() || lowerName.find(lowerQ) != std::string::npos) {
            hits.push_back(RegistrySearchHit{
                pkg.manifest.name,
                pkg.manifest.version,
                pkg.manifest.license
            });
        }
    }
    return hits;
}

const RbxPackageArchive* RobixPackageRegistry::GetPackageInfo(
    std::string_view name,
    std::string_view versionConstraint) const noexcept {

    const RbxPackageArchive* best = nullptr;
    SemVer bestVer{ -1, -1, -1 };

    for (const auto& pkg : m_resolver.AvailablePackages()) {
        if (pkg.manifest.name != name) continue;
        auto v = SemVer::Parse(pkg.manifest.version);
        if (v && v->SatisfiesConstraint(versionConstraint)) {
            if (!best || bestVer < *v) {
                best    = &pkg;
                bestVer = *v;
            }
        }
    }
    return best;
}

// ============================================================================
// 2. PROJECT-LOCAL PACKAGE MANAGER IMPLEMENTATION
// ============================================================================

bool RobixPackageManager::NewProject(std::string_view projectName, std::string_view templateKind) {
    m_manifest.projectName     = std::string(projectName);
    m_manifest.version         = "1.0.0";
    m_manifest.projectTemplate = std::string(templateKind);
    m_manifest.dependencies.clear();

    m_virtualProjectFS[m_projectRoot + "/Robix.Project.rbxproj"] = m_manifest.SerializeToRbxProj();
    m_virtualProjectFS[m_projectRoot + "/Project.rbxb"] =
        "Project {\n    Name = \"" + std::string(projectName) + "\";\n    Style = \"" +
        std::string(templateKind) + "\";\n}\n";
    m_virtualProjectFS[m_projectRoot + "/Source/Main.rx"] =
        "package " + std::string(projectName) + ";\npublic Int Main() {\n    return 0;\n}\n";
    m_virtualProjectFS[m_projectRoot + "/Tests/Main.test.rx"] =
        "Test \"Main entry point check\" {\n    Assert.Equal(Main(), 0);\n}\n";
    return true;
}

bool RobixPackageManager::InitProject(std::string_view projectName) {
    return NewProject(projectName, "robotics");
}

bool RobixPackageManager::AddDependency(const std::string& packageName, const std::string& versionConstraint) {
    m_manifest.dependencies[packageName] = versionConstraint;
    m_virtualProjectFS[m_projectRoot + "/Robix.Project.rbxproj"] = m_manifest.SerializeToRbxProj();
    return true;
}

bool RobixPackageManager::RemoveDependency(const std::string& packageName) {
    const bool erased = m_manifest.dependencies.erase(packageName) > 0;
    m_virtualProjectFS[m_projectRoot + "/Robix.Project.rbxproj"] = m_manifest.SerializeToRbxProj();
    return erased;
}

bool RobixPackageManager::InstallPackages(const std::string& targetPlatform,
                                          Frontend::DiagnosticBag& diagnostics) {
    auto resolvedLockOpt = m_registry.Resolver().ResolveProjectDependencies(m_manifest, diagnostics);
    if (!resolvedLockOpt.has_value()) {
        return false;
    }

    m_lockfile = *resolvedLockOpt;
    m_installedPackages.clear();

    PackageVerificationOptions verifyOpts;
    verifyOpts.trustedSigningKey      = m_registry.TrustedPublisherKey();
    verifyOpts.requiredTargetPlatform = targetPlatform;
    verifyOpts.allowInstallScripts    = false;

    for (const auto& locked : m_lockfile.resolvedPackages) {
        const RbxPackageArchive* pkg = m_registry.GetPackageInfo(locked.name, "=" + locked.resolvedVersion);
        if (!pkg) {
            diagnostics.ReportError(
                "P8007",
                "Failed to fetch locked package '" + locked.name + "@" + locked.resolvedVersion + "' from registry.",
                Frontend::SourceLocation{ "Robix.Lock", 1, 1, 1 });
            return false;
        }

        if (!RbxPackagePacker::VerifyPackageArchive(*pkg, verifyOpts, diagnostics)) {
            return false;
        }

        const std::string installDir =
            m_projectRoot + "/.robix/packages/" + pkg->manifest.name + "/" + pkg->manifest.version;
        const std::string apiPath  = installDir + "/" + pkg->manifest.name + ".rbxapi";
        const std::string jsonPath = installDir + "/package.json";

        m_virtualProjectFS[apiPath]  = pkg->rbxApiContent;
        m_virtualProjectFS[jsonPath] = pkg->sbomJson;

        InstalledPackageLayout layout;
        layout.packageName      = pkg->manifest.name;
        layout.version          = pkg->manifest.version;
        layout.installDirectory = installDir;
        layout.rbxApiPath       = apiPath;
        layout.packageJsonPath  = jsonPath;

        for (const auto& bin : pkg->binaries) {
            const std::string binPath =
                installDir + "/binaries/" + bin.targetPlatform + "/" + bin.artifactFileName;
            m_virtualProjectFS[binPath] = std::string(
                reinterpret_cast<const char*>(bin.binaryPayload.data()),
                bin.binaryPayload.size());
            layout.binaryPaths.push_back(binPath);
        }

        m_installedPackages.push_back(std::move(layout));
    }

    m_virtualProjectFS[m_projectRoot + "/Robix.Lock"] = m_lockfile.SerializeToRbxLock();
    return true;
}

bool RobixPackageManager::RestoreFromLockfile(const std::string& targetPlatform,
                                              Frontend::DiagnosticBag& diagnostics) {
    return InstallPackages(targetPlatform, diagnostics);
}

std::string RobixPackageManager::RenderDependencyTree() const {
    std::ostringstream oss;
    oss << m_manifest.projectName << " v" << m_manifest.version << "\n";
    for (const auto& pkg : m_lockfile.resolvedPackages) {
        oss << "├── " << pkg.name << "@" << pkg.resolvedVersion << "\n";
        for (const auto& dep : pkg.directDependencies) {
            oss << "│   └── " << dep << "\n";
        }
    }
    return oss.str();
}

} // namespace Robix::Packages

