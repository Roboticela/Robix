#include "SemanticAnalyzer.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace Robix::Frontend {

namespace {

std::int64_t ParseIntLiteralValue(std::string_view text) {
    std::string cleaned;
    cleaned.reserve(text.size());
    for (char c : text) {
        if (c != '_') {
            cleaned.push_back(c);
        }
    }
    if (cleaned.size() > 2 && cleaned[0] == '0' && (cleaned[1] == 'x' || cleaned[1] == 'X')) {
        return std::strtoll(cleaned.c_str() + 2, nullptr, 16);
    }
    if (cleaned.size() > 2 && cleaned[0] == '0' && (cleaned[1] == 'b' || cleaned[1] == 'B')) {
        return std::strtoll(cleaned.c_str() + 2, nullptr, 2);
    }
    return std::strtoll(cleaned.c_str(), nullptr, 10);
}

double ParseFloatLiteralValue(std::string_view text) {
    std::string cleaned;
    cleaned.reserve(text.size());
    for (char c : text) {
        if (c != '_') {
            cleaned.push_back(c);
        }
    }
    return std::strtod(cleaned.c_str(), nullptr);
}

std::string StripQuotes(std::string_view text) {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return std::string(text.substr(1, text.size() - 2));
    }
    return std::string(text);
}

SemanticMethodInfo BuildMethodInfoFromDecl(const DeclNode& m, const std::string& enclosingTypeName = {}) {
    SemanticMethodInfo info;
    info.name = m.name;
    for (const auto& p : m.parameters) {
        info.parameters.push_back(FunctionParamInfo{
            p.name,
            p.type,
            p.defaultValue != nullptr,
            p.isMutable,
            p.isVariadic
        });
    }
    info.returnType = (m.kind == DeclKind::Constructor)
                          ? TypeNode{ enclosingTypeName, {}, false, false, false, false, m.location }
                          : m.returnType;
    info.visibility  = m.visibility;
    info.isStatic    = m.isStatic;
    info.isAbstract  = m.isAbstract;
    info.isFinal     = m.isFinal;
    info.isOverride  = m.isOverride;
    info.isOperator  = m.isOperator;
    info.isConsteval = m.isConsteval;
    info.astNode     = &m;
    info.location    = m.location;

    for (const auto& attr : m.attributes) {
        if (attr.name == "RealTime") {
            info.isRealTime = true;
        } else if (attr.name == "Deprecated") {
            info.isDeprecated = true;
            if (!attr.args.empty()) {
                info.deprecatedMessage = StripQuotes(attr.args[0]);
            }
        }
    }
    return info;
}

} // namespace

SemanticAnalyzer::SemanticAnalyzer(DiagnosticBag& diagnostics, SemanticOptions options)
    : m_diagnostics(diagnostics)
    , m_options(options) {}

const SemanticClassInfo* SemanticAnalyzer::FindClassOrType(const std::string& name) const {
    auto it = m_classes.find(name);
    return (it != m_classes.end()) ? &it->second : nullptr;
}

const SemanticMethodInfo* SemanticAnalyzer::FindGlobalFunction(const std::string& name) const {
    auto it = m_globalFunctions.find(name);
    return (it != m_globalFunctions.end()) ? &it->second : nullptr;
}

bool SemanticAnalyzer::HasMacro(const std::string& macroName) const {
    return m_macros.find(macroName) != m_macros.end();
}

std::optional<ReflectionTypeMetadata> SemanticAnalyzer::GetReflectionMetadata(const std::string& typeName) const {
    const auto* cls = FindClassOrType(typeName);
    if (!cls) {
        return std::nullopt;
    }
    ReflectionTypeMetadata meta;
    meta.typeName = cls->name;
    meta.kind     = cls->kind;
    for (const auto& [fName, _] : cls->fields) {
        meta.fieldNames.push_back(fName);
    }
    std::sort(meta.fieldNames.begin(), meta.fieldNames.end());
    for (const auto& [mName, _] : cls->methods) {
        meta.methodNames.push_back(mName);
    }
    std::sort(meta.methodNames.begin(), meta.methodNames.end());
    for (const auto& attr : cls->attributes) {
        meta.attributeNames.push_back(attr.name);
    }
    return meta;
}

bool SemanticAnalyzer::CanDevirtualizeCall(const std::string& className, const std::string& methodName) const {
    const auto* cls = FindClassOrType(className);
    if (!cls) {
        return false;
    }
    if (cls->kind == DeclKind::Interface || cls->kind == DeclKind::Trait || cls->isAbstract) {
        return false;
    }
    if (cls->isFinal || cls->kind == DeclKind::Struct || !cls->hasDerivedClasses) {
        return true;
    }
    auto mIt = cls->methods.find(methodName);
    if (mIt != cls->methods.end()) {
        const auto& m = mIt->second;
        if (m.isFinal || m.isStatic || m.visibility == Spec::AccessVisibility::Private) {
            return true;
        }
    }
    return false;
}

bool SemanticAnalyzer::AnalyzeCompilationUnit(CompilationUnitAST& unit) {
    m_currentPackage = unit.packageName;
    CollectAndMergeTypesAndFunctions(unit);
    ValidateInheritanceAndBuildVTables();

    for (const auto& declPtr : unit.declarations) {
        if (declPtr) {
            AnalyzeDeclaration(*declPtr);
        }
    }

    return !m_diagnostics.HasErrors();
}

