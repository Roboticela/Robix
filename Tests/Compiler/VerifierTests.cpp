#include "../../StdLib/Testing/TestingFramework.hpp"
#include "../../Tools/Verifier/RobixVerifier.hpp"

#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

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

void TestAssertStandardFramework() {
    std::cout << "\n--- 1. Robix.Testing Assert Framework (All 12 Official Methods) ---\n";
    using Robix::Testing::Assert;
    Assert::ResetStats();

    // 1 & 2: Equal & NotEqual
    Check(Assert::Equal(42, 42, "Int equality"), "Assert.Equal passes on matching values");
    Check(Assert::NotEqual(42, 99, "Int inequality"), "Assert.NotEqual passes on distinct values");

    // 3 & 4: True & False
    Check(Assert::True(10 > 5, "10 > 5"), "Assert.True passes on true condition");
    Check(Assert::False(10 < 5, "10 < 5"), "Assert.False passes on false condition");

    // 5 & 6: Null & NotNull
    std::optional<int> emptyOpt = std::nullopt;
    std::optional<int> fullOpt  = 77;
    Check(Assert::Null(emptyOpt) && Assert::Null(static_cast<const void*>(nullptr)),
          "Assert.Null passes on null optional and nullptr");
    Check(Assert::NotNull(fullOpt) && Assert::NotNull(&fullOpt),
          "Assert.NotNull passes on populated optional and non-null pointer");

    // 7 & 8: Throws & ThrowsAsync
    Check(Assert::Throws([]() { throw std::runtime_error("MotorOvercurrentError"); }, "MotorOvercurrent"),
          "Assert.Throws catches synchronous exception and validates message substring");

    auto asyncTask = std::async(std::launch::async, []() {
        throw std::runtime_error("SensorTimeoutError");
    });
    Check(Assert::ThrowsAsync(asyncTask, "SensorTimeout"),
          "Assert.ThrowsAsync catches asynchronous future exception and validates message substring");

    // 9 & 10: Success & Error
    Check(Assert::Success(true, ""), "Assert.Success passes on successful Result");
    Check(Assert::Error(false, "I2CBusDisconnected", "I2CBus"),
          "Assert.Error passes on failed Result with expected error substring");

    // 11: Contains (string and vector)
    Check(Assert::Contains("Roboticela Robix Language", "Robix") &&
          Assert::Contains(std::vector<int>{ 10, 20, 30 }, 20),
          "Assert.Contains validates both string and collection membership");

    // 12: Timeout
    Check(Assert::Timeout(500, []() {
        volatile int sum = 0;
        for (int i = 0; i < 1000; ++i) sum += i;
    }), "Assert.Timeout verifies execution completes within millisecond budget");

    Check(Assert::PassedCount() == 15 && Assert::FailedCount() == 0,
          "Assert statistics tracker records all 15 passed assertions with 0 failures");
}

void TestLanguageLevelAssertAndTestFileSyntax() {
    std::cout << "\n--- 2. Language-Level Test & TestFile Syntax + Assert.* Lowering ---\n";

    const std::string prodSource = R"(
package Robotics.Math;

public Int ComputeTorque(Int currentAmps, Int ktConstant) {
    Explain {
        Description: "Computes motor torque from current and Kt constant.";
        Input: { currentAmps: "Motor current", ktConstant: "Torque constant" }
        Processing: "Multiplies currentAmps by ktConstant.";
        Output: "Motor torque value.";
    }
    return currentAmps * ktConstant;
}
)";

    const std::string testSource = R"(
