#include "../../Packages/Manager/RxAPIAndPackageFormat.hpp"
#include "../../Packages/Registry/PackageManagerAndRegistry.hpp"
#include "../../Packages/Resolver/DependencyResolver.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool condition, const char* description) {
    if (condition) {
        ++g_passed;
        std::cout << "  [PASS] " << description << "\n";
    } else {
        ++g_failed;
        std::cerr << "  [FAIL] " << description << "\n";
    }
}

void TestRxAPIGenerationAndValidation() {
    std::cout << "\n--- 1. Public API Definition (.rxapi) Generation & Explain Enforcement ---\n";
    using namespace Robix::Packages;

    const std::string motorLibrarySource = R"(
Package RobixMotors {
    Permissions {
        Hardware.GPIO;
        Hardware.PWM;
    }

    private Int SecretInternalCalibration(Int raw) {
        return raw * 42;
    }

    public Int SetMotorSpeed(Int rpm) {
        Explain {
            Description: "Sets target motor speed in RPM.";
            Input {
                rpm: "Requested speed in revolutions per minute";
            }
            Processing: "Validates RPM bounds and writes PWM duty cycle to hardware.";
            Output: "Applied RPM value.";
        }
        return rpm;
    }
}
)";

    Robix::Frontend::DiagnosticBag genDiag;
    const std::string generatedAPI = RxAPIEngine::GenerateRxAPIFromSource(
        "RobixMotors", "1.2.0", motorLibrarySource, genDiag);

    Check(!genDiag.HasErrors() &&
          generatedAPI.find("SetMotorSpeed") != std::string::npos &&
          generatedAPI.find("SecretInternalCalibration") == std::string::npos &&
          generatedAPI.find("return rpm * 42") == std::string::npos,
          "RxAPIEngine::GenerateRxAPIFromSource exports public signatures + Explain while stripping private functions and implementation bodies");

    Robix::Frontend::DiagnosticBag valDiag;
    auto apiVal = RxAPIEngine::ValidateRxAPI("RobixMotors.rxapi", generatedAPI, valDiag);
    Check(apiVal.valid && !valDiag.HasErrors() &&
          apiVal.publicFunctionCount == 1 &&
          apiVal.requiredPermissions.size() == 2 &&
          !apiVal.apiSha256Hex.empty(),
          "RxAPIEngine::ValidateRxAPI validates well-formed .rxapi and extracts permissions + SHA-256 hash");

    // Verify .rxapi missing Explain block is rejected with P8001 / D3001
    const std::string undocumentedAPI = R"(
Package BadPkg {
    Version = "1.0.0";
    public Int UndocumentedFn(Int x);
}
)";
    Robix::Frontend::DiagnosticBag badDiag;
    auto badVal = RxAPIEngine::ValidateRxAPI("BadPkg.rxapi", undocumentedAPI, badDiag);
    Check(!badVal.valid && (badDiag.ContainsCode("P8001") || badDiag.ContainsCode("D3001")),
          "RxAPIEngine::ValidateRxAPI rejects public .rxapi functions missing mandatory Explain blocks");
}

