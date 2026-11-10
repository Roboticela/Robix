#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Semantic/SemanticAnalyzer.hpp"
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

struct FullAnalysisResult {
    Robix::Frontend::DiagnosticBag diagnostics;
    Robix::Frontend::CompilationUnitAST ast;
    Robix::Frontend::SemanticAnalyzer analyzer;
    bool succeeded = false;

    explicit FullAnalysisResult(Robix::Frontend::SemanticOptions semOpts = {})
        : analyzer(diagnostics, semOpts) {}
};

FullAnalysisResult AnalyzeProgram(std::string_view source,
                             std::string_view fileName = "SemanticUnit.rx",
                             Robix::Frontend::SemanticOptions semOpts = {}) {
    FullAnalysisResult res(semOpts);
    Robix::Frontend::Lexer lexer(source, fileName, res.diagnostics);
    auto tokens = lexer.TokenizeAll();

    Robix::Frontend::Parser parser(std::move(tokens), fileName, res.diagnostics);
    res.ast = parser.ParseCompilationUnit();

    Robix::Frontend::TypeChecker typeChecker(res.diagnostics);
    typeChecker.CheckCompilationUnit(res.ast);

    res.succeeded = res.analyzer.AnalyzeCompilationUnit(res.ast);
    return res;
}

void TestOOPInheritanceVTableAndDevirtualization() {
    const std::string src = R"(
        package Robotics.Actuators;

        public interface IActuator {
            public Void Enable();
            public Int ReadPosition();
        }

        public abstract class BaseMotor implements IActuator {
            protected Int currentPos = 0;

            public Void Enable() {
                this.currentPos = 0;
            }

            public abstract Int ReadPosition();

            public final Void EmergencyBrake() {
                this.currentPos = 0;
            }
        }

        public final class ServoMotor extends BaseMotor {
            private Int targetAngle = 90;

            public ServoMotor(Int angle) : Base() {
                this.targetAngle = angle;
            }

            public Destructor() {
                this.targetAngle = 0;
            }

            public override Int ReadPosition() {
                return this.currentPos + this.targetAngle;
            }
        }

        public partial class RobotChassis {
            public Int wheelCount = 4;
        }

        public partial class RobotChassis {
            public Void Calibrate() {
                this.wheelCount = 4;
            }
        }

        public struct Vector2 {
            public Int x = 0;
            public Int y = 0;

            public Vector2 operator +(Vector2 other) {
                return new Vector2();
            }
        }
    )";

    auto res = AnalyzeProgram(src, "OOPValid.rx");
    Check(!res.diagnostics.HasErrors(), "Valid OOP hierarchy with abstract class, interface, override, final, destructor, partial class, and operator+ passes");

    const auto* servoInfo = res.analyzer.FindClassOrType("ServoMotor");
    Check(servoInfo != nullptr, "ServoMotor registered in SemanticAnalyzer");
    if (servoInfo) {
        Check(servoInfo->hasCustomDestructor, "ServoMotor destructor is tracked");
        Check(servoInfo->vtable.size() >= 3, "ServoMotor vtable inherits and overrides BaseMotor slots");
    }

    Check(res.analyzer.CanDevirtualizeCall("ServoMotor", "ReadPosition"),
          "Calls on final class ServoMotor are devirtualizable at compile time");
    Check(res.analyzer.CanDevirtualizeCall("BaseMotor", "EmergencyBrake"),
          "Calls to final method BaseMotor.EmergencyBrake are devirtualizable");

    const auto* chassisInfo = res.analyzer.FindClassOrType("RobotChassis");
    Check(chassisInfo != nullptr && chassisInfo->fields.size() == 1 && chassisInfo->methods.size() == 1,
          "Partial class declarations for RobotChassis are merged into a single SemanticClassInfo");
}

