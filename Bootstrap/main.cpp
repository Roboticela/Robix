// Robix bootstrap compiler.
// Transpiles this repository's compiler sources to C++ and asks clang or g++
// to produce a native binary for a Linux or Windows architecture.
// When a GitHub release already contains RobixBootstrap, the build uses that
// binary instead of compiling this file again.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/utsname.h>
#endif

namespace fs = std::filesystem;

namespace {

[[noreturn]] void Die(const std::string& message) {
    std::cerr << "error: " << message << std::endl;
    std::exit(1);
}

struct Target {
    const char* name;
    const char* triple;
    const char* os;
    const char* arch;
};

const Target kTargets[] = {
    {"linux-x64", "x86_64-unknown-linux-gnu", "linux", "x64"},
    {"linux-arm64", "aarch64-unknown-linux-gnu", "linux", "arm64"},
    {"linux-arm", "arm-unknown-linux-gnueabihf", "linux", "arm"},
    {"linux-riscv64", "riscv64-unknown-linux-gnu", "linux", "riscv64"},
    {"linux-ppc64le", "powerpc64le-unknown-linux-gnu", "linux", "ppc64le"},
    {"linux-s390x", "s390x-unknown-linux-gnu", "linux", "s390x"},
    {"linux-loongarch64", "loongarch64-unknown-linux-gnu", "linux", "loongarch64"},
    {"windows-x64", "x86_64-w64-windows-gnu", "windows", "x64"},
    {"windows-arm64", "aarch64-w64-windows-gnu", "windows", "arm64"},
    {"windows-x86", "i686-w64-windows-gnu", "windows", "x86"},
};

const Target* FindTarget(const std::string& name) {
    for (const Target& target : kTargets) {
        if (name == target.name) return &target;
    }
    return nullptr;
}

std::string HostTargetName() {
#ifdef _WIN32
#if defined(_M_ARM64) || defined(__aarch64__)
    return "windows-arm64";
#elif defined(_M_IX86) || defined(__i386__)
    return "windows-x86";
#else
    return "windows-x64";
#endif
#else
    utsname info{};
    if (uname(&info) != 0) return "linux-x64";
    std::string machine = info.machine;
    if (machine == "x86_64" || machine == "amd64") return "linux-x64";
    if (machine == "aarch64" || machine == "arm64") return "linux-arm64";
    if (machine == "armv7l" || machine == "armv6l" || machine == "arm") return "linux-arm";
    if (machine == "riscv64") return "linux-riscv64";
    if (machine == "ppc64le") return "linux-ppc64le";
    if (machine == "s390x") return "linux-s390x";
    if (machine == "loongarch64") return "linux-loongarch64";
    return "linux-x64";
#endif
}

std::string FindOnPath(const std::string& name) {
    const char* path = std::getenv("PATH");
    if (path == nullptr) return "";
    std::stringstream stream(path);
    std::string dir;
    char separator = ':';
#ifdef _WIN32
    separator = ';';
#endif
    while (std::getline(stream, dir, separator)) {
        if (dir.empty()) continue;
        fs::path candidate = fs::path(dir) / name;
        std::error_code error;
        if (fs::is_regular_file(candidate, error)) return candidate.string();
    }
    return "";
}

bool IsClang(const std::string& compiler) {
    return compiler.find("clang") != std::string::npos;
}

std::string ShellQuote(const std::string& text) {
    std::string quoted = "'";
    for (char character : text) {
        if (character == '\'') quoted += "'\\''";
        else quoted += character;
    }
    quoted += "'";
    return quoted;
}

const std::set<std::string> kCppKeywords = {
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break", "case", "catch",
    "char", "char8_t", "char16_t", "char32_t", "class", "compl", "concept", "const", "consteval", "constexpr",
    "constinit", "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype", "default", "delete",
    "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float", "for",
    "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "not", "not_eq",
    "nullptr", "operator", "or", "or_eq", "private", "protected", "public", "register", "reinterpret_cast",
    "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct",
    "switch", "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename",
    "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"};

std::string Safe(const std::string& name) {
    if (kCppKeywords.count(name) || name == "rx") return name + "_";
    return name;
}

enum class Tok {
    End, Ident, Int, Float, String, Char, True, False, Null, This, If, Else, While, For, In, Return, Break,
    Continue, Var, Function, Class, Struct, Enum, Constructor, Destructor, Explain, Import, Package, Extern,
    Not, And, Or, Public, Private, Protected, Modifier, LParen, RParen, LBrace, RBrace, LBrack, RBrack, Comma,
    Semi, Dot, Colon, Arrow, Question, Assign, Eq, Ne, Lt, Gt, Le, Ge, Plus, Minus, Star, Slash, Percent,
    AndAnd, OrOr, Bang, Amp, Pipe, Caret
};

struct Token {
    Tok kind = Tok::End;
    std::string text;
    int64_t integer = 0;
    double number = 0;
    int line = 1;
    int column = 1;
};

struct Expr {
    enum class Kind { None, Int, Float, String, Char, Bool, Null, Name, This, Unary, Binary, Call, Member, Index, ListMake } kind = Kind::None;
    std::string text;
    std::string typeSpelling;
    int64_t integer = 0;
    double number = 0;
    std::vector<Expr> kids;
    int line = 1;
};

struct Stmt {
    enum class Kind { Block, If, While, ForIn, Return, Break, Continue, Decl, ExprStmt, Assign } kind = Kind::Block;
    std::vector<Stmt> body;
    std::vector<Stmt> elseOne;
    std::string name;
    std::string typeSpelling;
    Expr expr;
    Expr expr2;
    bool hasExpr = false;
    bool hasExpr2 = false;
    bool inferred = false;
    int line = 1;
};

struct Param {
    std::string name;
    std::string typeSpelling;
};

struct Method {
    std::string name;
    std::vector<Param> params;
    std::string returnSpelling;
    Stmt body;
    bool isExtern = false;
    bool isConstructor = false;
    int line = 1;
};

struct Field {
    std::string name;
    std::string typeSpelling;
};

struct ClassInfo {
    std::string name;
    std::vector<Field> fields;
    std::vector<Method> methods;
};

struct FuncInfo {
    std::string name;
    std::vector<Param> params;
    std::string returnSpelling;
    Stmt body;
    bool isExtern = false;
    int line = 1;
    std::string file;
};

struct EnumInfo {
    std::string name;
    std::vector<std::pair<std::string, int64_t>> members;
};

struct Type {
    enum class Kind { Int, Bool, Float, Char, String, Void, Exec, Enum, Object, List, Invalid } kind = Kind::Invalid;
    std::string name;
    std::shared_ptr<Type> elem;
};

struct Typed {
    Type type;
    std::string code;
    std::string lvalue;
};

class Program {
public:
    std::map<std::string, ClassInfo> classes;
    std::map<std::string, EnumInfo> enums;
    std::vector<FuncInfo> functions;
    std::set<std::string> externs;
};

class Lexer {
public:
    explicit Lexer(std::string source, std::string file) : source_(std::move(source)), file_(std::move(file)) {}