Test "Motor torque and all Assert.* methods in .test.rx" {
    let torque = ComputeTorque(6, 7);
    Assert.Equal(torque, 42);
    Assert.NotEqual(torque, 0);
    Assert.True(torque > 40);
    Assert.False(torque < 0);
    Assert.Null(null);
    Assert.NotNull(torque);
    Assert.Contains("MotorTorque=42", "Torque=42");
    Assert.Success(Success(torque));
    Assert.Error(Error("StallDetected"));
    Assert.Throws(Error("OverVoltage"));
    Assert.ThrowsAsync(Error("AsyncTimeout"));
    Assert.Timeout(100);
}
)";

    Robix::Frontend::DiagnosticBag diag;
    auto res = Robix::Testing::MandatoryTestEnforcer::RunSingleFileTest(
        "Torque.rx",
        prodSource,
        "Torque.test.rx",
        testSource,
        Robix::Testing::TestExecutionEngine::Differential,
        diag);

    Check(res.compiledCleanly && !diag.HasErrors(),
          "Production + Test compilation unit with all 12 Assert.* methods compiles cleanly");
    Check(res.testBlocksExecuted == 1 && res.assertionsPassed == 12 && res.assertionsFailed == 0,
          "BytecodeVM executes Test block and passes all 12 first-class Assert.* assertions");
    Check(res.nativeParityMatch && !res.generatedLLVMIR.empty(),
          "Differential engine confirms Native LLVM IR code generation parity for Test blocks");
}

void TestMandatoryPerFileEnforcementR5001() {
    std::cout << "\n--- 3. Mandatory Per-File .test.rx Enforcement (R5001) ---\n";

    const std::string motorSource = R"(
package Robotics.Motors;

public Int ClampPWM(Int duty) {
    Explain {
        Description: "Clamps PWM duty cycle to [0, 100].";
        Input: { duty: "Requested PWM duty" }
        Processing: "Bounds duty between 0 and 100.";
        Output: "Clamped PWM duty.";
    }
    if (duty < 0) { return 0; }
    if (duty > 100) { return 100; }
    return duty;
}
)";

    // Case A: Missing .test.rx file -> Blocked with R5001
    {
        Robix::Frontend::DiagnosticBag diag;
        Robix::Testing::TestEnforcementConfig cfg;
        auto cov = Robix::Testing::MandatoryTestEnforcer::EnforceProjectTests(
            { { "Motor.rx", motorSource, "", "" } },
            cfg,
            diag);

        Check(cov.size() == 1 && !cov[0].passedEnforcement && diag.ContainsCode("R5001"),
              "Missing Motor.test.rx blocks build with ROBIX TEST ERROR R5001");
    }

    // Case B: Empty .test.rx file (0 Test blocks) -> Blocked with R5001
    {
        Robix::Frontend::DiagnosticBag diag;
        Robix::Testing::TestEnforcementConfig cfg;
        auto cov = Robix::Testing::MandatoryTestEnforcer::EnforceProjectTests(
            { { "Motor.rx", motorSource, "Motor.test.rx", "package Robotics.Motors;" } },
            cfg,
            diag);

        Check(cov.size() == 1 && !cov[0].passedEnforcement && diag.ContainsCode("R5001"),
              "Motor.test.rx with 0 Test blocks blocks build with ROBIX TEST ERROR R5001");
    }

    // Case C: Unrelated .test.rx file that does not exercise Motor.rx -> Blocked with R5001
    {
        Robix::Frontend::DiagnosticBag diag;
        Robix::Testing::TestEnforcementConfig cfg;
        const std::string unrelatedTest = R"(
Test "Unrelated arithmetic check" {
    Assert.Equal(2 + 2, 4);
}
)";
        auto cov = Robix::Testing::MandatoryTestEnforcer::EnforceProjectTests(
            { { "Motor.rx", motorSource, "Motor.test.rx", unrelatedTest } },
            cfg,
            diag);

        Check(cov.size() == 1 && !cov[0].passedEnforcement && diag.ContainsCode("R5001"),
              "Motor.test.rx that does not reference any symbol from Motor.rx is rejected with R5001");
    }

    // Case D: Failing assertion inside .test.rx -> Blocked with R5001
    {
        Robix::Frontend::DiagnosticBag diag;
        Robix::Testing::TestEnforcementConfig cfg;
        const std::string failingTest = R"(
Test "Broken expectation for ClampPWM" {
    Assert.Equal(ClampPWM(150), 999);
}
)";
        auto cov = Robix::Testing::MandatoryTestEnforcer::EnforceProjectTests(
            { { "Motor.rx", motorSource, "Motor.test.rx", failingTest } },
            cfg,
            diag);

        Check(cov.size() == 1 && !cov[0].passedEnforcement && diag.ContainsCode("R5001"),
              "Failing test assertion in Motor.test.rx blocks build with R5001");
    }

    // Case E: Declaration/constants-only file covered via TestFile { Covers = "Constants.rx"; } -> Passes!
    {
        Robix::Frontend::DiagnosticBag diag;
        Robix::Testing::TestEnforcementConfig cfg;
        const std::string constantsSource = R"(
package Robotics.Config;
const Int MAX_RPM = 6000;
)";
        const std::string constantsTest = R"(
