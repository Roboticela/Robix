"use strict";

const vscode = require("vscode");
const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawn } = require("child_process");
const { analyze } = require("./analyze");
const { formatRobix, formatProject } = require("./format");
const { extractTests, caseTitles } = require("./run-target");
const { setPackageRoot, clearPackageCache } = require("./packages");
const {
    TARGETS,
    readConfigurations,
    activeConfiguration,
    writeConfigurations,
    defaultConfiguration,
    highlights,
    findType,
    typeOfSymbol,
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
    selectionRanges,
    markupDiagnostics,
    testSkeleton,
    readFormatOptions
} = require("./features");

const configurationProviders = [];
let server;
let serverRestarts = 0;
let owns = false;
let languageChannel;
let intellisenseStatus;
let projectStatus;
let lastDiagnostics = new Map();
let dimDecoration;
let extensionContext;
let projectFiles = [];
let activeProject = "";
let previewPanel;
let previewDocument;
let configureTimer;
const services = {};

function registerConfigurationProvider(provider) {
    configurationProviders.push(provider);
    return { dispose: () => configurationProviders.splice(configurationProviders.indexOf(provider), 1) };
}

function start(context, given) {
    extensionContext = context;
    Object.assign(services, given);
    languageChannel = vscode.window.createOutputChannel("Robix Language");
    intellisenseStatus = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 40);
    intellisenseStatus.command = "robix.selectConfiguration";
    projectStatus = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 50);
    projectStatus.command = "robix.selectTarget";
    dimDecoration = vscode.window.createTextEditorDecorationType({ opacity: "0.45" });
    context.subscriptions.push(
        languageChannel,
        intellisenseStatus,
        projectStatus,
        dimDecoration,
        ...registerCommands(context),
        ...registerProviders(),
        registerTasks(),
        registerTests(context),
        registerDebug(),
        vscode.workspace.createFileSystemWatcher("**/*.{rx,rxp,test.rx}"),
        vscode.workspace.createFileSystemWatcher("**/Packages/**"),
        vscode.workspace.onDidChangeConfiguration(() => {
            sendConfiguration();
            refreshStatus();
        }),
        vscode.workspace.onDidOpenTextDocument(checkMarkup),
        vscode.workspace.onDidChangeTextDocument((event) => {
            checkMarkup(event.document);
            syncDocument(event.document);
            paintInactive(vscode.window.activeTextEditor);
        }),
        vscode.window.onDidChangeActiveTextEditor((editor) => {
            refreshStatus();
            paintInactive(editor);
            bindFolderPackages();
        }),
        vscode.workspace.onDidSaveTextDocument((document) => {
            refreshPreview(document);
            const file = document.uri.fsPath || "";
            if (file.endsWith(".rxp") || file.includes(`${path.sep}Packages${path.sep}`)) {
                clearTimeout(configureTimer);
                configureTimer = setTimeout(() => configureProject(), 400);
            }
        }),
        vscode.tasks.onDidStartTask((event) => {
            if (event.execution.task.source !== "robix") {
                return;
            }
            const editor = vscode.window.activeTextEditor;
            if (!editor) {
                return;
            }
            const previous = services.diagnostics().get(editor.document.uri) || [];
            services.diagnostics().set(editor.document.uri, [...previous].filter((item) => item.source !== "robixc"));
        }),
        vscode.window.registerTreeDataProvider("robix.project", projectTree())
    );
    const watcher = vscode.workspace.createFileSystemWatcher("**/*.rx");
    watcher.onDidChange((uri) => {
        if (String(uri.fsPath).includes(`${path.sep}Packages${path.sep}`)) {
            clearPackageCache();
        }
    });
    context.subscriptions.push(watcher);
    refreshStatus();
    loadProject();
    bindFolderPackages();
    launchServer(context);
    for (const document of vscode.workspace.textDocuments) {
        checkMarkup(document);
    }
}

function ownsDiagnostics() {
    return owns;
}

function configFile(folder) {
    const root = folder ? folder.uri.fsPath : workspaceRoot();
    return path.join(root, ".vscode", "robix.json");
}

function workspaceRoot() {
    const editor = vscode.window.activeTextEditor;
    if (editor) {
        const folder = vscode.workspace.getWorkspaceFolder(editor.document.uri);
        if (folder) {
            return folder.uri.fsPath;
        }
    }
    const folders = vscode.workspace.workspaceFolders || [];
    return folders.length ? folders[0].uri.fsPath : os.homedir();
}

function bindFolderPackages() {
    const root = workspaceRoot();
    if (fs.existsSync(path.join(root, "Packages"))) {
        setPackageRoot(root);
    }
}

function currentConfiguration() {
    for (const provider of configurationProviders) {
        if (provider && provider.configuration) {
            return provider.configuration();
        }
    }
    return activeConfiguration(configFile());
}

function refreshStatus() {
    const configuration = currentConfiguration();
    const database = compileDatabase();
    const editor = vscode.window.activeTextEditor;
    const large = editor && editor.document.getText().length > maxFileSize();
    projectStatus.text = `$(file-code) ${configuration.name}  ${configuration.target}  ${configuration.mode}`;
    projectStatus.tooltip = "Select a Robix target";
    projectStatus.show();
    intellisenseStatus.text = large
        ? "$(warning) Robix syntax only"
        : `$(database) Robix ${owns ? "idle" : "local"}  ${database.length} files`;
    intellisenseStatus.tooltip = large
        ? "This file is over robix.maxFileSize. The language service left it to syntax coloring."
        : "Select a Robix configuration";
    intellisenseStatus.show();
}

function maxFileSize() {
    return Number(services.config().get("maxFileSize")) || 1048576;
}

function trusted() {
    return vscode.workspace.isTrusted !== false;
}

function compileDatabase() {
    const file = path.join(workspaceRoot(), "Build", "compile_commands.json");
    if (!fs.existsSync(file)) {
        return [];
    }
    try {
        const parsed = JSON.parse(fs.readFileSync(file, "utf8"));
        return Array.isArray(parsed) ? parsed : [];
    } catch (error) {
        return [];
    }
}

function packagePathFor(filePath) {
    const record = compileDatabase().find((item) => item.file === filePath);
    if (record && record.packagePath && record.packagePath.length) {
        const root = record.directory || path.dirname(filePath);
        const candidate = path.resolve(root, record.packagePath[0]);
        if (fs.existsSync(candidate)) {
            setPackageRoot(path.dirname(candidate) === root ? root : path.dirname(candidate));
        }
        return record.packagePath;
    }
    return currentConfiguration().packagePath;
}

