#include "DependencyResolver.hpp"

#include "../../StdLib/Core/CoreTypesAndRegex.hpp"
#include "../../StdLib/Data/JsonAndDataFormats.hpp"

#include <algorithm>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace Robix::Packages {

// ============================================================================
// 1. SEMANTIC VERSIONING IMPLEMENTATION
// ============================================================================

std::optional<SemVer> SemVer::Parse(std::string_view versionStr) noexcept {
    const std::string trimmed = StdLib::StringOps::Trim(versionStr);
    const auto parts          = StdLib::StringOps::Split(trimmed, ".");
    if (parts.size() != 3) return std::nullopt;

    auto maj = StdLib::StringOps::ToInt(parts[0]);
    auto min = StdLib::StringOps::ToInt(parts[1]);
    auto pat = StdLib::StringOps::ToInt(parts[2]);
    if (!maj || !min || !pat || *maj < 0 || *min < 0 || *pat < 0) {
        return std::nullopt;
    }
    return SemVer{ static_cast<int>(*maj), static_cast<int>(*min), static_cast<int>(*pat) };
}

std::string SemVer::ToString() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

bool SemVer::SatisfiesConstraint(std::string_view constraint) const noexcept {
    const std::string c = StdLib::StringOps::Trim(constraint);
    if (c.empty() || c == "*") return true;

    if (c[0] == '^') {
        auto req = Parse(c.substr(1));
        if (!req) return false;
        return (major == req->major) && (*this >= *req);
    }
    if (c[0] == '~') {
        auto req = Parse(c.substr(1));
        if (!req) return false;
        return (major == req->major) && (minor == req->minor) && (*this >= *req);
    }
    if (c.size() >= 2 && c.substr(0, 2) == ">=") {
        auto req = Parse(c.substr(2));
        if (!req) return false;
        return (*this >= *req);
    }
    if (c[0] == '=') {
        auto req = Parse(c.substr(1));
        if (!req) return false;
        return (*this == *req);
    }
    auto req = Parse(c);
    return req.has_value() && (*this == *req);
}

// ============================================================================
// 2. PROJECT MANIFEST (.rbxproj) & LOCKFILE (.rbxlock) IMPLEMENTATION
// ============================================================================

std::string RbxProjectManifest::SerializeToRbxProj() const {
    using namespace StdLib;
    JsonValue root = JsonValue::MakeObject();
    root.objectVal["projectName"]     = JsonValue::MakeString(projectName);
    root.objectVal["version"]         = JsonValue::MakeString(version);
    root.objectVal["projectTemplate"] = JsonValue::MakeString(projectTemplate);

    JsonValue deps = JsonValue::MakeObject();
    for (const auto& [pkg, ver] : dependencies) {
        deps.objectVal[pkg] = JsonValue::MakeString(ver);
    }
    root.objectVal["dependencies"] = std::move(deps);

    return JsonCodec::Serialize(root, JsonFormatOption::Pretty);
}

std::optional<RbxProjectManifest> RbxProjectManifest::ParseFromRbxProj(std::string_view content) {
    using namespace StdLib;
    auto rootOpt = JsonCodec::TryParse(content);
    if (!rootOpt || rootOpt->kind != JsonKind::Object) return std::nullopt;

    RbxProjectManifest proj;
    if (const auto* v = rootOpt->GetField("projectName"))     proj.projectName     = v->strVal;
    if (const auto* v = rootOpt->GetField("version"))         proj.version         = v->strVal;
    if (const auto* v = rootOpt->GetField("projectTemplate")) proj.projectTemplate = v->strVal;
    if (const auto* d = rootOpt->GetField("dependencies")) {
        for (const auto& [k, v] : d->objectVal) {
            proj.dependencies[k] = v.strVal;
        }
    }
    return proj;
}

std::string RbxLockfile::SerializeToRbxLock() const {
    using namespace StdLib;
    JsonValue root = JsonValue::MakeObject();
    root.objectVal["lockfileFormat"] = JsonValue::MakeString(lockfileFormat);

    JsonValue pkgs = JsonValue::MakeArray();
    for (const auto& entry : resolvedPackages) {
        JsonValue pObj = JsonValue::MakeObject();
        pObj.objectVal["name"]            = JsonValue::MakeString(entry.name);
        pObj.objectVal["resolvedVersion"] = JsonValue::MakeString(entry.resolvedVersion);
        pObj.objectVal["rbxApiSha256Hex"] = JsonValue::MakeString(entry.rbxApiSha256Hex);
        pObj.objectVal["signatureHex"]    = JsonValue::MakeString(entry.signatureHex);

        JsonValue dArr = JsonValue::MakeArray();
        for (const auto& d : entry.directDependencies) {
            dArr.arrayVal.push_back(JsonValue::MakeString(d));
        }
        pObj.objectVal["directDependencies"] = std::move(dArr);
        pkgs.arrayVal.push_back(std::move(pObj));
    }
    root.objectVal["resolvedPackages"] = std::move(pkgs);

    return JsonCodec::Serialize(root, JsonFormatOption::Pretty);
}

