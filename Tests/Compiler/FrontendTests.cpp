#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Types/TypeChecker.hpp"

#include <iostream>
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

struct FrontendRunResult {
    Robix::Frontend::DiagnosticBag diagnostics;
    Robix::Frontend::CompilationUnitAST ast;
    bool checkSucceeded = false;
};

FrontendRunResult RunFrontend(std::string_view source,
                              std::string_view fileName = "TestUnit.rx",
                              Robix::Frontend::TypeCheckerOptions options = {}) {
    FrontendRunResult res;
    Robix::Frontend::Lexer lexer(source, fileName, res.diagnostics);
    auto tokens = lexer.TokenizeAll();

    Robix::Frontend::Parser parser(std::move(tokens), fileName, res.diagnostics);
    res.ast = parser.ParseCompilationUnit();

    Robix::Frontend::TypeChecker checker(res.diagnostics, options);
    res.checkSucceeded = checker.CheckCompilationUnit(res.ast);
    return res;
}

void TestLexerFeatures() {
    Robix::Frontend::DiagnosticBag diags;
    const std::string src = R"(
        package Robotics.Motors;
        // Single-line comment
        /* Multi-line
           comment */
        const Int MAX_SPEED = 0xFF_00;
        let binMask = 0b1010_0101;
        let ratio = 3.14159;
        let msg = $"Speed: {MAX_SPEED}";
        let doc = """Multi-line text""";
        let pattern = r"^[A-Z]+$";
        let r1 = 0..10;
        let r2 = 1..=10;
        let safeVal = maybeVal?.Field ?? 42;
    )";

    Robix::Frontend::Lexer lexer(src, "LexerTest.rx", diags);
    auto tokens = lexer.TokenizeAll();

    Check(!diags.HasErrors(), "Lexer tokenizes valid Robix source without errors");
    Check(tokens.size() > 35, "Lexer produces expected token stream length");

    bool foundHex = false;
    bool foundBin = false;
    bool foundInterp = false;
    bool foundRegex = false;
    bool foundRangeInc = false;
    bool foundNullCoal = false;

    for (const auto& tok : tokens) {
        if (tok.kind == Robix::Frontend::TokenKind::IntegerLiteral && tok.lexeme == "0xFF_00") foundHex = true;
        if (tok.kind == Robix::Frontend::TokenKind::IntegerLiteral && tok.lexeme == "0b1010_0101") foundBin = true;
        if (tok.kind == Robix::Frontend::TokenKind::InterpolatedString) foundInterp = true;
        if (tok.kind == Robix::Frontend::TokenKind::RegexLiteral && tok.lexeme == "r\"^[A-Z]+$\"") foundRegex = true;
        if (tok.kind == Robix::Frontend::TokenKind::RangeIncl) foundRangeInc = true;
        if (tok.kind == Robix::Frontend::TokenKind::QuestionQuestion) foundNullCoal = true;
    }

    Check(foundHex, "Lexer recognizes hex literal with underscore separator");
    Check(foundBin, "Lexer recognizes binary literal with underscore separator");
    Check(foundInterp, "Lexer recognizes interpolated string literal $\"...\"");
    Check(foundRegex, "Lexer recognizes regex literal r\"...\"");
    Check(foundRangeInc, "Lexer recognizes inclusive range operator ..=");
    Check(foundNullCoal, "Lexer recognizes null-coalescing operator ??");

    // Trojan Source check in Lexer
    Robix::Frontend::DiagnosticBag secDiags;
    std::string trojanSrc = "Int x = 1; // " "\xE2\x80\xAE" "evil";
    Robix::Frontend::Lexer secLexer(trojanSrc, "Trojan.rx", secDiags);
    auto secTokens = secLexer.TokenizeAll();
    (void)secTokens;
    Check(secDiags.ContainsCode("S1001"), "Lexer detects Trojan Source Unicode Bidi control character (S1001)");
}

