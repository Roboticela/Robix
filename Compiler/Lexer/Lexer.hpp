#pragma once

#include "../Semantic/Diagnostics.hpp"
#include "Token.hpp"

#include <string_view>
#include <vector>

namespace Robix::Frontend {

class Lexer {
public:
    Lexer(std::string_view source,
          std::string_view fileName,
          DiagnosticBag& diagnostics) noexcept;

    [[nodiscard]] std::vector<Token> TokenizeAll();

private:
    [[nodiscard]] bool IsAtEnd(std::size_t lookahead = 0) const noexcept;
    [[nodiscard]] char Peek(std::size_t lookahead = 0) const noexcept;
    char Advance() noexcept;
    bool Match(char expected) noexcept;
    void SkipWhitespaceAndComments();

    [[nodiscard]] Token MakeToken(TokenKind kind,
                                  std::size_t startOffset,
                                  std::uint32_t startLine,
                                  std::uint32_t startColumn) const noexcept;

    [[nodiscard]] Token LexIdentifierOrKeyword(std::size_t startOffset,
                                               std::uint32_t startLine,
                                               std::uint32_t startColumn);
    [[nodiscard]] Token LexNumber(std::size_t startOffset,
                                  std::uint32_t startLine,
                                  std::uint32_t startColumn);
    [[nodiscard]] Token LexString(std::size_t startOffset,
                                  std::uint32_t startLine,
                                  std::uint32_t startColumn,
                                  TokenKind kind);
    [[nodiscard]] Token LexMultilineString(std::size_t startOffset,
                                           std::uint32_t startLine,
                                           std::uint32_t startColumn);
    [[nodiscard]] Token LexChar(std::size_t startOffset,
                                std::uint32_t startLine,
                                std::uint32_t startColumn);

    std::string_view m_source;
    std::string_view m_fileName;
    DiagnosticBag& m_diagnostics;
    std::size_t m_cursor    = 0;
    std::uint32_t m_line    = 1;
    std::uint32_t m_column  = 1;
};

} // namespace Robix::Frontend