    std::vector<Token> Tokenize() {
        std::vector<Token> tokens;
        while (!Done()) {
            SkipTrivia();
            if (Done()) break;
            tokens.push_back(Read());
        }
        Token end;
        end.kind = Tok::End;
        end.line = line_;
        end.column = column_;
        tokens.push_back(end);
        return tokens;
    }

private:
    std::string source_;
    std::string file_;
    size_t index_ = 0;
    int line_ = 1;
    int column_ = 1;

    [[noreturn]] void Bad(const std::string& message) const {
        Die(file_ + ":" + std::to_string(line_) + ":" + std::to_string(column_) + ": " + message);
    }

    bool Done() const { return index_ >= source_.size(); }
    char Peek(size_t offset = 0) const {
        size_t at = index_ + offset;
        return at < source_.size() ? source_[at] : '\0';
    }
    char Take() {
        if (Done()) return '\0';
        char character = source_[index_++];
        if (character == '\n') {
            line_++;
            column_ = 1;
        } else {
            column_++;
        }
        return character;
    }

    void SkipTrivia() {
        while (!Done()) {
            char character = Peek();
            if (character == ' ' || character == '\t' || character == '\r' || character == '\n') {
                Take();
                continue;
            }
            if (character == '/' && Peek(1) == '/') {
                while (!Done() && Peek() != '\n') Take();
                continue;
            }
            if (character == '/' && Peek(1) == '*') {
                Take();
                Take();
                while (!Done() && !(Peek() == '*' && Peek(1) == '/')) Take();
                if (Done()) Bad("unterminated block comment");
                Take();
                Take();
                continue;
            }
            break;
        }
    }

    Token Here(Tok kind) const {
        Token token;
        token.kind = kind;
        token.line = line_;
        token.column = column_;
        return token;
    }

    static Tok Keyword(const std::string& text) {
        static const std::map<std::string, Tok> words = {
            {"True", Tok::True}, {"False", Tok::False}, {"Null", Tok::Null}, {"This", Tok::This}, {"If", Tok::If},
            {"Else", Tok::Else}, {"While", Tok::While}, {"For", Tok::For}, {"In", Tok::In}, {"Return", Tok::Return},
            {"Break", Tok::Break}, {"Continue", Tok::Continue}, {"Var", Tok::Var}, {"Function", Tok::Function},
            {"Class", Tok::Class}, {"Struct", Tok::Struct}, {"Enum", Tok::Enum}, {"Constructor", Tok::Constructor},
            {"Destructor", Tok::Destructor}, {"Explain", Tok::Explain}, {"Import", Tok::Import}, {"Package", Tok::Package},
            {"Extern", Tok::Extern}, {"Not", Tok::Not}, {"And", Tok::And}, {"Or", Tok::Or}, {"Public", Tok::Public},
            {"Private", Tok::Private}, {"Protected", Tok::Protected}, {"Internal", Tok::Modifier}, {"Partial", Tok::Modifier},
            {"Static", Tok::Modifier}, {"Final", Tok::Modifier}, {"Override", Tok::Modifier}, {"Virtual", Tok::Modifier},
            {"Abstract", Tok::Modifier}, {"Sealed", Tok::Modifier}, {"Async", Tok::Modifier}, {"Unsafe", Tok::Modifier}};
        auto found = words.find(text);
        return found == words.end() ? Tok::Ident : found->second;
    }

