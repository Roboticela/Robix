#include "RxApiAndPackageFormat.hpp"

#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Types/SpecValidator.hpp"
#include "../../Compiler/Types/TypeChecker.hpp"
#include "../../StdLib/Data/JsonAndDataFormats.hpp"
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

void EmitFunctionSignatureForRxApi(std::ostringstream& out, const Frontend::DeclNode& fn, const std::string& indent) {
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
        << "|api=" << archive.rxApiSha256Hex;
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

std::string RxApiEngine::GenerateRxApiFromSource(
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
            EmitFunctionSignatureForRxApi(out, decl, "    ");
        } else if (decl.kind == Frontend::DeclKind::Class || decl.kind == Frontend::DeclKind::Struct) {
            const char* kw = (decl.kind == Frontend::DeclKind::Class) ? "class" : "struct";
            out << "\n    public " << kw << " " << decl.name << " {\n";
            for (const auto& mPtr : decl.members) {
                if (!mPtr || mPtr->visibility != Spec::AccessVisibility::Public) continue;
                if (mPtr->kind == Frontend::DeclKind::Function) {
                    EmitFunctionSignatureForRxApi(out, *mPtr, "        ");
                }
            }
            out << "    }\n";
        }
    }

    out << "}\n";
    return out.str();
}

RxApiValidationResult RxApiEngine::ValidateRxApi(
    std::string_view fileName,
    std::string_view rxApiContent,
    Frontend::DiagnosticBag& diagnostics) {

    RxApiValidationResult res;
    res.apiSha256Hex = StdLib::CryptoEngine::ComputeSHA256Hex(rxApiContent);

    Frontend::Lexer lexer(rxApiContent, fileName, diagnostics);
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
    std::string rxApiContent,
    std::vector<TargetBinaryArtifact> targetBinaries,
    std::string_view publisherSigningKey,
    Frontend::DiagnosticBag& diagnostics) {

    RxPackageArchive archive;
    archive.manifest        = std::move(manifest);
    archive.rxApiContent   = std::move(rxApiContent);
    archive.rxApiSha256Hex = StdLib::CryptoEngine::ComputeSHA256Hex(archive.rxApiContent);

    // Validate the .rxapi before packaging
    auto apiVal = RxApiEngine::ValidateRxApi(
        archive.manifest.name + ".rxapi",
        archive.rxApiContent,
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
         << "\",\"apiSha256\":\"" << archive.rxApiSha256Hex << "\"}";
    archive.sbomJson = sbom.str();

    // Sign the archive using HMAC-SHA256 with the publisher key
    archive.publisherSignatureHex = StdLib::CryptoEngine::ComputeHMACSHA256Hex(
        publisherSigningKey,
        ComputeArchiveSigningPayload(archive));

    return archive;
}

std::string RxPackagePacker::SerializeArchive(const RxPackageArchive& archive) {
    using namespace StdLib;
    JsonValue root = JsonValue::MakeObject();
    root.objectVal["name"]                  = JsonValue::MakeString(archive.manifest.name);
    root.objectVal["version"]               = JsonValue::MakeString(archive.manifest.version);
    root.objectVal["compilerVersion"]       = JsonValue::MakeString(archive.manifest.compilerVersion);
    root.objectVal["abiVersion"]            = JsonValue::MakeString(archive.manifest.abiVersion);
    root.objectVal["license"]               = JsonValue::MakeString(archive.manifest.license);
    root.objectVal["hasInstallScript"]      = JsonValue::MakeBool(archive.manifest.hasInstallScript);
    root.objectVal["rxApiContent"]         = JsonValue::MakeString(archive.rxApiContent);
    root.objectVal["rxApiSha256Hex"]       = JsonValue::MakeString(archive.rxApiSha256Hex);
    root.objectVal["documentationMarkdown"] = JsonValue::MakeString(archive.documentationMarkdown);
    root.objectVal["sbomJson"]              = JsonValue::MakeString(archive.sbomJson);
    root.objectVal["publisherSignatureHex"] = JsonValue::MakeString(archive.publisherSignatureHex);

    JsonValue permsArr = JsonValue::MakeArray();
    for (const auto& p : archive.manifest.permissions) {
        permsArr.arrayVal.push_back(JsonValue::MakeString(p));
    }
    root.objectVal["permissions"] = std::move(permsArr);

    JsonValue depsObj = JsonValue::MakeObject();
    for (const auto& [depName, depVer] : archive.manifest.dependencies) {
        depsObj.objectVal[depName] = JsonValue::MakeString(depVer);
    }
    root.objectVal["dependencies"] = std::move(depsObj);

    JsonValue binsArr = JsonValue::MakeArray();
    for (const auto& bin : archive.binaries) {
        JsonValue bObj = JsonValue::MakeObject();
        bObj.objectVal["targetPlatform"]   = JsonValue::MakeString(bin.targetPlatform);
        bObj.objectVal["artifactFileName"] = JsonValue::MakeString(bin.artifactFileName);
        bObj.objectVal["sha256Hex"]        = JsonValue::MakeString(bin.sha256Hex);
        std::string payloadStr(reinterpret_cast<const char*>(bin.binaryPayload.data()), bin.binaryPayload.size());
        bObj.objectVal["payload"]          = JsonValue::MakeString(std::move(payloadStr));
        binsArr.arrayVal.push_back(std::move(bObj));
    }
    root.objectVal["binaries"] = std::move(binsArr);

    return JsonCodec::Serialize(root, JsonFormatOption::Pretty);
}

std::optional<RxPackageArchive> RxPackagePacker::DeserializeArchive(
    std::string_view serialized,
    Frontend::DiagnosticBag& diagnostics) {

    using namespace StdLib;
    std::string parseErr;
    auto rootOpt = JsonCodec::TryParse(serialized, &parseErr);
    if (!rootOpt.has_value() || rootOpt->kind != JsonKind::Object) {
        diagnostics.ReportError(
            "P8002",
            "Corrupted .rxp package archive: " + parseErr,
            Frontend::SourceLocation{});
        return std::nullopt;
    }

    const JsonValue& root = *rootOpt;
    RxPackageArchive archive;
    if (const auto* v = root.GetField("name"))                  archive.manifest.name            = v->strVal;
    if (const auto* v = root.GetField("version"))               archive.manifest.version         = v->strVal;
    if (const auto* v = root.GetField("compilerVersion"))       archive.manifest.compilerVersion = v->strVal;
    if (const auto* v = root.GetField("abiVersion"))            archive.manifest.abiVersion      = v->strVal;
    if (const auto* v = root.GetField("license"))               archive.manifest.license         = v->strVal;
    if (const auto* v = root.GetField("hasInstallScript"))      archive.manifest.hasInstallScript = v->boolVal;
    if (const auto* v = root.GetField("rxApiContent"))         archive.rxApiContent            = v->strVal;
    if (const auto* v = root.GetField("rxApiSha256Hex"))       archive.rxApiSha256Hex          = v->strVal;
    if (const auto* v = root.GetField("documentationMarkdown")) archive.documentationMarkdown    = v->strVal;
    if (const auto* v = root.GetField("sbomJson"))              archive.sbomJson                 = v->strVal;
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
    const std::string actualApiHash = StdLib::CryptoEngine::ComputeSHA256Hex(archive.rxApiContent);
    if (!StdLib::SecretBuffer::ConstantTimeEquals(actualApiHash, archive.rxApiSha256Hex)) {
        diagnostics.ReportError(
            "P8002",
            "SHA-256 digest mismatch for '" + archive.manifest.name + ".rxapi'.",
            Frontend::SourceLocation{ archive.manifest.name + ".rxp", 1, 1, 1 },
            "The .rxapi interface was modified after packaging.");
        ok = false;
    }

    auto apiCheck = RxApiEngine::ValidateRxApi(
        archive.manifest.name + ".rxapi",
        archive.rxApiContent,
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