void SemanticAnalyzer::CollectAndMergeTypesAndFunctions(const CompilationUnitAST& unit) {
    for (const auto& declPtr : unit.declarations) {
        if (!declPtr) continue;
        const DeclNode& decl = *declPtr;

        if (decl.kind == DeclKind::Macro) {
            m_macros[decl.name] = &decl;
            continue;
        }

        if (decl.kind == DeclKind::Function) {
            SemanticMethodInfo fnInfo = BuildMethodInfoFromDecl(decl);
            if (decl.isOperator) {
                if (decl.parameters.empty() || decl.parameters.size() > 2 ||
                    decl.returnType.baseName == "Void" || decl.returnType.IsEmpty()) {
                    m_diagnostics.ReportError(
                        "R2009",
                        "Operator overload '" + decl.name + "' must declare 1 or 2 parameters and a non-Void return type.",
                        decl.location,
                        "Provide 1 or 2 parameters and a valid return type for '" + decl.name + "'.");
                }
            }
            m_overloads[decl.name].push_back(fnInfo);
            m_globalFunctions[decl.name] = std::move(fnInfo);
            continue;
        }

        if (decl.kind == DeclKind::Class ||
            decl.kind == DeclKind::Struct ||
            decl.kind == DeclKind::Interface ||
            decl.kind == DeclKind::Trait ||
            decl.kind == DeclKind::Enum ||
            decl.kind == DeclKind::Service) {
            auto existingIt = m_classes.find(decl.name);
            if (existingIt != m_classes.end()) {
                if (!decl.isPartial || !existingIt->second.isPartial) {
                    m_diagnostics.ReportError(
                        "R2000",
                        "Duplicate type definition '" + decl.name + "' without 'partial' modifier.",
                        decl.location,
                        "Mark all parts of '" + decl.name + "' as 'partial class' or rename the type.");
                    continue;
                }
            } else {
                SemanticClassInfo newCls;
                newCls.name       = decl.name;
                newCls.kind       = decl.kind;
                newCls.visibility = decl.visibility;
                newCls.isAbstract = decl.isAbstract || (decl.kind == DeclKind::Interface) || (decl.kind == DeclKind::Trait);
                newCls.isFinal    = decl.isFinal;
                newCls.isPartial  = decl.isPartial;
                newCls.location   = decl.location;
                m_classes[decl.name] = std::move(newCls);
            }

            SemanticClassInfo& cls = m_classes[decl.name];
            if (!decl.baseClassName.empty()) {
                cls.baseClassName = decl.baseClassName;
            }
            for (const auto& iface : decl.implementedInterfaces) {
                if (std::find(cls.implementedInterfaces.begin(), cls.implementedInterfaces.end(), iface) ==
                    cls.implementedInterfaces.end()) {
                    cls.implementedInterfaces.push_back(iface);
                }
            }
            for (const auto& attr : decl.attributes) {
                cls.attributes.push_back(attr);
            }
            for (const auto& v : decl.enumVariants) {
                cls.enumVariants.push_back(v);
            }

            for (const auto& memberPtr : decl.members) {
                if (!memberPtr) continue;
                const DeclNode& m = *memberPtr;
                if (m.kind == DeclKind::Field) {
                    SemanticFieldInfo fInfo;
                    fInfo.name       = m.name;
                    fInfo.type       = m.fieldType;
                    fInfo.visibility = m.visibility;
                    fInfo.mutability = m.fieldMutability;
                    fInfo.isStatic   = m.isStatic;
                    fInfo.location   = m.location;
                    cls.fields[m.name] = std::move(fInfo);
                } else if (m.kind == DeclKind::Destructor) {
                    cls.hasCustomDestructor = true;
                } else if (m.kind == DeclKind::Function || m.kind == DeclKind::Constructor) {
                    SemanticMethodInfo mInfo = BuildMethodInfoFromDecl(m, decl.name);
                    if (cls.kind == DeclKind::Interface) {
                        mInfo.isAbstract = true;
                    }
                    if (m.isOperator) {
                        if (m.parameters.size() > 2 || m.returnType.baseName == "Void" || m.returnType.IsEmpty()) {
                            m_diagnostics.ReportError(
                                "R2009",
                                "Member operator overload '" + m.name + "' must have 1 parameter and a non-Void return type.",
                                m.location);
                        }
                    }
                    cls.methods[m.name] = std::move(mInfo);
                }
            }
        }
    }
}

bool SemanticAnalyzer::HasInheritanceCycle(const std::string& className) const {
    std::unordered_set<std::string> seen;
    std::string curr = className;
    while (!curr.empty()) {
        if (!seen.insert(curr).second) {
            return true;
        }
        const auto* cls = FindClassOrType(curr);
        if (!cls) break;
        curr = cls->baseClassName;
    }
    return false;
}

bool SemanticAnalyzer::IsDerivedFrom(const std::string& derivedName, const std::string& baseName) const {
    std::unordered_set<std::string> seen;
    std::string curr = derivedName;
    while (!curr.empty() && seen.insert(curr).second) {
        const auto* cls = FindClassOrType(curr);
        if (!cls) break;
        if (cls->baseClassName == baseName) {
            return true;
        }
        curr = cls->baseClassName;
    }
    return false;
}

void SemanticAnalyzer::ValidateInheritanceAndBuildVTables() {
    for (auto& [name, cls] : m_classes) {
        if (!cls.baseClassName.empty()) {
            if (HasInheritanceCycle(name)) {
                m_diagnostics.ReportError(
                    "R2003",
                    "Circular inheritance hierarchy detected involving class '" + name + "'.",
                    cls.location,
                    "Break the inheritance cycle so class hierarchies form a directed acyclic graph.");
                continue;
            }
            auto baseIt = m_classes.find(cls.baseClassName);
            if (baseIt != m_classes.end()) {
                baseIt->second.hasDerivedClasses = true;
                if (baseIt->second.isFinal) {
                    m_diagnostics.ReportError(
                        "R2001",
                        "Class '" + name + "' cannot inherit from final class '" + cls.baseClassName + "'.",
                        cls.location,
                        "Remove 'final' from '" + cls.baseClassName + "' or use composition instead.");
                }
            }
        }
    }

    std::unordered_set<std::string> visited;
    for (auto& [name, cls] : m_classes) {
        BuildClassVTable(cls, visited);
    }

    for (const auto& [name, cls] : m_classes) {
        ValidateInterfaceAndAbstractImplementations(cls);
    }
}