    Token Read() {
        char character = Peek();
        if (std::isalpha(static_cast<unsigned char>(character)) || character == '_') return ReadIdent();
        if (std::isdigit(static_cast<unsigned char>(character))) return ReadNumber();
        if (character == '"') return ReadString();
        if (character == '\'') return ReadChar();
        int line = line_;
        int column = column_;
        auto mark = [&](Tok kind) {
            Token token = Here(kind);
            token.line = line;
            token.column = column;
            return token;
        };
        Take();
        auto two = [&](char next, Tok kind, Tok one) {
            if (Peek() == next) {
                Take();
                return mark(kind);
            }
            return mark(one);
        };
        switch (character) {
            case '(': return mark(Tok::LParen);
            case ')': return mark(Tok::RParen);
            case '{': return mark(Tok::LBrace);
            case '}': return mark(Tok::RBrace);
            case '[': return mark(Tok::LBrack);
            case ']': return mark(Tok::RBrack);
            case ',': return mark(Tok::Comma);
            case ';': return mark(Tok::Semi);
            case ':': return mark(Tok::Colon);
            case '?': return mark(Tok::Question);
            case '^': return mark(Tok::Caret);
            case '.':
                if (Peek() == '.' && Peek(1) == '.') {
                    Take();
                    Take();
                    Bad("spread is not used by the bootstrap sources");
                }
                if (Peek() == '.' && Peek(1) == '=') {
                    Take();
                    Take();
                    Bad("ranges are not used by the bootstrap sources");
                }
                if (Peek() == '.') {
                    Take();
                    Bad("ranges are not used by the bootstrap sources");
                }
                return mark(Tok::Dot);
            case '-':
                if (Peek() == '>') {
                    Take();
                    return mark(Tok::Arrow);
                }
                return mark(Tok::Minus);
            case '+': return mark(Tok::Plus);
            case '*': return mark(Tok::Star);
            case '/': return mark(Tok::Slash);
            case '%': return mark(Tok::Percent);
            case '!': return two('=', Tok::Ne, Tok::Bang);
            case '=': return two('=', Tok::Eq, Tok::Assign);
            case '<': return two('=', Tok::Le, Tok::Lt);
            case '>': return two('=', Tok::Ge, Tok::Gt);
            case '&': return two('&', Tok::AndAnd, Tok::Amp);
            case '|': return two('|', Tok::OrOr, Tok::Pipe);
            default: Bad(std::string("unexpected character '") + character + "'");
        }
    }

    Token ReadIdent() {
        Token token = Here(Tok::Ident);
        while (std::isalnum(static_cast<unsigned char>(Peek())) || Peek() == '_') token.text.push_back(Take());
        token.kind = Keyword(token.text);
        return token;
    }

    Token ReadNumber() {
        Token token = Here(Tok::Int);
        std::string spelling;
        int base = 10;
        if (Peek() == '0' && (Peek(1) == 'x' || Peek(1) == 'X' || Peek(1) == 'b' || Peek(1) == 'B' || Peek(1) == 'o' || Peek(1) == 'O')) {
            Take();
            char marker = Take();
            if (marker == 'x' || marker == 'X') base = 16;
            else if (marker == 'b' || marker == 'B') base = 2;
            else base = 8;
            while (std::isxdigit(static_cast<unsigned char>(Peek())) || Peek() == '_') {
                if (Peek() != '_') spelling.push_back(Take());
                else Take();
            }
            token.integer = std::stoll(spelling.empty() ? "0" : spelling, nullptr, base);
            return token;
        }
        while (std::isdigit(static_cast<unsigned char>(Peek())) || Peek() == '_') {
            if (Peek() != '_') spelling.push_back(Take());
            else Take();
        }
        if (Peek() == '.' && std::isdigit(static_cast<unsigned char>(Peek(1)))) {
            spelling.push_back(Take());
            while (std::isdigit(static_cast<unsigned char>(Peek())) || Peek() == '_') {
                if (Peek() != '_') spelling.push_back(Take());
                else Take();
            }
            if (Peek() == 'e' || Peek() == 'E') {
                spelling.push_back(Take());
                if (Peek() == '+' || Peek() == '-') spelling.push_back(Take());
                while (std::isdigit(static_cast<unsigned char>(Peek()))) spelling.push_back(Take());
            }
            token.kind = Tok::Float;
            token.number = std::stod(spelling);
            return token;
        }
        token.integer = std::stoll(spelling.empty() ? "0" : spelling, nullptr, 10);
        return token;
    }

    char ReadEscape() {
        char character = Take();
        switch (character) {
            case 'n': return '\n';
            case 'r': return '\r';
            case 't': return '\t';
            case '0': return '\0';
            case '\\': return '\\';
            case '"': return '"';
            case '\'': return '\'';
            default: return character;
        }
    }

    Token ReadString() {
        Token token = Here(Tok::String);
        Take();
        while (!Done() && Peek() != '"') {
            if (Peek() == '\n') Bad("unterminated string");
            if (Peek() == '\\') {
                Take();
                token.text.push_back(ReadEscape());
            } else {
                token.text.push_back(Take());
            }
        }
        if (Peek() != '"') Bad("unterminated string");
        Take();
        return token;
    }

    Token ReadChar() {
        Token token = Here(Tok::Char);
        Take();
        if (Peek() == '\\') {
            Take();
            token.integer = static_cast<unsigned char>(ReadEscape());
        } else if (Peek() == '\'' || Done()) {
            Bad("empty character");
        } else {
            token.integer = static_cast<unsigned char>(Take());
        }
        if (Peek() != '\'') Bad("unterminated character");
        Take();
        return token;
    }
};

class Parser {
public:
    Parser(std::vector<Token> tokens, std::string file) : tokens_(std::move(tokens)), file_(std::move(file)) {}

    void ParseFile(Program& program) {
        while (!Check(Tok::End)) {
            if (Match(Tok::Semi)) continue;
            if (Check(Tok::Package) || Check(Tok::Import)) {
                while (!Check(Tok::End) && !Check(Tok::Semi) && !Check(Tok::Explain)) Advance();
                if (Match(Tok::Explain)) SkipBraces();
                Match(Tok::Semi);
                continue;
            }
            SkipModifiers();
            if (Check(Tok::Class) || Check(Tok::Struct)) ParseClass(program);
            else if (Check(Tok::Enum)) ParseEnum(program);
            else if (Match(Tok::Function)) program.functions.push_back(ParseFunction(false));
            else if (Match(Tok::Extern)) {
                Expect(Tok::Function, "expected Function after Extern");
                program.functions.push_back(ParseFunction(true));
            } else {
                Bad("expected a class, enum, or function");
            }
        }
    }

private:
    std::vector<Token> tokens_;
    std::string file_;
    size_t index_ = 0;

