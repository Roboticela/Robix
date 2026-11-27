"use strict";

const fs = require("fs");
const path = require("path");
const { analyze } = require("./analyze");
const { tokenize } = require("./scanner");
const { formatRobix } = require("./format");
const { extractTests, functionNames } = require("./run-target");

const TARGETS = [
    "linux-x64", "linux-arm64", "linux-arm", "linux-riscv64", "linux-ppc64le",
    "linux-s390x", "linux-loongarch64", "windows-x64", "windows-arm64", "windows-x86"
];

function hostTarget() {
    const arch = { x64: "x64", arm64: "arm64", arm: "arm", ia32: "x86", ppc64: "ppc64le", s390x: "s390x", riscv64: "riscv64", loong64: "loongarch64" }[process.arch] || "x64";
    const os = process.platform === "win32" ? "windows" : "linux";
    const name = `${os}-${arch}`;
    return TARGETS.includes(name) ? name : "linux-x64";
}

function defaultConfiguration() {
    return {
        name: "Host",
        toolchain: "",
        target: hostTarget(),
        mode: "Debug",
        packagePath: ["Packages"],
        sourcePath: ["."],
        defines: [],
        forcedImports: [],
        entry: "Main",
        standard: "Robix"
    };
}

function normalizeConfigurations(parsed) {
    const list = parsed && Array.isArray(parsed.configurations) ? parsed.configurations : [];
    const configurations = list.length ? list.map((item) => ({ ...defaultConfiguration(), ...item })) : [defaultConfiguration()];
    const active = configurations.some((item) => item.name === (parsed && parsed.active))
        ? parsed.active
        : configurations[0].name;
    return { configurations, active };
}

function readConfigurations(file) {
    if (!file || !fs.existsSync(file)) {
        return normalizeConfigurations(null);
    }
    try {
        return normalizeConfigurations(JSON.parse(fs.readFileSync(file, "utf8")));
    } catch (error) {
        return normalizeConfigurations(null);
    }
}

function activeConfiguration(file) {
    const store = readConfigurations(file);
    return store.configurations.find((item) => item.name === store.active) || store.configurations[0];
}

function writeConfigurations(file, store) {
    fs.mkdirSync(path.dirname(file), { recursive: true });
    fs.writeFileSync(file, `${JSON.stringify(store, null, 4)}\n`);
}

function tokenAt(tokens, offset) {
    return tokens.find((token) => token.start <= offset && offset <= token.end && token.kind !== "eof");
}

function previousCode(tokens, token) {
    const index = tokens.indexOf(token);
    for (let cursor = index - 1; cursor >= 0; cursor -= 1) {
        if (tokens[cursor].kind !== "comment") {
            return tokens[cursor];
        }
    }
    return null;
}

function nextCode(tokens, token) {
    const index = tokens.indexOf(token);
    for (let cursor = index + 1; cursor < tokens.length; cursor += 1) {
        if (tokens[cursor].kind !== "comment") {
            return tokens[cursor];
        }
    }
    return null;
}

function highlights(source, offset) {
    const doc = analyze(source);
    const token = tokenAt(doc.tokens, offset);
    if (!token || token.kind !== "identifier") {
        return [];
    }
    const dotted = previousCode(doc.tokens, token);
    const member = dotted && (dotted.text === "." || dotted.text === "?.");
    return doc.tokens
        .filter((item) => item.kind === "identifier" && item.text === token.text)
        .filter((item) => {
            const previous = previousCode(doc.tokens, item);
            const isMember = previous && (previous.text === "." || previous.text === "?.");
            return member ? isMember : !isMember;
        })
        .map((item) => {
            const following = nextCode(doc.tokens, item);
            return {
                start: item.start,
                end: item.end,
                kind: following && (following.text === "=" || following.text === "+=" || following.text === "-=") ? "write" : "read"
            };
        });
}

function typeOfSymbol(symbol) {
    if (!symbol) {
        return "";
    }
    if (symbol.typeName) {
        return symbol.typeName;
    }
    const detail = String(symbol.detail || "");
    const arrow = detail.lastIndexOf("->");
    if (arrow >= 0) {
        return detail.slice(arrow + 2).trim();
    }
    const colon = detail.indexOf(":");
    if (colon >= 0) {
        return detail.slice(colon + 1).trim();
    }
    return "";
}

