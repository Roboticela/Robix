#include "TypeChecker.hpp"

#include <algorithm>
#include <sstream>

namespace Robix::Frontend {

TypeChecker::TypeChecker(DiagnosticBag& diagnostics, TypeCheckerOptions options)
    : m_diagnostics(diagnostics)
    , m_options(options) {
    RegisterBuiltInTypesAndFunctions();
}

TypeNode TypeChecker::MakePrimitiveType(const std::string& name, bool isNullable) {
    TypeNode t;
    t.baseName   = name;
    t.isNullable = isNullable;
    return t;
}

bool TypeChecker::AreTypesEqual(const TypeNode& a, const TypeNode& b) {
    if (a.baseName != b.baseName || a.isNullable != b.isNullable || a.isPointer != b.isPointer) {
        return false;
    }
    if (a.typeArgs.size() != b.typeArgs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.typeArgs.size(); ++i) {
        if (!AreTypesEqual(a.typeArgs[i], b.typeArgs[i])) {
            return false;
        }
    }
    return true;
}

bool TypeChecker::IsIntegerType(const TypeNode& t) {
    if (t.isNullable || t.isPointer) {
        return false;
    }
    Spec::PrimitiveKind kind{};
    if (!Spec::TryResolvePrimitiveName(t.baseName, kind)) {
        return false;
    }
    const auto& layout = Spec::GetPrimitiveLayout(kind);
    return layout.isNumeric && !layout.isFloatingPoint;
}

bool TypeChecker::IsFloatingPointType(const TypeNode& t) {
    if (t.isNullable || t.isPointer) {
        return false;
    }
    Spec::PrimitiveKind kind{};
    if (!Spec::TryResolvePrimitiveName(t.baseName, kind)) {
        return false;
    }
    const auto& layout = Spec::GetPrimitiveLayout(kind);
    return layout.isFloatingPoint;
}

bool TypeChecker::IsNumericType(const TypeNode& t) {
    return IsIntegerType(t) || IsFloatingPointType(t);
}

void TypeChecker::RegisterBuiltInTypesAndFunctions() {
    auto addBuiltInFn = [this](const std::string& name,
                               std::vector<FunctionParamInfo> params,
                               TypeNode retType) {
        FunctionSignature sig;
        sig.name       = name;
        sig.parameters = std::move(params);
        sig.returnType = std::move(retType);
        m_functions[name] = std::move(sig);
    };

    FunctionParamInfo anyParam{ "value", MakePrimitiveType("Any"), false, false, false };
    FunctionParamInfo boolParam{ "condition", MakePrimitiveType("Bool"), false, false, false };

    addBuiltInFn("Print", { anyParam }, MakePrimitiveType("Void"));
    addBuiltInFn("Assert", { boolParam }, MakePrimitiveType("Void"));
    addBuiltInFn("AssertTrue", { boolParam }, MakePrimitiveType("Void"));
    addBuiltInFn("AssertFalse", { boolParam }, MakePrimitiveType("Void"));
    addBuiltInFn("AssertEqual", { anyParam, anyParam }, MakePrimitiveType("Void"));
    addBuiltInFn("AssertNear", { anyParam, anyParam, anyParam }, MakePrimitiveType("Void"));
    addBuiltInFn("Success", { anyParam }, MakePrimitiveType("Result"));
    addBuiltInFn("Error", { anyParam }, MakePrimitiveType("Result"));
}

const TypeDefinitionInfo* TypeChecker::FindTypeDefinition(const std::string& name) const {
    auto it = m_types.find(name);
    return (it != m_types.end()) ? &it->second : nullptr;
}

const FunctionSignature* TypeChecker::FindFunction(const std::string& name) const {
    auto it = m_functions.find(name);
    return (it != m_functions.end()) ? &it->second : nullptr;
}

void TypeChecker::PushScope() {
    m_scopes.emplace_back();
}

void TypeChecker::PopScope() {
    if (!m_scopes.empty()) {
        m_scopes.pop_back();
    }
}

bool TypeChecker::DeclareLocal(const SymbolInfo& sym) {
    if (m_scopes.empty()) {
        PushScope();
    }
    auto& current = m_scopes.back();
    if (current.find(sym.name) != current.end()) {
        m_diagnostics.ReportError(
            "R1008",
            "Duplicate declaration of identifier '" + sym.name + "' in the same scope.",
            sym.location,
            "Rename the variable or assign to the existing variable instead.");
        return false;
    }
    current[sym.name] = sym;
    return true;
}

SymbolInfo* TypeChecker::LookupSymbol(const std::string& name) {
    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &found->second;
        }
    }
    if (!m_currentClassName.empty()) {
        auto classIt = m_types.find(m_currentClassName);
        if (classIt != m_types.end()) {
            auto fieldIt = classIt->second.fields.find(name);
            if (fieldIt != classIt->second.fields.end()) {
                return &fieldIt->second;
            }
        }
    }
    return nullptr;
}

bool TypeChecker::CheckCompilationUnit(CompilationUnitAST& unit) {
    PushScope(); // Global / file scope
    CollectTopLevelDeclarations(unit);

    for (auto& decl : unit.declarations) {
        if (decl) {
            CheckDeclaration(*decl);
        }
    }

    PopScope();
    return !m_diagnostics.HasErrors();
}