void TestParserAllConstructsAndTenLoops() {
    const std::string src = R"(
        package Robotics.Core;
        import Robix.Hardware.GPIO;

        public interface IMotor {
            public Void SetSpeed(Int rpm);
        }

        public class DCMotor : BaseActuator implements IMotor {
            private Int currentRPM = 0;

            public DCMotor(Int initialRPM) {
                this.currentRPM = initialRPM;
            }

            public override Void SetSpeed(Int rpm) {
                this.currentRPM = rpm;
            }
        }

        public Void ExerciseAllTenLoops(Array<Int> readings) {
            // 1. Exclusive range loop
            for i in 0..10 {
                Print(i);
            }
            // 2. Inclusive range loop
            for i in 1..=10 {
                Print(i);
            }
            // 3. Stepped range loop
            for i in 0..100 step 5 {
                Print(i);
            }
            // 4. Collection iteration
            for item in readings {
                Print(item);
            }
            // 5. Indexed collection iteration
            for (index, item) in readings.Enumerate() {
                Print(index);
                Print(item);
            }
            // 6. While loop
            var count = 3;
            while count > 0 {
                count -= 1;
            }
            // 7. Do-while loop
            do {
                count += 1;
            } while count < 2;
            // 8. Repeat loop
            repeat 5 {
                Print(count);
            }
            // 9. Infinite loop with break/continue
            loop {
                if count == 0 {
                    continue;
                }
                break;
            }
            // 10. C-style for loop
            for (Int i = 0; i < 4; i += 1) {
                Print(i);
            }
        }
    )";

    auto res = RunFrontend(src, "LoopsAndOOP.rx");
    Check(!res.diagnostics.HasErrors(), "Parser & TypeChecker accept OOP class hierarchy and all 10 loop forms");
    Check(res.ast.packageName == "Robotics.Core", "Parser extracts package declaration Robotics.Core");
    Check(res.ast.imports.size() == 1 && res.ast.imports[0] == "Robix.Hardware.GPIO", "Parser extracts import declaration");
    Check(res.ast.declarations.size() == 3, "Parser produces 3 top-level declarations (IMotor, DCMotor, ExerciseAllTenLoops)");
}

void TestTypeInferenceAndReturnInference() {
    const std::string src = R"(
        package Examples.Inference;

        const MAX_SPEED = 500;

        public CalculateTotal(Int price, Int tax) {
            return price + tax;
        }

        public Void Main() {
            let robotName = "Atlas";
            var speed = 100;
            speed = 150;
            let readings = [10, 20, 30];
            let summary = {
                status = "Ready",
                total = CalculateTotal(100, 15)
            };
            Print(robotName);
            Print(speed);
            Print(readings);
            Print(summary);
        }
    )";

    auto res = RunFrontend(src, "Inference.rx");
    Check(!res.diagnostics.HasErrors(), "Type inference for let, var, const, Array<Int>, Object, and function return type succeeds");

    // Verify CalculateTotal return type was inferred as Int
    const auto& calcDecl = *res.ast.declarations[1];
    Check(calcDecl.name == "CalculateTotal", "Second declaration is CalculateTotal");
    Check(calcDecl.returnType.baseName == "Int", "CalculateTotal return type automatically inferred as Int");
}