function findType(doc, name, external) {
    if (!name) {
        return [];
    }
    const pool = doc.symbols.concat(external || []);
    return pool.filter((symbol) => symbol.name === name && ["class", "struct", "enum", "interface", "trait", "service", "type"].includes(symbol.kind));
}

function implementsOf(source, symbol) {
    if (!symbol) {
        return [];
    }
    const slice = source.slice(symbol.start, Math.min(source.length, symbol.start + 400));
    const match = /(?:Implements|:)\s+([A-Za-z0-9_.,\s]+)/.exec(slice);
    if (!match) {
        return [];
    }
    return match[1].split(",").map((item) => item.trim()).filter((item) => /^[A-Za-z_]/.test(item) && item !== "Public" && item !== "Private");
}

function callHierarchyItem(symbol, uri) {
    if (!symbol || !["function", "method", "constructor"].includes(symbol.kind)) {
        return null;
    }
    return {
        name: symbol.name,
        kind: symbol.kind,
        detail: symbol.detail || "",
        uri: symbol.uri || uri,
        start: symbol.start || 0,
        end: symbol.end || (symbol.start || 0) + symbol.name.length,
        container: symbol.container || ""
    };
}

function outgoingCalls(source, symbol) {
    if (!symbol) {
        return [];
    }
    const doc = analyze(source);
    const bodyStart = symbol.start || 0;
    const bodyEnd = symbol.end || source.length;
    const calls = [];
    const seen = new Set();
    for (const token of doc.tokens) {
        if (token.start < bodyStart || token.end > bodyEnd || token.kind !== "identifier") {
            continue;
        }
        const following = nextCode(doc.tokens, token);
        if (!following || following.text !== "(") {
            continue;
        }
        if (token.text === symbol.name || seen.has(token.text)) {
            continue;
        }
        seen.add(token.text);
        const target = doc.symbols.find((item) => item.name === token.text && ["function", "method"].includes(item.kind));
        if (target) {
            calls.push({ symbol: target, start: token.start, end: token.end });
        }
    }
    return calls;
}

function incomingCalls(sources, name) {
    const hits = [];
    for (const entry of sources) {
        const doc = analyze(entry.source);
        for (const symbol of doc.symbols) {
            if (!["function", "method"].includes(symbol.kind)) {
                continue;
            }
            const calls = outgoingCalls(entry.source, symbol).filter((call) => call.symbol.name === name);
            if (calls.length) {
                hits.push({ uri: entry.uri, symbol, from: calls[0].start });
            }
        }
    }
    return hits;
}

function inlayHints(source) {
    const doc = analyze(source);
    const hints = [];
    for (const symbol of doc.symbols) {
        if (symbol.kind === "variable" && symbol.typeName && symbol.nameStart != null) {
            const line = source.slice(symbol.start, symbol.nameEnd || symbol.end || symbol.start);
            if (!line.includes(":")) {
                hints.push({
                    offset: symbol.nameEnd || symbol.end || symbol.start + symbol.name.length,
                    label: `: ${symbol.typeName}`,
                    kind: "type",
                    paddingLeft: true
                });
            }
        }
    }
    for (let index = 0; index < doc.tokens.length; index += 1) {
        const token = doc.tokens[index];
        if (token.kind !== "identifier") {
            continue;
        }
        const open = nextCode(doc.tokens, token);
        if (!open || open.text !== "(") {
            continue;
        }
        const symbol = doc.symbols.find((item) => item.name === token.text && item.params && item.params.length);
        if (!symbol) {
            continue;
        }
        const args = argumentStarts(doc.tokens, open);
        symbol.params.forEach((param, paramIndex) => {
            const arg = args[paramIndex];
            if (!arg || !param.name && !param.label) {
                return;
            }
            const label = String(param.label || param.name).split(/\s+/).pop();
            if (!label || source.slice(arg.start, arg.start + label.length + 1) === `${label}:`) {
                return;
            }
            hints.push({ offset: arg.start, label: `${label}:`, kind: "parameter", paddingRight: true });
        });
    }
    return hints;
}