void TypeChecker::CollectTopLevelDeclarations(CompilationUnitAST& unit) {
    for (const auto& declPtr : unit.declarations) {
        if (!declPtr) continue;
        const DeclNode& decl = *declPtr;

        if (decl.kind == DeclKind::Class ||
            decl.kind == DeclKind::Struct ||
            decl.kind == DeclKind::Interface ||
            decl.kind == DeclKind::Trait ||
            decl.kind == DeclKind::Service ||
            decl.kind == DeclKind::Enum) {
            TypeDefinitionInfo info;
            info.name                  = decl.name;
            info.kind                  = decl.kind;
            info.baseClassName         = decl.baseClassName;
            info.implementedInterfaces = decl.implementedInterfaces;
            info.location              = decl.location;

            for (const auto& v : decl.enumVariants) {
                info.enumVariants.insert(v);
            }

            for (const auto& memberPtr : decl.members) {
                if (!memberPtr) continue;
                const DeclNode& m = *memberPtr;
                if (m.kind == DeclKind::Field) {
                    SymbolInfo fieldSym;
                    fieldSym.name          = m.name;
                    fieldSym.type          = m.fieldType;
                    fieldSym.mutability    = m.fieldMutability;
                    fieldSym.isInitialized = (m.fieldInitializer != nullptr);
                    fieldSym.location      = m.location;
                    info.fields[m.name]    = fieldSym;
                } else if (m.kind == DeclKind::Function || m.kind == DeclKind::Constructor) {
                    FunctionSignature sig;
                    sig.name = m.name;
                    for (const auto& p : m.parameters) {
                        sig.parameters.push_back(FunctionParamInfo{ p.name, p.type, p.defaultValue != nullptr, p.isMutable, p.isVariadic });
                    }
                    sig.returnType = (m.kind == DeclKind::Constructor) ? MakePrimitiveType(decl.name) : m.returnType;
                    sig.isAsync    = m.isAsync;
                    sig.visibility = m.visibility;
                    sig.location   = m.location;
                    info.methods[m.name] = std::move(sig);
                }
            }
            m_types[decl.name] = std::move(info);
        } else if (decl.kind == DeclKind::Function) {
            FunctionSignature sig;
            sig.name = decl.name;
            for (const auto& p : decl.parameters) {
                sig.parameters.push_back(FunctionParamInfo{ p.name, p.type, p.defaultValue != nullptr, p.isMutable, p.isVariadic });
            }
            sig.returnType = decl.returnType;
            sig.isAsync    = decl.isAsync;
            sig.visibility = decl.visibility;
            sig.location   = decl.location;
            m_functions[decl.name] = std::move(sig);
        }
    }
}

void TypeChecker::ValidateNamingConventions(const DeclNode& decl) {
    if (decl.name.empty() || decl.kind == DeclKind::Test || decl.kind == DeclKind::TestFile) {
        return;
    }

    auto emitStyleDiag = [this](const std::string& code,
                                const std::string& msg,
                                SourceLocation loc,
                                const std::string& suggestion) {
        if (m_options.strictStyleMode) {
            m_diagnostics.ReportError(code, msg, loc, suggestion);
        } else {
            m_diagnostics.ReportWarning(code, msg, loc, suggestion);
        }
    };

    if (decl.kind == DeclKind::Class ||
        decl.kind == DeclKind::Struct ||
        decl.kind == DeclKind::Enum ||
        decl.kind == DeclKind::Interface ||
        decl.kind == DeclKind::Trait ||
        decl.kind == DeclKind::Service ||
        decl.kind == DeclKind::Function) {
        if (!Spec::IsValidPascalCaseWithAcronyms(decl.name)) {
            emitStyleDiag(
                "L0002",
                "Identifier '" + decl.name + "' violates Robix PascalCase and uppercase acronym naming rules.",
                decl.location,
                "Use PascalCase and keep acronyms uppercase (e.g., 'HTTPClient', 'GPUController', 'CalculateMotorSpeed').");
        }
    } else if (decl.kind == DeclKind::Field || decl.kind == DeclKind::TopLevelVar) {
        ValidateVarNaming(decl.name, decl.fieldMutability, decl.location);
    }
}

void TypeChecker::ValidateVarNaming(const std::string& name, VarMutability mutability, SourceLocation loc) {
    if (name.empty() || name == "_") {
        return;
    }
    auto emitStyleDiag = [this](const std::string& code,
                                const std::string& msg,
                                SourceLocation location,
                                const std::string& suggestion) {
        if (m_options.strictStyleMode) {
            m_diagnostics.ReportError(code, msg, location, suggestion);
        } else {
            m_diagnostics.ReportWarning(code, msg, location, suggestion);
        }
    };

    if (mutability == VarMutability::Constant) {
        if (!Spec::IsValidUpperSnakeCase(name)) {
            emitStyleDiag(
                "L0004",
                "Constant '" + name + "' must use UPPER_SNAKE_CASE.",
                loc,
                "Rename constant to UPPER_SNAKE_CASE (e.g., 'MAX_MOTOR_SPEED').");
        }
    } else {
        if (!Spec::IsValidCamelCaseWithAcronyms(name)) {
            emitStyleDiag(
                "L0003",
                "Variable or field '" + name + "' violates Robix camelCase and uppercase acronym rules.",
                loc,
                "Use camelCase with uppercase internal acronyms (e.g., 'motorSpeed', 'isUSBConnected', 'httpResponse').");
        }
    }
}

void TypeChecker::ValidateExplainBlock(const DeclNode& decl) {
    if (!decl.explainBlock.has_value() || !decl.explainBlock->present) {
        if (m_options.requireExplain &&
            decl.visibility == Spec::AccessVisibility::Public &&
            decl.kind == DeclKind::Function &&
            decl.body != nullptr) {
            m_diagnostics.ReportError(
                "D3001",
                "Public function '" + decl.name + "' is missing a required 'Explain { ... }' block.",
                decl.location,
                "Add an Explain { Description: ...; Processing: ...; Output: ...; } block inside '" + decl.name + "'.");
        }
        return;
    }

    const ExplainBlockNode& explain = *decl.explainBlock;

    if (explain.description.empty()) {
        m_diagnostics.ReportError(
            "D3001",
            "Explain block in function '" + decl.name + "' is missing a 'Description' entry.",
            explain.location,
            "Provide 'Description: \"...\";' explaining the purpose of '" + decl.name + "'.");
    }

    if (explain.processing.empty()) {
        m_diagnostics.ReportError(
            "D3001",
            "Explain block in function '" + decl.name + "' is missing a 'Processing' entry.",
            explain.location,
            "Provide 'Processing: \"...\";' summarizing the algorithm or logic.");
    }

    // Validate Input parameter alignment with function signature
    std::unordered_set<std::string> documentedParams;
    for (const auto& [paramName, paramDoc] : explain.inputs) {
        documentedParams.insert(paramName);
        bool existsInSignature = false;
        for (const auto& sigParam : decl.parameters) {
            if (sigParam.name == paramName) {
                existsInSignature = true;
                break;
            }
        }
        if (!existsInSignature) {
            m_diagnostics.ReportError(
                "D3001",
                "Explain block in '" + decl.name + "' documents unknown Input parameter '" + paramName + "'.",
                explain.location,
                "Update the Explain Input section to match the function's actual parameter names.");
        }
    }

    for (const auto& sigParam : decl.parameters) {
        if (documentedParams.find(sigParam.name) == documentedParams.end()) {
            m_diagnostics.ReportError(
                "D3001",
                "Explain block in '" + decl.name + "' is missing Input documentation for parameter '" + sigParam.name + "'.",
                explain.location,
                "Add '" + sigParam.name + ": \"...\";' inside the Explain Input block.");
        }
    }

    // Validate Output requirement for non-Void functions
    const bool isNonVoid = !decl.returnType.IsEmpty() && decl.returnType.baseName != "Void";
    if (isNonVoid && explain.output.empty()) {
        m_diagnostics.ReportError(
            "D3001",
            "Explain block in non-Void function '" + decl.name + "' is missing an 'Output' entry.",
            explain.location,
            "Add 'Output: \"...\";' describing the returned '" + decl.returnType.ToString() + "' value.");
    }

    // Validate Errors requirement when return type is Result<T, E>
    if (decl.returnType.baseName == "Result" && explain.errors.empty()) {
        m_diagnostics.ReportWarning(
            "D3001",
            "Function '" + decl.name + "' returns '" + decl.returnType.ToString() + "' but its Explain block does not document 'Errors'.",
            explain.location,
            "Add an 'Errors { ... }' section documenting failure conditions.");
    }
}

