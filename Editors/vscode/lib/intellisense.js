"use strict";

const { analyze } = require("./analyze");
const {
    KEYWORD_WORDS,
    KEYWORD_DOCS,
    keywordInfo,
    SNIPPETS,
    BUILTIN_FUNCTIONS,
    BUILTIN_TYPES,
    TYPE_METHODS,
    NAMESPACES,
    PACKAGES,
    ATTRIBUTES,
    SEMANTIC_TYPES
} = require("./keywords");

function tokenAt(tokens, offset) {
    let found = null;
    for (const token of tokens) {
        if (token.kind === "eof") {
            break;
        }
        if (token.start <= offset && offset <= token.end) {
            found = token;
        }
        if (token.start > offset) {
            break;
        }
    }
    return found;
}

function previousToken(tokens, token) {
    const index = tokens.indexOf(token);
    for (let cursor = index - 1; cursor >= 0; cursor -= 1) {
        if (tokens[cursor].kind !== "comment") {
            return tokens[cursor];
        }
    }
    return null;
}

function complete(source, offset, external) {
    const doc = analyze(source, externalNames(external));
    const tokens = doc.tokens;
    const left = Math.max(0, offset);
    const current = tokenAt(tokens, left);
    if (current && (current.kind === "string" || current.kind === "comment" || current.kind === "char")) {
        return { items: [], replaceStart: left, replaceEnd: left };
    }
    const word = wordAt(source, left);
    const before = source.slice(0, left);
    const line = before.slice(before.lastIndexOf("\n") + 1);
    if (/^\s*@\w*$/.test(line) || /\s@\w*$/.test(line)) {
        return {
            items: attributeItems(external),
            replaceStart: word.start,
            replaceEnd: word.end
        };
    }
    if (/(?:^|\s)(?:Import|Package)\s+[\w.]*$/.test(line) || /(?:^|\s)As\s+\w*$/.test(line)) {
        return {
            items: packageItems(external),
            replaceStart: Math.max(lineStart(source, left), left - trailingName(line).length),
            replaceEnd: left
        };
    }
    const dot = line.match(/(?:^|.*?)([A-Za-z_][A-Za-z0-9_]*)\s*(?:\?\.|\.)\s*([A-Za-z_][A-Za-z0-9_]*)?$/);
    if (dot) {
        const receiver = dot[1];
        const partial = dot[2] || "";
        return {
            items: memberItems(doc, receiver, left, external),
            replaceStart: left - partial.length,
            replaceEnd: left
        };
    }
    return {
        items: scopeItems(doc, left, external),
        replaceStart: word.start,
        replaceEnd: word.end
    };
}

function wordAt(source, offset) {
    let start = offset;
    while (start > 0 && /[A-Za-z0-9_]/.test(source[start - 1])) {
        start -= 1;
    }
    let end = offset;
    while (end < source.length && /[A-Za-z0-9_]/.test(source[end])) {
        end += 1;
    }
    return { start, end, text: source.slice(start, end) };
}

function lineStart(source, offset) {
    return source.lastIndexOf("\n", Math.max(0, offset - 1)) + 1;
}

function trailingName(line) {
    const match = line.match(/[A-Za-z_][A-Za-z0-9_.]*$/);
    return match ? match[0] : "";
}

