"use strict";

const fs = require("fs");
const path = require("path");
const { analyze } = require("../lib/analyze");
const { complete, hover, signature, definition, referencesIn, semanticSpans } = require("../lib/intellisense");
const {
    highlights,
    findType,
    typeOfSymbol,
    callHierarchyItem,
    outgoingCalls,
    inlayHints,
    missingTests,
    unusedImports,
    selectionRanges,
    formatRange,
    markupDiagnostics
} = require("../lib/features");
const { formatRobix, formatProject } = require("../lib/format");

const documents = new Map();
const cancelled = new Set();
let maxFileSize = 1024 * 1024;
let defines = [];
const logPath = process.argv.find((arg, index) => process.argv[index - 1] === "--log") || "";

function log(level, message) {
    const line = `${new Date().toISOString()} ${level} ${message}\n`;
    if (logPath) {
        try {
            fs.appendFileSync(logPath, line);
        } catch (error) {
            process.stderr.write(line);
        }
    }
    process.stderr.write(line);
}

function send(message) {
    const body = Buffer.from(JSON.stringify(message), "utf8");
    process.stdout.write(`Content-Length: ${body.length}\r\n\r\n`);
    process.stdout.write(body);
}

function reply(id, result) {
    if (cancelled.has(id)) {
        cancelled.delete(id);
        send({ jsonrpc: "2.0", id, error: { code: -32800, message: "cancelled" } });
        return;
    }
    send({ jsonrpc: "2.0", id, result });
}

function notify(method, params) {
    send({ jsonrpc: "2.0", method, params });
}

let buffer = Buffer.alloc(0);
process.stdin.on("data", (chunk) => {
    buffer = Buffer.concat([buffer, chunk]);
    while (true) {
        const headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return;
        }
        const header = buffer.slice(0, headerEnd).toString("utf8");
        const match = /Content-Length:\s*(\d+)/i.exec(header);
        if (!match) {
            buffer = buffer.slice(headerEnd + 4);
            continue;
        }
        const length = Number(match[1]);
        const start = headerEnd + 4;
        if (buffer.length < start + length) {
            return;
        }
        const body = buffer.slice(start, start + length).toString("utf8");
        buffer = buffer.slice(start + length);
        let message;
        try {
            message = JSON.parse(body);
        } catch (error) {
            log("Error", error.message);
            continue;
        }
        try {
            handle(message);
        } catch (error) {
            log("Error", error.stack || error.message);
            if (message.id != null) {
                send({ jsonrpc: "2.0", id: message.id, error: { code: -32603, message: String(error.message || error) } });
            }
        }
    }
});