TestFile "Constants.test.rx" {
    Covers = "Constants.rx";
    Test "Validate MAX_RPM constant" {
        Assert.Equal(6000, 6000);
    }
}
)";
        auto cov = Robix::Testing::MandatoryTestEnforcer::EnforceProjectTests(
            { { "Constants.rx", constantsSource, "Constants.test.rx", constantsTest } },
            cfg,
            diag);

        Check(cov.size() == 1 && cov[0].passedEnforcement && cov[0].hasExplicitCoversDecl && !diag.HasErrors(),
              "TestFile { Covers = \"Constants.rx\"; } satisfies R5001 for declaration/constant files");
    }
}

void TestAdvancedTestingEngines() {
    std::cout << "\n--- 4. Advanced Testing (Negative, Property Shrinking, Fuzz, Mutation, Stress, Leak, Snapshot) ---\n";

    // 1. Negative Compilation Tests
    Check(Robix::Testing::MandatoryTestEnforcer::RunNegativeCompilationTest(
              "public Void Bad() { Int x = \"not an int\"; }", "R1001"),
          "Negative compilation test verifies type mismatch triggers R1001");

    Check(Robix::Testing::MandatoryTestEnforcer::RunNegativeCompilationTest(
              "public Void BadNull() { String? s = null; Print(s.Length); }", "R1005"),
          "Negative compilation test verifies unguarded nullable access triggers R1005");

    // 2. Property-Based Testing with Counterexample Shrinking
    auto propPass = Robix::Testing::MandatoryTestEnforcer::RunPropertyBasedTest(
        12345ULL, 100, -500, 500,
        [](std::int64_t x) { return (x + 0) == x; });
    Check(propPass.passed && propPass.iterationsRun == 100,
          "Property-based test passes 100 generated inputs when invariant holds");

    auto propShrink = Robix::Testing::MandatoryTestEnforcer::RunPropertyBasedTest(
        99999ULL, 200, 0, 1000,
        [](std::int64_t x) { return x < 17; });
    Check(!propShrink.passed && propShrink.shrunkMinimalInput == 17,
          "Property-based test automatically shrinks failing random input down to exact minimal counterexample (17)");

    // 3. Frontend Fuzz Testing
    const std::string seedCode = R"(
package Fuzz.Sample;
public Int Add(Int a, Int b) { return a + b; }
)";
    auto fuzzRes = Robix::Testing::MandatoryTestEnforcer::RunFrontendFuzzTest(42ULL, 150, seedCode);
    Check(fuzzRes.passed && fuzzRes.mutatedInputsTested == 150 && fuzzRes.crashesOrHangs == 0,
          "Frontend fuzzer executes 150 syntax-mutated inputs with 0 crashes or hangs");

    // 4. Mutation Testing
    const std::string mathSrc = R"(
public Int CalculateEnergy(Int mass, Int velocitySq) {
    if (mass > 0) {
        return mass * velocitySq + 10;
    }
    return 0;
}
)";
    const std::string mathTest = R"(
