#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Robix::Spec {

// ============================================================================
// 1. LANGUAGE & ABI VERSION GOVERNANCE CONSTANTS
// ============================================================================

inline constexpr std::string_view LANGUAGE_NAME          = "Robix";
inline constexpr std::string_view UI_LANGUAGE_NAME       = "RobixUI";
inline constexpr std::string_view BUILD_LANGUAGE_NAME    = "RobixBuild";
inline constexpr std::string_view COMPANY_NAME           = "Roboticela";
inline constexpr std::string_view LANGUAGE_VERSION       = "1.0";
inline constexpr std::string_view ABI_VERSION            = "RobixABI_v1";
inline constexpr std::string_view MANGLE_PREFIX          = "_RBX1_";

// Official File Extensions
inline constexpr std::string_view EXT_SOURCE             = ".rx";
inline constexpr std::string_view EXT_TEST               = ".test.rx";
inline constexpr std::string_view EXT_UI                 = ".rxu";
inline constexpr std::string_view EXT_BUILD              = ".rbxb";
inline constexpr std::string_view EXT_PROJECT            = ".rbxproj";
inline constexpr std::string_view EXT_API                = ".rbxapi";
inline constexpr std::string_view EXT_PACKAGE            = ".rbxp";
inline constexpr std::string_view EXT_LOCK               = ".rbxlock";

// ============================================================================
// 2. PRIMITIVE TYPE SPECIFICATION & EXACT LAYOUT METADATA
// ============================================================================

enum class PrimitiveKind : std::uint8_t {
    Bool,
    Byte,
    Char,
    Int8,
    Int16,
    Int32,
    Int64,
    Int,
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    UInt,
    Float16,
    Float32,
    Float64,
    Float,
    Double,
    Decimal,
    String,
    Void
};

struct PrimitiveTypeLayout {
    PrimitiveKind kind;
    std::string_view name;
    std::string_view mangleCode;
    std::size_t sizeBytes64;      // Size on 64-bit targets
    std::size_t sizeBytes32;      // Size on 32-bit targets
    std::size_t alignment64;      // Alignment on 64-bit targets
    bool isSigned;
    bool isNumeric;
    bool isFloatingPoint;
};

const PrimitiveTypeLayout& GetPrimitiveLayout(PrimitiveKind kind) noexcept;
bool TryResolvePrimitiveName(std::string_view name, PrimitiveKind& outKind) noexcept;

// ============================================================================
// 3. OPERATOR PRECEDENCE & ASSOCIATIVITY SPECIFICATION
// ============================================================================

enum class Associativity : std::uint8_t {
    None,
    LeftToRight,
    RightToLeft
};

enum class OperatorKind : std::uint8_t {
    // Precedence 17: Postfix / Member
    MemberAccess,       // .
    NullSafeMember,     // ?.
    ErrorPropagate,     // ?
    PostfixIncrement,   // ++
    PostfixDecrement,   // --
    // Precedence 16: Unary Prefix
    LogicalNot,         // !
    BitwiseNot,         // ~
    UnaryMinus,         // -
    UnaryPlus,          // +
    Dereference,        // *
    AddressOf,          // &
    PrefixIncrement,    // ++
    PrefixDecrement,    // --
    Await,              // await
    // Precedence 15: Multiplicative
    Multiply,           // *
    Divide,             // /
    Modulo,             // %
    // Precedence 14: Additive
    Add,                // +
    Subtract,           // -
    // Precedence 13: Shift
    ShiftLeft,          // <<
    ShiftRight,         // >>
    // Precedence 12: Range
    RangeExclusive,     // ..
    RangeInclusive,     // ..=
    // Precedence 11: Relational
    Less,               // <
    LessEqual,          // <=
    Greater,            // >
    GreaterEqual,       // >=
    TypeIs,             // is
    TypeAs,             // as
    // Precedence 10: Equality
    Equal,              // ==
    NotEqual,           // !=
    // Precedence 9..7: Bitwise
    BitwiseAnd,         // &
    BitwiseXor,         // ^
    BitwiseOr,          // |
    // Precedence 6..5: Logical
    LogicalAnd,         // &&
    LogicalOr,          // ||
    // Precedence 4: Null-Coalescing
    NullCoalesce,       // ??
    // Precedence 3: Ternary
    Ternary,            // ?:
    // Precedence 2: Lambda
    LambdaArrow,        // =>
    // Precedence 1: Assignment
    Assign,             // =
    AddAssign,          // +=
    SubAssign,          // -=
    MulAssign,          // *=
    DivAssign,          // /=
    ModAssign           // %=
};

struct OperatorSpec {
    OperatorKind kind;
    std::string_view token;
    std::uint8_t precedence; // 1 (lowest) .. 18 (highest)
    Associativity associativity;
};

const OperatorSpec& GetOperatorSpec(OperatorKind kind) noexcept;

// ============================================================================
// 4. OFFICIAL NAMING CONVENTION & ACRONYM CAPITALIZATION VALIDATOR
// ============================================================================

bool IsValidPascalCaseWithAcronyms(std::string_view identifier) noexcept;
bool IsValidCamelCaseWithAcronyms(std::string_view identifier) noexcept;
bool IsValidUpperSnakeCase(std::string_view identifier) noexcept;
bool ContainsForbiddenBidiControlChars(std::string_view utf8Text) noexcept;

// ============================================================================
// 5. VISIBILITY & MODULE ACCESS CONTROL MATRIX
// ============================================================================

enum class AccessVisibility : std::uint8_t {
    Private,
    Protected,
    Package,
    Internal,
    Public
};

struct AccessContext {
    bool isSameClassOrStruct = false;
    bool isDerivedClass      = false;
    bool isSameModuleFile    = false;
    bool isSamePackage       = false;
    bool isSameBuildTarget   = false;
};

bool IsAccessPermitted(AccessVisibility declared, const AccessContext& context) noexcept;

// ============================================================================
// 6. FLEXIBLE MAIN() EXIT CODE & ABI NAME MANGLING
// ============================================================================

enum class MainReturnCategory : std::uint8_t {
    VoidReturn,
    IntExitCode,
    StructuredOutput, // String, Array<T>, Object
    ResultType        // Result<T, E>
};

std::int32_t ComputeProcessExitCode(MainReturnCategory category,
                                    std::int64_t intReturnValue,
                                    bool isResultSuccess) noexcept;

std::string MangleRobixSymbol(std::string_view packageName,
                              std::string_view classOrModuleName,
                              std::string_view functionName,
                              std::string_view paramTypeCodes,
                              std::string_view returnTypeCode);

// ============================================================================
// 7. TARGET ARCHITECTURE & ABI CALLING CONVENTION METADATA
// ============================================================================

enum class TargetArch : std::uint8_t {
    X86_64_SysV,
    X86_64_Windows,
    ARM64,
    ARM32,
    RISCV64,
    WASM32
};

struct ABITargetDescriptor {
    TargetArch arch;
    std::string_view name;
    std::size_t pointerSizeBytes;
    std::size_t stackAlignmentBytes;
    std::size_t classHeaderSizeBytes;
    std::size_t maxIntRegisterArgs;
    std::size_t maxFloatRegisterArgs;
};

const ABITargetDescriptor& GetABITargetDescriptor(TargetArch arch) noexcept;

} // namespace Robix::Spec