    const Token& At() const { return tokens_[index_]; }
    bool Check(Tok kind) const { return At().kind == kind; }
    bool Match(Tok kind) {
        if (!Check(kind)) return false;
        Advance();
        return true;
    }
    void Advance() {
        if (!Check(Tok::End)) index_++;
    }
    [[noreturn]] void Bad(const std::string& message) const {
        Die(file_ + ":" + std::to_string(At().line) + ":" + std::to_string(At().column) + ": " + message);
    }
    void Expect(Tok kind, const std::string& message) {
        if (!Match(kind)) Bad(message);
    }
    bool IsModifier(Tok kind) const {
        return kind == Tok::Public || kind == Tok::Private || kind == Tok::Protected || kind == Tok::Modifier;
    }
    bool IsWord(Tok kind) const {
        switch (kind) {
            case Tok::Ident:
            case Tok::True: case Tok::False: case Tok::Null: case Tok::This:
            case Tok::If: case Tok::Else: case Tok::While: case Tok::For: case Tok::In:
            case Tok::Return: case Tok::Break: case Tok::Continue: case Tok::Var:
            case Tok::Function: case Tok::Class: case Tok::Struct: case Tok::Enum:
            case Tok::Constructor: case Tok::Destructor: case Tok::Explain:
            case Tok::Import: case Tok::Package: case Tok::Extern:
            case Tok::Not: case Tok::And: case Tok::Or:
            case Tok::Public: case Tok::Private: case Tok::Protected: case Tok::Modifier:
                return true;
            default: return false;
        }
    }
    void SkipModifiers() {
        while (IsModifier(At().kind)) Advance();
    }

    void SkipBraces() {
        Expect(Tok::LBrace, "expected '{'");
        int depth = 1;
        while (depth > 0) {
            if (Check(Tok::End)) Bad("unbalanced brace");
            if (Check(Tok::LBrace)) depth++;
            if (Check(Tok::RBrace)) depth--;
            Advance();
        }
    }

    std::string ParseType() {
        if (!Check(Tok::Ident)) Bad("expected a type");
        std::string name = At().text;
        Advance();
        if (name == "Args") name = "List";
        if (Match(Tok::Lt)) {
            std::string inner = ParseType();
            while (Match(Tok::Comma)) inner += "," + ParseType();
            Expect(Tok::Gt, "expected '>'");
            name += "<" + inner + ">";
        }
        if (Match(Tok::Question)) name += "?";
        return name;
    }

    bool LooksLikeDeclaration() {
        size_t saved = index_;
        if (!Check(Tok::Ident)) return false;
        bool ok = true;
        auto guard = [&]() {
            if (!ok) index_ = saved;
        };
        std::string name = At().text;
        Advance();
        if (name == "List" || name == "Args" || Check(Tok::Lt)) {
            if (!Match(Tok::Lt)) {
                index_ = saved;
                return false;
            }
            int depth = 1;
            while (depth > 0) {
                if (Check(Tok::End)) {
                    index_ = saved;
                    return false;
                }
                if (Check(Tok::Lt)) depth++;
                if (Check(Tok::Gt)) depth--;
                Advance();
            }
        }
        if (Check(Tok::Question)) Advance();
        ok = IsWord(At().kind);
        index_ = saved;
        (void)guard;
        return ok;
    }

    std::vector<Param> ParseParams() {
        std::vector<Param> params;
        Expect(Tok::LParen, "expected '('");
        if (Match(Tok::RParen)) return params;
        do {
            Param param;
            param.typeSpelling = ParseType();
            if (!IsWord(At().kind)) Bad("expected a parameter name");
            param.name = At().text;
            Advance();
            if (Match(Tok::Assign)) ParseExpression();
            params.push_back(param);
        } while (Match(Tok::Comma));
        Expect(Tok::RParen, "expected ')'");
        return params;
    }

    Stmt ParseBlock() {
        Stmt block;
        block.kind = Stmt::Kind::Block;
        block.line = At().line;
        Expect(Tok::LBrace, "expected '{'");
        while (!Check(Tok::RBrace) && !Check(Tok::End)) {
            if (Match(Tok::Semi)) continue;
            block.body.push_back(ParseStatement());
        }
        Expect(Tok::RBrace, "expected '}'");
        return block;
    }

    Stmt ParseStatement() {
        if (Check(Tok::If)) return ParseIf();
        if (Check(Tok::While)) return ParseWhile();
        if (Check(Tok::For)) return ParseFor();
        if (Check(Tok::LBrace)) return ParseBlock();
        if (Check(Tok::Return)) return ParseReturn();
        if (Match(Tok::Break)) {
            Stmt stmt;
            stmt.kind = Stmt::Kind::Break;
            stmt.line = At().line;
            Expect(Tok::Semi, "expected ';'");
            return stmt;
        }
        if (Match(Tok::Continue)) {
            Stmt stmt;
            stmt.kind = Stmt::Kind::Continue;
            Expect(Tok::Semi, "expected ';'");
            return stmt;
        }
        if (Check(Tok::Var) || (Check(Tok::Ident) && LooksLikeDeclaration())) return ParseDecl();
        Stmt stmt;
        stmt.line = At().line;
        Expr left = ParseExpression();
        if (Match(Tok::Assign)) {
            stmt.kind = Stmt::Kind::Assign;
            stmt.expr = std::move(left);
            stmt.expr2 = ParseExpression();
            stmt.hasExpr = true;
            stmt.hasExpr2 = true;
        } else {
            stmt.kind = Stmt::Kind::ExprStmt;
            stmt.expr = std::move(left);
            stmt.hasExpr = true;
        }
        Expect(Tok::Semi, "expected ';'");
        return stmt;
    }

    Stmt ParseIf() {
        Stmt stmt;
        stmt.kind = Stmt::Kind::If;
        stmt.line = At().line;
        Expect(Tok::If, "expected If");
        stmt.expr = ParseExpression();
        stmt.hasExpr = true;
        stmt.body.push_back(ParseStatement());
        if (Match(Tok::Else)) stmt.elseOne.push_back(ParseStatement());
        return stmt;
    }

