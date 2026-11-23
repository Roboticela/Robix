"use strict";

const { analyze } = require("./analyze");
const { toPascal, toPascalName, toCamel, isConstantCase, variablePartial } = require("./names");
const {
    KEYWORD_WORDS,
    KEYWORDS,
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
    const declared = declarationContext(line);
    if (declared && declared.kind === "namespace") {
        const items = packageItems(external);
        const corrected = toPascalName(declared.partial);
        if (!declared.partial.endsWith(".") && corrected !== declared.partial) {
            addCorrection(items, corrected, "namespace");
        }
        return {
            items,
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
    const items = scopeItems(doc, left, external);
    if (declared && declared.partial) {
        const corrected = toPascal(declared.partial);
        if (corrected !== declared.partial) {
            addCorrection(items, corrected, declared.kind);
        }
    } else {
        const variable = variablePartial(line);
        const corrected = toCamel(variable);
        if (variable && corrected !== variable && !isConstantCase(variable)) {
            addCorrection(items, corrected, "variable");
        }
    }
    return {
        items,
        replaceStart: word.start,
        replaceEnd: word.end
    };
}

function declarationContext(line) {
    const packaged = /(?:^|\s)(?:import|package|connect)\s+([A-Za-z_][A-Za-z0-9_.]*)$/i.exec(line);
    if (packaged) {
        return { kind: "namespace", partial: packaged[1] };
    }
    const alias = /(?:^|\s)as\s+([A-Za-z_][A-Za-z0-9_]*)$/i.exec(line);
    if (alias) {
        return { kind: "namespace", partial: alias[1] };
    }
    const named = /(?:^|\s)(function|macro|class|struct|interface|trait|enum|service|type|test|implement)\s+([A-Za-z_][A-Za-z0-9_]*)$/i.exec(line);
    if (!named) {
        return null;
    }
    const keyword = named[1].toLowerCase();
    const kind = keyword === "function" || keyword === "macro" || keyword === "test" ? "function" : "class";
    return { kind, partial: named[2] };
}

function addCorrection(items, label, kind) {
    if (!label || items.some((item) => item.label === label)) {
        return;
    }
    items.unshift({
        label,
        kind,
        detail: kind === "variable" ? "camelCase" : "PascalCase",
        documentation: `The name is written ${label}.`,
        insert: label,
        filter: `${label.toLowerCase()} ${label}`,
        sort: "0"
    });
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

function localSort(symbol) {
    if (symbol.kind === "variable" || symbol.kind === "parameter" || symbol.kind === "property" || symbol.kind === "enumMember") {
        return "1";
    }
    if (symbol.kind === "function" || symbol.kind === "method" || symbol.kind === "macro") {
        return "2";
    }
    return "4";
}

function packageOfNamespace(name) {
    const found = PACKAGES.find((item) => item.split(".").pop().toLowerCase() === String(name || "").toLowerCase());
    return found || `Robix.${name}`;
}

function libraryFunctions() {
    const items = [];
    for (const builtin of BUILTIN_FUNCTIONS) {
        items.push({
            name: builtin[0],
            signature: builtin[1],
            summary: builtin[2],
            namespace: "Core",
            packageName: "Robix.Core",
            kind: "function"
        });
    }
    for (const namespace of Object.keys(NAMESPACES)) {
        const packageName = packageOfNamespace(namespace);
        for (const entry of NAMESPACES[namespace]) {
            const signature = entry[1] || "";
            items.push({
                name: entry[0],
                signature,
                summary: entry[2] || "",
                namespace,
                packageName,
                kind: signature.includes("(") ? "function" : "field"
            });
        }
    }
    return items;
}

function packageImported(doc, packageName) {
    const names = new Set((doc.imports || []).map((item) => String(item).toLowerCase()));
    const lower = String(packageName || "").toLowerCase();
    if (!lower) {
        return false;
    }
    if (sameText(doc.packageName, packageName) || names.has(lower)) {
        return true;
    }
    return names.has(lower.split(".").pop());
}

function importInsertion(source, packageName) {
    if (!packageName) {
        return null;
    }
    const pattern = new RegExp(`(^|\\n)\\s*Import\\s+${packageName.replace(/\./g, "\\.")}\\b`, "i");
    if (pattern.test(source)) {
        return null;
    }
    const lines = String(source).split("\n");
    let anchor = -1;
    for (let index = 0; index < lines.length; index += 1) {
        if (/^\s*Package\b/i.test(lines[index]) || /^\s*Import\b/i.test(lines[index])) {
            anchor = index;
        }
    }
    const text = `Import ${packageName};\n`;
    if (anchor < 0) {
        return { start: 0, end: 0, text: `${text}\n` };
    }
    const start = lines.slice(0, anchor + 1).join("\n").length + 1;
    return { start, end: start, text };
}

function signatureParts(signature) {
    const text = String(signature || "");
    const arrow = text.lastIndexOf("->");
    const output = arrow >= 0 ? text.slice(arrow + 2).trim() : "Void";
    const open = text.indexOf("(");
    const close = text.lastIndexOf(")");
    const inside = open >= 0 && close > open ? text.slice(open + 1, close).trim() : "";
    return { input: inside || "None", output: output || "Void" };
}

function quoteField(value) {
    const text = String(value || "").trim();
    if (!text) {
        return '""';
    }
    if (text.startsWith("\"") || text.includes("=")) {
        return text;
    }
    return `"${text.replace(/"/g, "\\\"")}"`;
}

function explainDocument(symbol) {
    if (!symbol) {
        return "";
    }
    if (symbol.explain && symbol.explain.text) {
        const lines = [`**${symbol.name}**`];
        if (symbol.detail) {
            lines.push("", symbol.detail);
        }
        if (symbol.packageName) {
            lines.push("", `Package \`${symbol.packageName}\``);
        }
        lines.push("", "```robix", symbol.explain.text, "```");
        return lines.join("\n");
    }
    if (symbol.summary || symbol.signature) {
        return libraryDocument(symbol);
    }
    return "";
}

function libraryDocument(library) {
    const parts = signatureParts(library.signature);
    const description = library.summary || library.explain && library.explain.description || "";
    const lines = [`**${library.name}** \`${library.signature || ""}\``.trim()];
    if (library.packageName) {
        lines.push("", `Package \`${library.packageName}\``);
    }
    lines.push(
        "",
        "```robix",
        "Explain {",
        `    Description: ${quoteField(description)};`,
        `    Input: ${quoteField(parts.input)};`,
        `    Processing: ${quoteField(description)};`,
        `    Output: ${quoteField(parts.output)};`,
        "}",
        "```"
    );
    return lines.join("\n");
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
            filter: `${word.toLowerCase()} ${word}`,
            sort: info && info.isType ? "3" : "4"
        });
    }
    const typed = wordAt(doc.source, offset).text.toLowerCase();
    const typedKeyword = KEYWORDS.has(typed);
    for (const snippet of SNIPPETS) {
        if (typedKeyword && snippet.label.toLowerCase().startsWith(typed)) {
            continue;
        }
        add({
            label: snippet.label,
            kind: "snippet",
            detail: snippet.detail || "snippet",
            documentation: "Inserts a Robix snippet. Choose it explicitly; typing a keyword inserts only the keyword.",
            insert: snippet.insert,
            snippet: true,
            filter: `snippet:${snippet.label}`,
            sort: "9"
        });
    }
    for (const symbol of visibleSymbols(doc, offset)) {
        add(symbolItem(symbol, localSort(symbol)));
    }
    for (const library of libraryFunctions()) {
        const imported = packageImported(doc, library.packageName);
        add({
            label: library.name,
            kind: library.kind,
            detail: `${library.packageName} ${library.signature}`,
            documentation: libraryDocument(library),
            insert: library.name,
            filter: `${library.name.toLowerCase()} ${library.name}`,
            sort: imported ? "3" : "7",
            importEdit: imported ? null : importInsertion(doc.source, library.packageName)
        });
    }
    for (const typeName of BUILTIN_TYPES) {
        add({
            label: typeName,
            kind: "class",
            detail: "type",
            documentation: `${typeName} is a Robix standard type.`,
            insert: typeName,
            filter: `${typeName.toLowerCase()} ${typeName}`,
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
            filter: `${name.toLowerCase()} ${name}`,
            sort: "3"
        });
    }
    for (const symbol of external || []) {
        if (!(symbol.exported || symbol.fileWide || symbol.kind === "namespace" || symbol.kind === "package")) {
            continue;
        }
        const callable = symbol.kind === "function" || symbol.kind === "method" || symbol.kind === "macro";
        if (!callable) {
            add(symbolItem(symbol, "5"));
            continue;
        }
        const imported = !symbol.packageName || packageImported(doc, symbol.packageName) || sameText(doc.packageName, symbol.packageName);
        const item = symbolItem(symbol, imported ? "3" : "7");
        if (symbol.explain) {
            item.documentation = explainDocument(symbol);
        }
        if (symbol.packageName && !imported) {
            item.importEdit = importInsertion(doc.source, symbol.packageName);
        }
        add(item);
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
        documentation: explainDocument(symbol) || symbol.detail || "",
        insert: symbol.name,
        filter: `${symbol.name.toLowerCase()} ${symbol.name}`,
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
            filter: `${label.toLowerCase()} ${label}`,
            sort: "1"
        });
    }
    const typeName = resolveType(doc, receiver, offset);
    const tables = [];
    if (typeName && TYPE_METHODS[typeName]) {
        tables.push(TYPE_METHODS[typeName]);
    }
    const receiverNamespace = canonicalKey(NAMESPACES, receiver);
    const typeNamespace = canonicalKey(NAMESPACES, typeName);
    if (receiverNamespace) {
        tables.push(NAMESPACES[receiverNamespace]);
    }
    if (typeNamespace && typeNamespace !== receiverNamespace) {
        tables.push(NAMESPACES[typeNamespace]);
    }
    for (const table of tables) {
        for (const entry of table) {
            add(entry[0], entry[1], libraryDocument({
                name: entry[0],
                signature: entry[1],
                summary: entry[2],
                packageName: ""
            }), "method");
        }
    }
    const pools = doc.symbols.concat(external || []);
    for (const symbol of pools) {
        if (!symbol.member || !sameText(symbol.container, receiver) && !sameText(symbol.container, typeName)) {
            continue;
        }
        add(symbol.name, symbol.detail || symbol.kind, explainDocument(symbol) || symbol.detail, symbol.kind === "property" || symbol.kind === "enumMember" ? "field" : "method");
    }
    if (items.length === 0) {
        for (const entry of TYPE_METHODS.Any) {
            add(entry[0], entry[1], libraryDocument({
                name: entry[0],
                signature: entry[1],
                summary: entry[2],
                packageName: ""
            }), "method");
        }
    }
    return items;
}