function argumentStarts(tokens, open) {
    const starts = [];
    let depth = 0;
    let expect = true;
    const begin = tokens.indexOf(open);
    for (let index = begin; index < tokens.length; index += 1) {
        const token = tokens[index];
        if (token.kind === "comment") {
            continue;
        }
        if (token.text === "(") {
            depth += 1;
            if (depth === 1) {
                continue;
            }
        } else if (token.text === ")") {
            depth -= 1;
            if (depth === 0) {
                break;
            }
        } else if (token.text === "," && depth === 1) {
            expect = true;
            continue;
        }
        if (depth === 1 && expect) {
            starts.push(token);
            expect = false;
        }
    }
    return starts;
}

function missingTests(sourcePath, source) {
    if (!sourcePath || sourcePath.endsWith(".test.rx")) {
        return [];
    }
    const names = functionNames(source);
    const companion = sourcePath.replace(/\.rx$/, ".test.rx");
    const tests = fs.existsSync(companion) ? extractTests(fs.readFileSync(companion, "utf8")) : [];
    const covered = new Set(tests.map((test) => test.name));
    const doc = analyze(source);
    return names.filter((name) => !covered.has(name)).map((name) => {
        const symbol = doc.symbols.find((item) => item.name === name && ["function", "method"].includes(item.kind));
        return {
            code: "RX120",
            severity: "warning",
            message: `Companion has no test for '${name}'.`,
            start: symbol ? symbol.start : 0,
            end: symbol ? (symbol.nameEnd || symbol.start + name.length) : name.length,
            name
        };
    });
}

function unusedImports(source) {
    const doc = analyze(source);
    const used = new Set(doc.tokens.filter((token) => token.kind === "identifier").map((token) => token.text));
    const found = [];
    const pattern = /^[ \t]*Import\s+([A-Za-z0-9_.]+)\s*;/gm;
    let match = pattern.exec(source);
    while (match) {
        const leaf = match[1].split(".").pop();
        const count = doc.tokens.filter((token) => token.kind === "identifier" && token.text === leaf).length;
        if (count === 0 && !used.has(match[1])) {
            found.push({
                code: "RX202",
                severity: "info",
                message: `Unused import '${match[1]}'.`,
                start: match.index,
                end: match.index + match[0].length,
                packageName: match[1]
            });
        }
        match = pattern.exec(source);
    }
    return found;
}

function inactiveRegions(source, defines) {
    const enabled = new Set(defines || []);
    const lines = String(source).split("\n");
    const regions = [];
    const stack = [];
    lines.forEach((line, index) => {
        const start = /^\s*\/\/\s*#if\s+(\S+)/.exec(line);
        const end = /^\s*\/\/\s*#endif\b/.exec(line);
        if (start) {
            stack.push({ line: index, name: start[1], active: enabled.has(start[1]) || start[1] === "1" });
        } else if (end && stack.length) {
            const open = stack.pop();
            if (!open.active && index > open.line) {
                regions.push({ startLine: open.line, endLine: index });
            }
        }
    });
    return regions;
}

function analysisRules(source, disabled) {
    const off = new Set(disabled || []);
    const diagnostics = [];
    if (!off.has("RX901")) {
        const doc = analyze(source);
        for (const symbol of doc.symbols) {
            if (!["function", "method"].includes(symbol.kind)) {
                continue;
            }
            const body = source.slice(symbol.start, symbol.end || source.length);
            if (!/Description\s*:/.test(body)) {
                diagnostics.push({
                    code: "RX901",
                    severity: "info",
                    message: `${symbol.name} has no Explain description.`,
                    start: symbol.start,
                    end: symbol.nameEnd || symbol.start + symbol.name.length
                });
            }
        }
    }
    if (!off.has("RX900")) {
        String(source).split("\n").forEach((line, index) => {
            if (line.length > 120) {
                const start = String(source).split("\n").slice(0, index).join("\n").length + (index ? 1 : 0);
                diagnostics.push({
                    code: "RX900",
                    severity: "info",
                    message: "Line is longer than 120 characters.",
                    start,
                    end: start + line.length
                });
            }
        });
    }
    return diagnostics;
}

function loadRuleFile(file) {
    if (!file || !fs.existsSync(file)) {
        return [];
    }
    return fs.readFileSync(file, "utf8")
        .split(/\r?\n/)
        .map((line) => line.trim())
        .filter((line) => line.startsWith("off "))
        .map((line) => line.slice(4).trim());
}