function registerCommands(context) {
    const bind = (id, handler) => vscode.commands.registerCommand(id, handler);
    return [
        bind("robix.restartServer", () => launchServer(context, true)),
        bind("robix.rescan", () => rescan()),
        bind("robix.logDiagnostics", () => {
            const file = logFile();
            languageChannel.appendLine(file);
            languageChannel.show();
            vscode.window.showInformationMessage(file);
        }),
        bind("robix.resetDatabase", () => resetDatabase()),
        bind("robix.selectConfiguration", () => selectConfiguration()),
        bind("robix.editConfigurations", () => editConfigurations()),
        bind("robix.selectTarget", () => selectField("target", TARGETS)),
        bind("robix.selectMode", () => selectField("mode", ["Debug", "Release"])),
        bind("robix.configure", () => configureProject()),
        bind("robix.build", () => services.build(false)),
        bind("robix.rebuild", () => services.build(false, true)),
        bind("robix.clean", () => cleanOutput()),
        bind("robix.switchSourceTest", () => switchSourceTest()),
        bind("robix.runCodeAnalysis", () => runAnalysis()),
        bind("robix.fixAll", () => fixAll()),
        bind("robix.runTestsInFile", () => runTestsInFile()),
        bind("robix.debugTestAtCursor", () => debugAtCursor()),
        bind("robix.preview", () => previewActive()),
        bind("robix.discoverToolchain", () => discoverToolchain()),
        bind("robix.selectProject", () => loadProject(true)),
        bind("robix.debug.start", () => startDebug(false)),
        bind("robix.showReferences", (uri, position, locations) => {
            const editor = vscode.window.activeTextEditor;
            if (editor) {
                return vscode.commands.executeCommand("editor.action.showReferences", uri || editor.document.uri, position || editor.selection.active, locations || []);
            }
            return undefined;
        })
    ];
}

function registerProviders() {
    return [
        vscode.languages.registerDocumentHighlightProvider("robix", {
            provideDocumentHighlights(document, position) {
                return highlights(document.getText(), document.offsetAt(position)).map((item) => new vscode.DocumentHighlight(
                    offsetRange(document, item.start, item.end),
                    item.kind === "write" ? vscode.DocumentHighlightKind.Write : vscode.DocumentHighlightKind.Read
                ));
            }
        }),
        vscode.languages.registerDeclarationProvider("robix", definitionForward()),
        vscode.languages.registerTypeDefinitionProvider("robix", {
            async provideTypeDefinition(document, position) {
                const hits = await definitionLocations(document, position);
                const doc = analyze(document.getText());
                const symbol = doc.symbols.find((item) => item.name && hits.some((hit) => hit.range.contains(document.positionAt(item.start || 0))));
                const types = findType(doc, typeOfSymbol(symbol), []);
                return types.map((item) => new vscode.Location(document.uri, offsetRange(document, item.start || 0, item.end || item.start || 0)));
            }
        }),
        vscode.languages.registerImplementationProvider("robix", {
            provideImplementation(document, position) {
                const doc = analyze(document.getText());
                const offset = document.offsetAt(position);
                const symbol = doc.symbols.find((item) => item.start <= offset && offset <= (item.end || item.start + item.name.length));
                if (!symbol) {
                    return [];
                }
                const name = symbol.kind === "interface" || symbol.kind === "trait" ? symbol.name : symbol.container;
                return doc.symbols
                    .filter((item) => ["class", "struct", "service"].includes(item.kind) && implementsOf(document.getText(), item).includes(name || symbol.name))
                    .map((item) => new vscode.Location(document.uri, offsetRange(document, item.start || 0, item.end || 0)));
            }
        }),
        vscode.languages.registerWorkspaceSymbolProvider({
            provideWorkspaceSymbols(query) {
                const needle = String(query || "").toLowerCase();
                const symbols = [];
                for (const document of vscode.workspace.textDocuments) {
                    if (document.languageId !== "robix") {
                        continue;
                    }
                    for (const symbol of analyze(document.getText()).symbols) {
                        if (symbol.name && symbol.name.toLowerCase().includes(needle) && symbol.kind !== "parameter") {
                            symbols.push(new vscode.SymbolInformation(
                                symbol.name,
                                symbolKind(symbol.kind),
                                symbol.container || "",
                                new vscode.Location(document.uri, offsetRange(document, symbol.start || 0, symbol.end || 0))
                            ));
                        }
                    }
                }
                return symbols;
            }
        }),
        vscode.languages.registerSelectionRangeProvider("robix", {
            provideSelectionRanges(document, positions) {
                const ranges = selectionRanges(document.getText(), positions.map((position) => document.offsetAt(position)));
                return ranges.map((list) => {
                    let node = new vscode.SelectionRange(offsetRange(document, list[0].start, list[0].end));
                    let cursor = node;
                    for (let index = 1; index < list.length; index += 1) {
                        cursor.parent = new vscode.SelectionRange(offsetRange(document, list[index].start, list[index].end));
                        cursor = cursor.parent;
                    }
                    return node;
                });
            }
        }),
        vscode.languages.registerInlayHintsProvider("robix", {
            provideInlayHints(document) {
                if (!services.config().get("inlayHints.parameterNames") && !services.config().get("inlayHints.types")) {
                    return [];
                }
                return inlayHints(document.getText()).filter((hint) => {
                    if (hint.kind === "parameter") {
                        return services.config().get("inlayHints.parameterNames") !== false;
                    }
                    return services.config().get("inlayHints.types") !== false;
                }).map((hint) => {
                    const position = document.positionAt(hint.offset);
                    const label = new vscode.InlayHint(position, hint.label);
                    label.paddingLeft = Boolean(hint.paddingLeft);
                    label.paddingRight = Boolean(hint.paddingRight);
                    label.kind = hint.kind === "type" ? vscode.InlayHintKind.Type : vscode.InlayHintKind.Parameter;
                    return label;
                });
            }
        }),
        vscode.languages.registerCallHierarchyProvider("robix", {
            prepareCallHierarchy(document, position) {
                const symbol = symbolAt(document, position);
                const item = callHierarchyItem(symbol, document.uri.toString());
                return item ? [toCallItem(document, item)] : [];
            },
            provideCallHierarchyOutgoingCalls(item) {
                const document = vscode.workspace.textDocuments.find((entry) => entry.uri.toString() === item.uri.toString());
                if (!document) {
                    return [];
                }
                const symbol = analyze(document.getText()).symbols.find((entry) => entry.name === item.name);
                return outgoingCalls(document.getText(), symbol).map((call) => new vscode.CallHierarchyOutgoingCall(
                    toCallItem(document, callHierarchyItem(call.symbol, document.uri.toString())),
                    [offsetRange(document, call.start, call.end)]
                ));
            },
            async provideCallHierarchyIncomingCalls(item) {
                const files = await vscode.workspace.findFiles("**/*.rx", "{**/Build/**,**/node_modules/**}", 200);
                const sources = [];
                for (const file of files) {
                    const bytes = await vscode.workspace.fs.readFile(file);
                    sources.push({ uri: file.toString(), source: Buffer.from(bytes).toString("utf8") });
                }
                return incomingCalls(sources, item.name).map((hit) => new vscode.CallHierarchyIncomingCall(
                    toCallItemUri(hit.uri, hit.symbol),
                    [new vscode.Range(new vscode.Position(0, 0), new vscode.Position(0, 1))]
                ));
            }
        }),
        vscode.languages.registerTypeHierarchyProvider("robix", {
            prepareTypeHierarchy(document, position) {
                const symbol = symbolAt(document, position);
                if (!symbol || !["class", "struct", "interface", "trait", "service", "enum"].includes(symbol.kind)) {
                    return [];
                }
                return [toTypeItem(document, symbol)];
            },
            provideTypeHierarchySupertypes(item) {
                const document = openCached(item.uri);
                if (!document) {
                    return [];
                }
                const symbol = analyze(document.getText()).symbols.find((entry) => entry.name === item.name);
                return implementsOf(document.getText(), symbol).map((name) => {
                    const found = analyze(document.getText()).symbols.find((entry) => entry.name === name);
                    return found ? toTypeItem(document, found) : new vscode.TypeHierarchyItem(vscode.SymbolKind.Interface, name, "", item.uri, item.range, item.selectionRange);
                });
            },
            provideTypeHierarchySubtypes(item) {
                const document = openCached(item.uri);
                if (!document) {
                    return [];
                }
                return analyze(document.getText()).symbols
                    .filter((entry) => implementsOf(document.getText(), entry).includes(item.name))
                    .map((entry) => toTypeItem(document, entry));
            }
        }),
        vscode.languages.registerCodeActionsProvider("robix", codeActions(), { providedCodeActionKinds: [vscode.CodeActionKind.QuickFix, vscode.CodeActionKind.Refactor] }),
        vscode.languages.registerDocumentRangeFormattingEditProvider(["robix", "robix-project"], {
            provideDocumentRangeFormattingEdits(document) {
                return [fullFormat(document)];
            }
        }),
        vscode.languages.registerOnTypeFormattingEditProvider("robix", {
            provideOnTypeFormattingEdits(document) {
                if (services.config().get("format.onType") !== true) {
                    return [];
                }
                return [fullFormat(document)];
            }
        }, "}", "\n", ";")
    ];
}

