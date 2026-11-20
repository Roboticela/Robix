#define _GNU_SOURCE
/* Backup Robix compiler.
   The language is written in itself, so the first binary is produced here.
   Each Robix class becomes a C struct and each integer stays an int64_t,
   which is the same code clang emits for an equivalent C program.
   The self-hosted compiler in the .rx sources replaces this one after bootstrap. */
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum {
    T_EOF = 0, T_IDENT, T_INT, T_FLOAT, T_STR, T_CHAR,
    T_LPAREN, T_RPAREN, T_LBRACK, T_RBRACK, T_LBRACE, T_RBRACE,
    T_COMMA, T_SEMI, T_COLON, T_DOT, T_ARROW, T_Q,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_EQ, T_EQEQ, T_NE, T_LT, T_GT, T_LE, T_GE,
    T_AND, T_OR, T_NOT,
    T_PLUSEQ, T_MINUSEQ, T_STAREQ, T_SLASHEQ, T_PERCENTEQ,
    T_DOTDOT, T_DOTDOTEQ
};

enum {
    N_NONE = 0, N_INT, N_FLOAT, N_STR, N_CHAR, N_BOOL, N_NULL, N_NAME,
    N_BIN, N_UN, N_CALL, N_MEMBER, N_INDEX, N_ASSIGN, N_RANGE,
    N_VAR, N_IF, N_WHILE, N_FOR, N_RETURN, N_BREAK, N_CONT,
    N_BLOCK, N_EXPR
};

typedef struct Node Node;
struct Node {
    int kind;
    char *text;
    char *type;
    int line;
    int ival;
    int cap;
    int nkids;
    Node *a, *b, *c;
    Node **kids;
};

typedef struct Lex {
    const char *path;
    const char *src;
    int n, i, line, col;
    int kind;
    char *text;
    int tok_line;
    long long ival;
} Lex;

typedef struct Param {
    char *name;
    char *type;
} Param;

typedef struct Func {
    char *name;
    char *cname;
    char *ret;
    Param *params;
    int nparams;
    int cap;
    Node *body;
    int is_extern;
    int is_ctor;
    int assign_params;
    char *file;
    char *stem;
    int line;
    char *owner;
} Func;

typedef struct Field {
    char *name;
    char *type;
    Node *init;
} Field;

typedef struct EnumItem {
    char *name;
    int value;
} EnumItem;

typedef struct Class {
    char *name;
    int is_enum;
    Field *fields;
    int nfields;
    int fcap;
    Func *methods;
    int nmethods;
    int mcap;
    EnumItem *items;
    int nitems;
    int icap;
} Class;

typedef struct Expr {
    char *code;
    char *type;
} Expr;

typedef struct Var {
    char *name;
    char *type;
    int depth;
} Var;

static Class *classes;
static int nclasses, ccap;
static Func *funcs;
static int nfuncs, fcap;
static char *current_file;
static int errors;

static void die(const char *message) {
    fprintf(stderr, "rxc: %s\n", message);
    exit(1);
}

static void *xmalloc(size_t size) {
    void *pointer = calloc(1, size);
    if (pointer == NULL) {
        die("out of memory");
    }
    return pointer;
}

static char *xdup(const char *text) {
    size_t length = strlen(text);
    char *copy = xmalloc(length + 1);
    memcpy(copy, text, length);
    return copy;
}

static char *fmt(const char *spec, ...) {
    va_list args;
    va_start(args, spec);
    char *text = NULL;
    if (vasprintf(&text, spec, args) < 0 || text == NULL) {
        die("out of memory");
    }
    va_end(args);
    return text;
}

static int same(const char *left, const char *right) {
    if (left == NULL || right == NULL) {
        return 0;
    }
    return strcmp(left, right) == 0;
}

static int starts(const char *text, const char *prefix) {
    return text != NULL && strncmp(text, prefix, strlen(prefix)) == 0;
}

static Node *node(int kind, int line) {
    Node *value = xmalloc(sizeof(Node));
    value->kind = kind;
    value->line = line;
    value->text = "";
    value->type = "";
    return value;
}

static void add_kid(Node *parent, Node *child) {
    if (child == NULL) {
        return;
    }
    if (parent->nkids + 1 > parent->cap) {
        parent->cap = parent->cap == 0 ? 4 : parent->cap * 2;
        parent->kids = realloc(parent->kids, (size_t)parent->cap * sizeof(Node *));
        if (parent->kids == NULL) {
            die("out of memory");
        }
    }
    parent->kids[parent->nkids++] = child;
}

static void fail(Lex *lex, const char *message) {
    fprintf(stderr, "%s:%d: %s (near '%s')\n", lex->path, lex->tok_line, message, lex->text ? lex->text : "");
    exit(1);
}

static int is_ident_char(unsigned char character, int first) {
    if (character == '_' || isalpha(character)) {
        return 1;
    }
    if (!first && isdigit(character)) {
        return 1;
    }
    return character >= 128;
}

static char *unescape(Lex *lex, const char *text, int length, int *out_len) {
    char *copy = xmalloc((size_t)length + 1);
    int out = 0;
    int index = 0;
    while (index < length) {
        if (text[index] == '\\' && index + 1 < length) {
            char escaped = text[++index];
            if (escaped == 'n') copy[out++] = '\n';
            else if (escaped == 'r') copy[out++] = '\r';
            else if (escaped == 't') copy[out++] = '\t';
            else if (escaped == '0') copy[out++] = '\0';
            else if (escaped == 'u' && index + 4 < length) {
                int code = 0;
                int count = 0;
                while (count < 4) {
                    char digit = text[++index];
                    code *= 16;
                    if (digit >= '0' && digit <= '9') code += digit - '0';
                    else if (digit >= 'a' && digit <= 'f') code += digit - 'a' + 10;
                    else if (digit >= 'A' && digit <= 'F') code += digit - 'A' + 10;
                    count++;
                }
                copy[out++] = (char)code;
            } else {
                copy[out++] = escaped;
            }
        } else {
            copy[out++] = text[index];
        }
        index++;
    }
    copy[out] = 0;
    if (out_len) {
        *out_len = out;
    }
    (void)lex;
    return copy;
}

static void next_token(Lex *lex) {
    const char *source = lex->src;
    int index = lex->i;
    int line = lex->line;
    int column = lex->col;
    for (;;) {
        if (index >= lex->n) {
            lex->kind = T_EOF;
            lex->text = "";
            lex->tok_line = line;
            lex->i = index;
            lex->line = line;
            lex->col = column;
            return;
        }
        char character = source[index];
        if (character == ' ' || character == '\t' || character == '\r' || character == '\v' || character == '\f') {
            index++;
            column++;
            continue;
        }
        if (character == '\n') {
            index++;
            line++;
            column = 1;
            continue;
        }
        if (character == '/' && index + 1 < lex->n && source[index + 1] == '/') {
            index += 2;
            while (index < lex->n && source[index] != '\n') index++;
            continue;
        }
        if (character == '/' && index + 1 < lex->n && source[index + 1] == '*') {
            index += 2;
            while (index + 1 < lex->n && !(source[index] == '*' && source[index + 1] == '/')) {
                if (source[index] == '\n') line++;
                index++;
            }
            if (index + 1 < lex->n) index += 2;
            continue;
        }
        break;
    }
    lex->tok_line = line;
    char character = source[index];
    if (is_ident_char((unsigned char)character, 1)) {
        int start = index++;
        while (index < lex->n && is_ident_char((unsigned char)source[index], 0)) index++;
        char *text = xmalloc((size_t)(index - start) + 1);
        memcpy(text, source + start, (size_t)(index - start));
        lex->kind = T_IDENT;
        lex->text = text;
        lex->i = index;
        lex->line = line;
        lex->col = column + (index - start);
        return;
    }
    if (isdigit((unsigned char)character)) {
        int start = index++;
        while (index < lex->n && (isdigit((unsigned char)source[index]) || source[index] == '_')) index++;
        int is_float = 0;
        if (index < lex->n && source[index] == '.' && index + 1 < lex->n && isdigit((unsigned char)source[index + 1])) {
            is_float = 1;
            index++;
            while (index < lex->n && (isdigit((unsigned char)source[index]) || source[index] == '_')) index++;
        }
        if (index < lex->n && (source[index] == 'e' || source[index] == 'E')) {
            is_float = 1;
            index++;
            if (index < lex->n && (source[index] == '+' || source[index] == '-')) index++;
            while (index < lex->n && isdigit((unsigned char)source[index])) index++;
        }
        char *text = xmalloc((size_t)(index - start) + 1);
        int out = 0;
        int cursor = start;
        while (cursor < index) {
            if (source[cursor] != '_') text[out++] = source[cursor];
            cursor++;
        }
        text[out] = 0;
        lex->kind = is_float ? T_FLOAT : T_INT;
        lex->text = text;
        lex->ival = strtoll(text, NULL, 10);
        lex->i = index;
        lex->line = line;
        lex->col = column + (index - start);
        return;
    }
    if (character == '"' || (character == '$' && index + 1 < lex->n && source[index + 1] == '"')) {
        int interpolated = character == '$';
        if (interpolated) index++;
        int triple = index + 2 < lex->n && source[index + 1] == '"' && source[index + 2] == '"';
        int start = index + (triple ? 3 : 1);
        index = start;
        while (index < lex->n) {
            if (triple && index + 2 < lex->n && source[index] == '"' && source[index + 1] == '"' && source[index + 2] == '"') break;
            if (!triple && source[index] == '"') break;
            if (source[index] == '\\' && index + 1 < lex->n) index += 2;
            else {
                if (source[index] == '\n') line++;
                index++;
            }
        }
        int raw_len = index - start;
        int text_len = 0;
        char *text = unescape(lex, source + start, raw_len, &text_len);
        if (triple) index += 3;
        else if (index < lex->n && source[index] == '"') index++;
        lex->kind = T_STR;
        lex->text = text;
        lex->ival = text_len;
        lex->i = index;
        lex->line = line;
        lex->col = column;
        (void)interpolated;
        return;
    }
    if (character == '\'') {
        index++;
        int code = 0;
        if (index < lex->n && source[index] == '\\') {
            index++;
            char escaped = index < lex->n ? source[index++] : 0;
            if (escaped == 'n') code = '\n';
            else if (escaped == 'r') code = '\r';
            else if (escaped == 't') code = '\t';
            else if (escaped == '0') code = 0;
            else code = (unsigned char)escaped;
        } else if (index < lex->n) {
            code = (unsigned char)source[index++];
        }
        if (index < lex->n && source[index] == '\'') index++;
        lex->kind = T_CHAR;
        lex->text = fmt("%d", code);
        lex->ival = code;
        lex->i = index;
        lex->line = line;
        lex->col = column;
        return;
    }
    int two = index + 1 < lex->n ? source[index + 1] : 0;
    int three = index + 2 < lex->n ? source[index + 2] : 0;
    int kind = T_EOF;
    int length = 1;
    if (character == '=' && two == '=') { kind = T_EQEQ; length = 2; }
    else if (character == '!' && two == '=') { kind = T_NE; length = 2; }
    else if (character == '<' && two == '=') { kind = T_LE; length = 2; }
    else if (character == '>' && two == '=') { kind = T_GE; length = 2; }
    else if (character == '&' && two == '&') { kind = T_AND; length = 2; }
    else if (character == '|' && two == '|') { kind = T_OR; length = 2; }
    else if (character == '+' && two == '=') { kind = T_PLUSEQ; length = 2; }
    else if (character == '-' && two == '=') { kind = T_MINUSEQ; length = 2; }
    else if (character == '-' && two == '>') { kind = T_ARROW; length = 2; }
    else if (character == '*' && two == '=') { kind = T_STAREQ; length = 2; }
    else if (character == '/' && two == '=') { kind = T_SLASHEQ; length = 2; }
    else if (character == '%' && two == '=') { kind = T_PERCENTEQ; length = 2; }
    else if (character == '.' && two == '.' && three == '=') { kind = T_DOTDOTEQ; length = 3; }
    else if (character == '.' && two == '.') { kind = T_DOTDOT; length = 2; }
    else if (character == '(') kind = T_LPAREN;
    else if (character == ')') kind = T_RPAREN;
    else if (character == '[') kind = T_LBRACK;
    else if (character == ']') kind = T_RBRACK;
    else if (character == '{') kind = T_LBRACE;
    else if (character == '}') kind = T_RBRACE;
    else if (character == ',') kind = T_COMMA;
    else if (character == ';') kind = T_SEMI;
    else if (character == ':') kind = T_COLON;
    else if (character == '.') kind = T_DOT;
    else if (character == '?') kind = T_Q;
    else if (character == '+') kind = T_PLUS;
    else if (character == '-') kind = T_MINUS;
    else if (character == '*') kind = T_STAR;
    else if (character == '/') kind = T_SLASH;
    else if (character == '%') kind = T_PERCENT;
    else if (character == '=') kind = T_EQ;
    else if (character == '<') kind = T_LT;
    else if (character == '>') kind = T_GT;
    else if (character == '!') kind = T_NOT;
    else fail(lex, fmt("unexpected character '%c'", character));
    lex->kind = kind;
    lex->text = fmt("%.*s", length, source + index);
    lex->i = index + length;
    lex->line = line;
    lex->col = column + length;
}

