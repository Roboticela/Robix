"use strict";

const { tokenize } = require("./scanner");
const { declarationNameTokens, toPascal } = require("./names");
const {
    TYPE_KEYWORDS,
    MODIFIER_KEYWORDS,
    DECLARATION_KEYWORDS,
    STATEMENT_START,
    CONTINUING_KEYWORDS
} = require("./keywords");

const TAKES_EXPR = new Set([
    "Return", "Throw", "Yield", "Await", "Goto", "Not", "ASM", "If", "While", "For", "Match",
    "Repeat", "Lock", "Using", "And", "Or", "Is", "Equal", "Else"
]);
const ATTACH_AFTER_BRACE = new Set(["Else", "Catch", "Finally"]);
const METHOD_KEYWORDS = new Set(["Function", "Constructor", "Destructor", "Operator"]);
const RANGE_OPS = new Set(["..", "..=", "..."]);
const ASSIGNMENT = new Set(["=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^="]);
const COMPARISON = new Set(["==", "!=", "<", ">", "<=", ">=", "&&", "||", "&", "|", "^", "??", "And", "Or", "Is", "Equal"]);

function canonicalOf(token) {
    if (!token) {
        return "";
    }
    return token.canonical || token.text || "";
}

function isTypeLike(token) {
    if (!token) {
        return false;
    }
    if (token.kind === "keyword" && TYPE_KEYWORDS.has(token.canonical)) {
        return true;
    }
    return token.kind === "identifier" && /^[A-Z_]/.test(token.text);
}

function isModifier(token) {
    return Boolean(token && token.kind === "keyword" && MODIFIER_KEYWORDS.has(token.canonical));
}

function endsExpression(token) {
    if (!token) {
        return false;
    }
    if (token.text === ")" || token.text === "]" || token.text === "++" || token.text === "--") {
        return true;
    }
    if (token.kind === "number" || token.kind === "string" || token.kind === "char") {
        return true;
    }
    if (token.kind === "identifier") {
        return true;
    }
    return token.kind === "keyword" && (token.canonical === "True" || token.canonical === "False" || token.canonical === "Null" || token.canonical === "This" || token.canonical === "Base");
}

class Writer {
    constructor(size) {
        this.size = size;
        this.indent = 0;
        this.line = "";
        this.lines = [];
        this.atLineStart = true;
        this.written = false;
    }

    ensureNewline() {
        if (!this.atLineStart) {
            this.newline();
        }
    }

    newline() {
        this.lines.push(this.line.replace(/[ \t]+$/g, ""));
        this.line = "";
        this.atLineStart = true;
        this.written = true;
    }

    blankLine() {
        if (!this.written) {
            return;
        }
        this.ensureNewline();
        if (this.lines.length === 0 || this.lines[this.lines.length - 1] !== "") {
            this.lines.push("");
        }
    }

    space() {
        if (!this.atLineStart && !this.line.endsWith(" ")) {
            this.line += " ";
        }
    }

    write(text) {
        if (!text) {
            return;
        }
        if (this.atLineStart) {
            this.line += " ".repeat(Math.max(0, this.indent) * this.size);
            this.atLineStart = false;
        }
        this.line += text;
        this.written = true;
    }

    finish() {
        if (!this.atLineStart) {
            this.newline();
        }
        while (this.lines.length > 0 && this.lines[this.lines.length - 1] === "") {
            this.lines.pop();
        }
        return `${this.lines.join("\n")}\n`;
    }
}

function nextCode(tokens, index) {
    for (let cursor = index + 1; cursor < tokens.length; cursor += 1) {
        if (tokens[cursor].kind !== "comment" && tokens[cursor].kind !== "eof") {
            return tokens[cursor];
        }
    }
    return null;
}

