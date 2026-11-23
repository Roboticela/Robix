"use strict";

const vscode = require("vscode");
const path = require("path");
const fs = require("fs");
const os = require("os");
const { spawn } = require("child_process");
const { keywordCaseEdits, nameCaseEdits, tokenize } = require("./lib/scanner");
const { namingDiagnostic } = require("./lib/names");
const { formatRobix, formatProject } = require("./lib/format");
const { analyze } = require("./lib/analyze");
const { complete, hover, signature, definition, referencesIn, semanticSpans } = require("./lib/intellisense");
const { extractTests, functionNames, selectTests, buildCompanion, caseTitles } = require("./lib/run-target");
const { findTool } = require("./lib/tools");
const { SEMANTIC_TYPES, SEMANTIC_MODIFIERS } = require("./lib/keywords");

const legend = new vscode.SemanticTokensLegend(SEMANTIC_TYPES, SEMANTIC_MODIFIERS);
const index = new Map();
let output;
let status;
let fixing = false;
const generations = new Map();

function activate(context) {
    output = vscode.window.createOutputChannel("Robix");
    status = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    status.command = "robix.run";
    status.text = "$(play) Robix";
    status.tooltip = "Run (F5)  ·  Compile (Ctrl+F5)  ·  Release (Ctrl+Shift+F5)  ·  Check (Shift+F5)";
    context.subscriptions.push(output, status);

    context.subscriptions.push(
        vscode.languages.registerCompletionItemProvider("robix", completionProvider(), ".", "@"),
        vscode.languages.registerHoverProvider("robix", hoverProvider()),
        vscode.languages.registerSignatureHelpProvider("robix", signatureProvider(), "(", ","),
        vscode.languages.registerDefinitionProvider("robix", definitionProvider()),
        vscode.languages.registerReferenceProvider("robix", referenceProvider()),
        vscode.languages.registerRenameProvider("robix", renameProvider()),
        vscode.languages.registerDocumentSymbolProvider("robix", symbolProvider()),
        vscode.languages.registerFoldingRangeProvider("robix", foldingProvider()),
        vscode.languages.registerDocumentSemanticTokensProvider("robix", semanticProvider(), legend),
        vscode.languages.registerDocumentFormattingEditProvider("robix", formattingProvider(false)),
        vscode.languages.registerDocumentFormattingEditProvider("robix-project", formattingProvider(true)),
        vscode.languages.registerCodeLensProvider("robix", codeLensProvider()),
        vscode.commands.registerCommand("robix.run", () => runActive("run")),
        vscode.commands.registerCommand("robix.runSymbol", (target) => runSymbol(target)),
        vscode.commands.registerCommand("robix.compile", () => runActive("compile")),
        vscode.commands.registerCommand("robix.release", () => runActive("release")),
        vscode.commands.registerCommand("robix.check", () => {
            const document = activeDocument();
            if (document) {
                scheduleCheck(document, 0);
            }
        }),
        vscode.workspace.onDidOpenTextDocument((document) => {
            if (isRobix(document)) {
                remember(document);
                scheduleCheck(document, 50);
            }
        }),
        vscode.workspace.onDidChangeTextDocument((event) => {
            if (!isRobix(event.document)) {
                return;
            }
            correctKeywords(event);
            remember(event.document);
            scheduleCheck(event.document, 400);
        }),
        vscode.workspace.onDidSaveTextDocument((document) => {
            if (isRobix(document)) {
                remember(document);
                scheduleCheck(document, 0);
            }
        }),
        vscode.workspace.onDidCloseTextDocument((document) => {
            if (document.languageId === "robix") {
                diagnostics().delete(document.uri);
            }
        }),
        vscode.window.onDidChangeActiveTextEditor(updateStatus)
    );

    context.subscriptions.push(diagnostics());
    updateStatus();
    indexWorkspace();
    for (const document of vscode.workspace.textDocuments) {
        if (isRobix(document)) {
            remember(document);
            scheduleCheck(document, 50);
        }
    }
}