static int is_word(Lex *lex, const char *word) {
    return lex->kind == T_IDENT && same(lex->text, word);
}

static int is_modifier(const char *word) {
    static const char *mods[] = {
        "Public", "Private", "Protected", "Internal", "Static", "Partial",
        "Abstract", "Override", "Virtual", "Sealed", "Final", "Async",
        "Unsafe", "Mut", "Extern", "Iterator", "Consteval", "Ref", NULL
    };
    for (int index = 0; mods[index]; index++) {
        if (same(word, mods[index])) return 1;
    }
    return 0;
}

static int is_c_keyword(const char *word) {
    static const char *words[] = {
        "auto", "break", "case", "char", "const", "continue", "default", "do",
        "double", "else", "enum", "extern", "float", "for", "goto", "if",
        "inline", "int", "long", "register", "restrict", "return", "short",
        "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile", "while", NULL
    };
    for (int index = 0; words[index]; index++) {
        if (same(word, words[index])) return 1;
    }
    return 0;
}

static char *c_name(const char *name) {
    if (is_c_keyword(name)) return fmt("%s_rx", name);
    return xdup(name);
}

static void skip_braces(Lex *lex) {
    if (lex->kind != T_LBRACE) fail(lex, "expected '{'");
    int depth = 1;
    next_token(lex);
    while (lex->kind != T_EOF && depth > 0) {
        if (lex->kind == T_LBRACE) depth++;
        else if (lex->kind == T_RBRACE) depth--;
        if (depth > 0) next_token(lex);
    }
    if (lex->kind == T_RBRACE) next_token(lex);
}

static char *parse_type(Lex *lex);
static Node *parse_expr(Lex *lex);
static Node *parse_stmt(Lex *lex);
static Node *parse_block(Lex *lex);

static int upper_name(const char *name) {
    return name && name[0] >= 'A' && name[0] <= 'Z';
}

static char *parse_type(Lex *lex) {
    if (lex->kind != T_IDENT) return NULL;
    char *name = xdup(lex->text);
    next_token(lex);
    if (lex->kind == T_LT && (upper_name(name) || same(name, "List") || same(name, "Args"))) {
        next_token(lex);
        char *inner = parse_type(lex);
        if (inner == NULL) fail(lex, "expected a type argument");
        while (lex->kind == T_COMMA) {
            next_token(lex);
            char *more = parse_type(lex);
            inner = fmt("%s,%s", inner, more ? more : "");
        }
        if (lex->kind != T_GT) fail(lex, "expected '>' after type arguments");
        next_token(lex);
        if (same(name, "List") || same(name, "Args")) name = fmt("List:%s", inner);
        else name = fmt("%s:%s", name, inner);
    }
    if (lex->kind == T_Q) next_token(lex);
    return name;
}

static int looks_like_decl(Lex *lex) {
    if (lex->kind != T_IDENT) return 0;
    Lex saved = *lex;
    char *type = parse_type(lex);
    int yes = type != NULL && lex->kind == T_IDENT;
    *lex = saved;
    return yes;
}

static Node *parse_name(Lex *lex) {
    if (lex->kind != T_IDENT) fail(lex, "expected a name");
    Node *value = node(N_NAME, lex->tok_line);
    value->text = xdup(lex->text);
    next_token(lex);
    if (lex->kind == T_LT && upper_name(value->text)) {
        Lex saved = *lex;
        next_token(lex);
        char *inner = parse_type(lex);
        if (inner && lex->kind == T_GT) {
            next_token(lex);
            if (same(value->text, "List") || same(value->text, "Args")) value->type = fmt("List:%s", inner);
            else value->type = fmt("%s:%s", value->text, inner);
        } else {
            *lex = saved;
        }
    }
    return value;
}

static Node *parse_args(Lex *lex, Node *call) {
    if (lex->kind != T_LPAREN) fail(lex, "expected '('");
    next_token(lex);
    while (lex->kind != T_RPAREN && lex->kind != T_EOF) {
        if (lex->kind == T_IDENT && lex->text && isalpha((unsigned char)lex->text[0])) {
            Lex saved = *lex;
            next_token(lex);
            if (lex->kind == T_COLON) {
                next_token(lex);
                add_kid(call, parse_expr(lex));
            } else {
                *lex = saved;
                add_kid(call, parse_expr(lex));
            }
        } else {
            add_kid(call, parse_expr(lex));
        }
        if (lex->kind == T_COMMA) next_token(lex);
        else break;
    }
    if (lex->kind != T_RPAREN) fail(lex, "expected ')'");
    next_token(lex);
    return call;
}

static Node *parse_primary(Lex *lex) {
    if (lex->kind == T_INT) {
        Node *value = node(N_INT, lex->tok_line);
        value->text = xdup(lex->text);
        value->type = "Int";
        next_token(lex);
        return value;
    }
    if (lex->kind == T_FLOAT) {
        Node *value = node(N_FLOAT, lex->tok_line);
        value->text = xdup(lex->text);
        value->type = "Float";
        next_token(lex);
        return value;
    }
    if (lex->kind == T_STR) {
        Node *value = node(N_STR, lex->tok_line);
        value->text = xdup(lex->text);
        value->ival = (int)lex->ival;
        value->type = "String";
        next_token(lex);
        return value;
    }
    if (lex->kind == T_CHAR) {
        Node *value = node(N_CHAR, lex->tok_line);
        value->ival = (int)lex->ival;
        value->text = fmt("%d", value->ival);
        value->type = "Char";
        next_token(lex);
        return value;
    }
    if (lex->kind == T_LPAREN) {
        next_token(lex);
        Node *value = parse_expr(lex);
        if (lex->kind != T_RPAREN) fail(lex, "expected ')'");
        next_token(lex);
        return value;
    }
    if (lex->kind == T_IDENT) {
        if (same(lex->text, "True") || same(lex->text, "False")) {
            Node *value = node(N_BOOL, lex->tok_line);
            value->ival = same(lex->text, "True");
            value->text = value->ival ? "1" : "0";
            value->type = "Bool";
            next_token(lex);
            return value;
        }
        if (same(lex->text, "Null")) {
            Node *value = node(N_NULL, lex->tok_line);
            value->text = "0";
            value->type = "Null";
            next_token(lex);
            return value;
        }
        return parse_name(lex);
    }
    fail(lex, "expected an expression");
    return node(N_NONE, lex->tok_line);
}

static Node *parse_postfix(Lex *lex) {
    Node *value = parse_primary(lex);
    for (;;) {
        if (lex->kind == T_DOT) {
            next_token(lex);
            if (lex->kind != T_IDENT) fail(lex, "expected a member name");
            Node *member = node(N_MEMBER, lex->tok_line);
            member->a = value;
            member->text = xdup(lex->text);
            next_token(lex);
            value = member;
        } else if (lex->kind == T_LBRACK) {
            next_token(lex);
            Node *index = node(N_INDEX, value->line);
            index->a = value;
            index->b = parse_expr(lex);
            if (lex->kind != T_RBRACK) fail(lex, "expected ']'");
            next_token(lex);
            value = index;
        } else if (lex->kind == T_LPAREN) {
            Node *call = node(N_CALL, value->line);
            call->a = value;
            parse_args(lex, call);
            value = call;
        } else break;
    }
    return value;
}

static Node *parse_unary(Lex *lex) {
    if (lex->kind == T_NOT || lex->kind == T_MINUS || lex->kind == T_PLUS) {
        Node *value = node(N_UN, lex->tok_line);
        value->text = xdup(lex->text);
        next_token(lex);
        value->a = parse_unary(lex);
        return value;
    }
    if (is_word(lex, "Not")) {
        Node *value = node(N_UN, lex->tok_line);
        value->text = "!";
        next_token(lex);
        value->a = parse_unary(lex);
        return value;
    }
    return parse_postfix(lex);
}

static Node *parse_mul(Lex *lex) {
    Node *left = parse_unary(lex);
    while (lex->kind == T_STAR || lex->kind == T_SLASH || lex->kind == T_PERCENT) {
        Node *value = node(N_BIN, lex->tok_line);
        value->text = xdup(lex->text);
        next_token(lex);
        value->a = left;
        value->b = parse_unary(lex);
        left = value;
    }
    return left;
}

