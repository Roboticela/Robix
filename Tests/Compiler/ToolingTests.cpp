#include "../../Tools/CLI/ExamplesAndDocumentationCatalog.hpp"
#include "../../Tools/CLI/RobixCLIAndTestGenerator.hpp"
#include "../../Tools/Debugger/LSPDebuggerProfilerAndBenchmarks.hpp"
#include "../../StdLib/Core/CoreTypesAndRegex.hpp"
#include "../../StdLib/Data/JSONAndDataFormats.hpp"

#include <filesystem>
#include <iostream>
#include <map>
#include <string>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool condition, const std::string& testName) {
    if (condition) {
        ++g_passed;
        std::cout << "  [PASS] " << testName << "\n";
    } else {
        ++g_failed;
        std::cout << "  [FAIL] " << testName << "\n";
    }
}

// ----------------------------------------------------------------------------
// 1. CLI Test Scaffold Generator (`robix test-gen`, `robix test --generate`, `robixc --generate-tests`)
// ----------------------------------------------------------------------------
void TestCLITestScaffoldGeneratorAndUnifiedCLI() {
    std::cout << "\n--- 1. CLI Automatic .test.rx Scaffold Generator & Unified CLI (`robix` / `robixc`) ---\n";
    using namespace Robix;

    std::map<std::string, std::string> projectFS;
    projectFS["Source/MotorDriver.rx"] = R"(
package Robotics.Drivers;

public class MotorDriver {
    public Function SetSpeed(Int rpm) -> Void {
    }
    public Function GetSpeed() -> Int {
        return 100;
    }
}

public Function EmergencyBrake() -> Int {
    return 0;
}
)";
    projectFS["Source/SensorHub.rx"] = R"(
package Robotics.Sensors;

public Function ReadTemperature() -> Int {
    return 25;
}
)";
    // Pre-populate an existing `.test.rx` file to verify idempotency (never overwrites existing tests)
    const std::string existingCustomTest = R"(package Robotics.Sensors;
import Robix.Testing;
test "CustomPreservedSensorTest" {
    Assert.Equal(25, 25);
}
)";
    projectFS["Source/SensorHub.test.rx"] = existingCustomTest;

    // 1. Run `robix test-gen`
    auto genRes = Tools::RobixCLIDispatcher::ExecuteCommand(
        Tools::CLIBinaryKind::RobixUnifiedCLI,
        { "test-gen" },
        &projectFS);

    Check(genRes.exitCode == 0 &&
          genRes.generatedTests.createdTestFiles.size() == 1 &&
          genRes.generatedTests.createdTestFiles[0] == "Source/MotorDriver.test.rx" &&
          genRes.generatedTests.skippedExistingTestFiles.size() == 1 &&
          genRes.generatedTests.skippedExistingTestFiles[0] == "Source/SensorHub.test.rx",
          "`robix test-gen` creates missing Source/MotorDriver.test.rx and skips existing Source/SensorHub.test.rx");

    Check(projectFS["Source/SensorHub.test.rx"] == existingCustomTest,
          "`robix test-gen` preserves existing .test.rx file content untouched");

    const std::string& generatedMotorTest = projectFS["Source/MotorDriver.test.rx"];
    Check(StdLib::StringOps::Contains(generatedMotorTest, "package Robotics.Drivers;") &&
          StdLib::StringOps::Contains(generatedMotorTest, "import Robix.Testing;") &&
          StdLib::StringOps::Contains(generatedMotorTest, "Test \"MotorDriver Construction\"") &&
          StdLib::StringOps::Contains(generatedMotorTest, "Test \"MotorDriver.SetSpeed\"") &&
          StdLib::StringOps::Contains(generatedMotorTest, "Test \"MotorDriver.GetSpeed\"") &&
          StdLib::StringOps::Contains(generatedMotorTest, "Test \"MotorDriver.EmergencyBrake\""),
          "Generated .test.rx contains empty test blocks for every class, method, and function in source file");

    // 2. Verify that generated empty `.test.rx` compiles and passes `robix test Source/MotorDriver.test.rx`
    auto testRunRes = Tools::RobixCLIDispatcher::ExecuteCommand(
        Tools::CLIBinaryKind::RobixUnifiedCLI,
        { "test", "Source/MotorDriver.test.rx" },
        &projectFS);
    Check(testRunRes.exitCode == 0 &&
          StdLib::StringOps::Contains(testRunRes.stdoutText, "4 passed, 0 failed"),
          "Generated empty .test.rx file compiles and executes cleanly via `robix test`");

    // 3. Verify `robix test --generate` and `robixc --generate-tests` on disk in Build/TempScaffoldTest
    const std::filesystem::path tempDir = "Build/TempScaffoldTest";
    std::filesystem::remove_all(tempDir);
    std::filesystem::create_directories(tempDir);
    const std::filesystem::path diskRxPath  = tempDir / "ArmController.rx";
    const std::filesystem::path diskTestPath = tempDir / "ArmController.test.rx";

    StdLib::CrossPlatformIO::WriteAllText(diskRxPath.generic_string(), R"(
package Robotics.Arm;
public Function HomeJoints() -> Int {
    return 1;
}
)");

    auto diskGenRes = Tools::RobixCLIDispatcher::ExecuteCommand(
        Tools::CLIBinaryKind::RobixNativeCompiler,
        { "--generate-tests", tempDir.generic_string() });

    Check(diskGenRes.exitCode == 0 &&
          std::filesystem::exists(diskTestPath) &&
          diskGenRes.generatedTests.createdTestFiles.size() == 1,
          "`robixc --generate-tests` creates ArmController.test.rx on disk when missing");

    // Running a second time on disk skips existing file
    auto diskGenSecond = Tools::RobixCLIDispatcher::ExecuteCommand(
        Tools::CLIBinaryKind::RobixUnifiedCLI,
        { "generate-tests", tempDir.generic_string() });
    Check(diskGenSecond.generatedTests.createdTestFiles.empty() &&
          diskGenSecond.generatedTests.skippedExistingTestFiles.size() == 1,
          "`robix generate-tests` on disk is idempotent and never overwrites existing .test.rx");
    std::filesystem::remove_all(tempDir);

    // 4. Verify `robix repl` and `robixc` LLVM IR emission
    auto replRes = Tools::RobixCLIDispatcher::EvaluateReplLine("Print(\"REPL_OK\");");
    Check(replRes.exitCode == 0 && StdLib::StringOps::Contains(replRes.stdoutText, "REPL_OK"),
          "`robix repl` evaluates single-line interactive Robix statement");

    projectFS["Source/Main.rx"] = R"(
