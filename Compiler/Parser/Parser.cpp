#include "Parser.hpp"

#include <utility>

namespace Robix::Frontend {

namespace {

std::string UnquoteString(std::string_view raw) {
    if (raw.size() >= 6 && raw.substr(0, 3) == "\"\"\"" && raw.substr(raw.size() - 3) == "\"\"\"") {
        return std::string(raw.substr(3, raw.size() - 6));
    }
    if (raw.size() >= 3 && (raw[0] == '$' || raw[0] == 'r') && raw[1] == '"' && raw.back() == '"') {
        return std::string(raw.substr(2, raw.size() - 3));
    }
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
        return std::string(raw.substr(1, raw.size() - 2));
    }
    return std::string(raw);
}

bool LooksLikeExplicitTypeVarDecl(const Token& first, const Token& second, const Token& third) noexcept {
    if (first.kind != TokenKind::Identifier) {
        return false;
    }
    // Simple Type varName = ... or Type varName;
    if (second.kind == TokenKind::Identifier &&
        (third.kind == TokenKind::Equal || third.kind == TokenKind::Semicolon)) {
        return true;
    }
    // Nullable Type? varName = ...
    if (second.kind == TokenKind::Question && third.kind == TokenKind::Identifier) {
        return true;
    }
    // Generic Type<...> varName = ...
    if (second.kind == TokenKind::Less) {
        return true;
    }
    return false;
}

} // namespace

Parser::Parser(const std::vector<Token>& tokens,
               std::string_view fileName,
               DiagnosticBag& diagnostics) noexcept
    : m_tokens(tokens), m_fileName(fileName), m_diagnostics(diagnostics) {}

const Token& Parser::Current() const noexcept {
    return m_tokens[m_pos < m_tokens.size() ? m_pos : m_tokens.size() - 1];
}

const Token& Parser::Peek(std::size_t offset) const noexcept {
    const std::size_t idx = m_pos + offset;
    return m_tokens[idx < m_tokens.size() ? idx : m_tokens.size() - 1];
}

bool Parser::IsAtEnd() const noexcept {
    return Current().kind == TokenKind::EndOfFile;
}

bool Parser::Check(TokenKind kind) const noexcept {
    return !IsAtEnd() && Current().kind == kind;
}

const Token& Parser::Advance() noexcept {
    if (!IsAtEnd()) {
        ++m_pos;
    }
    return m_tokens[m_pos - 1];
}

bool Parser::Match(TokenKind kind) noexcept {
    if (Check(kind)) {
        Advance();
        return true;
    }
    return false;
}

const Token& Parser::Expect(TokenKind kind, const char* code, const char* errorMsg) {
    if (Check(kind)) {
        return Advance();
    }
    m_diagnostics.ReportError(
        code,
        errorMsg,
        Current().location,
        "Check syntax near '" + std::string(Current().lexeme) + "'.");
    return Current();
}

void Parser::Synchronize() noexcept {
    Advance();
    while (!IsAtEnd()) {
        if (m_tokens[m_pos - 1].kind == TokenKind::Semicolon ||
            m_tokens[m_pos - 1].kind == TokenKind::RBrace) {
            return;
        }
        switch (Current().kind) {
            case TokenKind::KwClass:
            case TokenKind::KwStruct:
            case TokenKind::KwEnum:
            case TokenKind::KwInterface:
            case TokenKind::KwTrait:
            case TokenKind::KwFunction:
            case TokenKind::KwPublic:
            case TokenKind::KwPrivate:
            case TokenKind::KwProtected:
            case TokenKind::KwLet:
            case TokenKind::KwVar:
            case TokenKind::KwConst:
            case TokenKind::KwIf:
            case TokenKind::KwFor:
            case TokenKind::KwWhile:
            case TokenKind::KwReturn:
            case TokenKind::KwTest:
                return;
            default:
                Advance();
                break;
        }
    }
}

std::string Parser::ParseQualifiedName() {
    std::string result(Current().lexeme);
    Advance();
    while (Match(TokenKind::Dot)) {
        result += '.';
        result += Current().lexeme;
        Advance();
    }
    return result;
}

TypeNode Parser::ParseType() {
    TypeNode type;
    type.location = Current().location;

    if (Match(TokenKind::Amp)) {
        type.isReference = true;
        if (Check(TokenKind::Identifier) && Current().lexeme == "mut") {
            type.isMutable = true;
            Advance();
        }
    }

    if (Check(TokenKind::Identifier)) {
        type.baseName = ParseQualifiedName();
    } else if (Check(TokenKind::LBrace)) {
        // Anonymous structured object type { Name: String, ... }
        Advance();
        type.baseName = "Object";
        while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
            Advance();
        }
        Match(TokenKind::RBrace);
        return type;
    } else {
        type.baseName = std::string(Current().lexeme);
        Advance();
    }

    if (type.baseName == "Pointer") {
        type.isPointer = true;
    }

    // Generic arguments <T, U>
    if (Match(TokenKind::Less)) {
        do {
            if (Check(TokenKind::KwConst) || (Check(TokenKind::Identifier) && Current().lexeme == "mut")) {
                Advance();
            }
            type.typeArgs.push_back(ParseType());
        } while (Match(TokenKind::Comma));

        // Handle nested '>>' token splitting if needed
        if (Check(TokenKind::ShiftRight)) {
            // Consume as closing '>'
            Advance();
        } else {
            Expect(TokenKind::Greater, "P1001", "Expected '>' after generic type arguments");
        }
    }

    if (Match(TokenKind::Question)) {
        type.isNullable = true;
    }

    return type;
}

std::vector<std::string> Parser::ParseOptionalGenericParams() {
    std::vector<std::string> params;
    if (Match(TokenKind::Less)) {
        do {
            params.emplace_back(Current().lexeme);
            Advance();
            if (Match(TokenKind::Colon)) {
                // Consume constraint type
                (void)ParseType();
            }
        } while (Match(TokenKind::Comma));
        Expect(TokenKind::Greater, "P1002", "Expected '>' after generic parameter list");
    }
    return params;
}

std::vector<ParameterNode> Parser::ParseParameterList() {
    std::vector<ParameterNode> params;
    if (Check(TokenKind::RParen)) {
        return params;
    }

    do {
        if (Match(TokenKind::Ellipsis)) {
            ParameterNode variadicParam;
            variadicParam.isVariadic = true;
            variadicParam.location   = Current().location;
            if (!Check(TokenKind::RParen)) {
                variadicParam.type = ParseType();
                if (Check(TokenKind::Identifier)) {
                    variadicParam.name = std::string(Advance().lexeme);
                }
            }
            params.push_back(std::move(variadicParam));
            break;
        }

        ParameterNode param;
        param.location = Current().location;
        if (Check(TokenKind::Identifier) && (Current().lexeme == "mut" || Current().lexeme == "ref")) {
            param.isMutable = true;
            Advance();
        }
        param.type = ParseType();
        if (Check(TokenKind::Identifier)) {
            param.name = std::string(Advance().lexeme);
        }
        if (Match(TokenKind::Equal)) {
            param.defaultValue = ParseExpression();
        }
        params.push_back(std::move(param));
    } while (Match(TokenKind::Comma));

    return params;
}