static Node *parse_add(Lex *lex) {
    Node *left = parse_mul(lex);
    while (lex->kind == T_PLUS || lex->kind == T_MINUS) {
        Node *value = node(N_BIN, lex->tok_line);
        value->text = xdup(lex->text);
        next_token(lex);
        value->a = left;
        value->b = parse_mul(lex);
        left = value;
    }
    return left;
}

static Node *parse_range(Lex *lex) {
    Node *left = parse_add(lex);
    if (lex->kind == T_DOTDOT || lex->kind == T_DOTDOTEQ) {
        Node *value = node(N_RANGE, lex->tok_line);
        value->text = xdup(lex->text);
        next_token(lex);
        value->a = left;
        value->b = parse_add(lex);
        value->type = "Range";
        return value;
    }
    return left;
}

static Node *parse_rel(Lex *lex) {
    Node *left = parse_range(lex);
    while (lex->kind == T_LT || lex->kind == T_GT || lex->kind == T_LE || lex->kind == T_GE) {
        Node *value = node(N_BIN, lex->tok_line);
        value->text = xdup(lex->text);
        next_token(lex);
        value->a = left;
        value->b = parse_range(lex);
        left = value;
    }
    return left;
}

static Node *parse_eq(Lex *lex) {
    Node *left = parse_rel(lex);
    while (lex->kind == T_EQEQ || lex->kind == T_NE) {
        Node *value = node(N_BIN, lex->tok_line);
        value->text = xdup(lex->text);
        next_token(lex);
        value->a = left;
        value->b = parse_rel(lex);
        left = value;
    }
    return left;
}

static Node *parse_and(Lex *lex) {
    Node *left = parse_eq(lex);
    while (lex->kind == T_AND || is_word(lex, "And")) {
        Node *value = node(N_BIN, lex->tok_line);
        value->text = "&&";
        next_token(lex);
        value->a = left;
        value->b = parse_eq(lex);
        left = value;
    }
    return left;
}

static Node *parse_or(Lex *lex) {
    Node *left = parse_and(lex);
    while (lex->kind == T_OR || is_word(lex, "Or")) {
        Node *value = node(N_BIN, lex->tok_line);
        value->text = "||";
        next_token(lex);
        value->a = left;
        value->b = parse_and(lex);
        left = value;
    }
    return left;
}

static int assign_op(int kind) {
    return kind == T_EQ || kind == T_PLUSEQ || kind == T_MINUSEQ || kind == T_STAREQ || kind == T_SLASHEQ || kind == T_PERCENTEQ;
}

static Node *parse_expr(Lex *lex) {
    Node *left = parse_or(lex);
    if (assign_op(lex->kind)) {
        Node *value = node(N_ASSIGN, lex->tok_line);
        value->text = xdup(lex->text);
        next_token(lex);
        value->a = left;
        value->b = parse_expr(lex);
        return value;
    }
    return left;
}

static Node *parse_block(Lex *lex) {
    if (lex->kind != T_LBRACE) fail(lex, "expected '{'");
    Node *block = node(N_BLOCK, lex->tok_line);
    next_token(lex);
    while (lex->kind != T_RBRACE && lex->kind != T_EOF) {
        if (lex->kind == T_SEMI) {
            next_token(lex);
            continue;
        }
        add_kid(block, parse_stmt(lex));
    }
    if (lex->kind != T_RBRACE) fail(lex, "expected '}'");
    next_token(lex);
    return block;
}

static Node *parse_body(Lex *lex) {
    if (lex->kind == T_LBRACE) return parse_block(lex);
    return parse_stmt(lex);
}

static Node *parse_if(Lex *lex) {
    Node *value = node(N_IF, lex->tok_line);
    next_token(lex);
    value->a = parse_expr(lex);
    value->b = parse_body(lex);
    if (is_word(lex, "Else")) {
        next_token(lex);
        value->c = parse_body(lex);
    }
    return value;
}

static Node *parse_while(Lex *lex) {
    Node *value = node(N_WHILE, lex->tok_line);
    next_token(lex);
    value->a = parse_expr(lex);
    value->b = parse_body(lex);
    return value;
}

static Node *parse_for(Lex *lex) {
    Node *value = node(N_FOR, lex->tok_line);
    next_token(lex);
    if (lex->kind != T_IDENT) fail(lex, "expected a loop name");
    value->text = xdup(lex->text);
    next_token(lex);
    if (!is_word(lex, "In")) fail(lex, "expected 'In'");
    next_token(lex);
    value->a = parse_expr(lex);
    value->b = parse_body(lex);
    return value;
}

static Node *parse_return(Lex *lex) {
    Node *value = node(N_RETURN, lex->tok_line);
    next_token(lex);
    if (lex->kind != T_SEMI && lex->kind != T_RBRACE) value->a = parse_expr(lex);
    if (lex->kind == T_SEMI) next_token(lex);
    else fail(lex, "expected ';' after Return");
    return value;
}

static Node *parse_var(Lex *lex, char *type_name, char *name, int line) {
    Node *value = node(N_VAR, line);
    value->text = name;
    value->type = type_name ? type_name : "";
    if (lex->kind == T_EQ) {
        next_token(lex);
        value->a = parse_expr(lex);
    }
    if (lex->kind == T_SEMI) next_token(lex);
    else fail(lex, "expected ';' after the variable");
    return value;
}

static Node *parse_stmt(Lex *lex) {
    if (lex->kind == T_SEMI) {
        next_token(lex);
        return node(N_NONE, lex->tok_line);
    }
    if (lex->kind == T_LBRACE) return parse_block(lex);
    if (is_word(lex, "If")) return parse_if(lex);
    if (is_word(lex, "While")) return parse_while(lex);
    if (is_word(lex, "For")) return parse_for(lex);
    if (is_word(lex, "Return")) return parse_return(lex);
    if (is_word(lex, "Break") || is_word(lex, "Continue")) {
        Node *value = node(is_word(lex, "Break") ? N_BREAK : N_CONT, lex->tok_line);
        next_token(lex);
        if (lex->kind == T_SEMI) next_token(lex);
        return value;
    }
    if (is_word(lex, "Var") || is_word(lex, "Let") || is_word(lex, "Const")) {
        int line = lex->tok_line;
        next_token(lex);
        if (lex->kind != T_IDENT) fail(lex, "expected a variable name");
        char *name = xdup(lex->text);
        next_token(lex);
        return parse_var(lex, NULL, name, line);
    }
    if (looks_like_decl(lex)) {
        int line = lex->tok_line;
        char *type = parse_type(lex);
        if (lex->kind != T_IDENT) fail(lex, "expected a variable name");
        char *name = xdup(lex->text);
        next_token(lex);
        return parse_var(lex, type, name, line);
    }
    if (is_word(lex, "Explain")) {
        next_token(lex);
        if (lex->kind == T_LBRACE) skip_braces(lex);
        return node(N_NONE, lex->tok_line);
    }
    Node *expr = node(N_EXPR, lex->tok_line);
    expr->a = parse_expr(lex);
    if (lex->kind == T_SEMI) next_token(lex);
    else fail(lex, "expected ';' after the expression");
    return expr;
}

static void parse_params(Lex *lex, Func *func) {
    if (lex->kind != T_LPAREN) fail(lex, "expected '('");
    next_token(lex);
    while (lex->kind != T_RPAREN && lex->kind != T_EOF) {
        char *type = parse_type(lex);
        if (type == NULL || lex->kind != T_IDENT) fail(lex, "expected a parameter");
        if (func->nparams + 1 > func->cap) {
            func->cap = func->cap == 0 ? 4 : func->cap * 2;
            func->params = realloc(func->params, (size_t)func->cap * sizeof(Param));
        }
        func->params[func->nparams].type = type;
        func->params[func->nparams].name = xdup(lex->text);
        func->nparams++;
        next_token(lex);
        if (lex->kind == T_EQ) {
            next_token(lex);
            parse_expr(lex);
        }
        if (lex->kind == T_COMMA) next_token(lex);
        else break;
    }
    if (lex->kind != T_RPAREN) fail(lex, "expected ')'");
    next_token(lex);
}

static Func *new_func(const char *name, int line) {
    Func *func = xmalloc(sizeof(Func));
    func->name = xdup(name);
    func->ret = "Void";
    func->line = line;
    func->file = current_file;
    return func;
}

static void add_method(Class *class, Func *func) {
    if (class->nmethods + 1 > class->mcap) {
        class->mcap = class->mcap == 0 ? 4 : class->mcap * 2;
        class->methods = realloc(class->methods, (size_t)class->mcap * sizeof(Func));
    }
    func->owner = class->name;
    class->methods[class->nmethods++] = *func;
}

static void add_func(Func *func) {
    if (nfuncs + 1 > fcap) {
        fcap = fcap == 0 ? 8 : fcap * 2;
        funcs = realloc(funcs, (size_t)fcap * sizeof(Func));
    }
    funcs[nfuncs++] = *func;
}

static Class *find_class(const char *name) {
    for (int index = 0; index < nclasses; index++) {
        if (same(classes[index].name, name)) return &classes[index];
    }
    return NULL;
}

static Class *ensure_class(const char *name) {
    Class *found = find_class(name);
    if (found) return found;
    if (nclasses + 1 > ccap) {
        ccap = ccap == 0 ? 8 : ccap * 2;
        classes = realloc(classes, (size_t)ccap * sizeof(Class));
        if (classes == NULL) die("out of memory");
    }
    memset(&classes[nclasses], 0, sizeof(Class));
    classes[nclasses].name = xdup(name);
    return &classes[nclasses++];
}

static void parse_function(Lex *lex, Class *owner, int is_extern, int is_ctor) {
    int line = lex->tok_line;
    char *name = owner ? owner->name : "";
    if (!is_ctor) {
        next_token(lex);
        if (lex->kind != T_IDENT) fail(lex, "expected a function name");
        name = xdup(lex->text);
        next_token(lex);
        if (lex->kind == T_LT) {
            int depth = 1;
            next_token(lex);
            while (lex->kind != T_EOF && depth > 0) {
                if (lex->kind == T_LT) depth++;
                else if (lex->kind == T_GT) depth--;
                if (depth > 0) next_token(lex);
            }
            if (lex->kind == T_GT) next_token(lex);
        }
    } else {
        next_token(lex);
        name = owner->name;
    }
    Func *func = new_func(name, line);
    func->is_extern = is_extern;
    func->is_ctor = is_ctor;
    func->owner = owner ? owner->name : NULL;
    parse_params(lex, func);
    if (lex->kind == T_ARROW) {
        next_token(lex);
        char *ret = parse_type(lex);
        func->ret = ret ? ret : "Void";
    } else if (is_ctor) {
        func->ret = "Void";
    }
    if (is_word(lex, "Explain")) {
        next_token(lex);
        if (lex->kind == T_LBRACE) skip_braces(lex);
    }
    if (lex->kind == T_LBRACE) func->body = parse_block(lex);
    else if (lex->kind == T_SEMI || is_extern) {
        if (lex->kind == T_SEMI) next_token(lex);
    } else fail(lex, "expected a function body");
    if (owner) add_method(owner, func);
    else add_func(func);
}