function opensGeneric(tokens, index, prev) {
    const token = tokens[index];
    if (!token || token.text !== "<") {
        return false;
    }
    if (!prev || (prev.kind !== "identifier" && prev.kind !== "keyword")) {
        return false;
    }
    let depth = 0;
    for (let cursor = index; cursor < tokens.length; cursor += 1) {
        const item = tokens[cursor];
        if (item.kind === "comment") {
            continue;
        }
        if (item.text === "<") {
            depth += 1;
        } else if (item.text === ">") {
            depth -= 1;
            if (depth === 0) {
                return true;
            }
        } else if (item.text === ">>") {
            depth -= 2;
            if (depth === 0) {
                return true;
            }
            if (depth < 0) {
                return false;
            }
        } else if (depth > 0 && item.kind === "identifier" && !/^[A-Z_]/.test(item.text)) {
            return false;
        } else if (depth > 0 && (item.text === ";" || item.text === "{" || item.text === "}" || item.text === "=" || item.kind === "string")) {
            return false;
        } else if (item.text === "==" || item.text === "!=" || item.text === "&&" || item.text === "||" || item.text === "+" || item.text === "-" || item.text === "*" || item.text === "/" || item.text === "?") {
            return false;
        }
    }
    return false;
}

function isUnary(token, prev) {
    if (!token || (token.text !== "-" && token.text !== "+" && token.text !== "*" && token.text !== "&")) {
        return false;
    }
    if (!prev) {
        return true;
    }
    if ("([{,;:=?".includes(prev.text) || prev.text === "->" || prev.text === "=>") {
        return true;
    }
    if (prev.kind === "operator" && prev.text !== "++" && prev.text !== "--" && prev.text !== ")" && prev.text !== "]") {
        return true;
    }
    if (prev.kind === "keyword" && !["True", "False", "Null", "This", "Base"].includes(prev.canonical)) {
        return true;
    }
    return false;
}

function spaceBefore(token, prev, state) {
    if (!prev || state.writer.atLineStart) {
        return false;
    }
    const text = token.text;
    const previous = prev.text;
    if (text === ";" && previous === ";" && state.paren > 0) {
        return true;
    }
    if (text === ")" || text === "]" || text === "}" || text === "," || text === ";" || text === "." || text === "?.") {
        return false;
    }
    if (text === "++" || text === "--") {
        return false;
    }
    if (text === "(") {
        if (prev.kind === "identifier") {
            return false;
        }
        const name = canonicalOf(prev);
        if (name === "Function" || name === "ASM" || name === "Constructor" || name === "Destructor") {
            return false;
        }
        if (prev.kind === "keyword") {
            return true;
        }
        return prev.kind === "operator" && previous !== "." && previous !== "?." && previous !== "@";
    }
    if (text === "[") {
        return prev.kind !== "identifier" && !(prev.kind === "keyword" && TYPE_KEYWORDS.has(prev.canonical)) && prev.text !== ")" && prev.text !== "]";
    }
    if (text === ">" && state.generic > 0) {
        return false;
    }
    if (text === ">>" && state.generic >= 2) {
        return false;
    }
    if (text === "<" && state.genericOpen) {
        return false;
    }
    if (RANGE_OPS.has(text) || RANGE_OPS.has(previous)) {
        return false;
    }
    if (previous === "." || previous === "?." || previous === "@" || previous === "(" || previous === "[" || previous === "{" || previous === "...") {
        return false;
    }
    if (previous === "!" || previous === "~" || previous === "++" || previous === "--") {
        return false;
    }
    if (isUnary(prev, state.prevPrev)) {
        return false;
    }
    if ((previous === "<" || previous === ">") && state.generic > 0) {
        return false;
    }
    if (text === ":" && previous !== ")") {
        return false;
    }
    if (token.kind === "keyword" && prev.kind === "keyword" && MODIFIER_KEYWORDS.has(prev.canonical)) {
        return true;
    }
    return true;
}