ExplainBlockNode Parser::ParseExplainBlock() {
    ExplainBlockNode explain;
    explain.present  = true;
    explain.location = Current().location;
    Expect(TokenKind::KwExplain, "P1003", "Expected 'Explain' keyword");
    Expect(TokenKind::LBrace, "P1004", "Expected '{' after 'Explain'");

    while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
        if (Match(TokenKind::KwDescription)) {
            Expect(TokenKind::Colon, "P1005", "Expected ':' after 'Description'");
            if (Check(TokenKind::StringLiteral) || Check(TokenKind::MultilineString)) {
                explain.description = UnquoteString(Advance().lexeme);
            }
            Match(TokenKind::Semicolon);
        } else if (Match(TokenKind::KwProcessing)) {
            Expect(TokenKind::Colon, "P1005", "Expected ':' after 'Processing'");
            if (Check(TokenKind::StringLiteral) || Check(TokenKind::MultilineString)) {
                explain.processing = UnquoteString(Advance().lexeme);
            }
            Match(TokenKind::Semicolon);
        } else if (Match(TokenKind::KwOutput)) {
            Expect(TokenKind::Colon, "P1005", "Expected ':' after 'Output'");
            if (Check(TokenKind::StringLiteral) || Check(TokenKind::MultilineString)) {
                explain.output = UnquoteString(Advance().lexeme);
            }
            Match(TokenKind::Semicolon);
        } else if (Match(TokenKind::KwSideEffects)) {
            Expect(TokenKind::Colon, "P1005", "Expected ':' after 'SideEffects'");
            if (Check(TokenKind::StringLiteral) || Check(TokenKind::MultilineString)) {
                explain.sideEffects = UnquoteString(Advance().lexeme);
            }
            Match(TokenKind::Semicolon);
        } else if (Match(TokenKind::KwInput)) {
            const bool hasColon  = Match(TokenKind::Colon);
            const bool hasBraces = Match(TokenKind::LBrace);
            if (!hasColon && !hasBraces) {
                Expect(TokenKind::Colon, "P1005", "Expected ':' or '{' after 'Input'");
            }
            // Parse compact form (a = "desc", b = "desc") or multiline form (a: "desc";)
            while (!IsAtEnd() && Check(TokenKind::Identifier) &&
                   (Peek().kind == TokenKind::Colon || Peek().kind == TokenKind::Equal)) {
                std::string paramName(Advance().lexeme);
                Advance(); // consume ':' or '='
                std::string paramDoc;
                if (Check(TokenKind::StringLiteral) || Check(TokenKind::MultilineString)) {
                    paramDoc = UnquoteString(Advance().lexeme);
                }
                explain.inputs.emplace_back(std::move(paramName), std::move(paramDoc));
                Match(TokenKind::Comma);
                Match(TokenKind::Semicolon);
            }
            if (hasBraces) {
                Expect(TokenKind::RBrace, "P1006", "Expected '}' to close 'Input' section");
            }
            Match(TokenKind::Semicolon);
        } else if (Match(TokenKind::KwErrors)) {
            const bool hasColon  = Match(TokenKind::Colon);
            const bool hasBraces = Match(TokenKind::LBrace);
            if (!hasColon && !hasBraces) {
                Expect(TokenKind::Colon, "P1005", "Expected ':' or '{' after 'Errors'");
            }
            while (!IsAtEnd() && Check(TokenKind::Identifier) &&
                   (Peek().kind == TokenKind::Colon || Peek().kind == TokenKind::Equal)) {
                std::string errName(Advance().lexeme);
                Advance(); // consume ':' or '='
                std::string errDoc;
                if (Check(TokenKind::StringLiteral) || Check(TokenKind::MultilineString)) {
                    errDoc = UnquoteString(Advance().lexeme);
                }
                explain.errors.emplace_back(std::move(errName), std::move(errDoc));
                Match(TokenKind::Comma);
                Match(TokenKind::Semicolon);
            }
            if (hasBraces) {
                Expect(TokenKind::RBrace, "P1006", "Expected '}' to close 'Errors' section");
            }
            Match(TokenKind::Semicolon);
        } else {
            Advance();
        }
    }

    Expect(TokenKind::RBrace, "P1006", "Expected '}' to close 'Explain' block");
    return explain;
}

CompilationUnitAST Parser::ParseCompilationUnit() {
    CompilationUnitAST unit;
    unit.fileName = std::string(m_fileName);

    // Optional package declaration: either `package X.Y;` or `.rbxapi` style `Package X { ... }`
    if (Match(TokenKind::KwPackage)) {
        unit.packageName = ParseQualifiedName();
        if (Match(TokenKind::LBrace)) {
            while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
                if (Match(TokenKind::KwImport)) {
                    unit.imports.push_back(ParseQualifiedName());
                    Match(TokenKind::Semicolon);
                } else if (Match(TokenKind::KwPermissions)) {
                    Expect(TokenKind::LBrace, "P1007", "Expected '{' after 'Permissions'");
                    while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
                        unit.packagePermissions.push_back(ParseQualifiedName());
                        Match(TokenKind::Semicolon);
                    }
                    Match(TokenKind::RBrace);
                } else if (Check(TokenKind::Identifier) && Peek().kind == TokenKind::Equal) {
                    // Metadata like Version = "1.0.0";
                    Advance();
                    Advance();
                    Advance();
                    Match(TokenKind::Semicolon);
                } else {
                    auto decl = ParseTopLevelOrMemberDecl();
                    if (decl) {
                        unit.declarations.push_back(std::move(decl));
                    }
                }
            }
            Match(TokenKind::RBrace);
            return unit;
        }
        Match(TokenKind::Semicolon);
    }

    while (!IsAtEnd()) {
        if (Check(TokenKind::KwPackage) && Peek().kind == TokenKind::Identifier &&
            (Peek(2).kind == TokenKind::Dot || Peek(2).kind == TokenKind::Semicolon)) {
            Advance();
            (void)ParseQualifiedName();
            Match(TokenKind::Semicolon);
        } else if (Match(TokenKind::KwImport)) {
            unit.imports.push_back(ParseQualifiedName());
            if (Match(TokenKind::KwAs)) {
                Advance();
            }
            Match(TokenKind::Semicolon);
        } else {
            auto decl = ParseTopLevelOrMemberDecl();
            if (decl) {
                unit.declarations.push_back(std::move(decl));
            }
        }
    }

    return unit;
}