static void parse_enum(Lex *lex, Class *class) {
    if (lex->kind != T_LBRACE) fail(lex, "expected '{'");
    next_token(lex);
    int next_value = 0;
    while (lex->kind != T_RBRACE && lex->kind != T_EOF) {
        if (lex->kind != T_IDENT) fail(lex, "expected an enum name");
        EnumItem item;
        item.name = xdup(lex->text);
        item.value = next_value;
        next_token(lex);
        if (lex->kind == T_EQ) {
            next_token(lex);
            int sign = 1;
            if (lex->kind == T_MINUS) {
                sign = -1;
                next_token(lex);
            }
            if (lex->kind != T_INT) fail(lex, "expected an enum value");
            item.value = sign * (int)lex->ival;
            next_value = item.value;
            next_token(lex);
        }
        if (lex->kind == T_LPAREN) {
            int depth = 1;
            next_token(lex);
            while (lex->kind != T_EOF && depth > 0) {
                if (lex->kind == T_LPAREN) depth++;
                else if (lex->kind == T_RPAREN) depth--;
                if (depth > 0) next_token(lex);
            }
            if (lex->kind == T_RPAREN) next_token(lex);
        }
        next_value++;
        if (class->nitems + 1 > class->icap) {
            class->icap = class->icap == 0 ? 8 : class->icap * 2;
            class->items = realloc(class->items, (size_t)class->icap * sizeof(EnumItem));
        }
        class->items[class->nitems++] = item;
        if (lex->kind == T_COMMA) next_token(lex);
    }
    if (lex->kind != T_RBRACE) fail(lex, "expected '}'");
    next_token(lex);
}

static void add_field(Class *class, Field field) {
    for (int index = 0; index < class->nfields; index++) {
        if (same(class->fields[index].name, field.name)) return;
    }
    if (class->nfields + 1 > class->fcap) {
        class->fcap = class->fcap == 0 ? 4 : class->fcap * 2;
        class->fields = realloc(class->fields, (size_t)class->fcap * sizeof(Field));
    }
    class->fields[class->nfields++] = field;
}

static void parse_class(Lex *lex, int is_enum) {
    int line = lex->tok_line;
    next_token(lex);
    if (lex->kind != T_IDENT) fail(lex, "expected a type name");
    Class *class = ensure_class(lex->text);
    class->is_enum = is_enum;
    (void)line;
    next_token(lex);
    if (lex->kind == T_LT) {
        int depth = 1;
        next_token(lex);
        while (lex->kind != T_EOF && depth > 0) {
            if (lex->kind == T_LT) depth++;
            else if (lex->kind == T_GT) depth--;
            if (depth > 0) next_token(lex);
        }
        if (lex->kind == T_GT) next_token(lex);
    }
    while (lex->kind == T_COLON || is_word(lex, "Implements")) {
        next_token(lex);
        while (lex->kind == T_IDENT || lex->kind == T_DOT || lex->kind == T_LT || lex->kind == T_GT || lex->kind == T_COMMA || lex->kind == T_Q) {
            if (lex->kind == T_LT) {
                int depth = 1;
                next_token(lex);
                while (lex->kind != T_EOF && depth > 0) {
                    if (lex->kind == T_LT) depth++;
                    else if (lex->kind == T_GT) depth--;
                    if (depth > 0) next_token(lex);
                }
            }
            next_token(lex);
            if (lex->kind == T_COMMA) continue;
            if (lex->kind == T_IDENT) continue;
            break;
        }
    }
    if (is_enum) {
        parse_enum(lex, class);
        return;
    }
    if (lex->kind != T_LBRACE) fail(lex, "expected '{'");
    next_token(lex);
    while (lex->kind != T_RBRACE && lex->kind != T_EOF) {
        if (lex->kind == T_SEMI) {
            next_token(lex);
            continue;
        }
        int is_extern = 0;
        while (lex->kind == T_IDENT && is_modifier(lex->text)) {
            if (same(lex->text, "Extern")) is_extern = 1;
            next_token(lex);
        }
        if (is_word(lex, "Constructor")) parse_function(lex, class, 0, 1);
        else if (is_word(lex, "Function")) parse_function(lex, class, is_extern, 0);
        else if (is_word(lex, "Extern")) {
            next_token(lex);
            parse_function(lex, class, 1, 0);
        } else if (is_word(lex, "Class") || is_word(lex, "Struct") || is_word(lex, "Enum")) {
            parse_class(lex, is_word(lex, "Enum"));
        } else {
            int field_line = lex->tok_line;
            char *type = parse_type(lex);
            if (type == NULL || lex->kind != T_IDENT) fail(lex, "expected a field");
            Field field;
            field.type = type;
            field.name = xdup(lex->text);
            field.init = NULL;
            next_token(lex);
            if (lex->kind == T_EQ) {
                next_token(lex);
                field.init = parse_expr(lex);
            }
            if (lex->kind == T_SEMI) next_token(lex);
            else fail(lex, "expected ';' after the field");
            (void)field_line;
            add_field(class, field);
        }
    }
    if (lex->kind != T_RBRACE) fail(lex, "expected '}'");
    next_token(lex);
}

static void skip_until_semi(Lex *lex) {
    while (lex->kind != T_SEMI && lex->kind != T_EOF && lex->kind != T_LBRACE) next_token(lex);
    if (lex->kind == T_SEMI) next_token(lex);
}

static void parse_item(Lex *lex) {
    int is_extern = 0;
    while (lex->kind == T_IDENT && is_modifier(lex->text)) {
        if (same(lex->text, "Extern")) is_extern = 1;
        next_token(lex);
    }
    if (is_word(lex, "Package")) {
        next_token(lex);
        while (lex->kind == T_IDENT || lex->kind == T_DOT) next_token(lex);
        if (lex->kind == T_LBRACE) {
            next_token(lex);
            while (lex->kind != T_RBRACE && lex->kind != T_EOF) {
                if (lex->kind == T_SEMI) next_token(lex);
                else parse_item(lex);
            }
            if (lex->kind == T_RBRACE) next_token(lex);
        } else if (lex->kind == T_SEMI) next_token(lex);
        return;
    }
    if (is_word(lex, "Import")) {
        skip_until_semi(lex);
        return;
    }
    if (is_word(lex, "Permissions")) {
        next_token(lex);
        if (lex->kind == T_LBRACE) skip_braces(lex);
        return;
    }
    if (is_word(lex, "Explain")) {
        next_token(lex);
        if (lex->kind == T_LBRACE) skip_braces(lex);
        return;
    }
    if (is_word(lex, "Enum")) {
        parse_class(lex, 1);
        return;
    }
    if (is_word(lex, "Class") || is_word(lex, "Struct") || is_word(lex, "Interface") || is_word(lex, "Trait") || is_word(lex, "Service")) {
        parse_class(lex, 0);
        return;
    }
    if (is_word(lex, "Extern")) {
        next_token(lex);
        if (!is_word(lex, "Function")) fail(lex, "expected 'Function' after Extern");
        parse_function(lex, NULL, 1, 0);
        return;
    }
    if (is_word(lex, "Function")) {
        parse_function(lex, NULL, is_extern, 0);
        return;
    }
    fail(lex, "expected a declaration");
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "rxc: cannot read %s\n", path);
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = xmalloc((size_t)size + 1);
    fread(text, 1, (size_t)size, file);
    fclose(file);
    return text;
}

static void parse_file(const char *path) {
    Lex lex;
    memset(&lex, 0, sizeof(lex));
    lex.path = path;
    lex.src = read_file(path);
    lex.n = (int)strlen(lex.src);
    lex.line = 1;
    lex.col = 1;
    current_file = xdup(path);
    next_token(&lex);
    while (lex.kind != T_EOF) {
        if (lex.kind == T_SEMI) {
            next_token(&lex);
            continue;
        }
        parse_item(&lex);
    }
    fprintf(stderr, "parsed %s\n", path);
}

static int is_float_type(const char *type) {
    return same(type, "Float") || same(type, "Double");
}

static int is_enum(const char *type) {
    Class *class = find_class(type);
    return class && class->is_enum;
}

static int is_scalar(const char *type) {
    if (type == NULL || type[0] == 0) return 1;
    if (same(type, "Int") || same(type, "Bool") || same(type, "Char") || same(type, "Byte") || same(type, "UInt") || same(type, "Void") || same(type, "Null") || same(type, "Range")) return 1;
    if (is_float_type(type)) return 1;
    if (is_enum(type)) return 1;
    return 0;
}

static int is_string(const char *type) { return same(type, "String"); }
static int is_list(const char *type) { return starts(type, "List:"); }

static char *elem_type(const char *type) {
    if (!is_list(type)) return "Int";
    return xdup(type + 5);
}

static char *c_type(const char *type) {
    if (is_float_type(type)) return "double";
    if (is_scalar(type) || same(type, "Null")) return "int64_t";
    if (is_string(type)) return "char *";
    if (is_list(type)) return "RxList *";
    return fmt("%s *", c_name(type));
}

static int enum_value(const char *type, const char *name) {
    Class *class = find_class(type);
    if (class == NULL) return -1;
    for (int index = 0; index < class->nitems; index++) {
        if (same(class->items[index].name, name)) return class->items[index].value;
    }
    return -1;
}

static int exit_code(const char *name) {
    if (same(name, "Success") || same(name, "SuccessfulExecution")) return 0;
    if (same(name, "WarningExecution")) return 1;
    if (same(name, "ErrorExecution")) return 2;
    if (same(name, "FatalErrorExecution")) return 3;
    if (same(name, "InvalidArguments")) return 4;
    if (same(name, "PermissionDenied")) return 5;
    if (same(name, "ResourceUnavailable")) return 6;
    return -1;
}

static Field *find_field(const char *class_name, const char *field_name) {
    Class *class = find_class(class_name);
    if (class == NULL) return NULL;
    for (int index = 0; index < class->nfields; index++) {
        if (same(class->fields[index].name, field_name)) return &class->fields[index];
    }
    return NULL;
}

static Func *find_method(const char *class_name, const char *name, int arity) {
    Class *class = find_class(class_name);
    if (class == NULL) return NULL;
    Func *fallback = NULL;
    for (int index = 0; index < class->nmethods; index++) {
        if (!same(class->methods[index].name, name) || class->methods[index].is_ctor) continue;
        if (fallback == NULL) fallback = &class->methods[index];
        if (class->methods[index].nparams == arity) return &class->methods[index];
    }
    return fallback;
}