function formatRobix(source, options = {}) {
    const size = options.indentSize || 4;
    const { tokens } = tokenize(source);
    const pascalNames = new Set(declarationNameTokens(tokens).map((token) => token.start));
    const writer = new Writer(size);
    const state = {
        writer,
        indent: 0,
        paren: 0,
        bracket: 0,
        generic: 0,
        genericOpen: false,
        braces: [],
        nextBrace: null,
        operatorHeader: false,
        memberDone: false,
        statementDone: false,
        lastTop: null,
        currentTop: null,
        lastClosed: "",
        prevPrev: null
    };

    function topKind() {
        return state.braces.length ? state.braces[state.braces.length - 1] : "";
    }

    function setIndent(value) {
        state.indent = Math.max(0, value);
        writer.indent = state.indent;
    }

    function significant(index) {
        return nextCode(tokens, index - 1);
    }

    function mustBreak(token, prev, next) {
        if (!prev) {
            return false;
        }
        if (token.blankLinesBefore > 0 && state.paren === 0 && state.bracket === 0 && state.generic === 0) {
            return true;
        }
        if (state.paren > 0 || state.bracket > 0 || state.generic > 0) {
            return false;
        }
        if (token.text === "}") {
            return true;
        }
        if (token.text === "{" || token.text === ")" || token.text === "]" || token.text === ";" || token.text === "(" || token.text === "[") {
            return false;
        }
        if (token.text === "," && topKind() === "enum" && state.paren === 0) {
            return true;
        }
        if (token.text === ",") {
            return false;
        }
        if (token.canonical === "Explain" || (token.canonical === "Where" && !CONTINUING_KEYWORDS.has(canonicalOf(prev)))) {
            return true;
        }
        if (isModifier(prev) && (isModifier(token) || DECLARATION_KEYWORDS.has(token.canonical) || METHOD_KEYWORDS.has(token.canonical))) {
            return false;
        }
        if (prev.text === "}" && ATTACH_AFTER_BRACE.has(token.canonical)) {
            return false;
        }
        if (prev.text === "}" && token.canonical === "While" && state.lastClosed === "do") {
            return false;
        }
        if (prev.canonical === "Else" && token.canonical === "If") {
            return false;
        }
        if (CONTINUING_KEYWORDS.has(token.canonical) && token.canonical !== "Where") {
            return false;
        }
        if (CONTINUING_KEYWORDS.has(canonicalOf(prev)) && canonicalOf(prev) !== "Where" && !STATEMENT_START.has(token.canonical)) {
            return false;
        }
        if (token.text === "." || token.text === "?." || prev.text === "." || prev.text === "?.") {
            return false;
        }
        if (token.kind === "operator" && token.text !== ";") {
            return false;
        }
        if (prev.kind === "operator" && prev.text !== "}" && prev.text !== ")" && prev.text !== ";") {
            return false;
        }
        if (prev.text === "(" || prev.text === "[" || prev.text === "{") {
            return false;
        }
        if (TAKES_EXPR.has(canonicalOf(prev)) && token.canonical !== "Explain" && token.canonical !== "Where") {
            return false;
        }
        if (isTypeLike(prev) && (token.kind === "identifier" || token.text === "<" || token.text === "[" || token.text === "?")) {
            return false;
        }
        if (token.canonical === "Where") {
            return true;
        }
        if (STATEMENT_START.has(token.canonical) && !isModifier(prev)) {
            return true;
        }
        if (isModifier(token) && !isModifier(prev) && prev.text !== "@") {
            return true;
        }
        if (token.text === "@" && prev.text !== "(") {
            return true;
        }
        if (token.canonical === "Function" && state.operatorHeader && state.braces.length === 0) {
            return true;
        }
        if (endsExpression(prev) && token.kind === "identifier" && next && next.text === "(") {
            return true;
        }
        if (endsExpression(prev) && isTypeLike(token) && next && (next.kind === "identifier" || next.text === "<")) {
            return true;
        }
        if (endsExpression(prev) && token.kind === "identifier" && next && next.text === ":" && prev.text !== ".") {
            return true;
        }
        return false;
    }

    function methodAhead(index) {
        let cursor = index;
        while (cursor < tokens.length && tokens[cursor].kind === "comment") {
            cursor += 1;
        }
        while (cursor < tokens.length && isModifier(tokens[cursor])) {
            cursor += 1;
            while (cursor < tokens.length && tokens[cursor].kind === "comment") {
                cursor += 1;
            }
        }
        return Boolean(tokens[cursor] && METHOD_KEYWORDS.has(tokens[cursor].canonical));
    }

    function writeWord(token) {
        if (token.kind === "keyword") {
            writer.write(token.canonical);
            return;
        }
        if (token.kind === "identifier" && pascalNames.has(token.start)) {
            writer.write(toPascal(token.text));
            return;
        }
        writer.write(token.text);
    }

    function openBrace(token, prev) {
        const empty = nextCode(tokens, tokens.indexOf(token)) ;
        let kind = "block";
        if (canonicalOf(prev) === "Explain") {
            kind = "explain";
        } else if (state.nextBrace) {
            kind = state.nextBrace;
            state.nextBrace = null;
        }
        if (state.whereArm) {
            kind = "where";
            state.whereArm = false;
        }
        const ahead = nextCode(tokens, tokenIndex(token));
        if (ahead && ahead.text === "}") {
            if (!writer.atLineStart) {
                writer.space();
            }
            writer.write("{}");
            state.lastClosed = kind;
            return ahead;
        }
        if (!writer.atLineStart) {
            writer.space();
        }
        writer.write("{");
        state.braces.push(kind);
        setIndent(state.indent + 1);
        writer.ensureNewline();
        return null;
    }

    function tokenIndex(token) {
        return tokens.indexOf(token);
    }

    let previous = null;
    let index = 0;
    while (index < tokens.length) {
        const token = tokens[index];
        if (token.kind === "eof") {
            break;
        }
        if (token.kind === "comment") {
            if (!token.newlineBefore && previous) {
                writer.write("  ");
                writer.write(token.text);
                if (token.text.startsWith("//")) {
                    writer.ensureNewline();
                }
            } else {
                writer.ensureNewline();
                if (token.blankLinesBefore > 0) {
                    writer.blankLine();
                }
                writer.write(token.text);
                writer.ensureNewline();
            }
            index += 1;
            continue;
        }

        const next = nextCode(tokens, index);
        state.genericOpen = opensGeneric(tokens, index, previous);
        const breaking = mustBreak(token, previous, next);
        if (state.statementDone && state.braces.length === 0 && state.indent === 0 && token.text !== "}" && token.kind !== "comment") {
            if (!(state.lastTop === "import" && token.canonical === "Import")) {
                writer.blankLine();
            }
            state.statementDone = false;
        } else if (breaking && topKind() === "type" && state.memberDone && token.text !== "}" && methodAhead(index)) {
            writer.blankLine();
            state.memberDone = false;
        } else if (breaking) {
            writer.ensureNewline();
            if (token.blankLinesBefore > 0 && token.text !== "}") {
                writer.blankLine();
            }
        } else if (spaceBefore(token, previous, state)) {
            writer.space();
        }

        if (state.braces.length === 0 && state.paren === 0 && !state.currentTop && token.kind !== "comment") {
            state.currentTop = token.canonical === "Import" ? "import" : "other";
        }

        if (token.canonical === "Where" && state.paren === 0 && state.generic === 0 && breaking) {
            setIndent(state.indent + 1);
            state.whereArm = true;
        }
        if (token.canonical === "Function" && state.operatorHeader && state.braces.length === 0) {
            setIndent(state.indent + 1);
            state.operatorHeader = false;
            state.nextBrace = "operator-fn";
        }
        if (token.kind === "keyword") {
            const name = token.canonical;
            if (name === "Operator" && state.braces.length === 0) {
                state.operatorHeader = true;
            }
            if (name === "Class" || name === "Struct" || name === "Interface" || name === "Trait" || name === "Service" || name === "Implement") {
                state.nextBrace = "type";
            } else if (name === "Enum") {
                state.nextBrace = "enum";
            } else if (name === "Test") {
                state.nextBrace = "test";
            } else if (name === "If") {
                state.nextBrace = "if";
            } else if (name === "Do") {
                state.nextBrace = "do";
            } else if (name === "While" || name === "For" || name === "Repeat" || name === "Loop") {
                state.nextBrace = "loop";
            } else if (name === "Match") {
                state.nextBrace = "match";
            } else if (name === "Try") {
                state.nextBrace = "try";
            } else if (name === "Explain") {
                state.nextBrace = "explain";
            }
        }

        if (token.text === "{") {
            const skipped = openBrace(token, previous);
            state.prevPrev = previous;
            previous = token;
            index += 1;
            if (skipped) {
                index = tokenIndex(skipped) + 1;
                previous = skipped;
            }
            continue;
        }

        if (token.text === "}") {
            setIndent(state.indent - 1);
            const kind = state.braces.pop() || "block";
            if (kind === "where") {
                setIndent(state.indent - 1);
            }
            if (kind === "explain" && next && next.text === "{") {
                writer.write("} {");
                state.braces.push("block");
                setIndent(state.indent + 1);
                writer.ensureNewline();
                state.lastClosed = kind;
                state.nextBrace = null;
                previous = next;
                index = tokenIndex(next) + 1;
                continue;
            }
            writer.write("}");
            let consumed = null;
            if (next && next.text === ";") {
                writer.write(";");
                consumed = next;
            } else if (next && next.text === "," && topKind() !== "enum") {
                writer.write(",");
                consumed = next;
            }
            if (kind === "operator-fn") {
                setIndent(state.indent - 1);
            }
            state.lastClosed = kind;
            const follower = consumed ? nextCode(tokens, tokenIndex(consumed)) : next;
            const attach = follower && (
                ATTACH_AFTER_BRACE.has(follower.canonical) ||
                (kind === "do" && follower.canonical === "While")
            );
            if (!attach) {
                writer.ensureNewline();
                if (state.braces.length === 0 && state.paren === 0) {
                    state.lastTop = state.currentTop || "other";
                    state.currentTop = null;
                    state.statementDone = true;
                }
                if (topKind() === "type") {
                    state.memberDone = true;
                }
            }
            previous = consumed || token;
            index = consumed ? tokenIndex(consumed) + 1 : index + 1;
            continue;
        }

        if (token.text === ";") {
            writer.write(";");
            if (state.paren === 0 && state.bracket === 0 && state.generic === 0) {
                state.nextBrace = null;
                writer.ensureNewline();
                if (state.braces.length === 0) {
                    state.lastTop = state.currentTop || "other";
                    state.currentTop = null;
                    state.statementDone = true;
                    state.operatorHeader = false;
                }
                if (topKind() === "type") {
                    state.memberDone = true;
                }
            }
            state.prevPrev = previous;
            previous = token;
            index += 1;
            continue;
        }

        if (token.text === "<" && state.genericOpen) {
            writer.write("<");
            state.generic += 1;
        } else if (token.text === ">" && state.generic > 0) {
            writer.write(">");
            state.generic -= 1;
        } else if (token.text === ">>" && state.generic >= 2) {
            writer.write(">>");
            state.generic -= 2;
        } else if (token.text === "(") {
            writer.write("(");
            state.paren += 1;
        } else if (token.text === ")") {
            writer.write(")");
            state.paren = Math.max(0, state.paren - 1);
        } else if (token.text === "[") {
            writer.write("[");
            state.bracket += 1;
        } else if (token.text === "]") {
            writer.write("]");
            state.bracket = Math.max(0, state.bracket - 1);
        } else {
            writeWord(token);
        }

        state.prevPrev = previous;
        previous = token;
        index += 1;
    }

    return writer.finish();
}