void SemanticAnalyzer::BuildClassVTable(SemanticClassInfo& cls, std::unordered_set<std::string>& visited) {
    if (!visited.insert(cls.name).second) {
        return;
    }

    // First build base class vtable so we inherit its slots in order
    if (!cls.baseClassName.empty()) {
        auto baseIt = m_classes.find(cls.baseClassName);
        if (baseIt != m_classes.end()) {
            BuildClassVTable(baseIt->second, visited);
            cls.vtable = baseIt->second.vtable;
        }
    }

    for (const auto& [mName, method] : cls.methods) {
        if (method.isStatic || mName == cls.name) {
            continue; // Static methods and constructors are not in the virtual table
        }

        // Check if this method exists in base class vtable
        auto slotIt = std::find_if(cls.vtable.begin(), cls.vtable.end(), [&](const VTableSlot& s) {
            return s.methodName == mName;
        });

        // Also check if this method is declared in any implemented interface or trait
        bool inInterfaceOrTrait = false;
        for (const auto& ifaceName : cls.implementedInterfaces) {
            if (const auto* iface = FindClassOrType(ifaceName)) {
                if (iface->methods.find(mName) != iface->methods.end()) {
                    inInterfaceOrTrait = true;
                    break;
                }
            }
        }

        if (slotIt != cls.vtable.end()) {
            if (slotIt->isFinal) {
                m_diagnostics.ReportError(
                    "R2006",
                    "Method '" + mName + "' in '" + cls.name + "' cannot override final method from '" + slotIt->definingClassName + "'.",
                    method.location,
                    "Remove the overriding method or remove 'final' from the base method.");
            }
            if (!method.isOverride) {
                m_diagnostics.ReportError(
                    "R2007",
                    "Method '" + mName + "' in '" + cls.name + "' hides inherited method from '" + slotIt->definingClassName + "' without 'override'.",
                    method.location,
                    "Add the 'override' keyword to '" + mName + "'.");
            }
            slotIt->definingClassName = cls.name;
            slotIt->returnType        = method.returnType;
            slotIt->isAbstract        = method.isAbstract;
            slotIt->isFinal           = method.isFinal;
        } else {
            if (method.isOverride && !inInterfaceOrTrait && cls.baseClassName.empty()) {
                m_diagnostics.ReportError(
                    "R2005",
                    "Method '" + mName + "' in '" + cls.name + "' is marked 'override' but does not override any base or interface method.",
                    method.location,
                    "Remove 'override' or check the method name against the base class/interface.");
            } else if (method.isOverride && !inInterfaceOrTrait && !cls.baseClassName.empty() &&
                       FindClassOrType(cls.baseClassName) != nullptr) {
                // Base class is defined in this unit and does not contain mName
                m_diagnostics.ReportError(
                    "R2005",
                    "Method '" + mName + "' in '" + cls.name + "' is marked 'override' but base class '" + cls.baseClassName + "' has no such method.",
                    method.location,
                    "Check the method signature in '" + cls.baseClassName + "'.");
            }

            cls.vtable.push_back(VTableSlot{
                mName,
                cls.name,
                method.returnType,
                method.isAbstract,
                method.isFinal
            });
        }
    }
}

void SemanticAnalyzer::ValidateInterfaceAndAbstractImplementations(const SemanticClassInfo& cls) {
    if (cls.kind != DeclKind::Class || cls.isAbstract) {
        return;
    }

    // 1. Check for unimplemented abstract methods in vtable
    for (const auto& slot : cls.vtable) {
        if (slot.isAbstract) {
            m_diagnostics.ReportError(
                "R2004",
                "Concrete class '" + cls.name + "' does not implement abstract method '" + slot.methodName + "' from '" + slot.definingClassName + "'.",
                cls.location,
                "Implement 'public override " + slot.returnType.ToString() + " " + slot.methodName + "(...)' in '" + cls.name + "' or mark '" + cls.name + "' as 'abstract'.");
        }
    }

    // 2. Check all implemented interfaces and traits
    for (const auto& ifaceName : cls.implementedInterfaces) {
        const auto* iface = FindClassOrType(ifaceName);
        if (!iface) continue;

        for (const auto& [reqName, reqMethod] : iface->methods) {
            if (!reqMethod.isAbstract && reqMethod.astNode && reqMethod.astNode->body != nullptr) {
                continue; // Trait method with default implementation
            }
            auto foundInClass = cls.methods.find(reqName);
            bool foundInVTable = std::any_of(cls.vtable.begin(), cls.vtable.end(), [&](const VTableSlot& s) {
                return s.methodName == reqName && !s.isAbstract;
            });

            if (foundInClass == cls.methods.end() && !foundInVTable) {
                m_diagnostics.ReportError(
                    "R2004",
                    "Class '" + cls.name + "' claims to implement '" + ifaceName + "' but is missing required method '" + reqName + "'.",
                    cls.location,
                    "Implement '" + reqName + "' in '" + cls.name + "'.");
            } else if (foundInClass != cls.methods.end()) {
                if (foundInClass->second.parameters.size() != reqMethod.parameters.size()) {
                    m_diagnostics.ReportError(
                        "R2004",
                        "Method '" + reqName + "' in '" + cls.name + "' has " +
                            std::to_string(foundInClass->second.parameters.size()) +
                            " parameter(s), but interface '" + ifaceName + "' requires " +
                            std::to_string(reqMethod.parameters.size()) + ".",
                        foundInClass->second.location);
                }
            }
        }
    }
}

void SemanticAnalyzer::AnalyzeDeclaration(const DeclNode& decl) {
    switch (decl.kind) {
        case DeclKind::Function: {
            auto it = m_globalFunctions.find(decl.name);
            if (it != m_globalFunctions.end()) {
                AnalyzeFunctionOrMethod(decl, it->second);
            } else {
                SemanticMethodInfo temp = BuildMethodInfoFromDecl(decl);
                AnalyzeFunctionOrMethod(decl, temp);
            }
            break;
        }

        case DeclKind::Class:
        case DeclKind::Struct:
        case DeclKind::Interface:
        case DeclKind::Trait:
        case DeclKind::Service:
        case DeclKind::Implement: {
            std::string prevClass = m_currentClass;
            m_currentClass = decl.name;

            for (const auto& memberPtr : decl.members) {
                if (!memberPtr) continue;
                const DeclNode& m = *memberPtr;
                if (m.kind == DeclKind::Function || m.kind == DeclKind::Constructor || m.kind == DeclKind::Destructor) {
                    SemanticMethodInfo mInfo = BuildMethodInfoFromDecl(m, decl.name);
                    AnalyzeFunctionOrMethod(m, mInfo);
                } else if (m.kind == DeclKind::Field && m.fieldInitializer) {
                    AnalyzeExpression(*m.fieldInitializer, false);
                }
            }

            m_currentClass = prevClass;
            break;
        }

        case DeclKind::TopLevelVar:
            if (decl.fieldInitializer) {
                AnalyzeExpression(*decl.fieldInitializer, false);
            }
            break;

        case DeclKind::Test:
            if (decl.body) {
                std::vector<std::string> cleanups;
                AnalyzeStatement(*decl.body, cleanups);
            }
            break;

        default:
            break;
    }
}