DeclPtr Parser::ParseTopLevelOrMemberDecl(std::string_view enclosingClassName) {
    std::vector<AttributeNode> attrs;
    while (Match(TokenKind::At)) {
        AttributeNode attr;
        attr.location = Current().location;
        attr.name     = std::string(Advance().lexeme);
        if (Match(TokenKind::LParen)) {
            while (!IsAtEnd() && !Check(TokenKind::RParen)) {
                attr.args.emplace_back(Advance().lexeme);
                Match(TokenKind::Comma);
            }
            Match(TokenKind::RParen);
        }
        attrs.push_back(std::move(attr));
    }

    Spec::AccessVisibility vis = enclosingClassName.empty()
                                     ? Spec::AccessVisibility::Public
                                     : Spec::AccessVisibility::Private;
    bool isStatic    = false;
    bool isAbstract  = false;
    bool isFinal     = false;
    bool isPartial   = false;
    bool isOverride  = false;
    bool isAsync     = false;
    bool isConsteval = false;
    bool isExtern    = false;

    while (!IsAtEnd()) {
        if (Match(TokenKind::KwPublic)) {
            vis = Spec::AccessVisibility::Public;
        } else if (Match(TokenKind::KwPrivate)) {
            vis = Spec::AccessVisibility::Private;
        } else if (Match(TokenKind::KwProtected)) {
            vis = Spec::AccessVisibility::Protected;
        } else if (Match(TokenKind::KwInternal)) {
            vis = Spec::AccessVisibility::Internal;
        } else if (Match(TokenKind::KwPackage)) {
            vis = Spec::AccessVisibility::Package;
        } else if (Match(TokenKind::KwStatic)) {
            isStatic = true;
        } else if (Match(TokenKind::KwAbstract)) {
            isAbstract = true;
        } else if (Match(TokenKind::KwFinal)) {
            isFinal = true;
        } else if (Match(TokenKind::KwPartial)) {
            isPartial = true;
        } else if (Match(TokenKind::KwOverride)) {
            isOverride = true;
        } else if (Match(TokenKind::KwAsync)) {
            isAsync = true;
        } else if (Match(TokenKind::KwConsteval)) {
            isConsteval = true;
        } else if (Match(TokenKind::KwExtern)) {
            isExtern = true;
        } else {
            break;
        }
    }

    if (Check(TokenKind::KwTest)) {
        return ParseTestDecl();
    }
    if (Check(TokenKind::KwTestFile)) {
        return ParseTestFileDecl();
    }
    if (Match(TokenKind::KwClass)) {
        auto cls = ParseClassOrStructOrInterface(DeclKind::Class, vis, isAbstract, isFinal, std::move(attrs));
        if (cls) cls->isPartial = isPartial;
        return cls;
    }
    if (Match(TokenKind::KwStruct)) {
        return ParseClassOrStructOrInterface(DeclKind::Struct, vis, false, false, std::move(attrs));
    }
    if (Match(TokenKind::KwInterface)) {
        return ParseClassOrStructOrInterface(DeclKind::Interface, vis, true, false, std::move(attrs));
    }
    if (Match(TokenKind::KwTrait)) {
        return ParseClassOrStructOrInterface(DeclKind::Trait, vis, true, false, std::move(attrs));
    }
    if (Match(TokenKind::KwEnum)) {
        return ParseEnumDecl(vis, std::move(attrs));
    }
    if (Match(TokenKind::KwService)) {
        return ParseServiceOrImplementDecl(DeclKind::Service, vis, std::move(attrs));
    }
    if (Match(TokenKind::KwImplement)) {
        return ParseServiceOrImplementDecl(DeclKind::Implement, vis, std::move(attrs));
    }
    if (Match(TokenKind::KwMacro)) {
        auto mac = std::make_unique<DeclNode>();
        mac->kind       = DeclKind::Macro;
        mac->visibility = vis;
        mac->location   = Current().location;
        mac->name       = std::string(Expect(TokenKind::Identifier, "P1018", "Expected macro name").lexeme);
        Expect(TokenKind::LParen, "P1019", "Expected '(' after macro name");
        mac->parameters = ParseParameterList();
        Expect(TokenKind::RParen, "P1020", "Expected ')' after macro parameters");
        if (Check(TokenKind::LBrace)) {
            mac->body = ParseBlock();
        } else {
            Match(TokenKind::Semicolon);
        }
        return mac;
    }
    if (Match(TokenKind::KwDestructor) || Match(TokenKind::Tilde)) {
        auto dtor = std::make_unique<DeclNode>();
        dtor->kind       = DeclKind::Destructor;
        dtor->visibility = vis;
        dtor->location   = Current().location;
        if (Check(TokenKind::Identifier)) {
            dtor->name = "~" + std::string(Advance().lexeme);
        } else {
            dtor->name = "~" + std::string(enclosingClassName);
        }
        Expect(TokenKind::LParen, "P1008", "Expected '(' in destructor declaration");
        Expect(TokenKind::RParen, "P1009", "Expected ')' in destructor declaration");
        if (Check(TokenKind::LBrace)) {
            dtor->body = ParseBlock();
        } else {
            Match(TokenKind::Semicolon);
        }
        return dtor;
    }
    if (Match(TokenKind::KwOperator)) {
        auto opFn = std::make_unique<DeclNode>();
        opFn->kind       = DeclKind::Function;
        opFn->isOperator = true;
        opFn->visibility = vis;
        opFn->location   = Current().location;
        opFn->name       = "operator" + std::string(Advance().lexeme);
        Expect(TokenKind::LParen, "P1019", "Expected '(' after operator symbol");
        opFn->parameters = ParseParameterList();
        Expect(TokenKind::RParen, "P1020", "Expected ')' after operator parameters");
        if (Match(TokenKind::Arrow)) {
            opFn->returnType = ParseType();
        }
        if (Check(TokenKind::LBrace)) {
            opFn->body = ParseBlock();
        } else {
            Match(TokenKind::Semicolon);
        }
        return opFn;
    }
    if (Match(TokenKind::KwFunction)) {
        auto fn = ParseFunctionOrMethodDecl(
            vis, isStatic, isAbstract, isOverride, isAsync, isConsteval, isExtern, std::move(attrs));
        if (fn) fn->isFinal = isFinal;
        return fn;
    }

    // Constructor inside class: `Constructor(...)` or `ClassName(...)`
    if (Match(TokenKind::KwConstructor) ||
        (!enclosingClassName.empty() && Check(TokenKind::Identifier) &&
         Current().lexeme == enclosingClassName && Peek().kind == TokenKind::LParen)) {
        auto ctor = std::make_unique<DeclNode>();
        ctor->kind       = DeclKind::Constructor;
        ctor->visibility = vis;
        ctor->location   = Current().location;
        if (Check(TokenKind::Identifier)) {
            ctor->name = std::string(Advance().lexeme);
        } else {
            ctor->name = std::string(enclosingClassName);
        }
        Expect(TokenKind::LParen, "P1008", "Expected '(' in constructor declaration");
        ctor->parameters = ParseParameterList();
        Expect(TokenKind::RParen, "P1009", "Expected ')' after constructor parameters");

        // Optional `: Base(...)` chaining
        if (Match(TokenKind::Colon)) {
            Match(TokenKind::KwBase);
            if (Match(TokenKind::LParen)) {
                if (!Check(TokenKind::RParen)) {
                    do {
                        ctor->baseConstructorArgs.push_back(ParseExpression());
                    } while (Match(TokenKind::Comma));
                }
                Match(TokenKind::RParen);
            }
        }
        if (Check(TokenKind::KwExplain)) {
            ctor->explainBlock = ParseExplainBlock();
        }
        if (Check(TokenKind::LBrace)) {
            ctor->body = ParseBlock();
        } else {
            Match(TokenKind::Semicolon);
        }
        return ctor;
    }

    // Top-level or member `const`, `let`, `var`
    if (Check(TokenKind::KwConst) || Check(TokenKind::KwLet) || Check(TokenKind::KwVar)) {
        StmtPtr varStmt = ParseVarOrConstDecl(true);
        auto decl = std::make_unique<DeclNode>();
        decl->kind            = enclosingClassName.empty() ? DeclKind::TopLevelVar : DeclKind::Field;
        decl->name            = varStmt->varName;
        decl->visibility      = vis;
        decl->isStatic        = isStatic;
        decl->location        = varStmt->location;
        decl->fieldMutability = varStmt->mutability;
        decl->fieldType       = std::move(varStmt->declaredType);
        decl->fieldInitializer= std::move(varStmt->initializer);
        return decl;
    }

    // C/Java-style typed declaration: `Type Name(...)` (Method/Function), `Name(...)` (inferred return), or `Type name = ...;` (Field)
    if (Check(TokenKind::Identifier)) {
        if (Peek().kind == TokenKind::LParen) {
            auto fn = ParseFunctionOrMethodDecl(vis,
                                                isStatic,
                                                isAbstract,
                                                isOverride,
                                                isAsync,
                                                isConsteval,
                                                isExtern,
                                                std::move(attrs));
            if (fn) fn->isFinal = isFinal;
            return fn;
        }
        TypeNode firstType = ParseType();
        if (Match(TokenKind::KwOperator)) {
            auto opFn = std::make_unique<DeclNode>();
            opFn->kind       = DeclKind::Function;
            opFn->isOperator = true;
            opFn->visibility = vis;
            opFn->returnType = std::move(firstType);
            opFn->location   = Current().location;
            opFn->name       = "operator" + std::string(Advance().lexeme);
            Expect(TokenKind::LParen, "P1019", "Expected '(' after operator symbol");
            opFn->parameters = ParseParameterList();
            Expect(TokenKind::RParen, "P1020", "Expected ')' after operator parameters");
            if (Check(TokenKind::LBrace)) {
                opFn->body = ParseBlock();
            } else {
                Match(TokenKind::Semicolon);
            }
            return opFn;
        }
        if (Check(TokenKind::Identifier) &&
            (Peek().kind == TokenKind::LParen || Peek().kind == TokenKind::Less)) {
            auto fn = ParseFunctionOrMethodDecl(vis,
                                                isStatic,
                                                isAbstract,
                                                isOverride,
                                                isAsync,
                                                isConsteval,
                                                isExtern,
                                                std::move(attrs),
                                                std::move(firstType));
            if (fn) fn->isFinal = isFinal;
            return fn;
        }
        if (Check(TokenKind::Identifier)) {
            auto field = std::make_unique<DeclNode>();
            field->kind            = enclosingClassName.empty() ? DeclKind::TopLevelVar : DeclKind::Field;
            field->visibility      = vis;
            field->isStatic        = isStatic;
            field->location        = Current().location;
            field->name            = std::string(Advance().lexeme);
            field->fieldType       = std::move(firstType);
            field->fieldMutability = VarMutability::MutableVar;
            if (Match(TokenKind::Equal)) {
                field->fieldInitializer = ParseExpression();
            }
            Match(TokenKind::Semicolon);
            return field;
        }
    }

    Synchronize();
    return nullptr;
}