static Func *find_ctor(const char *class_name, int arity) {
    Class *class = find_class(class_name);
    if (class == NULL) return NULL;
    for (int index = 0; index < class->nmethods; index++) {
        if (class->methods[index].is_ctor && class->methods[index].nparams == arity) return &class->methods[index];
    }
    return NULL;
}

static Func *find_free(const char *name, int arity) {
    Func *fallback = NULL;
    for (int index = 0; index < nfuncs; index++) {
        if (!same(funcs[index].name, name)) continue;
        if (fallback == NULL) fallback = &funcs[index];
        if (funcs[index].nparams == arity) return &funcs[index];
    }
    return fallback;
}

static Var *vars;
static int nvars, vcap, depth;
static FILE *out;
static char *ret_type;

static void push_scope(void) { depth++; }
static void pop_scope(void) {
    while (nvars > 0 && vars[nvars - 1].depth == depth) nvars--;
    depth--;
}

static void add_var(const char *name, const char *type) {
    if (nvars + 1 > vcap) {
        vcap = vcap == 0 ? 32 : vcap * 2;
        vars = realloc(vars, (size_t)vcap * sizeof(Var));
    }
    vars[nvars].name = xdup(name);
    vars[nvars].type = xdup(type ? type : "Int");
    vars[nvars].depth = depth;
    nvars++;
}

static Var *find_var(const char *name) {
    for (int index = nvars - 1; index >= 0; index--) {
        if (same(vars[index].name, name)) return &vars[index];
    }
    return NULL;
}

static void emit(const char *text) { fputs(text, out); }
static void emitf(const char *spec, ...) {
    va_list args;
    va_start(args, spec);
    vfprintf(out, spec, args);
    va_end(args);
}

static char *c_string(const char *text) {
    size_t length = strlen(text);
    char *copy = xmalloc(length * 4 + 3);
    int out_index = 0;
    copy[out_index++] = '"';
    for (size_t index = 0; index < length; index++) {
        unsigned char character = (unsigned char)text[index];
        if (character == '\\' || character == '"') {
            copy[out_index++] = '\\';
            copy[out_index++] = (char)character;
        } else if (character == '\n') {
            copy[out_index++] = '\\';
            copy[out_index++] = 'n';
        } else if (character == '\r') {
            copy[out_index++] = '\\';
            copy[out_index++] = 'r';
        } else if (character == '\t') {
            copy[out_index++] = '\\';
            copy[out_index++] = 't';
        } else if (character < 32 || character > 126) {
            out_index += sprintf(copy + out_index, "\\%03o", character);
        } else copy[out_index++] = (char)character;
    }
    copy[out_index++] = '"';
    copy[out_index] = 0;
    return copy;
}

static char *box_code(Expr expr) {
    if (is_string(expr.type) || is_list(expr.type) || !is_scalar(expr.type)) return fmt("RXL(%s)", expr.code);
    if (is_float_type(expr.type)) return fmt("(int64_t)%s", expr.code);
    return xdup(expr.code);
}

static char *unbox_code(const char *type, const char *code) {
    if (is_string(type)) return fmt("RXS(%s)", code);
    if (is_list(type)) return fmt("((RxList *)(uintptr_t)(%s))", code);
    if (!is_scalar(type) && !same(type, "Null")) return fmt("((%s)(uintptr_t)(%s))", c_type(type), code);
    if (is_float_type(type)) return fmt("(double)(%s)", code);
    return xdup(code);
}

static void note_missing(int line, const char *message) {
    fprintf(stderr, "%s:%d: %s\n", current_file ? current_file : "<codegen>", line, message);
    errors++;
}

static Expr gen_expr(Node *value);

static char *call_args(Node *call, int start) {
    char *text = xdup("");
    for (int index = start; index < call->nkids; index++) {
        Expr arg = gen_expr(call->kids[index]);
        text = fmt("%s%s%s", text, index == start ? "" : ", ", arg.code);
    }
    return text;
}

static char *boxed_args(Node *call, int start) {
    char *text = xdup("");
    for (int index = start; index < call->nkids; index++) {
        Expr arg = gen_expr(call->kids[index]);
        text = fmt("%s%s%s", text, index == start ? "" : ", ", box_code(arg));
    }
    return text;
}

static Expr expr(const char *code, const char *type) {
    Expr value;
    value.code = xdup(code);
    value.type = xdup(type ? type : "Int");
    return value;
}

static Expr gen_call(Node *call) {
    Node *callee = call->a;
    int arity = call->nkids;
    if (callee && callee->kind == N_NAME) {
        if (same(callee->text, "Print")) {
            if (arity == 0) return expr("(fwrite(\"\\n\", 1, 1, stdout), (int64_t)0)", "Void");
            Expr arg = gen_expr(call->kids[0]);
            int kind = 0;
            if (is_string(arg.type)) kind = 1;
            else if (same(arg.type, "Bool")) kind = 2;
            else if (same(arg.type, "Char")) kind = 3;
            return expr(fmt("(rx_print(%s, %d), (int64_t)0)", box_code(arg), kind), "Void");
        }
        if (same(callee->text, "List")) {
            char *type = callee->type[0] ? callee->type : "List:Int";
            return expr("((RxList *)(uintptr_t)rx_list_new())", type);
        }
        if (same(callee->text, "RunProcess")) {
            Expr arg = arity ? gen_expr(call->kids[0]) : expr("NULL", "String");
            return expr(fmt("rx_process(%s)", box_code(arg)), "Int");
        }
        if (find_class(callee->text) && !find_class(callee->text)->is_enum) {
            Func *ctor = find_ctor(callee->text, arity);
            char *args = xdup("");
            for (int index = 0; index < arity; index++) {
                Expr arg = gen_expr(call->kids[index]);
                char *expected = ctor && index < ctor->nparams ? ctor->params[index].type : arg.type;
                char *code = arg.code;
                if (same(arg.type, "Null") && !is_scalar(expected)) code = "NULL";
                args = fmt("%s%s%s", args, index ? ", " : "", code);
            }
            return expr(fmt("%s_new_%d(%s)", c_name(callee->text), arity, args), callee->text);
        }
        Func *func = find_free(callee->text, arity);
        if (func) {
            char *args = xdup("");
            for (int index = 0; index < arity; index++) {
                Expr arg = gen_expr(call->kids[index]);
                char *expected = index < func->nparams ? func->params[index].type : arg.type;
                char *code = same(arg.type, "Null") && !is_scalar(expected) ? "NULL" : arg.code;
                args = fmt("%s%s%s", args, index ? ", " : "", code);
            }
            return expr(fmt("%s(%s)", func->cname, args), func->ret);
        }
        note_missing(call->line, fmt("unknown function %s", callee->text));
        return expr("0", "Int");
    }
    if (callee && callee->kind == N_MEMBER) {
        Node *owner = callee->a;
        if (owner && owner->kind == N_NAME && find_var(owner->text) == NULL) {
            if (same(owner->text, "String") && same(callee->text, "From")) {
                Expr arg = arity ? gen_expr(call->kids[0]) : expr("0", "Int");
                const char *fn = "rx_str_from_int";
                if (same(arg.type, "Char")) fn = "rx_str_from_char";
                else if (same(arg.type, "Bool")) fn = "rx_str_from_bool";
                else if (is_string(arg.type)) return expr(arg.code, "String");
                return expr(fmt("RXS(%s(%s))", fn, box_code(arg)), "String");
            }
            if (same(owner->text, "String") && same(callee->text, "FromChars")) {
                Expr arg = arity ? gen_expr(call->kids[0]) : expr("0", "List:Char");
                return expr(fmt("RXS(rx_str_from_chars(%s))", box_code(arg)), "String");
            }
            if (same(owner->text, "Char") && same(callee->text, "From")) {
                Expr arg = arity ? gen_expr(call->kids[0]) : expr("0", "Int");
                return expr(fmt("(int64_t)(%s)", arg.code), "Char");
            }
            if (same(owner->text, "Byte") && same(callee->text, "From")) {
                Expr arg = arity ? gen_expr(call->kids[0]) : expr("0", "Int");
                return expr(fmt("((int64_t)(%s) & 255)", arg.code), "Byte");
            }
            if (same(owner->text, "ExitCode") && same(callee->text, "Custom")) {
                Expr arg = arity ? gen_expr(call->kids[0]) : expr("0", "Int");
                return expr(arg.code, "Int");
            }
            if (same(owner->text, "Files") || same(owner->text, "Directory") || same(owner->text, "Environment") || same(owner->text, "Process")) {
                Expr a = arity > 0 ? gen_expr(call->kids[0]) : expr("NULL", "String");
                Expr b = arity > 1 ? gen_expr(call->kids[1]) : expr("NULL", "String");
                if (same(owner->text, "Files") && same(callee->text, "ReadText")) return expr(fmt("RXS(rx_file_read(%s))", box_code(a)), "String");
                if (same(owner->text, "Files") && same(callee->text, "WriteText")) return expr(fmt("(rx_file_write_text(%s, %s), (int64_t)0)", box_code(a), box_code(b)), "Void");
                if (same(owner->text, "Files") && same(callee->text, "WriteBytes")) return expr(fmt("(rx_file_write_bytes(%s, %s), (int64_t)0)", box_code(a), box_code(b)), "Void");
                if (same(owner->text, "Files") && same(callee->text, "Exists")) return expr(fmt("rx_file_exists(%s)", box_code(a)), "Bool");
                if (same(owner->text, "Files") && same(callee->text, "Delete")) return expr(fmt("(rx_file_delete(%s), (int64_t)0)", box_code(a)), "Void");
                if (same(owner->text, "Directory") && same(callee->text, "Create")) return expr(fmt("(rx_dir_create(%s), (int64_t)0)", box_code(a)), "Void");
                if (same(owner->text, "Directory") && same(callee->text, "Delete")) return expr(fmt("(rx_dir_delete(%s), (int64_t)0)", box_code(a)), "Void");
                if (same(owner->text, "Directory") && same(callee->text, "Exists")) return expr(fmt("rx_dir_exists(%s)", box_code(a)), "Bool");
                if (same(owner->text, "Directory") && same(callee->text, "Files")) return expr(fmt("((RxList *)(uintptr_t)rx_dir_files(%s))", box_code(a)), "List:String");
                if (same(owner->text, "Environment") && same(callee->text, "Get")) return expr(fmt("RXS(rx_env_get(%s))", box_code(a)), "String");
                if (same(owner->text, "Process") && same(callee->text, "Run")) return expr(fmt("rx_process(%s)", box_code(a)), "Int");
            }
        }
        Expr receiver = gen_expr(owner);
        if (is_string(receiver.type)) {
            Expr a = arity > 0 ? gen_expr(call->kids[0]) : expr("0", "Int");
            Expr b = arity > 1 ? gen_expr(call->kids[1]) : expr("0", "Int");
            char *self = box_code(receiver);
            if (same(callee->text, "Length")) return expr(fmt("rx_str_len(%s)", self), "Int");
            if (same(callee->text, "ToLower")) return expr(fmt("RXS(rx_str_lower(%s))", self), "String");
            if (same(callee->text, "ToUpper")) return expr(fmt("RXS(rx_str_upper(%s))", self), "String");
            if (same(callee->text, "StartsWith")) return expr(fmt("rx_str_starts(%s, %s)", self, box_code(a)), "Bool");
            if (same(callee->text, "EndsWith")) return expr(fmt("rx_str_ends(%s, %s)", self, box_code(a)), "Bool");
            if (same(callee->text, "Contains")) return expr(fmt("rx_str_contains(%s, %s)", self, box_code(a)), "Bool");
            if (same(callee->text, "IndexOf")) {
                char *start = arity > 1 ? a.code : "0";
                char *needle = arity > 1 ? box_code(gen_expr(call->kids[0])) : box_code(a);
                if (arity > 1) start = b.code;
                return expr(fmt("rx_str_index(%s, %s, %s)", self, arity ? box_code(gen_expr(call->kids[0])) : box_code(expr("NULL", "String")), arity > 1 ? b.code : "0"), "Int");
                (void)needle;
                (void)start;
            }
            if (same(callee->text, "LastIndexOf")) return expr(fmt("rx_str_rindex(%s, %s, %s)", self, box_code(a), arity > 1 ? b.code : "-1"), "Int");
            if (same(callee->text, "GetChar")) return expr(fmt("rx_str_get(%s, %s)", self, a.code), "Char");
            if (same(callee->text, "ToInt")) return expr(fmt("rx_str_to_int(%s)", self), "Int");
            if (same(callee->text, "ToString")) return expr(receiver.code, "String");
            if (same(callee->text, "Replace")) return expr(fmt("RXS(rx_str_replace(%s, %s, %s))", self, box_code(a), box_code(b)), "String");
            if (same(callee->text, "Substring")) {
                if (arity > 1) return expr(fmt("RXS(rx_str_substr(%s, %s, %s))", self, a.code, b.code), "String");
                return expr(fmt("RXS(rx_str_substr(%s, %s, rx_str_len(%s) - (%s)))", self, a.code, self, a.code), "String");
            }
        }
        if (is_list(receiver.type)) {
            Expr a = arity > 0 ? gen_expr(call->kids[0]) : expr("0", "Int");
            if (same(callee->text, "Add")) return expr(fmt("(rx_list_add(%s, %s), (int64_t)0)", box_code(receiver), box_code(a)), "Void");
            if (same(callee->text, "Count")) return expr(fmt("rx_list_count(%s)", box_code(receiver)), "Int");
            if (same(callee->text, "ToArray")) return expr(receiver.code, receiver.type);
        }
        if (same(callee->text, "ToInt")) return expr(fmt("(int64_t)(%s)", receiver.code), "Int");
        if (same(callee->text, "ToFloat")) {
            if (is_string(receiver.type)) return expr(fmt("rx_str_to_double(RXL(%s))", receiver.code), "Float");
            return expr(fmt("(double)(%s)", receiver.code), "Float");
        }
        if (same(callee->text, "ToString")) {
            if (same(receiver.type, "Bool")) return expr(fmt("RXS(rx_str_from_bool(%s))", receiver.code), "String");
            if (same(receiver.type, "Char")) return expr(fmt("RXS(rx_str_from_char(%s))", receiver.code), "String");
            if (is_string(receiver.type)) return expr(receiver.code, "String");
            return expr(fmt("RXS(rx_str_from_int((int64_t)(%s)))", receiver.code), "String");
        }
        if (!is_scalar(receiver.type) && !is_string(receiver.type) && !is_list(receiver.type)) {
            Func *method = find_method(receiver.type, callee->text, arity);
            if (method) {
                char *args = xdup(receiver.code);
                for (int index = 0; index < arity; index++) {
                    Expr arg = gen_expr(call->kids[index]);
                    char *expected = index < method->nparams ? method->params[index].type : arg.type;
                    char *code = same(arg.type, "Null") && !is_scalar(expected) ? "NULL" : arg.code;
                    args = fmt("%s, %s", args, code);
                }
                return expr(fmt("%s(%s)", method->cname, args), method->ret);
            }
        }
        if (same(callee->text, "Count")) return expr(fmt("rx_list_count(%s)", box_code(receiver)), "Int");
        if (same(callee->text, "Length")) return expr(fmt("rx_str_len(%s)", box_code(receiver)), "Int");
        if (same(callee->text, "Add") && arity) {
            Expr arg = gen_expr(call->kids[0]);
            return expr(fmt("(rx_list_add(%s, %s), (int64_t)0)", box_code(receiver), box_code(arg)), "Void");
        }
        note_missing(call->line, fmt("unknown call %s.%s", receiver.type, callee->text));
        return expr("0", "Int");
    }
    note_missing(call->line, "unknown call");
    return expr("0", "Int");
}

