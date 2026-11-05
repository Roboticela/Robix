#pragma once

#include "../AST/ASTNodes.hpp"
#include "../Types/SpecValidator.hpp"
#include "../Types/TypeChecker.hpp"
#include "Diagnostics.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace Robix::Frontend {

struct SemanticOptions {
    bool strictResultHandling   = false; // Treat discarded Result<T, E> as error R2012 instead of warning
    std::size_t maxConstevalSteps = 10000;
    std::size_t maxConstevalDepth = 64;
};

struct VTableSlot {
    std::string methodName;
    std::string definingClassName;
    TypeNode returnType;
    bool isAbstract = false;
    bool isFinal    = false;
};

struct SemanticFieldInfo {
    std::string name;
    TypeNode type;
    Spec::AccessVisibility visibility = Spec::AccessVisibility::Private;
    VarMutability mutability          = VarMutability::MutableVar;
    bool isStatic                     = false;
    SourceLocation location{};
};

struct SemanticMethodInfo {
    std::string name;
    std::vector<FunctionParamInfo> parameters;
    TypeNode returnType;
    Spec::AccessVisibility visibility = Spec::AccessVisibility::Public;
    bool isStatic      = false;
    bool isAbstract    = false;
    bool isFinal       = false;
    bool isOverride    = false;
    bool isOperator    = false;
    bool isConsteval   = false;
    bool isRealTime    = false;
    bool isDeprecated  = false;
    std::string deprecatedMessage;
    const DeclNode* astNode = nullptr;
    SourceLocation location{};
};

struct SemanticClassInfo {
    std::string name;
    DeclKind kind = DeclKind::Class;
    Spec::AccessVisibility visibility = Spec::AccessVisibility::Public;
    bool isAbstract = false;
    bool isFinal    = false;
    bool isPartial  = false;
    std::string baseClassName;
    std::vector<std::string> implementedInterfaces;
    std::unordered_map<std::string, SemanticFieldInfo> fields;
    std::unordered_map<std::string, SemanticMethodInfo> methods;
    std::vector<std::string> enumVariants;
    std::vector<AttributeNode> attributes;
    std::vector<VTableSlot> vtable;
    bool hasCustomDestructor = false;
    bool hasDerivedClasses   = false;
    SourceLocation location{};
};

struct StreamFusionPipeline {
    std::string sourceCollectionExpr;
    std::vector<std::string> stages; // e.g. {"Filter", "Map", "Reduce"}
    bool eliminatesIntermediateAllocations = true;
    SourceLocation location{};
};

struct ScopeCleanupPlan {
    std::string functionName;
    // Recorded in exact LIFO execution order (last declared runs first on exit)
    std::vector<std::string> lifoCleanupActions;
};

struct ReflectionTypeMetadata {
    std::string typeName;
    DeclKind kind = DeclKind::Class;
    std::vector<std::string> fieldNames;
    std::vector<std::string> methodNames;
    std::vector<std::string> attributeNames;
};

struct ConstevalValue {
    using ValueVariant = std::variant<std::monostate, std::int64_t, double, bool, std::string>;
    ValueVariant value{};

    [[nodiscard]] bool IsValid() const noexcept { return !std::holds_alternative<std::monostate>(value); }
    [[nodiscard]] bool IsInt() const noexcept { return std::holds_alternative<std::int64_t>(value); }
    [[nodiscard]] bool IsFloat() const noexcept { return std::holds_alternative<double>(value); }
    [[nodiscard]] bool IsBool() const noexcept { return std::holds_alternative<bool>(value); }
    [[nodiscard]] bool IsString() const noexcept { return std::holds_alternative<std::string>(value); }

    [[nodiscard]] std::int64_t AsInt() const noexcept { return IsInt() ? std::get<std::int64_t>(value) : 0; }
    [[nodiscard]] double AsFloat() const noexcept {
        if (IsFloat()) return std::get<double>(value);
        if (IsInt()) return static_cast<double>(std::get<std::int64_t>(value));
        return 0.0;
    }
    [[nodiscard]] bool AsBool() const noexcept { return IsBool() ? std::get<bool>(value) : false; }
    [[nodiscard]] std::string AsString() const { return IsString() ? std::get<std::string>(value) : std::string{}; }
};