void TypeChecker::CheckDeclaration(DeclNode& decl) {
    ValidateNamingConventions(decl);

    switch (decl.kind) {
        case DeclKind::Function:
        case DeclKind::Constructor:
        case DeclKind::Destructor:
            CheckFunctionOrMethod(decl, m_currentClassName);
            break;

        case DeclKind::Field:
        case DeclKind::TopLevelVar: {
            if (decl.fieldInitializer) {
                TypeNode initType = CheckExpression(*decl.fieldInitializer);
                if (decl.fieldType.IsEmpty()) {
                    decl.fieldType = initType;
                } else {
                    IsAssignable(decl.fieldType, initType, decl.location, decl.name);
                }
            }
            if (decl.kind == DeclKind::TopLevelVar) {
                SymbolInfo sym;
                sym.name          = decl.name;
                sym.type          = decl.fieldType;
                sym.mutability    = decl.fieldMutability;
                sym.isInitialized = (decl.fieldInitializer != nullptr);
                sym.location      = decl.location;
                DeclareLocal(sym);
            } else if (!m_currentClassName.empty()) {
                m_types[m_currentClassName].fields[decl.name].type = decl.fieldType;
            }
            break;
        }

        case DeclKind::Class:
        case DeclKind::Struct:
        case DeclKind::Interface:
        case DeclKind::Trait:
        case DeclKind::Service:
        case DeclKind::Implement: {
            std::string prevClass = m_currentClassName;
            m_currentClassName = decl.name;
            PushScope();

            // Bind `this` inside class/struct methods
            SymbolInfo thisSym;
            thisSym.name          = "this";
            thisSym.type          = MakePrimitiveType(decl.name);
            thisSym.mutability    = VarMutability::ImmutableLet;
            thisSym.isInitialized = true;
            thisSym.location      = decl.location;
            DeclareLocal(thisSym);

            for (auto& member : decl.members) {
                if (member) {
                    CheckDeclaration(*member);
                }
            }

            PopScope();
            m_currentClassName = prevClass;
            break;
        }

        case DeclKind::Test: {
            PushScope();
            if (decl.body) {
                CheckStatement(*decl.body);
            }
            PopScope();
            break;
        }

        case DeclKind::Enum:
        case DeclKind::TestFile:
            break;
    }
}

void TypeChecker::CheckFunctionOrMethod(DeclNode& decl, const std::string& enclosingTypeName) {
    DeclNode* prevFunction = m_currentFunction;
    m_currentFunction = &decl;
    m_inferredReturnTypes.clear();

    PushScope();

    for (const auto& param : decl.parameters) {
        ValidateVarNaming(param.name, param.isMutable ? VarMutability::MutableVar : VarMutability::ImmutableLet, param.location);
        SymbolInfo paramSym;
        paramSym.name          = param.name;
        paramSym.type          = param.type.IsEmpty() ? MakePrimitiveType("Any") : param.type;
        paramSym.mutability    = param.isMutable ? VarMutability::MutableVar : VarMutability::ImmutableLet;
        paramSym.isInitialized = true;
        paramSym.location      = param.location;
        DeclareLocal(paramSym);
    }

    if (decl.body) {
        CheckStatement(*decl.body);
    }

    // Infer return type if omitted in function signature
    if (decl.returnType.IsEmpty() && decl.kind == DeclKind::Function) {
        if (m_inferredReturnTypes.empty()) {
            decl.returnType = MakePrimitiveType("Void");
        } else {
            decl.returnType = m_inferredReturnTypes.front();
        }
        // Update registered function signature with inferred return type
        if (enclosingTypeName.empty()) {
            m_functions[decl.name].returnType = decl.returnType;
        } else {
            m_types[enclosingTypeName].methods[decl.name].returnType = decl.returnType;
        }
    }

    ValidateExplainBlock(decl);

    PopScope();
    m_currentFunction = prevFunction;
}