void SemanticAnalyzer::AnalyzeFunctionOrMethod(const DeclNode& decl, const SemanticMethodInfo& methodMeta) {
    if (!decl.body) {
        return;
    }

    const SemanticMethodInfo* prevMethod = m_currentMethod;
    m_currentMethod = &methodMeta;
    m_outerFunctionVars.clear();
    for (const auto& p : decl.parameters) {
        m_outerFunctionVars[p.name] = p.isMutable ? VarMutability::MutableVar : VarMutability::ImmutableLet;
    }

    ScopeCleanupPlan plan;
    plan.functionName = m_currentClass.empty() ? decl.name : (m_currentClass + "." + decl.name);

    std::vector<std::string> rootCleanups;
    AnalyzeStatement(*decl.body, rootCleanups);

    // Flush remaining root cleanups in LIFO order (reverse of registration)
    for (auto it = rootCleanups.rbegin(); it != rootCleanups.rend(); ++it) {
        plan.lifoCleanupActions.push_back(*it);
    }

    if (!plan.lifoCleanupActions.empty()) {
        m_cleanupPlans.push_back(std::move(plan));
    }

    m_currentMethod = prevMethod;
}

void SemanticAnalyzer::AnalyzeStatement(const StmtNode& stmt, std::vector<std::string>& scopeDeferActions) {
    switch (stmt.kind) {
        case StmtKind::Block: {
            std::vector<std::string> blockCleanups;
            for (const auto& s : stmt.statements) {
                if (s) {
                    AnalyzeStatement(*s, blockCleanups);
                }
            }
            // Append block cleanups in LIFO order to the enclosing function cleanup tracker
            for (auto it = blockCleanups.rbegin(); it != blockCleanups.rend(); ++it) {
                scopeDeferActions.insert(scopeDeferActions.begin(), *it);
            }
            break;
        }

        case StmtKind::VarDecl: {
            if (!m_insideLambda) {
                m_outerFunctionVars[stmt.varName] = stmt.mutability;
            }
            if (stmt.initializer) {
                AnalyzeExpression(*stmt.initializer, false);
            }
            break;
        }

        case StmtKind::DeferStmt: {
            scopeDeferActions.push_back("Defer@Line" + std::to_string(stmt.location.line));
            if (stmt.thenBranch) {
                std::vector<std::string> inner;
                AnalyzeStatement(*stmt.thenBranch, inner);
            }
            break;
        }

        case StmtKind::UsingStmt: {
            if (stmt.initializer) {
                AnalyzeExpression(*stmt.initializer, false);
            }
            const std::string resourceLabel = stmt.varName.empty()
                                                  ? ("Using@Line" + std::to_string(stmt.location.line))
                                                  : ("Using(" + stmt.varName + ")");
            scopeDeferActions.push_back(resourceLabel);
            if (stmt.thenBranch) {
                std::vector<std::string> inner;
                AnalyzeStatement(*stmt.thenBranch, inner);
            }
            break;
        }

        case StmtKind::IfStmt: {
            if (stmt.condition) AnalyzeExpression(*stmt.condition, false);
            if (stmt.thenBranch) AnalyzeStatement(*stmt.thenBranch, scopeDeferActions);
            if (stmt.elseBranch) AnalyzeStatement(*stmt.elseBranch, scopeDeferActions);
            break;
        }

        case StmtKind::WhileStmt:
        case StmtKind::DoWhileStmt:
        case StmtKind::RepeatStmt:
        case StmtKind::LoopStmt:
        case StmtKind::ForRangeOrCollection:
        case StmtKind::ForCStyle: {
            if (stmt.initStmt) AnalyzeStatement(*stmt.initStmt, scopeDeferActions);
            if (stmt.condition) AnalyzeExpression(*stmt.condition, false);
            if (stmt.stepExpr) AnalyzeExpression(*stmt.stepExpr, false);
            if (stmt.thenBranch) AnalyzeStatement(*stmt.thenBranch, scopeDeferActions);
            break;
        }

        case StmtKind::MatchStmt: {
            TypeNode targetType;
            if (stmt.condition) {
                AnalyzeExpression(*stmt.condition, false);
                targetType = stmt.condition->inferredType;
            }
            ValidateMatchExhaustiveness(targetType, stmt.matchArms, stmt.location);
            for (const auto& arm : stmt.matchArms) {
                if (arm.bodyExpr) AnalyzeExpression(*arm.bodyExpr, false);
                if (arm.bodyStmt) AnalyzeStatement(*arm.bodyStmt, scopeDeferActions);
            }
            break;
        }

        case StmtKind::TryCatchStmt: {
            if (stmt.thenBranch) AnalyzeStatement(*stmt.thenBranch, scopeDeferActions);
            if (stmt.catchBranch) AnalyzeStatement(*stmt.catchBranch, scopeDeferActions);
            if (stmt.finallyBranch) AnalyzeStatement(*stmt.finallyBranch, scopeDeferActions);
            break;
        }

        case StmtKind::ThrowStmt: {
            if (m_currentMethod && m_currentMethod->isRealTime) {
                m_diagnostics.ReportError(
                    "R2015",
                    "Exception 'throw' is forbidden inside '@RealTime' function '" + m_currentMethod->name + "'.",
                    stmt.location,
                    "Use deterministic 'Result<T, E>' error handling in '@RealTime' functions.");
            }
            if (stmt.condition) AnalyzeExpression(*stmt.condition, false);
            break;
        }

        case StmtKind::ReturnStmt: {
            if (stmt.condition) AnalyzeExpression(*stmt.condition, false);
            break;
        }

        case StmtKind::UnsafeStmt: {
            if (stmt.thenBranch) AnalyzeStatement(*stmt.thenBranch, scopeDeferActions);
            break;
        }

        case StmtKind::ExprStmt: {
            if (stmt.condition) {
                AnalyzeExpression(*stmt.condition, true);
            }
            break;
        }

        default:
            break;
    }
}