class SemanticAnalyzer {
public:
    SemanticAnalyzer(DiagnosticBag& diagnostics, SemanticOptions options = {});

    // Run complete Stage 3 semantic analysis over a type-checked CompilationUnitAST
    bool AnalyzeCompilationUnit(CompilationUnitAST& unit);

    // Query semantic analysis results
    [[nodiscard]] const SemanticClassInfo* FindClassOrType(const std::string& name) const;
    [[nodiscard]] const SemanticMethodInfo* FindGlobalFunction(const std::string& name) const;
    [[nodiscard]] bool CanDevirtualizeCall(const std::string& className, const std::string& methodName) const;
    [[nodiscard]] const std::vector<StreamFusionPipeline>& GetFusedStreamPipelines() const noexcept { return m_fusedPipelines; }
    [[nodiscard]] const std::vector<ScopeCleanupPlan>& GetCleanupPlans() const noexcept { return m_cleanupPlans; }
    [[nodiscard]] std::optional<ReflectionTypeMetadata> GetReflectionMetadata(const std::string& typeName) const;
    [[nodiscard]] bool HasMacro(const std::string& macroName) const;

    // Evaluate a consteval function call at compile time
    [[nodiscard]] ConstevalValue EvaluateConstevalCall(const std::string& fnName,
                                                       const std::vector<ConstevalValue>& args,
                                                       SourceLocation callLoc = {});

private:
    DiagnosticBag& m_diagnostics;
    SemanticOptions m_options;

    std::unordered_map<std::string, SemanticClassInfo> m_classes;
    std::unordered_map<std::string, SemanticMethodInfo> m_globalFunctions;
    std::unordered_map<std::string, const DeclNode*> m_macros;
    std::vector<StreamFusionPipeline> m_fusedPipelines;
    std::vector<ScopeCleanupPlan> m_cleanupPlans;

    std::string m_currentPackage;
    std::string m_currentClass;
    const SemanticMethodInfo* m_currentMethod = nullptr;
    std::vector<std::string> m_currentScopeCleanups;
    std::unordered_map<std::string, VarMutability> m_outerFunctionVars;
    bool m_insideLambda = false;

    void CollectAndMergeTypesAndFunctions(const CompilationUnitAST& unit);
    void ValidateInheritanceAndBuildVTables();
    bool HasInheritanceCycle(const std::string& className) const;
    bool IsDerivedFrom(const std::string& derivedName, const std::string& baseName) const;
    void BuildClassVTable(SemanticClassInfo& cls, std::unordered_set<std::string>& visited);
    void ValidateInterfaceAndAbstractImplementations(const SemanticClassInfo& cls);

    void AnalyzeDeclaration(const DeclNode& decl);
    void AnalyzeFunctionOrMethod(const DeclNode& decl, const SemanticMethodInfo& methodMeta);
    void AnalyzeStatement(const StmtNode& stmt, std::vector<std::string>& scopeDeferActions);
    void AnalyzeExpression(const ExprNode& expr, bool isDiscardedStmtExpr = false);

    void ValidateMatchExhaustiveness(const TypeNode& targetType,
                                     const std::vector<MatchArmNode>& arms,
                                     SourceLocation loc);
    void ValidateCallArguments(const SemanticMethodInfo& callee,
                               const ExprNode& callExpr);
    void DetectStreamFusion(const ExprNode& callExpr);
    void CheckMemberAccessVisibility(const std::string& targetTypeName,
                                     const std::string& memberName,
                                     SourceLocation loc);

    // Consteval interpreter helpers
    ConstevalValue EvalStmt(const StmtNode& stmt,
                            std::unordered_map<std::string, ConstevalValue>& env,
                            bool& returned,
                            std::size_t& stepCounter,
                            std::size_t depth,
                            SourceLocation rootLoc);
    ConstevalValue EvalExpr(const ExprNode& expr,
                            std::unordered_map<std::string, ConstevalValue>& env,
                            std::size_t& stepCounter,
                            std::size_t depth,
                            SourceLocation rootLoc);
};

} // namespace Robix::Frontend