void TypeChecker::CheckStatement(StmtNode& stmt) {
    switch (stmt.kind) {
        case StmtKind::Block: {
            PushScope();
            for (auto& s : stmt.statements) {
                if (s) {
                    CheckStatement(*s);
                }
            }
            PopScope();
            break;
        }

        case StmtKind::VarDecl: {
            ValidateVarNaming(stmt.varName, stmt.mutability, stmt.location);

            bool isInit = false;
            if (stmt.initializer) {
                TypeNode initType = CheckExpression(*stmt.initializer);
                isInit = true;

                if (stmt.declaredType.IsEmpty()) {
                    if (initType.baseName == "Null") {
                        m_diagnostics.ReportError(
                            "R1001",
                            "Cannot infer type for '" + stmt.varName + "' from bare 'null' without an explicit nullable type annotation.",
                            stmt.location,
                            "Specify an explicit nullable type such as 'String? " + stmt.varName + " = null;'.");
                        stmt.declaredType = MakePrimitiveType("Any", true);
                    } else {
                        stmt.declaredType = initType;
                    }
                } else {
                    IsAssignable(stmt.declaredType, initType, stmt.location, stmt.varName);
                }
            } else if (stmt.mutability == VarMutability::ImmutableLet || stmt.mutability == VarMutability::Constant) {
                m_diagnostics.ReportError(
                    "R1007",
                    "Immutable binding '" + stmt.varName + "' must be initialized at declaration.",
                    stmt.location,
                    "Provide an initializer expression '= ...'.");
            }

            SymbolInfo sym;
            sym.name          = stmt.varName;
            sym.type          = stmt.declaredType;
            sym.mutability    = stmt.mutability;
            sym.isInitialized = isInit;
            sym.location      = stmt.location;
            DeclareLocal(sym);
            break;
        }

        case StmtKind::IfStmt: {
            std::string narrowedVar;
            if (stmt.condition) {
                TypeNode condType = CheckExpression(*stmt.condition);
                if (condType.baseName != "Bool" && condType.baseName != "Any") {
                    m_diagnostics.ReportError(
                        "R1001",
                        "Condition in 'if' statement must evaluate to 'Bool'.",
                        stmt.condition->location,
                        "Use a boolean comparison expression.",
                        "Bool",
                        condType.ToString());
                }

                // Detect `x != null` null-safety narrowing
                if (stmt.condition->kind == ExprKind::Binary && stmt.condition->text == "!=") {
                    if (stmt.condition->left && stmt.condition->left->kind == ExprKind::Identifier &&
                        stmt.condition->right && stmt.condition->right->token.kind == TokenKind::NullLiteral) {
                        narrowedVar = stmt.condition->left->text;
                    } else if (stmt.condition->right && stmt.condition->right->kind == ExprKind::Identifier &&
                               stmt.condition->left && stmt.condition->left->token.kind == TokenKind::NullLiteral) {
                        narrowedVar = stmt.condition->right->text;
                    }
                }
            }

            SymbolInfo* symToNarrow = !narrowedVar.empty() ? LookupSymbol(narrowedVar) : nullptr;
            bool prevNarrowed = symToNarrow ? symToNarrow->isNullNarrowedSafe : false;
            if (symToNarrow) {
                symToNarrow->isNullNarrowedSafe = true;
            }

            if (stmt.thenBranch) {
                CheckStatement(*stmt.thenBranch);
            }

            if (symToNarrow) {
                symToNarrow->isNullNarrowedSafe = prevNarrowed;
            }

            if (stmt.elseBranch) {
                CheckStatement(*stmt.elseBranch);
            }
            break;
        }

        case StmtKind::WhileStmt:
        case StmtKind::DoWhileStmt: {
            if (stmt.condition) {
                TypeNode condType = CheckExpression(*stmt.condition);
                if (condType.baseName != "Bool" && condType.baseName != "Any") {
                    m_diagnostics.ReportError(
                        "R1001",
                        "Loop condition must evaluate to 'Bool'.",
                        stmt.condition->location,
                        "Provide a boolean condition.",
                        "Bool",
                        condType.ToString());
                }
            }
            ++m_loopDepth;
            if (stmt.thenBranch) {
                CheckStatement(*stmt.thenBranch);
            }
            --m_loopDepth;
            break;
        }

        case StmtKind::ForRangeOrCollection: {
            PushScope();
            TypeNode iterType = stmt.condition ? CheckExpression(*stmt.condition) : MakePrimitiveType("Range");
            if (stmt.stepExpr) {
                CheckExpression(*stmt.stepExpr);
            }

            TypeNode elemType = MakePrimitiveType("Int");
            if ((iterType.baseName == "Array" || iterType.baseName == "List" || iterType.baseName == "Span") &&
                !iterType.typeArgs.empty()) {
                elemType = iterType.typeArgs[0];
            } else if (iterType.baseName == "Enumerate" && !iterType.typeArgs.empty()) {
                elemType = iterType.typeArgs[0];
            }

            if (!stmt.loopVar2.empty()) {
                // `for (index, item) in collection.Enumerate()`
                SymbolInfo idxSym{ stmt.loopVar1, MakePrimitiveType("Int"), VarMutability::ImmutableLet, true, false, stmt.location };
                SymbolInfo itemSym{ stmt.loopVar2, elemType, VarMutability::ImmutableLet, true, false, stmt.location };
                DeclareLocal(idxSym);
                DeclareLocal(itemSym);
            } else if (!stmt.loopVar1.empty()) {
                SymbolInfo loopSym{ stmt.loopVar1, elemType, VarMutability::ImmutableLet, true, false, stmt.location };
                DeclareLocal(loopSym);
            }

            ++m_loopDepth;
            if (stmt.thenBranch) {
                CheckStatement(*stmt.thenBranch);
            }
            --m_loopDepth;
            PopScope();
            break;
        }

        case StmtKind::ForCStyle: {
            PushScope();
            if (stmt.initStmt) {
                CheckStatement(*stmt.initStmt);
            }
            if (stmt.condition) {
                CheckExpression(*stmt.condition);
            }
            if (stmt.stepExpr) {
                CheckExpression(*stmt.stepExpr);
            }
            ++m_loopDepth;
            if (stmt.thenBranch) {
                CheckStatement(*stmt.thenBranch);
            }
            --m_loopDepth;
            PopScope();
            break;
        }

        case StmtKind::RepeatStmt: {
            if (stmt.condition) {
                TypeNode countType = CheckExpression(*stmt.condition);
                if (!IsIntegerType(countType) && countType.baseName != "Any") {
                    m_diagnostics.ReportError(
                        "R1001",
                        "'repeat' count expression must be an integer type.",
                        stmt.condition->location,
                        "Provide an 'Int' count for 'repeat'.",
                        "Int",
                        countType.ToString());
                }
            }
            ++m_loopDepth;
            if (stmt.thenBranch) {
                CheckStatement(*stmt.thenBranch);
            }
            --m_loopDepth;
            break;
        }

        case StmtKind::LoopStmt: {
            ++m_loopDepth;
            if (stmt.thenBranch) {
                CheckStatement(*stmt.thenBranch);
            }
            --m_loopDepth;
            break;
        }

        case StmtKind::MatchStmt: {
            if (stmt.condition) {
                CheckExpression(*stmt.condition);
            }
            for (auto& arm : stmt.matchArms) {
                PushScope();
                if (!arm.bindingVar.empty()) {
                    SymbolInfo bindSym{ arm.bindingVar, MakePrimitiveType("Any"), VarMutability::ImmutableLet, true, false, arm.location };
                    DeclareLocal(bindSym);
                }
                if (arm.bodyExpr) {
                    CheckExpression(*arm.bodyExpr);
                }
                if (arm.bodyStmt) {
                    CheckStatement(*arm.bodyStmt);
                }
                PopScope();
            }
            break;
        }

        case StmtKind::ReturnStmt: {
            TypeNode retValType = MakePrimitiveType("Void");
            if (stmt.condition) {
                retValType = CheckExpression(*stmt.condition);
            }
            m_inferredReturnTypes.push_back(retValType);

            if (m_currentFunction && !m_currentFunction->returnType.IsEmpty()) {
                IsAssignable(m_currentFunction->returnType, retValType, stmt.location, m_currentFunction->name);
            }
            break;
        }

        case StmtKind::BreakStmt:
        case StmtKind::ContinueStmt: {
            if (m_loopDepth <= 0) {
                m_diagnostics.ReportError(
                    "R1009",
                    "'" + std::string(stmt.kind == StmtKind::BreakStmt ? "break" : "continue") + "' statement used outside of a loop.",
                    stmt.location,
                    "Place 'break' or 'continue' inside a 'for', 'while', 'repeat', or 'loop' block.");
            }
            break;
        }

        case StmtKind::DeferStmt: {
            if (stmt.thenBranch) {
                CheckStatement(*stmt.thenBranch);
            }
            break;
        }

        case StmtKind::UsingStmt: {
            PushScope();
            if (stmt.initializer) {
                TypeNode initType = CheckExpression(*stmt.initializer);
                TypeNode varType  = stmt.declaredType.IsEmpty() ? initType : stmt.declaredType;
                SymbolInfo sym{ stmt.varName, varType, VarMutability::ImmutableLet, true, false, stmt.location };
                DeclareLocal(sym);
            }
            if (stmt.thenBranch) {
                CheckStatement(*stmt.thenBranch);
            }
            PopScope();
            break;
        }

        case StmtKind::UnsafeStmt: {
            if (!m_options.allowUnsafeBlocks) {
                m_diagnostics.ReportError(
                    "R1006",
                    "'Unsafe' blocks are disabled in this compilation profile.",
                    stmt.location);
            }
            ++m_unsafeDepth;
            if (stmt.thenBranch) {
                CheckStatement(*stmt.thenBranch);
            }
            --m_unsafeDepth;
            break;
        }

        case StmtKind::ASMStmt: {
            if (m_unsafeDepth <= 0) {
                m_diagnostics.ReportError(
                    "R1006",
                    "Inline assembly 'ASM(\"" + stmt.asmInstruction + "\")' is only permitted inside an explicit 'Unsafe { ... }' block.",
                    stmt.location,
                    "Wrap the inline assembly statement in 'Unsafe { ASM(...); }'.");
            }
            break;
        }

        case StmtKind::TryCatchStmt: {
            if (stmt.thenBranch) {
                CheckStatement(*stmt.thenBranch);
            }
            if (stmt.catchBranch) {
                PushScope();
                if (!stmt.catchVar.empty()) {
                    SymbolInfo errSym{
                        stmt.catchVar,
                        MakePrimitiveType(stmt.catchType.empty() ? "Error" : stmt.catchType),
                        VarMutability::ImmutableLet,
                        true,
                        false,
                        stmt.location
                    };
                    DeclareLocal(errSym);
                }
                CheckStatement(*stmt.catchBranch);
                PopScope();
            }
            if (stmt.finallyBranch) {
                CheckStatement(*stmt.finallyBranch);
            }
            break;
        }

        case StmtKind::ThrowStmt:
        case StmtKind::ExprStmt: {
            if (stmt.condition) {
                CheckExpression(*stmt.condition);
            }
            break;
        }
    }
}

