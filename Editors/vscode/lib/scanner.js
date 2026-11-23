"use strict";

const { KEYWORDS, keywordInfo, modifierMask } = require("./keywords");
const { declarationNameTokens, toPascal } = require("./names");

const OPERATORS = [
    "...", "..=", "..", "=>", "->", "?.", "??", "==", "!=", "<=", ">=", "&&", "||", "<<", ">>",
    "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "++", "--",
    "(", ")", "[", "]", "{", "}", ",", ";", ":", ".", "@",
    "+", "-", "*", "/", "%", "=", "<", ">", "&", "|", "^", "~", "!", "?"
];

function isIdentStart(char) {
    return (char >= "A" && char <= "Z") || (char >= "a" && char <= "z") || char === "_";
}

function isIdentPart(char) {
    return isIdentStart(char) || (char >= "0" && char <= "9");
}

function isDigit(char) {
    return char >= "0" && char <= "9";
}

function digitValue(char) {
    if (char >= "0" && char <= "9") {
        return char.charCodeAt(0) - 48;
    }
    if (char >= "a" && char <= "f") {
        return char.charCodeAt(0) - 87;
    }
    if (char >= "A" && char <= "F") {
        return char.charCodeAt(0) - 55;
    }
    return -1;
}

function isSuffix(char) {
    return isDigit(char) || "iufdmIUFD".includes(char);
}

function semanticFor(token) {
    if (token.kind === "comment") {
        return { semType: "comment", semMods: 0 };
    }
    if (token.kind === "string" || token.kind === "char") {
        return { semType: "string", semMods: 0 };
    }
    if (token.kind === "number") {
        return { semType: "number", semMods: 0 };
    }
    if (token.kind === "operator") {
        return { semType: "operator", semMods: 0 };
    }
    if (token.kind === "keyword") {
        const info = keywordInfo(token.canonical);
        if (info && info.isType) {
            return { semType: "type", semMods: modifierMask(["defaultLibrary"]) };
        }
        const mods = [];
        if (info && info.isControl) {
            mods.push("control");
        }
        if (info && info.isModifier) {
            mods.push("modifier");
        }
        if (info && info.isValue) {
            mods.push("readonly", "defaultLibrary");
        }
        return { semType: "keyword", semMods: modifierMask(mods) };
    }
    return { semType: "variable", semMods: 0 };
}