function codeActions() {
    return {
        provideCodeActions(document, range) {
            const actions = [];
            const text = document.getText();
            const diagnostics = services.diagnostics().get(document.uri) || [];
            for (const item of diagnostics) {
                if (!item.range.intersection(range) && item.range.start.line !== range.start.line) {
                    continue;
                }
                if (item.code === "RX120") {
                    const name = /'([^']+)'/.exec(item.message);
                    if (name) {
                        const action = new vscode.CodeAction(`Create test ${name[1]}`, vscode.CodeActionKind.QuickFix);
                        action.diagnostics = [item];
                        action.edit = testEdit(document, name[1]);
                        actions.push(action);
                    }
                }
                if (item.code === "RX202") {
                    const action = new vscode.CodeAction("Remove unused import", vscode.CodeActionKind.QuickFix);
                    action.diagnostics = [item];
                    action.edit = new vscode.WorkspaceEdit();
                    action.edit.delete(document.uri, item.range);
                    actions.push(action);
                }
                if (item.code === "RX200") {
                    const found = /'([^']+)'/.exec(item.message);
                    const packageName = found ? packageExporting(found[1]) : "";
                    if (packageName) {
                        const action = new vscode.CodeAction(`Import ${packageName}`, vscode.CodeActionKind.QuickFix);
                        action.diagnostics = [item];
                        action.edit = importEdit(document, packageName);
                        actions.push(action);
                    }
                }
            }
            for (const item of unusedImports(text)) {
                if (document.offsetAt(range.start) < item.start || document.offsetAt(range.start) > item.end) {
                    continue;
                }
            }
            const organize = new vscode.CodeAction("Organize imports", vscode.CodeActionKind.Refactor);
            organize.edit = organizeImports(document);
            actions.push(organize);
            const selected = document.getText(range);
            if (selected && range.start.line === range.end.line && !selected.includes("\n")) {
                const extract = new vscode.CodeAction("Extract variable", vscode.CodeActionKind.RefactorExtract);
                extract.edit = new vscode.WorkspaceEdit();
                const line = document.lineAt(range.start.line);
                extract.edit.insert(document.uri, line.range.start, `${" ".repeat(line.firstNonWhitespaceCharacterIndex)}Var extracted = ${selected};\n`);
                extract.edit.replace(document.uri, range, "extracted");
                actions.push(extract);
            }
            if (selected && selected.includes("\n")) {
                const extract = new vscode.CodeAction("Extract function", vscode.CodeActionKind.RefactorExtract);
                extract.edit = new vscode.WorkspaceEdit();
                extract.edit.insert(document.uri, range.start, `Function Extracted() -> Int\nExplain {\n    Description: "Extracted.";\n    Input: None;\n    Processing: "Extracted statements.";\n    Output: "Zero.";\n} {\n${selected}\n    Return 0;\n}\n\n`);
                extract.edit.replace(document.uri, range, "Extracted();\n");
                actions.push(extract);
            }
            const explain = new vscode.CodeAction("Generate Explain", vscode.CodeActionKind.Refactor);
            explain.edit = new vscode.WorkspaceEdit();
            explain.edit.insert(document.uri, range.start, "Explain {\n    Description: \"Describe this.\";\n    Input: None;\n    Processing: \"Steps.\";\n    Output: \"Result.\";\n}\n");
            actions.push(explain);
            for (const word of ["If", "While", "For", "Match"]) {
                const surround = new vscode.CodeAction(`Surround with ${word}`, vscode.CodeActionKind.Refactor);
                surround.edit = new vscode.WorkspaceEdit();
                const body = selected || document.lineAt(range.start.line).text;
                surround.edit.replace(document.uri, selected ? range : document.lineAt(range.start.line).range, `${word} (True) {\n    ${body.trim()}\n}`);
                actions.push(surround);
            }
            return actions;
        }
    };
}