DeclPtr Parser::ParseClassOrStructOrInterface(DeclKind kind,
                                              Spec::AccessVisibility vis,
                                              bool isAbstract,
                                              bool isFinal,
                                              std::vector<AttributeNode> attrs) {
    auto decl = std::make_unique<DeclNode>();
    decl->kind          = kind;
    decl->visibility    = vis;
    decl->isAbstract    = isAbstract;
    decl->isFinal       = isFinal;
    decl->attributes    = std::move(attrs);
    decl->location      = Current().location;
    decl->name          = std::string(Expect(TokenKind::Identifier, "P1010", "Expected type name").lexeme);
    decl->genericParams = ParseOptionalGenericParams();

    if (Match(TokenKind::KwExtends) || Match(TokenKind::Colon)) {
        decl->baseClassName = ParseQualifiedName();
    }
    if (Match(TokenKind::KwImplements)) {
        do {
            decl->implementedInterfaces.push_back(ParseQualifiedName());
        } while (Match(TokenKind::Comma));
    }

    Expect(TokenKind::LBrace, "P1011", "Expected '{' to open type body");
    while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
        auto member = ParseTopLevelOrMemberDecl(decl->name);
        if (member) {
            decl->members.push_back(std::move(member));
        }
    }
    Expect(TokenKind::RBrace, "P1012", "Expected '}' to close type body");
    return decl;
}

DeclPtr Parser::ParseEnumDecl(Spec::AccessVisibility vis, std::vector<AttributeNode> attrs) {
    auto decl = std::make_unique<DeclNode>();
    decl->kind          = DeclKind::Enum;
    decl->visibility    = vis;
    decl->attributes    = std::move(attrs);
    decl->location      = Current().location;
    decl->name          = std::string(Expect(TokenKind::Identifier, "P1013", "Expected enum name").lexeme);
    decl->genericParams = ParseOptionalGenericParams();

    Expect(TokenKind::LBrace, "P1014", "Expected '{' after enum name");
    while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
        if (Check(TokenKind::Identifier)) {
            decl->enumVariants.emplace_back(Advance().lexeme);
            if (Match(TokenKind::LParen)) {
                while (!IsAtEnd() && !Check(TokenKind::RParen)) {
                    Advance();
                }
                Match(TokenKind::RParen);
            }
            if (Match(TokenKind::Equal)) {
                (void)ParseExpression();
            }
        } else {
            Advance();
        }
        Match(TokenKind::Comma);
    }
    Expect(TokenKind::RBrace, "P1015", "Expected '}' after enum variants");
    return decl;
}

DeclPtr Parser::ParseServiceOrImplementDecl(DeclKind kind,
                                            Spec::AccessVisibility vis,
                                            std::vector<AttributeNode> attrs) {
    auto decl = std::make_unique<DeclNode>();
    decl->kind       = kind;
    decl->visibility = vis;
    decl->attributes = std::move(attrs);
    decl->location   = Current().location;
    decl->name       = ParseQualifiedName();

    Expect(TokenKind::LBrace, "P1016", "Expected '{' after Service/Implement name");
    while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
        auto member = ParseTopLevelOrMemberDecl(decl->name);
        if (member) {
            decl->members.push_back(std::move(member));
        }
    }
    Expect(TokenKind::RBrace, "P1017", "Expected '}' to close Service/Implement block");
    return decl;
}

DeclPtr Parser::ParseFunctionOrMethodDecl(Spec::AccessVisibility vis,
                                          bool isStatic,
                                          bool isAbstract,
                                          bool isOverride,
                                          bool isAsync,
                                          bool isConsteval,
                                          bool isExtern,
                                          std::vector<AttributeNode> attrs,
                                          TypeNode preParsedReturnType) {
    auto fn = std::make_unique<DeclNode>();
    fn->kind        = DeclKind::Function;
    fn->visibility  = vis;
    fn->isStatic    = isStatic;
    fn->isAbstract  = isAbstract;
    fn->isOverride  = isOverride;
    fn->isAsync     = isAsync;
    fn->isConsteval = isConsteval;
    fn->isExtern    = isExtern;
    fn->attributes  = std::move(attrs);
    fn->location    = Current().location;
    fn->name        = std::string(Expect(TokenKind::Identifier, "P1018", "Expected function or method name").lexeme);
    fn->genericParams = ParseOptionalGenericParams();

    Expect(TokenKind::LParen, "P1019", "Expected '(' after function name");
    fn->parameters = ParseParameterList();
    Expect(TokenKind::RParen, "P1020", "Expected ')' after parameter list");

    if (!preParsedReturnType.IsEmpty()) {
        fn->returnType = std::move(preParsedReturnType);
    } else if (Match(TokenKind::Arrow)) {
        fn->returnType = ParseType();
    }

    if (Match(TokenKind::KwRequires)) {
        do {
            fn->capabilities.push_back(ParseQualifiedName());
        } while (Match(TokenKind::Comma));
    }

    if (Check(TokenKind::KwExplain)) {
        fn->explainBlock = ParseExplainBlock();
    }

    if (Check(TokenKind::LBrace)) {
        auto block = std::make_unique<StmtNode>();
        block->kind     = StmtKind::Block;
        block->location = Current().location;
        Expect(TokenKind::LBrace, "P1026", "Expected '{'");
        if (Check(TokenKind::KwExplain)) {
            fn->explainBlock = ParseExplainBlock();
        }
        while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
            auto stmt = ParseStatement();
            if (stmt) {
                block->statements.push_back(std::move(stmt));
            }
        }
        Expect(TokenKind::RBrace, "P1027", "Expected '}'");
        fn->body = std::move(block);
    } else {
        Match(TokenKind::Semicolon);
    }
    return fn;
}

DeclPtr Parser::ParseTestDecl() {
    auto test = std::make_unique<DeclNode>();
    test->kind     = DeclKind::Test;
    test->location = Current().location;
    Expect(TokenKind::KwTest, "P1021", "Expected 'Test'");
    if (Check(TokenKind::StringLiteral)) {
        test->testDescription = UnquoteString(Advance().lexeme);
        test->name            = test->testDescription;
    }
    test->body = ParseBlock();
    return test;
}

DeclPtr Parser::ParseTestFileDecl() {
    auto tf = std::make_unique<DeclNode>();
    tf->kind     = DeclKind::TestFile;
    tf->location = Current().location;
    Expect(TokenKind::KwTestFile, "P1022", "Expected 'TestFile'");
    if (Check(TokenKind::StringLiteral)) {
        tf->name = UnquoteString(Advance().lexeme);
    }
    Expect(TokenKind::LBrace, "P1023", "Expected '{' after TestFile name");
    while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
        if (Match(TokenKind::KwCovers)) {
            Expect(TokenKind::Equal, "P1024", "Expected '=' after 'Covers'");
            if (Check(TokenKind::StringLiteral)) {
                tf->coversFile = UnquoteString(Advance().lexeme);
            }
            Match(TokenKind::Semicolon);
        } else if (Check(TokenKind::KwTest)) {
            tf->members.push_back(ParseTestDecl());
        } else {
            Advance();
        }
    }
    Expect(TokenKind::RBrace, "P1025", "Expected '}' to close TestFile");
    return tf;
}

StmtPtr Parser::ParseBlock() {
    auto block = std::make_unique<StmtNode>();
    block->kind     = StmtKind::Block;
    block->location = Current().location;
    Expect(TokenKind::LBrace, "P1026", "Expected '{'");
    while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
        auto stmt = ParseStatement();
        if (stmt) {
            block->statements.push_back(std::move(stmt));
        }
    }
    Expect(TokenKind::RBrace, "P1027", "Expected '}'");
    return block;
}

