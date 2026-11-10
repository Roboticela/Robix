#include "../../Compiler/CodeGen/LLVMIRGenerator.hpp"
#include "../../Compiler/IR/SSAIR.hpp"
#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Optimizer/IRPassManager.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Semantic/SemanticAnalyzer.hpp"
#include "../../Compiler/Types/TypeChecker.hpp"
#include "../../Interpreter/Bytecode/BytecodeModule.hpp"
#include "../../Interpreter/VM/BytecodeVM.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_passedTests = 0;
int g_failedTests = 0;

void Check(bool condition, const char* description) {
    if (condition) {
        ++g_passedTests;
    } else {
        ++g_failedTests;
        std::cerr << "[FAIL] " << description << "\n";
    }
}

struct DualPipelineBundle {
    Robix::Frontend::DiagnosticBag diagnostics;
    Robix::Frontend::CompilationUnitAST ast;
    Robix::Frontend::SemanticAnalyzer sema;
    Robix::IR::IRModule irMod;
    Robix::IR::OptimizationStats optStats;
    Robix::VM::BytecodeModule bcMod;

    DualPipelineBundle() : sema(diagnostics) {}
};

DualPipelineBundle BuildDualPipeline(std::string_view source,
                                     std::string_view fileName = "SingleFile.rx",
                                     bool optimize = true) {
    DualPipelineBundle b;
    Robix::Frontend::Lexer lexer(source, fileName, b.diagnostics);
    auto tokens = lexer.TokenizeAll();

    Robix::Frontend::Parser parser(std::move(tokens), fileName, b.diagnostics);
    b.ast = parser.ParseCompilationUnit();

    Robix::Frontend::TypeChecker typeChecker(b.diagnostics);
    typeChecker.CheckCompilationUnit(b.ast);
    b.sema.AnalyzeCompilationUnit(b.ast);

    Robix::IR::SSAIRBuilder irBuilder(b.sema, b.diagnostics);
    b.irMod = irBuilder.LowerCompilationUnit(b.ast);

    Robix::IR::IRPassManager passMgr(b.sema, b.diagnostics);
    passMgr.RunOptimizationPipeline(b.irMod, optimize);
    b.optStats = passMgr.GetStats();

    Robix::VM::BytecodeGenerator bcGen(b.diagnostics);
    b.bcMod = bcGen.GenerateFromIR(b.irMod);
    (void)bcGen.VerifyBytecodeModule(b.bcMod);

    return b;
}

