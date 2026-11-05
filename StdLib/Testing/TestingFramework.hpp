#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../../Compiler/AST/ASTNodes.hpp"
#include "../../Compiler/Semantic/Diagnostics.hpp"
#include "../../Runtime/Memory/UnifiedMemory.hpp"

namespace Robix::Testing {

// ============================================================================
// 1. ROBIX.TESTING ASSERTION RESULT & CONTEXT
// ============================================================================
struct AssertionRecord {
    std::string assertionKind;
    bool        passed = true;
    std::string expectedText;
    std::string actualText;
    std::string message;
};

class Assert {
public:
    static void ResetStats() noexcept;
    static std::size_t PassedCount() noexcept;
    static std::size_t FailedCount() noexcept;
    static const std::vector<AssertionRecord>& Records() noexcept;

    // 1. Assert.Equal
    template <typename T, typename U>
    static bool Equal(const T& expected, const U& actual, std::string_view msg = "") {
        const bool ok = (expected == actual);
        Record("Assert.Equal", ok, ToStr(expected), ToStr(actual), msg);
        return ok;
    }

    // 2. Assert.NotEqual
    template <typename T, typename U>
    static bool NotEqual(const T& unexpected, const U& actual, std::string_view msg = "") {
        const bool ok = (unexpected != actual);
        Record("Assert.NotEqual", ok, "!= " + ToStr(unexpected), ToStr(actual), msg);
        return ok;
    }

    // 3. Assert.True
    static bool True(bool condition, std::string_view msg = "");

    // 4. Assert.False
    static bool False(bool condition, std::string_view msg = "");

    // 5. Assert.Null
    template <typename T>
    static bool Null(const std::optional<T>& val, std::string_view msg = "") {
        const bool ok = !val.has_value();
        Record("Assert.Null", ok, "null", ok ? "null" : "non-null", msg);
        return ok;
    }
    static bool Null(const void* ptr, std::string_view msg = "");

    // 6. Assert.NotNull
    template <typename T>
    static bool NotNull(const std::optional<T>& val, std::string_view msg = "") {
        const bool ok = val.has_value();
        Record("Assert.NotNull", ok, "non-null", ok ? "non-null" : "null", msg);
        return ok;
    }
    static bool NotNull(const void* ptr, std::string_view msg = "");

    // 7. Assert.Throws
    static bool Throws(const std::function<void()>& fn, std::string_view expectedSubstring = "");

    // 8. Assert.ThrowsAsync
    static bool ThrowsAsync(std::future<void>& fut, std::string_view expectedSubstring = "");

    // 9. Assert.Success
    static bool Success(bool isOk, std::string_view errorText = "", std::string_view msg = "");

    // 10. Assert.Error
    static bool Error(bool isOk, std::string_view errorText = "", std::string_view expectedErrorSubstring = "");

    // 11. Assert.Contains
    static bool Contains(std::string_view haystack, std::string_view needle, std::string_view msg = "");

    template <typename T>
    static bool Contains(const std::vector<T>& items, const T& target, std::string_view msg = "") {
        bool found = false;
        for (const auto& item : items) {
            if (item == target) {
                found = true;
                break;
            }
        }
        Record("Assert.Contains", found, ToStr(target), found ? "found" : "not found", msg);
        return found;
    }

    // 12. Assert.Timeout
    static bool Timeout(std::uint64_t maxMilliseconds, const std::function<void()>& fn, std::string_view msg = "");

private:
    template <typename T>
    static std::string ToStr(const T& v) {
        if constexpr (std::is_convertible_v<T, std::string_view>) {
            return std::string(std::string_view(v));
        } else if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(v);
        } else {
            return "<object>";
        }
    }

    static void Record(std::string_view kind,
                       bool passed,
                       std::string expected,
                       std::string actual,
                       std::string_view message);
};

// ============================================================================
// 2. MANDATORY PER-FILE TEST ENFORCEMENT CONFIG (.rbxproj & R5001)
// ============================================================================
struct TestEnforcementConfig {
    bool requireTestFile                = true;
    bool requireTestsToPass             = true;
    bool requireTestsForAllSourceFiles  = true;
    bool allowUntestedDevelopmentBuild  = false;
    bool allowUntestedReleaseBuild      = false;
    bool enableCoverageReport           = true;
    bool isReleaseBuild                 = false;
};

struct SourceTestPair {
    std::string sourceFileName; // e.g., "Motor.rbx"
    std::string sourceCode;
    std::string testFileName;   // e.g., "Motor.test.rbx" (empty if missing)
    std::string testCode;
};

struct FileTestCoverageResult {
    std::string sourceFileName;
    std::string testFileName;
    bool        hasTestFile            = false;
    std::size_t testBlockCount         = 0;
    bool        exercisesSourceSymbols = false;
    bool        hasExplicitCoversDecl  = false;
    std::size_t assertionsPassed       = 0;
    std::size_t assertionsFailed       = 0;
    double      symbolCoveragePercent  = 0.0;
    bool        passedEnforcement      = false;
};