TypeNode TypeChecker::CheckExpression(ExprNode& expr) {
    switch (expr.kind) {
        case ExprKind::Literal: {
            switch (expr.token.kind) {
                case TokenKind::IntegerLiteral:
                    expr.inferredType = MakePrimitiveType("Int");
                    break;
                case TokenKind::FloatLiteral:
                    expr.inferredType = MakePrimitiveType("Float");
                    break;
                case TokenKind::StringLiteral:
                case TokenKind::InterpolatedString:
                case TokenKind::MultilineString:
                    expr.inferredType = MakePrimitiveType("String");
                    break;
                case TokenKind::CharLiteral:
                    expr.inferredType = MakePrimitiveType("Char");
                    break;
                case TokenKind::RegexLiteral:
                    expr.inferredType = MakePrimitiveType("Regex");
                    break;
                case TokenKind::BoolLiteral:
                    expr.inferredType = MakePrimitiveType("Bool");
                    break;
                case TokenKind::NullLiteral:
                    expr.inferredType = MakePrimitiveType("Null", true);
                    break;
                default:
                    expr.inferredType = MakePrimitiveType("Void");
                    break;
            }
            return expr.inferredType;
        }

        case ExprKind::Identifier: {
            if (expr.text == "Memory" || expr.text == "GPIO" || expr.text == "File" ||
                expr.text == "JSON" || expr.text == "Time" || expr.text == "Env" || expr.text == "Test") {
                expr.inferredType = MakePrimitiveType(expr.text);
                return expr.inferredType;
            }

            if (SymbolInfo* sym = LookupSymbol(expr.text)) {
                if (!sym->isInitialized) {
                    m_diagnostics.ReportError(
                        "R1007",
                        "Variable '" + expr.text + "' is used before being definitely assigned a value.",
                        expr.location,
                        "Initialize '" + expr.text + "' before reading from it.");
                }
                TypeNode resolved = sym->type;
                if (sym->isNullNarrowedSafe) {
                    resolved.isNullable = false;
                }
                expr.inferredType = resolved;
                return expr.inferredType;
            }

            if (const auto* fn = FindFunction(expr.text)) {
                expr.inferredType = fn->returnType;
                return expr.inferredType;
            }

            if (FindTypeDefinition(expr.text) != nullptr) {
                expr.inferredType = MakePrimitiveType(expr.text);
                return expr.inferredType;
            }

            m_diagnostics.ReportError(
                "R1003",
                "Use of undeclared identifier '" + expr.text + "'.",
                expr.location,
                "Check the spelling of '" + expr.text + "' or declare it before use.");
            expr.inferredType = MakePrimitiveType("Any");
            return expr.inferredType;
        }

        case ExprKind::Unary: {
            TypeNode operandType = expr.left ? CheckExpression(*expr.left) : MakePrimitiveType("Any");
            if (expr.text == "*") {
                // Pointer dereference requires Unsafe {}
                if (m_unsafeDepth <= 0) {
                    m_diagnostics.ReportError(
                        "R1006",
                        "Pointer dereference '*ptr' is only permitted inside an 'Unsafe { ... }' block.",
                        expr.location,
                        "Wrap raw pointer operations in an 'Unsafe { ... }' block.");
                }
                if (operandType.baseName == "Pointer" && !operandType.typeArgs.empty()) {
                    expr.inferredType = operandType.typeArgs[0];
                } else {
                    expr.inferredType = MakePrimitiveType("Any");
                }
                return expr.inferredType;
            }
            if (expr.text == "&") {
                // Address-of requires Unsafe {}
                if (m_unsafeDepth <= 0) {
                    m_diagnostics.ReportError(
                        "R1006",
                        "Taking raw memory address '&expr' is only permitted inside an 'Unsafe { ... }' block.",
                        expr.location,
                        "Wrap '&' address-of operations in an 'Unsafe { ... }' block.");
                }
                TypeNode ptrType;
                ptrType.baseName  = "Pointer";
                ptrType.isPointer = true;
                ptrType.typeArgs.push_back(operandType);
                expr.inferredType = ptrType;
                return expr.inferredType;
            }
            if (expr.text == "!") {
                if (operandType.baseName != "Bool" && operandType.baseName != "Any") {
                    m_diagnostics.ReportError(
                        "R1001",
                        "Logical negation '!' requires a 'Bool' operand.",
                        expr.location,
                        "Convert the operand to 'Bool'.",
                        "Bool",
                        operandType.ToString());
                }
                expr.inferredType = MakePrimitiveType("Bool");
                return expr.inferredType;
            }
            expr.inferredType = operandType;
            return expr.inferredType;
        }

        case ExprKind::Binary: {
            TypeNode leftType  = expr.left ? CheckExpression(*expr.left) : MakePrimitiveType("Any");
            TypeNode rightType = expr.right ? CheckExpression(*expr.right) : MakePrimitiveType("Any");
            const std::string& op = expr.text;

            if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
                if (op == "+" && leftType.baseName == "String" && rightType.baseName == "String" &&
                    !leftType.isNullable && !rightType.isNullable) {
                    expr.inferredType = MakePrimitiveType("String");
                    return expr.inferredType;
                }

                if (leftType.isNullable || rightType.isNullable) {
                    m_diagnostics.ReportError(
                        "R1005",
                        "Cannot perform arithmetic operator '" + op + "' on nullable type without null check or null-coalescing fallback.",
                        expr.location,
                        "Unwrap the nullable value with null-coalescing or check '!= null' first.");
                }

                if ((IsIntegerType(leftType) && IsFloatingPointType(rightType)) ||
                    (IsFloatingPointType(leftType) && IsIntegerType(rightType))) {
                    m_diagnostics.ReportError(
                        "R1001",
                        "Implicit arithmetic conversion between '" + leftType.ToString() + "' and '" + rightType.ToString() + "' is forbidden in Robix.",
                        expr.location,
                        "Use explicit conversion such as '.ToFloat()' or '.ToInt()'.",
                        leftType.ToString(),
                        rightType.ToString());
                    expr.inferredType = MakePrimitiveType("Float");
                    return expr.inferredType;
                }

                if (!AreTypesEqual(leftType, rightType) && leftType.baseName != "Any" && rightType.baseName != "Any") {
                    m_diagnostics.ReportError(
                        "R1001",
                        "Binary operator '" + op + "' cannot be applied to mismatched types '" + leftType.ToString() + "' and '" + rightType.ToString() + "'.",
                        expr.location,
                        "Convert both operands to the same type.",
                        leftType.ToString(),
                        rightType.ToString());
                }

                expr.inferredType = leftType;
                return expr.inferredType;
            }

            if (op == "<" || op == "<=" || op == ">" || op == ">=") {
                if ((IsIntegerType(leftType) && IsFloatingPointType(rightType)) ||
                    (IsFloatingPointType(leftType) && IsIntegerType(rightType))) {
                    m_diagnostics.ReportError(
                        "R1001",
                        "Cannot compare '" + leftType.ToString() + "' and '" + rightType.ToString() + "' without explicit conversion.",
                        expr.location,
                        "Use '.ToFloat()' or '.ToInt()' before comparing.",
                        leftType.ToString(),
                        rightType.ToString());
                }
                expr.inferredType = MakePrimitiveType("Bool");
                return expr.inferredType;
            }

            if (op == "==" || op == "!=") {
                expr.inferredType = MakePrimitiveType("Bool");
                return expr.inferredType;
            }

            if (op == "&&" || op == "||") {
                expr.inferredType = MakePrimitiveType("Bool");
                return expr.inferredType;
            }

            if (op == "??") {
                TypeNode unwrapped = leftType;
                unwrapped.isNullable = false;
                if (!AreTypesEqual(unwrapped, rightType) && unwrapped.baseName != "Any" && rightType.baseName != "Any") {
                    m_diagnostics.ReportError(
                        "R1001",
                        "Fallback expression in null-coalescing operator must match non-null type '" + unwrapped.ToString() + "'.",
                        expr.location,
                        "Provide a fallback of type '" + unwrapped.ToString() + "'.",
                        unwrapped.ToString(),
                        rightType.ToString());
                }
                expr.inferredType = unwrapped;
                return expr.inferredType;
            }

            expr.inferredType = leftType;
            return expr.inferredType;
        }

        case ExprKind::Range: {
            if (expr.left) CheckExpression(*expr.left);
            if (expr.right) CheckExpression(*expr.right);
            expr.inferredType = MakePrimitiveType("Range");
            return expr.inferredType;
        }

        case ExprKind::Assignment: {
            TypeNode rhsType = expr.right ? CheckExpression(*expr.right) : MakePrimitiveType("Any");

            if (expr.left && expr.left->kind == ExprKind::Identifier) {
                SymbolInfo* sym = LookupSymbol(expr.left->text);
                if (!sym) {
                    m_diagnostics.ReportError(
                        "R1003",
                        "Cannot assign to undeclared identifier '" + expr.left->text + "'.",
                        expr.location,
                        "Declare '" + expr.left->text + "' with 'var' or an explicit type before assigning.");
                    expr.inferredType = rhsType;
                    return expr.inferredType;
                }

                if (sym->mutability == VarMutability::ImmutableLet || sym->mutability == VarMutability::Constant) {
                    m_diagnostics.ReportError(
                        "R1002",
                        "Cannot reassign immutable binding '" + sym->name + "'.",
                        expr.location,
                        "Declare '" + sym->name + "' with 'var' or an explicit mutable type if reassignment is needed.");
                }

                sym->isInitialized = true;
                if (!IsAssignable(sym->type, rhsType, expr.location, sym->name)) {
                    // Also tag R1004 specifically when a `var` variable is assigned an incompatible type
                    m_diagnostics.ReportError(
                        "R1004",
                        "Variable '" + sym->name + "' was locked to type '" + sym->type.ToString() + "' and cannot be reassigned a value of type '" + rhsType.ToString() + "'.",
                        expr.location,
                        "Keep '" + sym->name + "' as '" + sym->type.ToString() + "' or declare a new variable.",
                        sym->type.ToString(),
                        rhsType.ToString());
                }
                expr.inferredType = sym->type;
                return expr.inferredType;
            }

            TypeNode lhsType = expr.left ? CheckExpression(*expr.left) : MakePrimitiveType("Any");
            IsAssignable(lhsType, rhsType, expr.location, "assignment target");
            expr.inferredType = lhsType;
            return expr.inferredType;
        }

        case ExprKind::ArrayLiteral: {
            TypeNode elemType = MakePrimitiveType("Any");
            for (std::size_t i = 0; i < expr.arguments.size(); ++i) {
                TypeNode argType = CheckExpression(*expr.arguments[i]);
                if (i == 0) {
                    elemType = argType;
                } else if (!AreTypesEqual(elemType, argType) && elemType.baseName != "Any") {
                    m_diagnostics.ReportError(
                        "R1001",
                        "Array literal elements must all share the same type; expected '" + elemType.ToString() + "' but found '" + argType.ToString() + "'.",
                        expr.arguments[i]->location,
                        "Ensure all elements in the array literal have type '" + elemType.ToString() + "'.",
                        elemType.ToString(),
                        argType.ToString());
                }
            }
            TypeNode arrType;
            arrType.baseName = "Array";
            arrType.typeArgs.push_back(elemType);
            expr.inferredType = arrType;
            return expr.inferredType;
        }

        case ExprKind::ObjectLiteral: {
            for (auto& [fieldName, fieldExpr] : expr.namedFields) {
                if (fieldExpr) {
                    CheckExpression(*fieldExpr);
                }
            }
            expr.inferredType = MakePrimitiveType("Object");
            return expr.inferredType;
        }

        case ExprKind::NewObject: {
            for (auto& arg : expr.arguments) {
                if (arg) CheckExpression(*arg);
            }
            for (auto& [fieldName, fieldExpr] : expr.namedFields) {
                if (fieldExpr) CheckExpression(*fieldExpr);
            }
            expr.inferredType = expr.targetType;
            return expr.inferredType;
        }

        case ExprKind::MemberAccess: {
            TypeNode recvType = expr.left ? CheckExpression(*expr.left) : MakePrimitiveType("Any");

            if (recvType.isNullable && expr.token.kind != TokenKind::QuestionDot) {
                m_diagnostics.ReportError(
                    "R1005",
                    "Cannot access member '" + expr.text + "' on potentially null value of type '" + recvType.ToString() + "'.",
                    expr.location,
                    "Check '!= null' in an 'if' condition or use the null-safe member operator '?.' instead.");
            }

            if (recvType.baseName == "Memory" && (expr.text == "Allocate" || expr.text == "Free" || expr.text == "Copy")) {
                if (m_unsafeDepth <= 0) {
                    m_diagnostics.ReportError(
                        "R1006",
                        "'Memory." + expr.text + "' is a manual memory operation and requires an explicit 'Unsafe { ... }' block.",
                        expr.location,
                        "Wrap manual memory management calls inside 'Unsafe { ... }'.");
                }
                if (expr.text == "Allocate") {
                    TypeNode ptr;
                    ptr.baseName  = "Pointer";
                    ptr.isPointer = true;
                    ptr.typeArgs.push_back(MakePrimitiveType("Byte"));
                    expr.inferredType = ptr;
                    return expr.inferredType;
                }
                expr.inferredType = MakePrimitiveType("Void");
                return expr.inferredType;
            }

            // Built-in explicit conversion and utility methods
            if (expr.text == "ToFloat") {
                expr.inferredType = MakePrimitiveType("Float", recvType.isNullable);
                return expr.inferredType;
            }
            if (expr.text == "ToInt") {
                expr.inferredType = MakePrimitiveType("Int", recvType.isNullable);
                return expr.inferredType;
            }
            if (expr.text == "ToString") {
                expr.inferredType = MakePrimitiveType("String", recvType.isNullable);
                return expr.inferredType;
            }
            if (expr.text == "Length" || expr.text == "Count") {
                expr.inferredType = MakePrimitiveType("Int", recvType.isNullable);
                return expr.inferredType;
            }
            if (expr.text == "Enumerate") {
                TypeNode enumType;
                enumType.baseName = "Enumerate";
                if (!recvType.typeArgs.empty()) {
                    enumType.typeArgs.push_back(recvType.typeArgs[0]);
                } else {
                    enumType.typeArgs.push_back(MakePrimitiveType("Any"));
                }
                expr.inferredType = enumType;
                return expr.inferredType;
            }

            // Lookup member on user-defined class/struct/enum/service
            if (const TypeDefinitionInfo* def = FindTypeDefinition(recvType.baseName)) {
                auto fIt = def->fields.find(expr.text);
                if (fIt != def->fields.end()) {
                    expr.inferredType = fIt->second.type;
                    if (expr.token.kind == TokenKind::QuestionDot) {
                        expr.inferredType.isNullable = true;
                    }
                    return expr.inferredType;
                }
                auto mIt = def->methods.find(expr.text);
                if (mIt != def->methods.end()) {
                    expr.inferredType = mIt->second.returnType;
                    if (expr.token.kind == TokenKind::QuestionDot) {
                        expr.inferredType.isNullable = true;
                    }
                    return expr.inferredType;
                }
                if (def->enumVariants.find(expr.text) != def->enumVariants.end()) {
                    expr.inferredType = MakePrimitiveType(def->name);
                    return expr.inferredType;
                }
            }

            expr.inferredType = MakePrimitiveType("Any", expr.token.kind == TokenKind::QuestionDot);
            return expr.inferredType;
        }

        case ExprKind::Call: {
            for (auto& arg : expr.arguments) {
                if (arg) CheckExpression(*arg);
            }
            for (auto& [name, val] : expr.namedFields) {
                if (val) CheckExpression(*val);
            }

            if (expr.left && expr.left->kind == ExprKind::Identifier) {
                const std::string& fnName = expr.left->text;
                if (const FunctionSignature* sig = FindFunction(fnName)) {
                    // Check positional arguments if not variadic/Any
                    for (std::size_t i = 0; i < expr.arguments.size() && i < sig->parameters.size(); ++i) {
                        const auto& expectedParam = sig->parameters[i];
                        if (expectedParam.type.baseName != "Any" && !expectedParam.type.IsEmpty()) {
                            IsAssignable(
                                expectedParam.type,
                                expr.arguments[i]->inferredType,
                                expr.arguments[i]->location,
                                "parameter '" + expectedParam.name + "' of '" + fnName + "'");
                        }
                    }
                    expr.inferredType = sig->returnType;
                    return expr.inferredType;
                }
            }

            TypeNode calleeType = expr.left ? CheckExpression(*expr.left) : MakePrimitiveType("Any");
            expr.inferredType = calleeType;
            return expr.inferredType;
        }

        case ExprKind::Index: {
            TypeNode containerType = expr.left ? CheckExpression(*expr.left) : MakePrimitiveType("Any");
            if (expr.right) {
                CheckExpression(*expr.right);
            }
            if (!containerType.typeArgs.empty()) {
                expr.inferredType = containerType.typeArgs.back();
            } else {
                expr.inferredType = MakePrimitiveType("Any");
            }
            return expr.inferredType;
        }

        case ExprKind::Lambda: {
            PushScope();
            for (const auto& p : expr.lambdaParams) {
                SymbolInfo sym{ p, MakePrimitiveType("Any"), VarMutability::ImmutableLet, true, false, expr.location };
                DeclareLocal(sym);
            }
            if (expr.lambdaBody) {
                CheckStatement(*expr.lambdaBody);
            }
            PopScope();
            expr.inferredType = MakePrimitiveType("Function");
            return expr.inferredType;
        }

        case ExprKind::MatchExpr: {
            if (expr.left) {
                CheckExpression(*expr.left);
            }
            TypeNode armResult = MakePrimitiveType("Any");
            for (auto& arm : expr.matchArms) {
                PushScope();
                if (!arm.bindingVar.empty()) {
                    SymbolInfo sym{ arm.bindingVar, MakePrimitiveType("Any"), VarMutability::ImmutableLet, true, false, arm.location };
                    DeclareLocal(sym);
                }
                if (arm.bodyExpr) {
                    armResult = CheckExpression(*arm.bodyExpr);
                }
                if (arm.bodyStmt) {
                    CheckStatement(*arm.bodyStmt);
                }
                PopScope();
            }
            expr.inferredType = armResult;
            return expr.inferredType;
        }
    }

    return MakePrimitiveType("Any");
}