function scopeItems(doc, offset, external) {
    const items = [];
    const seen = new Set();
    function add(item) {
        const key = `${item.kind}:${item.label}:${item.detail || ""}`;
        if (seen.has(key)) {
            return;
        }
        seen.add(key);
        items.push(item);
    }
    for (const word of KEYWORD_WORDS) {
        const info = keywordInfo(word);
        add({
            label: word,
            kind: info && info.isType ? "type" : "keyword",
            detail: info && info.isType ? "type" : "keyword",
            documentation: KEYWORD_DOCS[word] || "",
            insert: word,
            filter: `${word} ${word.toLowerCase()}`,
            sort: info && info.isType ? "3" : "4"
        });
    }
    for (const snippet of SNIPPETS) {
        add({
            label: snippet.label,
            kind: "snippet",
            detail: snippet.detail || "snippet",
            documentation: "Inserts a Robix snippet.",
            insert: snippet.insert,
            snippet: true,
            filter: `${snippet.label} ${snippet.label.toLowerCase()}`,
            sort: "0"
        });
    }
    for (const symbol of visibleSymbols(doc, offset)) {
        add(symbolItem(symbol, "1"));
    }
    for (const builtin of BUILTIN_FUNCTIONS) {
        add({
            label: builtin[0],
            kind: "function",
            detail: builtin[1],
            documentation: builtin[2],
            insert: builtin[0],
            filter: `${builtin[0]} ${builtin[0].toLowerCase()}`,
            sort: "2"
        });
    }
    for (const typeName of BUILTIN_TYPES) {
        add({
            label: typeName,
            kind: "class",
            detail: "type",
            documentation: `${typeName} is a Robix standard type.`,
            insert: typeName,
            filter: `${typeName} ${typeName.toLowerCase()}`,
            sort: "3"
        });
    }
    for (const name of Object.keys(NAMESPACES)) {
        add({
            label: name,
            kind: "namespace",
            detail: "namespace",
            documentation: `${name} is part of the Robix standard library.`,
            insert: name,
            filter: `${name} ${name.toLowerCase()}`,
            sort: "3"
        });
    }
    for (const symbol of external || []) {
        if (symbol.exported || symbol.kind === "namespace" || symbol.kind === "package") {
            add(symbolItem(symbol, "5"));
        }
    }
    return items;
}

function symbolItem(symbol, sort) {
    const kind = symbol.kind === "namespace" ? "namespace"
        : symbol.kind === "parameter" ? "variable"
        : symbol.kind === "property" ? "field"
        : symbol.kind === "enumMember" ? "enum"
        : symbol.kind;
    return {
        label: symbol.name,
        kind,
        detail: symbol.detail || symbol.kind,
        documentation: symbol.detail || "",
        insert: symbol.name,
        filter: `${symbol.name} ${symbol.name.toLowerCase()}`,
        sort
    };
}

function visibleSymbols(doc, offset) {
    return doc.symbols.filter((symbol) => {
        if (!symbol.name || symbol.member) {
            return false;
        }
        if (symbol.fileWide) {
            return true;
        }
        return offset >= symbol.visibleFrom && offset <= symbol.visibleTo;
    });
}

function memberItems(doc, receiver, offset, external) {
    const items = [];
    const seen = new Set();
    function add(label, detail, documentation, kind) {
        if (seen.has(label)) {
            return;
        }
        seen.add(label);
        items.push({
            label,
            kind: kind || "method",
            detail,
            documentation: documentation || detail,
            insert: label,
            filter: `${label} ${label.toLowerCase()}`,
            sort: "1"
        });
    }
    const typeName = resolveType(doc, receiver, offset);
    const tables = [];
    if (typeName && TYPE_METHODS[typeName]) {
        tables.push(TYPE_METHODS[typeName]);
    }
    if (NAMESPACES[receiver]) {
        tables.push(NAMESPACES[receiver]);
    }
    if (NAMESPACES[typeName]) {
        tables.push(NAMESPACES[typeName]);
    }
    for (const table of tables) {
        for (const entry of table) {
            add(entry[0], entry[1], entry[2], "method");
        }
    }
    const pools = doc.symbols.concat(external || []);
    for (const symbol of pools) {
        if (!symbol.member || symbol.container !== receiver && symbol.container !== typeName) {
            continue;
        }
        add(symbol.name, symbol.detail || symbol.kind, symbol.detail, symbol.kind === "property" || symbol.kind === "enumMember" ? "field" : "method");
    }
    if (items.length === 0) {
        for (const entry of TYPE_METHODS.Any) {
            add(entry[0], entry[1], entry[2], "method");
        }
    }
    return items;
}

function resolveType(doc, name, offset) {
    if (TYPE_METHODS[name] || NAMESPACES[name]) {
        return name;
    }
    const matches = doc.symbols.filter((symbol) => symbol.name === name && !symbol.member && (symbol.fileWide || (offset >= symbol.visibleFrom && offset <= symbol.visibleTo)));
    matches.sort((left, right) => Number(left.fileWide) - Number(right.fileWide));
    if (matches[0] && matches[0].typeName) {
        return matches[0].typeName;
    }
    return "";
}