void SemanticAnalyzer::AnalyzeExpression(const ExprNode& expr, bool isDiscardedStmtExpr) {
    switch (expr.kind) {
        case ExprKind::NewObject: {
            const std::string& typeName = expr.targetType.baseName;
            if (const auto* cls = FindClassOrType(typeName)) {
                if (cls->isAbstract || cls->kind == DeclKind::Interface || cls->kind == DeclKind::Trait) {
                    m_diagnostics.ReportError(
                        "R2002",
                        "Cannot instantiate abstract class or interface '" + typeName + "'.",
                        expr.location,
                        "Instantiate a concrete derived class that implements '" + typeName + "'.");
                }
            }
            for (const auto& arg : expr.arguments) {
                if (arg) AnalyzeExpression(*arg, false);
            }
            break;
        }

        case ExprKind::MemberAccess: {
            if (expr.left) {
                AnalyzeExpression(*expr.left, false);
                const std::string& recvType = expr.left->inferredType.baseName;

                if (recvType == "Memory" && expr.text == "Allocate" &&
                    m_currentMethod && m_currentMethod->isRealTime) {
                    m_diagnostics.ReportError(
                        "R2015",
                        "Unbounded heap allocation 'Memory.Allocate' is forbidden inside '@RealTime' function '" + m_currentMethod->name + "'.",
                        expr.location,
                        "Pre-allocate a fixed-capacity pool or stack buffer outside the '@RealTime' loop.");
                }

                CheckMemberAccessVisibility(recvType, expr.text, expr.location);
            }
            break;
        }

        case ExprKind::Call: {
            if (expr.left) {
                AnalyzeExpression(*expr.left, false);
            }
            for (const auto& arg : expr.arguments) {
                if (arg) AnalyzeExpression(*arg, false);
            }
            for (const auto& [name, val] : expr.namedFields) {
                if (val) AnalyzeExpression(*val, false);
            }

            DetectStreamFusion(expr);

            // Resolve target function or method metadata
            const SemanticMethodInfo* calleeMeta = nullptr;
            if (expr.left && expr.left->kind == ExprKind::Identifier) {
                calleeMeta = ResolveGlobalCall(expr.left->text, expr.arguments.size(), expr.location);
                if (!calleeMeta && !m_currentClass.empty()) {
                    if (const auto* cls = FindClassOrType(m_currentClass)) {
                        auto mIt = cls->methods.find(expr.left->text);
                        if (mIt != cls->methods.end()) {
                            calleeMeta = &mIt->second;
                        }
                    }
                }
            } else if (expr.left && expr.left->kind == ExprKind::MemberAccess && expr.left->left) {
                const std::string& recvType = expr.left->left->inferredType.baseName;
                if (const auto* cls = FindClassOrType(recvType)) {
                    auto mIt = cls->methods.find(expr.left->text);
                    if (mIt != cls->methods.end()) {
                        calleeMeta = &mIt->second;
                    }
                }
            }

            if (calleeMeta) {
                if (calleeMeta->isDeprecated) {
                    std::string msg = "Call to deprecated function '" + calleeMeta->name + "'.";
                    if (!calleeMeta->deprecatedMessage.empty()) {
                        msg += " " + calleeMeta->deprecatedMessage;
                    }
                    m_diagnostics.ReportWarning("W2014", msg, expr.location);
                }
                ValidateCallArguments(*calleeMeta, expr);

                if (isDiscardedStmtExpr && calleeMeta->returnType.baseName == "Result") {
                    if (m_options.strictResultHandling) {
                        m_diagnostics.ReportError(
                            "R2012",
                            "Unused 'Result' return value from '" + calleeMeta->name + "' must not be silently ignored.",
                            expr.location,
                            "Handle the Result with 'match', assign it to a variable, or propagate with '?'.");
                    } else {
                        m_diagnostics.ReportWarning(
                            "W2012",
                            "Unused 'Result' return value from '" + calleeMeta->name + "'.",
                            expr.location,
                            "Handle the Result with 'match' or assign it.");
                    }
                }
            }
            break;
        }

        case ExprKind::MatchExpr: {
            TypeNode targetType;
            if (expr.left) {
                AnalyzeExpression(*expr.left, false);
                targetType = expr.left->inferredType;
            }
            ValidateMatchExhaustiveness(targetType, expr.matchArms, expr.location);
            for (const auto& arm : expr.matchArms) {
                if (arm.bodyExpr) AnalyzeExpression(*arm.bodyExpr, false);
                if (arm.bodyStmt) {
                    std::vector<std::string> inner;
                    AnalyzeStatement(*arm.bodyStmt, inner);
                }
            }
            break;
        }

        case ExprKind::Lambda: {
            bool prevLambda = m_insideLambda;
            m_insideLambda = true;
            if (expr.left) {
                AnalyzeExpression(*expr.left, false);
            }
            if (expr.lambdaBody) {
                std::vector<std::string> inner;
                AnalyzeStatement(*expr.lambdaBody, inner);
            }
            m_insideLambda = prevLambda;
            break;
        }

        case ExprKind::Assignment: {
            if (m_insideLambda && expr.left && expr.left->kind == ExprKind::Identifier) {
                auto outerIt = m_outerFunctionVars.find(expr.left->text);
                if (outerIt != m_outerFunctionVars.end() && outerIt->second == VarMutability::ImmutableLet) {
                    m_diagnostics.ReportError(
                        "R1002",
                        "Closure cannot mutate captured immutable 'let' variable '" + expr.left->text + "'.",
                        expr.location,
                        "Declare '" + expr.left->text + "' with 'var' if mutation inside a closure is required.");
                }
            }
            if (expr.left) AnalyzeExpression(*expr.left, false);
            if (expr.right) AnalyzeExpression(*expr.right, false);
            break;
        }

        default: {
            if (expr.left) AnalyzeExpression(*expr.left, false);
            if (expr.right) AnalyzeExpression(*expr.right, false);
            for (const auto& arg : expr.arguments) {
                if (arg) AnalyzeExpression(*arg, false);
            }
            for (const auto& [name, val] : expr.namedFields) {
                if (val) AnalyzeExpression(*val, false);
            }
            break;
        }
    }
}