package App;
public Function Main() -> Int {
    Print("CLI_MAIN_OK");
    return 0;
}
)";
    auto llvmCompileRes = Tools::RobixCLIDispatcher::ExecuteCommand(
        Tools::CLIBinaryKind::RobixNativeCompiler,
        { "Source/Main.rx" },
        &projectFS);
    Check(llvmCompileRes.exitCode == 0 &&
          StdLib::StringOps::Contains(llvmCompileRes.stdoutText, "define"),
          "`robixc Source/Main.rx` compiles source to native LLVM IR");
}

// ----------------------------------------------------------------------------
// 2. Formatter (`robix fmt`), Strict Linter (`robix lint --strict`), LSP & `robix doc`
// ----------------------------------------------------------------------------
void TestFormatterLinterLSPAndDocGen() {
    std::cout << "\n--- 2. Formatter (`robix fmt`), Strict Linter (`robix lint`), LSP & Documentation Generator (`robix doc`) ---\n";
    using namespace Robix;

    const std::string unformatted = "package App;\npublic class Robot {\npublic Function Start() -> Void {\nPrint(\"Go\");\n}\n}\n";
    const std::string formatted   = Tools::RobixFormatterAndLinter::FormatSource(unformatted);
    Check(StdLib::StringOps::Contains(formatted, "    public Function Start() -> Void {") &&
          StdLib::StringOps::Contains(formatted, "        Print(\"Go\");"),
          "RobixFormatterAndLinter::FormatSource indents nested blocks with canonical 4-space indentation");

    // Strict linter rejects bad casing or missing Explain
    Frontend::DiagnosticBag badLintDiags;
    const bool badLintOk = Tools::RobixFormatterAndLinter::LintSource(
        "BadStyle.rx",
        "package App;\nconst Int badConst = 1;\npublic Function Undocumented() -> Void {}\n",
        true,
        badLintDiags);
    Check(!badLintOk && badLintDiags.HasErrors(),
          "RobixFormatterAndLinter::LintSource (--strict) rejects non-UPPER_SNAKE_CASE constants and missing Explain");

    // LSP indexing, autocomplete, hover Explain docs, go-to-definition, and Markdown doc generation
    Tools::RobixLanguageServer lsp;
    lsp.IndexDocument("file:///Project/Source/Navigation.rx", R"(
package Robotics.Navigation;

public class PathPlanner {
    Explain {
        Description: "Computes collision-free trajectories.";
    }
}

public Function ComputeWaypoint(Int index) -> Int {
    Explain {
        Description: "Returns waypoint coordinate at index.";
        Input: index = "Waypoint index.";
        Output: "Coordinate value.";
    }
    return index * 10;
}
)");

    const auto completions = lsp.GetCompletions("Compute");
    Check(completions.size() == 1 && completions[0].label == "ComputeWaypoint",
          "RobixLanguageServer::GetCompletions returns matching symbol with signature metadata");

    const auto hoverOpt = lsp.GetHoverDocumentation("ComputeWaypoint");
    Check(hoverOpt.has_value() &&
          StdLib::StringOps::Contains(*hoverOpt, "Returns waypoint coordinate at index."),
          "RobixLanguageServer::GetHoverDocumentation surfaces Explain block description");

    const auto defOpt = lsp.GoToDefinition("PathPlanner");
    Check(defOpt.has_value() && defOpt->fileUri == "file:///Project/Source/Navigation.rx" && defOpt->line >= 1,
          "RobixLanguageServer::GoToDefinition resolves exact file URI and line number");

    const std::string docMd = lsp.GenerateMarkdownDocumentation("Robotics.Navigation");
    Check(StdLib::StringOps::Contains(docMd, "# Robix Documentation: Robotics.Navigation") &&
          StdLib::StringOps::Contains(docMd, "ComputeWaypoint"),
          "RobixLanguageServer::GenerateMarkdownDocumentation (`robix doc`) exports structured Markdown API docs");
}