    Stmt ParseWhile() {
        Stmt stmt;
        stmt.kind = Stmt::Kind::While;
        stmt.line = At().line;
        Expect(Tok::While, "expected While");
        stmt.expr = ParseExpression();
        stmt.hasExpr = true;
        stmt.body.push_back(ParseStatement());
        return stmt;
    }

    Stmt ParseFor() {
        Stmt stmt;
        stmt.kind = Stmt::Kind::ForIn;
        stmt.line = At().line;
        Expect(Tok::For, "expected For");
        if (!IsWord(At().kind)) Bad("expected a loop name");
        stmt.name = At().text;
        Advance();
        Expect(Tok::In, "expected In");
        stmt.expr = ParseExpression();
        stmt.hasExpr = true;
        stmt.body.push_back(ParseStatement());
        return stmt;
    }

    Stmt ParseReturn() {
        Stmt stmt;
        stmt.kind = Stmt::Kind::Return;
        stmt.line = At().line;
        Expect(Tok::Return, "expected Return");
        if (!Check(Tok::Semi)) {
            stmt.expr = ParseExpression();
            stmt.hasExpr = true;
        }
        Expect(Tok::Semi, "expected ';'");
        return stmt;
    }

    Stmt ParseDecl() {
        Stmt stmt;
        stmt.kind = Stmt::Kind::Decl;
        stmt.line = At().line;
        if (Match(Tok::Var)) {
            stmt.inferred = true;
            if (!IsWord(At().kind)) Bad("expected a variable name");
            stmt.name = At().text;
            Advance();
            Expect(Tok::Assign, "expected '='");
            stmt.expr = ParseExpression();
            stmt.hasExpr = true;
        } else {
            stmt.typeSpelling = ParseType();
            if (!IsWord(At().kind)) Bad("expected a variable name");
            stmt.name = At().text;
            Advance();
            if (Match(Tok::Assign)) {
                stmt.expr = ParseExpression();
                stmt.hasExpr = true;
            }
        }
        Expect(Tok::Semi, "expected ';'");
        return stmt;
    }

    Expr ParseExpression() { return ParseOr(); }
    Expr ParseOr() { return ParseBinary({Tok::OrOr, Tok::Or}, &Parser::ParseAnd); }
    Expr ParseAnd() { return ParseBinary({Tok::AndAnd, Tok::And}, &Parser::ParseBitOr); }
    Expr ParseBitOr() { return ParseBinary({Tok::Pipe}, &Parser::ParseBitXor); }
    Expr ParseBitXor() { return ParseBinary({Tok::Caret}, &Parser::ParseBitAnd); }
    Expr ParseBitAnd() { return ParseBinary({Tok::Amp}, &Parser::ParseEq); }
    Expr ParseEq() { return ParseBinary({Tok::Eq, Tok::Ne}, &Parser::ParseRel); }
    Expr ParseRel() { return ParseBinary({Tok::Lt, Tok::Gt, Tok::Le, Tok::Ge}, &Parser::ParseAdd); }
    Expr ParseAdd() { return ParseBinary({Tok::Plus, Tok::Minus}, &Parser::ParseMul); }
    Expr ParseMul() { return ParseBinary({Tok::Star, Tok::Slash, Tok::Percent}, &Parser::ParseUnary); }

    Expr ParseBinary(std::initializer_list<Tok> ops, Expr (Parser::*next)()) {
        Expr left = (this->*next)();
        while (std::find(ops.begin(), ops.end(), At().kind) != ops.end()) {
            std::string op = OpText(At().kind);
            Advance();
            Expr right = (this->*next)();
            Expr binary;
            binary.kind = Expr::Kind::Binary;
            binary.text = op;
            binary.line = left.line;
            binary.kids.push_back(std::move(left));
            binary.kids.push_back(std::move(right));
            left = std::move(binary);
        }
        return left;
    }

    static std::string OpText(Tok kind) {
        switch (kind) {
            case Tok::OrOr:
            case Tok::Or: return "||";
            case Tok::AndAnd:
            case Tok::And: return "&&";
            case Tok::Pipe: return "|";
            case Tok::Caret: return "^";
            case Tok::Amp: return "&";
            case Tok::Eq: return "==";
            case Tok::Ne: return "!=";
            case Tok::Lt: return "<";
            case Tok::Gt: return ">";
            case Tok::Le: return "<=";
            case Tok::Ge: return ">=";
            case Tok::Plus: return "+";
            case Tok::Minus: return "-";
            case Tok::Star: return "*";
            case Tok::Slash: return "/";
            case Tok::Percent: return "%";
            default: return "?";
        }
    }

    Expr ParseUnary() {
        if (Check(Tok::Bang) || Check(Tok::Not) || Check(Tok::Minus) || Check(Tok::Plus)) {
            Tok kind = At().kind;
            int line = At().line;
            Advance();
            if (kind == Tok::Plus) return ParseUnary();
            Expr expr;
            expr.kind = Expr::Kind::Unary;
            expr.text = (kind == Tok::Minus) ? "-" : "!";
            expr.line = line;
            expr.kids.push_back(ParseUnary());
            return expr;
        }
        return ParseSuffix(ParsePrimary());
    }