void TestTypeErrorsAndDiagnosticsFormat() {
    // 1. Int age = "Twenty Five"; -> R1001
    {
        const std::string src = R"(
            public Void Main() {
                Int age = "Twenty Five";
            }
        )";
        auto res = RunFrontend(src, "TypeError.rx");
        Check(res.diagnostics.ContainsCode("R1001"), "Assigning String to Int emits R1001");
        std::string human = res.diagnostics.FormatHumanReadable();
        std::string json  = res.diagnostics.FormatJSON();
        Check(human.find("Robix Error [R1001]") != std::string::npos, "Human-readable diagnostic contains R1001 header");
        Check(human.find("Expected: Int") != std::string::npos, "Human-readable diagnostic contains Expected: Int");
        Check(human.find("Received: String") != std::string::npos, "Human-readable diagnostic contains Received: String");
        Check(json.find("\"code\":\"R1001\"") != std::string::npos, "JSON diagnostic contains code R1001");
    }

    // 2. Implicit Int + Float mixing -> R1001, explicit .ToFloat() -> OK
    {
        const std::string badMix = R"(
            public Void Main() {
                Int a = 10;
                Float b = 2.5;
                Float c = a + b;
            }
        )";
        auto badRes = RunFrontend(badMix, "BadMix.rx");
        Check(badRes.diagnostics.ContainsCode("R1001"), "Implicit Int + Float arithmetic is rejected with R1001");

        const std::string goodMix = R"(
            public Void Main() {
                Int a = 10;
                Float b = 2.5;
                Float c = a.ToFloat() + b;
                Print(c);
            }
        )";
        auto goodRes = RunFrontend(goodMix, "GoodMix.rx");
        Check(!goodRes.diagnostics.HasErrors(), "Explicit a.ToFloat() + b compiles cleanly with zero errors");
    }

    // 3. Reassigning immutable `let` -> R1002
    {
        const std::string src = R"(
            public Void Main() {
                let maxLimit = 50;
                maxLimit = 60;
            }
        )";
        auto res = RunFrontend(src, "ImmutableLet.rx");
        Check(res.diagnostics.ContainsCode("R1002"), "Reassigning immutable 'let' variable emits R1002");
    }

    // 4. Changing static type of `var` -> R1004
    {
        const std::string src = R"(
            public Void Main() {
                var speed = 100;
                speed = "Fast";
            }
        )";
        auto res = RunFrontend(src, "VarTypeChange.rx");
        Check(res.diagnostics.ContainsCode("R1004"), "Reassigning 'var' to a different type emits R1004");
    }

    // 5. Definite assignment check -> R1007
    {
        const std::string src = R"(
            public Void Main() {
                Int uninitVal;
                Print(uninitVal);
            }
        )";
        auto res = RunFrontend(src, "Uninit.rx");
        Check(res.diagnostics.ContainsCode("R1007"), "Reading uninitialized variable emits R1007");
    }
}

void TestNullSafetyAndNarrowing() {
    // Accessing member on String? without check -> R1005
    {
        const std::string badNull = R"(
            public Void Main() {
                String? nickname = null;
                Int len = nickname.Length();
            }
        )";
        auto res = RunFrontend(badNull, "BadNull.rx");
        Check(res.diagnostics.ContainsCode("R1005"), "Calling .Length() on nullable String? without null check emits R1005");
    }

    // Null narrowing inside `if (nickname != null)` and null-coalescing `??` -> OK
    {
        const std::string safeNull = R"(
            public Void Main() {
                String? nickname = null;
                if nickname != null {
                    Int len = nickname.Length();
                    Print(len);
                }
                String display = nickname ?? "Anonymous";
                Print(display);
            }
        )";
        auto res = RunFrontend(safeNull, "SafeNull.rx");
        Check(!res.diagnostics.HasErrors(), "Null narrowing in 'if nickname != null' and null-coalescing fallback pass with zero errors");
    }
}

void TestUnsafeBoundariesAndInlineASM() {
    // Raw pointer & Memory.Allocate & ASM outside Unsafe -> R1006
    {
        const std::string outsideUnsafe = R"(
            public Void Main() {
                Int value = 10;
                Pointer<Int> ptr = &value;
                *ptr = 20;
                ASM("NOP");
            }
        )";
        auto res = RunFrontend(outsideUnsafe, "OutsideUnsafe.rx");
        Check(res.diagnostics.ContainsCode("R1006"), "Pointer address-of, dereference, and ASM outside Unsafe block emit R1006");
    }

    // Inside Unsafe { ... } -> OK
    {
        const std::string insideUnsafe = R"(
            public Void Main() {
                Unsafe {
                    Int value = 10;
                    Pointer<Int> ptr = &value;
                    *ptr = 20;
                    Pointer<Byte> buf = Memory.Allocate(256);
                    Memory.Free(buf);
                    ASM("NOP");
                }
            }
        )";
        auto res = RunFrontend(insideUnsafe, "InsideUnsafe.rx");
        Check(!res.diagnostics.HasErrors(), "Pointer ops, Memory.Allocate/Free, and ASM inside Unsafe {} pass cleanly");
    }
}

