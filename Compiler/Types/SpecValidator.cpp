#include "SpecValidator.hpp"

#include <array>
#include <cctype>
#include <sstream>

namespace Robix::Spec {

namespace {

constexpr std::array<PrimitiveTypeLayout, 21> PRIMITIVE_TABLE = {{
    {PrimitiveKind::Bool,    "Bool",    "b",   1,  1,  1,  false, false, false},
    {PrimitiveKind::Byte,    "Byte",    "u8",  1,  1,  1,  false, true,  false},
    {PrimitiveKind::Char,    "Char",    "c",   4,  4,  4,  false, false, false},
    {PrimitiveKind::Int8,    "Int8",    "i8",  1,  1,  1,  true,  true,  false},
    {PrimitiveKind::Int16,   "Int16",   "i16", 2,  2,  2,  true,  true,  false},
    {PrimitiveKind::Int32,   "Int32",   "i32", 4,  4,  4,  true,  true,  false},
    {PrimitiveKind::Int64,   "Int64",   "i64", 8,  8,  8,  true,  true,  false},
    {PrimitiveKind::Int,     "Int",     "is",  8,  4,  8,  true,  true,  false},
    {PrimitiveKind::UInt8,   "UInt8",   "u8",  1,  1,  1,  false, true,  false},
    {PrimitiveKind::UInt16,  "UInt16",  "u16", 2,  2,  2,  false, true,  false},
    {PrimitiveKind::UInt32,  "UInt32",  "u32", 4,  4,  4,  false, true,  false},
    {PrimitiveKind::UInt64,  "UInt64",  "u64", 8,  8,  8,  false, true,  false},
    {PrimitiveKind::UInt,    "UInt",    "us",  8,  4,  8,  false, true,  false},
    {PrimitiveKind::Float16, "Float16", "f16", 2,  2,  2,  true,  true,  true},
    {PrimitiveKind::Float32, "Float32", "f32", 4,  4,  4,  true,  true,  true},
    {PrimitiveKind::Float64, "Float64", "f64", 8,  8,  8,  true,  true,  true},
    {PrimitiveKind::Float,   "Float",   "f64", 8,  8,  8,  true,  true,  true},
    {PrimitiveKind::Double,  "Double",  "f64", 8,  8,  8,  true,  true,  true},
    {PrimitiveKind::Decimal, "Decimal", "d",   16, 16, 16, true,  true,  true},
    {PrimitiveKind::String,  "String",  "s",   24, 12, 8,  false, false, false},
    {PrimitiveKind::Void,    "Void",    "v",   0,  0,  1,  false, false, false},
}};

constexpr std::array<OperatorSpec, 41> OPERATOR_TABLE = {{
    {OperatorKind::MemberAccess,     ".",     17, Associativity::LeftToRight},
    {OperatorKind::NullSafeMember,   "?.",    17, Associativity::LeftToRight},
    {OperatorKind::ErrorPropagate,   "?",     17, Associativity::LeftToRight},
    {OperatorKind::PostfixIncrement, "++",    17, Associativity::LeftToRight},
    {OperatorKind::PostfixDecrement, "--",    17, Associativity::LeftToRight},
    {OperatorKind::LogicalNot,       "!",     16, Associativity::RightToLeft},
    {OperatorKind::BitwiseNot,       "~",     16, Associativity::RightToLeft},
    {OperatorKind::UnaryMinus,       "-",     16, Associativity::RightToLeft},
    {OperatorKind::UnaryPlus,        "+",     16, Associativity::RightToLeft},
    {OperatorKind::Dereference,      "*",     16, Associativity::RightToLeft},
    {OperatorKind::AddressOf,        "&",     16, Associativity::RightToLeft},
    {OperatorKind::PrefixIncrement,  "++",    16, Associativity::RightToLeft},
    {OperatorKind::PrefixDecrement,  "--",    16, Associativity::RightToLeft},
    {OperatorKind::Await,            "await", 16, Associativity::RightToLeft},
    {OperatorKind::Multiply,         "*",     15, Associativity::LeftToRight},
    {OperatorKind::Divide,           "/",     15, Associativity::LeftToRight},
    {OperatorKind::Modulo,           "%",     15, Associativity::LeftToRight},
    {OperatorKind::Add,              "+",     14, Associativity::LeftToRight},
    {OperatorKind::Subtract,         "-",     14, Associativity::LeftToRight},
    {OperatorKind::ShiftLeft,        "<<",    13, Associativity::LeftToRight},
    {OperatorKind::ShiftRight,       ">>",    13, Associativity::LeftToRight},
    {OperatorKind::RangeExclusive,   "..",    12, Associativity::None},
    {OperatorKind::RangeInclusive,   "..=",   12, Associativity::None},
    {OperatorKind::Less,             "<",     11, Associativity::LeftToRight},
    {OperatorKind::LessEqual,        "<=",    11, Associativity::LeftToRight},
    {OperatorKind::Greater,          ">",     11, Associativity::LeftToRight},
    {OperatorKind::GreaterEqual,     ">=",    11, Associativity::LeftToRight},
    {OperatorKind::TypeIs,           "is",    11, Associativity::LeftToRight},
    {OperatorKind::TypeAs,           "as",    11, Associativity::LeftToRight},
    {OperatorKind::Equal,            "==",    10, Associativity::LeftToRight},
    {OperatorKind::NotEqual,         "!=",    10, Associativity::LeftToRight},
    {OperatorKind::BitwiseAnd,       "&",     9,  Associativity::LeftToRight},
    {OperatorKind::BitwiseXor,       "^",     8,  Associativity::LeftToRight},
    {OperatorKind::BitwiseOr,        "|",     7,  Associativity::LeftToRight},
    {OperatorKind::LogicalAnd,       "&&",    6,  Associativity::LeftToRight},
    {OperatorKind::LogicalOr,        "||",    5,  Associativity::LeftToRight},
    {OperatorKind::NullCoalesce,     "??",    4,  Associativity::LeftToRight},
    {OperatorKind::Ternary,          "?:",    3,  Associativity::RightToLeft},
    {OperatorKind::LambdaArrow,      "=>",    2,  Associativity::RightToLeft},
    {OperatorKind::Assign,           "=",     1,  Associativity::RightToLeft},
    {OperatorKind::AddAssign,        "+=",    1,  Associativity::RightToLeft},
}};

// Official forbidden lowercase-acronym patterns (must be uppercase: API, HTTP, GPU, CPU, USB, AI, XML, JSON, TCP, UART, SDK, CPP, PWM, GPIO, I2C, SPI, CAN, DMA)
constexpr std::array<std::string_view, 18> FORBIDDEN_MIXED_ACRONYMS = {{
    "Api", "Http", "Gpu", "Cpu", "Usb", "Ai", "Xml", "Json", "Tcp",
    "Uart", "Sdk", "Cpp", "Pwm", "Gpio", "I2c", "Spi", "Can", "Dma"
}};

bool HasForbiddenMixedCaseAcronym(std::string_view identifier) noexcept {
    for (std::string_view bad : FORBIDDEN_MIXED_ACRONYMS) {
        std::size_t pos = identifier.find(bad);
        while (pos != std::string_view::npos) {
            const std::size_t endPos = pos + bad.size();
            // It is a forbidden mixed-case acronym if it is at the end of the identifier
            // or followed by an uppercase letter or digit (e.g., "ApiManager", "isUsbConnected", "Http2Client")
            if (endPos == identifier.size() ||
                std::isupper(static_cast<unsigned char>(identifier[endPos])) ||
                std::isdigit(static_cast<unsigned char>(identifier[endPos]))) {
                return true;
            }
            pos = identifier.find(bad, pos + 1);
        }
    }
    return false;
}

constexpr std::array<ABITargetDescriptor, 6> ABI_TARGETS = {{
    {TargetArch::X86_64_SysV,    "x86_64-sysv",    8, 16, 16, 6, 8},
    {TargetArch::X86_64_Windows, "x86_64-windows", 8, 16, 16, 4, 4},
    {TargetArch::ARM64,          "aarch64",        8, 16, 16, 8, 8},
    {TargetArch::ARM32,          "arm32-eabihf",   4, 8,  8,  4, 16},
    {TargetArch::RISCV64,        "riscv64gc",      8, 16, 16, 8, 8},
    {TargetArch::WASM32,         "wasm32",         4, 16, 8,  0, 0},
}};

} // namespace

const PrimitiveTypeLayout& GetPrimitiveLayout(PrimitiveKind kind) noexcept {
    const auto idx = static_cast<std::size_t>(kind);
    return PRIMITIVE_TABLE[idx < PRIMITIVE_TABLE.size() ? idx : 0];
}

bool TryResolvePrimitiveName(std::string_view name, PrimitiveKind& outKind) noexcept {
    for (const auto& entry : PRIMITIVE_TABLE) {
        if (entry.name == name) {
            outKind = entry.kind;
            return true;
        }
    }
    return false;
}

const OperatorSpec& GetOperatorSpec(OperatorKind kind) noexcept {
    for (const auto& op : OPERATOR_TABLE) {
        if (op.kind == kind) {
            return op;
        }
    }
    return OPERATOR_TABLE[0];
}

bool IsValidPascalCaseWithAcronyms(std::string_view identifier) noexcept {
    if (identifier.empty()) {
        return false;
    }
    if (!std::isupper(static_cast<unsigned char>(identifier.front()))) {
        return false;
    }
    for (char ch : identifier) {
        if (ch == '_' || !std::isalnum(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return !HasForbiddenMixedCaseAcronym(identifier);
}

bool IsValidCamelCaseWithAcronyms(std::string_view identifier) noexcept {
    if (identifier.empty()) {
        return false;
    }
    if (!std::islower(static_cast<unsigned char>(identifier.front()))) {
        return false;
    }
    for (char ch : identifier) {
        if (ch == '_' || !std::isalnum(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return !HasForbiddenMixedCaseAcronym(identifier);
}

bool IsValidUpperSnakeCase(std::string_view identifier) noexcept {
    if (identifier.empty() || identifier.front() == '_' || identifier.back() == '_') {
        return false;
    }
    if (std::isdigit(static_cast<unsigned char>(identifier.front()))) {
        return false;
    }
    bool prevUnderscore = false;
    bool hasUppercase = false;
    for (char ch : identifier) {
        if (ch == '_') {
            if (prevUnderscore) {
                return false; // No double underscores
            }
            prevUnderscore = true;
        } else if (std::isupper(static_cast<unsigned char>(ch))) {
            hasUppercase = true;
            prevUnderscore = false;
        } else if (std::isdigit(static_cast<unsigned char>(ch))) {
            prevUnderscore = false;
        } else {
            return false;
        }
    }
    return hasUppercase;
}

bool ContainsForbiddenBidiControlChars(std::string_view utf8Text) noexcept {
    // Check UTF-8 sequences for U+202A..U+202E (E2 80 AA .. E2 80 AE)
    // and U+2066..U+2069 (E2 81 A6 .. E2 81 A9)
    for (std::size_t i = 0; i + 2 < utf8Text.size(); ++i) {
        const auto b0 = static_cast<unsigned char>(utf8Text[i]);
        const auto b1 = static_cast<unsigned char>(utf8Text[i + 1]);
        const auto b2 = static_cast<unsigned char>(utf8Text[i + 2]);
        if (b0 == 0xE2 && b1 == 0x80 && (b2 >= 0xAA && b2 <= 0xAE)) {
            return true;
        }
        if (b0 == 0xE2 && b1 == 0x81 && (b2 >= 0xA6 && b2 <= 0xA9)) {
            return true;
        }
    }
    return false;
}

bool IsAccessPermitted(AccessVisibility declared, const AccessContext& context) noexcept {
    switch (declared) {
        case AccessVisibility::Private:
            return context.isSameClassOrStruct;
        case AccessVisibility::Protected:
            return context.isSameClassOrStruct || context.isDerivedClass;
        case AccessVisibility::Package:
            return context.isSameClassOrStruct || context.isSameModuleFile || context.isSamePackage;
        case AccessVisibility::Internal:
            return context.isSameClassOrStruct || context.isSameModuleFile ||
                   context.isSamePackage || context.isSameBuildTarget;
        case AccessVisibility::Public:
            return true;
    }
    return false;
}

std::int32_t ComputeProcessExitCode(MainReturnCategory category,
                                    std::int64_t intReturnValue,
                                    bool isResultSuccess) noexcept {
    switch (category) {
        case MainReturnCategory::VoidReturn:
        case MainReturnCategory::StructuredOutput:
            return 0;
        case MainReturnCategory::IntExitCode:
            return static_cast<std::int32_t>(intReturnValue);
        case MainReturnCategory::ResultType:
            return isResultSuccess ? 0 : 1;
    }
    return 1;
}

std::string MangleRobixSymbol(std::string_view packageName,
                              std::string_view classOrModuleName,
                              std::string_view functionName,
                              std::string_view paramTypeCodes,
                              std::string_view returnTypeCode) {
    std::ostringstream out;
    out << MANGLE_PREFIX;

    // Encode dot-separated package segments with length prefixes
    std::size_t start = 0;
    while (start < packageName.size()) {
        std::size_t dot = packageName.find('.', start);
        std::string_view segment = (dot == std::string_view::npos)
                                       ? packageName.substr(start)
                                       : packageName.substr(start, dot - start);
        out << segment.size() << segment;
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }

    out << '_' << classOrModuleName.size() << classOrModuleName
        << '_' << functionName.size() << functionName
        << '_' << (paramTypeCodes.empty() ? "v" : paramTypeCodes)
        << "__" << returnTypeCode;

    return out.str();
}

const ABITargetDescriptor& GetABITargetDescriptor(TargetArch arch) noexcept {
    for (const auto& desc : ABI_TARGETS) {
        if (desc.arch == arch) {
            return desc;
        }
    }
    return ABI_TARGETS[0];
}

} // namespace Robix::Spec

