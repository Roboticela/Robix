#include "Lexer.hpp"
#include "../Types/SpecValidator.hpp"

#include <cctype>

namespace Robix::Frontend {

namespace {

bool IsIdentifierStart(char ch) noexcept {
    const auto u = static_cast<unsigned char>(ch);
    return std::isalpha(u) || ch == '_' || (u >= 0x80);
}

bool IsIdentifierContinue(char ch) noexcept {
    const auto u = static_cast<unsigned char>(ch);
    return std::isalnum(u) || ch == '_' || (u >= 0x80);
}

TokenKind ResolveKeyword(std::string_view text) noexcept {
    if (text == "package" || text == "Package") return TokenKind::KwPackage;
    if (text == "import" || text == "Import") return TokenKind::KwImport;
    if (text == "export" || text == "Export") return TokenKind::KwExport;
    if (text == "as") return TokenKind::KwAs;
    if (text == "public" || text == "Public") return TokenKind::KwPublic;
    if (text == "private" || text == "Private") return TokenKind::KwPrivate;
    if (text == "protected" || text == "Protected") return TokenKind::KwProtected;
    if (text == "internal" || text == "Internal") return TokenKind::KwInternal;
    if (text == "class" || text == "Class") return TokenKind::KwClass;
    if (text == "abstract" || text == "Abstract") return TokenKind::KwAbstract;
    if (text == "final" || text == "Final") return TokenKind::KwFinal;
    if (text == "partial" || text == "Partial") return TokenKind::KwPartial;
    if (text == "struct" || text == "Struct") return TokenKind::KwStruct;
    if (text == "enum" || text == "Enum") return TokenKind::KwEnum;
    if (text == "interface" || text == "Interface") return TokenKind::KwInterface;
    if (text == "trait" || text == "Trait") return TokenKind::KwTrait;
    if (text == "extends") return TokenKind::KwExtends;
    if (text == "implements") return TokenKind::KwImplements;
    if (text == "override") return TokenKind::KwOverride;
    if (text == "static") return TokenKind::KwStatic;
    if (text == "operator") return TokenKind::KwOperator;
    if (text == "function" || text == "Function") return TokenKind::KwFunction;
    if (text == "consteval") return TokenKind::KwConsteval;
    if (text == "Async" || text == "async") return TokenKind::KwAsync;
    if (text == "await") return TokenKind::KwAwait;
    if (text == "Constructor") return TokenKind::KwConstructor;
    if (text == "Destructor") return TokenKind::KwDestructor;
    if (text == "Base") return TokenKind::KwBase;
    if (text == "this") return TokenKind::KwThis;
    if (text == "new") return TokenKind::KwNew;
    if (text == "type") return TokenKind::KwType;
    if (text == "where") return TokenKind::KwWhere;
    if (text == "requires") return TokenKind::KwRequires;
    if (text == "is") return TokenKind::KwIs;
    if (text == "let") return TokenKind::KwLet;
    if (text == "var") return TokenKind::KwVar;
    if (text == "const") return TokenKind::KwConst;
    if (text == "Explain") return TokenKind::KwExplain;
    if (text == "Description") return TokenKind::KwDescription;
    if (text == "Input") return TokenKind::KwInput;
    if (text == "Processing") return TokenKind::KwProcessing;
    if (text == "Output") return TokenKind::KwOutput;
    if (text == "Errors") return TokenKind::KwErrors;
    if (text == "SideEffects") return TokenKind::KwSideEffects;
    if (text == "if") return TokenKind::KwIf;
    if (text == "else") return TokenKind::KwElse;
    if (text == "match") return TokenKind::KwMatch;
    if (text == "switch") return TokenKind::KwSwitch;
    if (text == "case") return TokenKind::KwCase;
    if (text == "default") return TokenKind::KwDefault;
    if (text == "for") return TokenKind::KwFor;
    if (text == "in") return TokenKind::KwIn;
    if (text == "while") return TokenKind::KwWhile;
    if (text == "do") return TokenKind::KwDo;
    if (text == "repeat") return TokenKind::KwRepeat;
    if (text == "loop") return TokenKind::KwLoop;
    if (text == "break") return TokenKind::KwBreak;
    if (text == "continue") return TokenKind::KwContinue;
    if (text == "return") return TokenKind::KwReturn;
    if (text == "yield") return TokenKind::KwYield;
    if (text == "goto") return TokenKind::KwGoto;
    if (text == "try") return TokenKind::KwTry;
    if (text == "catch") return TokenKind::KwCatch;
    if (text == "finally") return TokenKind::KwFinally;
    if (text == "throw") return TokenKind::KwThrow;
    if (text == "defer") return TokenKind::KwDefer;
    if (text == "using") return TokenKind::KwUsing;
    if (text == "Unsafe") return TokenKind::KwUnsafe;
    if (text == "ASM") return TokenKind::KwASM;
    if (text == "Extern" || text == "extern") return TokenKind::KwExtern;
    if (text == "Service") return TokenKind::KwService;
    if (text == "Implement") return TokenKind::KwImplement;
    if (text == "Connect") return TokenKind::KwConnect;
    if (text == "Permissions") return TokenKind::KwPermissions;
    if (text == "Test" || text == "test") return TokenKind::KwTest;
    if (text == "TestFile") return TokenKind::KwTestFile;
    if (text == "Covers") return TokenKind::KwCovers;
    if (text == "macro") return TokenKind::KwMacro;
    if (text == "true" || text == "false") return TokenKind::BoolLiteral;
    if (text == "null") return TokenKind::NullLiteral;
    return TokenKind::Identifier;
}

} // namespace

std::string_view TokenKindToString(TokenKind kind) noexcept {
    switch (kind) {
        case TokenKind::EndOfFile:          return "EOF";
        case TokenKind::Invalid:            return "Invalid";
        case TokenKind::Identifier:         return "Identifier";
        case TokenKind::IntegerLiteral:     return "IntegerLiteral";
        case TokenKind::FloatLiteral:       return "FloatLiteral";
        case TokenKind::StringLiteral:      return "StringLiteral";
        case TokenKind::InterpolatedString: return "InterpolatedString";
        case TokenKind::MultilineString:    return "MultilineString";
        case TokenKind::RegexLiteral:       return "RegexLiteral";
        case TokenKind::CharLiteral:        return "CharLiteral";
        case TokenKind::BoolLiteral:        return "BoolLiteral";
        case TokenKind::NullLiteral:        return "NullLiteral";
        default:                            return "Token";
    }
}

Lexer::Lexer(std::string_view source,
             std::string_view fileName,
             DiagnosticBag& diagnostics) noexcept
    : m_source(source), m_fileName(fileName), m_diagnostics(diagnostics) {}

bool Lexer::IsAtEnd(std::size_t lookahead) const noexcept {
    return (m_cursor + lookahead) >= m_source.size();
}

char Lexer::Peek(std::size_t lookahead) const noexcept {
    if (IsAtEnd(lookahead)) {
        return '\0';
    }
    return m_source[m_cursor + lookahead];
}

char Lexer::Advance() noexcept {
    if (IsAtEnd()) {
        return '\0';
    }
    const char ch = m_source[m_cursor++];
    if (ch == '\n') {
        ++m_line;
        m_column = 1;
    } else {
        ++m_column;
    }
    return ch;
}

bool Lexer::Match(char expected) noexcept {
    if (IsAtEnd() || m_source[m_cursor] != expected) {
        return false;
    }
    Advance();
    return true;
}

Token Lexer::MakeToken(TokenKind kind,
                       std::size_t startOffset,
                       std::uint32_t startLine,
                       std::uint32_t startColumn) const noexcept {
    const std::size_t length = m_cursor - startOffset;
    return Token{
        kind,
        m_source.substr(startOffset, length),
        SourceLocation{
            m_fileName,
            startLine,
            startColumn,
            static_cast<std::uint32_t>(startOffset),
            static_cast<std::uint32_t>(length > 0 ? length : 1),
        },
    };
}

void Lexer::SkipWhitespaceAndComments() {
    while (!IsAtEnd()) {
        const char c = Peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
            Advance();
        } else if (c == '/' && Peek(1) == '/') {
            while (!IsAtEnd() && Peek() != '\n') {
                Advance();
            }
        } else if (c == '/' && Peek(1) == '*') {
            const std::uint32_t startLine = m_line;
            const std::uint32_t startCol  = m_column;
            const std::size_t startOff    = m_cursor;
            Advance();
            Advance();
            bool closed = false;
            while (!IsAtEnd()) {
                if (Peek() == '*' && Peek(1) == '/') {
                    Advance();
                    Advance();
                    closed = true;
                    break;
                }
                Advance();
            }
            if (!closed) {
                m_diagnostics.ReportError(
                    "L0002",
                    "Unterminated multi-line comment",
                    SourceLocation{m_fileName, startLine, startCol, static_cast<std::uint32_t>(startOff), 2},
                    "Add closing '*/' to terminate the comment.");
            }
        } else {
            break;
        }
    }
}

