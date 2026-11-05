#pragma once

#include "../AST/ASTNodes.hpp"
#include "../Semantic/Diagnostics.hpp"
#include "../Semantic/SemanticAnalyzer.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Robix::IR {

enum class IROpcode : std::uint8_t {
    Nop = 0,
    ConstInt,
    ConstFloat,
    ConstBool,
    ConstString,
    Copy,
    // Binary Arithmetic & Logic
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or,
    // Unary
    Neg,
    Not,
    // Memory & Objects
    AllocStack,
    AllocHeap,
    LoadVar,
    StoreVar,
    GetField,
    SetField,
    // Calls & OS Intrinsics
    CallDirect,
    CallVirtual,
    CallOSIntrinsic,
    PrintVal,
    AssertVal,
    // Control Flow Terminators
    Jump,
    BranchCond,
    ReturnVal,
    ReturnVoid,
    // Low-Level & Cleanup
    DeferCleanup,
    InlineASM
};

[[nodiscard]] std::string_view IROpcodeToString(IROpcode op) noexcept;
[[nodiscard]] bool IsTerminatorOpcode(IROpcode op) noexcept;

struct IRValue {
    std::uint32_t ssaId = 0; // 0 means void / unused destination
    Frontend::TypeNode type{};

    [[nodiscard]] bool IsValid() const noexcept { return ssaId != 0; }
    [[nodiscard]] std::string ToString() const { return "%" + std::to_string(ssaId); }
};

struct IRInstruction {
    IROpcode opcode = IROpcode::Nop;
    IRValue dest{};
    std::vector<std::uint32_t> operandIds;

    // Immediate payload or symbolic target
    std::int64_t intImm = 0;
    double floatImm     = 0.0;
    bool boolImm        = false;
    std::string textImm;       // Variable name, function/method name, string literal, or target label
    std::string secondaryText; // Receiver class name or false-branch label in BranchCond

    // Optimization metadata populated by passes
    bool isDevirtualized = false;
    bool isStackPromoted = false;
    bool isDead          = false;

    Frontend::SourceLocation location{};
};

struct IRBasicBlock {
    std::string label;
    std::vector<IRInstruction> instructions;
    std::vector<std::string> predecessors;
    std::vector<std::string> successors;
};

struct IRFunction {
    std::string name;
    std::string mangledName;
    std::string enclosingClassName;
    std::vector<Frontend::FunctionParamInfo> parameters;
    Frontend::TypeNode returnType;
    std::vector<IRBasicBlock> blocks;
    std::uint32_t nextSSAId = 1;
    bool isTestBlock        = false;
    bool isRealTime         = false;
    bool usesHeapOrGC       = false;
    bool usesReflection     = false;
    Frontend::SourceLocation location{};

    [[nodiscard]] std::uint32_t AllocateSSAId() noexcept { return nextSSAId++; }
};

struct IRModule {
    std::string moduleName;
    std::string packageName;
    std::vector<IRFunction> functions;
    bool requiresFullRuntime = false;

    [[nodiscard]] const IRFunction* FindFunction(std::string_view name) const noexcept;
    [[nodiscard]] IRFunction* FindFunctionMutable(std::string_view name) noexcept;
    [[nodiscard]] std::string DumpText() const;
};

class SSAIRBuilder {
public:
    SSAIRBuilder(const Frontend::SemanticAnalyzer& sema, Frontend::DiagnosticBag& diagnostics);

    [[nodiscard]] IRModule LowerCompilationUnit(const Frontend::CompilationUnitAST& unit);

private:
    const Frontend::SemanticAnalyzer& m_sema;
    Frontend::DiagnosticBag& m_diagnostics;

    IRFunction* m_currentFn = nullptr;
    std::size_t m_currentBlockIdx = 0;
    std::uint32_t m_blockCounter = 0;

    void LowerFunctionOrMethod(const Frontend::DeclNode& decl,
                               const std::string& enclosingClass,
                               IRModule& outModule);
    void LowerTestDecl(const Frontend::DeclNode& testDecl, IRModule& outModule);

    std::size_t CreateBasicBlock(const std::string& prefix);
    IRBasicBlock& CurrentBlock();
    void Emit(IRInstruction inst);
    bool CurrentBlockHasTerminator() const;

    void LowerStmt(const Frontend::StmtNode& stmt);
    IRValue LowerExpr(const Frontend::ExprNode& expr);
};

} // namespace Robix::IR