function handle(message) {
    if (message.method === "$/cancelRequest") {
        cancelled.add(message.params && message.params.id);
        return;
    }
    if (message.method === "initialize") {
        const init = message.params && message.params.initializationOptions || {};
        if (init.maxFileSize) {
            maxFileSize = Number(init.maxFileSize);
        }
        reply(message.id, {
            capabilities: {
                textDocumentSync: 1,
                completionProvider: { triggerCharacters: [".", "("], resolveProvider: false },
                hoverProvider: true,
                signatureHelpProvider: { triggerCharacters: ["(", ","] },
                definitionProvider: true,
                declarationProvider: true,
                typeDefinitionProvider: true,
                implementationProvider: true,
                referencesProvider: true,
                renameProvider: { prepareProvider: true },
                documentHighlightProvider: true,
                documentSymbolProvider: true,
                workspaceSymbolProvider: true,
                foldingRangeProvider: true,
                selectionRangeProvider: true,
                semanticTokensProvider: {
                    legend: { tokenTypes: [], tokenModifiers: [] },
                    full: true
                },
                inlayHintProvider: true,
                callHierarchyProvider: true,
                typeHierarchyProvider: true,
                codeActionProvider: true,
                codeLensProvider: true,
                documentFormattingProvider: true,
                documentRangeFormattingProvider: true,
                documentOnTypeFormattingProvider: { firstTriggerCharacter: "}", moreTriggerCharacter: ["\n", ";"] },
                executeCommandProvider: { commands: ["robix.rescan"] }
            },
            serverInfo: { name: "robix-server" }
        });
        log("Info", "initialize");
        return;
    }
    if (message.method === "initialized" || message.method === "shutdown") {
        if (message.id != null) {
            reply(message.id, null);
        }
        if (message.method === "shutdown") {
            process.exit(0);
        }
        return;
    }
    if (message.method === "workspace/didChangeConfiguration") {
        const next = message.params && message.params.settings && message.params.settings.defines;
        if (Array.isArray(next)) {
            defines = next;
        }
        for (const uri of documents.keys()) {
            publish(uri);
        }
        if (message.id != null) {
            reply(message.id, null);
        }
        return;
    }
    if (message.method === "textDocument/didOpen" || message.method === "textDocument/didChange") {
        const doc = message.params.textDocument || message.params;
        const text = message.method === "textDocument/didOpen"
            ? message.params.textDocument.text
            : message.params.contentChanges[message.params.contentChanges.length - 1].text;
        const previous = documents.get(doc.uri);
        documents.set(doc.uri, {
            uri: doc.uri,
            text,
            version: doc.version || 0,
            languageId: doc.languageId || (previous && previous.languageId) || "robix"
        });
        if (previous && previous.packageName) {
            for (const [uri, item] of documents) {
                if (uri !== doc.uri && String(item.text).includes(previous.packageName)) {
                    item.analyzed = null;
                }
            }
        }
        publish(doc.uri);
        return;
    }
    if (message.method === "textDocument/didClose") {
        documents.delete(message.params.textDocument.uri);
        return;
    }
    if (message.method === "robix/rescan") {
        for (const item of documents.values()) {
            item.analyzed = null;
            publish(item.uri);
        }
        reply(message.id, documents.size);
        return;
    }
    if (message.id == null) {
        return;
    }
    const uri = message.params && message.params.textDocument && message.params.textDocument.uri;
    const item = uri ? documents.get(uri) : null;
    if (uri && item && item.text.length > maxFileSize) {
        reply(message.id, emptyResult(message.method));
        return;
    }
    reply(message.id, answer(message.method, message.params, item));
}

function emptyResult(method) {
    if (method === "textDocument/hover" || method === "textDocument/signatureHelp") {
        return null;
    }
    return [];
}

function analyzed(item) {
    if (!item.analyzed || item.analyzed.text !== item.text) {
        item.analyzed = analyze(item.text);
        item.packageName = item.analyzed.packageName;
    }
    return item.analyzed;
}

function offsetOf(text, position) {
    const lines = String(text).split("\n");
    let offset = 0;
    for (let line = 0; line < position.line && line < lines.length; line += 1) {
        offset += lines[line].length + 1;
    }
    return offset + position.character;
}

function publish(uri) {
    const item = documents.get(uri);
    if (!item) {
        return;
    }
    if (item.text.length > maxFileSize) {
        notify("textDocument/publishDiagnostics", { uri, diagnostics: [], syntaxOnly: true });
        return;
    }
    const language = item.languageId || "robix";
    const found = language === "robix" || language === "robix-project"
        ? analyzed(item).diagnostics.concat(missingOnDisk(uri, item.text), unusedImports(item.text))
        : markupDiagnostics(item.text);
    notify("textDocument/publishDiagnostics", {
        uri,
        diagnostics: found.map((entry) => ({
            range: rangeFromOffsets(item.text, entry.start || 0, entry.end || (entry.start || 0) + 1),
            severity: entry.severity === "error" ? 1 : entry.severity === "info" ? 3 : 2,
            source: "robix",
            code: entry.code,
            message: entry.message
        }))
    });
}

function missingOnDisk(uri, text) {
    const file = filePath(uri);
    if (!file) {
        return [];
    }
    return missingTests(file, text);
}

