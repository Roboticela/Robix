#include "RxAPIAndPackageFormat.hpp"

#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Types/SpecValidator.hpp"
#include "../../Compiler/Types/TypeChecker.hpp"
#include "../../StdLib/Data/JSONAndDataFormats.hpp"
#include "../../StdLib/Security/SecurityAndNetwork.hpp"

#include <sstream>

namespace Robix::Packages {

namespace {

void EmitExplainBlockText(std::ostringstream& out, const Frontend::ExplainBlockNode& explain, const std::string& indent) {
    out << indent << "Explain {\n";
    out << indent << "    Description: \"" << explain.description << "\";\n";
    if (!explain.inputs.empty()) {
        out << indent << "    Input {\n";
        for (const auto& [pName, pDoc] : explain.inputs) {
            out << indent << "        " << pName << ": \"" << pDoc << "\";\n";
        }
        out << indent << "    }\n";
    }
    out << indent << "    Processing: \"" << explain.processing << "\";\n";
    if (!explain.output.empty()) {
        out << indent << "    Output: \"" << explain.output << "\";\n";
    }
    out << indent << "}\n";
}

void EmitFunctionSignatureForRxAPI(std::ostringstream& out, const Frontend::DeclNode& fn, const std::string& indent) {
    out << indent << "public ";
    if (fn.isStatic) out << "static ";
    if (fn.isAsync) out << "Async ";
    out << (fn.returnType.IsEmpty() ? "Void" : fn.returnType.ToString()) << " " << fn.name << "(";
    for (std::size_t i = 0; i < fn.parameters.size(); ++i) {
        if (i > 0) out << ", ";
        out << fn.parameters[i].type.ToString() << " " << fn.parameters[i].name;
    }
    out << ")\n";
    if (fn.explainBlock.has_value() && fn.explainBlock->present) {
        EmitExplainBlockText(out, *fn.explainBlock, indent + "    ");
    }
    out << indent << ";\n";
}

std::string ComputeArchiveSigningPayload(const RxPackageArchive& archive) {
    std::ostringstream oss;
    oss << archive.manifest.name << "@" << archive.manifest.version
        << "|abi=" << archive.manifest.abiVersion
        << "|api=" << archive.rxAPISha256Hex;
    for (const auto& bin : archive.binaries) {
        oss << "|bin:" << bin.targetPlatform << ":" << bin.artifactFileName << ":" << bin.sha256Hex;
    }
    return oss.str();
}

void ValidatePublicExplainRecursive(const Frontend::DeclNode& decl,
                                    std::size_t& outPublicTypes,
                                    std::size_t& outPublicFunctions,
                                    Frontend::DiagnosticBag& diagnostics) {
    if (decl.visibility != Spec::AccessVisibility::Public) {
        return;
    }

    if (decl.kind == Frontend::DeclKind::Function) {
        ++outPublicFunctions;
        if (!decl.explainBlock.has_value() || !decl.explainBlock->present ||
            decl.explainBlock->description.empty() || decl.explainBlock->processing.empty()) {
            diagnostics.ReportError(
                "P8001",
                "Public API declaration '" + decl.name + "' in .rxapi is missing a complete 'Explain { ... }' block.",
                decl.location,
                "Include an Explain { Description: ...; Processing: ...; Output: ...; } block in the .rxapi signature.");
        }
    } else if (decl.kind == Frontend::DeclKind::Class ||
               decl.kind == Frontend::DeclKind::Struct ||
               decl.kind == Frontend::DeclKind::Interface ||
               decl.kind == Frontend::DeclKind::Enum) {
        ++outPublicTypes;
        for (const auto& member : decl.members) {
            if (member) {
                ValidatePublicExplainRecursive(*member, outPublicTypes, outPublicFunctions, diagnostics);
            }
        }
    }
}

} // namespace

// ============================================================================
// 1. RXAPI GENERATOR & VALIDATOR IMPLEMENTATION
// ============================================================================

std::string RxAPIEngine::GenerateRxAPIFromSource(
    std::string_view packageName,
    std::string_view version,
    std::string_view rxSourceCode,
    Frontend::DiagnosticBag& diagnostics) {

    Frontend::Lexer lexer(rxSourceCode, std::string(packageName) + ".rx", diagnostics);
    Frontend::Parser parser(lexer.TokenizeAll(), std::string(packageName) + ".rx", diagnostics);
    auto ast = parser.ParseCompilationUnit();

    std::ostringstream out;
    out << "Package " << (packageName.empty() ? ast.packageName : std::string(packageName)) << " {\n";
    out << "    Version = \"" << version << "\";\n";
    out << "    Compiler = \"" << Spec::LANGUAGE_VERSION << "\";\n";

    if (!ast.packagePermissions.empty()) {
        out << "\n    Permissions {\n";
        for (const auto& perm : ast.packagePermissions) {
            out << "        " << perm << ";\n";
        }
        out << "    }\n";
    }

    for (const auto& imp : ast.imports) {
        out << "    import " << imp << ";\n";
    }

    for (const auto& declPtr : ast.declarations) {
        if (!declPtr || declPtr->visibility != Spec::AccessVisibility::Public) {
            continue; // Strip private/internal declarations completely
        }
        const auto& decl = *declPtr;
        if (decl.kind == Frontend::DeclKind::Function) {
            out << "\n";
            EmitFunctionSignatureForRxAPI(out, decl, "    ");
        } else if (decl.kind == Frontend::DeclKind::Class || decl.kind == Frontend::DeclKind::Struct) {
            const char* kw = (decl.kind == Frontend::DeclKind::Class) ? "class" : "struct";
            out << "\n    public " << kw << " " << decl.name << " {\n";
            for (const auto& mPtr : decl.members) {
                if (!mPtr || mPtr->visibility != Spec::AccessVisibility::Public) continue;
                if (mPtr->kind == Frontend::DeclKind::Function) {
                    EmitFunctionSignatureForRxAPI(out, *mPtr, "        ");
                }
            }
            out << "    }\n";
        }
    }

    out << "}\n";
    return out.str();
}

RxAPIValidationResult RxAPIEngine::ValidateRxAPI(
    std::string_view fileName,
    std::string_view rxAPIContent,
    Frontend::DiagnosticBag& diagnostics) {

    RxAPIValidationResult res;
    res.apiSha256Hex = StdLib::CryptoEngine::ComputeSHA256Hex(rxAPIContent);

    Frontend::Lexer lexer(rxAPIContent, fileName, diagnostics);
    Frontend::Parser parser(lexer.TokenizeAll(), fileName, diagnostics);
    auto ast = parser.ParseCompilationUnit();

    res.packageName         = ast.packageName;
    res.requiredPermissions = ast.packagePermissions;

    Frontend::TypeCheckerOptions tcOpts;
    tcOpts.strictStyleMode = true;
    tcOpts.requireExplain  = true;
    Frontend::TypeChecker tc(diagnostics, tcOpts);
    tc.CheckCompilationUnit(ast);

    for (const auto& declPtr : ast.declarations) {
        if (declPtr) {
            ValidatePublicExplainRecursive(
                *declPtr,
                res.publicTypeCount,
                res.publicFunctionCount,
                diagnostics);
        }
    }

    res.valid = !diagnostics.HasErrors();
    return res;
}

// ============================================================================
// 2. RXP PACKER, SERIALIZER & SECURITY/ABI VERIFIER IMPLEMENTATION
// ============================================================================

RxPackageArchive RxPackagePacker::CreateSignedPackage(
    RxPackageManifest manifest,
    std::string rxAPIContent,
    std::vector<TargetBinaryArtifact> targetBinaries,
    std::string_view publisherSigningKey,
    Frontend::DiagnosticBag& diagnostics) {

    RxPackageArchive archive;
    archive.manifest        = std::move(manifest);
    archive.rxAPIContent   = std::move(rxAPIContent);
    archive.rxAPISha256Hex = StdLib::CryptoEngine::ComputeSHA256Hex(archive.rxAPIContent);

    // Validate the .rxapi before packaging
    auto apiVal = RxAPIEngine::ValidateRxAPI(
        archive.manifest.name + ".rxapi",
        archive.rxAPIContent,
        diagnostics);

    if (archive.manifest.permissions.empty() && !apiVal.requiredPermissions.empty()) {
        archive.manifest.permissions = apiVal.requiredPermissions;
    }

    // Compute SHA-256 for every target binary artifact
    for (auto& bin : targetBinaries) {
        std::string_view payloadView(
            reinterpret_cast<const char*>(bin.binaryPayload.data()),
            bin.binaryPayload.size());
        bin.sha256Hex = StdLib::CryptoEngine::ComputeSHA256Hex(payloadView);
    }
    archive.binaries = std::move(targetBinaries);

    // Generate documentation summary & SBOM JSON
    archive.documentationMarkdown = "# Package Documentation: " + archive.manifest.name +
                                    " v" + archive.manifest.version + "\n";

    std::ostringstream sbom;
    sbom << "{\"sbomFormat\":\"RobixSBOM-1.0\",\"package\":\"" << archive.manifest.name
         << "\",\"version\":\"" << archive.manifest.version
         << "\",\"abi\":\"" << archive.manifest.abiVersion
         << "\",\"apiSha256\":\"" << archive.rxAPISha256Hex << "\"}";
    archive.sbomJSON = sbom.str();

    // Sign the archive using HMAC-SHA256 with the publisher key
    archive.publisherSignatureHex = StdLib::CryptoEngine::ComputeHMACSHA256Hex(
        publisherSigningKey,
        ComputeArchiveSigningPayload(archive));

    return archive;
}

std::string RxPackagePacker::SerializeArchive(const RxPackageArchive& archive) {
    using namespace StdLib;
    JSONValue root = JSONValue::MakeObject();
    root.objectVal["name"]                  = JSONValue::MakeString(archive.manifest.name);
    root.objectVal["version"]               = JSONValue::MakeString(archive.manifest.version);
    root.objectVal["compilerVersion"]       = JSONValue::MakeString(archive.manifest.compilerVersion);
    root.objectVal["abiVersion"]            = JSONValue::MakeString(archive.manifest.abiVersion);
    root.objectVal["license"]               = JSONValue::MakeString(archive.manifest.license);
    root.objectVal["hasInstallScript"]      = JSONValue::MakeBool(archive.manifest.hasInstallScript);
    root.objectVal["rxAPIContent"]         = JSONValue::MakeString(archive.rxAPIContent);
    root.objectVal["rxAPISha256Hex"]       = JSONValue::MakeString(archive.rxAPISha256Hex);
    root.objectVal["documentationMarkdown"] = JSONValue::MakeString(archive.documentationMarkdown);
    root.objectVal["sbomJSON"]              = JSONValue::MakeString(archive.sbomJSON);
    root.objectVal["publisherSignatureHex"] = JSONValue::MakeString(archive.publisherSignatureHex);

    JSONValue permsArr = JSONValue::MakeArray();
    for (const auto& p : archive.manifest.permissions) {
        permsArr.arrayVal.push_back(JSONValue::MakeString(p));
    }
    root.objectVal["permissions"] = std::move(permsArr);

    JSONValue depsObj = JSONValue::MakeObject();
    for (const auto& [depName, depVer] : archive.manifest.dependencies) {
        depsObj.objectVal[depName] = JSONValue::MakeString(depVer);
    }
    root.objectVal["dependencies"] = std::move(depsObj);

    JSONValue binsArr = JSONValue::MakeArray();
    for (const auto& bin : archive.binaries) {
        JSONValue bObj = JSONValue::MakeObject();
        bObj.objectVal["targetPlatform"]   = JSONValue::MakeString(bin.targetPlatform);
        bObj.objectVal["artifactFileName"] = JSONValue::MakeString(bin.artifactFileName);
        bObj.objectVal["sha256Hex"]        = JSONValue::MakeString(bin.sha256Hex);
        std::string payloadStr(reinterpret_cast<const char*>(bin.binaryPayload.data()), bin.binaryPayload.size());
        bObj.objectVal["payload"]          = JSONValue::MakeString(std::move(payloadStr));
        binsArr.arrayVal.push_back(std::move(bObj));
    }
    root.objectVal["binaries"] = std::move(binsArr);

    return JSONCodec::Serialize(root, JSONFormatOption::Pretty);
}

std::optional<RxPackageArchive> RxPackagePacker::DeserializeArchive(
    std::string_view serialized,
    Frontend::DiagnosticBag& diagnostics) {

    using namespace StdLib;
    std::string parseErr;
    auto rootOpt = JSONCodec::TryParse(serialized, &parseErr);
    if (!rootOpt.has_value() || rootOpt->kind != JSONKind::Object) {
        diagnostics.ReportError(
            "P8002",
            "Corrupted .rxp package archive: " + parseErr,
            Frontend::SourceLocation{});
        return std::nullopt;
    }

    const JSONValue& root = *rootOpt;
    RxPackageArchive archive;
    if (const auto* v = root.GetField("name"))                  archive.manifest.name            = v->strVal;
    if (const auto* v = root.GetField("version"))               archive.manifest.version         = v->strVal;
    if (const auto* v = root.GetField("compilerVersion"))       archive.manifest.compilerVersion = v->strVal;
    if (const auto* v = root.GetField("abiVersion"))            archive.manifest.abiVersion      = v->strVal;
    if (const auto* v = root.GetField("license"))               archive.manifest.license         = v->strVal;
    if (const auto* v = root.GetField("hasInstallScript"))      archive.manifest.hasInstallScript = v->boolVal;
    if (const auto* v = root.GetField("rxAPIContent"))         archive.rxAPIContent            = v->strVal;
    if (const auto* v = root.GetField("rxAPISha256Hex"))       archive.rxAPISha256Hex          = v->strVal;
    if (const auto* v = root.GetField("documentationMarkdown")) archive.documentationMarkdown    = v->strVal;
    if (const auto* v = root.GetField("sbomJSON"))              archive.sbomJSON                 = v->strVal;
    if (const auto* v = root.GetField("publisherSignatureHex")) archive.publisherSignatureHex    = v->strVal;

    if (const auto* perms = root.GetField("permissions")) {
        for (const auto& el : perms->arrayVal) {
            archive.manifest.permissions.push_back(el.strVal);
        }
    }
    if (const auto* deps = root.GetField("dependencies")) {
        for (const auto& [k, v] : deps->objectVal) {
            archive.manifest.dependencies[k] = v.strVal;
        }
    }
    if (const auto* bins = root.GetField("binaries")) {
        for (const auto& bObj : bins->arrayVal) {
            TargetBinaryArtifact art;
            if (const auto* p = bObj.GetField("targetPlatform"))   art.targetPlatform   = p->strVal;
            if (const auto* f = bObj.GetField("artifactFileName")) art.artifactFileName = f->strVal;
            if (const auto* s = bObj.GetField("sha256Hex"))        art.sha256Hex        = s->strVal;
            if (const auto* d = bObj.GetField("payload")) {
                art.binaryPayload.assign(d->strVal.begin(), d->strVal.end());
            }
            archive.binaries.push_back(std::move(art));
        }
    }

    return archive;
}

bool RxPackagePacker::VerifyPackageArchive(
    const RxPackageArchive& archive,
    const PackageVerificationOptions& options,
    Frontend::DiagnosticBag& diagnostics) {

    bool ok = true;

    // 1. Block install scripts by default (P8006)
    if (archive.manifest.hasInstallScript && !options.allowInstallScripts) {
        diagnostics.ReportError(
            "P8006",
            "Package '" + archive.manifest.name + "' contains an install script, which is blocked by default for security.",
            Frontend::SourceLocation{ archive.manifest.name + ".rxp", 1, 1, 1 },
            "Remove the install script or explicitly pass '--allow-install-scripts' after auditing.");
        ok = false;
    }

    // 2. Verify ABI compatibility (P8004)
    if (archive.manifest.abiVersion != options.expectedABIVersion) {
        diagnostics.ReportError(
            "P8004",
            "ABI mismatch in package '" + archive.manifest.name + "': expected '" +
                options.expectedABIVersion + "', found '" + archive.manifest.abiVersion + "'.",
            Frontend::SourceLocation{ archive.manifest.name + ".rxp", 1, 1, 1 },
            "Install a package built for '" + options.expectedABIVersion + "'.");
        ok = false;
    }

    // 3. Verify .rxapi SHA-256 integrity (P8002) and Explain validity (P8001)
    const std::string actualAPIHash = StdLib::CryptoEngine::ComputeSHA256Hex(archive.rxAPIContent);
    if (!StdLib::SecretBuffer::ConstantTimeEquals(actualAPIHash, archive.rxAPISha256Hex)) {
        diagnostics.ReportError(
            "P8002",
            "SHA-256 digest mismatch for '" + archive.manifest.name + ".rxapi'.",
            Frontend::SourceLocation{ archive.manifest.name + ".rxp", 1, 1, 1 },
            "The .rxapi interface was modified after packaging.");
        ok = false;
    }

    auto apiCheck = RxAPIEngine::ValidateRxAPI(
        archive.manifest.name + ".rxapi",
        archive.rxAPIContent,
        diagnostics);
    if (!apiCheck.valid) {
        ok = false;
    }

    // 4. Verify target binary availability (P8005) and binary SHA-256 digests (P8002)
    bool foundTargetBinary = options.requiredTargetPlatform.empty();
    for (const auto& bin : archive.binaries) {
        std::string_view payloadView(
            reinterpret_cast<const char*>(bin.binaryPayload.data()),
            bin.binaryPayload.size());
        const std::string actualBinHash = StdLib::CryptoEngine::ComputeSHA256Hex(payloadView);
        if (!StdLib::SecretBuffer::ConstantTimeEquals(actualBinHash, bin.sha256Hex)) {
            diagnostics.ReportError(
                "P8002",
                "SHA-256 digest mismatch for binary artifact '" + bin.artifactFileName +
                    "' (" + bin.targetPlatform + ").",
                Frontend::SourceLocation{ archive.manifest.name + ".rxp", 1, 1, 1 },
                "Re-download the untampered .rxp package.");
            ok = false;
        }
        if (bin.targetPlatform == options.requiredTargetPlatform) {
            foundTargetBinary = true;
        }
    }

    if (!foundTargetBinary) {
        diagnostics.ReportError(
            "P8005",
            "Package '" + archive.manifest.name + "' does not contain a compiled binary for target '" +
                options.requiredTargetPlatform + "'.",
            Frontend::SourceLocation{ archive.manifest.name + ".rxp", 1, 1, 1 },
            "Select a supported target architecture or install a package supporting '" + options.requiredTargetPlatform + "'.");
        ok = false;
    }

    // 5. Verify publisher cryptographic signature (P8003)
    const std::string expectedSig = StdLib::CryptoEngine::ComputeHMACSHA256Hex(
        options.trustedSigningKey,
        ComputeArchiveSigningPayload(archive));

    if (!StdLib::SecretBuffer::ConstantTimeEquals(expectedSig, archive.publisherSignatureHex)) {
        diagnostics.ReportError(
            "P8003",
            "Cryptographic signature verification failed for package '" + archive.manifest.name + "'.",
            Frontend::SourceLocation{ archive.manifest.name + ".rxp", 1, 1, 1 },
            "Verify the publisher signing key and package authenticity.");
        ok = false;
    }

    return ok;
}

} // namespace Robix::Packages