void TestSSAIRVerifierAndOptimizationPasses() {
    const std::string src = R"(
        package Robotics.Perf;

        public final class Motor {
            public Int rpm = 0;

            public Motor(Int initialRPM) {
                this.rpm = initialRPM;
            }

            public Int GetRPM() {
                return this.rpm;
            }
        }

        public Int Main() {
            // Constant expression 10 + 20 * 3 that folds at IR level
            let baseSpeed = 10 + 20 * 3;
            // Non-escaping object allocation that promotes from Heap to Stack!
            let localMotor = new Motor(baseSpeed);
            return localMotor.GetRPM();
        }
    )";

    auto bundle = BuildDualPipeline(src, "OptTest.rx", true);
    Check(!bundle.diagnostics.HasErrors(), "SSA IR lowering and 4-pass optimization pipeline complete with zero errors");
    Check(bundle.optStats.verificationRunsCount == 5,
          "IR Verifier runs before optimization and after each of the 4 optimization passes (5 total runs)");
    Check(bundle.optStats.constantsFoldedCount >= 2,
          "ConstantFoldingPass folds 20 * 3 and 10 + 60 at compile time");
    Check(bundle.optStats.stackPromotedAllocsCount == 1,
          "EscapeAnalysisPass promotes non-escaping 'new Motor' from Heap to Stack allocation");
    Check(!bundle.irMod.requiresFullRuntime,
          "After stack promotion of all local objects, module requires only Minimal Runtime (Zero GC overhead)");

    std::string irText = bundle.irMod.DumpText();
    Check(irText.find("[stack-promoted]") != std::string::npos, "SSA IR dump marks Motor allocation as [stack-promoted]");
    Check(irText.find("[devirtualized]") != std::string::npos, "SSA IR dump marks Motor.GetRPM call as [devirtualized]");

    // Negative IR Verifier test: manually corrupt an IRFunction (duplicate SSA ID and missing terminator)
    {
        Robix::Frontend::DiagnosticBag badDiags;
        Robix::Frontend::SemanticAnalyzer dummySema(badDiags);
        Robix::IR::IRPassManager verifier(dummySema, badDiags);

        Robix::IR::IRModule corruptMod;
        Robix::IR::IRFunction badFn;
        badFn.name = "CorruptFn";
        Robix::IR::IRBasicBlock bb;
        bb.label = "entry";
        Robix::IR::IRInstruction inst1;
        inst1.opcode     = Robix::IR::IROpcode::ConstInt;
        inst1.dest.ssaId = 1;
        bb.instructions.push_back(inst1);
        bb.instructions.push_back(inst1); // Duplicate SSA ID %1 and no Return/Jump terminator!
        badFn.blocks.push_back(bb);
        corruptMod.functions.push_back(badFn);

        bool verifyOk = verifier.VerifyIRModule(corruptMod);
        Check(!verifyOk && badDiags.ContainsCode("I4001"),
              "IR Verifier catches duplicate SSA register definition and missing terminator with I4001");
    }

    // Compiler Crash Recovery test
    {
        Robix::Frontend::DiagnosticBag iceDiags;
        Robix::Frontend::SemanticAnalyzer dummySema(iceDiags);
        Robix::IR::IRPassManager passMgr(dummySema, iceDiags);

        bool recovered = passMgr.ExecuteWithCrashRecovery("SimulatedPassFault", []() {
            throw std::runtime_error("Simulated internal pass failure");
        });
        Check(!recovered && iceDiags.ContainsCode("ICE9999"),
              "Compiler crash recovery catches internal exception and emits structured ICE9999 diagnostic");
    }
}

void TestBytecodeVMExecutionRecursionAndOOP() {
    const std::string src = R"(
        package Robotics.Control;

        public class PIDState {
            public Int gain = 0;

            public PIDState(Int initialGain) {
                this.gain = initialGain;
            }

            public Int Compute(Int errorVal) {
                return this.gain * errorVal;
            }
        }

        public Int Factorial(Int n) {
            if n <= 1 {
                return 1;
            }
            return n * Factorial(n - 1);
        }

        public Int Main() {
            var sum = 0;
            for i in 1..=5 {
                sum += i;
            }
            let fact5 = Factorial(5);
            let pid = new PIDState(3);
            let output = pid.Compute(10);
            Print($"Sum={sum}");
            Print(fact5 + output + sum);
            return fact5 + output + sum;
        }
    )";

    auto bundle = BuildDualPipeline(src, "VMExec.rx", true);
    Check(!bundle.diagnostics.HasErrors(), "VMExec.rx compiles cleanly to verified BytecodeModule");

    Robix::VM::BytecodeVM vm(bundle.bcMod, 4);
    Robix::VM::VMValue mainResult = vm.ExecuteFunction("Main");

    // fact5 (120) + output (3 * 10 = 30) + sum (1+2+3+4+5 = 15) = 165
    Check(mainResult.IsInt() && mainResult.AsInt() == 165,
          "BytecodeVM executes loops, recursion (Factorial(5)=120), and OOP method calls (3*10=30) -> 165");
    Check(vm.GetStdoutBuffer().find("165\n") != std::string::npos,
          "BytecodeVM captures Print(165) output in stdout buffer");

    auto hotFns = vm.GetHotJITCandidateFunctions();
    Check(!hotFns.empty() && hotFns[0] == "Factorial",
          "BytecodeVM JIT profiler identifies recursive 'Factorial' as a hot JIT candidate function");
}