function sameText(left, right) {
    return String(left || "").toLowerCase() === String(right || "").toLowerCase();
}

function canonicalKey(table, name) {
    if (!name || !table) {
        return "";
    }
    if (table[name]) {
        return name;
    }
    const lower = String(name).toLowerCase();
    return Object.keys(table).find((key) => key.toLowerCase() === lower) || "";
}

function resolveType(doc, name, offset) {
    const typed = canonicalKey(TYPE_METHODS, name);
    const namespace = canonicalKey(NAMESPACES, name);
    if (typed || namespace) {
        return typed || namespace;
    }
    const matches = doc.symbols.filter((symbol) => sameText(symbol.name, name) && !symbol.member && (symbol.fileWide || (offset >= symbol.visibleFrom && offset <= symbol.visibleTo)));
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
        filter: `${name.toLowerCase()} ${name}`,
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
        filter: `${name.toLowerCase()} ${name}`,
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
        const library = libraryFunctions().find((item) => sameText(item.name, token.text));
        if (!library) {
            return null;
        }
        return { contents: libraryDocument(library), start: token.start, end: token.end };
    }
    return {
        contents: explainDocument(declaration) || `**${declaration.detail || declaration.name}**${declaration.container ? `\n\nIn ${declaration.container}` : ""}`,
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
                const type = SEMANTIC_TYPES.indexOf(token.semType || "variable");
                if (type < 0) {
                    offset = pieceEnd >= token.end ? token.end : pieceEnd + 1;
                    if (pieceEnd >= token.end) {
                        break;
                    }
                    continue;
                }
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