function packageItems(external) {
    const names = new Set(PACKAGES);
    for (const symbol of external || []) {
        if (symbol.kind === "namespace" || symbol.kind === "package") {
            if (symbol.detail && symbol.detail.startsWith("Package ")) {
                names.add(symbol.detail.slice("Package ".length));
            }
            if (symbol.typeName && symbol.typeName.includes(".")) {
                names.add(symbol.typeName);
            }
        }
    }
    return [...names].map((name) => ({
        label: name,
        kind: "namespace",
        detail: "package",
        documentation: `Package ${name}`,
        insert: name,
        filter: `${name} ${name.toLowerCase()}`,
        sort: "1"
    }));
}

function attributeItems(external) {
    const names = new Set(ATTRIBUTES);
    for (const symbol of external || []) {
        if (symbol.kind === "decorator" && symbol.name) {
            names.add(symbol.name);
        }
    }
    return [...names].map((name) => ({
        label: name,
        kind: "keyword",
        detail: "attribute",
        documentation: `Attribute @${name}`,
        insert: name,
        filter: `${name} ${name.toLowerCase()}`,
        sort: "1"
    }));
}

function hover(source, offset) {
    const doc = analyze(source);
    const token = tokenAt(doc.tokens, offset);
    if (!token) {
        return null;
    }
    if (token.kind === "keyword") {
        const info = keywordInfo(token.canonical || token.text);
        if (!info) {
            return null;
        }
        return { contents: `**${info.canonical}**\n\n${info.documentation}`, start: token.start, end: token.end };
    }
    if (token.kind !== "identifier") {
        return null;
    }
    const declaration = declarationAt(doc, token, offset);
    if (!declaration) {
        const builtin = BUILTIN_FUNCTIONS.find((item) => item[0] === token.text);
        if (builtin) {
            return { contents: `**${builtin[0]}** ${builtin[1]}\n\n${builtin[2]}`, start: token.start, end: token.end };
        }
        return null;
    }
    return {
        contents: `**${declaration.detail || declaration.name}**${declaration.container ? `\n\nIn ${declaration.container}` : ""}`,
        start: token.start,
        end: token.end
    };
}

function declarationAt(doc, token, offset) {
    const previous = previousToken(doc.tokens, token);
    if (previous && (previous.text === "." || previous.text === "?.")) {
        const receiver = previousToken(doc.tokens, previous);
        const typeName = receiver ? resolveType(doc, receiver.text, offset) : "";
        return doc.symbols.find((symbol) => symbol.member && symbol.name === token.text && (!typeName || symbol.container === typeName || symbol.container === receiver.text))
            || doc.symbols.find((symbol) => symbol.member && symbol.name === token.text);
    }
    const matches = doc.symbols.filter((symbol) => {
        if (symbol.name !== token.text || symbol.member) {
            return false;
        }
        if (symbol.token && offset >= symbol.start && offset <= symbol.end) {
            return true;
        }
        return symbol.fileWide || (offset >= symbol.visibleFrom && offset <= symbol.visibleTo);
    });
    matches.sort((left, right) => {
        if (left.fileWide !== right.fileWide) {
            return Number(left.fileWide) - Number(right.fileWide);
        }
        return (left.visibleTo - left.visibleFrom) - (right.visibleTo - right.visibleFrom);
    });
    return matches[0] || null;
}

function signature(source, offset) {
    const doc = analyze(source);
    let paren = -1;
    let depth = 0;
    const tokens = doc.tokens.filter((token) => token.kind !== "comment" && token.start < offset);
    for (let index = tokens.length - 1; index >= 0; index -= 1) {
        const token = tokens[index];
        if (token.text === ")") {
            depth += 1;
        } else if (token.text === "(") {
            if (depth === 0) {
                paren = index;
                break;
            }
            depth -= 1;
        }
    }
    if (paren < 0) {
        return null;
    }
    let name = "";
    for (let index = paren - 1; index >= 0; index -= 1) {
        const token = tokens[index];
        if (token.kind === "identifier" || token.kind === "keyword") {
            name = token.text;
            break;
        }
    }
    let active = 0;
    depth = 0;
    for (let index = paren + 1; index < tokens.length; index += 1) {
        const token = tokens[index];
        if (token.text === "(") {
            depth += 1;
        } else if (token.text === ")") {
            depth -= 1;
        } else if (token.text === "," && depth === 0) {
            active += 1;
        }
    }
    const symbol = doc.symbols.find((item) => item.name === name && item.params && item.params.length)
        || null;
    const builtin = knownCall(name);
    if (!symbol && !builtin) {
        return null;
    }
    const parameters = symbol
        ? symbol.params.map((param) => ({ label: param.label || param.name }))
        : splitSignature(builtin[1]);
    return {
        label: symbol ? symbol.detail : `${builtin[0]}${builtin[1]}`,
        documentation: symbol ? symbol.detail : builtin[2],
        parameters,
        active
    };
}