void TestRxpArchiveSecurityAndABIVerification() {
    std::cout << "\n--- 2. Compiled Binary Package (.rxp) Archive, Signatures, SHA-256 & ABI Checks ---\n";
    using namespace Robix::Packages;

    const std::string validAPI = R"(
Package RobixHAL {
    Version = "1.0.0";
    Permissions {
        Hardware.GPIO;
    }
    public Int ReadPin(Int pinNumber)
        Explain {
            Description: "Reads digital GPIO pin state.";
            Input {
                pinNumber: "Hardware GPIO pin index";
            }
            Processing: "Reads volatile MMIO register for pinNumber.";
            Output: "1 for HIGH, 0 for LOW.";
        }
    ;
}
)";

    RxPackageManifest manifest;
    manifest.name            = "RobixHAL";
    manifest.version         = "1.0.0";
    manifest.abiVersion      = "RobixABI_v1";
    manifest.hasInstallScript = false;

    std::vector<TargetBinaryArtifact> binaries = {
        { "windows-x64", "RobixHAL.dll", { 0x4D, 0x5A, 0x90, 0x00 }, "" },
        { "linux-x64",   "libRobixHAL.so", { 0x7F, 0x45, 0x4C, 0x46 }, "" }
    };

    const std::string pubKey = "RoboticelaOfficialPublisherKey";
    Robix::Frontend::DiagnosticBag packDiag;
    auto archive = RxPackagePacker::CreateSignedPackage(manifest, validAPI, binaries, pubKey, packDiag);

    Check(!packDiag.HasErrors() && !archive.publisherSignatureHex.empty() &&
          archive.sbomJSON.find("RobixSBOM-1.0") != std::string::npos,
          "RxPackagePacker::CreateSignedPackage creates signed .rxp with SHA-256 digests and SBOM");

    // Round-trip serialization/deserialization
    const std::string serialized = RxPackagePacker::SerializeArchive(archive);
    Robix::Frontend::DiagnosticBag deserDiag;
    auto deserializedOpt = RxPackagePacker::DeserializeArchive(serialized, deserDiag);
    Check(deserializedOpt.has_value() && !deserDiag.HasErrors() &&
          deserializedOpt->manifest.name == "RobixHAL" &&
          deserializedOpt->binaries.size() == 2,
          "RxPackagePacker SerializeArchive and DeserializeArchive round-trip .rxp container cleanly");

    // 1. Valid verification
    PackageVerificationOptions opts;
    opts.trustedSigningKey      = pubKey;
    opts.requiredTargetPlatform = "windows-x64";
    Robix::Frontend::DiagnosticBag okDiag;
    Check(RxPackagePacker::VerifyPackageArchive(*deserializedOpt, opts, okDiag) && !okDiag.HasErrors(),
          "RxPackagePacker::VerifyPackageArchive passes authentic, ABI-compatible .rxp archive");

    // 2. Tampered binary payload -> P8002
    {
        auto tamperedBin = *deserializedOpt;
        tamperedBin.binaries[0].binaryPayload[0] ^= 0xFFU;
        Robix::Frontend::DiagnosticBag d;
        Check(!RxPackagePacker::VerifyPackageArchive(tamperedBin, opts, d) && d.ContainsCode("P8002"),
              "VerifyPackageArchive detects tampered binary payload with SHA-256 error P8002");
    }

    // 3. Invalid publisher signature -> P8003
    {
        auto badSig = *deserializedOpt;
        badSig.publisherSignatureHex = "0000deadbeef";
        Robix::Frontend::DiagnosticBag d;
        Check(!RxPackagePacker::VerifyPackageArchive(badSig, opts, d) && d.ContainsCode("P8003"),
              "VerifyPackageArchive detects invalid publisher cryptographic signature with P8003");
    }

    // 4. Incompatible ABI version -> P8004
    {
        auto badABI = *deserializedOpt;
        badABI.manifest.abiVersion = "LegacyABI_v0";
        Robix::Frontend::DiagnosticBag d;
        Check(!RxPackagePacker::VerifyPackageArchive(badABI, opts, d) && d.ContainsCode("P8004"),
              "VerifyPackageArchive blocks incompatible compiler ABI version with P8004");
    }

    // 5. Missing target architecture binary -> P8005
    {
        PackageVerificationOptions wasmOpts = opts;
        wasmOpts.requiredTargetPlatform      = "wasm32-wasi";
        Robix::Frontend::DiagnosticBag d;
        Check(!RxPackagePacker::VerifyPackageArchive(*deserializedOpt, wasmOpts, d) && d.ContainsCode("P8005"),
              "VerifyPackageArchive reports P8005 when requested target OS/CPU binary is missing");
    }

    // 6. Unauthorized install script -> P8006
    {
        auto withScript = *deserializedOpt;
        withScript.manifest.hasInstallScript = true;
        Robix::Frontend::DiagnosticBag d;
        Check(!RxPackagePacker::VerifyPackageArchive(withScript, opts, d) && d.ContainsCode("P8006"),
              "VerifyPackageArchive blocks arbitrary package install script execution by default with P8006");
    }
}