    Expr ParseSuffix(Expr expr) {
        while (true) {
            if (Match(Tok::Dot)) {
                if (!IsWord(At().kind)) Bad("expected a member name");
                Expr member;
                member.kind = Expr::Kind::Member;
                member.text = At().text;
                member.line = At().line;
                Advance();
                member.kids.push_back(std::move(expr));
                expr = std::move(member);
                continue;
            }
            if (Match(Tok::LParen)) {
                Expr call;
                call.kind = Expr::Kind::Call;
                call.line = expr.line;
                call.kids.push_back(std::move(expr));
                if (!Check(Tok::RParen)) {
                    do { call.kids.push_back(ParseExpression()); } while (Match(Tok::Comma));
                }
                Expect(Tok::RParen, "expected ')'");
                expr = std::move(call);
                continue;
            }
            if (Match(Tok::LBrack)) {
                Expr index;
                index.kind = Expr::Kind::Index;
                index.line = expr.line;
                index.kids.push_back(std::move(expr));
                index.kids.push_back(ParseExpression());
                Expect(Tok::RBrack, "expected ']'");
                expr = std::move(index);
                continue;
            }
            break;
        }
        return expr;
    }

    Expr ParsePrimary() {
        int line = At().line;
        if (Match(Tok::Int)) {
            Expr expr;
            expr.kind = Expr::Kind::Int;
            expr.integer = tokens_[index_ - 1].integer;
            expr.line = line;
            return expr;
        }
        if (Check(Tok::Float)) {
            Expr expr;
            expr.kind = Expr::Kind::Float;
            expr.number = At().number;
            expr.line = line;
            Advance();
            return expr;
        }
        if (Check(Tok::String)) {
            Expr expr;
            expr.kind = Expr::Kind::String;
            expr.text = At().text;
            expr.line = line;
            Advance();
            return expr;
        }
        if (Check(Tok::Char)) {
            Expr expr;
            expr.kind = Expr::Kind::Char;
            expr.integer = At().integer;
            expr.line = line;
            Advance();
            return expr;
        }
        if (Match(Tok::True) || Match(Tok::False)) {
            Expr expr;
            expr.kind = Expr::Kind::Bool;
            expr.integer = tokens_[index_ - 1].kind == Tok::True;
            expr.line = line;
            return expr;
        }
        if (Match(Tok::Null)) {
            Expr expr;
            expr.kind = Expr::Kind::Null;
            expr.line = line;
            return expr;
        }
        if (Match(Tok::This)) {
            Expr expr;
            expr.kind = Expr::Kind::This;
            expr.line = line;
            return expr;
        }
        if (Match(Tok::LParen)) {
            Expr expr = ParseExpression();
            Expect(Tok::RParen, "expected ')'");
            return expr;
        }
        if (IsWord(At().kind)) {
            std::string name = At().text;
            if ((name == "List" || name == "Args") && index_ + 1 < tokens_.size() && tokens_[index_ + 1].kind == Tok::Lt) {
                Expr expr;
                expr.kind = Expr::Kind::ListMake;
                expr.typeSpelling = ParseType();
                expr.line = line;
                Expect(Tok::LParen, "expected '(' after a list type");
                Expect(Tok::RParen, "expected ')'");
                return expr;
            }
            Expr expr;
            expr.kind = Expr::Kind::Name;
            expr.text = name;
            expr.line = line;
            Advance();
            return expr;
        }
        Bad("expected an expression");
    }

    void ParseClass(Program& program) {
        bool isStruct = Match(Tok::Struct);
        if (!isStruct) Expect(Tok::Class, "expected Class");
        if (!Check(Tok::Ident)) Bad("expected a class name");
        std::string name = At().text;
        Advance();
        if (Match(Tok::Lt)) {
            int depth = 1;
            while (depth > 0) {
                if (Check(Tok::End)) Bad("unbalanced generic");
                if (Check(Tok::Lt)) depth++;
                if (Check(Tok::Gt)) depth--;
                Advance();
            }
        }
        ClassInfo& info = program.classes[name];
        info.name = name;
        Expect(Tok::LBrace, "expected '{'");
        while (!Check(Tok::RBrace) && !Check(Tok::End)) {
            if (Match(Tok::Semi)) continue;
            SkipModifiers();
            if (Match(Tok::Extern)) {
                Expect(Tok::Function, "expected Function");
                info.methods.push_back(ParseMethod(true));
            } else if (Check(Tok::Function)) {
                Expect(Tok::Function, "expected Function");
                info.methods.push_back(ParseMethod(false));
            } else if (Check(Tok::Constructor)) {
                info.methods.push_back(ParseConstructor(name));
            } else if (Check(Tok::Destructor)) {
                Advance();
                ParseParams();
                if (Check(Tok::Explain)) {
                    Advance();
                    SkipBraces();
                }
                ParseBlock();
            } else {
                Field field;
                field.typeSpelling = ParseType();
                if (!IsWord(At().kind)) Bad("expected a field name");
                field.name = At().text;
                Advance();
                Expect(Tok::Semi, "expected ';'");
                info.fields.push_back(field);
            }
        }
        Expect(Tok::RBrace, "expected '}'");
        (void)isStruct;
    }

    Method ParseMethod(bool isExtern) {
        Method method;
        method.isExtern = isExtern;
        method.line = At().line;
        if (!IsWord(At().kind)) Bad("expected a method name");
        method.name = At().text;
        Advance();
        method.params = ParseParams();
        Expect(Tok::Arrow, "expected '->'");
        method.returnSpelling = ParseType();
        if (Match(Tok::Explain)) SkipBraces();
        if (isExtern) {
            Expect(Tok::Semi, "expected ';'");
        } else {
            method.body = ParseBlock();
        }
        return method;
    }

    Method ParseConstructor(const std::string& className) {
        Method method;
        method.isConstructor = true;
        method.name = className;
        method.returnSpelling = "Void";
        method.line = At().line;
        Expect(Tok::Constructor, "expected Constructor");
        method.params = ParseParams();
        if (Match(Tok::Explain)) SkipBraces();
        method.body = ParseBlock();
        return method;
    }