function formatRange(source, start, end, indentSize) {
    const formatted = formatRobix(source, { indentSize });
    if (start <= 0 && end >= source.length) {
        return formatted;
    }
    return formatted;
}

function selectionRanges(source, offsets) {
    const { tokens } = tokenize(source);
    return offsets.map((offset) => {
        const token = tokenAt(tokens, offset) || { start: offset, end: offset };
        const ranges = [{ start: token.start, end: token.end }];
        const braces = braceRange(tokens, token.start);
        if (braces) {
            ranges.push(braces);
        }
        ranges.push({ start: 0, end: source.length });
        return ranges;
    });
}

function braceRange(tokens, offset) {
    const stack = [];
    let containing = null;
    for (const token of tokens) {
        if (token.text === "{") {
            stack.push(token);
        } else if (token.text === "}" && stack.length) {
            const open = stack.pop();
            if (open.start <= offset && token.end >= offset) {
                containing = { start: open.start, end: token.end };
            }
        }
    }
    return containing;
}

function markupDiagnostics(source) {
    const diagnostics = [];
    const stack = [];
    const tag = /<!--[\s\S]*?-->|<\/\s*([A-Za-z_][\w.]*)\s*>|<\s*([A-Za-z_][\w.]*)\b[^>]*?(\/?)>/g;
    let match = tag.exec(source);
    while (match) {
        if (match[0].startsWith("<!--")) {
            match = tag.exec(source);
            continue;
        }
        if (match[1]) {
            const open = stack.pop();
            if (!open || open.name !== match[1]) {
                diagnostics.push({
                    code: "RX006",
                    severity: "error",
                    message: `Unexpected '</${match[1]}>'.`,
                    start: match.index,
                    end: match.index + match[0].length
                });
            }
        } else if (!match[3]) {
            stack.push({ name: match[2], start: match.index, end: match.index + match[0].length });
        }
        match = tag.exec(source);
    }
    for (const open of stack) {
        diagnostics.push({
            code: "RX007",
            severity: "error",
            message: `Unclosed '<${open.name}>'.`,
            start: open.start,
            end: open.end
        });
    }
    if (/<!--/.test(source) && !/-->/.test(source)) {
        const start = source.indexOf("<!--");
        diagnostics.push({
            code: "RX007",
            severity: "error",
            message: "Unclosed comment.",
            start,
            end: start + 4
        });
    }
    return diagnostics;
}

function testSkeleton(name) {
    return `Test ${name} {\n    Constraint: True;\n    Processing: True;\n    Output: True;\n}\n`;
}

function readFormatOptions(root, fallbackIndent) {
    const options = {
        indentSize: Number(fallbackIndent) || 4,
        brace: "same",
        spaceBeforeParen: false,
        blankBetween: false
    };
    const file = root ? path.join(root, ".robix-format") : "";
    if (!file || !fs.existsSync(file)) {
        return options;
    }
    const text = fs.readFileSync(file, "utf8");
    const indent = /indent\s*[:=]\s*(\d+)/i.exec(text);
    if (indent) {
        options.indentSize = Number(indent[1]);
    }
    const brace = /brace\s*[:=]\s*(same|next)/i.exec(text);
    if (brace) {
        options.brace = brace[1].toLowerCase();
    }
    if (/spaceBeforeParen\s*[:=]\s*true/i.test(text)) {
        options.spaceBeforeParen = true;
    }
    if (/blankBetween\s*[:=]\s*true/i.test(text)) {
        options.blankBetween = true;
    }
    return options;
}

module.exports = {
    TARGETS,
    hostTarget,
    defaultConfiguration,
    readConfigurations,
    activeConfiguration,
    writeConfigurations,
    normalizeConfigurations,
    highlights,
    typeOfSymbol,
    findType,
    implementsOf,
    callHierarchyItem,
    outgoingCalls,
    incomingCalls,
    inlayHints,
    missingTests,
    unusedImports,
    inactiveRegions,
    analysisRules,
    loadRuleFile,
    formatRange,
    selectionRanges,
    markupDiagnostics,
    testSkeleton,
    readFormatOptions
};