function tokenize(source) {
    const text = String(source);
    const tokens = [];
    const diagnostics = [];
    let index = 0;
    let line = 1;
    let column = 1;
    let pendingNewlines = 0;

    function position() {
        return { line, column, index };
    }

    function advance(count) {
        const steps = count === undefined ? 1 : count;
        for (let step = 0; step < steps; step += 1) {
            if (index >= text.length) {
                return;
            }
            const char = text[index];
            index += 1;
            if (char === "\n") {
                line += 1;
                column = 1;
            } else {
                column += 1;
            }
        }
    }

    function startsWith(value) {
        return text.startsWith(value, index);
    }

    function add(token) {
        const blankLinesBefore = pendingNewlines > 1 ? pendingNewlines - 1 : 0;
        const newlineBefore = pendingNewlines > 0;
        pendingNewlines = 0;
        const sem = semanticFor(token);
        tokens.push({
            ...token,
            newlineBefore,
            blankLinesBefore,
            semType: sem.semType,
            semMods: sem.semMods,
            role: token.kind === "identifier" ? "use" : ""
        });
    }

    function error(code, message, at) {
        diagnostics.push({
            severity: "error",
            code,
            message,
            line: at.line,
            column: at.column,
            start: at.index,
            end: index
        });
    }

    function skipWhitespace() {
        while (index < text.length) {
            const char = text[index];
            if (char === " " || char === "\t" || char === "\r") {
                advance();
            } else if (char === "\n") {
                pendingNewlines += 1;
                advance();
            } else {
                break;
            }
        }
    }

    function readComment() {
        const start = position();
        if (startsWith("//")) {
            const begin = index;
            advance(2);
            while (index < text.length && text[index] !== "\n") {
                advance();
            }
            add({
                kind: "comment",
                text: text.slice(begin, index),
                start: begin,
                end: index,
                line: start.line,
                column: start.column
            });
            return;
        }
        const begin = index;
        advance(2);
        let closed = false;
        while (index < text.length) {
            if (text[index] === "*" && text[index + 1] === "/") {
                advance(2);
                closed = true;
                break;
            }
            advance();
        }
        if (!closed) {
            error("RX001", "Unclosed block comment.", start);
        }
        add({
            kind: "comment",
            text: text.slice(begin, index),
            start: begin,
            end: index,
            line: start.line,
            column: start.column
        });
    }

    function readString(prefixStart, raw) {
        const start = position();
        start.index = prefixStart;
        let multiline = false;
        if (startsWith("\"\"\"")) {
            multiline = true;
            advance(3);
        } else {
            advance();
        }
        let closed = false;
        while (index < text.length) {
            if (multiline && startsWith("\"\"\"")) {
                advance(3);
                closed = true;
                break;
            }
            if (!multiline && text[index] === "\"") {
                advance();
                closed = true;
                break;
            }
            if (!raw && text[index] === "\\") {
                advance();
                if (index < text.length) {
                    advance();
                }
                continue;
            }
            advance();
        }
        if (!closed) {
            error("RX002", "Unclosed string literal.", { line: start.line, column: start.column, index: prefixStart });
        }
        add({
            kind: "string",
            text: text.slice(prefixStart, index),
            start: prefixStart,
            end: index,
            line: start.line,
            column: start.column
        });
    }

    function readChar() {
        const start = position();
        advance();
        if (index < text.length && text[index] === "\\") {
            advance();
            if (index < text.length) {
                advance();
            }
        } else if (index < text.length) {
            advance();
        }
        if (index < text.length && text[index] === "'") {
            advance();
        } else {
            error("RX003", "Unclosed character literal.", start);
        }
        add({
            kind: "char",
            text: text.slice(start.index, index),
            start: start.index,
            end: index,
            line: start.line,
            column: start.column
        });
    }

    function readNumber() {
        const start = position();
        const begin = index;
        if (text[index] === "0" && index + 1 < text.length) {
            const marker = text[index + 1];
            if (marker === "x" || marker === "X" || marker === "b" || marker === "B" || marker === "o" || marker === "O") {
                advance(2);
                while (index < text.length && (isDigit(text[index]) || text[index] === "_" || digitValue(text[index]) >= 0)) {
                    advance();
                }
            }
        }
        if (index === begin) {
            while (index < text.length && (isDigit(text[index]) || text[index] === "_")) {
                advance();
            }
        }
        if (index < text.length && text[index] === "." && text[index + 1] !== "." && isDigit(text[index + 1] || "")) {
            advance();
            while (index < text.length && (isDigit(text[index]) || text[index] === "_")) {
                advance();
            }
        }
        if (index < text.length && (text[index] === "e" || text[index] === "E")) {
            const saved = { index, line, column };
            advance();
            if (index < text.length && (text[index] === "+" || text[index] === "-")) {
                advance();
            }
            if (index < text.length && isDigit(text[index])) {
                while (index < text.length && (isDigit(text[index]) || text[index] === "_")) {
                    advance();
                }
            } else {
                index = saved.index;
                line = saved.line;
                column = saved.column;
            }
        }
        while (index < text.length && isSuffix(text[index])) {
            const suffix = text[index];
            if (isDigit(suffix) || "iufdmIUFD".includes(suffix)) {
                advance();
            } else {
                break;
            }
        }
        add({
            kind: "number",
            text: text.slice(begin, index),
            start: begin,
            end: index,
            line: start.line,
            column: start.column
        });
    }

    function readIdentifier() {
        const start = position();
        const begin = index;
        while (index < text.length && isIdentPart(text[index])) {
            advance();
        }
        const value = text.slice(begin, index);
        const canonical = KEYWORDS.get(value.toLowerCase());
        if (canonical) {
            add({
                kind: "keyword",
                text: value,
                canonical,
                start: begin,
                end: index,
                line: start.line,
                column: start.column
            });
            if (value !== canonical) {
                diagnostics.push({
                    severity: "warning",
                    code: "RX005",
                    message: `Keyword '${value}' should be written '${canonical}'.`,
                    line: start.line,
                    column: start.column,
                    start: begin,
                    end: index,
                    fix: canonical
                });
            }
            return;
        }
        add({
            kind: "identifier",
            text: value,
            canonical: value,
            start: begin,
            end: index,
            line: start.line,
            column: start.column
        });
    }

    function readOperator() {
        const start = position();
        const point = text.codePointAt(index);
        if (point > 126) {
            const begin = index;
            while (index < text.length && text.codePointAt(index) > 126) {
                const size = text.codePointAt(index) > 0xffff ? 2 : 1;
                advance(size);
            }
            add({
                kind: "operator",
                text: text.slice(begin, index),
                start: begin,
                end: index,
                line: start.line,
                column: start.column
            });
            return;
        }
        for (const op of OPERATORS) {
            if (startsWith(op)) {
                const begin = index;
                advance(op.length);
                add({
                    kind: "operator",
                    text: op,
                    start: begin,
                    end: index,
                    line: start.line,
                    column: start.column
                });
                return;
            }
        }
        const char = text[index];
        error("RX004", `Unexpected character '${char}'.`, start);
        const begin = index;
        advance();
        add({
            kind: "unknown",
            text: text.slice(begin, index),
            start: begin,
            end: index,
            line: start.line,
            column: start.column
        });
    }

    while (index < text.length) {
        skipWhitespace();
        if (index >= text.length) {
            break;
        }
        const char = text[index];
        if (char === "/" && (text[index + 1] === "/" || text[index + 1] === "*")) {
            readComment();
            continue;
        }
        if ((char === "r" || char === "R") && (text[index + 1] === "\"" || (text[index + 1] === "$" && text[index + 2] === "\""))) {
            const prefix = index;
            advance();
            if (text[index] === "$") {
                advance();
            }
            readString(prefix, true);
            continue;
        }
        if (char === "$" && text[index + 1] === "\"") {
            const prefix = index;
            advance();
            readString(prefix, false);
            continue;
        }
        if (char === "\"") {
            readString(index, false);
            continue;
        }
        if (char === "'") {
            readChar();
            continue;
        }
        if (isIdentStart(char)) {
            readIdentifier();
            continue;
        }
        if (isDigit(char)) {
            readNumber();
            continue;
        }
        readOperator();
    }

    add({
        kind: "eof",
        text: "",
        start: index,
        end: index,
        line,
        column
    });
    return { tokens, diagnostics };
}