Token Lexer::LexIdentifierOrKeyword(std::size_t startOffset,
                                    std::uint32_t startLine,
                                    std::uint32_t startColumn) {
    // Check for r"..." regex literal
    if (m_source[startOffset] == 'r' && Peek() == '"') {
        Advance(); // consume opening '"'
        return LexString(startOffset, startLine, startColumn, TokenKind::RegexLiteral);
    }

    while (!IsAtEnd() && IsIdentifierContinue(Peek())) {
        Advance();
    }
    const std::string_view text = m_source.substr(startOffset, m_cursor - startOffset);
    return MakeToken(ResolveKeyword(text), startOffset, startLine, startColumn);
}

Token Lexer::LexNumber(std::size_t startOffset,
                       std::uint32_t startLine,
                       std::uint32_t startColumn) {
    bool isFloat = false;

    if (m_source[startOffset] == '0' &&
        (Peek() == 'x' || Peek() == 'X' || Peek() == 'b' || Peek() == 'B' || Peek() == 'o' || Peek() == 'O')) {
        Advance();
        while (!IsAtEnd() && (std::isxdigit(static_cast<unsigned char>(Peek())) || Peek() == '_')) {
            Advance();
        }
    } else {
        while (!IsAtEnd() && (std::isdigit(static_cast<unsigned char>(Peek())) || Peek() == '_')) {
            Advance();
        }
        // Check for fractional part (avoid consuming range '..' or '..=' or method like '10.Milliseconds')
        if (Peek() == '.' && std::isdigit(static_cast<unsigned char>(Peek(1)))) {
            isFloat = true;
            Advance(); // consume '.'
            while (!IsAtEnd() && (std::isdigit(static_cast<unsigned char>(Peek())) || Peek() == '_')) {
                Advance();
            }
        }
        if (Peek() == 'e' || Peek() == 'E') {
            isFloat = true;
            Advance();
            if (Peek() == '+' || Peek() == '-') {
                Advance();
            }
            while (!IsAtEnd() && (std::isdigit(static_cast<unsigned char>(Peek())) || Peek() == '_')) {
                Advance();
            }
        }
    }

    // Optional numeric type suffix (i8, i16, i32, i64, u8, u16, u32, u64, f16, f32, f64, d)
    if ((Peek() == 'i' || Peek() == 'u' || Peek() == 'f' || Peek() == 'd') &&
        (std::isdigit(static_cast<unsigned char>(Peek(1))) || Peek() == 'd')) {
        if (Peek() == 'f' || Peek() == 'd') {
            isFloat = true;
        }
        Advance();
        while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) {
            Advance();
        }
    }

    return MakeToken(isFloat ? TokenKind::FloatLiteral : TokenKind::IntegerLiteral,
                     startOffset,
                     startLine,
                     startColumn);
}