void TestOOPNegativeSemanticRules() {
    // 1. Inheriting from a final class -> R2001
    {
        const std::string src = R"(
            public final class SealedController {}
            public class CustomController extends SealedController {}
        )";
        auto res = AnalyzeProgram(src);
        Check(res.diagnostics.ContainsCode("R2001"), "Inheriting from final class emits R2001");
    }

    // 2. Instantiating an abstract class or interface -> R2002
    {
        const std::string src = R"(
            public abstract class AbstractSensor {
                public abstract Int Sample();
            }
            public Void Main() {
                let s = new AbstractSensor();
            }
        )";
        auto res = AnalyzeProgram(src);
        Check(res.diagnostics.ContainsCode("R2002"), "Instantiating abstract class emits R2002");
    }

    // 3. Circular inheritance -> R2003
    {
        const std::string src = R"(
            public class NodeA extends NodeB {}
            public class NodeB extends NodeA {}
        )";
        auto res = AnalyzeProgram(src);
        Check(res.diagnostics.ContainsCode("R2003"), "Circular class inheritance emits R2003");
    }

    // 4. Missing required interface or abstract method -> R2004
    {
        const std::string src = R"(
            public interface ISensor {
                public Int ReadValue();
            }
            public class LidarSensor implements ISensor {
                // Missing ReadValue()
            }
        )";
        auto res = AnalyzeProgram(src);
        Check(res.diagnostics.ContainsCode("R2004"), "Concrete class missing interface method emits R2004");
    }

    // 5. Method marked override when no base/interface method exists -> R2005
    {
        const std::string src = R"(
            public class StandaloneMotor {
                public override Void NonExistentBaseMethod() {}
            }
        )";
        auto res = AnalyzeProgram(src);
        Check(res.diagnostics.ContainsCode("R2005"), "Method marked override without base/interface method emits R2005");
    }

    // 6. Overriding a final method -> R2006 & Method hiding without override -> R2007
    {
        const std::string src = R"(
            public class BaseDevice {
                public final Void LockDevice() {}
                public Void ResetDevice() {}
            }
            public class SubDevice extends BaseDevice {
                public override Void LockDevice() {}
                public Void ResetDevice() {}
            }
        )";
        auto res = AnalyzeProgram(src);
        Check(res.diagnostics.ContainsCode("R2006"), "Overriding final base method emits R2006");
        Check(res.diagnostics.ContainsCode("R2007"), "Hiding base method without override emits R2007");
    }

    // 7. Access visibility violation (private member access outside class) -> R2008
    {
        const std::string src = R"(
            public class BatteryPack {
                private Int secretCellVoltage = 4200;
            }
            public Void Main() {
                let pack = new BatteryPack();
                Int v = pack.secretCellVoltage;
            }
        )";
        auto res = AnalyzeProgram(src);
        Check(res.diagnostics.ContainsCode("R2008"), "Accessing private field outside declaring class emits R2008");
    }
}

void TestProceduralNamedArgsDefaultParamsAndVariadics() {
    const std::string validProc = R"(
        public Void MoveRobot(Float speed, Bool emergencyStop = false, Int retryCount = 3) {
            Print(speed);
        }

        public Void LogValues(...Int values) {
            Print(values);
        }

        public Void Main() {
            // Positional + default parameters
            MoveRobot(25.0);
            // Out-of-order named arguments
            MoveRobot(emergencyStop: true, speed: 50.0);
            // Variadic arguments
            LogValues(1, 2, 3, 4, 5);
        }
    )";
    auto res = AnalyzeProgram(validProc);
    Check(!res.diagnostics.HasErrors(), "Named arguments, default parameters, and variadic functions pass semantic checks");

    // Negative test: unknown named argument and missing required argument -> R2010
    const std::string badCall = R"(
        public Void MoveRobot(Float speed, Bool emergencyStop = false) {}
        public Void Main() {
            MoveRobot(unknownArg: 10.0);
        }
    )";
    auto badRes = AnalyzeProgram(badCall);
    Check(badRes.diagnostics.ContainsCode("R2010"), "Unknown named argument and missing required parameter emit R2010");
}