function packageExporting(name) {
    const packages = path.join(workspaceRoot(), "Packages");
    if (!name || !fs.existsSync(packages)) {
        return "";
    }
    for (const dir of fs.readdirSync(packages)) {
        const leaf = dir.split(".").pop();
        const file = path.join(packages, dir, `${leaf}.rx`);
        if (!fs.existsSync(file)) {
            continue;
        }
        const text = fs.readFileSync(file, "utf8");
        if (new RegExp(`(?:Function|Class|Struct|Interface|Trait|Enum)\\s+${name}\\b`).test(text)) {
            return dir;
        }
    }
    return "";
}

function importEdit(document, packageName) {
    const edit = new vscode.WorkspaceEdit();
    const lines = document.getText().split("\n");
    if (lines.some((line) => new RegExp(`^\\s*Import\\s+${packageName.replace(/\./g, "\\.")}\\b`).test(line))) {
        return edit;
    }
    let anchor = 0;
    lines.forEach((line, index) => {
        if (/^\s*Package\b/.test(line) || /^\s*Import\b/.test(line)) {
            anchor = index + 1;
        }
    });
    edit.insert(document.uri, new vscode.Position(anchor, 0), `Import ${packageName};\n`);
    return edit;
}

function testEdit(document, name) {
    const edit = new vscode.WorkspaceEdit();
    const companion = document.uri.fsPath.endsWith(".test.rx")
        ? document.uri
        : vscode.Uri.file(document.uri.fsPath.replace(/\.rx$/, ".test.rx"));
    edit.insert(companion, new vscode.Position(0, 0), testSkeleton(name));
    return edit;
}

function organizeImports(document) {
    const lines = document.getText().split("\n");
    const imports = [];
    const rest = [];
    for (const line of lines) {
        if (/^\s*Import\s+/.test(line)) {
            imports.push(line.trim());
        } else {
            rest.push(line);
        }
    }
    imports.sort();
    const edit = new vscode.WorkspaceEdit();
    const packageIndex = rest.findIndex((line) => /^\s*Package\b/.test(line));
    const kept = rest.filter((line) => !/^\s*Import\s+/.test(line));
    if (packageIndex >= 0) {
        kept.splice(packageIndex + 1, 0, "", ...imports);
    } else {
        kept.unshift(...imports, "");
    }
    const last = document.lineCount - 1;
    edit.replace(document.uri, new vscode.Range(0, 0, last, document.lineAt(last).text.length), `${kept.join("\n")}\n`);
    return edit;
}

function fullFormat(document) {
    const options = readFormatOptions(workspaceRoot(), services.config().get("format.indentSize"));
    const size = options.indentSize;
    const project = document.languageId === "robix-project";
    const text = project ? formatProject(document.getText(), { indentSize: size }) : formatRobix(document.getText(), { indentSize: size });
    const last = document.lineCount - 1;
    return vscode.TextEdit.replace(new vscode.Range(0, 0, last, document.lineAt(last).text.length), text);
}

function definitionForward() {
    return {
        provideDeclaration(document, position) {
            return definitionLocations(document, position);
        }
    };
}

async function definitionLocations(document, position) {
    const { definition } = require("./intellisense");
    const hits = definition(document.getText(), document.offsetAt(position), []);
    return hits.map((symbol) => new vscode.Location(document.uri, offsetRange(document, symbol.start || 0, symbol.end || 0)));
}

function symbolAt(document, position) {
    const offset = document.offsetAt(position);
    return analyze(document.getText()).symbols.find((symbol) => (symbol.nameStart || symbol.start) <= offset && offset <= (symbol.nameEnd || symbol.end || symbol.start));
}

function toCallItem(document, item) {
    const symbol = new vscode.CallHierarchyItem(
        symbolKind(item.kind),
        item.name,
        item.detail,
        document.uri,
        offsetRange(document, item.start, item.end),
        offsetRange(document, item.start, item.start + item.name.length)
    );
    return symbol;
}

function toCallItemUri(uri, symbol) {
    const location = vscode.Uri.parse(uri);
    return new vscode.CallHierarchyItem(
        symbolKind(symbol.kind),
        symbol.name,
        symbol.detail || "",
        location,
        new vscode.Range(0, 0, 0, 1),
        new vscode.Range(0, 0, 0, 1)
    );
}

function toTypeItem(document, symbol) {
    return new vscode.TypeHierarchyItem(
        symbolKind(symbol.kind),
        symbol.name,
        symbol.detail || "",
        document.uri,
        offsetRange(document, symbol.start || 0, symbol.end || 0),
        offsetRange(document, symbol.nameStart || symbol.start || 0, symbol.nameEnd || symbol.start || 0)
    );
}

function openCached(uri) {
    return vscode.workspace.textDocuments.find((document) => document.uri.toString() === uri.toString());
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
        variable: kinds.Variable,
        namespace: kinds.Namespace
    }[kind] || kinds.Object;
}

function shellFor(name) {
    const folder = workspaceRoot();
    const configuration = currentConfiguration();
    const editor = vscode.window.activeTextEditor;
    const file = editor && (editor.document.languageId === "robix" || editor.document.languageId === "robix-project")
        ? editor.document.uri.fsPath
        : "";
    const probe = vscode.Uri.file(file || path.join(folder, "Robix.rx"));
    const compiler = services.compilerPath({ uri: probe });
    const interpreter = services.interpreterPath({ uri: probe });
    const target = configuration.target || "linux-x64";
    if ((name === "Build" || name === "Rebuild" || name === "Release") && compiler && file.endsWith(".rx")) {
        const dest = path.join(folder, "Build", name === "Release" ? "Release" : target, path.basename(file, ".rx"));
        fs.mkdirSync(path.dirname(dest), { recursive: true });
        return new vscode.ShellExecution(compiler, [file, dest, "--target", target], { cwd: folder });
    }
    if (name === "Run" && interpreter && file) {
        const args = file.endsWith(".rxp") ? ["Run"] : ["run", file];
        return new vscode.ShellExecution(interpreter, args, { cwd: folder });
    }
    if (name === "Check" && interpreter && file.endsWith(".rx")) {
        return new vscode.ShellExecution(interpreter, ["check", file], { cwd: folder });
    }
    if (name === "Clean") {
        return new vscode.ShellExecution("rm", ["-rf", path.join(folder, "Build", target)], { cwd: folder });
    }
    if (name === "Configure") {
        return new vscode.ShellExecution("echo", ["Use Robix: Configure to write Build/compile_commands.json."], { cwd: folder });
    }
    return new vscode.ShellExecution("echo", [`Robix ${name}`], { cwd: folder });
}