function filePath(uri) {
    if (!uri || !uri.startsWith("file://")) {
        return "";
    }
    return decodeURIComponent(uri.slice("file://".length));
}

function rangeFromOffsets(text, start, end) {
    return { start: positionAt(text, start), end: positionAt(text, end) };
}

function positionAt(text, offset) {
    let line = 0;
    let character = 0;
    for (let index = 0; index < offset && index < text.length; index += 1) {
        if (text[index] === "\n") {
            line += 1;
            character = 0;
        } else {
            character += 1;
        }
    }
    return { line, character };
}

function answer(method, params, item) {
    const text = item ? item.text : "";
    const position = params && params.position;
    const offset = position ? offsetOf(text, position) : 0;
    if (method === "textDocument/completion") {
        const result = complete(text, offset, externals(item && item.uri));
        return {
            isIncomplete: result.items.length === 0,
            items: result.items.map((entry) => ({
                label: entry.label,
                kind: 1,
                detail: entry.detail,
                documentation: entry.documentation,
                insertText: entry.insert,
                insertTextFormat: entry.snippet ? 2 : 1,
                filterText: entry.filter,
                sortText: entry.sort
            }))
        };
    }
    if (method === "textDocument/hover") {
        const result = hover(text, offset);
        return result ? { contents: { kind: "markdown", value: result.contents }, range: rangeFromOffsets(text, result.start, result.end) } : null;
    }
    if (method === "textDocument/signatureHelp") {
        const result = signature(text, offset);
        if (!result) {
            return null;
        }
        const signatures = (result.overloads && result.overloads.length ? result.overloads : [result]).map((entry) => ({
            label: entry.label,
            documentation: entry.documentation,
            parameters: (entry.parameters || []).map((parameter) => ({ label: parameter.label }))
        }));
        return { signatures, activeSignature: 0, activeParameter: result.active };
    }
    if (method === "textDocument/definition" || method === "textDocument/declaration") {
        return definition(text, offset, externals(item && item.uri)).map(locationOf);
    }
    if (method === "textDocument/typeDefinition") {
        const doc = analyzed(item);
        const hits = definition(text, offset, []);
        const typeName = typeOfSymbol(hits[0]);
        return findType(doc, typeName, externals(item && item.uri)).map(locationOf);
    }
    if (method === "textDocument/implementation") {
        return definition(text, offset, externals(item && item.uri)).map(locationOf);
    }
    if (method === "textDocument/references") {
        const hits = definition(text, offset, externals(item && item.uri));
        const symbol = hits[0];
        if (!symbol) {
            return [];
        }
        return referencesIn(text, symbol.name, null, { member: Boolean(symbol.member) }).map((range) => ({
            uri: item.uri,
            range: rangeFromOffsets(text, range.start, range.end)
        }));
    }
    if (method === "textDocument/documentHighlight") {
        return highlights(text, offset).map((entry) => ({
            range: rangeFromOffsets(text, entry.start, entry.end),
            kind: entry.kind === "write" ? 3 : 2
        }));
    }
    if (method === "textDocument/prepareRename") {
        const doc = analyzed(item);
        const token = (doc.tokens || []).find((entry) => entry.start <= offset && offset <= entry.end);
        if (!token || token.kind === "keyword") {
            throw new Error("Keywords keep their PascalCase spelling and cannot be renamed.");
        }
        return rangeFromOffsets(text, token.start, token.end);
    }
    if (method === "textDocument/rename") {
        const hits = definition(text, offset, []);
        const symbol = hits[0];
        if (!symbol) {
            return { changes: {} };
        }
        const edits = referencesIn(text, symbol.name, null, { member: Boolean(symbol.member) }).map((range) => ({
            range: rangeFromOffsets(text, range.start, range.end),
            newText: params.newName
        }));
        return { changes: { [item.uri]: edits } };
    }
    if (method === "textDocument/documentSymbol") {
        return analyzed(item).symbols.filter((symbol) => symbol.name && symbol.kind !== "parameter").map((symbol) => ({
            name: symbol.name,
            kind: 12,
            range: rangeFromOffsets(text, symbol.start || 0, symbol.end || symbol.start || 0),
            selectionRange: rangeFromOffsets(text, symbol.nameStart || symbol.start || 0, symbol.nameEnd || symbol.end || 0)
        }));
    }
    if (method === "workspace/symbol") {
        const query = String(params.query || "").toLowerCase();
        const symbols = [];
        for (const entry of documents.values()) {
            for (const symbol of analyzed(entry).symbols) {
                if (symbol.name && symbol.name.toLowerCase().includes(query)) {
                    symbols.push({
                        name: symbol.name,
                        kind: 12,
                        location: { uri: entry.uri, range: rangeFromOffsets(entry.text, symbol.start || 0, symbol.end || 0) }
                    });
                }
            }
        }
        return symbols;
    }
    if (method === "textDocument/foldingRange") {
        return [];
    }
    if (method === "textDocument/selectionRange") {
        const offsets = (params.positions || []).map((entry) => offsetOf(text, entry));
        return selectionRanges(text, offsets).map((ranges) => foldSelection(text, ranges));
    }
    if (method === "textDocument/semanticTokens/full") {
        const data = semanticSpans(text).data;
        return { data };
    }
    if (method === "textDocument/inlayHint") {
        return inlayHints(text).map((hint) => ({
            position: positionAt(text, hint.offset),
            label: hint.label,
            paddingLeft: Boolean(hint.paddingLeft),
            paddingRight: Boolean(hint.paddingRight)
        }));
    }
    if (method === "textDocument/prepareCallHierarchy") {
        const hits = definition(text, offset, []);
        const itemCall = callHierarchyItem(hits[0], item.uri);
        return itemCall ? [itemCall] : [];
    }
    if (method === "callHierarchy/outgoingCalls") {
        return outgoingCalls(text, params.item).map((call) => ({
            to: callHierarchyItem(call.symbol, item.uri),
            fromRanges: [rangeFromOffsets(text, call.start, call.end)]
        }));
    }
    if (method === "callHierarchy/incomingCalls") {
        return [];
    }
    if (method === "textDocument/formatting" || method === "textDocument/rangeFormatting" || method === "textDocument/onTypeFormatting") {
        const indent = params.options && params.options.tabSize || 4;
        const formatted = item && item.languageId === "robix-project" ? formatProject(text, { indentSize: indent }) : formatRange(text, 0, text.length, indent);
        return [{ range: rangeFromOffsets(text, 0, text.length), newText: formatted }];
    }
    if (method === "textDocument/codeLens" || method === "textDocument/codeAction") {
        return [];
    }
    return null;
}

function foldSelection(text, ranges) {
    let node = { range: rangeFromOffsets(text, ranges[0].start, ranges[0].end) };
    let cursor = node;
    for (let index = 1; index < ranges.length; index += 1) {
        cursor.parent = { range: rangeFromOffsets(text, ranges[index].start, ranges[index].end) };
        cursor = cursor.parent;
    }
    return node;
}

function externals(uri) {
    const symbols = [];
    for (const [key, item] of documents) {
        if (key === uri) {
            continue;
        }
        for (const symbol of analyzed(item).symbols) {
            if (symbol.exported || symbol.fileWide || symbol.kind === "namespace") {
                symbols.push(symbol);
            }
        }
    }
    return symbols;
}

function locationOf(symbol) {
    return {
        uri: symbol.uri || "",
        range: {
            start: { line: 0, character: 0 },
            end: { line: 0, character: symbol.name ? symbol.name.length : 1 }
        },
        name: symbol.name
    };
}

process.on("uncaughtException", (error) => {
    log("Error", error.stack || error.message);
});

log("Info", `robix-server started defines=${defines.join(",")}`);
