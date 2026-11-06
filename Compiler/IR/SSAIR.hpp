#pragma once

#include "../AST/ASTNodes.hpp"
#include "../Semantic/Diagnostics.hpp"
#include "../Semantic/SemanticAnalyzer.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    BitAnd,
    BitOr,
    BitXor,
    Shl,
    Shr,
    // Unary
    Neg,
    Not,
    BitNot,
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
    InlineASM,
    // Aggregates, pointers, and slices. Aggregate values are pointers.
    LoadIndex,
    StoreIndex,
    AddrOf,
    PtrLoad,
    PtrStore,
    PtrOffset,
    SliceCopy
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
    bool runOnNative        = false;
    bool isRealTime         = false;
    bool isExtern           = false;
    bool usesHeapOrGC       = false;
    bool usesReflection     = false;
    Frontend::SourceLocation location{};

    [[nodiscard]] std::uint32_t AllocateSSAId() noexcept { return nextSSAId++; }
};

struct IRVTable {
    std::string typeName;
    std::vector<std::string> slots;
};

struct IRModule {
    std::string moduleName;
    std::string packageName;
    std::vector<IRFunction> functions;
    std::vector<IRVTable> vtables;
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

    // Innermost-last stack of active loop jump targets for `break` / `continue` lowering
    struct LoopTargets {
        std::string breakLabel;
        std::string continueLabel;
    };
    std::vector<LoopTargets> m_loopTargets;

    void LowerStmt(const Frontend::StmtNode& stmt);
    IRValue LowerExpr(const Frontend::ExprNode& expr);
    IRValue OutlineLambda(const Frontend::ExprNode& call, const Frontend::ExprNode& lambda,
                          const std::vector<std::uint32_t>& argIds);

    struct FnSig {
        const Frontend::DeclNode* decl = nullptr;
        std::string sourceName;
        std::string irName;
        std::string mangled;
        int arity = 0;
        bool isExtern = false;
    };

    std::vector<FnSig> m_sigs;
    std::string m_package = "Default";

    void CollectSigs(const Frontend::CompilationUnitAST& unit);
    void NoteSig(const Frontend::DeclNode& decl, const std::string& enclosing);
    [[nodiscard]] const FnSig* FindSig(const Frontend::DeclNode* decl) const;
    [[nodiscard]] std::string ResolveCallable(const std::string& name, int arity, const Frontend::SourceLocation& loc);
    [[nodiscard]] bool IsRefType(const Frontend::TypeNode& type) const;
    [[nodiscard]] int FieldSlot(const std::string& typeName, const std::string& field) const;
    [[nodiscard]] std::string SlotTypeName(const Frontend::TypeNode& type) const;
    IRValue EmitAggregate(const std::string& kind, std::int64_t slots, Frontend::TypeNode type);

    struct LiveObj {
        std::string varName;
        std::string dtorIrName;
        const Frontend::StmtNode* deferBody = nullptr;
    };
    struct GenericInst {
        const Frontend::DeclNode* decl = nullptr;
        std::string irName;
        std::vector<Frontend::TypeNode> args;
    };

    std::vector<std::vector<LiveObj>> m_scopes;
    std::string m_catchLabel;
    std::string m_catchVar;
    int m_lambdaSerial = 0;
    std::string m_activeIrClass;
    std::vector<std::pair<std::string, Frontend::TypeNode>> m_subst;
    std::unordered_map<std::string, const Frontend::DeclNode*> m_classDecls;
    std::vector<GenericInst> m_genericQueue;
    std::unordered_set<std::string> m_genericSeen;
    IRModule* m_module = nullptr;

    void EmitScopeCleanups(std::size_t scopes, bool clear);
    void NoteGenericUse(const Frontend::TypeNode& type);
    void LowerClassBody(const Frontend::DeclNode& decl,
                        const std::string& irName,
                        const std::vector<Frontend::TypeNode>& args);
    void RecordVTable(const std::string& irName, const std::string& lookupName);
    [[nodiscard]] Frontend::TypeNode SubstType(Frontend::TypeNode type) const;
    [[nodiscard]] std::string TypeKey(const Frontend::TypeNode& type) const;
    [[nodiscard]] std::string ClassChain(const std::string& irName) const;
    [[nodiscard]] bool IsClassName(const std::string& name) const;
    [[nodiscard]] int VirtualSlot(const std::string& typeName, const std::string& method) const;
    [[nodiscard]] std::vector<std::string> MethodOrder(const Frontend::SemanticClassInfo& cls) const;
    [[nodiscard]] std::string DefiningClass(const Frontend::SemanticClassInfo& cls, const std::string& method) const;
};

} // namespace Robix::IR