void TestSingleFileStandaloneExecutionAndSingleFileTesting() {
    // 1. Standalone Single-File Script (Void Main() -> Exit Code 0)
    {
        const std::string singleFile = R"(
            public Void Main() {
                let speed = 42;
                Print(speed);
            }
        )";
        auto bundle = BuildDualPipeline(singleFile, "QuickScript.rx", true);
        Robix::VM::BytecodeVM vm(bundle.bcMod);
        std::int32_t exitCode = vm.ExecuteMainAndGetExitCode();
        Check(exitCode == 0 && vm.GetStdoutBuffer() == "42\n",
              "Single-file standalone execution of QuickScript.rx runs Void Main() and returns exit code 0");
    }

    // 2. Standalone Single-File Test Runner (`robix test Motor.test.rx`)
    {
        const std::string testFile = R"(
            public Int DoubleSpeed(Int s) {
                return s * 2;
            }

            TestFile {
                Covers = "Motor.rx";
            }

            Test "DoubleSpeed positive value" {
                let res = DoubleSpeed(60);
                AssertEqual(res, 120);
            }

            Test "DoubleSpeed zero value" {
                let res = DoubleSpeed(0);
                AssertEqual(res, 0);
                AssertTrue(res == 0);
            }
        )";
        auto bundle = BuildDualPipeline(testFile, "Motor.test.rx", true);
        Robix::VM::BytecodeVM vm(bundle.bcMod);
        auto suiteRes = vm.ExecuteAllTests();
        Check(suiteRes.passedTests == 2 && suiteRes.failedTests == 0 && vm.GetAssertionPassCount() == 3,
              "Single-file test runner executes all Test blocks in Motor.test.rx with 2/2 tests and 3/3 assertions passing");
    }
}

void TestWriteOnceCompileNativeEverywhereLLVMCodeGen() {
    const std::string osIndependentCode = R"(
        package System.CrossPlatform;

        public Int Main() {
            File.WriteText("telemetry.log", "ONLINE");
            let text = File.ReadText("telemetry.log");
            let now = Time.NowMicroseconds();
            Unsafe {
                ASM("NOP");
            }
            return 0;
        }
    )";

    auto bundle = BuildDualPipeline(osIndependentCode, "CrossPlatform.rx", true);
    Check(!bundle.diagnostics.HasErrors(), "OS-independent Robix code lowers to SSA IR without #ifdef boilerplate");

    // 1. Verify BytecodeVM execution of OS-independent File & Time intrinsics
    {
        Robix::VM::BytecodeVM vm(bundle.bcMod);
        std::int32_t code = vm.ExecuteMainAndGetExitCode();
        Check(code == 0, "BytecodeVM executes OS-independent File.WriteText/ReadText and Time.NowMicroseconds");
    }

    // 2. Compile the EXACT SAME SSA IR to Windows x64, Linux x64, macOS ARM64, and Bare-Metal ARM
    {
        Robix::CodeGen::NativeCodeGenOptions winOpts;
        winOpts.target  = Robix::CodeGen::TargetPlatform::WindowsX64;
        winOpts.profile = Robix::CodeGen::BuildProfile::Performance;
        Robix::CodeGen::LLVMIRGenerator winGen(bundle.diagnostics, winOpts);
        std::string winLLVM = winGen.GenerateLLVMIR(bundle.irMod);

        Check(winLLVM.find("target triple = \"x86_64-pc-windows-msvc\"") != std::string::npos,
              "Windows x64 LLVM IR emits x86_64-pc-windows-msvc target triple");
        Check(winLLVM.find("@Win32_CreateFileW_WriteFile") != std::string::npos &&
              winLLVM.find("@Win32_QueryPerformanceCounter") != std::string::npos,
              "Windows backend inlines File.WriteText and Time.NowMicroseconds directly to Win32 native calls");
        Check(winLLVM.find("alwaysinline nounwind uwtable") != std::string::npos,
              "Performance build profile attaches 'alwaysinline nounwind uwtable' attributes");
    }

    {
        Robix::CodeGen::NativeCodeGenOptions linuxOpts;
        linuxOpts.target  = Robix::CodeGen::TargetPlatform::LinuxX64;
        linuxOpts.profile = Robix::CodeGen::BuildProfile::RealTime;
        Robix::CodeGen::LLVMIRGenerator linuxGen(bundle.diagnostics, linuxOpts);
        std::string linuxLLVM = linuxGen.GenerateLLVMIR(bundle.irMod);

        Check(linuxLLVM.find("target triple = \"x86_64-unknown-linux-gnu\"") != std::string::npos,
              "Linux x64 LLVM IR emits x86_64-unknown-linux-gnu target triple");
        Check(linuxLLVM.find("@Linux_sys_openat_write") != std::string::npos &&
              linuxLLVM.find("@Linux_clock_gettime_monotonic") != std::string::npos,
              "Linux backend inlines File.WriteText and Time.NowMicroseconds directly to Linux syscalls");
        Check(linuxLLVM.find("Runtime Profile: Minimal (Zero GC/Reflection Overhead)") != std::string::npos,
              "RealTime build profile enforces Minimal Runtime with zero GC overhead");
    }

    {
        Robix::CodeGen::NativeCodeGenOptions macOpts;
        macOpts.target  = Robix::CodeGen::TargetPlatform::MacOSARM64;
        macOpts.profile = Robix::CodeGen::BuildProfile::Release;
        Robix::CodeGen::LLVMIRGenerator macGen(bundle.diagnostics, macOpts);
        std::string macLLVM = macGen.GenerateLLVMIR(bundle.irMod);

        Check(macLLVM.find("target triple = \"arm64-apple-macos14.0\"") != std::string::npos &&
              macLLVM.find("@Darwin_mach_absolute_time") != std::string::npos,
              "macOS ARM64 backend lowers Time.NowMicroseconds directly to Darwin mach_absolute_time");
    }
}

