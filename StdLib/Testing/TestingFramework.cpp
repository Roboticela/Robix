#include "TestingFramework.hpp"

#include "../../Compiler/CodeGen/LLVMIRGenerator.hpp"
#include "../../Compiler/IR/SSAIR.hpp"
#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Optimizer/IRPassManager.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Semantic/SemanticAnalyzer.hpp"
#include "../../Compiler/Types/TypeChecker.hpp"
#include "../../Interpreter/Bytecode/BytecodeModule.hpp"
#include "../../Interpreter/VM/BytecodeVM.hpp"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_set>

namespace Robix::Testing {

namespace {
std::mutex g_assertMutex;
std::size_t g_passedCount = 0;
std::size_t g_failedCount = 0;
std::vector<AssertionRecord> g_records;

// Helper to collect top-level callable/type symbol names declared in a source AST
std::unordered_set<std::string> CollectDeclaredSymbols(const Frontend::CompilationUnitAST& unit) {
    std::unordered_set<std::string> symbols;
    for (const auto& declPtr : unit.declarations) {
        if (!declPtr) continue;
        if (!declPtr->name.empty() &&
            declPtr->kind != Frontend::DeclKind::Test &&
            declPtr->kind != Frontend::DeclKind::TestFile) {
            symbols.insert(declPtr->name);
        }
        for (const auto& memberPtr : declPtr->members) {
            if (memberPtr && !memberPtr->name.empty()) {
                symbols.insert(memberPtr->name);
            }
        }
    }
    return symbols;
}

// Helper to count Test blocks and check if any source symbols are referenced or covered
void InspectTestUnit(const Frontend::CompilationUnitAST& testUnit,
                     std::string_view sourceFileName,
                     std::string_view testSourceText,
                     const std::unordered_set<std::string>& sourceSymbols,
                     std::size_t& outTestBlockCount,
                     bool& outHasCoversDecl,
                     std::size_t& outCoveredSymbolCount) {
    outTestBlockCount     = 0;
    outHasCoversDecl      = false;
    outCoveredSymbolCount = 0;

    for (const auto& declPtr : testUnit.declarations) {
        if (!declPtr) continue;
        if (declPtr->kind == Frontend::DeclKind::Test) {
            ++outTestBlockCount;
        } else if (declPtr->kind == Frontend::DeclKind::TestFile) {
            if (declPtr->coversFile == sourceFileName ||
                (!sourceFileName.empty() && declPtr->coversFile.find(sourceFileName) != std::string::npos)) {
                outHasCoversDecl = true;
            }
            for (const auto& m : declPtr->members) {
                if (m && m->kind == Frontend::DeclKind::Test) {
                    ++outTestBlockCount;
                }
            }
        }
    }

    for (const auto& sym : sourceSymbols) {
        if (testSourceText.find(sym) != std::string_view::npos) {
            ++outCoveredSymbolCount;
        }
    }
}

} // namespace

// ============================================================================
// 1. ROBIX.TESTING ASSERT IMPLEMENTATION
// ============================================================================

void Assert::ResetStats() noexcept {
    std::lock_guard<std::mutex> lock(g_assertMutex);
    g_passedCount = 0;
    g_failedCount = 0;
    g_records.clear();
}

std::size_t Assert::PassedCount() noexcept {
    std::lock_guard<std::mutex> lock(g_assertMutex);
    return g_passedCount;
}

std::size_t Assert::FailedCount() noexcept {
    std::lock_guard<std::mutex> lock(g_assertMutex);
    return g_failedCount;
}

const std::vector<AssertionRecord>& Assert::Records() noexcept {
    return g_records;
}

void Assert::Record(std::string_view kind,
                    bool passed,
                    std::string expected,
                    std::string actual,
                    std::string_view message) {
    std::lock_guard<std::mutex> lock(g_assertMutex);
    if (passed) {
        ++g_passedCount;
    } else {
        ++g_failedCount;
    }
    g_records.push_back(AssertionRecord{
        std::string(kind),
        passed,
        std::move(expected),
        std::move(actual),
        std::string(message)
    });
}

bool Assert::True(bool condition, std::string_view msg) {
    Record("Assert.True", condition, "true", condition ? "true" : "false", msg);
    return condition;
}

bool Assert::False(bool condition, std::string_view msg) {
    const bool ok = !condition;
    Record("Assert.False", ok, "false", condition ? "true" : "false", msg);
    return ok;
}

bool Assert::Null(const void* ptr, std::string_view msg) {
    const bool ok = (ptr == nullptr);
    Record("Assert.Null", ok, "null", ok ? "null" : "non-null", msg);
    return ok;
}

bool Assert::NotNull(const void* ptr, std::string_view msg) {
    const bool ok = (ptr != nullptr);
    Record("Assert.NotNull", ok, "non-null", ok ? "non-null" : "null", msg);
    return ok;
}

bool Assert::Throws(const std::function<void()>& fn, std::string_view expectedSubstring) {
    bool threw   = false;
    bool matched = false;
    std::string caughtMsg;
    try {
        if (fn) fn();
    } catch (const std::exception& ex) {
        threw     = true;
        caughtMsg = ex.what();
        matched   = expectedSubstring.empty() || (caughtMsg.find(expectedSubstring) != std::string::npos);
    } catch (...) {
        threw     = true;
        caughtMsg = "<non-std exception>";
        matched   = expectedSubstring.empty();
    }
    const bool ok = threw && matched;
    Record("Assert.Throws", ok,
           expectedSubstring.empty() ? "exception thrown" : std::string(expectedSubstring),
           threw ? caughtMsg : "no exception thrown",
           "");
    return ok;
}

bool Assert::ThrowsAsync(std::future<void>& fut, std::string_view expectedSubstring) {
    bool threw   = false;
    bool matched = false;
    std::string caughtMsg;
    try {
        fut.get();
    } catch (const std::exception& ex) {
        threw     = true;
        caughtMsg = ex.what();
        matched   = expectedSubstring.empty() || (caughtMsg.find(expectedSubstring) != std::string::npos);
    } catch (...) {
        threw     = true;
        caughtMsg = "<non-std async exception>";
        matched   = expectedSubstring.empty();
    }
    const bool ok = threw && matched;
    Record("Assert.ThrowsAsync", ok,
           expectedSubstring.empty() ? "async exception thrown" : std::string(expectedSubstring),
           threw ? caughtMsg : "no exception thrown",
           "");
    return ok;
}

bool Assert::Success(bool isOk, std::string_view errorText, std::string_view msg) {
    Record("Assert.Success", isOk, "Result::Success", isOk ? "Result::Success" : std::string(errorText), msg);
    return isOk;
}

bool Assert::Error(bool isOk, std::string_view errorText, std::string_view expectedErrorSubstring) {
    const bool hasErr = !isOk && (expectedErrorSubstring.empty() ||
                                  errorText.find(expectedErrorSubstring) != std::string_view::npos);
    Record("Assert.Error", hasErr,
           expectedErrorSubstring.empty() ? "Result::Error" : std::string(expectedErrorSubstring),
           isOk ? "Result::Success" : std::string(errorText),
           "");
    return hasErr;
}

bool Assert::Contains(std::string_view haystack, std::string_view needle, std::string_view msg) {
    const bool ok = (haystack.find(needle) != std::string_view::npos);
    Record("Assert.Contains", ok, std::string(needle), std::string(haystack), msg);
    return ok;
}

bool Assert::Timeout(std::uint64_t maxMilliseconds, const std::function<void()>& fn, std::string_view msg) {
    const auto start = std::chrono::steady_clock::now();
    if (fn) {
        fn();
    }
    const auto end = std::chrono::steady_clock::now();
    const auto elapsedMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    const bool ok = (elapsedMs <= maxMilliseconds);
    Record("Assert.Timeout", ok,
           "<= " + std::to_string(maxMilliseconds) + "ms",
           std::to_string(elapsedMs) + "ms",
           msg);
    return ok;
}

// ============================================================================
// 2. MANDATORY PER-FILE ENFORCEMENT (R5001) & SINGLE-FILE TEST RUNNER
// ============================================================================

SingleFileTestResult MandatoryTestEnforcer::RunSingleFileTest(
    std::string_view sourceFileName,
    std::string_view sourceCode,
    std::string_view testFileName,
    std::string_view testCode,
    TestExecutionEngine engine,
    Frontend::DiagnosticBag& diagnostics) {

    SingleFileTestResult result;
    result.targetFile = testFileName.empty() ? std::string(sourceFileName) : std::string(testFileName);
    result.engine     = engine;

    // Combine production declarations and test declarations into one compilation unit
    std::string combinedSource;
    if (!sourceCode.empty()) {
        combinedSource += std::string(sourceCode) + "\n";
    }
    if (!testCode.empty()) {
        combinedSource += std::string(testCode) + "\n";
    }

    Frontend::Lexer lexer(combinedSource, result.targetFile, diagnostics);
    auto tokens = lexer.TokenizeAll();

    Frontend::Parser parser(std::move(tokens), result.targetFile, diagnostics);
    auto ast = parser.ParseCompilationUnit();

    Frontend::TypeCheckerOptions tcOpts;
    tcOpts.strictStyleMode = false; // Allow test descriptions and flexible helpers during combined test run
    tcOpts.requireExplain  = false;
    Frontend::TypeChecker tc(diagnostics, tcOpts);
    tc.CheckCompilationUnit(ast);

    Frontend::SemanticAnalyzer sema(diagnostics);
    sema.AnalyzeCompilationUnit(ast);

    if (diagnostics.HasErrors()) {
        result.compiledCleanly = false;
        result.overallPassed   = false;
        return result;
    }

    result.compiledCleanly = true;

    IR::SSAIRBuilder irBuilder(sema, diagnostics);
    IR::IRModule irMod = irBuilder.LowerCompilationUnit(ast);

    IR::IRPassManager passMgr(sema, diagnostics);
    passMgr.RunOptimizationPipeline(irMod, true);

    if (diagnostics.HasErrors()) {
        result.compiledCleanly = false;
        result.overallPassed   = false;
        return result;
    }

    // Native LLVM IR code generation if requested (NativeCompiler or Differential)
    if (engine == TestExecutionEngine::NativeCompiler || engine == TestExecutionEngine::Differential) {
        CodeGen::NativeCodeGenOptions cgOpts;
        cgOpts.profile = CodeGen::BuildProfile::Release;
        cgOpts.target  = CodeGen::TargetPlatform::WindowsX64;
        CodeGen::LLVMIRGenerator llvmGen(diagnostics, cgOpts);
        result.generatedLLVMIR = llvmGen.GenerateLLVMIR(irMod);
    }

    // Execute all Test blocks in the Bytecode VM (and verify parity with LLVM IR in Differential/Native mode)
    VM::BytecodeGenerator bcGen(diagnostics);
    VM::BytecodeModule bcMod = bcGen.GenerateFromIR(irMod);
    if (!bcGen.VerifyBytecodeModule(bcMod)) {
        result.overallPassed = false;
        return result;
    }

    VM::BytecodeVM vm(bcMod);
    const VM::VMTestSuiteResult vmSuite = vm.ExecuteAllTests();
    result.testBlocksExecuted = vmSuite.passedTests + vmSuite.failedTests;
    result.assertionsPassed   = vm.GetAssertionPassCount();
    result.assertionsFailed   = vm.GetAssertionFailCount();
    result.stdoutCaptured     = vm.GetStdoutBuffer();

    if (engine == TestExecutionEngine::NativeCompiler || engine == TestExecutionEngine::Differential) {
        // Verify that every lowered test function is emitted in native LLVM IR
        bool allTestFnsInNativeIR = !result.generatedLLVMIR.empty();
        for (const auto& fn : irMod.functions) {
            if (fn.isTestBlock) {
                if (result.generatedLLVMIR.find(fn.mangledName) == std::string::npos) {
                    allTestFnsInNativeIR = false;
                    break;
                }
            }
        }
        result.nativeParityMatch = allTestFnsInNativeIR;
    }

    result.overallPassed = result.compiledCleanly &&
                           (result.testBlocksExecuted > 0) &&
                           (result.assertionsFailed == 0) &&
                           result.nativeParityMatch;
    return result;
}

std::vector<FileTestCoverageResult> MandatoryTestEnforcer::EnforceProjectTests(
    const std::vector<SourceTestPair>& files,
    const TestEnforcementConfig& config,
    Frontend::DiagnosticBag& diagnostics) {

    std::vector<FileTestCoverageResult> results;
    results.reserve(files.size());

    const bool bypassAllowed = (!config.isReleaseBuild && config.allowUntestedDevelopmentBuild) ||
                               (config.isReleaseBuild && config.allowUntestedReleaseBuild);

    for (const auto& pair : files) {
        FileTestCoverageResult cov;
        cov.sourceFileName = pair.sourceFileName;
        cov.testFileName   = pair.testFileName.empty()
                                 ? (pair.sourceFileName.substr(0, pair.sourceFileName.rfind('.')) + ".test.rx")
                                 : pair.testFileName;

        // 1. Check that the .test.rx file exists and is non-empty
        if (pair.testFileName.empty() || pair.testCode.empty()) {
            cov.hasTestFile       = false;
            cov.passedEnforcement = bypassAllowed || !config.requireTestFile;
            if (!cov.passedEnforcement) {
                diagnostics.ReportError(
                    "R5001",
                    "ROBIX TEST ERROR R5001: Missing mandatory test file '" + cov.testFileName +
                        "' for source file '" + cov.sourceFileName + "'.",
                    Frontend::SourceLocation{ cov.sourceFileName, 1, 1, 1 },
                    "Create '" + cov.testFileName + "' with at least one 'Test \"...\" { ... }' block exercising '" +
                        cov.sourceFileName + "'.");
            }
            results.push_back(std::move(cov));
            continue;
        }

        cov.hasTestFile = true;

        // 2. Parse both source and test file to verify test block count & symbol coverage
        Frontend::DiagnosticBag parseDiag;
        Frontend::Lexer srcLexer(pair.sourceCode, pair.sourceFileName, parseDiag);
        Frontend::Parser srcParser(srcLexer.TokenizeAll(), pair.sourceFileName, parseDiag);
        auto srcAST = srcParser.ParseCompilationUnit();

        Frontend::Lexer tstLexer(pair.testCode, pair.testFileName, parseDiag);
        Frontend::Parser tstParser(tstLexer.TokenizeAll(), pair.testFileName, parseDiag);
        auto tstAST = tstParser.ParseCompilationUnit();

        const auto declaredSymbols = CollectDeclaredSymbols(srcAST);
        std::size_t coveredSymbols = 0;
        InspectTestUnit(tstAST,
                        pair.sourceFileName,
                        pair.testCode,
                        declaredSymbols,
                        cov.testBlockCount,
                        cov.hasExplicitCoversDecl,
                        coveredSymbols);

        if (declaredSymbols.empty()) {
            cov.exercisesSourceSymbols = cov.hasExplicitCoversDecl || (cov.testBlockCount > 0);
            cov.symbolCoveragePercent  = cov.exercisesSourceSymbols ? 100.0 : 0.0;
        } else {
            cov.exercisesSourceSymbols = (coveredSymbols > 0) || cov.hasExplicitCoversDecl;
            cov.symbolCoveragePercent  = cov.hasExplicitCoversDecl
                                             ? 100.0
                                             : (static_cast<double>(coveredSymbols) * 100.0 /
                                                static_cast<double>(declaredSymbols.size()));
        }

        if (cov.testBlockCount == 0) {
            cov.passedEnforcement = bypassAllowed;
            if (!cov.passedEnforcement) {
                diagnostics.ReportError(
                    "R5001",
                    "ROBIX TEST ERROR R5001: Test file '" + cov.testFileName +
                        "' contains 0 'Test' blocks.",
                    Frontend::SourceLocation{ cov.testFileName, 1, 1, 1 },
                    "Add at least one 'Test \"description\" { ... }' block in '" + cov.testFileName + "'.");
            }
            results.push_back(std::move(cov));
            continue;
        }

        if (!cov.exercisesSourceSymbols && config.requireTestsForAllSourceFiles) {
            cov.passedEnforcement = bypassAllowed;
            if (!cov.passedEnforcement) {
                diagnostics.ReportError(
                    "R5001",
                    "ROBIX TEST ERROR R5001: Test file '" + cov.testFileName +
                        "' does not exercise any symbol from '" + cov.sourceFileName +
                        "' and has no 'Covers = \"" + cov.sourceFileName + "\"' declaration.",
                    Frontend::SourceLocation{ cov.testFileName, 1, 1, 1 },
                    "Call functions/classes from '" + cov.sourceFileName +
                        "' or declare 'TestFile \"" + cov.testFileName + "\" { Covers = \"" + cov.sourceFileName + "\"; ... }'.");
            }
            results.push_back(std::move(cov));
            continue;
        }

        // 3. Execute tests and enforce zero failures
        Frontend::DiagnosticBag runDiag;
        SingleFileTestResult execRes = RunSingleFileTest(
            pair.sourceFileName,
            pair.sourceCode,
            pair.testFileName,
            pair.testCode,
            TestExecutionEngine::Differential,
            runDiag);

        cov.assertionsPassed = execRes.assertionsPassed;
        cov.assertionsFailed = execRes.assertionsFailed;

        if (!execRes.overallPassed && config.requireTestsToPass) {
            cov.passedEnforcement = bypassAllowed;
            if (!cov.passedEnforcement) {
                diagnostics.ReportError(
                    "R5001",
                    "ROBIX TEST ERROR R5001: Test execution failed in '" + cov.testFileName +
                        "' (" + std::to_string(cov.assertionsFailed) + " assertion(s) failed).",
                    Frontend::SourceLocation{ cov.testFileName, 1, 1, 1 },
                    "Fix the failing assertions or broken code in '" + cov.sourceFileName + "' before building.");
            }
        } else {
            cov.passedEnforcement = true;
        }

        results.push_back(std::move(cov));
    }

    return results;
}

// ============================================================================
// 3. ADVANCED TESTING ENGINES (NEGATIVE, PROPERTY, FUZZ, MUTATION, STRESS, LEAK)
// ============================================================================

bool MandatoryTestEnforcer::RunNegativeCompilationTest(
    std::string_view invalidSourceCode,
    std::string_view expectedErrorCode) {

    Frontend::DiagnosticBag diagnostics;
    Frontend::Lexer lexer(invalidSourceCode, "NegativeTest.rx", diagnostics);
    auto tokens = lexer.TokenizeAll();

    Frontend::Parser parser(std::move(tokens), "NegativeTest.rx", diagnostics);
    auto ast = parser.ParseCompilationUnit();

    Frontend::TypeChecker tc(diagnostics);
    tc.CheckCompilationUnit(ast);

    Frontend::SemanticAnalyzer sema(diagnostics);
    sema.AnalyzeCompilationUnit(ast);

    return diagnostics.HasErrors() && diagnostics.ContainsCode(expectedErrorCode);
}

PropertyTestResult MandatoryTestEnforcer::RunPropertyBasedTest(
    std::uint64_t seed,
    std::size_t maxIterations,
    std::int64_t minVal,
    std::int64_t maxVal,
    const std::function<bool(std::int64_t)>& propertyPredicate) {

    PropertyTestResult res;
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<std::int64_t> dist(minVal, maxVal);

    for (std::size_t i = 0; i < maxIterations; ++i) {
        ++res.iterationsRun;
        const std::int64_t candidate = dist(rng);
        if (!propertyPredicate(candidate)) {
            res.passed             = false;
            res.failingInput       = candidate;
            res.shrunkMinimalInput = candidate;

            // Automatic counterexample shrinking toward 0
            std::int64_t current = candidate;
            while (current != 0) {
                const std::int64_t nextCandidate = current / 2;
                if (nextCandidate >= minVal && nextCandidate <= maxVal && !propertyPredicate(nextCandidate)) {
                    res.shrunkMinimalInput = nextCandidate;
                    if (nextCandidate == 0) break;
                    current = nextCandidate;
                } else {
                    break;
                }
            }
            // Linear step shrink near minimal boundary
            while (res.shrunkMinimalInput > 0 && (res.shrunkMinimalInput - 1) >= minVal &&
                   !propertyPredicate(res.shrunkMinimalInput - 1)) {
                --res.shrunkMinimalInput;
            }
            while (res.shrunkMinimalInput < 0 && (res.shrunkMinimalInput + 1) <= maxVal &&
                   !propertyPredicate(res.shrunkMinimalInput + 1)) {
                ++res.shrunkMinimalInput;
            }
            return res;
        }
    }

    res.passed = true;
    return res;
}

FuzzTestResult MandatoryTestEnforcer::RunFrontendFuzzTest(
    std::uint64_t seed,
    std::size_t iterations,
    std::string_view corpusSeedSource) {

    FuzzTestResult res;
    std::mt19937_64 rng(seed);
    const std::string base(corpusSeedSource);

    for (std::size_t i = 0; i < iterations; ++i) {
        ++res.mutatedInputsTested;
        std::string mutated = base;
        if (!mutated.empty()) {
            const std::size_t idx = rng() % mutated.size();
            const char replacementChars[] = "{}()[];:=+-*/<>!@#$%^&|~`\"'\\0123456789abcdefghijklmnopqrstuvwxyz";
            mutated[idx] = replacementChars[rng() % (sizeof(replacementChars) - 1)];
        }

        try {
            Frontend::DiagnosticBag diag;
            Frontend::Lexer lexer(mutated, "FuzzInput.rx", diag);
            auto tokens = lexer.TokenizeAll();
            Frontend::Parser parser(std::move(tokens), "FuzzInput.rx", diag);
            auto ast = parser.ParseCompilationUnit();
            Frontend::TypeChecker tc(diag);
            tc.CheckCompilationUnit(ast);
        } catch (...) {
            ++res.crashesOrHangs;
        }
    }

    res.passed = (res.crashesOrHangs == 0);
    return res;
}

MutationTestResult MandatoryTestEnforcer::RunMutationTest(
    std::string_view sourceFileName,
    std::string_view sourceCode,
    std::string_view testFileName,
    std::string_view testCode,
    double minimumScorePercent) {

    MutationTestResult result;
    const std::string src(sourceCode);

    struct OperatorMutantRule {
        std::string_view fromOp;
        std::string_view toOp;
    };
    constexpr OperatorMutantRule rules[] = {
        { " + ", " - " },
        { " - ", " + " },
        { " * ", " + " },
        { " == ", " != " },
        { " != ", " == " },
        { " > ", " <= " },
        { " < ", " >= " }
    };

    std::vector<std::string> mutants;
    for (const auto& rule : rules) {
        std::size_t pos = 0;
        while ((pos = src.find(rule.fromOp, pos)) != std::string::npos) {
            std::string mutated = src;
            mutated.replace(pos, rule.fromOp.size(), rule.toOp);
            mutants.push_back(std::move(mutated));
            pos += rule.fromOp.size();
        }
    }

    result.totalMutantsGenerated = mutants.size();
    if (mutants.empty()) {
        result.mutationScorePercent = 100.0;
        result.passedThreshold      = true;
        return result;
    }

    for (const auto& mutantCode : mutants) {
        Frontend::DiagnosticBag diag;
        SingleFileTestResult runRes = RunSingleFileTest(
            sourceFileName,
            mutantCode,
            testFileName,
            testCode,
            TestExecutionEngine::Interpreter,
            diag);

        // A mutant is killed if compilation fails OR at least one test assertion fails
        if (!runRes.overallPassed || runRes.assertionsFailed > 0) {
            ++result.mutantsKilled;
        } else {
            ++result.mutantsSurvived;
        }
    }

    result.mutationScorePercent =
        (static_cast<double>(result.mutantsKilled) * 100.0) / static_cast<double>(result.totalMutantsGenerated);
    result.passedThreshold = (result.mutationScorePercent >= minimumScorePercent);
    return result;
}

StressTestResult MandatoryTestEnforcer::RunStressTest(
    std::string_view sourceCode,
    std::string_view testCode,
    std::size_t repeatCount,
    std::size_t threadCount) {

    StressTestResult result;
    result.repeatIterations = repeatCount;
    result.threadCount      = std::max<std::size_t>(1, threadCount);

    std::mutex mtx;
    std::optional<std::string> referenceStdout;

    auto workerFn = [&](std::size_t iterationsForWorker) {
        for (std::size_t i = 0; i < iterationsForWorker; ++i) {
            Frontend::DiagnosticBag diag;
            SingleFileTestResult run = RunSingleFileTest(
                "Stress.rx",
                sourceCode,
                "Stress.test.rx",
                testCode,
                TestExecutionEngine::Interpreter,
                diag);

            std::lock_guard<std::mutex> lock(mtx);
            if (run.overallPassed) {
                ++result.successfulRuns;
                if (!referenceStdout.has_value()) {
                    referenceStdout = run.stdoutCaptured;
                } else if (*referenceStdout != run.stdoutCaptured) {
                    result.deterministicOutput = false;
                }
            } else {
                ++result.failedRuns;
            }
        }
    };

    std::vector<std::thread> threads;
    const std::size_t basePerThread = repeatCount / result.threadCount;
    const std::size_t remainder     = repeatCount % result.threadCount;

    for (std::size_t t = 0; t < result.threadCount; ++t) {
        const std::size_t count = basePerThread + (t < remainder ? 1 : 0);
        if (count > 0) {
            threads.emplace_back(workerFn, count);
        }
    }
    for (auto& th : threads) {
        if (th.joinable()) th.join();
    }

    result.passed = (result.failedRuns == 0) &&
                    (result.successfulRuns == repeatCount) &&
                    result.deterministicOutput;
    return result;
}

MemoryLeakTestResult MandatoryTestEnforcer::RunMemoryLeakCheckTest(
    const std::function<void(Runtime::UnifiedMemoryManager&)>& workload,
    Frontend::DiagnosticBag& diagnostics) {

    Runtime::UnifiedMemoryManager memRuntime(diagnostics);
    if (workload) {
        workload(memRuntime);
    }
    // Reclaim any cyclic reference graphs before auditing leaks
    (void)memRuntime.CollectReferenceCycles();

    MemoryLeakTestResult res;
    res.activeAllocationsAfterRun = memRuntime.GetActiveManualAllocationCount();
    res.leakedBytesAfterRun       = memRuntime.GetCurrentHeapBytes();
    res.peakAllocatedBytes        = memRuntime.GetCurrentHeapBytes();
    res.passedZeroLeaks           = (res.activeAllocationsAfterRun == 0) &&
                                    (res.leakedBytesAfterRun == 0) &&
                                    !diagnostics.HasErrors();

    if (!res.passedZeroLeaks) {
        diagnostics.ReportError(
            "R5001",
            "ROBIX TEST ERROR R5001: Memory leak detected during '--memory-check' test run (" +
                std::to_string(res.activeAllocationsAfterRun) + " active block(s), " +
                std::to_string(res.leakedBytesAfterRun) + " byte(s) leaked).",
            Frontend::SourceLocation{ "MemoryCheck.test.rx", 1, 1, 1 },
            "Ensure all manually allocated blocks are freed or wrapped in 'defer' / 'using'.");
    }

    return res;
}

bool MandatoryTestEnforcer::RunSnapshotTest(
    std::string_view snapshotName,
    std::string_view goldenExpected,
    std::string_view actualOutput,
    Frontend::DiagnosticBag& diagnostics) {

    if (goldenExpected == actualOutput) {
        return true;
    }

    diagnostics.ReportError(
        "R5001",
        "ROBIX TEST ERROR R5001: Snapshot mismatch in '" + std::string(snapshotName) + "'.",
        Frontend::SourceLocation{ std::string(snapshotName), 1, 1, 1 },
        "Update the snapshot if the output change is intentional, or fix the regression.",
        std::string(goldenExpected),
        std::string(actualOutput));
    return false;
}

} // namespace Robix::Testing