static Expr gen_expr(Node *value) {
    if (value == NULL) return expr("0", "Int");
    if (value->kind == N_INT) return expr(value->text, "Int");
    if (value->kind == N_FLOAT) return expr(value->text, "Float");
    if (value->kind == N_STR) return expr(c_string(value->text), "String");
    if (value->kind == N_CHAR) return expr(fmt("(int64_t)%d", value->ival), "Char");
    if (value->kind == N_BOOL) return expr(value->ival ? "(int64_t)1" : "(int64_t)0", "Bool");
    if (value->kind == N_NULL) return expr("0", "Null");
    if (value->kind == N_NAME) {
        if (same(value->text, "This")) {
            Var *self = find_var("This");
            return expr("This", self ? self->type : "Int");
        }
        Var *var = find_var(value->text);
        if (var) return expr(c_name(value->text), var->type);
        if (find_class(value->text)) return expr("0", value->text);
        note_missing(value->line, fmt("unknown name %s", value->text));
        return expr("0", "Int");
    }
    if (value->kind == N_MEMBER) {
        if (value->a && value->a->kind == N_NAME && find_var(value->a->text) == NULL) {
            int ordinal = enum_value(value->a->text, value->text);
            if (ordinal >= 0) return expr(fmt("(int64_t)%s_%s", c_name(value->a->text), c_name(value->text)), "Int");
            if (same(value->a->text, "ExitCode")) {
                int code = exit_code(value->text);
                if (code >= 0) return expr(fmt("(int64_t)%d", code), "Int");
            }
        }
        Expr receiver = gen_expr(value->a);
        Field *field = find_field(receiver.type, value->text);
        if (field) return expr(fmt("(%s)->%s", receiver.code, c_name(field->name)), field->type);
        if (is_enum(receiver.type)) return expr(receiver.code, "Int");
        note_missing(value->line, fmt("unknown field %s.%s", receiver.type, value->text));
        return expr("0", "Int");
    }
    if (value->kind == N_INDEX) {
        Expr collection = gen_expr(value->a);
        Expr index = gen_expr(value->b);
        if (is_string(collection.type)) return expr(fmt("rx_str_get(RXL(%s), %s)", collection.code, index.code), "Char");
        char *element = elem_type(collection.type);
        char *loaded = fmt("rx_list_get(RXL(%s), %s)", collection.code, index.code);
        return expr(unbox_code(element, loaded), element);
    }
    if (value->kind == N_CALL) return gen_call(value);
    if (value->kind == N_UN) {
        Expr inner = gen_expr(value->a);
        if (same(value->text, "!")) return expr(fmt("(int64_t)!(%s)", inner.code), "Bool");
        if (same(value->text, "-")) return expr(fmt("-(%s)", inner.code), inner.type);
        return expr(inner.code, inner.type);
    }
    if (value->kind == N_BIN) {
        if (same(value->text, "&&") || same(value->text, "||")) {
            Expr left = gen_expr(value->a);
            Expr right = gen_expr(value->b);
            return expr(fmt("(int64_t)((%s) %s (%s))", left.code, value->text, right.code), "Bool");
        }
        Expr left = gen_expr(value->a);
        Expr right = gen_expr(value->b);
        if (same(value->text, "+") && (is_string(left.type) || is_string(right.type))) {
            char *lc = is_string(left.type) ? fmt("RXL(%s)", left.code) : (same(left.type, "Bool") ? fmt("rx_str_from_bool(%s)", left.code) : same(left.type, "Char") ? fmt("rx_str_from_char(%s)", left.code) : fmt("rx_str_from_int((int64_t)(%s))", left.code));
            char *rc = is_string(right.type) ? fmt("RXL(%s)", right.code) : (same(right.type, "Bool") ? fmt("rx_str_from_bool(%s)", right.code) : same(right.type, "Char") ? fmt("rx_str_from_char(%s)", right.code) : fmt("rx_str_from_int((int64_t)(%s))", right.code));
            return expr(fmt("RXS(rx_str_concat(%s, %s))", lc, rc), "String");
        }
        if (same(value->text, "==") || same(value->text, "!=")) {
            int string_cmp = is_string(left.type) || is_string(right.type);
            if (string_cmp && !same(left.type, "Null") && !same(right.type, "Null")) {
                const char *op = same(value->text, "!=") ? "!" : "";
                return expr(fmt("(int64_t)(%srx_str_eq(RXL(%s), RXL(%s)))", op, left.code, right.code), "Bool");
            }
            return expr(fmt("(int64_t)((%s) %s (%s))", left.code, value->text, right.code), "Bool");
        }
        if (is_float_type(left.type) || is_float_type(right.type)) {
            return expr(fmt("((double)(%s) %s (double)(%s))", left.code, value->text, right.code), "Float");
        }
        if (same(value->text, "+") || same(value->text, "-") || same(value->text, "*") || same(value->text, "/") || same(value->text, "%")) {
            return expr(fmt("((%s) %s (%s))", left.code, value->text, right.code), "Int");
        }
        return expr(fmt("(int64_t)((%s) %s (%s))", left.code, value->text, right.code), "Bool");
    }
    if (value->kind == N_RANGE) return expr("0", "Range");
    return expr("0", "Void");
}

static void emit_stmt(Node *stmt);
static void emit_stmt_body(Node *stmt);

static void emit_owned_store(const char *type, const char *slot, const char *code) {
    if (is_scalar(type)) {
        emitf("%s = %s;\n", slot, code);
        return;
    }
    emit("{\n");
    emitf("%s rx_next = %s;\n", c_type(type), code);
    emitf("%s *rx_slot = &(%s);\n", c_type(type), slot);
    emit("rx_keep(RXL(rx_next));\n");
    emit("if (rx_next != *rx_slot) rx_release(RXL(*rx_slot));\n");
    emit("*rx_slot = rx_next;\n");
    emit("}\n");
}