Test "Kills arithmetic and comparison mutants" {
    Assert.Equal(CalculateEnergy(4, 5), 30);
    Assert.Equal(CalculateEnergy(0, 5), 0);
}
)";
    auto mutRes = Robix::Testing::MandatoryTestEnforcer::RunMutationTest(
        "Energy.rx", mathSrc, "Energy.test.rx", mathTest, 80.0);
    Check(mutRes.totalMutantsGenerated >= 3 && mutRes.mutantsKilled == mutRes.totalMutantsGenerated &&
          mutRes.mutationScorePercent == 100.0 && mutRes.passedThreshold,
          "Mutation test engine generates operator mutants and confirms 100% mutant kill score");

    // 5. Multi-Threaded Stress Testing (--stress --repeat 32)
    auto stressRes = Robix::Testing::MandatoryTestEnforcer::RunStressTest(mathSrc, mathTest, 32, 4);
    Check(stressRes.passed && stressRes.successfulRuns == 32 && stressRes.deterministicOutput,
          "Stress test engine runs 32 concurrent iterations across 4 threads with 100% deterministic output");

    // 6. Memory Leak & Ownership Verification (--memory-check)
    {
        Robix::Frontend::DiagnosticBag cleanDiag;
        auto cleanLeakRes = Robix::Testing::MandatoryTestEnforcer::RunMemoryLeakCheckTest(
            [](Robix::Runtime::UnifiedMemoryManager& mem) {
                void* p = mem.ManualAllocate(256, 16);
                mem.ManualFree(p);
            },
            cleanDiag);
        Check(cleanLeakRes.passedZeroLeaks && !cleanDiag.HasErrors(),
              "Memory leak check passes when all manual allocations are freed before exit");

        Robix::Frontend::DiagnosticBag leakDiag;
        auto leakedRes = Robix::Testing::MandatoryTestEnforcer::RunMemoryLeakCheckTest(
            [](Robix::Runtime::UnifiedMemoryManager& mem) {
                (void)mem.ManualAllocate(128, 16); // Intentionally leaked!
            },
            leakDiag);
        Check(!leakedRes.passedZeroLeaks && leakedRes.leakedBytesAfterRun == 128 && leakDiag.ContainsCode("R5001"),
              "Memory leak check detects 128-byte leak and reports ROBIX TEST ERROR R5001");
    }

    // 7. Snapshot Testing
    {
        Robix::Frontend::DiagnosticBag snapDiag;
        Check(Robix::Testing::MandatoryTestEnforcer::RunSnapshotTest(
                  "DashboardSnapshot", "<Window Title=\"Robix\"/>", "<Window Title=\"Robix\"/>", snapDiag),
              "Snapshot test passes when actual output matches golden snapshot");
        Check(!Robix::Testing::MandatoryTestEnforcer::RunSnapshotTest(
                  "DashboardSnapshot", "<Window Title=\"Robix\"/>", "<Window Title=\"Broken\"/>", snapDiag) &&
              snapDiag.ContainsCode("R5001"),
              "Snapshot test reports R5001 with expected/actual diff when output diverges");
    }
}

void TestRobixVerifySingleCommandEngine() {
    std::cout << "\n--- 5. Single-Command Verifier (`robix verify` Project & Single-File Modes) ---\n";

    const std::string validController = R"(
package Robotics.Controllers;

public class PIDController {
    public Int ComputeStep(Int targetValue, Int measuredValue) {
        Explain {
            Description: "Computes proportional control step from target and measured values.";
            Input: { targetValue: "Desired setpoint", measuredValue: "Current sensor reading" }
            Processing: "Subtracts measuredValue from targetValue and scales by gain 2.";
            Output: "Control output command.";
        }
        let errorDelta = targetValue - measuredValue;
        return errorDelta * 2;
    }
}
)";

    const std::string validControllerTest = R"(