function formatProject(source, options = {}) {
    const size = options.indentSize || 4;
    const { tokens } = tokenize(source);
    const writer = new Writer(size);
    let previous = null;
    let paren = 0;
    for (let index = 0; index < tokens.length; index += 1) {
        const token = tokens[index];
        if (token.kind === "eof") {
            break;
        }
        if (token.kind === "comment") {
            writer.ensureNewline();
            if (token.blankLinesBefore > 0) {
                writer.blankLine();
            }
            writer.write(token.text);
            writer.ensureNewline();
            continue;
        }
        const next = nextCode(tokens, index);
        if (token.text === "{") {
            writer.ensureNewline();
            writer.write("{");
            writer.indent += 1;
            writer.ensureNewline();
        } else if (token.text === "}") {
            writer.ensureNewline();
            writer.indent = Math.max(0, writer.indent - 1);
            writer.write("}");
            if (next && next.text === ";") {
                writer.write(";");
                index += 1;
            }
            writer.ensureNewline();
        } else if (token.text === ";") {
            writer.write(";");
            writer.ensureNewline();
            if (next && next.blankLinesBefore > 0) {
                writer.blankLine();
            }
        } else if (token.text === "(") {
            writer.write("(");
            paren += 1;
        } else if (token.text === ")") {
            writer.write(")");
            paren = Math.max(0, paren - 1);
        } else {
            const startSection = token.blankLinesBefore > 0 && paren === 0;
            if (startSection) {
                writer.blankLine();
            } else if (token.text !== "," && token.text !== ")" && token.text !== "]" && previous && previous.text !== "(" && previous.text !== "[" && previous.text !== ".") {
                if (token.text === ":") {
                    // The key stays against the colon.
                } else if (previous.text !== ":") {
                    writer.space();
                } else {
                    writer.space();
                }
            } else if (previous && previous.text === ":") {
                writer.space();
            }
            if (token.text === ":" && previous && previous.text !== " ") {
                // no space before
            }
            writer.write(token.text);
        }
        previous = token;
    }
    return writer.finish();
}

module.exports = {
    formatRobix,
    formatProject
};