function knownCall(name) {
    const tables = [BUILTIN_FUNCTIONS, ...Object.values(TYPE_METHODS), ...Object.values(NAMESPACES)];
    for (const table of tables) {
        const found = table.find((item) => item[0] === name);
        if (found) {
            return found;
        }
    }
    return null;
}

function splitSignature(text) {
    const inside = text.slice(text.indexOf("(") + 1, text.indexOf(")"));
    if (!inside.trim()) {
        return [];
    }
    return inside.split(",").map((part) => ({ label: part.trim() }));
}

function definition(source, offset, external) {
    const doc = analyze(source, externalNames(external));
    const token = tokenAt(doc.tokens, offset);
    if (!token || (token.kind !== "identifier" && token.kind !== "keyword")) {
        return [];
    }
    const local = declarationAt(doc, token, offset);
    if (local && local.token) {
        return [local];
    }
    return (external || []).filter((symbol) => symbol.name === token.text && (symbol.exported || symbol.fileWide || symbol.kind === "namespace"));
}

function referencesIn(source, name, limit, options) {
    const { tokens } = analyze(source);
    const ranges = [];
    const includeMembers = Boolean(options && options.member);
    for (const token of tokens) {
        if (token.kind !== "identifier" || token.text !== name) {
            continue;
        }
        const previous = previousToken(tokens, token);
        const dotted = previous && (previous.text === "." || previous.text === "?.");
        if (includeMembers) {
            const own = options.ownStart != null && token.start === options.ownStart && token.end === options.ownEnd;
            if (!dotted && !own) {
                continue;
            }
        } else if (dotted) {
            continue;
        }
        if (limit && (token.start < limit.start || token.end > limit.end)) {
            continue;
        }
        ranges.push({ start: token.start, end: token.end });
    }
    return ranges;
}

function semanticSpans(source) {
    const doc = analyze(source);
    const data = [];
    let previousLine = 0;
    let previousCharacter = 0;
    for (const token of doc.tokens) {
        if (!token.text || token.kind === "eof") {
            continue;
        }
        let offset = token.start;
        while (offset < token.end) {
            const newline = source.indexOf("\n", offset);
            const pieceEnd = newline === -1 || newline >= token.end ? token.end : newline;
            const length = pieceEnd - offset;
            if (length > 0) {
                const position = positionAt(source, offset);
                const deltaLine = position.line - previousLine;
                const deltaCharacter = deltaLine === 0 ? position.character - previousCharacter : position.character;
                const type = Math.max(0, SEMANTIC_TYPES.indexOf(token.semType || "variable"));
                data.push(deltaLine, deltaCharacter, length, type, token.semMods || 0);
                previousLine = position.line;
                previousCharacter = position.character;
            }
            if (pieceEnd >= token.end) {
                break;
            }
            offset = pieceEnd + 1;
        }
    }
    return { data, doc };
}

function positionAt(source, offset) {
    let line = 0;
    let character = 0;
    for (let index = 0; index < offset && index < source.length; index += 1) {
        if (source[index] === "\n") {
            line += 1;
            character = 0;
        } else {
            character += 1;
        }
    }
    return { line, character };
}

function externalNames(external) {
    const names = new Set();
    for (const symbol of external || []) {
        if (symbol.name && (symbol.exported || symbol.fileWide || symbol.kind === "namespace")) {
            names.add(symbol.name);
        }
    }
    return names;
}

module.exports = {
    complete,
    hover,
    signature,
    definition,
    referencesIn,
    semanticSpans,
    tokenAt
};