Token Lexer::LexMultilineString(std::size_t startOffset,
                                std::uint32_t startLine,
                                std::uint32_t startColumn) {
    bool closed = false;
    while (!IsAtEnd()) {
        if (Peek() == '"' && Peek(1) == '"' && Peek(2) == '"') {
            Advance();
            Advance();
            Advance();
            closed = true;
            break;
        }
        Advance();
    }
    if (!closed) {
        m_diagnostics.ReportError(
            "L0003",
            "Unterminated multiline string literal",
            SourceLocation{m_fileName, startLine, startColumn, static_cast<std::uint32_t>(startOffset), 3},
            "Close the multiline string with '\"\"\"'.");
    }
    return MakeToken(TokenKind::MultilineString, startOffset, startLine, startColumn);
}

Token Lexer::LexString(std::size_t startOffset,
                       std::uint32_t startLine,
                       std::uint32_t startColumn,
                       TokenKind kind) {
    bool closed = false;
    while (!IsAtEnd()) {
        const char c = Peek();
        if (c == '\n') {
            break;
        }
        if (c == '\\') {
            Advance();
            if (!IsAtEnd()) {
                Advance();
            }
            continue;
        }
        if (c == '"') {
            Advance();
            closed = true;
            break;
        }
        Advance();
    }
    if (!closed) {
        m_diagnostics.ReportError(
            "L0004",
            "Unterminated string literal",
            SourceLocation{m_fileName, startLine, startColumn, static_cast<std::uint32_t>(startOffset), 1},
            "Add a closing '\"' before the end of the line.");
    }
    return MakeToken(kind, startOffset, startLine, startColumn);
}