function deactivate() {
}

function diagnostics() {
    if (!diagnostics.collection) {
        diagnostics.collection = vscode.languages.createDiagnosticCollection("robix");
    }
    return diagnostics.collection;
}

function isRobix(document) {
    return document.languageId === "robix" || document.languageId === "robix-project";
}

function activeDocument() {
    const editor = vscode.window.activeTextEditor;
    if (!editor || !isRobix(editor.document)) {
        vscode.window.showInformationMessage("Open a Robix file to run it.");
        return null;
    }
    return editor.document;
}

function updateStatus() {
    const editor = vscode.window.activeTextEditor;
    if (editor && isRobix(editor.document)) {
        status.show();
    } else {
        status.hide();
    }
}

function config() {
    return vscode.workspace.getConfiguration("robix");
}

function correctKeywords(event) {
    if (fixing || event.document.languageId !== "robix" || !config().get("autoCorrectKeywords")) {
        return;
    }
    const edits = [];
    for (const change of event.contentChanges) {
        const text = event.document.getText();
        edits.push(...keywordCaseEdits(text, change.rangeOffset, change.text));
        edits.push(...nameCaseEdits(text, change.rangeOffset, change.text));
    }
    if (edits.length === 0) {
        return;
    }
    edits.sort((left, right) => right.start - left.start);
    const editor = vscode.window.visibleTextEditors.find((item) => item.document === event.document);
    if (!editor) {
        return;
    }
    fixing = true;
    editor.edit((builder) => {
        for (const edit of edits) {
            builder.replace(
                new vscode.Range(event.document.positionAt(edit.start), event.document.positionAt(edit.end)),
                edit.text
            );
        }
    }).then(() => {
        fixing = false;
    }, () => {
        fixing = false;
    });
}

function remember(document) {
    if (document.languageId !== "robix") {
        return;
    }
    try {
        const doc = analyze(document.getText());
        index.set(document.uri.toString(), doc.symbols.map((symbol) => ({
            ...symbol,
            token: undefined,
            uri: document.uri.toString()
        })));
    } catch (error) {
        output.appendLine(String(error && error.stack || error));
    }
}

async function indexWorkspace() {
    try {
        const files = await vscode.workspace.findFiles("**/*.rx", "{**/Build/**,**/node_modules/**}");
        for (const file of files) {
            if (index.has(file.toString())) {
                continue;
            }
            const bytes = await vscode.workspace.fs.readFile(file);
            const text = Buffer.from(bytes).toString("utf8");
            const doc = analyze(text);
            index.set(file.toString(), doc.symbols.map((symbol) => ({
                ...symbol,
                token: undefined,
                uri: file.toString()
            })));
        }
    } catch (error) {
        output.appendLine(String(error && error.stack || error));
    }
}

function externals(document) {
    const key = document.uri.toString();
    const symbols = [];
    for (const [uri, items] of index) {
        if (uri === key) {
            continue;
        }
        for (const symbol of items) {
            if (symbol.exported || symbol.fileWide || symbol.kind === "namespace" || symbol.kind === "enumMember") {
                symbols.push(symbol);
            }
        }
    }
    return symbols;
}

function completionProvider() {
    return {
        provideCompletionItems(document, position) {
            const offset = document.offsetAt(position);
            const result = complete(document.getText(), offset, externals(document));
            return result.items.map((item) => {
                const completion = new vscode.CompletionItem(item.label, completionKind(item.kind));
                completion.detail = item.detail;
                completion.documentation = item.documentation ? new vscode.MarkdownString(item.documentation) : undefined;
                completion.filterText = item.filter;
                completion.sortText = item.snippet ? `z${item.label}` : `${item.sort || "9"}${item.label}`;
                completion.preselect = false;
                completion.commitCharacters = [];
                completion.insertText = item.snippet ? new vscode.SnippetString(item.insert) : item.insert;
                completion.range = new vscode.Range(document.positionAt(result.replaceStart), document.positionAt(result.replaceEnd));
                if (item.importEdit) {
                    completion.additionalTextEdits = [vscode.TextEdit.replace(
                        new vscode.Range(document.positionAt(item.importEdit.start), document.positionAt(item.importEdit.end)),
                        item.importEdit.text
                    )];
                }
                return completion;
            });
        }
    };
}