void TestNamingConventionsStrictMode() {
    Robix::Frontend::TypeCheckerOptions opts;
    opts.strictStyleMode = true;

    const std::string badStyle = R"(
        public class HttpClient {
            public Void Main() {
                var isUsbConnected = true;
                const Int maxSpeed = 100;
            }
        }
    )";
    auto res = RunFrontend(badStyle, "BadStyle.rx", opts);
    Check(res.diagnostics.ContainsCode("L0002"), "Strict style mode rejects 'HttpClient' (must be 'HTTPClient') with L0002");
    Check(res.diagnostics.ContainsCode("L0003"), "Strict style mode rejects 'isUsbConnected' (must be 'isUSBConnected') with L0003");
    Check(res.diagnostics.ContainsCode("L0004"), "Strict style mode rejects 'maxSpeed' constant (must be 'MAX_SPEED') with L0004");
}

void TestFirstClassExplainBlockValidator() {
    // 1. Valid Explain block (full and compact)
    {
        const std::string validExplain = R"(
            public Result<Int, String> CalculateMotorSpeed(Int targetRPM, Int loadFactor) {
                Explain {
                    Description: "Calculates safe motor RPM based on load.";
                    Input {
                        targetRPM: "Requested motor revolutions per minute.";
                        loadFactor: "Current mechanical load percentage.";
                    }
                    Processing: "Subtracts load penalty and checks safety bounds.";
                    Output: "Safe motor RPM or error message.";
                    Errors {
                        InvalidLoad: "Returned when loadFactor is outside 0..100.";
                    }
                }
                if loadFactor < 0 || loadFactor > 100 {
                    return Error("Invalid load factor");
                }
                return Success(targetRPM - loadFactor);
            }
        )";
        auto res = RunFrontend(validExplain, "ValidExplain.rx");
        Check(!res.diagnostics.HasErrors(), "Complete Explain {} block matching parameters and Result return type passes");
    }

    // 2. Invalid Explain block (mismatched Input parameter name & missing Output) -> D3001
    {
        const std::string badExplain = R"(
            public Int ComputeTorque(Int currentAmps, Int motorConstant) {
                Explain {
                    Description: "Computes motor torque.";
                    Input {
                        currentAmps: "Current in amperes.";
                        wrongParamName: "Does not exist in signature.";
                    }
                    Processing: "Multiplies current by motor constant.";
                }
                return currentAmps * motorConstant;
            }
        )";
        auto res = RunFrontend(badExplain, "BadExplain.rx");
        Check(res.diagnostics.ContainsCode("D3001"), "Explain block with mismatched Input parameter and missing Output emits D3001");
    }
}

void TestNetworkServiceAndTestFileConstructs() {
    const std::string contractAndTests = R"(
        package Contracts;

        Service MotorService {
            Async GetTelemetry() -> Object;
            Stream SubscribeStatus() -> String;
        }

        TestFile {
            Covers = "Motor.rx";
        }

        Test "Motor speed calculation works" {
            let speed = 120;
            AssertEqual(speed, 120);
        }
    )";

    auto res = RunFrontend(contractAndTests, "Motor.test.rx");
    Check(!res.diagnostics.HasErrors(), "Parser & TypeChecker support Service contracts, TestFile metadata, and Test blocks");
}

} // namespace

int main() {
    TestLexerFeatures();
    TestParserAllConstructsAndTenLoops();
    TestTypeInferenceAndReturnInference();
    TestTypeErrorsAndDiagnosticsFormat();
    TestNullSafetyAndNarrowing();
    TestUnsafeBoundariesAndInlineASM();
    TestNamingConventionsStrictMode();
    TestFirstClassExplainBlockValidator();
    TestNetworkServiceAndTestFileConstructs();

    std::cout << "========================================================\n";
    std::cout << "Robix Stage 2 Frontend & TypeChecker Tests: "
              << g_passedTests << " passed, "
              << g_failedTests << " failed.\n";
    std::cout << "========================================================\n";

    return (g_failedTests == 0) ? 0 : 1;
}
