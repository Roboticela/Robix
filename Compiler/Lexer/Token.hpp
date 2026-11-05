#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Robix::Frontend {

struct SourceLocation {
    std::string_view fileName = "<memory>";
    std::uint32_t line        = 1;
    std::uint32_t column      = 1;
    std::uint32_t byteOffset  = 0;
    std::uint32_t byteLength  = 1;
};

enum class TokenKind : std::uint16_t {
    EndOfFile = 0,
    Invalid,

    // Identifiers & Literals
    Identifier,
    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    InterpolatedString,
    MultilineString,
    RegexLiteral,
    CharLiteral,
    BoolLiteral,
    NullLiteral,

    // Keywords: Modules & Visibility
    KwPackage,
    KwImport,
    KwExport,
    KwAs,
    KwPublic,
    KwPrivate,
    KwProtected,
    KwInternal,

    // Keywords: OOP, Types & Declarations
    KwClass,
    KwAbstract,
    KwFinal,
    KwPartial,
    KwStruct,
    KwEnum,
    KwInterface,
    KwTrait,
    KwExtends,
    KwImplements,
    KwOverride,
    KwStatic,
    KwOperator,
    KwFunction,
    KwConsteval,
    KwAsync,
    KwAwait,
    KwConstructor,
    KwDestructor,
    KwBase,
    KwThis,
    KwNew,
    KwType,
    KwWhere,
    KwRequires,
    KwIs,

    // Keywords: Variables & Constants
    KwLet,
    KwVar,
    KwConst,

    // Keywords: First-Class Explain Documentation Block
    KwExplain,
    KwDescription,
    KwInput,
    KwProcessing,
    KwOutput,
    KwErrors,
    KwSideEffects,

    // Keywords: Control Flow & Loops
    KwIf,
    KwElse,
    KwMatch,
    KwSwitch,
    KwCase,
    KwDefault,
    KwFor,
    KwIn,
    KwWhile,
    KwDo,
    KwRepeat,
    KwLoop,
    KwBreak,
    KwContinue,
    KwReturn,
    KwYield,
    KwGoto,

    // Keywords: Errors, Resource Management & Systems
    KwTry,
    KwCatch,
    KwFinally,
    KwThrow,
    KwDefer,
    KwUsing,
    KwUnsafe,
    KwASM,
    KwExtern,

    // Keywords: Network, Testing & Macros
    KwService,
    KwImplement,
    KwConnect,
    KwPermissions,
    KwTest,
    KwTestFile,
    KwCovers,
    KwMacro,

    // Punctuation & Delimiters
    LParen,         // (
    RParen,         // )
    LBrace,         // {
    RBrace,         // }
    LBracket,       // [
    RBracket,       // ]
    Semicolon,      // ;
    Colon,          // :
    Comma,          // ,
    Dot,            // .
    QuestionDot,    // ?.
    RangeExcl,      // ..
    RangeIncl,      // ..=
    Ellipsis,       // ...
    At,             // @
    Question,       // ?
    QuestionQuestion, // ??
    Arrow,          // ->
    FatArrow,       // =>

    // Operators
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Percent,        // %
    PlusPlus,       // ++
    MinusMinus,     // --
    Equal,          // =
    PlusEqual,      // +=
    MinusEqual,     // -=
    StarEqual,      // *=
    SlashEqual,     // /=
    PercentEqual,   // %=
    EqualEqual,     // ==
    BangEqual,      // !=
    Less,           // <
    LessEqual,      // <=
    Greater,        // >
    GreaterEqual,   // >=
    AmpAmp,         // &&
    PipePipe,       // ||
    Bang,           // !
    Amp,            // &
    Pipe,           // |
    Caret,          // ^
    Tilde,          // ~
    ShiftLeft,      // <<
    ShiftRight      // >>
};

struct Token {
    TokenKind kind = TokenKind::EndOfFile;
    std::string_view lexeme{};
    SourceLocation location{};
};

std::string_view TokenKindToString(TokenKind kind) noexcept;

} // namespace Robix::Frontend