void TestIncrementalBuildCache() {
    Robix::CodeGen::IncrementalBuildCache cache;
    const std::string srcV1 = "public Int Main() { return 10; }";
    const std::string srcV2 = "public Int Main() { return 20; }";
    const std::vector<std::string> imports = { "Robix.Core", "Robix.Math" };

    std::uint64_t fp1 = Robix::CodeGen::IncrementalBuildCache::ComputeFingerprint(
        srcV1, imports, Robix::CodeGen::BuildProfile::Release, Robix::CodeGen::TargetPlatform::WindowsX64);

    std::string cachedIR;
    Check(!cache.TryGetCachedLLVMIR("Main.rx", fp1, cachedIR), "First build of Main.rx is a cache miss");

    cache.StoreCachedLLVMIR("Main.rx", fp1, "; Cached LLVM IR v1");
    Check(cache.TryGetCachedLLVMIR("Main.rx", fp1, cachedIR) && cachedIR == "; Cached LLVM IR v1",
          "Second build with identical source, imports, profile, and target hits incremental cache");

    std::uint64_t fpModifiedSource = Robix::CodeGen::IncrementalBuildCache::ComputeFingerprint(
        srcV2, imports, Robix::CodeGen::BuildProfile::Release, Robix::CodeGen::TargetPlatform::WindowsX64);
    Check(!cache.TryGetCachedLLVMIR("Main.rx", fpModifiedSource, cachedIR),
          "Modifying source code invalidates incremental build cache entry");

    std::uint64_t fpDifferentTarget = Robix::CodeGen::IncrementalBuildCache::ComputeFingerprint(
        srcV1, imports, Robix::CodeGen::BuildProfile::Release, Robix::CodeGen::TargetPlatform::LinuxX64);
    Check(!cache.TryGetCachedLLVMIR("Main.rx", fpDifferentTarget, cachedIR),
          "Changing target OS invalidates incremental build cache entry");
}

} // namespace

int main() {
    TestSSAIRVerifierAndOptimizationPasses();
    TestBytecodeVMExecutionRecursionAndOOP();
    TestSingleFileStandaloneExecutionAndSingleFileTesting();
    TestWriteOnceCompileNativeEverywhereLLVMCodeGen();
    TestIncrementalBuildCache();

    std::cout << "========================================================\n";
    std::cout << "Robix Stage 4 Dual Execution, SSA IR & LLVM Tests: "
              << g_passedTests << " passed, "
              << g_failedTests << " failed.\n";
    std::cout << "========================================================\n";

    return (g_failedTests == 0) ? 0 : 1;
}