function registerTasks() {
    return vscode.tasks.registerTaskProvider("robix", {
        provideTasks() {
            return ["Configure", "Build", "Rebuild", "Clean", "Release", "Check", "Run", "Test"].map((name) => {
                const task = new vscode.Task(
                    { type: "robix", task: name },
                    vscode.TaskScope.Workspace,
                    name,
                    "robix",
                    shellFor(name)
                );
                task.presentationOptions = {
                    reveal: vscode.TaskRevealKind.Always,
                    panel: vscode.TaskPanelKind.Shared,
                    clear: false
                };
                if (name === "Build") {
                    task.group = vscode.TaskGroup.Build;
                }
                if (name === "Test") {
                    task.group = vscode.TaskGroup.Test;
                }
                task.problemMatchers = ["robix"];
                return task;
            });
        },
        resolveTask(task) {
            return task;
        }
    });
}

function registerTests() {
    const controller = vscode.tests.createTestController("robix.tests", "Robix");
    controller.resolveHandler = async (item) => {
        if (!item) {
            await discoverTests(controller);
        }
    };
    controller.createRunProfile("Run", vscode.TestRunProfileKind.Run, (request, token) => executeTests(controller, request, token, false), true);
    controller.createRunProfile("Debug", vscode.TestRunProfileKind.Debug, (request, token) => executeTests(controller, request, token, true));
    const watch = vscode.workspace.createFileSystemWatcher("**/*.test.rx");
    watch.onDidChange(() => discoverTests(controller));
    watch.onDidCreate(() => discoverTests(controller));
    return controller;
}

async function discoverTests(controller) {
    const files = await vscode.workspace.findFiles("**/*.test.rx", "{**/Build/**,**/node_modules/**}");
    controller.items.replace([]);
    for (const file of files) {
        const bytes = await vscode.workspace.fs.readFile(file);
        const text = Buffer.from(bytes).toString("utf8");
        const root = controller.createTestItem(file.toString(), path.basename(file.fsPath), file);
        for (const test of extractTests(text)) {
            const id = `${file.fsPath}:${test.name}`;
            const item = controller.createTestItem(id, test.name, file);
            item.range = new vscode.Range(Math.max(0, (test.line || 1) - 1), 0, Math.max(0, (test.line || 1) - 1), test.name.length);
            for (const title of caseTitles(test)) {
                const child = controller.createTestItem(`${id}:${title}`, title, file);
                item.children.add(child);
            }
            root.children.add(item);
        }
        controller.items.add(root);
    }
}

async function executeTests(controller, request, token, debug) {
    const run = controller.createTestRun(request);
    const items = [];
    collectTests(request.include.length ? request.include : [...controller.items], items);
    for (const item of items) {
        if (token.isCancellationRequested) {
            break;
        }
        if (item.children.size) {
            continue;
        }
        run.started(item);
        if (debug) {
            await startDebug(false, item.uri.fsPath);
            run.passed(item);
            continue;
        }
        const document = await vscode.workspace.openTextDocument(item.uri);
        const tool = services.compilerPath(document);
        if (!trusted() || !tool) {
            run.failed(item, new vscode.TestMessage(trusted() ? "The Robix compiler was not found." : "Trust this workspace before running tests."));
            continue;
        }
        const result = await services.runProcess(tool, ["check", item.uri.fsPath], services.projectRoot(item.uri.fsPath));
        if (result.code === 0 && !/^FAIL\b/m.test(result.out || "")) {
            run.passed(item);
        } else {
            run.failed(item, new vscode.TestMessage(result.out || "The test failed."));
        }
    }
    run.end();
}

function collectTests(list, into) {
    for (const item of list) {
        into.push(item);
        collectTests([...item.children], into);
    }
}

function registerDebug() {
    return vscode.debug.registerDebugConfigurationProvider("robix", {
        provideDebugConfigurations() {
            return [launchTemplate("${file}")];
        },
        resolveDebugConfiguration(folder, config) {
            const editor = vscode.window.activeTextEditor;
            const program = config.program || (editor ? editor.document.uri.fsPath : "");
            const next = {
                ...launchTemplate(program),
                ...config,
                type: "robix",
                request: config.request || "launch",
                program,
                cwd: config.cwd || (program ? path.dirname(program) : workspaceRoot()),
                target: currentConfiguration().target,
                mode: currentConfiguration().mode,
                justMyCode: services.config().get("debug.justMyCode") !== false,
                interpreterPath: program ? services.interpreterPath({ uri: vscode.Uri.file(program) }) : "",
                compilerPath: program ? services.compilerPath({ uri: vscode.Uri.file(program) }) : "",
                args: config.args && config.args.length ? config.args : (services.config().get("run.arguments") || [])
            };
            if (!trusted()) {
                vscode.window.showWarningMessage("Trust this workspace before Robix launches a toolchain.");
                return undefined;
            }
            return next;
        }
    });
}

function launchTemplate(program) {
    return {
        type: "robix",
        request: "launch",
        name: "Robix",
        program,
        args: [],
        cwd: "${workspaceFolder}",
        stopOnEntry: false,
        target: hostTargetSafe(),
        mode: "Debug",
        console: "integratedTerminal",
        justMyCode: true
    };
}

function hostTargetSafe() {
    return currentConfiguration().target;
}

async function startDebug(noDebug, program) {
    const editor = vscode.window.activeTextEditor;
    const file = program || (editor ? editor.document.uri.fsPath : "");
    if (!file) {
        return;
    }
    await vscode.debug.startDebugging(vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders[0], {
        ...launchTemplate(file),
        noDebug: Boolean(noDebug),
        program: file
    });
}