void SemanticAnalyzer::CheckMemberAccessVisibility(const std::string& targetTypeName,
                                                   const std::string& memberName,
                                                   SourceLocation loc) {
    const auto* cls = FindClassOrType(targetTypeName);
    if (!cls) {
        return;
    }

    Spec::AccessVisibility declaredVis = Spec::AccessVisibility::Public;
    bool found = false;

    auto fIt = cls->fields.find(memberName);
    if (fIt != cls->fields.end()) {
        declaredVis = fIt->second.visibility;
        found = true;
    } else {
        auto mIt = cls->methods.find(memberName);
        if (mIt != cls->methods.end()) {
            declaredVis = mIt->second.visibility;
            found = true;
        }
    }

    if (!found) {
        return;
    }

    Spec::AccessContext ctx;
    ctx.isSameClassOrStruct = (m_currentClass == targetTypeName);
    ctx.isDerivedClass      = (!m_currentClass.empty() && IsDerivedFrom(m_currentClass, targetTypeName));
    ctx.isSameModuleFile    = true;
    ctx.isSamePackage       = true;
    ctx.isSameBuildTarget   = true;

    if (!Spec::IsAccessPermitted(declaredVis, ctx)) {
        const char* visName = (declaredVis == Spec::AccessVisibility::Private) ? "private" : "protected";
        m_diagnostics.ReportError(
            "R2008",
            "Cannot access " + std::string(visName) + " member '" + memberName + "' of type '" + targetTypeName + "'.",
            loc,
            "Use a public accessor method or adjust the visibility of '" + memberName + "'.");
    }
}

const SemanticMethodInfo* SemanticAnalyzer::ResolveGlobalCall(const std::string& name,
                                                               std::size_t argc,
                                                               SourceLocation loc) {
    auto it = m_overloads.find(name);
    if (it == m_overloads.end() || it->second.empty()) {
        return FindGlobalFunction(name);
    }
    const SemanticMethodInfo* match = nullptr;
    int matches = 0;
    for (const auto& sig : it->second) {
        bool variadic = false;
        for (const auto& p : sig.parameters) {
            if (p.isVariadic) variadic = true;
        }
        if (variadic || sig.parameters.size() == argc) {
            match = &sig;
            ++matches;
        }
    }
    if (matches > 1) {
        m_diagnostics.ReportError(
            "R2010",
            "Call to '" + name + "' matches " + std::to_string(matches) + " overloads with " +
                std::to_string(argc) + " argument(s).",
            loc,
            "Change the argument list so only one overload applies.");
        return nullptr;
    }
    if (matches == 1) return match;
    return FindGlobalFunction(name);
}

void SemanticAnalyzer::ValidateCallArguments(const SemanticMethodInfo& callee, const ExprNode& callExpr) {
    bool isVariadic = false;
    for (const auto& p : callee.parameters) {
        if (p.isVariadic) {
            isVariadic = true;
            break;
        }
    }

    const std::size_t totalSupplied = callExpr.arguments.size() + callExpr.namedFields.size();
    if (!isVariadic && totalSupplied > callee.parameters.size()) {
        m_diagnostics.ReportError(
            "R2010",
            "Function '" + callee.name + "' expects at most " + std::to_string(callee.parameters.size()) +
                " argument(s), but " + std::to_string(totalSupplied) + " were provided.",
            callExpr.location);
        return;
    }

    std::unordered_set<std::string> boundParams;
    for (std::size_t i = 0; i < callExpr.arguments.size() && i < callee.parameters.size(); ++i) {
        boundParams.insert(callee.parameters[i].name);
    }

    for (const auto& [argName, argVal] : callExpr.namedFields) {
        bool exists = false;
        for (const auto& param : callee.parameters) {
            if (param.name == argName) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_diagnostics.ReportError(
                "R2010",
                "Unknown named argument '" + argName + "' in call to '" + callee.name + "'.",
                callExpr.location,
                "Check the parameter names of '" + callee.name + "'.");
        } else {
            boundParams.insert(argName);
        }
    }

    for (const auto& param : callee.parameters) {
        if (param.hasDefault || param.isVariadic) {
            continue;
        }
        if (boundParams.find(param.name) == boundParams.end()) {
            m_diagnostics.ReportError(
                "R2010",
                "Missing required argument for parameter '" + param.name + "' in call to '" + callee.name + "'.",
                callExpr.location,
                "Pass a value for '" + param.name + "' or define a default parameter value.");
        }
    }
}

void SemanticAnalyzer::DetectStreamFusion(const ExprNode& callExpr) {
    // Look for chained calls: `collection.Filter(...).Map(...)` or `.Filter(...).Map(...).Reduce(...)`
    std::vector<std::string> stages;
    const ExprNode* curr = &callExpr;
    std::string sourceName;

    while (curr && curr->kind == ExprKind::Call && curr->left && curr->left->kind == ExprKind::MemberAccess) {
        const std::string& method = curr->left->text;
        if (method == "Filter" || method == "Map" || method == "Reduce") {
            stages.push_back(method);
            const ExprNode* receiver = curr->left->left.get();
            if (receiver && receiver->kind == ExprKind::Identifier) {
                sourceName = receiver->text;
            }
            curr = receiver;
        } else {
            break;
        }
    }

    if (stages.size() >= 2) {
        std::reverse(stages.begin(), stages.end());
        StreamFusionPipeline pipeline;
        pipeline.sourceCollectionExpr              = sourceName;
        pipeline.stages                            = std::move(stages);
        pipeline.eliminatesIntermediateAllocations = true;
        pipeline.location                          = callExpr.location;

        if (!m_fusedPipelines.empty() &&
            m_fusedPipelines.back().sourceCollectionExpr == pipeline.sourceCollectionExpr &&
            m_fusedPipelines.back().stages.size() + 1 == pipeline.stages.size()) {
            m_fusedPipelines.back() = std::move(pipeline);
        } else {
            m_fusedPipelines.push_back(std::move(pipeline));
        }
    }
}

