#pragma once

#include "../Lexer/Token.hpp"
#include "../Types/SpecValidator.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Robix::Frontend {

struct ExprNode;
struct StmtNode;
struct DeclNode;

using ExprPtr = std::unique_ptr<ExprNode>;
using StmtPtr = std::unique_ptr<StmtNode>;
using DeclPtr = std::unique_ptr<DeclNode>;

// ============================================================================
// 1. TYPE ANNOTATION AST NODES
// ============================================================================

struct TypeNode {
    std::string baseName; // e.g. "Int", "String", "Array", "Result", "Pointer", "Object"
    std::vector<TypeNode> typeArgs;
    bool isNullable  = false; // T?
    bool isPointer   = false; // Pointer<T>
    bool isReference = false; // &T or &mut T
    bool isMutable   = false;
    SourceLocation location{};
    std::int64_t arrayLength = 0; // 0 means one value. N means a fixed array of N values.

    [[nodiscard]] bool IsEmpty() const noexcept { return baseName.empty(); }
    [[nodiscard]] std::string ToString() const {
        if (baseName.empty()) {
            return "Inferred";
        }
        std::string s = baseName;
        if (!typeArgs.empty()) {
            s += '<';
            for (std::size_t i = 0; i < typeArgs.size(); ++i) {
                if (i > 0) s += ", ";
                s += typeArgs[i].ToString();
            }
            s += '>';
        }
        if (isNullable) {
            s += '?';
        }
        return s;
    }
};

// ============================================================================
// 2. FIRST-CLASS EXPLAIN { } DOCUMENTATION BLOCK AST
// ============================================================================

struct ExplainBlockNode {
    bool present = false;
    std::string description;
    std::vector<std::pair<std::string, std::string>> inputs; // (paramName, explanation)
    std::string processing;
    std::string output;
    std::vector<std::pair<std::string, std::string>> errors;
    std::string sideEffects;
    SourceLocation location{};
};

// ============================================================================
// 3. EXPRESSIONS
// ============================================================================

enum class ExprKind : std::uint8_t {
    Literal,
    Identifier,
    Unary,
    Binary,
    Assignment,
    Call,
    MemberAccess,
    Index,
    ArrayLiteral,
    ObjectLiteral,
    NewObject,
    Lambda,
    MatchExpr,
    Range
};

struct MatchArmNode {
    std::string patternName;     // e.g. "Success", "Error", "_"
    std::string bindingVar;      // e.g. "value", "message"
    ExprPtr bodyExpr;
    StmtPtr bodyStmt;
    SourceLocation location{};
};

struct ExprNode {
    ExprKind kind = ExprKind::Literal;
    Token token{};
    SourceLocation location{};

    // Literal / Identifier / Operator
    std::string text;
    bool isInclusiveRange = false;

    // Children
    ExprPtr left;
    ExprPtr right;
    std::vector<ExprPtr> arguments;
    std::vector<std::pair<std::string, ExprPtr>> namedFields; // For ObjectLiteral or named args
    TypeNode targetType;                                      // For NewObject or Type<Args>
    std::vector<std::string> lambdaParams;
    StmtPtr lambdaBody;
    std::vector<MatchArmNode> matchArms;

    // Populated by Stage 2 TypeChecker
    TypeNode inferredType{};
};

// ============================================================================
// 4. STATEMENTS & ALL 10 LOOP FORMS
// ============================================================================

enum class StmtKind : std::uint8_t {
    Block,
    VarDecl,
    IfStmt,
    WhileStmt,
    DoWhileStmt,
    ForRangeOrCollection,
    ForCStyle,
    RepeatStmt,
    LoopStmt,
    MatchStmt,
    ReturnStmt,
    BreakStmt,
    ContinueStmt,
    DeferStmt,
    UsingStmt,
    UnsafeStmt,
    ASMStmt,
    TryCatchStmt,
    ThrowStmt,
    ExprStmt
};

enum class VarMutability : std::uint8_t {
    ImmutableLet, // let x = ...
    MutableVar,   // var x = ... or Type x = ...
    Constant      // const Int MAX = ...
};

struct StmtNode {
    StmtKind kind = StmtKind::Block;
    SourceLocation location{};

    // Variable / Constant Declaration
    VarMutability mutability = VarMutability::MutableVar;
    std::string varName;
    TypeNode declaredType;
    ExprPtr initializer;

    // Control Flow / Loops
    ExprPtr condition;
    ExprPtr stepExpr;
    StmtPtr initStmt;
    StmtPtr thenBranch;
    StmtPtr elseBranch;
    std::string loopVar1;
    std::string loopVar2; // For (index, item) in collection.Enumerate()
    std::vector<StmtPtr> statements;
    std::vector<MatchArmNode> matchArms;

    // Try / Catch / Finally
    std::string catchType;
    std::string catchVar;
    StmtPtr catchBranch;
    StmtPtr finallyBranch;

    // Inline ASM. `in:` operands are expressions, `out:` names a local, `clobber:` is a register list.
    std::string asmInstruction;
    std::string asmOutput;
    std::string asmClobber;
    std::vector<ExprPtr> asmInputs;
};

// ============================================================================
// 5. TOP-LEVEL & CLASS MEMBER DECLARATIONS
// ============================================================================

struct AttributeNode {
    std::string name;
    std::vector<std::string> args;
    SourceLocation location{};
};

struct ParameterNode {
    std::string name;
    TypeNode type;
    ExprPtr defaultValue;
    bool isMutable  = false;
    bool isVariadic = false;
    SourceLocation location{};
};

enum class DeclKind : std::uint8_t {
    Function,
    Constructor,
    Destructor,
    Field,
    Class,
    Struct,
    Enum,
    Interface,
    Trait,
    Service,
    Implement,
    Test,
    TestFile,
    TopLevelVar,
    Macro
};

struct DeclNode {
    DeclKind kind = DeclKind::Function;
    std::string name;
    Spec::AccessVisibility visibility = Spec::AccessVisibility::Public;
    bool isStatic    = false;
    bool isAbstract  = false;
    bool isFinal     = false;
    bool isPartial   = false;
    bool isOverride  = false;
    bool isOperator  = false;
    bool isAsync     = false;
    bool isConsteval = false;
    bool isExtern    = false;
    SourceLocation location{};

    std::vector<AttributeNode> attributes;
    std::vector<std::string> genericParams;
    std::vector<std::string> capabilities; // requires Hardware.Camera

    // Function / Method / Constructor
    std::vector<ParameterNode> parameters;
    TypeNode returnType;
    std::optional<ExplainBlockNode> explainBlock;
    StmtPtr body; // nullptr for interface/service/abstract/rxapi declarations
    std::vector<ExprPtr> baseConstructorArgs;

    // Class / Struct / Interface / Enum / Service
    std::string baseClassName;
    std::vector<std::string> implementedInterfaces;
    std::vector<DeclPtr> members;
    std::vector<std::string> enumVariants;

    // Field / TopLevelVar
    VarMutability fieldMutability = VarMutability::MutableVar;
    TypeNode fieldType;
    ExprPtr fieldInitializer;

    // Test / TestFile
    std::string testDescription;
    std::string coversFile;
    bool runOnNative = false;
};

struct CompilationUnitAST {
    std::string fileName;
    std::string packageName;
    std::vector<std::string> imports;
    std::vector<std::string> packagePermissions;
    std::vector<DeclPtr> declarations;
};

} // namespace Robix::Frontend