function launchServer(context, restart) {
    stopServer();
    if (!trusted()) {
        languageChannel.appendLine("The workspace is untrusted. The language server stays on the local checker.");
        owns = false;
        refreshStatus();
        return;
    }
    const configured = String(services.config().get("languageServer.path") || "").trim();
    const script = path.join(__dirname, "..", "server", "server.js");
    const command = configured || process.execPath;
    const args = configured ? [] : [script, "--log", logFile()];
    const trace = String(services.config().get("languageServer.trace") || "Info");
    languageChannel.appendLine(`${restart ? "Restart" : "Start"} ${command} ${args.join(" ")} trace=${trace}`);
    try {
        server = spawn(command, args, { cwd: workspaceRoot() });
    } catch (error) {
        languageChannel.appendLine(String(error));
        owns = false;
        return;
    }
    let pending = Buffer.alloc(0);
    const waiting = new Map();
    server.stdout.on("data", (chunk) => {
        pending = Buffer.concat([pending, chunk]);
        while (true) {
            const headerEnd = pending.indexOf("\r\n\r\n");
            if (headerEnd < 0) {
                return;
            }
            const header = pending.slice(0, headerEnd).toString("utf8");
            const match = /Content-Length:\s*(\d+)/i.exec(header);
            if (!match) {
                pending = pending.slice(headerEnd + 4);
                continue;
            }
            const length = Number(match[1]);
            const start = headerEnd + 4;
            if (pending.length < start + length) {
                return;
            }
            const message = JSON.parse(pending.slice(start, start + length).toString("utf8"));
            pending = pending.slice(start + length);
            onServer(message, waiting);
        }
    });
    server.stderr.on("data", (chunk) => languageChannel.append(chunk.toString()));
    server.on("exit", (code) => {
        owns = false;
        languageChannel.appendLine(`robix-server exited ${code}`);
        if (code && serverRestarts < 3) {
            serverRestarts += 1;
            launchServer(context, true);
        }
        refreshStatus();
    });
    server.send = (message) => {
        const body = Buffer.from(JSON.stringify(message), "utf8");
        if (server && server.stdin.writable) {
            server.stdin.write(`Content-Length: ${body.length}\r\n\r\n`);
            server.stdin.write(body);
        }
    };
    server.send({
        jsonrpc: "2.0",
        id: 1,
        method: "initialize",
        params: {
            processId: process.pid,
            rootUri: vscode.Uri.file(workspaceRoot()).toString(),
            initializationOptions: { maxFileSize: maxFileSize(), trace },
            capabilities: {}
        }
    });
    waiting.set(1, () => {
        owns = true;
        serverRestarts = 0;
        server.send({ jsonrpc: "2.0", method: "initialized", params: {} });
        sendConfiguration();
        for (const document of vscode.workspace.textDocuments) {
            syncDocument(document);
        }
        refreshStatus();
        languageChannel.appendLine(logFile());
    });
    server.waiting = waiting;
    context.subscriptions.push({ dispose: stopServer });
}

function onServer(message, waiting) {
    if (message.id && waiting.has(message.id)) {
        const done = waiting.get(message.id);
        waiting.delete(message.id);
        done(message.result);
        return;
    }
    if (message.method === "textDocument/publishDiagnostics") {
        const uri = vscode.Uri.parse(message.params.uri);
        if (message.params.syntaxOnly) {
            intellisenseStatus.text = "$(warning) Robix syntax only";
            return;
        }
        const diagnostics = (message.params.diagnostics || []).map((item) => {
            const diagnostic = new vscode.Diagnostic(
                new vscode.Range(item.range.start.line, item.range.start.character, item.range.end.line, item.range.end.character),
                item.message,
                item.severity === 1 ? vscode.DiagnosticSeverity.Error : item.severity === 3 ? vscode.DiagnosticSeverity.Information : vscode.DiagnosticSeverity.Warning
            );
            diagnostic.source = "robix";
            diagnostic.code = item.code;
            return diagnostic;
        });
        lastDiagnostics.set(uri.toString(), diagnostics);
        services.diagnostics().set(uri, diagnostics);
        refreshStatus();
    }
}

function syncDocument(document) {
    if (!server || !owns || !["robix", "robix-project", "robix-ui", "robix-3d"].includes(document.languageId)) {
        return;
    }
    if (document.getText().length > maxFileSize()) {
        intellisenseStatus.text = "$(warning) Robix syntax only";
        return;
    }
    server.send({
        jsonrpc: "2.0",
        method: "textDocument/didOpen",
        params: {
            textDocument: {
                uri: document.uri.toString(),
                languageId: document.languageId,
                version: document.version,
                text: document.getText()
            }
        }
    });
}

function sendConfiguration() {
    if (!server || !owns) {
        return;
    }
    const configuration = currentConfiguration();
    server.send({
        jsonrpc: "2.0",
        method: "workspace/didChangeConfiguration",
        params: { settings: { defines: configuration.defines || [] } }
    });
}

function stopServer() {
    if (server) {
        server.kill();
        server = null;
    }
    owns = false;
}

function logFile() {
    return path.join(os.tmpdir(), "robix-language.log");
}

async function rescan() {
    clearPackageCache();
    if (services.clearIndex) {
        services.clearIndex();
    }
    if (services.indexWorkspace) {
        await services.indexWorkspace();
    }
    if (server && owns) {
        server.send({ jsonrpc: "2.0", id: 2, method: "robix/rescan", params: {} });
    }
    vscode.window.showInformationMessage("Robix rescanned the workspace.");
}

async function resetDatabase() {
    clearPackageCache();
    const cache = path.join(workspaceRoot(), "Build", "intellisense");
    fs.rmSync(cache, { recursive: true, force: true });
    await rescan();
}

async function selectConfiguration() {
    const store = readConfigurations(configFile());
    const picked = await vscode.window.showQuickPick(store.configurations.map((item) => item.name), { placeHolder: "Robix configuration" });
    if (!picked) {
        return;
    }
    store.active = picked;
    writeConfigurations(configFile(), store);
    sendConfiguration();
    refreshStatus();
    recheckOpen();
}

async function selectField(field, values) {
    const picked = await vscode.window.showQuickPick(values, { placeHolder: field });
    if (!picked) {
        return;
    }
    const file = configFile();
    const store = readConfigurations(file);
    const configuration = store.configurations.find((item) => item.name === store.active) || store.configurations[0];
    configuration[field] = picked;
    writeConfigurations(file, store);
    refreshStatus();
    recheckOpen();
}

async function editConfigurations() {
    const file = configFile();
    const store = readConfigurations(file);
    const panel = vscode.window.createWebviewPanel("robix.configuration", "Robix Configurations", vscode.ViewColumn.Active, { enableScripts: true });
    panel.webview.html = configurationPage(store);
    panel.webview.onDidReceiveMessage((message) => {
        if (!message || !message.configurations) {
            return;
        }
        writeConfigurations(file, { configurations: message.configurations, active: message.active || message.configurations[0].name });
        refreshStatus();
        panel.dispose();
    });
}