function completionKind(kind) {
    const kinds = vscode.CompletionItemKind;
    return {
        keyword: kinds.Keyword,
        type: kinds.TypeParameter,
        class: kinds.Class,
        struct: kinds.Struct,
        interface: kinds.Interface,
        enum: kinds.Enum,
        function: kinds.Function,
        method: kinds.Method,
        field: kinds.Field,
        variable: kinds.Variable,
        namespace: kinds.Module,
        snippet: kinds.Snippet
    }[kind] || kinds.Text;
}

function hoverProvider() {
    return {
        provideHover(document, position) {
            const result = hover(document.getText(), document.offsetAt(position));
            if (!result) {
                return null;
            }
            return new vscode.Hover(
                new vscode.MarkdownString(result.contents),
                new vscode.Range(document.positionAt(result.start), document.positionAt(result.end))
            );
        }
    };
}

function signatureProvider() {
    return {
        provideSignatureHelp(document, position) {
            const result = signature(document.getText(), document.offsetAt(position));
            if (!result) {
                return null;
            }
            const help = new vscode.SignatureHelp();
            const info = new vscode.SignatureInformation(result.label, new vscode.MarkdownString(result.documentation || ""));
            info.parameters = result.parameters.map((parameter) => new vscode.ParameterInformation(parameter.label));
            help.signatures = [info];
            help.activeSignature = 0;
            help.activeParameter = Math.min(result.active, Math.max(0, info.parameters.length - 1));
            return help;
        }
    };
}

function definitionProvider() {
    return {
        async provideDefinition(document, position) {
            const hits = definition(document.getText(), document.offsetAt(position), externals(document));
            const locations = [];
            for (const symbol of hits) {
                locations.push(await locationOf(document, symbol));
            }
            return locations.filter(Boolean);
        }
    };
}

function referenceProvider() {
    return {
        async provideReferences(document, position, context) {
            const offset = document.offsetAt(position);
            const hits = definition(document.getText(), offset, externals(document));
            const symbol = hits[0];
            if (!symbol) {
                return [];
            }
            const locations = [];
            const files = symbol.exported
                ? [...index.keys()]
                : [document.uri.toString()];
            if (!files.includes(document.uri.toString())) {
                files.push(document.uri.toString());
            }
            for (const uri of files) {
                const doc = uri === document.uri.toString()
                    ? document
                    : await vscode.workspace.openTextDocument(vscode.Uri.parse(uri));
                const limit = symbol.fileWide || symbol.exported || symbol.member
                    ? null
                    : {
                        start: Math.min(symbol.start || 0, symbol.visibleFrom || 0),
                        end: Math.max(symbol.visibleTo || 0, symbol.end || 0, (symbol.start || 0) + symbol.name.length)
                    };
                const defining = symbol.uri === uri || (!symbol.uri && uri === document.uri.toString());
                for (const range of referencesIn(doc.getText(), symbol.name, limit, {
                    member: Boolean(symbol.member),
                    ownStart: defining ? symbol.start : null,
                    ownEnd: defining ? symbol.end : null
                })) {
                    locations.push(new vscode.Location(doc.uri, offsetRange(doc, range.start, range.end)));
                }
                if (context.includeDeclaration && symbol.uri === uri) {
                    const declaration = offsetRange(doc, symbol.start, symbol.end || symbol.start + symbol.name.length);
                    if (!locations.some((item) => item.range.isEqual(declaration))) {
                        locations.push(new vscode.Location(doc.uri, declaration));
                    }
                }
            }
            return locations;
        }
    };
}