void TestFunctionalClosuresStreamFusionAndExhaustiveMatch() {
    // 1. Stream fusion pipeline detection (.Filter().Map().Reduce()) + exhaustive enum match
    const std::string src = R"(
        public enum RobotState {
            Idle,
            Moving,
            Charging
        }

        public Void ProcessTelemetry(Array<Int> readings, RobotState state) {
            let total = readings
                .Filter(x => x > 0)
                .Map(x => x * 2)
                .Reduce(0, (sum, x) => sum + x);

            let statusText = match state {
                RobotState.Idle => "Ready",
                RobotState.Moving => "Active",
                RobotState.Charging => "PluggedIn"
            };
            Print(total);
            Print(statusText);
        }
    )";
    auto res = AnalyzeProgram(src);
    Check(!res.diagnostics.HasErrors(), "Functional Filter/Map/Reduce pipeline and exhaustive Enum match expression pass");
    Check(res.analyzer.GetFusedStreamPipelines().size() == 1, "Stream fusion optimizer detects 3-stage Filter->Map->Reduce pipeline");
    if (!res.analyzer.GetFusedStreamPipelines().empty()) {
        const auto& pipe = res.analyzer.GetFusedStreamPipelines()[0];
        Check(pipe.stages.size() == 3 && pipe.stages[0] == "Filter" && pipe.stages[1] == "Map" && pipe.stages[2] == "Reduce",
              "Fused stream pipeline preserves stage order Filter -> Map -> Reduce without intermediate allocations");
    }

    // 2. Non-exhaustive match on Enum -> R2011
    const std::string nonExhaustive = R"(
        public enum RobotState {
            Idle,
            Moving,
            Charging
        }
        public Void CheckState(RobotState state) {
            match state {
                RobotState.Idle => Print("Idle"),
                RobotState.Moving => Print("Moving")
            }
        }
    )";
    auto badMatch = AnalyzeProgram(nonExhaustive);
    Check(badMatch.diagnostics.ContainsCode("R2011"), "Non-exhaustive match missing 'Charging' variant emits R2011");

    // 3. Mutating captured immutable `let` variable inside a closure -> R1002
    const std::string badClosure = R"(
        public Void Main() {
            let fixedLimit = 100;
            let fn = x => {
                fixedLimit = 200;
            };
        }
    )";
    auto badClosureRes = AnalyzeProgram(badClosure);
    Check(badClosureRes.diagnostics.ContainsCode("R1002"), "Mutating captured immutable 'let' binding inside closure emits R1002");
}