function keywordContinues(lower) {
    for (const word of KEYWORDS.keys()) {
        if (word.length > lower.length && word.startsWith(lower)) {
            return true;
        }
    }
    return false;
}

function keywordCaseEdits(text, changeOffset, inserted) {
    if (!inserted) {
        return [];
    }
    const { tokens } = tokenize(text);
    const edits = [];
    const lettersOnly = /^[A-Za-z0-9_]+$/.test(inserted);
    const boundaryOnly = !lettersOnly && /^[^A-Za-z0-9_]+$/.test(inserted);
    const regionStart = changeOffset;
    const regionEnd = changeOffset + inserted.length;
    for (const token of tokens) {
        if (token.kind !== "keyword" || token.text === token.canonical) {
            continue;
        }
        const nextChar = text[token.end];
        const closed = token.end >= text.length || !/[A-Za-z0-9_]/.test(nextChar);
        if (!closed) {
            continue;
        }
        let hit = false;
        if (boundaryOnly) {
            hit = token.end === changeOffset;
        } else if (!lettersOnly) {
            hit = token.start >= regionStart && token.end <= regionEnd;
        } else if (token.end !== changeOffset) {
            const overlaps = token.start < regionEnd && token.end > regionStart;
            hit = overlaps && !keywordContinues(token.text.toLowerCase());
        }
        if (hit) {
            edits.push({ start: token.start, end: token.end, text: token.canonical });
        }
    }
    return edits;
}

function nameCaseEdits(text, changeOffset, inserted) {
    if (!inserted || /^[A-Za-z0-9_]+$/.test(inserted)) {
        return [];
    }
    const { tokens } = tokenize(text);
    const regionStart = changeOffset;
    const regionEnd = changeOffset + inserted.length;
    const boundaryOnly = /^[^A-Za-z0-9_]+$/.test(inserted);
    const edits = [];
    for (const token of declarationNameTokens(tokens)) {
        const corrected = toPascal(token.text);
        if (!token.text || corrected === token.text) {
            continue;
        }
        const nextChar = text[token.end];
        const closed = token.end >= text.length || !/[A-Za-z0-9_]/.test(nextChar || "");
        if (!closed) {
            continue;
        }
        const hit = boundaryOnly
            ? token.end === changeOffset
            : token.start >= regionStart && token.end <= regionEnd;
        if (hit) {
            edits.push({ start: token.start, end: token.end, text: corrected });
        }
    }
    return edits;
}

module.exports = {
    tokenize,
    keywordCaseEdits,
    nameCaseEdits,
    isIdentStart,
    isIdentPart
};