StmtPtr Parser::ParseVarOrConstDecl(bool expectSemicolon) {
    auto stmt = std::make_unique<StmtNode>();
    stmt->kind     = StmtKind::VarDecl;
    stmt->location = Current().location;

    if (Match(TokenKind::KwConst)) {
        stmt->mutability = VarMutability::Constant;
        if (Check(TokenKind::Identifier) && Peek().kind == TokenKind::Identifier) {
            stmt->declaredType = ParseType();
        }
        stmt->varName = std::string(Expect(TokenKind::Identifier, "P1028", "Expected constant name").lexeme);
        if (Match(TokenKind::Colon)) {
            stmt->declaredType = ParseType();
        }
        Expect(TokenKind::Equal, "P1029", "Constant declaration requires '= <value>'");
        stmt->initializer = ParseExpression();
    } else if (Match(TokenKind::KwLet) || Match(TokenKind::KwVar)) {
        stmt->mutability = (m_tokens[m_pos - 1].kind == TokenKind::KwLet)
                               ? VarMutability::ImmutableLet
                               : VarMutability::MutableVar;
        stmt->varName = std::string(Expect(TokenKind::Identifier, "P1030", "Expected variable name").lexeme);
        if (Match(TokenKind::Colon)) {
            stmt->declaredType = ParseType();
        }
        if (Match(TokenKind::Equal)) {
            stmt->initializer = ParseExpression();
        }
    } else {
        // Explicit type variable declaration: `Int speed = 100;`
        stmt->mutability   = VarMutability::MutableVar;
        stmt->declaredType = ParseType();
        stmt->varName      = std::string(Expect(TokenKind::Identifier, "P1030", "Expected variable name").lexeme);
        if (Match(TokenKind::Equal)) {
            stmt->initializer = ParseExpression();
        }
    }

    if (expectSemicolon) {
        Match(TokenKind::Semicolon);
    }
    return stmt;
}

StmtPtr Parser::ParseIfStatement() {
    auto stmt = std::make_unique<StmtNode>();
    stmt->kind     = StmtKind::IfStmt;
    stmt->location = Current().location;
    Expect(TokenKind::KwIf, "P1031", "Expected 'if'");

    const bool hasParens = Match(TokenKind::LParen);
    stmt->condition = ParseExpression();
    if (hasParens) {
        Match(TokenKind::RParen);
    }
    stmt->thenBranch = ParseBlock();
    if (Match(TokenKind::KwElse)) {
        if (Check(TokenKind::KwIf)) {
            stmt->elseBranch = ParseIfStatement();
        } else {
            stmt->elseBranch = ParseBlock();
        }
    }
    return stmt;
}

StmtPtr Parser::ParseForStatement() {
    auto stmt = std::make_unique<StmtNode>();
    stmt->location = Current().location;
    Expect(TokenKind::KwFor, "P1032", "Expected 'for'");

    // Check for `for (index, item) in ...` vs `for (Int i = 0; i < 10; i++)`
    if (Check(TokenKind::LParen) && Peek(1).kind == TokenKind::Identifier && Peek(2).kind == TokenKind::Comma) {
        Advance(); // '('
        stmt->kind     = StmtKind::ForRangeOrCollection;
        stmt->loopVar1 = std::string(Advance().lexeme);
        Match(TokenKind::Comma);
        stmt->loopVar2 = std::string(Advance().lexeme);
        Expect(TokenKind::RParen, "P1033", "Expected ')' in tuple loop binding");
        Expect(TokenKind::KwIn, "P1034", "Expected 'in' in for loop");
        stmt->condition  = ParseExpression();
        stmt->thenBranch = ParseBlock();
        return stmt;
    }

    if (Match(TokenKind::LParen)) {
        stmt->kind = StmtKind::ForCStyle;
        if (!Check(TokenKind::Semicolon)) {
            if (Check(TokenKind::KwLet) || Check(TokenKind::KwVar) ||
                LooksLikeExplicitTypeVarDecl(Current(), Peek(1), Peek(2))) {
                stmt->initStmt = ParseVarOrConstDecl(false);
            } else {
                auto exprStmt = std::make_unique<StmtNode>();
                exprStmt->kind      = StmtKind::ExprStmt;
                exprStmt->condition = ParseExpression();
                stmt->initStmt      = std::move(exprStmt);
            }
        }
        Expect(TokenKind::Semicolon, "P1035", "Expected ';' after for-loop initializer");
        if (!Check(TokenKind::Semicolon)) {
            stmt->condition = ParseExpression();
        }
        Expect(TokenKind::Semicolon, "P1036", "Expected ';' after for-loop condition");
        if (!Check(TokenKind::RParen)) {
            stmt->stepExpr = ParseExpression();
        }
        Expect(TokenKind::RParen, "P1037", "Expected ')' after for-loop clauses");
        stmt->thenBranch = ParseBlock();
        return stmt;
    }

    // `for i in 0..10` or `for i in 0..100 step 5` or `for item in collection`
    stmt->kind     = StmtKind::ForRangeOrCollection;
    stmt->loopVar1 = std::string(Expect(TokenKind::Identifier, "P1038", "Expected loop variable").lexeme);
    Expect(TokenKind::KwIn, "P1034", "Expected 'in' after loop variable");
    stmt->condition = ParseExpression();
    if (Check(TokenKind::Identifier) && Current().lexeme == "step") {
        Advance(); // consume 'step'
        stmt->stepExpr = ParseExpression();
    }
    stmt->thenBranch = ParseBlock();
    return stmt;
}

StmtPtr Parser::ParseWhileStatement() {
    auto stmt = std::make_unique<StmtNode>();
    stmt->kind     = StmtKind::WhileStmt;
    stmt->location = Current().location;
    Expect(TokenKind::KwWhile, "P1039", "Expected 'while'");
    const bool hasParens = Match(TokenKind::LParen);
    stmt->condition = ParseExpression();
    if (hasParens) {
        Match(TokenKind::RParen);
    }
    stmt->thenBranch = ParseBlock();
    return stmt;
}

StmtPtr Parser::ParseDoWhileStatement() {
    auto stmt = std::make_unique<StmtNode>();
    stmt->kind     = StmtKind::DoWhileStmt;
    stmt->location = Current().location;
    Expect(TokenKind::KwDo, "P1040", "Expected 'do'");
    stmt->thenBranch = ParseBlock();
    Expect(TokenKind::KwWhile, "P1041", "Expected 'while' after 'do' block");
    const bool hasParens = Match(TokenKind::LParen);
    stmt->condition = ParseExpression();
    if (hasParens) {
        Match(TokenKind::RParen);
    }
    Match(TokenKind::Semicolon);
    return stmt;
}

StmtPtr Parser::ParseRepeatStatement() {
    auto stmt = std::make_unique<StmtNode>();
    stmt->kind      = StmtKind::RepeatStmt;
    stmt->location  = Current().location;
    Expect(TokenKind::KwRepeat, "P1042", "Expected 'repeat'");
    stmt->condition  = ParseExpression();
    stmt->thenBranch = ParseBlock();
    return stmt;
}

StmtPtr Parser::ParseLoopStatement() {
    auto stmt = std::make_unique<StmtNode>();
    stmt->kind       = StmtKind::LoopStmt;
    stmt->location   = Current().location;
    Expect(TokenKind::KwLoop, "P1043", "Expected 'loop'");
    stmt->thenBranch = ParseBlock();
    return stmt;
}

StmtPtr Parser::ParseMatchStatement() {
    auto stmt = std::make_unique<StmtNode>();
    stmt->kind     = StmtKind::MatchStmt;
    stmt->location = Current().location;
    Expect(TokenKind::KwMatch, "P1044", "Expected 'match'");
    stmt->condition = ParseExpression();
    Expect(TokenKind::LBrace, "P1045", "Expected '{' after match expression");

    while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
        MatchArmNode arm;
        arm.location    = Current().location;
        arm.patternName = ParseQualifiedName();
        if (Match(TokenKind::LParen)) {
            if (Check(TokenKind::Identifier)) {
                arm.bindingVar = std::string(Advance().lexeme);
            }
            Match(TokenKind::RParen);
        }
        Expect(TokenKind::FatArrow, "P1046", "Expected '=>' in match arm");
        if (Check(TokenKind::LBrace)) {
            arm.bodyStmt = ParseBlock();
        } else {
            arm.bodyExpr = ParseExpression();
        }
        stmt->matchArms.push_back(std::move(arm));
        Match(TokenKind::Comma);
        Match(TokenKind::Semicolon);
    }

    Expect(TokenKind::RBrace, "P1047", "Expected '}' to close match statement");
    return stmt;
}

