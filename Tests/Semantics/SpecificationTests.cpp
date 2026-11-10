#include "../../Compiler/Types/SpecValidator.hpp"

#include <cstdlib>
#include <iostream>

namespace {

int g_passedTests = 0;
int g_failedTests = 0;

void Expect(bool condition, const char* description) {
    if (condition) {
        ++g_passedTests;
        std::cout << "  [PASS] " << description << "\n";
    } else {
        ++g_failedTests;
        std::cerr << "  [FAIL] " << description << "\n";
    }
}

void TestNamingAndAcronymConventions() {
    using namespace Robix::Spec;
    std::cout << "\n--- Suite 1: Naming & Acronym Capitalization Policy ---\n";

    // Valid PascalCase + Uppercase Acronyms
    Expect(IsValidPascalCaseWithAcronyms("RobotController"), "PascalCase: RobotController");
    Expect(IsValidPascalCaseWithAcronyms("APIManager"),      "PascalCase Acronym: APIManager");
    Expect(IsValidPascalCaseWithAcronyms("HTTPClient"),      "PascalCase Acronym: HTTPClient");
    Expect(IsValidPascalCaseWithAcronyms("GPUController"),   "PascalCase Acronym: GPUController");
    Expect(IsValidPascalCaseWithAcronyms("CPUInfo"),         "PascalCase Acronym: CPUInfo");
    Expect(IsValidPascalCaseWithAcronyms("USBDevice"),       "PascalCase Acronym: USBDevice");
    Expect(IsValidPascalCaseWithAcronyms("AIModel"),         "PascalCase Acronym: AIModel");
    Expect(IsValidPascalCaseWithAcronyms("XMLParser"),       "PascalCase Acronym: XMLParser");
    Expect(IsValidPascalCaseWithAcronyms("JSONReader"),      "PascalCase Acronym: JSONReader");
    Expect(IsValidPascalCaseWithAcronyms("TCPConnection"),   "PascalCase Acronym: TCPConnection");
    Expect(IsValidPascalCaseWithAcronyms("UARTDriver"),      "PascalCase Acronym: UARTDriver");
    Expect(IsValidPascalCaseWithAcronyms("SDKBuilder"),      "PascalCase Acronym: SDKBuilder");
    Expect(IsValidPascalCaseWithAcronyms("CPPInterop"),      "PascalCase Acronym: CPPInterop");

    // Forbidden Mixed-Case Acronyms in PascalCase
    Expect(!IsValidPascalCaseWithAcronyms("ApiManager"),     "Reject incorrect acronym: ApiManager");
    Expect(!IsValidPascalCaseWithAcronyms("HttpClient"),     "Reject incorrect acronym: HttpClient");
    Expect(!IsValidPascalCaseWithAcronyms("GpuController"),  "Reject incorrect acronym: GpuController");
    Expect(!IsValidPascalCaseWithAcronyms("CpuInfo"),        "Reject incorrect acronym: CpuInfo");
    Expect(!IsValidPascalCaseWithAcronyms("UsbDevice"),      "Reject incorrect acronym: UsbDevice");
    Expect(!IsValidPascalCaseWithAcronyms("AiModel"),        "Reject incorrect acronym: AiModel");
    Expect(!IsValidPascalCaseWithAcronyms("XmlParser"),      "Reject incorrect acronym: XmlParser");
    Expect(!IsValidPascalCaseWithAcronyms("JsonReader"),     "Reject incorrect acronym: JsonReader");
    Expect(!IsValidPascalCaseWithAcronyms("TcpConnection"),  "Reject incorrect acronym: TcpConnection");
    Expect(!IsValidPascalCaseWithAcronyms("UartDriver"),     "Reject incorrect acronym: UartDriver");

    // Valid camelCase + Uppercase Acronyms
    Expect(IsValidCamelCaseWithAcronyms("motorSpeed"),       "camelCase: motorSpeed");
    Expect(IsValidCamelCaseWithAcronyms("robotName"),        "camelCase: robotName");
    Expect(IsValidCamelCaseWithAcronyms("isConnected"),      "camelCase: isConnected");
    Expect(IsValidCamelCaseWithAcronyms("apiURL"),           "camelCase Acronym: apiURL");
    Expect(IsValidCamelCaseWithAcronyms("httpResponse"),     "camelCase Acronym: httpResponse");
    Expect(IsValidCamelCaseWithAcronyms("gpuTemperature"),   "camelCase Acronym: gpuTemperature");
    Expect(IsValidCamelCaseWithAcronyms("isUSBConnected"),   "camelCase Acronym: isUSBConnected");
    Expect(!IsValidCamelCaseWithAcronyms("isUsbConnected"),  "Reject incorrect camelCase acronym: isUsbConnected");

    // Valid UPPER_SNAKE_CASE Constants
    Expect(IsValidUpperSnakeCase("MAX_SPEED"),               "UPPER_SNAKE_CASE: MAX_SPEED");
    Expect(IsValidUpperSnakeCase("PI_VALUE"),                "UPPER_SNAKE_CASE: PI_VALUE");
    Expect(IsValidUpperSnakeCase("COMPANY_NAME"),            "UPPER_SNAKE_CASE: COMPANY_NAME");
    Expect(!IsValidUpperSnakeCase("max_speed"),              "Reject lowercase constant: max_speed");
    Expect(!IsValidUpperSnakeCase("MaxSpeed"),               "Reject PascalCase constant: MaxSpeed");

    // Trojan Source Bidi Control Rejection
    Expect(!ContainsForbiddenBidiControlChars("NormalIdentifier"), "Safe UTF-8 text accepted");
    Expect(ContainsForbiddenBidiControlChars(" Bad" "\xE2\x80\xAE" "Code "), "Trojan Source U+202E RLO rejected");
}

void TestPrimitiveLayoutsAndOperatorPrecedence() {
    using namespace Robix::Spec;
    std::cout << "\n--- Suite 2: Primitive Layouts & Operator Precedence ---\n";

    PrimitiveKind kind{};
    Expect(TryResolvePrimitiveName("Int32", kind) && kind == PrimitiveKind::Int32, "Resolve Int32");
    Expect(GetPrimitiveLayout(PrimitiveKind::Int8).sizeBytes64 == 1,    "Int8 is 1 byte");
    Expect(GetPrimitiveLayout(PrimitiveKind::Int16).sizeBytes64 == 2,   "Int16 is 2 bytes");
    Expect(GetPrimitiveLayout(PrimitiveKind::Int32).sizeBytes64 == 4,   "Int32 is 4 bytes");
    Expect(GetPrimitiveLayout(PrimitiveKind::Int64).sizeBytes64 == 8,   "Int64 is 8 bytes");
    Expect(GetPrimitiveLayout(PrimitiveKind::Int).sizeBytes64 == 8,     "Int is 8 bytes on 64-bit");
    Expect(GetPrimitiveLayout(PrimitiveKind::Int).sizeBytes32 == 4,     "Int is 4 bytes on 32-bit");
    Expect(GetPrimitiveLayout(PrimitiveKind::Decimal).sizeBytes64 == 16,"Decimal is 16 bytes");
    Expect(GetPrimitiveLayout(PrimitiveKind::String).sizeBytes64 == 24, "String is 24 bytes on 64-bit");
    Expect(GetPrimitiveLayout(PrimitiveKind::Void).sizeBytes64 == 0,    "Void is 0 bytes");

    // Operator Precedence Ordering
    Expect(GetOperatorSpec(OperatorKind::MemberAccess).precedence >
           GetOperatorSpec(OperatorKind::Await).precedence,
           "MemberAccess (.) binds tighter than await");
    Expect(GetOperatorSpec(OperatorKind::Multiply).precedence >
           GetOperatorSpec(OperatorKind::Add).precedence,
           "Multiply (*) binds tighter than Add (+)");
    Expect(GetOperatorSpec(OperatorKind::Add).precedence >
           GetOperatorSpec(OperatorKind::RangeExclusive).precedence,
           "Add (+) binds tighter than Range (..)");
    Expect(GetOperatorSpec(OperatorKind::LogicalAnd).precedence >
           GetOperatorSpec(OperatorKind::LogicalOr).precedence,
           "LogicalAnd (&&) binds tighter than LogicalOr (||)");
    Expect(GetOperatorSpec(OperatorKind::NullCoalesce).precedence >
           GetOperatorSpec(OperatorKind::Assign).precedence,
           "NullCoalesce binds tighter than Assign (=)");
}

void TestVisibilityMainExitAndABIMangling() {
    using namespace Robix::Spec;
    std::cout << "\n--- Suite 3: Visibility, Main() Exit Codes & ABI Mangling ---\n";

    AccessContext externalConsumer{};
    AccessContext derivedInOtherPkg{.isDerivedClass = true};
    AccessContext samePkg{.isSamePackage = true};

    Expect(!IsAccessPermitted(AccessVisibility::Private, derivedInOtherPkg), "private hidden from derived class");
    Expect(IsAccessPermitted(AccessVisibility::Protected, derivedInOtherPkg),"protected visible to derived class");
    Expect(IsAccessPermitted(AccessVisibility::Package, samePkg),            "package visible within same package");
    Expect(!IsAccessPermitted(AccessVisibility::Package, externalConsumer),  "package hidden from external .rxapi consumer");
    Expect(IsAccessPermitted(AccessVisibility::Public, externalConsumer),    "public visible to external .rxapi consumer");

    // Flexible Main() Exit Codes
    Expect(ComputeProcessExitCode(MainReturnCategory::VoidReturn, 0, true) == 0,       "Main() -> Void exits 0");
    Expect(ComputeProcessExitCode(MainReturnCategory::IntExitCode, 4, true) == 4,      "Main() -> Int returns exact OS exit code 4");
    Expect(ComputeProcessExitCode(MainReturnCategory::StructuredOutput, 0, true) == 0, "Main() -> Object exits 0");
    Expect(ComputeProcessExitCode(MainReturnCategory::ResultType, 0, true) == 0,       "Main() -> Result Success exits 0");
    Expect(ComputeProcessExitCode(MainReturnCategory::ResultType, 0, false) == 1,      "Main() -> Result Error exits non-zero (1)");

    // Canonical ABI Name Mangling
    const std::string mangled = MangleRobixSymbol("Robotics.Motors", "Motor", "SetSpeed", "is", "v");
    Expect(mangled == "_RBX1_8Robotics6Motors_5Motor_8SetSpeed_is__v",
           "Canonical ABI v1 symbol mangling matches specification");

    // Target ABI Descriptors
    Expect(GetABITargetDescriptor(TargetArch::X86_64_SysV).maxIntRegisterArgs == 6,    "x86_64 SysV has 6 int arg regs");
    Expect(GetABITargetDescriptor(TargetArch::X86_64_Windows).maxIntRegisterArgs == 4, "x86_64 Windows has 4 int arg regs");
    Expect(GetABITargetDescriptor(TargetArch::ARM64).maxIntRegisterArgs == 8,          "ARM64 has 8 int arg regs");
    Expect(GetABITargetDescriptor(TargetArch::RISCV64).maxIntRegisterArgs == 8,        "RISC-V 64 has 8 int arg regs");
}

} // namespace

int main() {
    std::cout << "=========================================================\n";
    std::cout << " ROBIX STAGE 1 SPECIFICATION & ABI CONFORMANCE SUITE\n";
    std::cout << "=========================================================\n";

    TestNamingAndAcronymConventions();
    TestPrimitiveLayoutsAndOperatorPrecedence();
    TestVisibilityMainExitAndABIMangling();

    std::cout << "\n=========================================================\n";
    std::cout << " Passed: " << g_passedTests << " | Failed: " << g_failedTests << "\n";
    std::cout << "=========================================================\n";

    return g_failedTests == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