static void emit_assign(Node *stmt) {
    const char *op = stmt->text;
    Expr right = gen_expr(stmt->b);
    Node *left = stmt->a;
    if (left && left->kind == N_NAME) {
        Var *var = find_var(left->text);
        char *name = c_name(left->text);
        char *code = same(right.type, "Null") && var && !is_scalar(var->type) ? "NULL" : right.code;
        if (same(op, "=") && var) emit_owned_store(var->type, name, code);
        else if (same(op, "=")) emitf("%s = %s;\n", name, code);
        else emitf("%s = %s %c (%s);\n", name, name, op[0], code);
        return;
    }
    if (left && left->kind == N_MEMBER) {
        Expr receiver = gen_expr(left->a);
        Field *field = find_field(receiver.type, left->text);
        char *field_name = field ? c_name(field->name) : c_name(left->text);
        char *code = right.code;
        if (field && same(right.type, "Null") && !is_scalar(field->type)) code = "NULL";
        char *slot = fmt("(%s)->%s", receiver.code, field_name);
        if (same(op, "=") && field) emit_owned_store(field->type, slot, code);
        else if (same(op, "=")) emitf("%s = %s;\n", slot, code);
        else emitf("(%s)->%s = (%s)->%s %c (%s);\n", receiver.code, field_name, receiver.code, field_name, op[0], code);
        return;
    }
    if (left && left->kind == N_INDEX) {
        Expr collection = gen_expr(left->a);
        Expr index = gen_expr(left->b);
        if (same(op, "=")) emitf("rx_list_set(RXL(%s), %s, %s);\n", collection.code, index.code, box_code(right));
        else emitf("rx_list_set(RXL(%s), %s, rx_list_get(RXL(%s), %s) %c (%s));\n", collection.code, index.code, collection.code, index.code, op[0], box_code(right));
        return;
    }
    note_missing(stmt->line, "unsupported assignment");
}

static void emit_stmt_body(Node *stmt) {
    if (stmt == NULL || stmt->kind == N_NONE) return;
    if (stmt->line > 0 && current_file) emitf("#line %d \"%s\"\n", stmt->line, current_file);
    if (stmt->kind == N_BLOCK) {
        emit("{\n");
        push_scope();
        for (int index = 0; index < stmt->nkids; index++) emit_stmt(stmt->kids[index]);
        pop_scope();
        emit("}\n");
        return;
    }
    if (stmt->kind == N_VAR) {
        char *type = stmt->type;
        if ((type == NULL || type[0] == 0) && stmt->a) {
            Expr init = gen_expr(stmt->a);
            type = init.type[0] ? init.type : "Int";
            if (same(type, "Null")) type = "Int";
            if (same(type, "Range")) type = "Int";
            add_var(stmt->text, type);
            char *code = same(init.type, "Null") && !is_scalar(type) ? "NULL" : init.code;
            emitf("%s %s = %s;\n", c_type(type), c_name(stmt->text), code);
            if (!is_scalar(type)) emitf("rx_keep(RXL(%s));\n", c_name(stmt->text));
            return;
        }
        if (type == NULL || type[0] == 0) type = "Int";
        add_var(stmt->text, type);
        if (stmt->a) {
            Expr init = gen_expr(stmt->a);
            char *code = same(init.type, "Null") && !is_scalar(type) ? "NULL" : init.code;
            emitf("%s %s = %s;\n", c_type(type), c_name(stmt->text), code);
            if (!is_scalar(type)) emitf("rx_keep(RXL(%s));\n", c_name(stmt->text));
        } else {
            const char *zero = is_scalar(type) ? (is_float_type(type) ? "0.0" : "0") : "NULL";
            emitf("%s %s = %s;\n", c_type(type), c_name(stmt->text), zero);
        }
        return;
    }
    if (stmt->kind == N_IF) {
        Expr cond = gen_expr(stmt->a);
        emitf("if (%s) {\n", cond.code);
        emit_stmt(stmt->b);
        emit("}\n");
        if (stmt->c) {
            emit("else {\n");
            emit_stmt(stmt->c);
            emit("}\n");
        }
        return;
    }
    if (stmt->kind == N_WHILE) {
        Expr cond = gen_expr(stmt->a);
        emitf("while (%s) {\n", cond.code);
        emit_stmt(stmt->b);
        emit("}\n");
        return;
    }
    if (stmt->kind == N_FOR) {
        if (stmt->a && stmt->a->kind == N_RANGE) {
            Expr start = gen_expr(stmt->a->a);
            Expr end = gen_expr(stmt->a->b);
            const char *cmp = same(stmt->a->text, "..=") ? "<=" : "<";
            emit("{\n");
            emitf("int64_t rx_start = %s;\n", start.code);
            emitf("int64_t rx_end = %s;\n", end.code);
            emitf("for (int64_t %s = rx_start; %s %s rx_end; %s++) {\n", c_name(stmt->text), c_name(stmt->text), cmp, c_name(stmt->text));
            push_scope();
            add_var(stmt->text, "Int");
            emit_stmt(stmt->b);
            pop_scope();
            emit("}\n}\n");
            return;
        }
        Expr collection = gen_expr(stmt->a);
        char *element = is_string(collection.type) ? "Char" : elem_type(collection.type);
        emit("{\n");
        emitf("int64_t rx_n = %s;\n", is_list(collection.type) ? fmt("rx_list_count(RXL(%s))", collection.code) : "0");
        emit("for (int64_t rx_i = 0; rx_i < rx_n; rx_i++) {\n");
        char *loaded = is_list(collection.type) ? fmt("rx_list_get(RXL(%s), rx_i)", collection.code) : "0";
        emitf("%s %s = %s;\n", c_type(element), c_name(stmt->text), unbox_code(element, loaded));
        push_scope();
        add_var(stmt->text, element);
        emit_stmt(stmt->b);
        pop_scope();
        emit("}\n}\n");
        return;
    }
    if (stmt->kind == N_RETURN) {
        if (stmt->a == NULL || same(ret_type, "Void") || ret_type == NULL || ret_type[0] == 0) {
            if (stmt->a) gen_expr(stmt->a);
            emit("rx_scope_pop();\n");
            emit("return;\n");
        } else {
            Expr value = gen_expr(stmt->a);
            char *code = same(value.type, "Null") && !is_scalar(ret_type) ? "NULL" : value.code;
            emitf("%s rx_ret = %s;\n", c_type(ret_type), code);
            if (!is_scalar(ret_type)) emit("rx_keep(RXL(rx_ret));\n");
            emit("rx_scope_pop();\n");
            emit("return rx_ret;\n");
        }
        return;
    }
    if (stmt->kind == N_BREAK) { emit("break;\n"); return; }
    if (stmt->kind == N_CONT) { emit("continue;\n"); return; }
    if (stmt->kind == N_EXPR) {
        if (stmt->a && stmt->a->kind == N_ASSIGN) emit_assign(stmt->a);
        else {
            Expr value = gen_expr(stmt->a);
            emitf("(void)(%s);\n", value.code);
        }
        return;
    }
    if (stmt->kind == N_ASSIGN) emit_assign(stmt);
}

static void emit_stmt(Node *stmt) {
    if (stmt == NULL || stmt->kind == N_NONE) return;
    if (stmt->kind == N_BLOCK) {
        emit_stmt_body(stmt);
        return;
    }
    emit("rx_scope_push();\n");
    emit_stmt_body(stmt);
    if (stmt->kind != N_RETURN) emit("rx_scope_pop();\n");
}

static void emit_params(Func *func, int with_this) {
    int first = 1;
    if (with_this) {
        emitf("%s This", c_type(func->owner));
        first = 0;
    }
    for (int index = 0; index < func->nparams; index++) {
        if (!first) emit(", ");
        emitf("%s %s", c_type(func->params[index].type), c_name(func->params[index].name));
        first = 0;
    }
    if (first) emit("void");
}

static void bind_params(Func *func, int with_this) {
    if (with_this) add_var("This", func->owner);
    for (int index = 0; index < func->nparams; index++) add_var(func->params[index].name, func->params[index].type);
}

static void emit_func_body(Func *func, int with_this) {
    current_file = func->file;
    ret_type = func->is_ctor ? "Void" : func->ret;
    push_scope();
    bind_params(func, with_this);
    if (func->body) emit_stmt(func->body);
    if (!func->is_ctor && (same(func->ret, "Void") || func->ret == NULL || func->ret[0] == 0)) emit("return;\n");
    pop_scope();
}

static int class_has_ctor(Class *class, int arity) {
    for (int index = 0; index < class->nmethods; index++) {
        if (class->methods[index].is_ctor && class->methods[index].nparams == arity) return 1;
    }
    return 0;
}

static void emit_extern_func(Func *func) {
    int void_ret = func->ret == NULL || func->ret[0] == 0 || same(func->ret, "Void");
    const char *ret = void_ret ? "void" : c_type(func->ret);
    emitf("int64_t rx_%s(", c_name(func->name));
    for (int index = 0; index < func->nparams; index++) {
        if (index > 0) emit(", ");
        emit("int64_t");
    }
    if (func->nparams == 0) emit("void");
    emit(");\n");
    emitf("static %s %s(", ret, func->cname);
    emit_params(func, 0);
    emit(") {\n    ");
    if (!void_ret) {
        if (is_string(func->ret)) emit("return RXS(");
        else emitf("return (%s)(", ret);
    }
    emitf("rx_%s(", c_name(func->name));
    for (int index = 0; index < func->nparams; index++) {
        if (index > 0) emit(", ");
        const char *param_type = func->params[index].type;
        const char *param_name = c_name(func->params[index].name);
        if (is_string(param_type) || !is_scalar(param_type)) emitf("RXL(%s)", param_name);
        else emitf("(int64_t)%s", param_name);
    }
    emit(")");
    if (!void_ret) emit(")");
    emit(";\n}\n");
}

static void declare_func(Func *func) {
    if (func->is_ctor) {
        emitf("static void %s_ctor_%d(", c_name(func->owner), func->nparams);
        emit_params(func, 1);
        emit(");\n");
        emitf("static %s %s_new_%d(", c_type(func->owner), c_name(func->owner), func->nparams);
        emit_params(func, 0);
        emit(");\n");
        return;
    }
    const char *ret = same(func->ret, "Void") || func->ret[0] == 0 ? "void" : c_type(func->ret);
    emitf("static %s %s(", ret, func->cname);
    emit_params(func, func->owner != NULL);
    emit(");\n");
}