function renameProvider() {
    return {
        prepareRename(document, position) {
            const offset = document.offsetAt(position);
            const token = tokenText(document.getText(), offset);
            if (!token || token.kind === "keyword") {
                throw new Error("Keywords keep their PascalCase spelling and cannot be renamed.");
            }
            return offsetRange(document, token.start, token.end);
        },
        async provideRenameEdits(document, position, newName) {
            const locations = await referenceProvider().provideReferences(document, position, { includeDeclaration: true });
            const edit = new vscode.WorkspaceEdit();
            for (const location of locations || []) {
                edit.replace(location.uri, location.range, newName);
            }
            return edit;
        }
    };
}

function tokenText(source, offset) {
    const { tokens } = tokenize(source);
    return tokens.find((token) => token.start <= offset && offset <= token.end && token.kind !== "eof");
}

function symbolProvider() {
    return {
        provideDocumentSymbols(document) {
            const doc = analyze(document.getText());
            const byName = new Map();
            const roots = [];
            for (const symbol of doc.symbols) {
                if (!symbol.name || symbol.kind === "parameter") {
                    continue;
                }
                const item = new vscode.DocumentSymbol(
                    symbol.name,
                    symbol.detail || symbol.kind,
                    symbolKind(symbol.kind),
                    symbolRange(document, symbol),
                    offsetRange(document, symbol.nameStart || symbol.start, symbol.nameEnd || symbol.end || symbol.start)
                );
                if (symbol.member && symbol.container && byName.has(symbol.container)) {
                    byName.get(symbol.container).children.push(item);
                } else {
                    roots.push(item);
                }
                if (!symbol.member) {
                    byName.set(symbol.name, item);
                }
            }
            return roots;
        }
    };
}

function symbolKind(kind) {
    const kinds = vscode.SymbolKind;
    return {
        function: kinds.Function,
        method: kinds.Method,
        class: kinds.Class,
        struct: kinds.Struct,
        interface: kinds.Interface,
        trait: kinds.Interface,
        enum: kinds.Enum,
        enumMember: kinds.EnumMember,
        service: kinds.Class,
        variable: kinds.Variable,
        property: kinds.Property,
        namespace: kinds.Namespace,
        type: kinds.TypeParameter,
        typeParameter: kinds.TypeParameter,
        macro: kinds.Function,
        parameter: kinds.Variable
    }[kind] || kinds.Object;
}

function foldingProvider() {
    return {
        provideFoldingRanges(document) {
            const { tokens } = tokenize(document.getText());
            const stack = [];
            const ranges = [];
            for (const token of tokens) {
                if (token.text === "{") {
                    stack.push(token);
                } else if (token.text === "}") {
                    const open = stack.pop();
                    if (open && token.line > open.line) {
                        ranges.push(new vscode.FoldingRange(open.line - 1, token.line - 1, vscode.FoldingRangeKind.Region));
                    }
                } else if (token.kind === "comment" && token.text.startsWith("/*") && token.line) {
                    const lines = token.text.split("\n").length;
                    if (lines > 1) {
                        ranges.push(new vscode.FoldingRange(token.line - 1, token.line - 2 + lines, vscode.FoldingRangeKind.Comment));
                    }
                }
            }
            return ranges;
        }
    };
}

function semanticProvider() {
    return {
        provideDocumentSemanticTokens(document) {
            try {
                const { data } = semanticSpans(document.getText());
                return new vscode.SemanticTokens(new Uint32Array(data));
            } catch (error) {
                output.appendLine(String(error && error.stack || error));
                return new vscode.SemanticTokens(new Uint32Array());
            }
        }
    };
}

function formattingProvider(project) {
    return {
        provideDocumentFormattingEdits(document) {
            const size = Number(config().get("format.indentSize")) || 4;
            const text = project ? formatProject(document.getText(), { indentSize: size }) : formatRobix(document.getText(), { indentSize: size });
            const last = document.lineCount - 1;
            const range = new vscode.Range(0, 0, last, document.lineAt(last).text.length);
            return [vscode.TextEdit.replace(range, text)];
        }
    };
}