Token Lexer::LexChar(std::size_t startOffset,
                    std::uint32_t startLine,
                    std::uint32_t startColumn) {
    if (Peek() == '\\') {
        Advance();
        if (!IsAtEnd()) {
            Advance();
        }
    } else if (!IsAtEnd() && Peek() != '\'') {
        Advance();
    }
    if (!Match('\'')) {
        m_diagnostics.ReportError(
            "L0005",
            "Unterminated character literal",
            SourceLocation{m_fileName, startLine, startColumn, static_cast<std::uint32_t>(startOffset), 1},
            "Close character literal with single quote \"'\".");
    }
    return MakeToken(TokenKind::CharLiteral, startOffset, startLine, startColumn);
}

std::vector<Token> Lexer::TokenizeAll() {
    if (Spec::ContainsForbiddenBidiControlChars(m_source)) {
        m_diagnostics.ReportError(
            "S1001",
            "Forbidden Unicode bidirectional override control character detected",
            SourceLocation{m_fileName, 1, 1, 0, 1},
            "Remove hidden Unicode bidirectional control characters (Trojan Source protection).");
    }

    std::vector<Token> tokens;
    tokens.reserve(m_source.size() / 4 + 8);

    while (true) {
        SkipWhitespaceAndComments();
        const std::size_t startOffset = m_cursor;
        const std::uint32_t startLine = m_line;
        const std::uint32_t startCol  = m_column;

        if (IsAtEnd()) {
            tokens.push_back(MakeToken(TokenKind::EndOfFile, startOffset, startLine, startCol));
            break;
        }

        const char c = Advance();

        if (c == '$' && Peek() == '"') {
            Advance(); // consume '"'
            tokens.push_back(LexString(startOffset, startLine, startCol, TokenKind::InterpolatedString));
            continue;
        }

        if (IsIdentifierStart(c)) {
            tokens.push_back(LexIdentifierOrKeyword(startOffset, startLine, startCol));
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(LexNumber(startOffset, startLine, startCol));
            continue;
        }

        switch (c) {
            case '"':
                if (Peek() == '"' && Peek(1) == '"') {
                    Advance();
                    Advance();
                    tokens.push_back(LexMultilineString(startOffset, startLine, startCol));
                } else {
                    tokens.push_back(LexString(startOffset, startLine, startCol, TokenKind::StringLiteral));
                }
                break;
            case '\'':
                tokens.push_back(LexChar(startOffset, startLine, startCol));
                break;
            case '(': tokens.push_back(MakeToken(TokenKind::LParen, startOffset, startLine, startCol)); break;
            case ')': tokens.push_back(MakeToken(TokenKind::RParen, startOffset, startLine, startCol)); break;
            case '{': tokens.push_back(MakeToken(TokenKind::LBrace, startOffset, startLine, startCol)); break;
            case '}': tokens.push_back(MakeToken(TokenKind::RBrace, startOffset, startLine, startCol)); break;
            case '[': tokens.push_back(MakeToken(TokenKind::LBracket, startOffset, startLine, startCol)); break;
            case ']': tokens.push_back(MakeToken(TokenKind::RBracket, startOffset, startLine, startCol)); break;
            case ';': tokens.push_back(MakeToken(TokenKind::Semicolon, startOffset, startLine, startCol)); break;
            case ':': tokens.push_back(MakeToken(TokenKind::Colon, startOffset, startLine, startCol)); break;
            case ',': tokens.push_back(MakeToken(TokenKind::Comma, startOffset, startLine, startCol)); break;
            case '@': tokens.push_back(MakeToken(TokenKind::At, startOffset, startLine, startCol)); break;
            case '~': tokens.push_back(MakeToken(TokenKind::Tilde, startOffset, startLine, startCol)); break;
            case '^': tokens.push_back(MakeToken(TokenKind::Caret, startOffset, startLine, startCol)); break;
            case '.':
                if (Match('.')) {
                    if (Match('.')) {
                        tokens.push_back(MakeToken(TokenKind::Ellipsis, startOffset, startLine, startCol));
                    } else if (Match('=')) {
                        tokens.push_back(MakeToken(TokenKind::RangeIncl, startOffset, startLine, startCol));
                    } else {
                        tokens.push_back(MakeToken(TokenKind::RangeExcl, startOffset, startLine, startCol));
                    }
                } else {
                    tokens.push_back(MakeToken(TokenKind::Dot, startOffset, startLine, startCol));
                }
                break;
            case '?':
                if (Match('.')) {
                    tokens.push_back(MakeToken(TokenKind::QuestionDot, startOffset, startLine, startCol));
                } else if (Match('?')) {
                    tokens.push_back(MakeToken(TokenKind::QuestionQuestion, startOffset, startLine, startCol));
                } else {
                    tokens.push_back(MakeToken(TokenKind::Question, startOffset, startLine, startCol));
                }
                break;
            case '+':
                if (Match('+')) {
                    tokens.push_back(MakeToken(TokenKind::PlusPlus, startOffset, startLine, startCol));
                } else if (Match('=')) {
                    tokens.push_back(MakeToken(TokenKind::PlusEqual, startOffset, startLine, startCol));
                } else {
                    tokens.push_back(MakeToken(TokenKind::Plus, startOffset, startLine, startCol));
                }
                break;
            case '-':
                if (Match('>')) {
                    tokens.push_back(MakeToken(TokenKind::Arrow, startOffset, startLine, startCol));
                } else if (Match('-')) {
                    tokens.push_back(MakeToken(TokenKind::MinusMinus, startOffset, startLine, startCol));
                } else if (Match('=')) {
                    tokens.push_back(MakeToken(TokenKind::MinusEqual, startOffset, startLine, startCol));
                } else {
                    tokens.push_back(MakeToken(TokenKind::Minus, startOffset, startLine, startCol));
                }
                break;
            case '*':
                tokens.push_back(MakeToken(Match('=') ? TokenKind::StarEqual : TokenKind::Star, startOffset, startLine, startCol));
                break;
            case '/':
                tokens.push_back(MakeToken(Match('=') ? TokenKind::SlashEqual : TokenKind::Slash, startOffset, startLine, startCol));
                break;
            case '%':
                tokens.push_back(MakeToken(Match('=') ? TokenKind::PercentEqual : TokenKind::Percent, startOffset, startLine, startCol));
                break;
            case '=':
                if (Match('=')) {
                    tokens.push_back(MakeToken(TokenKind::EqualEqual, startOffset, startLine, startCol));
                } else if (Match('>')) {
                    tokens.push_back(MakeToken(TokenKind::FatArrow, startOffset, startLine, startCol));
                } else {
                    tokens.push_back(MakeToken(TokenKind::Equal, startOffset, startLine, startCol));
                }
                break;
            case '!':
                tokens.push_back(MakeToken(Match('=') ? TokenKind::BangEqual : TokenKind::Bang, startOffset, startLine, startCol));
                break;
            case '<':
                if (Match('<')) {
                    tokens.push_back(MakeToken(TokenKind::ShiftLeft, startOffset, startLine, startCol));
                } else if (Match('=')) {
                    tokens.push_back(MakeToken(TokenKind::LessEqual, startOffset, startLine, startCol));
                } else {
                    tokens.push_back(MakeToken(TokenKind::Less, startOffset, startLine, startCol));
                }
                break;
            case '>':
                if (Match('>')) {
                    tokens.push_back(MakeToken(TokenKind::ShiftRight, startOffset, startLine, startCol));
                } else if (Match('=')) {
                    tokens.push_back(MakeToken(TokenKind::GreaterEqual, startOffset, startLine, startCol));
                } else {
                    tokens.push_back(MakeToken(TokenKind::Greater, startOffset, startLine, startCol));
                }
                break;
            case '&':
                tokens.push_back(MakeToken(Match('&') ? TokenKind::AmpAmp : TokenKind::Amp, startOffset, startLine, startCol));
                break;
            case '|':
                tokens.push_back(MakeToken(Match('|') ? TokenKind::PipePipe : TokenKind::Pipe, startOffset, startLine, startCol));
                break;
            default:
                m_diagnostics.ReportError(
                    "L0006",
                    std::string("Unexpected character '") + c + "'",
                    SourceLocation{m_fileName, startLine, startCol, static_cast<std::uint32_t>(startOffset), 1},
                    "Remove or replace the invalid character.");
                tokens.push_back(MakeToken(TokenKind::Invalid, startOffset, startLine, startCol));
                break;
        }
    }

    return tokens;
}

} // namespace Robix::Frontend