package Robotics.Controllers;

Test "PIDController ComputeStep proportional gain" {
    let pid = new PIDController();
    Assert.Equal(pid.ComputeStep(100, 85), 30);
    Assert.Equal(pid.ComputeStep(50, 50), 0);
}
)";

    // 1. Full Project Verification (All 8 checks pass -> exitCode == 0)
    auto projReport = Robix::Tools::RobixVerifier::VerifyProject(
        "RoboticsControlProject",
        { { "PIDController.rx", validController, "PIDController.test.rx", validControllerTest } });

    Check(projReport.AllChecksPassed() && projReport.exitCode == 0 &&
          projReport.syntaxCheckPassed && projReport.typeCheckPassed &&
          projReport.explainCheckPassed && projReport.namingStyleCheckPassed &&
          projReport.mandatoryTestFileCheckPassed && projReport.testExecutionPassed &&
          projReport.staticAnalysisPassed && projReport.memorySafetyAuditPassed &&
          projReport.totalAssertionsPassed == 2 && projReport.averageSymbolCoveragePercent == 100.0,
          "RobixVerifier::VerifyProject passes all 8 verification stages with exitCode 0 and 100% symbol coverage");

    // 2. Project Verification Failure when Explain block is missing and .test.rx is missing
    const std::string undocumentedSource = R"(
package Robotics.Broken;
public Int MissingExplainFn(Int x) {
    return x + 1;
}
)";
    auto failedProjReport = Robix::Tools::RobixVerifier::VerifyProject(
        "BrokenProject",
        { { "BrokenFile.rx", undocumentedSource, "", "" } });

    Check(!failedProjReport.AllChecksPassed() && failedProjReport.exitCode == 1 &&
          !failedProjReport.explainCheckPassed && !failedProjReport.mandatoryTestFileCheckPassed,
          "RobixVerifier::VerifyProject catches missing Explain (R3001) and missing .test.rx (R5001) with exitCode 1");

    // 3. Single-File .rx Verification (`robix verify PIDController.rx`)
    auto singleRbxReport = Robix::Tools::RobixVerifier::VerifySingleFile(
        "PIDController.rx",
        validController);
    Check(singleRbxReport.isSingleFileMode && singleRbxReport.AllChecksPassed() && singleRbxReport.exitCode == 0,
          "RobixVerifier::VerifySingleFile verifies standalone .rx file in isolation");

    // 4. Single-File .rxu UI Markup Verification (`robix verify MainWindow.rxu`)
    const std::string validRbxu = R"(
<?xml version="1.0"?>
<RobixUI Version="1.0">
    <Window Name="Dashboard" Title="Roboticela Control" Width="800" Height="600">
        <Column Spacing="12">
            <Text Value="Ready" FontSize="20"/>
        </Column>
    </Window>
</RobixUI>
)";
    auto singleRbxuReport = Robix::Tools::RobixVerifier::VerifySingleFile("MainWindow.rxu", validRbxu);
    Check(singleRbxuReport.isSingleFileMode && singleRbxuReport.AllChecksPassed() && singleRbxuReport.exitCode == 0,
          "RobixVerifier::VerifySingleFile verifies standalone .rxu UI document in isolation");
}

} // namespace

int main() {
    std::cout << "================================================================\n";
    std::cout << "Robix Stage 6 Mandatory Testing & Verification Engine Suite\n";
    std::cout << "================================================================\n";

    TestAssertStandardFramework();
    TestLanguageLevelAssertAndTestFileSyntax();
    TestMandatoryPerFileEnforcementR5001();
    TestAdvancedTestingEngines();
    TestRobixVerifySingleCommandEngine();

    std::cout << "\n================================================================\n";
    std::cout << "Stage 6 Results: " << g_passed << " passed, " << g_failed << " failed.\n";
    std::cout << "================================================================\n";

    return (g_failed == 0) ? 0 : 1;
}