// ----------------------------------------------------------------------------
// 3. Complete Debugger (`robix debug`) & Profiler (`robix profile`) Infrastructure
// ----------------------------------------------------------------------------
void TestDebuggerAndProfilerInfrastructure() {
    std::cout << "\n--- 3. Complete Debugger (`robix debug`) & Profiler (`robix profile`) Infrastructure ---\n";
    using namespace Robix;

    Tools::RobixDebuggerEngine debugger;
    int motorSpeed = 50;

    // Conditional breakpoint: only triggers when motorSpeed > 100
    debugger.AddConditionalBreakpoint(
        "MotorController.rx",
        42,
        "motorSpeed > 100",
        [&]() { return motorSpeed > 100; });

    Check(!debugger.CheckBreakpointHit("MotorController.rx", 42),
          "Conditional breakpoint does not pause when condition (motorSpeed > 100) is false");
    motorSpeed = 140;
    Check(debugger.CheckBreakpointHit("MotorController.rx", 42),
          "Conditional breakpoint pauses execution when condition (motorSpeed > 100) evaluates to true");

    // Memory / variable Watchpoint
    debugger.AddWatchpoint("batteryVoltage", 24, true, false);
    Check(!debugger.NotifyMemoryAccess("batteryVoltage", 24, true),
          "Write watchpoint does not trigger when written value is unchanged");
    Check(debugger.NotifyMemoryAccess("batteryVoltage", 19, true),
          "Write watchpoint triggers immediately when watched variable mutates");

    // Call Stack Navigation, Frame Inspection, Memory Hex View & Crash Dump
    debugger.PushFrame("Main", "Main.rx", 10, { { "mode", "Autonomous" } });
    debugger.PushFrame("ControlLoop", "Controller.rx", 88, { { "dt", "0.001" } });
    const auto* topFrame = debugger.InspectFrame(1);
    Check(topFrame != nullptr && topFrame->functionName == "ControlLoop" && topFrame->locals.at("dt") == "0.001",
          "RobixDebuggerEngine navigates call stack frames and inspects frame local variables");

    const std::uint8_t rawBytes[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    Check(Tools::RobixDebuggerEngine::InspectMemoryHex(rawBytes, 4) == "DE AD BE EF",
          "RobixDebuggerEngine::InspectMemoryHex formats raw memory bytes as uppercase hex");

    const std::string crashDump = debugger.GenerateNativeCrashDump("HardwareFaultException", "VM+NativeParity");
    Check(StdLib::StringOps::Contains(crashDump, "ROBIX SYMBOLICATED CRASH DUMP") &&
          StdLib::StringOps::Contains(crashDump, "#1 ControlLoop at Controller.rx:88") &&
          StdLib::StringOps::Contains(crashDump, "#0 Main at Main.rx:10"),
          "RobixDebuggerEngine::GenerateNativeCrashDump produces symbolicated backtrace with VM & Native parity");

    // Profiler: CPU Flame Graph (folded + SVG), Heap Visualization JSON & Data-Race Detection (RACE9001)
    Tools::RobixProfilerEngine profiler;
    profiler.RecordCPUSample({ "Main", "ControlLoop", "PIDUpdate" }, 450);
    profiler.RecordCPUSample({ "Main", "ControlLoop", "KalmanUpdate" }, 210);
    profiler.RecordHeapAllocation("SensorBufferPool", 8192);

    const std::string folded = profiler.ExportFoldedFlameGraph();
    const std::string svg    = profiler.ExportSVGFlameGraph();
    const std::string heapJs = profiler.ExportHeapVisualizationJSON();
    Check(StdLib::StringOps::Contains(folded, "Main;ControlLoop;PIDUpdate 450") &&
          StdLib::StringOps::Contains(svg, "<svg") &&
          StdLib::StringOps::Contains(heapJs, "\"callsite\":\"SensorBufferPool\""),
          "RobixProfilerEngine exports folded CPU stacks, SVG Flame Graph, and Heap Visualization JSON");

    Frontend::DiagnosticBag raceDiags;
    Check(profiler.RecordSharedMemoryAccess("sharedOdometryPose", 1, true, "poseMutex", raceDiags) &&
          profiler.RecordSharedMemoryAccess("sharedOdometryPose", 2, false, "poseMutex", raceDiags),
          "RobixProfilerEngine allows concurrent access across threads when protected by common lock");

    Check(!profiler.RecordSharedMemoryAccess("sharedOdometryPose", 3, true, "", raceDiags) &&
          raceDiags.ContainsCode("RACE9001"),
          "RobixProfilerEngine detects unsynchronized multi-thread write conflict and reports RACE9001");
}

// ----------------------------------------------------------------------------
// 4. Formal 16-Workload Benchmark Suite, 22 Working Examples & 30 Official Docs
// ----------------------------------------------------------------------------
void TestBenchmarksExamplesAndDocumentationSuite() {
    std::cout << "\n--- 4. 16-Workload Benchmark Suite, 22 Working Examples (with .test.rx) & 30 Official Guides ---\n";
    using namespace Robix;

    // 16 Benchmark Workloads
    const auto benchMetrics = Tools::RobixBenchmarkSuite::RunAll16Workloads();
    Check(benchMetrics.size() == 16, "RobixBenchmarkSuite executes all 16 mandatory performance workloads");

    bool allChecksumsNonZero = true;
    for (const auto& m : benchMetrics) {
        if (m.verificationChecksum == 0 || m.executionTimeMicroseconds <= 0.0) {
            allChecksumsNonZero = false;
        }
    }
    const std::string benchReport = Tools::RobixBenchmarkSuite::FormatBenchmarkReport(benchMetrics);
    Check(allChecksumsNonZero &&
          StdLib::StringOps::Contains(benchReport, "IntegerArithmetic") &&
          StdLib::StringOps::Contains(benchReport, "NeuralNetworkInference"),
          "All 16 benchmark workloads produce verified non-zero checksums and formatted timing metrics");

    // 22 Mandatory Working Example Applications (every example has a `.test.rx` file and executes cleanly)
    const auto& examples = Tools::OfficialExamplesCatalog::GetAll22Examples();
    Check(examples.size() == 22, "OfficialExamplesCatalog provides all 22 mandatory example applications");

    std::size_t passedExamples = 0;
    const bool allExamplesOk   = Tools::OfficialExamplesCatalog::VerifyAndExecuteAll22Examples(passedExamples);
    Check(allExamplesOk && passedExamples == 22,
          "All 22 working example applications and their companion .test.rx files compile and pass execution");

    // 30 Official Documentation Guides (including Guide 29 RobixBuild and Guide 30 Robix Network)
    const auto& guides = Tools::OfficialDocumentationCatalog::GetAll30Guides();
    const std::string docsIndex = Tools::OfficialDocumentationCatalog::RenderCompleteIndexMarkdown();
    Check(guides.size() == 30 &&
          guides[28].fileName == "29_RobixBuildConfigurationGuide.md" &&
          guides[29].fileName == "30_RobixNetworkDistributedGuide.md" &&
          StdLib::StringOps::Contains(docsIndex, "30_RobixNetworkDistributedGuide.md"),
          "OfficialDocumentationCatalog provides all 30 official guides including RobixBuild (#29) and Robix Network (#30)");
}

} // namespace

int main() {
    std::cout << "================================================================\n";
    std::cout << "Robix Stage 13 CLI, Test Generator, Tooling, Benchmarks & Docs\n";
    std::cout << "================================================================\n";

    TestCLITestScaffoldGeneratorAndUnifiedCLI();
    TestFormatterLinterLSPAndDocGen();
    TestDebuggerAndProfilerInfrastructure();
    TestBenchmarksExamplesAndDocumentationSuite();

    std::cout << "\n================================================================\n";
    std::cout << "Stage 13 Results: " << g_passed << " passed, " << g_failed << " failed.\n";
    std::cout << "================================================================\n";

    return (g_failed == 0) ? 0 : 1;
}