// ============================================================================
// 3. EXECUTION ENGINE MODE FOR SINGLE-FILE & PROJECT TESTING
// ============================================================================
enum class TestExecutionEngine : std::uint8_t {
    Interpreter,    // robix test File.rbx --engine interpreter
    NativeCompiler, // robixc test File.rbx / robix test File.rbx --engine native
    Differential    // Runs both Interpreter and Native pipelines and verifies parity
};

struct SingleFileTestResult {
    std::string         targetFile;
    TestExecutionEngine engine = TestExecutionEngine::Interpreter;
    bool                compiledCleanly    = false;
    std::size_t         testBlocksExecuted = 0;
    std::size_t         assertionsPassed   = 0;
    std::size_t         assertionsFailed   = 0;
    bool                nativeParityMatch  = true;
    std::string         stdoutCaptured;
    std::string         generatedLLVMIR;
    bool                overallPassed      = false;
};

// ============================================================================
// 4. ADVANCED TESTING RESULTS (PROPERTY, FUZZ, MUTATION, STRESS, MEMORY)
// ============================================================================
struct PropertyTestResult {
    bool        passed                = true;
    std::size_t iterationsRun         = 0;
    std::int64_t failingInput         = 0;
    std::int64_t shrunkMinimalInput   = 0;
};

struct FuzzTestResult {
    bool        passed                = true;
    std::size_t mutatedInputsTested   = 0;
    std::size_t crashesOrHangs        = 0;
};

struct MutationTestResult {
    std::size_t totalMutantsGenerated = 0;
    std::size_t mutantsKilled         = 0;
    std::size_t mutantsSurvived       = 0;
    double      mutationScorePercent  = 0.0;
    bool        passedThreshold       = false;
};

struct StressTestResult {
    std::size_t repeatIterations      = 0;
    std::size_t threadCount           = 0;
    std::size_t successfulRuns        = 0;
    std::size_t failedRuns            = 0;
    bool        deterministicOutput   = true;
    bool        passed                = false;
};

struct MemoryLeakTestResult {
    std::size_t activeAllocationsAfterRun = 0;
    std::size_t leakedBytesAfterRun       = 0;
    std::size_t peakAllocatedBytes        = 0;
    bool        passedZeroLeaks           = false;
};

// ============================================================================
// 5. MANDATORY TEST ENFORCER & ADVANCED TEST RUNNER ENGINE
// ============================================================================
class MandatoryTestEnforcer {
public:
    // Enforces Rule R5001 across a project's source/test file pairs
    static std::vector<FileTestCoverageResult> EnforceProjectTests(
        const std::vector<SourceTestPair>& files,
        const TestEnforcementConfig& config,
        Frontend::DiagnosticBag& diagnostics);

    // Executes a single .rbx + .test.rbx (or standalone .test.rbx) in isolation
    static SingleFileTestResult RunSingleFileTest(
        std::string_view sourceFileName,
        std::string_view sourceCode,
        std::string_view testFileName,
        std::string_view testCode,
        TestExecutionEngine engine,
        Frontend::DiagnosticBag& diagnostics);

    // Negative compilation test: verifies invalid code produces expected diagnostic code
    static bool RunNegativeCompilationTest(
        std::string_view invalidSourceCode,
        std::string_view expectedErrorCode);

    // Property-based test with automatic counterexample shrinking
    static PropertyTestResult RunPropertyBasedTest(
        std::uint64_t seed,
        std::size_t maxIterations,
        std::int64_t minVal,
        std::int64_t maxVal,
        const std::function<bool(std::int64_t)>& propertyPredicate);

    // Fuzz test against the Robix Lexer, Parser, and TypeChecker
    static FuzzTestResult RunFrontendFuzzTest(
        std::uint64_t seed,
        std::size_t iterations,
        std::string_view corpusSeedSource);

    // Mutation testing: mutates source operators and checks if .test.rbx kills each mutant
    static MutationTestResult RunMutationTest(
        std::string_view sourceFileName,
        std::string_view sourceCode,
        std::string_view testFileName,
        std::string_view testCode,
        double minimumScorePercent = 80.0);

    // Stress & concurrency test (--stress --repeat N)
    static StressTestResult RunStressTest(
        std::string_view sourceCode,
        std::string_view testCode,
        std::size_t repeatCount,
        std::size_t threadCount);

    // Memory leak & ownership verification test (--memory-check)
    static MemoryLeakTestResult RunMemoryLeakCheckTest(
        const std::function<void(Runtime::UnifiedMemoryManager&)>& workload,
        Frontend::DiagnosticBag& diagnostics);

    // Snapshot test comparing actual structured output to golden snapshot
    static bool RunSnapshotTest(
        std::string_view snapshotName,
        std::string_view goldenExpected,
        std::string_view actualOutput,
        Frontend::DiagnosticBag& diagnostics);
};

} // namespace Robix::Testing

