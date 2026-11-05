#pragma once

#include "../AST/ASTNodes.hpp"
#include "../Lexer/Token.hpp"
#include "../Semantic/Diagnostics.hpp"

#include <string_view>
#include <vector>

namespace Robix::Frontend {

class Parser {
public:
    Parser(const std::vector<Token>& tokens,
           std::string_view fileName,
           DiagnosticBag& diagnostics) noexcept;

    [[nodiscard]] CompilationUnitAST ParseCompilationUnit();

private:
    [[nodiscard]] const Token& Current() const noexcept;
    [[nodiscard]] const Token& Peek(std::size_t offset = 1) const noexcept;
    [[nodiscard]] bool IsAtEnd() const noexcept;
    [[nodiscard]] bool Check(TokenKind kind) const noexcept;
    const Token& Advance() noexcept;
    bool Match(TokenKind kind) noexcept;
    const Token& Expect(TokenKind kind, const char* code, const char* errorMsg);
    void Synchronize() noexcept;

    [[nodiscard]] std::string ParseQualifiedName();
    [[nodiscard]] TypeNode ParseType();
    [[nodiscard]] std::vector<std::string> ParseOptionalGenericParams();
    [[nodiscard]] std::vector<ParameterNode> ParseParameterList();
    [[nodiscard]] ExplainBlockNode ParseExplainBlock();

    // Declarations
    [[nodiscard]] DeclPtr ParseTopLevelOrMemberDecl(std::string_view enclosingClassName = {});
    [[nodiscard]] DeclPtr ParseClassOrStructOrInterface(DeclKind kind,
                                                        Spec::AccessVisibility vis,
                                                        bool isAbstract,
                                                        bool isFinal,
                                                        std::vector<AttributeNode> attrs);
    [[nodiscard]] DeclPtr ParseEnumDecl(Spec::AccessVisibility vis, std::vector<AttributeNode> attrs);
    [[nodiscard]] DeclPtr ParseServiceOrImplementDecl(DeclKind kind,
                                                      Spec::AccessVisibility vis,
                                                      std::vector<AttributeNode> attrs);
    [[nodiscard]] DeclPtr ParseFunctionOrMethodDecl(Spec::AccessVisibility vis,
                                                    bool isStatic,
                                                    bool isAbstract,
                                                    bool isOverride,
                                                    bool isAsync,
                                                    bool isConsteval,
                                                    bool isExtern,
                                                    std::vector<AttributeNode> attrs,
                                                    TypeNode preParsedReturnType = {});
    [[nodiscard]] DeclPtr ParseTestDecl();
    [[nodiscard]] DeclPtr ParseTestFileDecl();

    // Statements
    [[nodiscard]] StmtPtr ParseStatement();
    [[nodiscard]] StmtPtr ParseBlock();
    [[nodiscard]] StmtPtr ParseVarOrConstDecl(bool expectSemicolon = true);
    [[nodiscard]] StmtPtr ParseIfStatement();
    [[nodiscard]] StmtPtr ParseForStatement();
    [[nodiscard]] StmtPtr ParseWhileStatement();
    [[nodiscard]] StmtPtr ParseDoWhileStatement();
    [[nodiscard]] StmtPtr ParseRepeatStatement();
    [[nodiscard]] StmtPtr ParseLoopStatement();
    [[nodiscard]] StmtPtr ParseMatchStatement();
    [[nodiscard]] StmtPtr ParseTryCatchStatement();

    // Expressions (Precedence Climbing)
    [[nodiscard]] ExprPtr ParseExpression();
    [[nodiscard]] ExprPtr ParseAssignment();
    [[nodiscard]] ExprPtr ParseTernary();
    [[nodiscard]] ExprPtr ParseNullCoalesce();
    [[nodiscard]] ExprPtr ParseLogicalOr();
    [[nodiscard]] ExprPtr ParseLogicalAnd();
    [[nodiscard]] ExprPtr ParseEquality();
    [[nodiscard]] ExprPtr ParseRelational();
    [[nodiscard]] ExprPtr ParseRange();
    [[nodiscard]] ExprPtr ParseAdditive();
    [[nodiscard]] ExprPtr ParseMultiplicative();
    [[nodiscard]] ExprPtr ParseUnary();
    [[nodiscard]] ExprPtr ParsePostfix();
    [[nodiscard]] ExprPtr ParsePrimary();

    std::vector<Token> m_tokens;
    std::string_view m_fileName;
    DiagnosticBag& m_diagnostics;
    std::size_t m_pos = 0;
};

} // namespace Robix::Frontend