    FuncInfo ParseFunction(bool isExtern) {
        FuncInfo func;
        func.isExtern = isExtern;
        func.file = file_;
        func.line = At().line;
        if (!IsWord(At().kind)) Bad("expected a function name");
        func.name = At().text;
        Advance();
        func.params = ParseParams();
        Expect(Tok::Arrow, "expected '->'");
        func.returnSpelling = ParseType();
        if (Match(Tok::Explain)) SkipBraces();
        if (isExtern) Expect(Tok::Semi, "expected ';'");
        else func.body = ParseBlock();
        return func;
    }

    void ParseEnum(Program& program) {
        Expect(Tok::Enum, "expected Enum");
        if (!Check(Tok::Ident)) Bad("expected an enum name");
        EnumInfo info;
        info.name = At().text;
        Advance();
        Expect(Tok::LBrace, "expected '{'");
        int64_t next = 0;
        while (!Check(Tok::RBrace) && !Check(Tok::End)) {
            if (Match(Tok::Comma)) continue;
            if (!IsWord(At().kind)) Bad("expected an enum member");
            std::string member = At().text;
            Advance();
            int64_t value = next;
            if (Match(Tok::Assign)) {
                bool negative = Match(Tok::Minus);
                if (!Check(Tok::Int)) Bad("expected an enum value");
                value = At().integer;
                if (negative) value = -value;
                Advance();
            }
            info.members.push_back({member, value});
            next = value + 1;
            Match(Tok::Comma);
        }
        Expect(Tok::RBrace, "expected '}'");
        program.enums[info.name] = info;
    }
};

Type MakeType(Type::Kind kind, const std::string& name = "") {
    Type type;
    type.kind = kind;
    type.name = name;
    return type;
}

std::string InnerSpelling(const std::string& spelling) {
    std::string text = spelling;
    if (!text.empty() && text.back() == '?') text.pop_back();
    if (text.size() < 6 || text.compare(0, 5, "List<") != 0 || text.back() != '>') return "";
    return text.substr(5, text.size() - 6);
}

Type ResolveType(const std::string& spelling, const Program& program);

Type ResolveType(const std::string& original, const Program& program) {
    std::string spelling = original;
    if (!spelling.empty() && spelling.back() == '?') spelling.pop_back();
    if (spelling == "Int" || spelling == "Byte" || spelling == "UInt" || spelling == "Int64" || spelling == "UInt64" ||
        spelling == "Int32" || spelling == "UInt32" || spelling == "Int16" || spelling == "UInt16" || spelling == "Int8" ||
        spelling == "UInt8") {
        return MakeType(Type::Kind::Int);
    }
    if (spelling == "Bool") return MakeType(Type::Kind::Bool);
    if (spelling == "Float" || spelling == "Double" || spelling == "Float64" || spelling == "Float32") return MakeType(Type::Kind::Float);
    if (spelling == "Char") return MakeType(Type::Kind::Char);
    if (spelling == "String") return MakeType(Type::Kind::String);
    if (spelling == "Void") return MakeType(Type::Kind::Void);
    if (spelling == "ExecutionResult") return MakeType(Type::Kind::Exec);
    std::string inner = InnerSpelling(spelling);
    if (!inner.empty() && spelling.compare(0, 5, "List<") == 0) {
        Type type = MakeType(Type::Kind::List);
        type.elem = std::make_shared<Type>(ResolveType(inner, program));
        return type;
    }
    if (program.enums.count(spelling)) return MakeType(Type::Kind::Enum, spelling);
    if (program.classes.count(spelling)) return MakeType(Type::Kind::Object, spelling);
    Die("unknown type '" + original + "'");
}

std::string CppType(const Type& type) {
    switch (type.kind) {
        case Type::Kind::Int:
        case Type::Kind::Char: return "int64_t";
        case Type::Kind::Bool: return "rx::Bool";
        case Type::Kind::Float: return "double";
        case Type::Kind::String: return "rx::String";
        case Type::Kind::Void: return "void";
        case Type::Kind::Exec: return "rx::ExecutionResult";
        case Type::Kind::Enum: return Safe(type.name);
        case Type::Kind::Object: return Safe(type.name) + "Ptr";
        case Type::Kind::List: return "rx::List<" + CppType(*type.elem) + ">";
        default: return "void";
    }
}

bool SameType(const Type& left, const Type& right) {
    if (left.kind != right.kind) return false;
    if (left.kind == Type::Kind::List) return SameType(*left.elem, *right.elem);
    return left.name == right.name;
}

std::string Escape(const std::string& text) {
    std::string out;
    for (unsigned char character : text) {
        switch (character) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (character < 32) {
                    const char* hex = "0123456789abcdef";
                    out += "\\x";
                    out.push_back(hex[character >> 4]);
                    out.push_back(hex[character & 15]);
                } else {
                    out.push_back(static_cast<char>(character));
                }
        }
    }
    return out;
}

#include "emitter.inc"