std::optional<RbxLockfile> RbxLockfile::ParseFromRbxLock(std::string_view content) {
    using namespace StdLib;
    auto rootOpt = JsonCodec::TryParse(content);
    if (!rootOpt || rootOpt->kind != JsonKind::Object) return std::nullopt;

    RbxLockfile lock;
    if (const auto* f = rootOpt->GetField("lockfileFormat")) lock.lockfileFormat = f->strVal;
    if (const auto* arr = rootOpt->GetField("resolvedPackages")) {
        for (const auto& pObj : arr->arrayVal) {
            LockedPackageEntry entry;
            if (const auto* n = pObj.GetField("name"))            entry.name            = n->strVal;
            if (const auto* v = pObj.GetField("resolvedVersion")) entry.resolvedVersion = v->strVal;
            if (const auto* h = pObj.GetField("rbxApiSha256Hex")) entry.rbxApiSha256Hex = h->strVal;
            if (const auto* s = pObj.GetField("signatureHex"))    entry.signatureHex    = s->strVal;
            if (const auto* d = pObj.GetField("directDependencies")) {
                for (const auto& el : d->arrayVal) {
                    entry.directDependencies.push_back(el.strVal);
                }
            }
            lock.resolvedPackages.push_back(std::move(entry));
        }
    }
    return lock;
}

// ============================================================================
// 3. DETERMINISTIC DEPENDENCY RESOLVER IMPLEMENTATION
// ============================================================================

void DependencyResolver::RegisterAvailablePackage(RbxPackageArchive pkg) {
    m_availablePackages.push_back(std::move(pkg));
}

const RbxPackageArchive* DependencyResolver::FindBestMatchingPackage(
    const std::string& pkgName,
    std::string_view constraint) const noexcept {

    const RbxPackageArchive* best = nullptr;
    SemVer bestVer{ -1, -1, -1 };

    for (const auto& candidate : m_availablePackages) {
        if (candidate.manifest.name != pkgName) continue;
        auto verOpt = SemVer::Parse(candidate.manifest.version);
        if (!verOpt) continue;
        if (verOpt->SatisfiesConstraint(constraint)) {
            if (!best || bestVer < *verOpt) {
                best    = &candidate;
                bestVer = *verOpt;
            }
        }
    }
    return best;
}

std::optional<RbxLockfile> DependencyResolver::ResolveProjectDependencies(
    const RbxProjectManifest& project,
    Frontend::DiagnosticBag& diagnostics) const {

    std::unordered_map<std::string, const RbxPackageArchive*> resolvedMap;
    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visited;
    std::vector<LockedPackageEntry> topologicalOrder;

    std::function<bool(const std::string&, std::string_view)> dfsResolve =
        [&](const std::string& pkgName, std::string_view constraint) -> bool {
        if (visiting.find(pkgName) != visiting.end()) {
            diagnostics.ReportError(
                "P8008",
                "Circular package dependency detected involving package '" + pkgName + "'.",
                Frontend::SourceLocation{ "Robix.Project.rbxproj", 1, 1, 1 },
                "Break the dependency cycle between packages.");
            return false;
        }

        const RbxPackageArchive* chosen = FindBestMatchingPackage(pkgName, constraint);
        if (!chosen) {
            diagnostics.ReportError(
                "P8007",
                "Unsatisfied package dependency or version conflict: '" + pkgName +
                    "' matching constraint '" + std::string(constraint) + "'.",
                Frontend::SourceLocation{ "Robix.Project.rbxproj", 1, 1, 1 },
                "Verify the package name and version constraint in Robix.Project.rbxproj.");
            return false;
        }

        // If already resolved by another branch, verify version compatibility
        auto existingIt = resolvedMap.find(pkgName);
        if (existingIt != resolvedMap.end()) {
            auto existingVer = SemVer::Parse(existingIt->second->manifest.version);
            if (!existingVer || !existingVer->SatisfiesConstraint(constraint)) {
                diagnostics.ReportError(
                    "P8007",
                    "Version conflict for package '" + pkgName + "': already resolved to '" +
                        existingIt->second->manifest.version + "', which does not satisfy '" +
                        std::string(constraint) + "'.",
                    Frontend::SourceLocation{ "Robix.Project.rbxproj", 1, 1, 1 },
                    "Align conflicting dependency version constraints.");
                return false;
            }
            return true;
        }

        visiting.insert(pkgName);
        resolvedMap[pkgName] = chosen;

        std::vector<std::string> directDeps;
        for (const auto& [childName, childConstraint] : chosen->manifest.dependencies) {
            directDeps.push_back(childName);
            if (!dfsResolve(childName, childConstraint)) {
                return false;
            }
        }

        visiting.erase(pkgName);
        if (visited.insert(pkgName).second) {
            topologicalOrder.push_back(LockedPackageEntry{
                chosen->manifest.name,
                chosen->manifest.version,
                chosen->rbxApiSha256Hex,
                chosen->publisherSignatureHex,
                std::move(directDeps)
            });
        }
        return true;
    };

    for (const auto& [pkgName, constraint] : project.dependencies) {
        if (!dfsResolve(pkgName, constraint)) {
            return std::nullopt;
        }
    }

    RbxLockfile lock;
    lock.resolvedPackages = std::move(topologicalOrder);
    return lock;
}

} // namespace Robix::Packages

