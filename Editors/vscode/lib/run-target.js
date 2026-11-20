"use strict";

const { tokenize } = require("./scanner");

function extractTests(source) {
    const tokens = tokenize(source).tokens;
    const tests = [];
    let depth = 0;
    for (let index = 0; index < tokens.length; index += 1) {
        const token = tokens[index];
        if (token.kind === "comment" || token.kind === "eof") {
            continue;
        }
        if (token.text === "{") {
            depth += 1;
            continue;
        }
        if (token.text === "}") {
            depth = Math.max(0, depth - 1);
            continue;
        }
        if (depth !== 0 || token.kind !== "keyword" || token.canonical !== "Test") {
            continue;
        }
        const parsed = readTest(source, tokens, index);
        if (!parsed) {
            continue;
        }
        tests.push(parsed.test);
        index = parsed.next - 1;
        depth = 0;
    }
    return tests;
}

function functionNames(source) {
    const tokens = tokenize(source).tokens;
    const names = [];
    for (let index = 0; index < tokens.length; index += 1) {
        const token = tokens[index];
        if (token.kind !== "keyword" || token.canonical !== "Function") {
            continue;
        }
        const next = nextCode(tokens, index + 1);
        if (next && next.kind === "identifier") {
            names.push(next.text);
        }
    }
    return [...new Set(names)];
}

function selectTests(tests, target) {
    const name = target.name || "";
    const container = target.container || "";
    return tests.filter((test) => {
        if (target.kind === "class") {
            return test.owner === name || (test.owner === "" && test.name === name);
        }
        if (target.kind === "method") {
            return test.name === name && (test.owner === container || test.owner === "");
        }
        return test.owner === "" && test.name === name;
    });
}

function buildCompanion(selected, names) {
    const covered = new Set(selected.map((test) => test.name));
    const parts = selected.map((test) => test.text.trim());
    for (const name of names) {
        if (!name || covered.has(name)) {
            continue;
        }
        covered.add(name);
        parts.push(`Test ${name} {\n    Constraint: True;\n    Processing: True;\n    Output: True;\n}`);
    }
    return `${parts.join("\n\n")}\n`;
}

function caseTitles(test) {
    const titles = [];
    const pattern = /Case\s+(?:"([^"]*)"|([A-Za-z_][A-Za-z0-9_]*))/g;
    let match = pattern.exec(test.text);
    while (match) {
        titles.push(match[1] || match[2]);
        match = pattern.exec(test.text);
    }
    return titles;
}

function readTest(source, tokens, start) {
    let index = nextIndex(tokens, start + 1);
    const first = tokens[index];
    if (!first || (first.kind !== "identifier" && first.kind !== "keyword")) {
        return null;
    }
    let owner = "";
    let name = tokenName(first);
    index = nextIndex(tokens, index + 1);
    if (tokens[index] && tokens[index].text === ".") {
        index = nextIndex(tokens, index + 1);
        const second = tokens[index];
        if (!second || (second.kind !== "identifier" && second.kind !== "keyword")) {
            return null;
        }
        owner = name;
        name = tokenName(second);
        index = nextIndex(tokens, index + 1);
    }
    while (tokens[index] && tokens[index].text !== "{") {
        if (tokens[index].kind === "eof") {
            return null;
        }
        index += 1;
    }
    if (!tokens[index] || tokens[index].text !== "{") {
        return null;
    }
    let depth = 0;
    let end = index;
    for (; end < tokens.length; end += 1) {
        if (tokens[end].kind === "comment" || tokens[end].kind === "eof") {
            continue;
        }
        if (tokens[end].text === "{") {
            depth += 1;
        } else if (tokens[end].text === "}") {
            depth -= 1;
            if (depth === 0) {
                break;
            }
        }
    }
    if (!tokens[end] || tokens[end].text !== "}") {
        return null;
    }
    return {
        test: {
            owner,
            name,
            text: source.slice(tokens[start].start, tokens[end].end),
            start: tokens[start].start,
            end: tokens[end].end
        },
        next: end + 1
    };
}

function nextCode(tokens, index) {
    const found = nextIndex(tokens, index);
    return tokens[found] || null;
}

function nextIndex(tokens, index) {
    let cursor = index;
    while (tokens[cursor] && tokens[cursor].kind === "comment") {
        cursor += 1;
    }
    return cursor;
}

function tokenName(token) {
    return token.kind === "keyword" ? token.canonical : token.text;
}

module.exports = {
    extractTests,
    functionNames,
    selectTests,
    buildCompanion,
    caseTitles
};