void SemanticAnalyzer::ValidateMatchExhaustiveness(const TypeNode& targetType,
                                                   const std::vector<MatchArmNode>& arms,
                                                   SourceLocation loc) {
    std::unordered_set<std::string> covered;
    for (const auto& arm : arms) {
        if (arm.patternName == "_" || arm.patternName == "default") {
            return; // Wildcard arm covers all remaining cases
        }
        // Normalize `EnumName.Variant` -> `Variant`
        const std::size_t dotPos = arm.patternName.rfind('.');
        if (dotPos != std::string::npos) {
            covered.insert(arm.patternName.substr(dotPos + 1));
        } else {
            covered.insert(arm.patternName);
        }
    }

    std::vector<std::string> requiredPatterns;
    if (targetType.baseName == "Result") {
        requiredPatterns = { "Success", "Error" };
    } else if (targetType.baseName == "Option") {
        requiredPatterns = { "Some", "None" };
    } else if (targetType.baseName == "Bool") {
        requiredPatterns = { "true", "false" };
    } else if (const auto* enumInfo = FindClassOrType(targetType.baseName)) {
        if (enumInfo->kind == DeclKind::Enum) {
            requiredPatterns = enumInfo->enumVariants;
        }
    }

    std::vector<std::string> missing;
    for (const auto& req : requiredPatterns) {
        if (covered.find(req) == covered.end()) {
            missing.push_back(req);
        }
    }

    if (!missing.empty()) {
        std::string missingList;
        for (std::size_t i = 0; i < missing.size(); ++i) {
            if (i > 0) missingList += ", ";
            missingList += "'" + missing[i] + "'";
        }
        m_diagnostics.ReportError(
            "R2011",
            "Non-exhaustive 'match' on type '" + targetType.ToString() + "': missing pattern arm(s) " + missingList + ".",
            loc,
            "Add match arms for " + missingList + " or add a wildcard '_ => ...' arm.");
    }
}

// ============================================================================
// COMPILE-TIME EXECUTION (CONSTEVAL) ENGINE WITH BOUNDED STEP/DEPTH LIMITS
// ============================================================================

ConstevalValue SemanticAnalyzer::EvaluateConstevalCall(const std::string& fnName,
                                                       const std::vector<ConstevalValue>& args,
                                                       SourceLocation callLoc) {
    const auto* fnMeta = FindGlobalFunction(fnName);
    if (!fnMeta || !fnMeta->astNode || !fnMeta->astNode->body) {
        m_diagnostics.ReportError(
            "R2013",
            "Cannot evaluate unknown or bodyless consteval function '" + fnName + "'.",
            callLoc);
        return {};
    }
    if (!fnMeta->isConsteval) {
        m_diagnostics.ReportError(
            "R2013",
            "Function '" + fnName + "' is not marked 'consteval' and cannot be executed at compile time.",
            callLoc,
            "Add the 'consteval' modifier to '" + fnName + "'.");
        return {};
    }

    std::unordered_map<std::string, ConstevalValue> env;
    for (std::size_t i = 0; i < fnMeta->parameters.size() && i < args.size(); ++i) {
        env[fnMeta->parameters[i].name] = args[i];
    }

    bool returned = false;
    std::size_t stepCounter = 0;
    return EvalStmt(*fnMeta->astNode->body, env, returned, stepCounter, 1, callLoc);
}

ConstevalValue SemanticAnalyzer::EvalStmt(const StmtNode& stmt,
                                          std::unordered_map<std::string, ConstevalValue>& env,
                                          bool& returned,
                                          std::size_t& stepCounter,
                                          std::size_t depth,
                                          SourceLocation rootLoc) {
    if (++stepCounter > m_options.maxConstevalSteps) {
        m_diagnostics.ReportError(
            "R2013",
            "Compile-time 'consteval' execution exceeded maximum step limit (" +
                std::to_string(m_options.maxConstevalSteps) + " steps).",
            rootLoc.line > 0 ? rootLoc : stmt.location,
            "Ensure the 'consteval' function terminates within bounded steps.");
        returned = true;
        return {};
    }

    switch (stmt.kind) {
        case StmtKind::Block: {
            for (const auto& s : stmt.statements) {
                if (!s) continue;
                ConstevalValue val = EvalStmt(*s, env, returned, stepCounter, depth, rootLoc);
                if (returned) {
                    return val;
                }
            }
            return {};
        }

        case StmtKind::VarDecl: {
            ConstevalValue initVal{};
            if (stmt.initializer) {
                initVal = EvalExpr(*stmt.initializer, env, stepCounter, depth, rootLoc);
            }
            env[stmt.varName] = initVal;
            return {};
        }

        case StmtKind::IfStmt: {
            ConstevalValue cond = stmt.condition ? EvalExpr(*stmt.condition, env, stepCounter, depth, rootLoc) : ConstevalValue{};
            if (cond.AsBool()) {
                if (stmt.thenBranch) {
                    return EvalStmt(*stmt.thenBranch, env, returned, stepCounter, depth, rootLoc);
                }
            } else if (stmt.elseBranch) {
                return EvalStmt(*stmt.elseBranch, env, returned, stepCounter, depth, rootLoc);
            }
            return {};
        }

        case StmtKind::WhileStmt: {
            while (stmt.condition) {
                ConstevalValue cond = EvalExpr(*stmt.condition, env, stepCounter, depth, rootLoc);
                if (!cond.AsBool() || returned) {
                    break;
                }
                if (stmt.thenBranch) {
                    ConstevalValue v = EvalStmt(*stmt.thenBranch, env, returned, stepCounter, depth, rootLoc);
                    if (returned) return v;
                }
            }
            return {};
        }

        case StmtKind::ForRangeOrCollection: {
            if (stmt.condition && stmt.condition->kind == ExprKind::Range &&
                stmt.condition->left && stmt.condition->right) {
                std::int64_t start = EvalExpr(*stmt.condition->left, env, stepCounter, depth, rootLoc).AsInt();
                std::int64_t end   = EvalExpr(*stmt.condition->right, env, stepCounter, depth, rootLoc).AsInt();
                std::int64_t step  = stmt.stepExpr ? EvalExpr(*stmt.stepExpr, env, stepCounter, depth, rootLoc).AsInt() : 1;
                if (step <= 0) step = 1;
                const bool inclusive = stmt.condition->isInclusiveRange;
                for (std::int64_t i = start; inclusive ? (i <= end) : (i < end); i += step) {
                    env[stmt.loopVar1] = ConstevalValue{ i };
                    if (stmt.thenBranch) {
                        ConstevalValue v = EvalStmt(*stmt.thenBranch, env, returned, stepCounter, depth, rootLoc);
                        if (returned) return v;
                    }
                }
            }
            return {};
        }

        case StmtKind::ReturnStmt: {
            ConstevalValue result{};
            if (stmt.condition) {
                result = EvalExpr(*stmt.condition, env, stepCounter, depth, rootLoc);
            }
            returned = true;
            return result;
        }

        case StmtKind::ExprStmt: {
            if (stmt.condition) {
                (void)EvalExpr(*stmt.condition, env, stepCounter, depth, rootLoc);
            }
            return {};
        }

        default:
            return {};
    }
}