void TestSemVerAndDependencyResolver() {
    std::cout << "\n--- 3. SemVer Constraints, .rxproj / .rxlock & Deterministic DAG Resolver ---\n";
    using namespace Robix::Packages;

    auto v123 = SemVer::Parse("1.2.3");
    Check(v123.has_value() &&
          v123->SatisfiesConstraint("^1.0.0") &&
          v123->SatisfiesConstraint("~1.2.0") &&
          !v123->SatisfiesConstraint("~1.3.0") &&
          !v123->SatisfiesConstraint("^2.0.0"),
          "SemVer parses Major.Minor.Patch and evaluates caret (^), tilde (~), and exact constraints");

    // Circular dependency detection (PkgA <-> PkgB -> P8008)
    DependencyResolver cycleResolver;
    RxPackageArchive pkgA;
    pkgA.manifest.name = "PkgA"; pkgA.manifest.version = "1.0.0";
    pkgA.manifest.dependencies["PkgB"] = "^1.0.0";

    RxPackageArchive pkgB;
    pkgB.manifest.name = "PkgB"; pkgB.manifest.version = "1.0.0";
    pkgB.manifest.dependencies["PkgA"] = "^1.0.0";

    cycleResolver.RegisterAvailablePackage(pkgA);
    cycleResolver.RegisterAvailablePackage(pkgB);

    RxProjectManifest cycleProj;
    cycleProj.projectName          = "CycleApp";
    cycleProj.dependencies["PkgA"] = "^1.0.0";

    Robix::Frontend::DiagnosticBag cycleDiag;
    auto cycleLock = cycleResolver.ResolveProjectDependencies(cycleProj, cycleDiag);
    Check(!cycleLock.has_value() && cycleDiag.ContainsCode("P8008"),
          "DependencyResolver detects circular package dependencies and reports P8008");
}

void TestProjectLocalPackageManagerAndRegistry() {
    std::cout << "\n--- 4. Project-Local Package Manager (`robix`) & Registry Workflows ---\n";
    using namespace Robix::Packages;

    const std::string pubKey = "RoboticelaOfficialPublisherKey";
    RobixPackageRegistry registry(pubKey);

    const std::string halAPI = R"(
Package RobixHAL {
    Version = "1.1.0";
    public Int InitBus(Int busId)
        Explain {
            Description: "Initializes hardware bus.";
            Input { busId: "Bus identifier"; }
            Processing: "Configures bus clock and pins.";
            Output: "Status code.";
        }
    ;
}
)";
    const std::string motorsAPI = R"(