function codeLensProvider() {
    return {
        provideCodeLenses(document) {
            const doc = analyze(document.getText());
            const lenses = [];
            for (const symbol of doc.symbols) {
                const isTest = String(symbol.detail || "").startsWith("Test ");
                const runnable = !isTest && symbol.name && (symbol.kind === "function" || symbol.kind === "method" || symbol.kind === "class");
                if (!runnable) {
                    continue;
                }
                const range = offsetRange(document, symbol.nameStart || symbol.start, symbol.nameEnd || symbol.end || symbol.start);
                lenses.push(new vscode.CodeLens(range, {
                    title: "$(play) Run",
                    command: "robix.runSymbol",
                    tooltip: `Run ${symbol.name} and its test cases`,
                    arguments: [{
                        uri: document.uri.toString(),
                        name: symbol.name,
                        kind: symbol.kind,
                        container: symbol.container || "",
                        line: range.start.line,
                        character: range.start.character
                    }]
                }));
            }
            return lenses;
        }
    };
}

function scheduleCheck(document, delay) {
    if (document.languageId !== "robix" || !config().get("diagnostics.enable")) {
        return;
    }
    const key = document.uri.toString();
    const generation = (generations.get(key) || 0) + 1;
    generations.set(key, generation);
    setTimeout(() => {
        if (generations.get(key) === generation) {
            checkDocument(document);
        }
    }, delay);
}

async function checkDocument(document) {
    const key = document.uri.toString();
    const generation = generations.get(key);
    const found = [];
    try {
        const doc = analyze(document.getText(), new Set(externals(document).map((symbol) => symbol.name)));
        for (const item of doc.diagnostics) {
            if (item.code === "RX005") {
                continue;
            }
            found.push(toDiagnostic(document, item));
        }
    } catch (error) {
        output.appendLine(String(error && error.stack || error));
    }
    const tool = interpreterPath(document);
    const root = projectRoot(document.uri.fsPath);
    if (tool) {
        const directory = path.join(os.tmpdir(), "robix-check");
        fs.mkdirSync(directory, { recursive: true });
        const temp = path.join(directory, path.basename(document.uri.fsPath || "Module.rx"));
        fs.writeFileSync(temp, document.getText());
        const result = await runProcess(tool, ["check", temp], root);
        if (result.code === 139) {
            const diagnostic = new vscode.Diagnostic(
                new vscode.Range(0, 0, 0, 1),
                "The Robix checker crashed. Local checks are still shown.",
                vscode.DiagnosticSeverity.Warning
            );
            found.push(diagnostic);
            output.appendLine(result.out);
        } else {
            found.push(...parseToolOutput(document, result.out));
        }
    }
    if (generations.get(key) === generation) {
        diagnostics().set(document.uri, dedupe(found));
    }
}

function toDiagnostic(document, item) {
    const start = document.positionAt(item.start || 0);
    const end = document.positionAt(Math.max(item.end || 0, (item.start || 0) + 1));
    const diagnostic = new vscode.Diagnostic(new vscode.Range(start, end), item.message, severity(item.severity));
    diagnostic.source = "Robix";
    diagnostic.code = item.code;
    return diagnostic;
}