StmtPtr Parser::ParseTryCatchStatement() {
    auto stmt = std::make_unique<StmtNode>();
    stmt->kind       = StmtKind::TryCatchStmt;
    stmt->location   = Current().location;
    Expect(TokenKind::KwTry, "P1048", "Expected 'try'");
    stmt->thenBranch = ParseBlock();
    if (Match(TokenKind::KwCatch)) {
        const bool hasParens = Match(TokenKind::LParen);
        if (Check(TokenKind::Identifier)) {
            stmt->catchType = std::string(Advance().lexeme);
            if (Check(TokenKind::Identifier)) {
                stmt->catchVar = std::string(Advance().lexeme);
            }
        }
        if (hasParens) {
            Match(TokenKind::RParen);
        }
        stmt->catchBranch = ParseBlock();
    }
    if (Match(TokenKind::KwFinally)) {
        stmt->finallyBranch = ParseBlock();
    }
    return stmt;
}

StmtPtr Parser::ParseStatement() {
    if (Check(TokenKind::LBrace)) {
        return ParseBlock();
    }
    if (Check(TokenKind::KwLet) || Check(TokenKind::KwVar) || Check(TokenKind::KwConst) ||
        LooksLikeExplicitTypeVarDecl(Current(), Peek(1), Peek(2))) {
        return ParseVarOrConstDecl(true);
    }
    if (Check(TokenKind::KwIf)) {
        return ParseIfStatement();
    }
    if (Check(TokenKind::KwFor)) {
        return ParseForStatement();
    }
    if (Check(TokenKind::KwWhile)) {
        return ParseWhileStatement();
    }
    if (Check(TokenKind::KwDo)) {
        return ParseDoWhileStatement();
    }
    if (Check(TokenKind::KwRepeat)) {
        return ParseRepeatStatement();
    }
    if (Check(TokenKind::KwLoop)) {
        return ParseLoopStatement();
    }
    if (Check(TokenKind::KwMatch)) {
        return ParseMatchStatement();
    }
    if (Check(TokenKind::KwTry)) {
        return ParseTryCatchStatement();
    }
    if (Match(TokenKind::KwReturn)) {
        auto stmt = std::make_unique<StmtNode>();
        stmt->kind     = StmtKind::ReturnStmt;
        stmt->location = m_tokens[m_pos - 1].location;
        if (!Check(TokenKind::Semicolon) && !Check(TokenKind::RBrace)) {
            stmt->condition = ParseExpression();
        }
        Match(TokenKind::Semicolon);
        return stmt;
    }
    if (Match(TokenKind::KwBreak)) {
        auto stmt = std::make_unique<StmtNode>();
        stmt->kind     = StmtKind::BreakStmt;
        stmt->location = m_tokens[m_pos - 1].location;
        Match(TokenKind::Semicolon);
        return stmt;
    }
    if (Match(TokenKind::KwContinue)) {
        auto stmt = std::make_unique<StmtNode>();
        stmt->kind     = StmtKind::ContinueStmt;
        stmt->location = m_tokens[m_pos - 1].location;
        Match(TokenKind::Semicolon);
        return stmt;
    }
    if (Match(TokenKind::KwDefer)) {
        auto stmt = std::make_unique<StmtNode>();
        stmt->kind       = StmtKind::DeferStmt;
        stmt->location   = m_tokens[m_pos - 1].location;
        stmt->thenBranch = ParseBlock();
        return stmt;
    }
    if (Match(TokenKind::KwUsing)) {
        auto stmt = std::make_unique<StmtNode>();
        stmt->kind     = StmtKind::UsingStmt;
        stmt->location = m_tokens[m_pos - 1].location;
        Match(TokenKind::KwLet);
        Match(TokenKind::KwVar);
        if (Check(TokenKind::Identifier) && Peek().kind == TokenKind::Identifier) {
            stmt->declaredType = ParseType();
        }
        if (Check(TokenKind::Identifier)) {
            stmt->varName = std::string(Advance().lexeme);
        }
        if (Match(TokenKind::Equal)) {
            stmt->initializer = ParseExpression();
        }
        stmt->thenBranch = ParseBlock();
        return stmt;
    }
    if (Match(TokenKind::KwUnsafe)) {
        auto stmt = std::make_unique<StmtNode>();
        stmt->kind       = StmtKind::UnsafeStmt;
        stmt->location   = m_tokens[m_pos - 1].location;
        stmt->thenBranch = ParseBlock();
        return stmt;
    }
    if (Match(TokenKind::KwASM)) {
        auto stmt = std::make_unique<StmtNode>();
        stmt->kind     = StmtKind::ASMStmt;
        stmt->location = m_tokens[m_pos - 1].location;
        Expect(TokenKind::LParen, "P1049", "Expected '(' after 'ASM'");
        if (Check(TokenKind::StringLiteral)) {
            stmt->asmInstruction = UnquoteString(Advance().lexeme);
        }
        while (Match(TokenKind::Comma)) {
            const bool named = (Check(TokenKind::Identifier) || Check(TokenKind::KwIn))
                && (m_pos + 1) < m_tokens.size()
                && m_tokens[m_pos + 1].kind == TokenKind::Colon;
            if (!named) {
                (void)ParseExpression();
                continue;
            }
            const std::string key(Advance().lexeme);
            Expect(TokenKind::Colon, "P1051", "Expected ':' after an ASM operand name");
            if (key == "clobber") {
                if (!Check(TokenKind::StringLiteral)) {
                    m_diagnostics.ReportError("P1052", "ASM clobber expects a string.", Current().location);
                } else {
                    stmt->asmClobber = UnquoteString(Advance().lexeme);
                }
            } else if (key == "out") {
                if (!Check(TokenKind::Identifier)) {
                    m_diagnostics.ReportError("P1053", "ASM out expects a variable name.", Current().location);
                } else {
                    stmt->asmOutput = std::string(Advance().lexeme);
                }
            } else if (key == "in") {
                stmt->asmInputs.push_back(ParseExpression());
            } else {
                m_diagnostics.ReportError(
                    "P1054",
                    "Unknown ASM operand '" + key + "'. Use in:, out:, or clobber:.",
                    Current().location);
                (void)ParseExpression();
            }
        }
        Expect(TokenKind::RParen, "P1050", "Expected ')' after ASM arguments");
        Match(TokenKind::Semicolon);
        return stmt;
    }
    if (Match(TokenKind::KwThrow)) {
        auto stmt = std::make_unique<StmtNode>();
        stmt->kind      = StmtKind::ThrowStmt;
        stmt->location  = m_tokens[m_pos - 1].location;
        stmt->condition = ParseExpression();
        Match(TokenKind::Semicolon);
        return stmt;
    }
    if (Match(TokenKind::KwConnect)) {
        auto stmt = std::make_unique<StmtNode>();
        stmt->kind     = StmtKind::ExprStmt;
        stmt->location = m_tokens[m_pos - 1].location;
        stmt->varName  = ParseQualifiedName();
        Match(TokenKind::Semicolon);
        return stmt;
    }

    auto exprStmt = std::make_unique<StmtNode>();
    exprStmt->kind      = StmtKind::ExprStmt;
    exprStmt->location  = Current().location;
    exprStmt->condition = ParseExpression();
    Match(TokenKind::Semicolon);
    return exprStmt;
}

ExprPtr Parser::ParseExpression() {
    return ParseAssignment();
}

ExprPtr Parser::ParseAssignment() {
    // Check for single-param lambda `n => n * 2`
    if (Check(TokenKind::Identifier) && Peek().kind == TokenKind::FatArrow) {
        auto lambda = std::make_unique<ExprNode>();
        lambda->kind     = ExprKind::Lambda;
        lambda->location = Current().location;
        lambda->lambdaParams.emplace_back(Advance().lexeme);
        Advance(); // '=>'
        if (Check(TokenKind::LBrace)) {
            lambda->lambdaBody = ParseBlock();
        } else {
            lambda->left = ParseExpression();
        }
        return lambda;
    }

    ExprPtr left = ParseTernary();

    if (Check(TokenKind::Equal) || Check(TokenKind::PlusEqual) || Check(TokenKind::MinusEqual) ||
        Check(TokenKind::StarEqual) || Check(TokenKind::SlashEqual) || Check(TokenKind::PercentEqual)) {
        const Token op = Advance();
        auto assign = std::make_unique<ExprNode>();
        assign->kind     = ExprKind::Assignment;
        assign->token    = op;
        assign->text     = std::string(op.lexeme);
        assign->location = op.location;
        assign->left     = std::move(left);
        assign->right    = ParseAssignment();
        return assign;
    }

    return left;
}