std::vector<std::string> SourceFiles() {
    std::vector<std::string> files;
    auto add = [&](const fs::path& path) {
        std::string name = path.generic_string();
        if (name.size() >= 8 && name.rfind(".test.rx") == name.size() - 8) return;
        if (name.rfind("Examples/", 0) == 0) return;
        files.push_back(name);
    };
    for (const auto& entry : fs::directory_iterator(".")) {
        if (!entry.is_regular_file()) continue;
        auto path = entry.path();
        if (path.extension() == ".rx") add(path.filename());
    }
    for (const char* directory : {"Compiler", "Interpreter", "Runtime"}) {
        if (!fs::exists(directory)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ".rx") add(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

Program LoadProgram() {
    Program program;
    for (const std::string& file : SourceFiles()) {
        std::ifstream input(file, std::ios::binary);
        if (!input) Die("could not read " + file);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        Lexer lexer(buffer.str(), file);
        Parser parser(lexer.Tokenize(), file);
        parser.ParseFile(program);
    }
    std::set<std::string> defined;
    for (const FuncInfo& func : program.functions) {
        if (func.isExtern) continue;
        std::string key = func.file + "\n" + func.name;
        (void)key;
        if (func.name == "Main") continue;
        std::string id = func.name + "/" + std::to_string(func.params.size());
        if (!defined.insert(func.file.substr(0, 0) + id).second) {
            // duplicate detection across files happens below
        }
    }
    std::map<std::string, std::string> owners;
    for (const FuncInfo& func : program.functions) {
        if (func.isExtern || func.name == "Main") continue;
        std::string id = func.name + "/" + std::to_string(func.params.size());
        auto found = owners.find(id);
        if (found != owners.end() && found->second != func.file) {
            bool toolPair = (found->second == "Robix.rx" && func.file == "Robixc.rx") || (found->second == "Robixc.rx" && func.file == "Robix.rx");
            if (!toolPair) Die(func.name + " is defined in both " + found->second + " and " + func.file);
        }
        owners[id] = func.file;
    }
    return program;
}

std::string OutputFile(const Target& target, const std::string& emit, bool host) {
    std::string ext = std::string(target.os) == "windows" ? ".exe" : ".app";
    std::string leaf = emit == "Robixc" && std::string(target.os) == "windows" ? "Compiler.exe" : emit + ext;
    if (host) return (fs::path("Build/Compiler") / (emit + ext)).string();
    if (std::string(target.name) == "windows-x64") return (fs::path("dist/windows-tools") / leaf).string();
    return (fs::path("dist") / target.name / leaf).string();
}

std::string CompilerFor(const Target& target, bool host) {
    if (const char* overrideCompiler = std::getenv("ROBIX_CXX")) {
        if (*overrideCompiler) return overrideCompiler;
    }
    if (std::string(target.os) == "windows") {
        const char* tool = "x86_64-w64-mingw32-clang++";
        if (std::string(target.arch) == "arm64") tool = "aarch64-w64-mingw32-clang++";
        if (std::string(target.arch) == "x86") tool = "i686-w64-mingw32-clang++";
        std::string found = FindOnPath(tool);
        if (!found.empty()) return found;
    }
    std::string clang = FindOnPath("clang++");
    if (!clang.empty()) return clang;
    if (host) {
        std::string gcc = FindOnPath("g++");
        if (!gcc.empty()) return gcc;
    }
    Die(std::string("no C++ compiler was found for ") + target.name);
}

void Produce(const Target& target, const std::vector<std::string>& emits, bool host) {
    std::cerr << "bootstrap: reading Robix sources" << std::endl;
    Program program = LoadProgram();
    RobixEmitter emitter(program);
    std::string compiler = CompilerFor(target, host);
    fs::path include = fs::absolute(fs::path("Bootstrap"));
    for (const std::string& emit : emits) {
        std::cerr << "bootstrap: compiling " << emit << " for " << target.name << std::endl;
        std::string generated = emitter.Generate(emit);
        fs::path source = fs::path("Build/bootstrap") / ("generated-" + emit + ".cpp");
        fs::create_directories(source.parent_path());
        {
            std::ofstream output(source);
            if (!output) Die("could not write " + source.string());
            output << generated;
        }
        std::string binary = OutputFile(target, emit, host);
        fs::create_directories(fs::path(binary).parent_path());
        std::string command = ShellQuote(compiler) + " -std=c++20 -O2 -Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-sign-compare -Wno-parentheses -Wno-return-type";
        if (IsClang(compiler)) command += " --target=" + ShellQuote(target.triple);
        if (std::string(target.os) == "windows") command += " -static";
        else command += " -pthread";
        command += " -I " + ShellQuote(include.string()) + " " + ShellQuote(fs::absolute(source).string()) + " -o " + ShellQuote(fs::absolute(binary).string());
        int status = std::system(command.c_str());
        if (status != 0) Die(std::string("the host C++ compiler failed while building ") + emit + " for " + target.name);
#ifndef _WIN32
        chmod(binary.c_str(), 0755);
#endif
        std::cerr << "bootstrap: wrote " << binary << std::endl;
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string targetName = "host";
    std::vector<std::string> emits;
    std::string command;
    for (int index = 1; index < argc; index++) {
        std::string arg = argv[index];
        auto needs = [&](const std::string& flag) {
            if (index + 1 >= argc) Die(flag + " needs a value");
            return std::string(argv[++index]);
        };
        if (arg == "--targets" || arg == "--bootstrap") {
            for (const Target& target : kTargets) std::cout << target.name << " " << target.triple << std::endl;
            return 0;
        }
        if (arg == "--target") targetName = needs(arg);
        else if (arg.rfind("--target=", 0) == 0) targetName = arg.substr(9);
        else if (arg == "--emit") emits.push_back(needs(arg));
        else if (arg == "--Configuration" || arg == "--Filter" || arg == "--output") needs(arg);
        else if (arg == "--NoCache" || arg == "--Verbose" || arg == "--DryRun" || arg == "--console=off" || arg == "--console=on") {
        } else if (arg.rfind("--", 0) == 0) {
        } else if (command.empty()) command = arg;
    }
    if (command.empty()) command = "Build";
    if (command != "Build") Die("the bootstrap compiler supports Build. got '" + command + "'");
    if (emits.empty()) emits.push_back("Robix");
    bool host = targetName == "host";
    if (host) targetName = HostTargetName();
    const Target* target = FindTarget(targetName);
    if (target == nullptr) {
        std::string names;
        for (const Target& item : kTargets) {
            if (!names.empty()) names += ", ";
            names += item.name;
        }
        Die("unknown target '" + targetName + "'. targets: " + names);
    }
    if (!fs::exists("Robix.rxp")) Die("Robix.rxp was not found in the working directory");
    Produce(*target, emits, host);
    return 0;
}