void TestDeferUsingLifetimesAndConstevalReflectionMacros() {
    const std::string src = R"(
        @SensorComponent("IMU")
        public class IMUSensor {
            public Int sampleRate = 1000;
            public Void Calibrate() {}
        }

        macro DefineConstantGetter(Int val) {
            return val;
        }

        consteval public Int Factorial(Int n) {
            if n <= 1 {
                return 1;
            }
            return n * Factorial(n - 1);
        }

        consteval public Int SumInclusive(Int limit) {
            var total = 0;
            for i in 1..=limit {
                total += i;
            }
            return total;
        }

        public Void AcquireResources() {
            using let busA = 1 {
                Print(busA);
            }
            defer {
                Print("Cleanup1");
            }
            defer {
                Print("Cleanup2");
            }
        }
    )";

    auto res = AnalyzeProgram(src);
    Check(!res.diagnostics.HasErrors(), "Defer, using, consteval functions, attributes, and macros pass semantic analysis");

    // Verify LIFO cleanup schedule for AcquireResources
    const auto& plans = res.analyzer.GetCleanupPlans();
    Check(!plans.empty(), "Deterministic cleanup plan generated for AcquireResources");
    if (!plans.empty()) {
        const auto& actions = plans[0].lifoCleanupActions;
        Check(actions.size() == 3, "Three cleanup actions registered (2 defer blocks + 1 using block)");
        // The last registered defer runs before the earlier defer
        Check(actions[0].find("Defer@") != std::string::npos &&
              actions[1].find("Defer@") != std::string::npos &&
              actions[2] == "Using(busA)",
              "Defer and using actions are scheduled in deterministic LIFO reverse declaration order");
    }

    // Verify compile-time consteval execution
    auto fact6 = res.analyzer.EvaluateConstevalCall("Factorial", { Robix::Frontend::ConstevalValue{ std::int64_t(6) } });
    Check(fact6.IsInt() && fact6.AsInt() == 720, "consteval Factorial(6) evaluates to 720 at compile time");

    auto sum10 = res.analyzer.EvaluateConstevalCall("SumInclusive", { Robix::Frontend::ConstevalValue{ std::int64_t(10) } });
    Check(sum10.IsInt() && sum10.AsInt() == 55, "consteval SumInclusive(10) evaluates to 55 at compile time");

    // Verify Reflection metadata and Macro registration
    auto refl = res.analyzer.GetReflectionMetadata("IMUSensor");
    Check(refl.has_value() && refl->fieldNames.size() == 1 && refl->methodNames.size() == 1 &&
          refl->attributeNames.size() == 1 && refl->attributeNames[0] == "SensorComponent",
          "Opt-in reflection metadata captures fields, methods, and attributes of IMUSensor");
    Check(res.analyzer.HasMacro("DefineConstantGetter"), "Hygienic macro DefineConstantGetter is registered");

    // Negative test 1: Bounded consteval step limit -> R2013
    {
        const std::string infiniteConsteval = R"(
            consteval public Int InfiniteLoop(Int x) {
                var cur = x;
                while cur > 0 {
                    cur += 1;
                }
                return cur;
            }
        )";
        Robix::Frontend::SemanticOptions opts;
        opts.maxConstevalSteps = 200;
        auto infRes = AnalyzeProgram(infiniteConsteval, "InfiniteConsteval.rx", opts);
        (void)infRes.analyzer.EvaluateConstevalCall("InfiniteLoop", { Robix::Frontend::ConstevalValue{ std::int64_t(1) } });
        Check(infRes.diagnostics.ContainsCode("R2013"), "Non-terminating consteval function exceeding step limit emits R2013");
    }

    // Negative test 2: @RealTime function using throw or Memory.Allocate -> R2015
    {
        const std::string badRealTime = R"(
            @RealTime
            public Void ControlLoop() {
                Unsafe {
                    let p = Memory.Allocate(64);
                }
                throw "Fault";
            }
        )";
        auto rtRes = AnalyzeProgram(badRealTime);
        Check(rtRes.diagnostics.ContainsCode("R2015"), "@RealTime function using Memory.Allocate or throw emits R2015");
    }

    // Negative test 3: Strict unhandled Result<T, E> -> R2012
    {
        const std::string ignoredResult = R"(
            public Result<Int, String> ArmMotor() {
                return Success(1);
            }
            public Void Main() {
                ArmMotor();
            }
        )";
        Robix::Frontend::SemanticOptions opts;
        opts.strictResultHandling = true;
        auto resIgnored = AnalyzeProgram(ignoredResult, "IgnoredResult.rx", opts);
        Check(resIgnored.diagnostics.ContainsCode("R2012"), "Silently discarding Result<T, E> in strict mode emits R2012");
    }
}

} // namespace

int main() {
    TestOOPInheritanceVTableAndDevirtualization();
    TestOOPNegativeSemanticRules();
    TestProceduralNamedArgsDefaultParamsAndVariadics();
    TestFunctionalClosuresStreamFusionAndExhaustiveMatch();
    TestDeferUsingLifetimesAndConstevalReflectionMacros();

    std::cout << "========================================================\n";
    std::cout << "Robix Stage 3 Core Language Semantics Tests: "
              << g_passedTests << " passed, "
              << g_failedTests << " failed.\n";
    std::cout << "========================================================\n";

    return (g_failedTests == 0) ? 0 : 1;
}

