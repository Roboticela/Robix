"use strict";

const { TYPE_KEYWORDS } = require("./keywords");

// Package segments, functions, and types are PascalCase.
// Variables, fields, and parameters are camelCase.
// Each correction changes only the first letter.
const PACKAGE_WORDS = new Set(["Package", "Import", "Connect", "As"]);
const FUNCTION_WORDS = new Set(["Function", "Macro", "Test"]);
const TYPE_WORDS = new Set([
    "Class", "Struct", "Interface", "Trait", "Enum", "Service", "Type", "Implement"
]);

function toPascal(name) {
    const match = /^(_*)([A-Za-z])(.*)$/.exec(String(name || ""));
    if (!match) {
        return String(name || "");
    }
    return match[1] + match[2].toUpperCase() + match[3];
}

function toPascalName(name) {
    return String(name || "")
        .split(".")
        .map((part) => toPascal(part))
        .join(".");
}

function toCamel(name) {
    const match = /^(_*)([A-Za-z])(.*)$/.exec(String(name || ""));
    if (!match) {
        return String(name || "");
    }
    return match[1] + match[2].toLowerCase() + match[3];
}

function isConstantCase(name) {
    return /^[A-Z][A-Z0-9]*(_[A-Z0-9]+)*$/.test(String(name || ""));
}

function namingDiagnostic(code, message) {
    const match = /^(Field|Variable|Parameter) '([^']+)' should use PascalCase\.$/.exec(String(message || ""));
    if (code !== "RX032" || !match) {
        return { code, message, drop: false };
    }
    const name = match[2];
    if (/^[a-z]/.test(name) && !name.includes("_")) {
        return { code, message, drop: true };
    }
    return {
        code: "RX031",
        message: `${match[1]} '${name}' should use camelCase.`,
        drop: false
    };
}

function nextCodeIndex(tokens, index) {
    let cursor = index + 1;
    while (tokens[cursor] && tokens[cursor].kind === "comment") {
        cursor += 1;
    }
    return cursor;
}

function collectDotted(tokens, start, found) {
    let index = start;
    while (tokens[index] && tokens[index].kind === "identifier") {
        found.push(tokens[index]);
        const dot = nextCodeIndex(tokens, index);
        if (tokens[dot] && tokens[dot].text === ".") {
            index = nextCodeIndex(tokens, dot);
            continue;
        }
        break;
    }
}

function isTypeStart(token) {
    if (!token) {
        return false;
    }
    const name = token.canonical || token.text;
    if (token.kind === "keyword" && (TYPE_KEYWORDS.has(name) || name === "Iterator" || name === "Params")) {
        return true;
    }
    return token.kind === "identifier" && /^[A-Z]/.test(token.text);
}

function skipType(tokens, index) {
    let cursor = index + 1;
    if (tokens[cursor] && tokens[cursor].text === "<") {
        let depth = 0;
        while (tokens[cursor]) {
            if (tokens[cursor].text === "<") {
                depth += 1;
            } else if (tokens[cursor].text === ">") {
                depth -= 1;
                if (depth === 0) {
                    cursor += 1;
                    break;
                }
            }
            cursor += 1;
        }
    }
    while (tokens[cursor] && (tokens[cursor].text === "?" || tokens[cursor].text === "[" || tokens[cursor].text === "]")) {
        cursor += 1;
    }
    return cursor;
}

function hasConstBefore(tokens, index) {
    for (let cursor = index - 1; cursor >= 0 && cursor >= index - 8; cursor -= 1) {
        const token = tokens[cursor];
        if (!token || token.kind === "comment") {
            continue;
        }
        if (token.text === ";" || token.text === "{" || token.text === "}") {
            return false;
        }
        if ((token.canonical || token.text) === "Const") {
            return true;
        }
    }
    return false;
}

function variableNameTokens(tokens) {
    const found = [];
    const seen = new Set();
    function add(token) {
        if (!token || token.kind !== "identifier" || seen.has(token.start) || isConstantCase(token.text)) {
            return;
        }
        seen.add(token.start);
        found.push(token);
    }
    for (let index = 0; index < tokens.length; index += 1) {
        const token = tokens[index];
        if (!token || token.kind === "comment") {
            continue;
        }
        const name = token.canonical || token.text;
        if (token.kind === "keyword" && (name === "Var" || name === "Let")) {
            const next = nextCodeIndex(tokens, index);
            if (tokens[next] && tokens[next].text === "(") {
                let cursor = nextCodeIndex(tokens, next);
                while (tokens[cursor] && tokens[cursor].text !== ")") {
                    if (tokens[cursor].kind === "identifier") {
                        add(tokens[cursor]);
                    }
                    cursor = nextCodeIndex(tokens, cursor);
                }
            } else if (tokens[next] && tokens[next].kind === "identifier") {
                add(tokens[next]);
            }
            continue;
        }
        if (token.kind === "keyword" && name === "For") {
            const next = nextCodeIndex(tokens, index);
            const after = nextCodeIndex(tokens, next);
            if (tokens[next] && tokens[next].kind === "identifier" && tokens[after] && (tokens[after].canonical || tokens[after].text) === "In") {
                add(tokens[next]);
            }
            continue;
        }
        if (!isTypeStart(token) || hasConstBefore(tokens, index)) {
            continue;
        }
        let cursor = skipType(tokens, index);
        while (tokens[cursor] && tokens[cursor].kind === "identifier") {
            const next = nextCodeIndex(tokens, cursor);
            const boundary = tokens[next] && tokens[next].text;
            if (boundary !== "=" && boundary !== ";" && boundary !== "," && boundary !== ")") {
                break;
            }
            add(tokens[cursor]);
            if (boundary !== ",") {
                break;
            }
            cursor = nextCodeIndex(tokens, next);
        }
    }
    return found;
}

function variablePartial(line) {
    if (/(?:^|\s)const\b/i.test(line)) {
        return "";
    }
    const simple = /(?:^|\s)(?:var|let|for)\s+([A-Za-z_][A-Za-z0-9_]*)$/i.exec(line);
    if (simple) {
        return simple[1];
    }
    const generic = />\s*\??\s*([A-Za-z_][A-Za-z0-9_]*)$/.exec(line);
    if (generic) {
        return generic[1];
    }
    const typed = /(?:^|[\s,(])([A-Z][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)$/.exec(line);
    if (!typed) {
        return "";
    }
    if (/^(Function|Class|Struct|Interface|Trait|Enum|Service|Package|Import|Var|Let|Const|For|Test|Macro|Type|Implement)$/.test(typed[1])) {
        return "";
    }
    return typed[2];
}

function declarationNameTokens(tokens) {
    const found = [];
    for (let index = 0; index < tokens.length; index += 1) {
        const token = tokens[index];
        if (!token || token.kind !== "keyword") {
            continue;
        }
        const name = token.canonical || token.text;
        const next = nextCodeIndex(tokens, index);
        if (PACKAGE_WORDS.has(name)) {
            collectDotted(tokens, next, found);
        } else if ((FUNCTION_WORDS.has(name) || TYPE_WORDS.has(name)) && tokens[next] && tokens[next].kind === "identifier") {
            found.push(tokens[next]);
        }
    }
    return found;
}

module.exports = {
    toPascal,
    toPascalName,
    toCamel,
    isConstantCase,
    namingDiagnostic,
    declarationNameTokens,
    variableNameTokens,
    variablePartial
};