function configurationPage(store) {
    const json = JSON.stringify(store).replace(/</g, "\\u003c");
    return `<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<style>
body { font-family: var(--vscode-font-family); color: var(--vscode-foreground); background: var(--vscode-editor-background); margin: 0; padding: 16px; }
label { display: block; margin: 12px 0 4px; }
input, select { width: 100%; max-width: 32rem; box-sizing: border-box; padding: 8px; border: 1px solid var(--vscode-panel-border, #808080); background: var(--vscode-input-background); color: var(--vscode-input-foreground); }
button { margin-top: 16px; padding: 8px 12px; border: 1px solid var(--vscode-button-border, transparent); background: var(--vscode-button-background); color: var(--vscode-button-foreground); }
</style>
</head>
<body>
<form id="form">
<label>Name <input name="name" /></label>
<label>Toolchain <input name="toolchain" /></label>
<label>Target <input name="target" /></label>
<label>Mode <select name="mode"><option>Debug</option><option>Release</option></select></label>
<label>Package path <input name="packagePath" /></label>
<label>Entry <input name="entry" /></label>
<label>Defines <input name="defines" /></label>
<button type="submit">Save</button>
</form>
<script>
const vscode = acquireVsCodeApi();
const store = ${json};
const configuration = store.configurations.find((item) => item.name === store.active) || store.configurations[0];
const form = document.getElementById("form");
for (const element of form.elements) {
  if (configuration[element.name] != null) {
    element.value = Array.isArray(configuration[element.name]) ? configuration[element.name].join(",") : configuration[element.name];
  }
}
form.onsubmit = (event) => {
  event.preventDefault();
  const data = Object.fromEntries(new FormData(form).entries());
  data.packagePath = String(data.packagePath || "Packages").split(",").map((item) => item.trim());
  data.sourcePath = configuration.sourcePath || ["."];
  data.defines = String(data.defines || "").split(",").map((item) => item.trim()).filter(Boolean);
  data.forcedImports = configuration.forcedImports || [];
  data.standard = configuration.standard || "Robix";
  vscode.postMessage({ configurations: [data], active: data.name });
};
</script>
</body>
</html>`;
}

async function configureProject() {
    if (!trusted()) {
        vscode.window.showWarningMessage("Trust this workspace before Robix configures a toolchain.");
        return;
    }
    const root = workspaceRoot();
    const configuration = currentConfiguration();
    const files = await vscode.workspace.findFiles("**/*.rx", "{**/Build/**,**/node_modules/**}");
    const records = [];
    for (const file of files) {
        const text = Buffer.from(await vscode.workspace.fs.readFile(file)).toString("utf8");
        const missing = [];
        for (const match of text.matchAll(/^\s*Import\s+([A-Za-z0-9_.]+)\s*;/gm)) {
            const leaf = match[1].split(".").pop();
            const candidate = path.join(root, "Packages", match[1], `${leaf}.rx`);
            if (!fs.existsSync(candidate)) {
                missing.push(match[1]);
            }
        }
        records.push({
            file: file.fsPath,
            directory: root,
            target: configuration.target,
            mode: configuration.mode,
            packagePath: configuration.packagePath,
            command: `robixc ${JSON.stringify(file.fsPath)}`,
            missing
        });
    }
    const database = path.join(root, "Build", "compile_commands.json");
    fs.mkdirSync(path.dirname(database), { recursive: true });
    fs.writeFileSync(database, `${JSON.stringify(records, null, 4)}\n`);
    const problems = records.flatMap((record) => record.missing.map((name) => `${record.file}: missing ${name}`));
    services.output().show(true);
    services.output().appendLine(`Wrote ${database} (${records.length} files)`);
    for (const line of problems) {
        services.output().appendLine(line);
    }
    refreshStatus();
    vscode.window.showInformationMessage(problems.length ? `Configured with ${problems.length} missing imports.` : `Configured ${records.length} files.`);
}

function cleanOutput() {
    const root = path.join(workspaceRoot(), "Build", currentConfiguration().target || "host");
    fs.rmSync(root, { recursive: true, force: true });
    vscode.window.showInformationMessage(`Cleaned ${root}.`);
}

async function switchSourceTest() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        return;
    }
    const file = editor.document.uri.fsPath;
    const next = file.endsWith(".test.rx") ? file.slice(0, -".test.rx".length) + ".rx" : file.replace(/\.rx$/, ".test.rx");
    if (!fs.existsSync(next)) {
        vscode.window.showInformationMessage(`${path.basename(next)} is not in this folder.`);
        return;
    }
    const document = await vscode.workspace.openTextDocument(next);
    await vscode.window.showTextDocument(document);
}

async function runAnalysis() {
    const editor = vscode.window.activeTextEditor;
    if (!editor || editor.document.languageId !== "robix") {
        return;
    }
    const disabled = loadRuleFile(path.join(workspaceRoot(), ".robix-analysis"));
    const found = analysisRules(editor.document.getText(), disabled).map((item) => toDiagnostic(editor.document, item, "robix-analysis")).filter(Boolean);
    const previous = services.diagnostics().get(editor.document.uri) || [];
    services.diagnostics().set(editor.document.uri, [...previous].filter((item) => item.source !== "robix-analysis").concat(found));
}

async function fixAll() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        return;
    }
    const edit = organizeImports(editor.document);
    await vscode.workspace.applyEdit(edit);
}

async function runTestsInFile() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        return;
    }
    await vscode.commands.executeCommand("testing.runCurrentFile");
}

async function debugAtCursor() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        return;
    }
    await startDebug(false, editor.document.uri.fsPath);
}

async function previewActive() {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        return;
    }
    previewDocument = editor.document;
    const title = editor.document.languageId === "robix-3d" ? "Robix 3D" : "Robix UI";
    previewPanel = vscode.window.createWebviewPanel("robix.preview", title, vscode.ViewColumn.Beside, {});
    previewPanel.onDidDispose(() => {
        previewPanel = null;
        previewDocument = null;
    });
    previewPanel.webview.html = previewPage(editor.document.getText(), editor.document.languageId);
}

function refreshPreview(document) {
    if (!previewPanel || !previewDocument || document.uri.toString() !== previewDocument.uri.toString()) {
        return;
    }
    previewPanel.webview.html = previewPage(document.getText(), document.languageId);
}

async function discoverToolchain() {
    const probe = vscode.Uri.file(path.join(workspaceRoot(), "Robix.rx"));
    const interpreter = services.interpreterPath({ uri: probe });
    const compiler = services.compilerPath({ uri: probe });
    languageChannel.show(true);
    languageChannel.appendLine(`Interpreter: ${interpreter || "(not found)"}`);
    languageChannel.appendLine(`Compiler: ${compiler || "(not found)"}`);
    languageChannel.appendLine(`Target: ${currentConfiguration().target}`);
    languageChannel.appendLine(`Package path: ${(currentConfiguration().packagePath || []).join(", ")}`);
    vscode.window.showInformationMessage(compiler ? `Compiler ${compiler}` : "The Robix compiler was not found.");
}