ExprPtr Parser::ParseTernary() {
    ExprPtr cond = ParseNullCoalesce();
    if (Match(TokenKind::Question)) {
        auto tern = std::make_unique<ExprNode>();
        tern->kind     = ExprKind::Binary;
        tern->text     = "?:";
        tern->location = cond->location;
        tern->left     = std::move(cond);
        tern->right    = ParseExpression();
        Expect(TokenKind::Colon, "P1051", "Expected ':' in ternary conditional expression");
        tern->arguments.push_back(ParseTernary());
        return tern;
    }
    return cond;
}

ExprPtr Parser::ParseNullCoalesce() {
    ExprPtr left = ParseLogicalOr();
    while (Match(TokenKind::QuestionQuestion)) {
        const Token op = m_tokens[m_pos - 1];
        auto bin = std::make_unique<ExprNode>();
        bin->kind     = ExprKind::Binary;
        bin->token    = op;
        bin->text     = std::string(op.lexeme);
        bin->location = op.location;
        bin->left     = std::move(left);
        bin->right    = ParseLogicalOr();
        left          = std::move(bin);
    }
    return left;
}

ExprPtr Parser::ParseLogicalOr() {
    ExprPtr left = ParseLogicalAnd();
    while (Match(TokenKind::PipePipe)) {
        const Token op = m_tokens[m_pos - 1];
        auto bin = std::make_unique<ExprNode>();
        bin->kind     = ExprKind::Binary;
        bin->token    = op;
        bin->text     = std::string(op.lexeme);
        bin->location = op.location;
        bin->left     = std::move(left);
        bin->right    = ParseLogicalAnd();
        left          = std::move(bin);
    }
    return left;
}

ExprPtr Parser::ParseLogicalAnd() {
    ExprPtr left = ParseEquality();
    while (Match(TokenKind::AmpAmp)) {
        const Token op = m_tokens[m_pos - 1];
        auto bin = std::make_unique<ExprNode>();
        bin->kind     = ExprKind::Binary;
        bin->token    = op;
        bin->text     = std::string(op.lexeme);
        bin->location = op.location;
        bin->left     = std::move(left);
        bin->right    = ParseEquality();
        left          = std::move(bin);
    }
    return left;
}

ExprPtr Parser::ParseEquality() {
    ExprPtr left = ParseRelational();
    while (Check(TokenKind::EqualEqual) || Check(TokenKind::BangEqual)) {
        const Token op = Advance();
        auto bin = std::make_unique<ExprNode>();
        bin->kind     = ExprKind::Binary;
        bin->token    = op;
        bin->text     = std::string(op.lexeme);
        bin->location = op.location;
        bin->left     = std::move(left);
        bin->right    = ParseRelational();
        left          = std::move(bin);
    }
    return left;
}

ExprPtr Parser::ParseRelational() {
    ExprPtr left = ParseRange();
    while (Check(TokenKind::Less) || Check(TokenKind::LessEqual) ||
           Check(TokenKind::Greater) || Check(TokenKind::GreaterEqual)) {
        const Token op = Advance();
        auto bin = std::make_unique<ExprNode>();
        bin->kind     = ExprKind::Binary;
        bin->token    = op;
        bin->text     = std::string(op.lexeme);
        bin->location = op.location;
        bin->left     = std::move(left);
        bin->right    = ParseRange();
        left          = std::move(bin);
    }
    return left;
}

ExprPtr Parser::ParseRange() {
    ExprPtr left = ParseAdditive();
    if (Check(TokenKind::RangeExcl) || Check(TokenKind::RangeIncl)) {
        const Token op = Advance();
        auto rng = std::make_unique<ExprNode>();
        rng->kind             = ExprKind::Range;
        rng->token            = op;
        rng->text             = std::string(op.lexeme);
        rng->isInclusiveRange = (op.kind == TokenKind::RangeIncl);
        rng->location         = op.location;
        rng->left             = std::move(left);
        rng->right            = ParseAdditive();
        return rng;
    }
    return left;
}

ExprPtr Parser::ParseAdditive() {
    ExprPtr left = ParseMultiplicative();
    while (Check(TokenKind::Plus) || Check(TokenKind::Minus)) {
        const Token op = Advance();
        auto bin = std::make_unique<ExprNode>();
        bin->kind     = ExprKind::Binary;
        bin->token    = op;
        bin->text     = std::string(op.lexeme);
        bin->location = op.location;
        bin->left     = std::move(left);
        bin->right    = ParseMultiplicative();
        left          = std::move(bin);
    }
    return left;
}

ExprPtr Parser::ParseMultiplicative() {
    ExprPtr left = ParseUnary();
    while (Check(TokenKind::Star) || Check(TokenKind::Slash) || Check(TokenKind::Percent)) {
        const Token op = Advance();
        auto bin = std::make_unique<ExprNode>();
        bin->kind     = ExprKind::Binary;
        bin->token    = op;
        bin->text     = std::string(op.lexeme);
        bin->location = op.location;
        bin->left     = std::move(left);
        bin->right    = ParseUnary();
        left          = std::move(bin);
    }
    return left;
}

ExprPtr Parser::ParseUnary() {
    if (Check(TokenKind::Bang) || Check(TokenKind::Minus) || Check(TokenKind::Plus) ||
        Check(TokenKind::Tilde) || Check(TokenKind::Star) || Check(TokenKind::Amp) ||
        Check(TokenKind::PlusPlus) || Check(TokenKind::MinusMinus) || Check(TokenKind::KwAwait)) {
        const Token op = Advance();
        auto un = std::make_unique<ExprNode>();
        un->kind     = ExprKind::Unary;
        un->token    = op;
        un->text     = std::string(op.lexeme);
        un->location = op.location;
        un->left     = ParseUnary();
        return un;
    }

    if (Match(TokenKind::KwNew)) {
        auto node = std::make_unique<ExprNode>();
        node->kind       = ExprKind::NewObject;
        node->location   = m_tokens[m_pos - 1].location;
        node->targetType = ParseType();
        node->text       = node->targetType.baseName;
        Expect(TokenKind::LParen, "P1052", "Expected '(' after 'new Type'");
        if (!Check(TokenKind::RParen)) {
            do {
                node->arguments.push_back(ParseExpression());
            } while (Match(TokenKind::Comma));
        }
        Expect(TokenKind::RParen, "P1053", "Expected ')' after constructor arguments");
        return node;
    }

    return ParsePostfix();
}

