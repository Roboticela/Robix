"use strict";

// Package segments, functions, and types are PascalCase.
// Each segment keeps the rest of its spelling and only raises the first letter.
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
    declarationNameTokens
};