bool TypeChecker::IsAssignable(const TypeNode& target,
                               const TypeNode& valueType,
                               SourceLocation loc,
                               const std::string& contextName) {
    if (target.IsEmpty() || target.baseName == "Any" || valueType.baseName == "Any") {
        return true;
    }

    // Assigning `null`
    if (valueType.baseName == "Null") {
        if (target.isNullable) {
            return true;
        }
        m_diagnostics.ReportError(
            "R1005",
            "Cannot assign 'null' to non-nullable type '" + target.ToString() + "' for '" + contextName + "'.",
            loc,
            "Change the type to '" + target.ToString() + "?' if 'null' is a valid state.",
            target.ToString(),
            "null");
        return false;
    }

    // Assigning nullable T? to non-nullable T
    if (valueType.isNullable && !target.isNullable) {
        m_diagnostics.ReportError(
            "R1005",
            "Cannot assign nullable value of type '" + valueType.ToString() + "' to non-nullable '" + target.ToString() + "' for '" + contextName + "'.",
            loc,
            "Use null-coalescing to provide a default value or check 'if (" + contextName + " != null)' first.",
            target.ToString(),
            valueType.ToString());
        return false;
    }

    // Result<T, E> compatibility with Success(...) / Error(...)
    if (target.baseName == "Result" && valueType.baseName == "Result") {
        return true;
    }

    // Non-nullable T can be assigned to nullable T?
    TypeNode targetNonNull = target;
    targetNonNull.isNullable = false;
    TypeNode valueNonNull = valueType;
    valueNonNull.isNullable = false;

    if (AreTypesEqual(targetNonNull, valueNonNull)) {
        return true;
    }

    // Allow Array<T> literal to initialize List<T> or Span<T> if element types match
    if ((targetNonNull.baseName == "List" || targetNonNull.baseName == "Span") &&
        valueNonNull.baseName == "Array" &&
        targetNonNull.typeArgs.size() == 1 &&
        valueNonNull.typeArgs.size() == 1 &&
        AreTypesEqual(targetNonNull.typeArgs[0], valueNonNull.typeArgs[0])) {
        return true;
    }

    // Check class inheritance / interface implementation
    if (const TypeDefinitionInfo* valDef = FindTypeDefinition(valueNonNull.baseName)) {
        if (valDef->baseClassName == targetNonNull.baseName) {
            return true;
        }
        for (const auto& iface : valDef->implementedInterfaces) {
            if (iface == targetNonNull.baseName) {
                return true;
            }
        }
    }

    std::string suggestion = "Change the type of '" + contextName + "' to '" + valueType.ToString() + "' or convert the value explicitly.";
    if (targetNonNull.baseName == "Int" && valueNonNull.baseName == "String") {
        suggestion = "Change the type to 'String' or use an integer value.";
    } else if ((IsIntegerType(targetNonNull) && IsFloatingPointType(valueNonNull)) ||
               (IsFloatingPointType(targetNonNull) && IsIntegerType(valueNonNull))) {
        suggestion = "Use explicit numeric conversion '.ToFloat()' or '.ToInt()'.";
    }

    m_diagnostics.ReportError(
        "R1001",
        "Cannot assign value of type '" + valueType.ToString() + "' to '" + contextName + "' of type '" + target.ToString() + "'.",
        loc,
        suggestion,
        target.ToString(),
        valueType.ToString());
    return false;
}

} // namespace Robix::Frontend