async function loadProject(ask) {
    const root = workspaceRoot();
    const found = fs.existsSync(root)
        ? fs.readdirSync(root).filter((name) => name.endsWith(".rxp")).map((name) => path.join(root, name))
        : [];
    projectFiles = found;
    const key = `robix.project.${root}`;
    const saved = extensionContext && extensionContext.workspaceState.get(key);
    if (found.length === 0) {
        activeProject = "";
        refreshStatus();
        return;
    }
    if (found.length === 1 && !ask) {
        activeProject = found[0];
    } else if (saved && found.includes(saved) && !ask) {
        activeProject = saved;
    } else {
        const picked = await vscode.window.showQuickPick(found.map((file) => path.relative(root, file)), {
            placeHolder: "Robix project"
        });
        if (!picked && activeProject) {
            return;
        }
        activeProject = picked ? path.join(root, picked) : found[0];
    }
    if (extensionContext) {
        extensionContext.workspaceState.update(key, activeProject);
    }
    const text = fs.readFileSync(activeProject, "utf8");
    const name = /Project\s+"([^"]+)"/.exec(text);
    projectStatus.tooltip = name ? name[1] : path.basename(activeProject);
    refreshStatus();
}

function projectTree() {
    return {
        getTreeItem(element) {
            return element;
        },
        getChildren(element) {
            if (!element) {
                return ["Entry", "Sources", "Packages"].map((label) => {
                    const item = new vscode.TreeItem(label, vscode.TreeItemCollapsibleState.Collapsed);
                    item.contextValue = label;
                    return item;
                });
            }
            const root = workspaceRoot();
            if (element.label === "Entry") {
                const entry = new vscode.TreeItem(currentConfiguration().entry || "Main", vscode.TreeItemCollapsibleState.None);
                entry.description = activeProject ? path.basename(activeProject) : "";
                return [entry];
            }
            if (element.label === "Sources") {
                const files = [];
                if (fs.existsSync(root)) {
                    for (const name of fs.readdirSync(root)) {
                        if (name.endsWith(".rx") && files.length < 40) {
                            const item = new vscode.TreeItem(name, vscode.TreeItemCollapsibleState.None);
                            item.command = { command: "vscode.open", title: "Open", arguments: [vscode.Uri.file(path.join(root, name))] };
                            files.push(item);
                        }
                    }
                }
                return files;
            }
            const packages = path.join(root, "Packages");
            if (!fs.existsSync(packages)) {
                return [];
            }
            return fs.readdirSync(packages).slice(0, 80).map((name) => new vscode.TreeItem(name, vscode.TreeItemCollapsibleState.None));
        }
    };
}

function previewPage(source, language) {
    const body = language === "robix-3d" ? scenePreview(source) : uiPreview(source);
    return `<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1" />
<style>body{margin:0;padding:12px;font-family:var(--vscode-font-family);background:var(--vscode-editor-background);color:var(--vscode-foreground)} .box{border:1px solid var(--vscode-panel-border,#808080);padding:8px;margin:8px 0}</style>
</head><body>${body}</body></html>`;
}

function uiPreview(source) {
    return String(source)
        .replace(/<!--[\s\S]*?-->/g, "")
        .replace(/</g, "&lt;")
        .split("\n")
        .map((line) => `<div class="box">${line || "&nbsp;"}</div>`)
        .join("");
}

function scenePreview(source) {
    const meshes = String(source).split("\n").filter((line) => /Mesh|Node|Scene|Cube|Light/i.test(line));
    if (!meshes.length) {
        return "<p>This scene has no mesh lines yet.</p>";
    }
    return meshes.map((line) => `<div class="box">${line.replace(/</g, "&lt;")}</div>`).join("");
}

function checkMarkup(document) {
    if (!document || (document.languageId !== "robix-ui" && document.languageId !== "robix-3d")) {
        return;
    }
    const found = markupDiagnostics(document.getText()).map((item) => toDiagnostic(document, item, "robix"));
    services.diagnostics().set(document.uri, found);
}

function paintInactive(editor) {
    if (!editor || editor.document.languageId !== "robix") {
        return;
    }
    const regions = inactiveRegions(editor.document.getText(), currentConfiguration().defines || []);
    editor.setDecorations(dimDecoration, regions.map((region) => new vscode.Range(region.startLine, 0, region.endLine, 0)));
}

function extraDiagnostics(document) {
    if (document.languageId !== "robix" || document.getText().length > maxFileSize()) {
        return [];
    }
    packagePathFor(document.uri.fsPath);
    const found = missingTests(document.uri.fsPath, document.getText())
        .concat(unusedImports(document.getText()))
        .map((item) => toDiagnostic(document, item, "robix"))
        .filter(Boolean);
    return found;
}

function toDiagnostic(document, item, source) {
    const diagnostic = new vscode.Diagnostic(
        offsetRange(document, item.start || 0, item.end || (item.start || 0) + 1),
        item.message,
        item.severity === "error" ? vscode.DiagnosticSeverity.Error : item.severity === "info" ? vscode.DiagnosticSeverity.Information : vscode.DiagnosticSeverity.Warning
    );
    diagnostic.source = source;
    diagnostic.code = item.code;
    if (item.code === "RX202") {
        diagnostic.tags = [vscode.DiagnosticTag.Unnecessary];
    }
    if (item.code === "RX120") {
        const companion = document.uri.fsPath.endsWith(".test.rx")
            ? document.uri
            : vscode.Uri.file(document.uri.fsPath.replace(/\.rx$/, ".test.rx"));
        diagnostic.relatedInformation = [
            new vscode.DiagnosticRelatedInformation(
                new vscode.Location(companion, new vscode.Position(0, 0)),
                "The companion test file is where this Test belongs."
            )
        ];
    }
    const level = String(services.config().get("diagnostics.level") || "Warning");
    const rank = { Error: 0, Warning: 1, Information: 2 };
    const severity = diagnostic.severity === vscode.DiagnosticSeverity.Error ? 0 : diagnostic.severity === vscode.DiagnosticSeverity.Information ? 2 : 1;
    if (severity > (rank[level] == null ? 1 : rank[level])) {
        return null;
    }
    return diagnostic;
}

function recheckOpen() {
    for (const document of vscode.workspace.textDocuments) {
        if (services.scheduleCheck) {
            services.scheduleCheck(document, 0);
        }
        syncDocument(document);
    }
}

function offsetRange(document, start, end) {
    return new vscode.Range(document.positionAt(start), document.positionAt(Math.max(end, start)));
}

function ensureConfigurationFile() {
    const file = configFile();
    if (!fs.existsSync(file)) {
        writeConfigurations(file, { configurations: [defaultConfiguration()], active: "Host" });
    }
}

module.exports = {
    start,
    ownsDiagnostics,
    extraDiagnostics,
    syncDocument,
    registerConfigurationProvider,
    ensureConfigurationFile,
    packagePathFor,
    lastDiagnostics
};