function parseToolOutput(document, text) {
    const found = [];
    const pattern = /^(error|warning|info|note)\s+([A-Za-z][A-Za-z0-9]*)\s+(\d+):(\d+)\s+(.*)$/i;
    for (const line of String(text).split(/\r?\n/)) {
        const match = pattern.exec(line.replace(/\u001b\[[0-9;]*m/g, "").trim());
        if (!match || match[2] === "RX005") {
            continue;
        }
        const lineNumber = Math.max(0, Number(match[3]) - 1);
        const column = Math.max(0, Number(match[4]) - 1);
        if (lineNumber >= document.lineCount) {
            continue;
        }
        const textLine = document.lineAt(lineNumber).text;
        let endColumn = column + 1;
        while (endColumn < textLine.length && /[A-Za-z0-9_]/.test(textLine[endColumn])) {
            endColumn += 1;
        }
        const diagnostic = new vscode.Diagnostic(
            new vscode.Range(lineNumber, column, lineNumber, Math.max(endColumn, column + 1)),
            match[5],
            severity(match[1])
        );
        const naming = namingDiagnostic(match[2], match[5]);
        if (naming.drop) {
            continue;
        }
        diagnostic.source = "Robix";
        diagnostic.code = naming.code;
        diagnostic.message = naming.message;
        found.push(diagnostic);
    }
    return found;
}

function severity(value) {
    const name = String(value || "").toLowerCase();
    if (name === "error") {
        return vscode.DiagnosticSeverity.Error;
    }
    if (name === "info" || name === "note") {
        return vscode.DiagnosticSeverity.Information;
    }
    return vscode.DiagnosticSeverity.Warning;
}

function dedupe(items) {
    const seen = new Set();
    return items.filter((item) => {
        const key = `${item.range.start.line}:${item.range.start.character}:${item.message}`;
        if (seen.has(key)) {
            return false;
        }
        seen.add(key);
        return true;
    });
}

async function runActive(mode) {
    const document = activeDocument();
    if (!document) {
        return;
    }
    if (document.isDirty) {
        await document.save();
    }
    if (mode === "run") {
        await runDocument(document);
    } else {
        await buildDocument(document, mode === "release");
    }
}

async function runSymbol(target) {
    if (!target || !target.uri || !target.name) {
        return;
    }
    const uri = vscode.Uri.parse(target.uri);
    const document = await vscode.workspace.openTextDocument(uri);
    if (document.isDirty) {
        await document.save();
    }
    const sourcePath = implementationPath(document.uri.fsPath);
    if (!sourcePath || !fs.existsSync(sourcePath)) {
        vscode.window.showErrorMessage(`No Robix source was found for ${target.name}.`);
        return;
    }
    const source = fs.readFileSync(sourcePath, "utf8");
    const tests = loadTests(sourcePath, document);
    const selected = uniqueTests(selectTests(tests, target));
    const title = target.container && target.kind === "method" ? `${target.container}.${target.name}` : target.name;
    if (selected.length === 0) {
        output.show(true);
        output.appendLine(`Run ${title}`);
        output.appendLine(`${title} has no test cases.`);
        vscode.window.showInformationMessage(`${title} has no test cases.`);
        return;
    }
    const tool = compilerPath(document);
    if (!tool) {
        missingTool("compiler");
        return;
    }
    const directory = path.join(os.tmpdir(), "robix-run", `${Date.now()}`);
    fs.mkdirSync(directory, { recursive: true });
    const tempSource = path.join(directory, path.basename(sourcePath).replace(/\.test\.rx$/, ".rx"));
    fs.writeFileSync(tempSource, source);
    fs.writeFileSync(tempSource.replace(/\.rx$/, ".test.rx"), buildCompanion(selected, functionNames(source)));
    const destination = path.join(directory, "program.app");
    await vscode.window.withProgress({ location: vscode.ProgressLocation.Notification, title: `Run ${title}` }, async () => {
        output.show(true);
        output.appendLine(`Run ${title}`);
        for (const test of selected) {
            const cases = caseTitles(test);
            const heading = `${test.owner ? `${test.owner}.` : ""}${test.name}`;
            if (cases.length === 0) {
                output.appendLine(`  ${heading}`);
            }
            for (const item of cases) {
                output.appendLine(`  ${heading}  ${item}`);
            }
        }
        const result = await runProcess(tool, [tempSource, destination], projectRoot(sourcePath));
        const text = result.out || "";
        const problems = text.split(/\r?\n/).filter((line) => /^error\b/i.test(line.trim()) || /^FAIL\b/.test(line.trim()));
        const found = [];
        if (problems.length) {
            output.appendLine(problems.join("\n"));
            const parsed = parseToolOutput(document, problems.join("\n"));
            if (parsed.length) {
                found.push(...parsed);
            } else {
                const diagnostic = new vscode.Diagnostic(
                    new vscode.Range(target.line || 0, target.character || 0, target.line || 0, (target.character || 0) + target.name.length),
                    problems.join("\n"),
                    vscode.DiagnosticSeverity.Error
                );
                diagnostic.source = "Robix";
                diagnostic.code = "RX121";
                found.push(diagnostic);
            }
            vscode.window.showErrorMessage(`${title} failed. See the Robix output.`);
        } else {
            output.appendLine(`PASS ${title}`);
            vscode.window.showInformationMessage(`${title} passed.`);
        }
        const previous = diagnostics().get(document.uri) || [];
        const kept = [...previous].filter((item) => item.code !== "RX121");
        diagnostics().set(document.uri, kept.concat(found));
        for (const extra of [`${destination}`, `${destination}.ll`]) {
            if (fs.existsSync(extra)) {
                fs.unlinkSync(extra);
            }
        }
    });
}

function implementationPath(filePath) {
    if (filePath.endsWith(".test.rx")) {
        const source = filePath.slice(0, -".test.rx".length) + ".rx";
        if (fs.existsSync(source)) {
            return source;
        }
    }
    return filePath.endsWith(".rx") ? filePath : "";
}

function loadTests(sourcePath, document) {
    const chunks = [];
    const seen = new Set();
    function add(file) {
        if (!file || seen.has(file) || !fs.existsSync(file)) {
            return;
        }
        seen.add(file);
        chunks.push(fs.readFileSync(file, "utf8"));
    }
    add(sourcePath);
    add(sourcePath.replace(/\.rx$/, ".test.rx"));
    if (document && document.uri.fsPath !== sourcePath) {
        add(document.uri.fsPath);
    }
    return chunks.flatMap((text) => extractTests(text));
}

function uniqueTests(tests) {
    const seen = new Set();
    return tests.filter((test) => {
        if (seen.has(test.text)) {
            return false;
        }
        seen.add(test.text);
        return true;
    });
}

async function runDocument(document) {
    const tool = interpreterPath(document);
    const root = projectRoot(document.uri.fsPath);
    if (!tool) {
        missingTool("interpreter");
        return;
    }
    const terminal = vscode.window.terminals.find((item) => item.name === "Robix")
        || vscode.window.createTerminal({ name: "Robix", cwd: root });
    terminal.show(true);
    if (document.languageId === "robix-project") {
        terminal.sendText(`${shQuote(tool)} Run`);
        return;
    }
    const args = (config().get("run.arguments") || []).map(shQuote);
    terminal.sendText([shQuote(tool), "run", shQuote(document.uri.fsPath), ...args].join(" "));
}

async function buildDocument(document, release) {
    const root = projectRoot(document.uri.fsPath);
    const title = release ? "Robix Release" : "Robix Compile";
    await vscode.window.withProgress({ location: vscode.ProgressLocation.Notification, title }, async () => {
        output.show(true);
        if (document.languageId === "robix-project") {
            const tool = interpreterPath(document);
            if (!tool) {
                missingTool("interpreter");
                return;
            }
            const args = release
                ? ["Publish", "--Configuration", "Release"]
                : ["Build", "--Configuration", "Debug"];
            output.appendLine(`$ ${path.basename(tool)} ${args.join(" ")}`);
            const result = await runProcess(tool, args, root);
            output.append(result.out);
            if (result.out && !result.out.endsWith("\n")) {
                output.appendLine("");
            }
            notify(result.code === 0, release ? "Release finished." : "Project build finished.", "Project build failed.");
            return;
        }
        const tool = compilerPath(document);
        if (!tool) {
            missingTool("compiler");
            return;
        }
        const stem = path.basename(document.uri.fsPath, ".rx");
        const destination = release
            ? path.join(root, "Build", "Release", stem)
            : path.join(os.tmpdir(), "robix", `${stem}.app`);
        fs.mkdirSync(path.dirname(destination), { recursive: true });
        output.appendLine(`$ ${path.basename(tool)} ${document.uri.fsPath} ${destination}`);
        const result = await runProcess(tool, [document.uri.fsPath, destination], root);
        output.append(result.out);
        if (result.out && !result.out.endsWith("\n")) {
            output.appendLine("");
        }
        const problems = parseToolOutput(document, result.out);
        if (problems.length) {
            diagnostics().set(document.uri, dedupe(problems));
        }
        if (result.code === 0) {
            vscode.window.showInformationMessage(release
                ? `Release written to ${destination}`
                : `Temporary build written to ${destination}`);
        } else {
            vscode.window.showErrorMessage(release ? "Release failed. See the Robix output." : "Compile failed. See the Robix output.");
        }
    });
}

function notify(ok, success, failure) {
    if (ok) {
        vscode.window.showInformationMessage(success);
    } else {
        vscode.window.showErrorMessage(failure);
    }
}

function missingTool(kind) {
    const command = kind === "compiler" ? "Robixc" : "Robix";
    vscode.window.showErrorMessage(`The Robix ${kind} was not found on PATH. Install it or set robix.${kind}Path to the ${command} executable.`);
}

function interpreterPath(document) {
    return findTool({
        configured: config().get("interpreterPath"),
        names: ["Robix", "robix", "robix.exe", "robix-run", "robix-run.app"],
        root: projectRoot(document.uri.fsPath),
        relatives: ["Build/bootstrap/robix-run.app", "Build/Compiler/Robix.app"]
    });
}

function compilerPath(document) {
    return findTool({
        configured: config().get("compilerPath"),
        names: ["Robixc", "Robixc.cmd", "robixc", "robixc.exe", "robixc.app"],
        root: projectRoot(document.uri.fsPath),
        relatives: ["Build/bootstrap/robixc.app", "Build/Compiler/Robixc.bin"]
    });
}

function projectRoot(filePath) {
    let dir = path.dirname(filePath);
    for (let step = 0; step < 12; step += 1) {
        if (fs.existsSync(path.join(dir, "Robix.rxp")) || fs.existsSync(path.join(dir, "Runtime", "Runtime.cpp"))) {
            return dir;
        }
        const parent = path.dirname(dir);
        if (parent === dir) {
            break;
        }
        dir = parent;
    }
    const folder = vscode.workspace.getWorkspaceFolder(vscode.Uri.file(filePath));
    return folder ? folder.uri.fsPath : path.dirname(filePath);
}

function runProcess(command, args, cwd) {
    return new Promise((resolve) => {
        let out = "";
        let child;
        try {
            child = spawn(command, args, { cwd });
        } catch (error) {
            resolve({ code: 1, out: String(error) });
            return;
        }
        child.stdout.on("data", (chunk) => {
            out += chunk.toString();
        });
        child.stderr.on("data", (chunk) => {
            out += chunk.toString();
        });
        child.on("error", (error) => resolve({ code: 1, out: `${out}${error.message}\n` }));
        child.on("close", (code) => resolve({ code: code == null ? 1 : code, out }));
    });
}

function shQuote(value) {
    return `'${String(value).replace(/'/g, `'\\''`)}'`;
}

async function locationOf(document, symbol) {
    if (!symbol) {
        return null;
    }
    if (!symbol.uri || symbol.uri === document.uri.toString()) {
        return new vscode.Location(document.uri, symbolRange(document, symbol));
    }
    const other = await vscode.workspace.openTextDocument(vscode.Uri.parse(symbol.uri));
    return new vscode.Location(other.uri, symbolRange(other, symbol));
}

function symbolRange(document, symbol) {
    const start = symbol.start || 0;
    const end = Math.max(symbol.end || 0, start + (symbol.name ? symbol.name.length : 1));
    return offsetRange(document, start, end);
}

function offsetRange(document, start, end) {
    return new vscode.Range(document.positionAt(start), document.positionAt(Math.max(end, start)));
}

module.exports = { activate, deactivate };
