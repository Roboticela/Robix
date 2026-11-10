#pragma once

#include "../AST/ASTNodes.hpp"
#include "../Semantic/Diagnostics.hpp"
#include "SpecValidator.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Robix::Frontend {

struct TypeCheckerOptions {
    bool strictStyleMode    = false; // Enforce PascalCase / camelCase / UPPER_SNAKE_CASE / acronym rules as errors
    bool requireExplain     = false; // Require Explain {} on all public functions/methods
    bool allowUnsafeBlocks  = true;  // Permit explicit Unsafe { ... } blocks
};

struct SymbolInfo {
    std::string name;
    TypeNode type;
    VarMutability mutability = VarMutability::MutableVar;
    bool isInitialized       = true;
    bool isNullNarrowedSafe  = false; // Set to true inside `if (x != null)` branches
    SourceLocation location{};
};

struct FunctionParamInfo {
    std::string name;
    TypeNode type;
    bool hasDefault = false;
    bool isMutable  = false;
    bool isVariadic = false;
};

struct FunctionSignature {
    std::string name;
    std::vector<FunctionParamInfo> parameters;
    TypeNode returnType;
    bool isAsync = false;
    Spec::AccessVisibility visibility = Spec::AccessVisibility::Public;
    SourceLocation location{};
};

struct TypeDefinitionInfo {
    std::string name;
    DeclKind kind = DeclKind::Class;
    std::string baseClassName;
    std::vector<std::string> implementedInterfaces;
    std::vector<std::string> genericParams;
    std::unordered_map<std::string, SymbolInfo> fields;
    std::unordered_map<std::string, FunctionSignature> methods;
    std::unordered_set<std::string> enumVariants;
    SourceLocation location{};
};

class TypeChecker {
public:
    TypeChecker(DiagnosticBag& diagnostics, TypeCheckerOptions options = {});

    // Analyze a complete compilation unit (.rx, .test.rx, or .rxapi),
    // performing name resolution, type inference, type checking, null safety,
    // unsafe boundary verification, naming validation, and Explain {} validation.
    bool CheckCompilationUnit(CompilationUnitAST& unit);

    // Query resolved types and symbols after checking
    [[nodiscard]] const TypeDefinitionInfo* FindTypeDefinition(const std::string& name) const;
    [[nodiscard]] const FunctionSignature* FindFunction(const std::string& name) const;
    [[nodiscard]] static bool AreTypesEqual(const TypeNode& a, const TypeNode& b);
    [[nodiscard]] static bool IsNumericType(const TypeNode& t);
    [[nodiscard]] static bool IsIntegerType(const TypeNode& t);
    [[nodiscard]] static bool IsFloatingPointType(const TypeNode& t);

private:
    DiagnosticBag& m_diagnostics;
    TypeCheckerOptions m_options;

    // Global & package-level symbol tables
    std::unordered_map<std::string, TypeDefinitionInfo> m_types;
    std::unordered_map<std::string, FunctionSignature> m_functions;

    // Scoped local symbol stack (innermost scope at back)
    std::vector<std::unordered_map<std::string, SymbolInfo>> m_scopes;

    // Contextual state during traversal
    int m_unsafeDepth = 0;
    int m_loopDepth   = 0;
    DeclNode* m_currentFunction = nullptr;
    std::string m_currentClassName;
    std::vector<TypeNode> m_inferredReturnTypes;

    void RegisterBuiltInTypesAndFunctions();
    void CollectTopLevelDeclarations(CompilationUnitAST& unit);

    void CheckDeclaration(DeclNode& decl);
    void CheckFunctionOrMethod(DeclNode& decl, const std::string& enclosingTypeName = {});
    void ValidateExplainBlock(const DeclNode& decl);
    void ValidateNamingConventions(const DeclNode& decl);
    void ValidateVarNaming(const std::string& name, VarMutability mutability, SourceLocation loc);

    void CheckStatement(StmtNode& stmt);
    TypeNode CheckExpression(ExprNode& expr);

    // Scope management
    void PushScope();
    void PopScope();
    bool DeclareLocal(const SymbolInfo& sym);
    SymbolInfo* LookupSymbol(const std::string& name);

    // Type compatibility & inference helpers
    bool IsAssignable(const TypeNode& target, const TypeNode& valueType, SourceLocation loc, const std::string& contextName);
    [[nodiscard]] bool IsSubtype(const std::string& from, const std::string& to, int depth = 0) const;
    [[nodiscard]] TypeNode SubstituteGenerics(TypeNode type, const TypeDefinitionInfo* def, const TypeNode& recv) const;
    static TypeNode MakePrimitiveType(const std::string& name, bool isNullable = false);
};

} // namespace Robix::Frontend