static void define_ctor(Class *class, Func *func) {
    current_file = func->file;
    emitf("#line %d \"%s\"\n", func->line, func->file ? func->file : "");
    emitf("static void %s_ctor_%d(", c_name(class->name), func->nparams);
    emit_params(func, 1);
    emit(") {\n");
    for (int index = 0; index < class->nfields; index++) {
        if (class->fields[index].init) {
            push_scope();
            add_var("This", class->name);
            Expr init = gen_expr(class->fields[index].init);
            pop_scope();
            emitf("This->%s = %s;\n", c_name(class->fields[index].name), init.code);
        }
    }
    if (func->assign_params) {
        for (int index = 0; index < func->nparams && index < class->nfields; index++) {
            emitf("This->%s = %s;\n", c_name(class->fields[index].name), c_name(func->params[index].name));
        }
    }
    emit_func_body(func, 1);
    emit("}\n");
    emitf("static %s %s_new_%d(", c_type(class->name), c_name(class->name), func->nparams);
    emit_params(func, 0);
    emit(") {\n");
    emitf("%s This = calloc(1, sizeof(*This));\n", c_type(class->name));
    emitf("%s_ctor_%d(This", c_name(class->name), func->nparams);
    for (int index = 0; index < func->nparams; index++) emitf(", %s", c_name(func->params[index].name));
    emit(");\nreturn This;\n}\n");
}

static void define_func(Func *func) {
    if (func->is_extern) return;
    current_file = func->file;
    if (func->file) emitf("#line %d \"%s\"\n", func->line, func->file);
    const char *ret = same(func->ret, "Void") || func->ret == NULL || func->ret[0] == 0 ? "void" : c_type(func->ret);
    emitf("static %s %s(", ret, func->cname);
    emit_params(func, func->owner != NULL);
    emit(") {\n");
    emit_func_body(func, func->owner != NULL);
    emit("}\n");
}

static char *stem_of(const char *path) {
    const char *base = path;
    for (const char *cursor = path; *cursor; cursor++) if (*cursor == '/') base = cursor + 1;
    char *text = xdup(base);
    char *dot = strrchr(text, '.');
    if (dot) *dot = 0;
    return text;
}

static void assign_names(const char *entry_path) {
    for (int index = 0; index < nclasses; index++) {
        for (int method = 0; method < classes[index].nmethods; method++) {
            Func *func = &classes[index].methods[method];
            func->owner = classes[index].name;
            if (func->is_ctor) func->cname = fmt("%s_ctor_%d", c_name(classes[index].name), func->nparams);
            else func->cname = fmt("%s_%s", c_name(classes[index].name), c_name(func->name));
        }
    }
    for (int index = 0; index < nfuncs; index++) {
        funcs[index].stem = stem_of(funcs[index].file ? funcs[index].file : "");
        int dup = 0;
        for (int other = 0; other < nfuncs; other++) {
            if (other != index && same(funcs[index].name, funcs[other].name)) dup = 1;
        }
        int is_entry_main = same(funcs[index].name, "Main") && entry_path && funcs[index].file && strcmp(funcs[index].file, entry_path) == 0;
        if (same(funcs[index].name, "RunProcess")) funcs[index].cname = xdup("RunProcess");
        else if (is_entry_main || !dup) funcs[index].cname = c_name(funcs[index].name);
        else funcs[index].cname = fmt("%s_%s", funcs[index].stem, c_name(funcs[index].name));
    }
}

static void synthesize_result(void) {
    if (find_class("ExecutionResult")) return;
    Class *class = ensure_class("ExecutionResult");
    Field fields[3] = {
        {xdup("Code"), xdup("Int"), NULL},
        {xdup("Message"), xdup("String"), NULL},
        {xdup("Details"), xdup("String"), NULL}
    };
    for (int index = 0; index < 3; index++) add_field(class, fields[index]);
    for (int arity = 1; arity <= 3; arity++) {
        Func *func = new_func("ExecutionResult", 1);
        func->is_ctor = 1;
        func->assign_params = 1;
        func->owner = class->name;
        func->ret = "Void";
        func->file = "<runtime>";
        const char *names[] = {"Code", "Message", "Details"};
        const char *types[] = {"Int", "String", "String"};
        for (int index = 0; index < arity; index++) {
            func->params = realloc(func->params, (size_t)(index + 1) * sizeof(Param));
            func->params[index].name = xdup(names[index]);
            func->params[index].type = xdup(types[index]);
            func->nparams = index + 1;
        }
        add_method(class, func);
    }
}

static void emit_program(const char *output_c, const char *entry_path) {
    synthesize_result();
    assign_names(entry_path);
    out = fopen(output_c, "wb");
    if (out == NULL) die("cannot write generated C");
    emit("#include \"rx_runtime.h\"\n#include <stdlib.h>\n#include <stdint.h>\n");
    emit("#define RXL(p) ((int64_t)(uintptr_t)(p))\n#define RXS(v) ((char *)(uintptr_t)(v))\n\n");
    for (int index = 0; index < nclasses; index++) {
        Class *class = &classes[index];
        if (!class->is_enum) continue;
        for (int item = 0; item < class->nitems; item++) {
            emitf("#define %s_%s %d\n", c_name(class->name), c_name(class->items[item].name), class->items[item].value);
        }
    }
    emit("\n");
    for (int index = 0; index < nclasses; index++) {
        if (!classes[index].is_enum) emitf("typedef struct %s %s;\n", c_name(classes[index].name), c_name(classes[index].name));
    }
    emit("\n");
    for (int index = 0; index < nclasses; index++) {
        Class *class = &classes[index];
        if (class->is_enum) continue;
        emitf("struct %s {\n", c_name(class->name));
        for (int field = 0; field < class->nfields; field++) {
            emitf("    %s %s;\n", c_type(class->fields[field].type), c_name(class->fields[field].name));
        }
        if (class->nfields == 0) emit("    int64_t rx_unused;\n");
        emit("};\n");
    }
    emit("\n");
    for (int index = 0; index < nclasses; index++) {
        if (classes[index].is_enum) continue;
        if (!class_has_ctor(&classes[index], 0)) {
            emitf("static %s %s_new_0(void);\n", c_type(classes[index].name), c_name(classes[index].name));
        }
        for (int method = 0; method < classes[index].nmethods; method++) declare_func(&classes[index].methods[method]);
    }
    for (int index = 0; index < nfuncs; index++) {
        if (funcs[index].is_extern && !same(funcs[index].name, "RunProcess")) emit_extern_func(&funcs[index]);
        else declare_func(&funcs[index]);
    }
    emit("static int64_t RunProcess(char *command);\n\n");
    for (int index = 0; index < nclasses; index++) {
        Class *class = &classes[index];
        if (class->is_enum) continue;
        if (!class_has_ctor(class, 0)) {
            emitf("static %s %s_new_0(void) {\n    return calloc(1, sizeof(%s));\n}\n", c_type(class->name), c_name(class->name), c_name(class->name));
        }
        for (int method = 0; method < class->nmethods; method++) {
            Func *func = &class->methods[method];
            if (func->is_ctor) define_ctor(class, func);
            else if (!func->is_extern) define_func(func);
        }
    }
    for (int index = 0; index < nfuncs; index++) define_func(&funcs[index]);
    emit("static int64_t RunProcess(char *command) { return rx_process(RXL(command)); }\n\n");
    Func *entry = NULL;
    for (int index = 0; index < nfuncs; index++) {
        if (same(funcs[index].cname, "Main")) entry = &funcs[index];
    }
    emit("int main(int argc, char **argv) {\n");
    if (entry == NULL) {
        emit("    (void)argc; (void)argv;\n    return 0;\n}\n");
    } else if (entry->nparams == 0) {
        if (same(entry->ret, "ExecutionResult")) emit("    ExecutionResult *rx_result = Main();\n    return rx_result ? (int)rx_result->Code : 0;\n}\n");
        else if (same(entry->ret, "Void") || entry->ret[0] == 0) emit("    Main();\n    return 0;\n}\n");
        else emit("    return (int)Main();\n}\n");
    } else if (same(entry->ret, "ExecutionResult")) {
        emit("    RxList *rx_argv = (RxList *)(uintptr_t)rx_args((int32_t)argc, argv);\n");
        emit("    ExecutionResult *rx_result = Main(rx_argv);\n");
        emit("    return rx_result ? (int)rx_result->Code : 0;\n}\n");
    } else if (same(entry->ret, "Void") || entry->ret[0] == 0) {
        emit("    RxList *rx_argv = (RxList *)(uintptr_t)rx_args((int32_t)argc, argv);\n");
        emit("    Main(rx_argv);\n    return 0;\n}\n");
    } else {
        emit("    RxList *rx_argv = (RxList *)(uintptr_t)rx_args((int32_t)argc, argv);\n");
        emit("    return (int)Main(rx_argv);\n}\n");
    }
    fclose(out);
}

static char *quote(const char *text) {
    return fmt("\"%s\"", text);
}

int main(int argc, char **argv) {
    const char *output = "Build/Program.app";
    const char *entry = NULL;
    const char **files = xmalloc((size_t)argc * sizeof(char *));
    int nfiles = 0;
    for (int index = 1; index < argc; index++) {
        if (same(argv[index], "-o") && index + 1 < argc) output = argv[++index];
        else if (same(argv[index], "-e") && index + 1 < argc) entry = argv[++index];
        else if (argv[index][0] == '-') {
            fprintf(stderr, "usage: rxc -o output [-e Entry.rx] file.rx ...\n");
            return 2;
        } else files[nfiles++] = argv[index];
    }
    if (nfiles == 0) {
        fprintf(stderr, "usage: rxc -o output [-e Entry.rx] file.rx ...\n");
        return 2;
    }
    for (int index = 0; index < nfiles; index++) parse_file(files[index]);
    if (entry == NULL) {
        for (int index = 0; index < nfiles; index++) {
            if (same(stem_of(files[index]), "Robix")) entry = files[index];
        }
        if (entry == NULL) entry = files[0];
    }
    char *generated = fmt("%s.c", output);
    char *parent = xdup(generated);
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = 0;
        char command[4096];
        snprintf(command, sizeof(command), "mkdir -p \"%s\"", parent);
        system(command);
    }
    emit_program(generated, entry);
    if (errors) {
        fprintf(stderr, "rxc: %d codegen errors\n", errors);
        return 1;
    }
    const char *runtime = "InitialCompiler/rx_runtime.c";
    if (access(runtime, R_OK) != 0) runtime = "rx_runtime.c";
    char *include = xdup(runtime);
    char *runtime_slash = strrchr(include, '/');
    if (runtime_slash) *runtime_slash = 0;
    else include = ".";
    char *command = fmt("clang -O3 -flto -std=c11 -Wno-unused-function -Wno-unused-variable -I %s -o %s %s %s", quote(include), quote(output), quote(generated), quote(runtime));
    fprintf(stderr, "%s\n", command);
    int code = system(command);
    if (code != 0) return 1;
    fprintf(stderr, "wrote %s\n", output);
    return 0;
}