ExprPtr Parser::ParsePostfix() {
    ExprPtr expr = ParsePrimary();

    while (true) {
        if (Match(TokenKind::Dot) || Match(TokenKind::QuestionDot)) {
            const Token memberTok = Advance();
            auto mem = std::make_unique<ExprNode>();
            mem->kind     = ExprKind::MemberAccess;
            mem->token    = memberTok;
            mem->text     = std::string(memberTok.lexeme);
            mem->location = memberTok.location;
            mem->left     = std::move(expr);

            // Check for generic method call like `Memory.Allocate<Int>(10)`
            if (Check(TokenKind::Less) && Peek(2).kind == TokenKind::Greater && Peek(3).kind == TokenKind::LParen) {
                Advance(); // '<'
                mem->targetType = ParseType();
                Match(TokenKind::Greater);
            }
            expr = std::move(mem);
        } else if (Match(TokenKind::LParen)) {
            auto call = std::make_unique<ExprNode>();
            call->kind     = ExprKind::Call;
            call->location = expr->location;
            call->left     = std::move(expr);
            if (!Check(TokenKind::RParen)) {
                do {
                    if (Check(TokenKind::Identifier) && Peek().kind == TokenKind::Colon) {
                        std::string argName(Advance().lexeme);
                        Advance(); // ':'
                        call->namedFields.emplace_back(std::move(argName), ParseExpression());
                    } else {
                        call->arguments.push_back(ParseExpression());
                    }
                } while (Match(TokenKind::Comma));
            }
            Expect(TokenKind::RParen, "P1054", "Expected ')' after argument list");
            expr = std::move(call);
        } else if (Match(TokenKind::LBracket)) {
            auto idx = std::make_unique<ExprNode>();
            idx->kind     = ExprKind::Index;
            idx->location = expr->location;
            idx->left     = std::move(expr);
            idx->right    = ParseExpression();
            Expect(TokenKind::RBracket, "P1055", "Expected ']' after index expression");
            expr = std::move(idx);
        } else if (Match(TokenKind::PlusPlus) || Match(TokenKind::MinusMinus)) {
            const Token op = m_tokens[m_pos - 1];
            auto post = std::make_unique<ExprNode>();
            post->kind     = ExprKind::Unary;
            post->token    = op;
            post->text     = std::string(op.lexeme);
            post->location = op.location;
            post->left     = std::move(expr);
            expr           = std::move(post);
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::ParsePrimary() {
    const Token& tok = Current();

    if (Check(TokenKind::IntegerLiteral) || Check(TokenKind::FloatLiteral) ||
        Check(TokenKind::StringLiteral) || Check(TokenKind::InterpolatedString) ||
        Check(TokenKind::MultilineString) || Check(TokenKind::RegexLiteral) ||
        Check(TokenKind::CharLiteral) || Check(TokenKind::BoolLiteral) ||
        Check(TokenKind::NullLiteral)) {
        auto lit = std::make_unique<ExprNode>();
        lit->kind     = ExprKind::Literal;
        lit->token    = Advance();
        lit->text     = std::string(lit->token.lexeme);
        lit->location = lit->token.location;
        return lit;
    }

    if (Match(TokenKind::KwThis) || Match(TokenKind::KwBase)) {
        auto id = std::make_unique<ExprNode>();
        id->kind     = ExprKind::Identifier;
        id->token    = m_tokens[m_pos - 1];
        id->text     = std::string(id->token.lexeme);
        id->location = id->token.location;
        return id;
    }

    // `match expr { Pattern => expr, ... }` expression
    if (Match(TokenKind::KwMatch)) {
        auto mExpr = std::make_unique<ExprNode>();
        mExpr->kind     = ExprKind::MatchExpr;
        mExpr->location = tok.location;
        mExpr->left     = ParseExpression();
        Expect(TokenKind::LBrace, "P1045", "Expected '{' after match expression");
        while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
            MatchArmNode arm;
            arm.location    = Current().location;
            arm.patternName = ParseQualifiedName();
            if (Match(TokenKind::LParen)) {
                if (Check(TokenKind::Identifier)) {
                    arm.bindingVar = std::string(Advance().lexeme);
                }
                Match(TokenKind::RParen);
            }
            Expect(TokenKind::FatArrow, "P1046", "Expected '=>' in match arm");
            if (Check(TokenKind::LBrace)) {
                arm.bodyStmt = ParseBlock();
            } else {
                arm.bodyExpr = ParseExpression();
            }
            mExpr->matchArms.push_back(std::move(arm));
            Match(TokenKind::Comma);
            Match(TokenKind::Semicolon);
        }
        Expect(TokenKind::RBrace, "P1047", "Expected '}' to close match expression");
        return mExpr;
    }

    // `Function(msg) { ... }` anonymous function expression
    if (Match(TokenKind::KwFunction)) {
        auto lambda = std::make_unique<ExprNode>();
        lambda->kind     = ExprKind::Lambda;
        lambda->location = tok.location;
        if (Match(TokenKind::LParen)) {
            while (!IsAtEnd() && !Check(TokenKind::RParen)) {
                if (Check(TokenKind::Identifier)) {
                    lambda->lambdaParams.emplace_back(Advance().lexeme);
                } else {
                    Advance();
                }
                Match(TokenKind::Comma);
            }
            Match(TokenKind::RParen);
        }
        if (Check(TokenKind::LBrace)) {
            lambda->lambdaBody = ParseBlock();
        }
        return lambda;
    }

    if (Check(TokenKind::Identifier) || Check(TokenKind::KwInput)) {
        auto id = std::make_unique<ExprNode>();
        id->kind     = ExprKind::Identifier;
        id->token    = Advance();
        id->text     = std::string(id->token.lexeme);
        id->location = id->token.location;

        // Check for generic function call like `Input<Int>("Prompt")`
        if (Check(TokenKind::Less) && Peek(2).kind == TokenKind::Greater && Peek(3).kind == TokenKind::LParen) {
            Advance(); // '<'
            id->targetType = ParseType();
            Match(TokenKind::Greater);
        }
        return id;
    }

    // Array literal `[10, 20, 30]`
    if (Match(TokenKind::LBracket)) {
        auto arr = std::make_unique<ExprNode>();
        arr->kind     = ExprKind::ArrayLiteral;
        arr->location = tok.location;
        if (!Check(TokenKind::RBracket)) {
            do {
                if (Check(TokenKind::RBracket)) break;
                arr->arguments.push_back(ParseExpression());
            } while (Match(TokenKind::Comma));
        }
        Expect(TokenKind::RBracket, "P1056", "Expected ']' to close array literal");
        return arr;
    }

    // Structured object literal `{ Name: robot, Battery: 95 }` or `{ status = "Ready" }`
    if (Match(TokenKind::LBrace)) {
        auto obj = std::make_unique<ExprNode>();
        obj->kind     = ExprKind::ObjectLiteral;
        obj->location = tok.location;
        while (!IsAtEnd() && !Check(TokenKind::RBrace)) {
            std::string key(Expect(TokenKind::Identifier, "P1057", "Expected field name in object literal").lexeme);
            if (!Match(TokenKind::Colon)) {
                Expect(TokenKind::Equal, "P1058", "Expected ':' or '=' after object literal field name");
            }
            obj->namedFields.emplace_back(std::move(key), ParseExpression());
            Match(TokenKind::Comma);
            Match(TokenKind::Semicolon);
        }
        Expect(TokenKind::RBrace, "P1059", "Expected '}' to close object literal");
        return obj;
    }

    // Parenthesized expression or multi-param lambda `() => ...` / `(sum, n) => sum + n`
    if (Match(TokenKind::LParen)) {
        if (Match(TokenKind::RParen) && Match(TokenKind::FatArrow)) {
            auto lambda = std::make_unique<ExprNode>();
            lambda->kind     = ExprKind::Lambda;
            lambda->location = tok.location;
            if (Check(TokenKind::LBrace)) {
                lambda->lambdaBody = ParseBlock();
            } else {
                lambda->left = ParseExpression();
            }
            return lambda;
        }
        if (Check(TokenKind::Identifier) && Peek(1).kind == TokenKind::Comma &&
            Peek(2).kind == TokenKind::Identifier && Peek(3).kind == TokenKind::RParen &&
            Peek(4).kind == TokenKind::FatArrow) {
            auto lambda = std::make_unique<ExprNode>();
            lambda->kind     = ExprKind::Lambda;
            lambda->location = tok.location;
            lambda->lambdaParams.emplace_back(Advance().lexeme);
            Advance(); // ','
            lambda->lambdaParams.emplace_back(Advance().lexeme);
            Advance(); // ')'
            Advance(); // '=>'
            if (Check(TokenKind::LBrace)) {
                lambda->lambdaBody = ParseBlock();
            } else {
                lambda->left = ParseExpression();
            }
            return lambda;
        }
        ExprPtr inner = ParseExpression();
        Expect(TokenKind::RParen, "P1060", "Expected ')' after expression");
        return inner;
    }

    // Fallback error node
    m_diagnostics.ReportError(
        "P1000",
        "Unexpected token '" + std::string(tok.lexeme) + "' in expression",
        tok.location,
        "Provide a valid expression.");
    auto invalid = std::make_unique<ExprNode>();
    invalid->location = Advance().location;
    return invalid;
}

} // namespace Robix::Frontend