ConstevalValue SemanticAnalyzer::EvalExpr(const ExprNode& expr,
                                          std::unordered_map<std::string, ConstevalValue>& env,
                                          std::size_t& stepCounter,
                                          std::size_t depth,
                                          SourceLocation rootLoc) {
    if (++stepCounter > m_options.maxConstevalSteps) {
        m_diagnostics.ReportError(
            "R2013",
            "Compile-time 'consteval' execution exceeded maximum step limit (" +
                std::to_string(m_options.maxConstevalSteps) + " steps).",
            rootLoc.line > 0 ? rootLoc : expr.location);
        return {};
    }

    switch (expr.kind) {
        case ExprKind::Literal: {
            switch (expr.token.kind) {
                case TokenKind::IntegerLiteral:
                    return ConstevalValue{ ParseIntLiteralValue(expr.text) };
                case TokenKind::FloatLiteral:
                    return ConstevalValue{ ParseFloatLiteralValue(expr.text) };
                case TokenKind::BoolLiteral:
                    return ConstevalValue{ expr.text == "true" };
                case TokenKind::StringLiteral:
                case TokenKind::MultilineString:
                    return ConstevalValue{ StripQuotes(expr.text) };
                default:
                    return {};
            }
        }

        case ExprKind::Identifier: {
            auto it = env.find(expr.text);
            if (it != env.end()) {
                return it->second;
            }
            return {};
        }

        case ExprKind::Unary: {
            ConstevalValue v = expr.left ? EvalExpr(*expr.left, env, stepCounter, depth, rootLoc) : ConstevalValue{};
            if (expr.text == "-") {
                if (v.IsFloat()) return ConstevalValue{ -v.AsFloat() };
                return ConstevalValue{ -v.AsInt() };
            }
            if (expr.text == "!") {
                return ConstevalValue{ !v.AsBool() };
            }
            return v;
        }

        case ExprKind::Binary: {
            ConstevalValue l = expr.left ? EvalExpr(*expr.left, env, stepCounter, depth, rootLoc) : ConstevalValue{};
            ConstevalValue r = expr.right ? EvalExpr(*expr.right, env, stepCounter, depth, rootLoc) : ConstevalValue{};
            const std::string& op = expr.text;

            if (op == "+" && l.IsString() && r.IsString()) {
                return ConstevalValue{ l.AsString() + r.AsString() };
            }
            if (l.IsFloat() || r.IsFloat()) {
                const double lf = l.AsFloat();
                const double rf = r.AsFloat();
                if (op == "+") return ConstevalValue{ lf + rf };
                if (op == "-") return ConstevalValue{ lf - rf };
                if (op == "*") return ConstevalValue{ lf * rf };
                if (op == "/") return ConstevalValue{ rf != 0.0 ? (lf / rf) : 0.0 };
                if (op == "<") return ConstevalValue{ lf < rf };
                if (op == "<=") return ConstevalValue{ lf <= rf };
                if (op == ">") return ConstevalValue{ lf > rf };
                if (op == ">=") return ConstevalValue{ lf >= rf };
                if (op == "==") return ConstevalValue{ lf == rf };
                if (op == "!=") return ConstevalValue{ lf != rf };
            }
            if (l.IsInt() && r.IsInt()) {
                const std::int64_t li = l.AsInt();
                const std::int64_t ri = r.AsInt();
                if (op == "+") return ConstevalValue{ li + ri };
                if (op == "-") return ConstevalValue{ li - ri };
                if (op == "*") return ConstevalValue{ li * ri };
                if (op == "/") return ConstevalValue{ ri != 0 ? (li / ri) : 0 };
                if (op == "%") return ConstevalValue{ ri != 0 ? (li % ri) : 0 };
                if (op == "<") return ConstevalValue{ li < ri };
                if (op == "<=") return ConstevalValue{ li <= ri };
                if (op == ">") return ConstevalValue{ li > ri };
                if (op == ">=") return ConstevalValue{ li >= ri };
                if (op == "==") return ConstevalValue{ li == ri };
                if (op == "!=") return ConstevalValue{ li != ri };
            }
            if (op == "&&") return ConstevalValue{ l.AsBool() && r.AsBool() };
            if (op == "||") return ConstevalValue{ l.AsBool() || r.AsBool() };
            return {};
        }

        case ExprKind::Assignment: {
            ConstevalValue rhs = expr.right ? EvalExpr(*expr.right, env, stepCounter, depth, rootLoc) : ConstevalValue{};
            if (expr.left && expr.left->kind == ExprKind::Identifier) {
                const std::string& varName = expr.left->text;
                if (expr.text == "=") {
                    env[varName] = rhs;
                } else if (expr.text == "+=") {
                    env[varName] = ConstevalValue{ env[varName].AsInt() + rhs.AsInt() };
                } else if (expr.text == "-=") {
                    env[varName] = ConstevalValue{ env[varName].AsInt() - rhs.AsInt() };
                } else if (expr.text == "*=") {
                    env[varName] = ConstevalValue{ env[varName].AsInt() * rhs.AsInt() };
                }
                return env[varName];
            }
            return rhs;
        }

        case ExprKind::Call: {
            if (depth >= m_options.maxConstevalDepth) {
                m_diagnostics.ReportError(
                    "R2013",
                    "Compile-time 'consteval' recursion depth exceeded maximum limit (" +
                        std::to_string(m_options.maxConstevalDepth) + ").",
                    rootLoc.line > 0 ? rootLoc : expr.location);
                return {};
            }
            if (expr.left && expr.left->kind == ExprKind::Identifier) {
                const auto* fnMeta = FindGlobalFunction(expr.left->text);
                if (fnMeta && fnMeta->isConsteval && fnMeta->astNode && fnMeta->astNode->body) {
                    std::unordered_map<std::string, ConstevalValue> calleeEnv;
                    for (std::size_t i = 0; i < fnMeta->parameters.size() && i < expr.arguments.size(); ++i) {
                        calleeEnv[fnMeta->parameters[i].name] =
                            EvalExpr(*expr.arguments[i], env, stepCounter, depth + 1, rootLoc);
                    }
                    bool calleeReturned = false;
                    return EvalStmt(*fnMeta->astNode->body, calleeEnv, calleeReturned, stepCounter, depth + 1, rootLoc);
                }
            }
            return {};
        }

        default:
            return {};
    }
}

} // namespace Robix::Frontend