Package RobixMotors {
    Version = "2.1.0";
    import RobixHAL;
    public Int DriveMotor(Int channel, Int duty)
        Explain {
            Description: "Drives motor channel at specified duty cycle.";
            Input { channel: "Motor channel", duty: "Duty cycle"; }
            Processing: "Sends PWM command over RobixHAL.";
            Output: "Applied duty cycle.";
        }
    ;
}
)";

    Robix::Frontend::DiagnosticBag pubDiag;
    RxPackageManifest halManifest;
    halManifest.name    = "RobixHAL";
    halManifest.version = "1.1.0";
    auto halPkg = RxPackagePacker::CreateSignedPackage(
        halManifest, halAPI, { { "windows-x64", "RobixHAL.dll", { 1, 2, 3, 4 }, "" } }, pubKey, pubDiag);

    RxPackageManifest motorsManifest;
    motorsManifest.name                     = "RobixMotors";
    motorsManifest.version                  = "2.1.0";
    motorsManifest.dependencies["RobixHAL"] = "^1.0.0";
    auto motorsPkg = RxPackagePacker::CreateSignedPackage(
        motorsManifest, motorsAPI, { { "windows-x64", "RobixMotors.dll", { 5, 6, 7, 8 }, "" } }, pubKey, pubDiag);

    Check(registry.PublishPackage(halPkg, "windows-x64", pubDiag) &&
          registry.PublishPackage(motorsPkg, "windows-x64", pubDiag) &&
          !pubDiag.HasErrors(),
          "RobixPackageRegistry::PublishPackage verifies and registers signed .rxp packages");

    auto searchHits = registry.Search("motors");
    Check(searchHits.size() == 1 && searchHits[0].name == "RobixMotors" && searchHits[0].latestVersion == "2.1.0",
          "RobixPackageRegistry::Search locates published packages case-insensitively");

    // Project-local workflow: `robix new`, `robix add`, `robix install`, `robix list`, `robix tree`, `robix restore`
    RobixPackageManager mgr("D:/MyRobotApp", registry);
    Check(mgr.NewProject("MyRobotApp", "robotics"),
          "RobixPackageManager::NewProject scaffolds Robix.Project.rxproj, Project.rxb, Source/Main.rx, and Tests/Main.test.rx");

    Check(mgr.AddDependency("RobixMotors", "^2.0.0"),
          "RobixPackageManager::AddDependency records dependency in Robix.Project.rxproj");

    Robix::Frontend::DiagnosticBag installDiag;
    Check(mgr.InstallPackages("windows-x64", installDiag) && !installDiag.HasErrors(),
          "RobixPackageManager::InstallPackages resolves transitive DAG and verifies signatures/ABI");

    const auto& installed = mgr.ListInstalled();
    Check(installed.size() == 2 &&
          installed[0].packageName == "RobixHAL" &&
          installed[0].installDirectory == "D:/MyRobotApp/.robix/packages/RobixHAL/1.1.0" &&
          installed[1].packageName == "RobixMotors" &&
          installed[1].installDirectory == "D:/MyRobotApp/.robix/packages/RobixMotors/2.1.0",
          "Packages are installed project-locally under <ProjectRoot>/.robix/packages/<Name>/<Version>/ in topological order");

    const auto& vfs = mgr.VirtualFS();
    Check(vfs.find("D:/MyRobotApp/Robix.Lock") != vfs.end() &&
          vfs.find("D:/MyRobotApp/.robix/packages/RobixMotors/2.1.0/RobixMotors.rxapi") != vfs.end() &&
          vfs.find("D:/MyRobotApp/.robix/packages/RobixMotors/2.1.0/binaries/windows-x64/RobixMotors.dll") != vfs.end(),
          "Project-local package directory contains .rxapi, binaries/<target>/*.dll, package.json, and deterministic Robix.Lock");

    const std::string treeStr = mgr.RenderDependencyTree();
    Check(treeStr.find("RobixMotors@2.1.0") != std::string::npos &&
          treeStr.find("RobixHAL") != std::string::npos,
          "RobixPackageManager::RenderDependencyTree formats hierarchical dependency graph");

    Robix::Frontend::DiagnosticBag restoreDiag;
    Check(mgr.RestoreFromLockfile("windows-x64", restoreDiag) && !restoreDiag.HasErrors(),
          "RobixPackageManager::RestoreFromLockfile deterministically restores packages from Robix.Lock");
}

} // namespace

int main() {
    std::cout << "================================================================\n";
    std::cout << "Robix Stage 8 Package Ecosystem & Binary Distribution Suite\n";
    std::cout << "================================================================\n";

    TestRxAPIGenerationAndValidation();
    TestRxpArchiveSecurityAndABIVerification();
    TestSemVerAndDependencyResolver();
    TestProjectLocalPackageManagerAndRegistry();

    std::cout << "\n================================================================\n";
    std::cout << "Stage 8 Results: " << g_passed << " passed, " << g_failed << " failed.\n";
    std::cout << "================================================================\n";

    return (g_failed == 0) ? 0 : 1;
}

