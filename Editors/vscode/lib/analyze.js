"use strict";

const { tokenize } = require("./scanner");
const {
    TYPE_KEYWORDS,
    BUILTIN_FUNCTIONS,
    BUILTIN_TYPES,
    NAMESPACES,
    PACKAGES,
    LIBRARY_PACKAGES,
    modifierMask
} = require("./keywords");

const BUILTIN_NAMES = new Set([
    ...BUILTIN_FUNCTIONS.map((item) => item[0]),
    ...BUILTIN_TYPES,
    ...Object.keys(NAMESPACES)
]);

function analyze(source, externalNames) {
    const text = String(source);
    const scanned = tokenize(text);
    const diagnostics = scanned.diagnostics.filter((item) => item.code !== "RX005");
    const symbols = [];
    const known = externalNames instanceof Set ? externalNames : new Set(externalNames || []);
    let parsed = null;
    try {
        parsed = parseFile(text, scanned.tokens, symbols, diagnostics, known);
    } catch (error) {
        diagnostics.push({
            severity: "warning",
            code: "RX000",
            message: "The Robix language service stopped early in this file.",
            line: 1,
            column: 1,
            start: 0,
            end: 0
        });
    }
    return {
        source: text,
        tokens: scanned.tokens,
        symbols,
        diagnostics,
        imports: parsed && parsed.importNames ? parsed.importNames : [],
        packageName: parsed && parsed.packageName ? parsed.packageName : ""
    };
}

function parseFile(source, tokens, symbols, diagnostics, known) {
    const state = {
        source,
        tokens,
        symbols,
        diagnostics,
        known,
        index: 0,
        scopes: [{ names: new Map(), kind: "file" }],
        loopDepth: 0,
        asyncDepth: 0,
        relax: 0,
        container: "",
        fuel: 0,
        localStack: [[]],
        imports: new Set(),
        importNames: [],
        packageName: ""
    };
    checkDelimiters(state);
    while (!done(state)) {
        const before = state.index;
        parseTop(state);
        if (state.index === before) {
            eat(state);
        }
    }
    colorImportedUses(state);
    reportUnknown(state);
    return state;
}

function checkDelimiters(state) {
    const stack = [];
    const openOf = { ")": "(", "]": "[", "}": "{" };
    for (const token of state.tokens) {
        if (token.kind === "comment" || token.kind === "string" || token.kind === "char" || token.kind === "eof") {
            continue;
        }
        if (token.text === "(" || token.text === "[" || token.text === "{") {
            stack.push(token);
        } else if (token.text === ")" || token.text === "]" || token.text === "}") {
            const open = stack[stack.length - 1];
            if (!open || open.text !== openOf[token.text]) {
                diagnostic(state, "error", "RX006", `Unexpected '${token.text}'.`, token);
            } else {
                stack.pop();
            }
        }
    }
    for (const open of stack) {
        diagnostic(state, "error", "RX007", `Unclosed '${open.text}'.`, open);
    }
}

function reportUnknown(state) {
    for (const token of state.tokens) {
        if (token.relax || token.kind !== "identifier") {
            continue;
        }
        if (token.role === "member" || token.role === "decl" || token.role === "property" || token.role === "label") {
            continue;
        }
        if (token.role === "type") {
            if (!isKnownType(state, token.text, token.start)) {
                diagnostic(state, "warning", "RX201", `Unknown type '${token.text}'.`, token);
            }
            continue;
        }
        if (token.role === "call" || token.role === "use" || token.role === "") {
            const previous = previousSignificant(state.tokens, token);
            const next = nextSignificantToken(state.tokens, token);
            if (previous && (previous.text === "." || previous.text === "?.")) {
                continue;
            }
            if (next && next.text === "." && /^[A-Z_]/.test(token.text)) {
                continue;
            }
            if (!isKnown(state, token.text, token.start)) {
                const kind = token.role === "call" ? "function or type" : "name";
                diagnostic(state, "warning", "RX200", `Unknown ${kind} '${token.text}'.`, token);
            }
        }
    }
}

function isKnown(state, name, offset) {
    if (!name || name === "_") {
        return true;
    }
    if (BUILTIN_NAMES.has(name) || TYPE_KEYWORDS.has(name)) {
        return true;
    }
    if (state.known.has(name)) {
        return true;
    }
    const owner = enclosingType(state, offset);
    if (owner && state.symbols.some((symbol) => symbol.member && symbol.name === name)) {
        return true;
    }
    return state.symbols.some((symbol) => visible(symbol, name, offset) && !symbol.member);
}

function enclosingType(state, offset) {
    let found = null;
    for (const symbol of state.symbols) {
        if (symbol.member) {
            continue;
        }
        if (!["class", "struct", "interface", "trait", "enum", "service"].includes(symbol.kind)) {
            continue;
        }
        const end = symbol.end || symbol.visibleTo;
        if (symbol.start <= offset && offset <= end && (!found || symbol.start >= found.start)) {
            found = symbol;
        }
    }
    return found;
}

function previousSignificant(tokens, token) {
    const index = tokens.indexOf(token);
    for (let cursor = index - 1; cursor >= 0; cursor -= 1) {
        if (tokens[cursor].kind !== "comment") {
            return tokens[cursor];
        }
    }
    return null;
}

function nextSignificantToken(tokens, token) {
    const index = tokens.indexOf(token);
    for (let cursor = index + 1; cursor < tokens.length; cursor += 1) {
        if (tokens[cursor].kind !== "comment") {
            return tokens[cursor];
        }
    }
    return null;
}

function isKnownType(state, name, offset) {
    if (TYPE_KEYWORDS.has(name) || BUILTIN_TYPES.includes(name) || BUILTIN_NAMES.has(name) || state.known.has(name)) {
        return true;
    }
    return state.symbols.some((symbol) => {
        if (symbol.name !== name) {
            return false;
        }
        if (symbol.kind === "typeParameter") {
            return offset >= symbol.visibleFrom && offset <= symbol.visibleTo;
        }
        return symbol.kind === "class" || symbol.kind === "struct" || symbol.kind === "interface" || symbol.kind === "trait" || symbol.kind === "enum" || symbol.kind === "type" || symbol.kind === "service";
    });
}

function visible(symbol, name, offset) {
    if (symbol.name !== name) {
        return false;
    }
    if (symbol.fileWide) {
        return true;
    }
    return offset >= symbol.visibleFrom && offset <= symbol.visibleTo;
}

function diagnostic(state, severity, code, message, token) {
    state.diagnostics.push({
        severity,
        code,
        message,
        line: token ? token.line : 1,
        column: token ? token.column : 1,
        start: token ? token.start : 0,
        end: token ? token.end : 0
    });
}

function done(state) {
    const token = peek(state);
    return !token || token.kind === "eof";
}

function peek(state) {
    while (state.tokens[state.index] && state.tokens[state.index].kind === "comment") {
        state.index += 1;
    }
    return state.tokens[state.index];
}

function eat(state) {
    state.fuel += 1;
    if (state.fuel > state.tokens.length * 12) {
        throw new Error("fuel");
    }
    const token = peek(state);
    if (token && token.kind !== "eof") {
        state.index += 1;
    }
    return token;
}

function textOf(token) {
    if (!token) {
        return "";
    }
    return token.canonical || token.text || "";
}

function paint(token, type, mods, role) {
    if (!token || (paint.silent)) {
        return;
    }
    token.semType = type;
    token.semMods = modifierMask(mods || []);
    if (role) {
        token.role = role;
    }
}

function trackLocal(state, symbol) {
    if (!symbol.fileWide && !symbol.member) {
        symbol.visibleTo = state.source.length;
        state.localStack[state.localStack.length - 1].push(symbol);
    }
    return symbol;
}

function closeLocals(state, end) {
    const locals = state.localStack.pop() || [];
    for (const local of locals) {
        local.visibleTo = end;
    }
}

function addSymbol(state, symbol) {
    const scope = state.scopes[state.scopes.length - 1];
    if (symbol.name && !symbol.member && scope.names.has(symbol.name)) {
        diagnostic(state, "error", "RX100", `'${symbol.name}' is already declared in this scope.`, symbol.token);
    }
    if (symbol.name && !symbol.member) {
        scope.names.set(symbol.name, symbol);
    }
    state.symbols.push(symbol);
    return symbol;
}

function canStartType(token) {
    if (!token) {
        return false;
    }
    if (token.kind === "keyword" && (TYPE_KEYWORDS.has(token.canonical) || token.canonical === "Iterator" || token.canonical === "Params")) {
        return true;
    }
    return token.kind === "identifier" && token.text !== "_" && /^[A-Z_]/.test(token.text);
}

function looksLikeTypedDeclaration(state) {
    if (!canStartType(peek(state))) {
        return false;
    }
    const saved = state.index;
    paint.silent = true;
    try {
        parseType(state);
    } finally {
        paint.silent = false;
    }
    const name = peek(state);
    const matched = Boolean(name && name.kind === "identifier");
    state.index = saved;
    return matched;
}

function parseType(state) {
    const token = peek(state);
    if (!token) {
        return "";
    }
    if (token.text === "(") {
        let depth = 0;
        while (peek(state) && peek(state).kind !== "eof") {
            const item = peek(state);
            if (item.text === "(") {
                depth += 1;
            }
            if (item.text === ")") {
                depth -= 1;
                eat(state);
                if (depth === 0) {
                    break;
                }
                continue;
            }
            if (canStartType(item)) {
                paint(item, "type", TYPE_KEYWORDS.has(textOf(item)) ? ["defaultLibrary"] : [], "type");
            } else if (item.kind === "identifier") {
                paint(item, "property", ["declaration"], "property");
            }
            eat(state);
        }
        return "tuple";
    }
    const name = textOf(token);
    paint(token, "type", TYPE_KEYWORDS.has(name) ? ["defaultLibrary"] : [], "type");
    eat(state);
    if (peek(state) && peek(state).text === "<") {
        let depth = 0;
        while (peek(state) && peek(state).kind !== "eof") {
            const item = peek(state);
            if (item.text === "<") {
                depth += 1;
                eat(state);
                continue;
            }
            if (item.text === ">") {
                depth -= 1;
                eat(state);
                if (depth === 0) {
                    break;
                }
                continue;
            }
            if (item.text === ">>") {
                depth -= 2;
                eat(state);
                if (depth <= 0) {
                    break;
                }
                continue;
            }
            if (canStartType(item) || (item.kind === "identifier" && /^[A-Z_]/.test(item.text))) {
                paint(item, item.text.length === 1 ? "typeParameter" : "type", [], "type");
            }
            eat(state);
            if (depth === 0) {
                break;
            }
        }
    }
    while (peek(state) && peek(state).text === "[") {
        eat(state);
        if (peek(state) && peek(state).text === "]") {
            eat(state);
        }
    }
    if (peek(state) && peek(state).text === "?") {
        eat(state);
    }
    return name;
}

function parseTop(state) {
    parseAttributes(state);
    const mods = parseModifiers(state);
    const token = peek(state);
    const name = textOf(token);
    if (name === "Package") {
        parsePackage(state);
    } else if (name === "Import") {
        parseImport(state);
    } else if (name === "Function") {
        parseFunction(state, mods, false);
    } else if (name === "Test") {
        parseTest(state);
    } else if (name === "Class" || name === "Struct" || name === "Interface" || name === "Trait" || name === "Enum" || name === "Service") {
        parseTypeDecl(state, mods, name);
    } else if (name === "Implement") {
        parseImplement(state);
    } else if (name === "Connect") {
        eat(state);
        readDotted(state, "namespace");
        if (peek(state) && peek(state).text === ";") {
            eat(state);
        }
    } else if (name === "Let" || name === "Var" || name === "Const") {
        parseBinding(state, name);
    } else if (name === "Type") {
        parseTypeAlias(state);
    } else if (name === "Macro") {
        parseFunction(state, mods, false);
    } else if (name === "Operator") {
        parseOperator(state, mods);
    } else if (name === "Permissions") {
        eat(state);
        if (peek(state) && peek(state).text === "{") {
            parseRelaxedBlock(state);
        }
    } else if (looksLikeTypedDeclaration(state)) {
        parseTypedDeclaration(state, "");
    } else {
        parseStatement(state);
    }
}

function parseAttributes(state) {
    while (peek(state) && peek(state).text === "@") {
        eat(state);
        const name = peek(state);
        if (name && (name.kind === "identifier" || name.kind === "keyword")) {
            paint(name, "decorator", [], "decl");
            eat(state);
        }
        if (peek(state) && peek(state).text === "(") {
            skipBalanced(state, "(", ")");
        }
    }
}

function parseModifiers(state) {
    const mods = [];
    while (peek(state) && peek(state).kind === "keyword") {
        const name = peek(state).canonical;
        if (!["Public", "Private", "Protected", "Internal", "Abstract", "Override", "Virtual", "Sealed", "Final", "Static", "Partial", "Unsafe", "Extern", "Consteval", "Iterator", "Async"].includes(name)) {
            break;
        }
        mods.push(name);
        eat(state);
    }
    return mods;
}

function parsePackage(state) {
    const keyword = eat(state);
    const name = readDotted(state, "namespace", true);
    const symbol = addSymbol(state, symbolAt(keyword, name || "Package", "namespace", name, true, true));
    paint(keyword, "keyword", [], "");
    if (name) {
        state.packageName = name;
        symbol.detail = `Package ${name}`;
    }
    if (peek(state) && textOf(peek(state)) === "Explain") {
        symbol.explain = parseExplain(state);
    }
    const description = descriptionOf(symbol.explain);
    if (description) {
        symbol.summary = description;
    } else {
        diagnostic(state, "error", "RX173", `Package '${name || ""}' requires a Description.`, keyword);
    }
    if (peek(state) && peek(state).text === "{") {
        parseRelaxedBlock(state);
    } else if (peek(state) && peek(state).text === ";") {
        eat(state);
    }
}

function parseImport(state) {
    const keyword = eat(state);
    const dotted = readDotted(state, "namespace", false);
    noteImport(state, dotted);
    let alias = dotted.split(".").pop();
    if (peek(state) && peek(state).text === ".") {
        eat(state);
        if (peek(state) && peek(state).text === "*") {
            eat(state);
        }
    }
    if (peek(state) && textOf(peek(state)) === "As") {
        eat(state);
        const aliasToken = peek(state);
        if (aliasToken && (aliasToken.kind === "identifier" || (aliasToken.kind === "keyword" && /^[A-Za-z]/.test(aliasToken.text)))) {
            alias = aliasToken.text;
            const known = isKnownPackage(state, dotted);
            paint(aliasToken, known ? "namespace" : "variable", known ? ["declaration", "defaultLibrary"] : [], known ? "decl" : "use");
            if (known) {
                rememberImport(state, alias);
            }
            eat(state);
        }
    }
    if (peek(state) && peek(state).text === ";") {
        eat(state);
    }
    const symbol = addSymbol(state, symbolAt(keyword, alias, "namespace", dotted, true, true));
    symbol.detail = alias === dotted ? `Import ${dotted}` : `Import ${dotted} As ${alias}`;
    symbol.summary = libraryDescription(dotted);
    symbol.token = keyword;
}

function readDotted(state, role, declared) {
    const parts = [];
    while (peek(state) && (peek(state).kind === "identifier" || (peek(state).kind === "keyword" && /^[A-Z]/.test(peek(state).canonical)))) {
        parts.push(peek(state));
        eat(state);
        if (peek(state) && peek(state).text === ".") {
            const after = state.tokens[state.index + 1];
            if (after && (after.kind === "identifier" || after.kind === "keyword")) {
                eat(state);
                continue;
            }
        }
        break;
    }
    const name = parts.map((token) => token.text).join(".");
    const known = Boolean(declared) || isKnownPackage(state, name);
    for (const token of parts) {
        if (known) {
            paint(token, "namespace", ["defaultLibrary"], role === "namespace" ? "decl" : "use");
        } else {
            paint(token, "variable", [], "use");
        }
    }
    if (known && name) {
        rememberImport(state, name);
    }
    return name;
}

function descriptionOf(explain) {
    const raw = String(explain && explain.description || "").trim();
    if (raw.startsWith("\"") && raw.endsWith("\"") && raw.length >= 2) {
        return raw.slice(1, -1).trim();
    }
    return "";
}

function libraryDescription(name) {
    const lower = String(name || "").toLowerCase();
    const key = Object.keys(LIBRARY_PACKAGES).find((item) => item.toLowerCase() === lower);
    return key ? LIBRARY_PACKAGES[key] : "";
}

function isKnownPackage(state, name) {
    const lower = String(name || "").toLowerCase();
    if (!lower) {
        return false;
    }
    if (PACKAGES.some((item) => {
        const packageName = item.toLowerCase();
        return packageName === lower || packageName.startsWith(`${lower}.`);
    })) {
        return true;
    }
    if (Object.keys(NAMESPACES).some((item) => item.toLowerCase() === lower)) {
        return true;
    }
    if (state.known) {
        for (const known of state.known) {
            if (String(known).toLowerCase() === lower) {
                return true;
            }
        }
    }
    return false;
}

function noteImport(state, name) {
    if (!name) {
        return;
    }
    if (!state.importNames.some((item) => item.toLowerCase() === name.toLowerCase())) {
        state.importNames.push(name);
    }
}

function rememberImport(state, name) {
    noteImport(state, name);
    if (!name) {
        return;
    }
    for (const part of String(name).split(".")) {
        if (part) {
            state.imports.add(part);
            state.imports.add(part.toLowerCase());
        }
    }
}

function colorImportedUses(state) {
    for (const token of state.tokens) {
        if (token.kind !== "identifier" || token.role === "decl") {
            continue;
        }
        if (token.semType === "function" || token.semType === "method" || token.semType === "macro" || token.semType === "class") {
            continue;
        }
        if (state.imports.has(token.text) || state.imports.has(token.text.toLowerCase())) {
            paint(token, "namespace", ["defaultLibrary"], "use");
        }
    }
}

function parseFunction(state, mods, isMethod) {
    const keyword = eat(state);
    let nameToken = null;
    if (peek(state) && (peek(state).kind === "identifier" || (peek(state).kind === "keyword" && /^[A-Za-z]/.test(peek(state).text)))) {
        nameToken = eat(state);
    }
    const typeParams = parseTypeParameters(state, null);
    const params = parseParameterList(state);
    let result = "Void";
    if (peek(state) && peek(state).text === "->") {
        eat(state);
        result = parseType(state) || "Void";
    }
    if (peek(state) && textOf(peek(state)) === "CallingConvention") {
        eat(state);
        if (peek(state) && (peek(state).kind === "identifier" || peek(state).kind === "keyword")) {
            eat(state);
        }
    }
    if (peek(state) && textOf(peek(state)) === "Where") {
        while (peek(state) && peek(state).text !== "{" && peek(state).text !== ";" && peek(state).kind !== "eof") {
            if (canStartType(peek(state))) {
                parseType(state);
            } else {
                eat(state);
            }
        }
    }
    const name = nameToken ? nameToken.text : keyword.canonical;
    const async = mods.includes("Async");
    const symbol = addSymbol(state, {
        ...symbolAt(nameToken || keyword, name, isMethod ? "method" : keyword.canonical === "Macro" ? "macro" : "function", result, !isMethod, mods.includes("Public")),
        params,
        async,
        container: state.container,
        member: isMethod,
        fileWide: !isMethod,
        packageName: state.packageName || "",
        detail: `${mods.join(" ")}${mods.length ? " " : ""}${keyword.canonical} ${name}(${params.map((param) => param.label).join(", ")}) -> ${result}`.trim()
    });
    if (nameToken) {
        paint(nameToken, isMethod ? "method" : symbol.kind === "macro" ? "macro" : "function", ["declaration"].concat(async ? ["async"] : []), "decl");
    }
    for (const param of params) {
        param.visibleTo = sourceEnd(state);
        param.container = name;
    }
    state.scopes.push({ names: new Map(), kind: "function" });
    state.localStack.push([]);
    for (const param of params) {
        addSymbol(state, param);
        trackLocal(state, param);
        paint(param.token, "parameter", ["declaration"], "decl");
    }
    for (const typeParam of typeParams) {
        typeParam.visibleTo = sourceEnd(state);
        addSymbol(state, typeParam);
    }
    if (async) {
        state.asyncDepth += 1;
    }
    if (peek(state) && textOf(peek(state)) === "Explain") {
        symbol.explain = parseExplain(state);
    }
    if (peek(state) && peek(state).text === "{") {
        parseBlock(state, false);
    } else if (peek(state) && peek(state).text === ";") {
        eat(state);
    }
    const end = peek(state) ? peek(state).start : state.source.length;
    symbol.end = end;
    symbol.visibleTo = state.source.length;
    for (const param of params) {
        param.visibleTo = end;
    }
    for (const typeParam of typeParams) {
        typeParam.visibleTo = end;
    }
    if (async) {
        state.asyncDepth -= 1;
    }
    closeLocals(state, end);
    state.scopes.pop();
}

function parseTypeParameters(state, ownerEnd) {
    const params = [];
    if (!peek(state) || peek(state).text !== "<") {
        return params;
    }
    // Only a generic parameter list when the following token is a type name, not an expression.
    const next = state.tokens[state.index + 1];
    if (!next || (next.kind !== "identifier" && next.kind !== "keyword")) {
        return params;
    }
    eat(state);
    while (peek(state) && peek(state).text !== ">" && peek(state).text !== ">>" && peek(state).kind !== "eof") {
        const token = peek(state);
        if (token.kind === "identifier" || token.kind === "keyword") {
            paint(token, "typeParameter", ["declaration"], "decl");
            params.push({
                ...symbolAt(token, token.text, "typeParameter", "", false, false),
                visibleFrom: token.end,
                visibleTo: ownerEnd || sourceEnd(state)
            });
            eat(state);
        } else if (token.text === ",") {
            eat(state);
        } else {
            break;
        }
    }
    if (peek(state) && (peek(state).text === ">" || peek(state).text === ">>")) {
        eat(state);
    }
    return params;
}

function parseParameterList(state) {
    const params = [];
    if (!peek(state) || peek(state).text !== "(") {
        return params;
    }
    eat(state);
    while (peek(state) && peek(state).text !== ")" && peek(state).kind !== "eof") {
        if (peek(state).text === ",") {
            eat(state);
            continue;
        }
        const param = parseOneParameter(state);
        if (param) {
            params.push(param);
        } else if (peek(state) && peek(state).text !== ")" && peek(state).text !== ",") {
            eat(state);
        }
        if (peek(state) && peek(state).text === ",") {
            eat(state);
        }
    }
    if (peek(state) && peek(state).text === ")") {
        eat(state);
    }
    return params;
}

function parseOneParameter(state) {
    if (peek(state) && (textOf(peek(state)) === "Mut" || textOf(peek(state)) === "Ref")) {
        eat(state);
    }
    let typeName = "";
    if (canStartType(peek(state))) {
        const saved = state.index;
        typeName = parseType(state);
        if (!peek(state) || peek(state).kind !== "identifier") {
            state.index = saved;
            typeName = "";
        }
    }
    const name = peek(state);
    if (!name || name.kind !== "identifier") {
        return null;
    }
    paint(name, "parameter", ["declaration"], "decl");
    eat(state);
    if (peek(state) && peek(state).text === "=") {
        eat(state);
        scanExpression(state, () => {
            const token = peek(state);
            return token && (token.text === "," || token.text === ")");
        });
    }
    return {
        ...symbolAt(name, name.text, "parameter", typeName, false, false),
        typeName,
        label: typeName ? `${typeName} ${name.text}` : name.text,
        visibleFrom: name.end,
        member: false
    };
}

function parseExplain(state) {
    const keyword = peek(state);
    const start = keyword ? keyword.start : state.source.length;
    eat(state);
    const fields = {};
    if (!peek(state) || peek(state).text !== "{") {
        return { text: state.source.slice(start, keyword ? keyword.end : start), description: "", input: "", processing: "", output: "" };
    }
    eat(state);
    while (peek(state) && peek(state).text !== "}" && peek(state).kind !== "eof") {
        const field = peek(state);
        let key = "";
        if (field.kind === "identifier" || field.kind === "keyword") {
            key = field.text;
            paint(field, "property", ["declaration"], "property");
            eat(state);
        }
        if (peek(state) && peek(state).text === ":") {
            eat(state);
            const valueStart = peek(state) ? peek(state).start : state.source.length;
            scanExpression(state, () => peek(state) && (peek(state).text === ";" || peek(state).text === "}"));
            const valueEnd = peek(state) ? peek(state).start : state.source.length;
            if (key) {
                fields[key] = state.source.slice(valueStart, valueEnd).trim();
            }
        }
        if (peek(state) && peek(state).text === ";") {
            eat(state);
        } else if (peek(state) && peek(state).text !== "}") {
            eat(state);
        }
    }
    let end = state.source.length;
    if (peek(state) && peek(state).text === "}") {
        end = peek(state).end;
        eat(state);
    }
    return {
        text: state.source.slice(start, end),
        description: fields.Description || "",
        input: fields.Input || "",
        processing: fields.Processing || "",
        output: fields.Output || ""
    };
}

function parseTypeDecl(state, mods, keyword) {
    const keywordToken = eat(state);
    const typeParams = [];
    const nameToken = peek(state);
    let name = keyword;
    if (nameToken && (nameToken.kind === "identifier" || (nameToken.kind === "keyword" && /^[A-Za-z]/.test(nameToken.text)))) {
        name = nameToken.text;
        eat(state);
        const sem = keyword === "Enum" ? "enum" : keyword === "Interface" ? "interface" : keyword === "Struct" ? "struct" : "class";
        paint(nameToken, sem, ["declaration"].concat(mods.includes("Abstract") ? ["abstract"] : []), "decl");
    }
    if (peek(state) && peek(state).text === "<") {
        typeParams.push(...parseTypeParameters(state));
    }
    while (peek(state) && peek(state).text === ":") {
        eat(state);
        while (peek(state) && peek(state).text !== "{" && peek(state).text !== ";" && peek(state).kind !== "eof") {
            if (peek(state).text === ",") {
                eat(state);
                continue;
            }
            if (canStartType(peek(state))) {
                parseType(state);
            } else {
                break;
            }
        }
    }
    const kind = keyword === "Enum" ? "enum" : keyword === "Interface" ? "interface" : keyword === "Trait" ? "trait" : keyword === "Service" ? "service" : keyword === "Struct" ? "struct" : "class";
    const symbol = addSymbol(state, {
        ...symbolAt(nameToken || keywordToken, name, kind, "", true, mods.includes("Public") || true),
        detail: `${mods.join(" ")}${mods.length ? " " : ""}${keyword} ${name}`.trim(),
        packageName: state.packageName || ""
    });
    const previous = state.container;
    state.container = name;
    state.scopes.push({ names: new Map(), kind });
    for (const typeParam of typeParams) {
        typeParam.visibleTo = sourceEnd(state);
        addSymbol(state, typeParam);
    }
    if (peek(state) && peek(state).text === "{") {
        if (keyword === "Enum") {
            parseEnumBody(state, name);
        } else {
            parseMembers(state, keyword === "Interface" || keyword === "Trait" || keyword === "Service");
        }
    }
    symbol.end = peek(state) ? peek(state).start : state.source.length;
    for (const typeParam of typeParams) {
        typeParam.visibleTo = symbol.end;
    }
    state.scopes.pop();
    state.container = previous;
}

function parseEnumBody(state, enumName) {
    eat(state);
    while (peek(state) && peek(state).text !== "}" && peek(state).kind !== "eof") {
        const member = peek(state);
        if (member.kind === "identifier" || member.kind === "keyword") {
            paint(member, "enumMember", ["declaration"], "decl");
            addSymbol(state, {
                ...symbolAt(member, member.text, "enumMember", enumName, false, true),
                member: true,
                container: enumName,
                detail: `${enumName}.${member.text}`
            });
            eat(state);
            if (peek(state) && peek(state).text === "(") {
                parseParameterList(state);
            } else if (peek(state) && peek(state).text === "=") {
                eat(state);
                scanExpression(state, () => peek(state) && (peek(state).text === "," || peek(state).text === "}"));
            }
        }
        if (peek(state) && peek(state).text === ",") {
            eat(state);
        } else if (peek(state) && peek(state).text !== "}") {
            eat(state);
        }
    }
    if (peek(state) && peek(state).text === "}") {
        eat(state);
    }
}

function parseMembers(state, abstractBody) {
    eat(state);
    while (peek(state) && peek(state).text !== "}" && peek(state).kind !== "eof") {
        const before = state.index;
        parseAttributes(state);
        const mods = parseModifiers(state);
        const name = textOf(peek(state));
        if (name === "Function" || name === "Macro") {
            parseFunction(state, mods, true);
        } else if (name === "Constructor" || name === "Destructor") {
            parseConstructor(state, mods);
        } else if (name === "Operator") {
            parseOperator(state, mods);
        } else if (name === "Let" || name === "Var" || name === "Const") {
            parseBinding(state, name);
        } else if (name === "Class" || name === "Struct" || name === "Enum" || name === "Interface" || name === "Trait") {
            parseTypeDecl(state, mods, name);
        } else if (looksLikeTypedDeclaration(state)) {
            parseTypedDeclaration(state, state.container);
        } else if (peek(state) && peek(state).text === ";") {
            eat(state);
        } else {
            parseStatement(state);
        }
        if (state.index === before) {
            eat(state);
        }
        if (abstractBody && peek(state) && peek(state).text === ";") {
            eat(state);
        }
    }
    if (peek(state) && peek(state).text === "}") {
        eat(state);
    }
}

function parseConstructor(state, mods) {
    const keyword = eat(state);
    const params = parseParameterList(state);
    while (peek(state) && peek(state).text === ":") {
        eat(state);
        scanExpression(state, () => peek(state) && (peek(state).text === "{" || peek(state).text === ":" || peek(state).text === ";"));
    }
    addSymbol(state, {
        ...symbolAt(keyword, keyword.canonical, "method", "", false, mods.includes("Public")),
        params,
        member: true,
        container: state.container,
        detail: `${keyword.canonical}(${params.map((param) => param.label).join(", ")})`
    });
    state.scopes.push({ names: new Map(), kind: "function" });
    state.localStack.push([]);
    for (const param of params) {
        addSymbol(state, param);
        trackLocal(state, param);
    }
    if (peek(state) && peek(state).text === "{") {
        parseBlock(state, false);
    } else if (peek(state) && peek(state).text === ";") {
        eat(state);
    }
    closeLocals(state, peek(state) ? peek(state).start : state.source.length);
    state.scopes.pop();
}

function parseOperator(state, mods) {
    const keyword = eat(state);
    if (peek(state) && peek(state).kind === "string") {
        eat(state);
        while (peek(state) && textOf(peek(state)) !== "Function" && peek(state).text !== "{" && peek(state).text !== ";" && peek(state).kind !== "eof") {
            eat(state);
        }
        if (peek(state) && textOf(peek(state)) === "Function") {
            parseFunction(state, mods, false);
        }
        return;
    }
    if (peek(state) && peek(state).kind === "operator") {
        eat(state);
    }
    const params = parseParameterList(state);
    if (peek(state) && peek(state).text === "->") {
        eat(state);
        parseType(state);
    }
    addSymbol(state, {
        ...symbolAt(keyword, "Operator", "method", "", false, mods.includes("Public")),
        params,
        member: Boolean(state.container),
        container: state.container,
        detail: `Operator(${params.map((param) => param.label).join(", ")})`
    });
    state.scopes.push({ names: new Map(), kind: "function" });
    state.localStack.push([]);
    for (const param of params) {
        addSymbol(state, param);
        trackLocal(state, param);
    }
    if (peek(state) && textOf(peek(state)) === "Explain") {
        parseExplain(state);
    }
    if (peek(state) && peek(state).text === "{") {
        parseBlock(state, false);
    } else if (peek(state) && peek(state).text === ";") {
        eat(state);
    }
    closeLocals(state, peek(state) ? peek(state).start : state.source.length);
    state.scopes.pop();
}

function parseImplement(state) {
    eat(state);
    const name = readDotted(state, "type");
    const previous = state.container;
    state.container = name || state.container;
    if (peek(state) && peek(state).text === "{") {
        parseMembers(state, false);
    }
    state.container = previous;
}

function parseTest(state) {
    const keyword = eat(state);
    const nameToken = peek(state);
    const name = nameToken && nameToken.kind === "identifier" ? nameToken.text : "Test";
    if (nameToken && nameToken.kind === "identifier") {
        paint(nameToken, "function", ["declaration"], "decl");
        eat(state);
    }
    addSymbol(state, {
        ...symbolAt(nameToken || keyword, name, "function", "", true, false),
        detail: `Test ${name}`
    });
    if (peek(state) && peek(state).text === "{") {
        state.relax += 1;
        parseBlock(state, false);
        state.relax -= 1;
    }
}

function parseBinding(state, keyword) {
    eat(state);
    const declared = [];
    if (peek(state) && peek(state).text === "(") {
        eat(state);
        while (peek(state) && peek(state).text !== ")" && peek(state).kind !== "eof") {
            if (peek(state).kind === "identifier") {
                declared.push(eat(state));
            } else {
                eat(state);
            }
        }
        if (peek(state) && peek(state).text === ")") {
            eat(state);
        }
    } else if (peek(state) && peek(state).kind === "identifier") {
        declared.push(eat(state));
    }
    let typeName = "";
    if (peek(state) && peek(state).text === ":") {
        eat(state);
        typeName = parseType(state);
    }
    if (peek(state) && peek(state).text === "=") {
        eat(state);
        typeName = typeName || inferExpression(state);
        scanExpression(state, () => peek(state) && (peek(state).text === ";" || peek(state).text === "}"));
    }
    if (peek(state) && peek(state).text === ";") {
        eat(state);
    }
    const from = peek(state) ? peek(state).start : state.source.length;
    for (const token of declared) {
        paint(token, keyword === "Const" ? "variable" : "variable", keyword === "Const" || keyword === "Let" ? ["declaration", "readonly"] : ["declaration"], "decl");
        trackLocal(state, addSymbol(state, {
            ...symbolAt(token, token.text, "variable", typeName, false, false),
            typeName,
            visibleFrom: from,
            detail: `${keyword} ${token.text}${typeName ? `: ${typeName}` : ""}`
        }));
    }
}

function parseTypedDeclaration(state, container) {
    const typeName = parseType(state);
    const declared = [];
    while (peek(state) && peek(state).kind === "identifier") {
        const token = eat(state);
        declared.push(token);
        if (peek(state) && peek(state).text === "=") {
            eat(state);
            scanExpression(state, () => peek(state) && (peek(state).text === "," || peek(state).text === ";" || peek(state).text === "}"));
        }
        if (peek(state) && peek(state).text === ",") {
            eat(state);
            continue;
        }
        break;
    }
    if (peek(state) && peek(state).text === ";") {
        eat(state);
    }
    const from = peek(state) ? peek(state).start : state.source.length;
    for (const token of declared) {
        const member = Boolean(container);
        paint(token, member ? "property" : "variable", ["declaration"], "decl");
        const symbol = addSymbol(state, {
            ...symbolAt(token, token.text, member ? "property" : "variable", typeName, false, false),
            typeName,
            member,
            container,
            visibleFrom: from,
            detail: `${typeName} ${token.text}`
        });
        if (!member) {
            trackLocal(state, symbol);
        }
    }
}

function parseTypeAlias(state) {
    eat(state);
    const name = peek(state);
    if (!name || name.kind !== "identifier") {
        return;
    }
    paint(name, "type", ["declaration"], "decl");
    eat(state);
    let target = "";
    if (peek(state) && peek(state).text === "=") {
        eat(state);
        target = parseType(state);
    }
    if (peek(state) && peek(state).text === ";") {
        eat(state);
    }
    addSymbol(state, {
        ...symbolAt(name, name.text, "type", target, true, true),
        packageName: state.packageName || "",
        detail: `Type ${name.text}${target ? ` = ${target}` : ""}`
    });
}

function parseBlock(state, isLoop) {
    if (!peek(state) || peek(state).text !== "{") {
        return;
    }
    eat(state);
    if (isLoop) {
        state.loopDepth += 1;
    }
    state.scopes.push({ names: new Map(), kind: "block" });
    state.localStack.push([]);
    while (peek(state) && peek(state).text !== "}" && peek(state).kind !== "eof") {
        const before = state.index;
        parseStatement(state);
        if (state.index === before) {
            eat(state);
        }
    }
    if (peek(state) && peek(state).text === "}") {
        eat(state);
    }
    closeLocals(state, peek(state) ? peek(state).start : state.source.length);
    state.scopes.pop();
    if (isLoop) {
        state.loopDepth -= 1;
    }
}

function parseRelaxedBlock(state) {
    state.relax += 1;
    parseBlock(state, false);
    state.relax -= 1;
}

function parseStatement(state) {
    parseAttributes(state);
    const mods = parseModifiers(state);
    const token = peek(state);
    if (!token || token.kind === "eof" || token.text === "}") {
        return;
    }
    const name = textOf(token);
    if (name === "Function" || name === "Macro") {
        parseFunction(state, mods, Boolean(state.container) && state.scopes.length > 1);
        return;
    }
    if (name === "If") {
        parseIf(state);
        return;
    }
    if (name === "For") {
        parseFor(state);
        return;
    }
    if (name === "While") {
        eat(state);
        state.loopDepth += 1;
        scanHeader(state);
        parseTrailingBlock(state, true);
        state.loopDepth -= 1;
        return;
    }
    if (name === "Do") {
        eat(state);
        state.loopDepth += 1;
        parseTrailingBlock(state, true);
        state.loopDepth -= 1;
        if (peek(state) && textOf(peek(state)) === "While") {
            eat(state);
            scanHeader(state);
            if (peek(state) && peek(state).text === ";") {
                eat(state);
            }
        }
        return;
    }
    if (name === "Repeat" || name === "Loop") {
        eat(state);
        state.loopDepth += 1;
        if (name === "Repeat") {
            scanExpression(state, () => peek(state) && peek(state).text === "{");
        }
        parseTrailingBlock(state, true);
        state.loopDepth -= 1;
        return;
    }
    if (name === "Match") {
        eat(state);
        scanExpression(state, () => peek(state) && peek(state).text === "{");
        parseMatch(state);
        return;
    }
    if (name === "Try") {
        eat(state);
        parseTrailingBlock(state, false);
        while (peek(state) && textOf(peek(state)) === "Catch") {
            eat(state);
            if (peek(state) && peek(state).text === "(") {
                const params = parseParameterList(state);
                state.scopes.push({ names: new Map(), kind: "block" });
                state.localStack.push([]);
                for (const param of params) {
                    addSymbol(state, param);
                    trackLocal(state, param);
                }
                parseTrailingBlock(state, false);
                closeLocals(state, peek(state) ? peek(state).start : state.source.length);
                state.scopes.pop();
            } else {
                parseTrailingBlock(state, false);
            }
        }
        if (peek(state) && textOf(peek(state)) === "Finally") {
            eat(state);
            parseTrailingBlock(state, false);
        }
        return;
    }
    if (name === "Break" || name === "Continue") {
        if (state.loopDepth === 0) {
            diagnostic(state, "error", "RX110", `'${name}' is outside a loop.`, token);
        }
        eat(state);
        finishSimple(state);
        return;
    }
    if (name === "Await") {
        if (state.asyncDepth === 0) {
            diagnostic(state, "warning", "RX111", "'Await' is outside an Async function.", token);
        }
        eat(state);
        scanExpression(state, () => peek(state) && (peek(state).text === ";" || peek(state).text === "}"));
        if (peek(state) && peek(state).text === ";") {
            eat(state);
        }
        return;
    }
    if (name === "Return" || name === "Throw" || name === "Yield" || name === "Goto") {
        eat(state);
        finishSimple(state);
        return;
    }
    if (name === "Defer" || name === "Unsafe") {
        eat(state);
        parseTrailingBlock(state, false);
        return;
    }
    if (name === "Using" || name === "Lock") {
        eat(state);
        scanHeader(state);
        parseTrailingBlock(state, false);
        return;
    }
    if (name === "ASM") {
        eat(state);
        if (peek(state) && peek(state).text === "(") {
            skipBalanced(state, "(", ")");
        }
        if (peek(state) && peek(state).text === ";") {
            eat(state);
        }
        return;
    }
    if (name === "Let" || name === "Var" || name === "Const") {
        parseBinding(state, name);
        return;
    }
    if (name === "Class" || name === "Struct" || name === "Interface" || name === "Trait" || name === "Enum" || name === "Service") {
        parseTypeDecl(state, mods, name);
        return;
    }
    if (token.kind === "identifier" && state.tokens[state.index + 1] && state.tokens[state.index + 1].kind === "string") {
        paint(token, "function", [], "decl");
        eat(state);
        eat(state);
        parseTrailingBlock(state, false);
        return;
    }
    if (token.kind === "identifier" && state.tokens[state.index + 1] && state.tokens[state.index + 1].text === ":") {
        paint(token, "label", ["declaration"], "label");
        eat(state);
        eat(state);
        if (peek(state) && peek(state).text !== ";" && peek(state).text !== "}") {
            parseStatement(state);
        } else if (peek(state) && peek(state).text === ";") {
            eat(state);
        }
        return;
    }
    if (looksLikeTypedDeclaration(state)) {
        parseTypedDeclaration(state, "");
        return;
    }
    scanExpression(state, () => peek(state) && (peek(state).text === ";" || peek(state).text === "}"));
    if (peek(state) && peek(state).text === ";") {
        eat(state);
    }
}

function parseIf(state) {
    eat(state);
    scanHeader(state);
    parseTrailingBlock(state, false);
    if (peek(state) && textOf(peek(state)) === "Else") {
        eat(state);
        if (peek(state) && textOf(peek(state)) === "If") {
            parseIf(state);
        } else {
            parseTrailingBlock(state, false);
        }
    }
}

function parseFor(state) {
    eat(state);
    state.loopDepth += 1;
    state.scopes.push({ names: new Map(), kind: "for" });
    state.localStack.push([]);
    const locals = [];
    if (peek(state) && peek(state).text === "(") {
        eat(state);
        if (peek(state) && peek(state).text !== ";") {
            if (textOf(peek(state)) === "Var" || textOf(peek(state)) === "Let") {
                const keyword = textOf(eat(state));
                if (peek(state) && peek(state).kind === "identifier") {
                    locals.push(captureLocal(state, eat(state), "", keyword));
                }
                scanExpression(state, () => peek(state) && (peek(state).text === ";" || peek(state).text === ")"));
            } else if (looksLikeTypedDeclaration(state)) {
                const typeName = parseType(state);
                if (peek(state) && peek(state).kind === "identifier") {
                    locals.push(captureLocal(state, eat(state), typeName, "Var"));
                }
                scanExpression(state, () => peek(state) && (peek(state).text === ";" || peek(state).text === ")"));
            } else {
                while (peek(state) && peek(state).text !== ";" && peek(state).text !== ")" && peek(state).kind !== "eof") {
                    if (peek(state).kind === "identifier" && (!state.tokens[state.index + 1] || state.tokens[state.index + 1].text === "," || state.tokens[state.index + 1].text === ")")) {
                        locals.push(captureLocal(state, eat(state), "", "Var"));
                    } else {
                        eat(state);
                    }
                }
            }
        }
        while (peek(state) && peek(state).text !== ")" && peek(state).kind !== "eof") {
            eat(state);
        }
        if (peek(state) && peek(state).text === ")") {
            eat(state);
        }
    } else if (peek(state) && peek(state).kind === "identifier") {
        locals.push(captureLocal(state, eat(state), "", "Var"));
    }
    if (peek(state) && textOf(peek(state)) === "In") {
        eat(state);
        scanExpression(state, () => peek(state) && (peek(state).text === "{" || peek(state).text === ";"));
    }
    const endTarget = locals;
    parseTrailingBlock(state, true);
    const end = peek(state) ? peek(state).start : state.source.length;
    for (const local of endTarget) {
        local.visibleTo = end;
    }
    closeLocals(state, end);
    state.scopes.pop();
    state.loopDepth -= 1;
}

function captureLocal(state, token, typeName, keyword) {
    paint(token, "variable", ["declaration"], "decl");
    return addSymbol(state, {
        ...symbolAt(token, token.text, "variable", typeName, false, false),
        typeName,
        visibleFrom: token.end,
        detail: `${keyword} ${token.text}${typeName ? `: ${typeName}` : ""}`
    });
}

function parseMatch(state) {
    if (!peek(state) || peek(state).text !== "{") {
        return;
    }
    eat(state);
    while (peek(state) && peek(state).text !== "}" && peek(state).kind !== "eof") {
        const bindings = [];
        state.scopes.push({ names: new Map(), kind: "match" });
        state.localStack.push([]);
        if (canStartType(peek(state))) {
            parseType(state);
            if (peek(state) && peek(state).kind === "identifier") {
                bindings.push(captureLocal(state, eat(state), "", "Let"));
            }
        } else if (peek(state).kind === "identifier" && peek(state).text !== "_") {
            eat(state);
        } else {
            eat(state);
        }
        if (peek(state) && peek(state).text === "=>") {
            eat(state);
        }
        if (peek(state) && peek(state).text === "{") {
            parseBlock(state, false);
        } else {
            scanExpression(state, () => peek(state) && (peek(state).text === ";" || peek(state).text === "}"));
            if (peek(state) && peek(state).text === ";") {
                eat(state);
            }
        }
        const end = peek(state) ? peek(state).start : state.source.length;
        for (const binding of bindings) {
            binding.visibleTo = end;
        }
        closeLocals(state, end);
        state.scopes.pop();
    }
    if (peek(state) && peek(state).text === "}") {
        eat(state);
    }
}

function parseTrailingBlock(state, isLoop) {
    if (peek(state) && peek(state).text === "{") {
        parseBlock(state, isLoop);
    } else if (peek(state) && peek(state).text !== ";" && peek(state).text !== "}") {
        parseStatement(state);
    }
}

function scanHeader(state) {
    scanExpression(state, () => peek(state) && (peek(state).text === "{" || peek(state).text === ";"));
}

function finishSimple(state) {
    scanExpression(state, () => peek(state) && (peek(state).text === ";" || peek(state).text === "}"));
    if (peek(state) && peek(state).text === ";") {
        eat(state);
    }
}

function skipBalanced(state, open, close) {
    if (!peek(state) || peek(state).text !== open) {
        return;
    }
    let depth = 0;
    while (peek(state) && peek(state).kind !== "eof") {
        const token = eat(state);
        if (token.text === open) {
            depth += 1;
        } else if (token.text === close) {
            depth -= 1;
            if (depth === 0) {
                return;
            }
        }
    }
}

function scanExpression(state, stop) {
    let paren = 0;
    let bracket = 0;
    let brace = 0;
    while (peek(state) && peek(state).kind !== "eof") {
        if (paren === 0 && bracket === 0 && brace === 0 && stop()) {
            return;
        }
        const token = peek(state);
        if (token.kind === "identifier" || (token.kind === "keyword" && ["True", "False", "Null", "This", "Base"].includes(token.canonical))) {
            const previous = previousCode(state);
            const next = nextCode(state);
            if (previous && (previous.text === "." || previous.text === "?.")) {
                paint(token, next && next.text === "(" ? "method" : "property", [], next && next.text === "(" ? "call" : "member");
            } else if (next && next.text === "(") {
                paint(token, "function", [], "call");
            } else if (next && next.text === ":" && previous && previous.text !== "?") {
                paint(token, "property", [], "property");
            } else if (next && next.text === "{" && previous && (previous.text === "=" || previous.text === "(" || previous.text === "," || previous.canonical === "Return" || previous.canonical === "Throw")) {
                paint(token, "class", [], "type");
            } else if (!token.role || token.role === "use") {
                paint(token, token.kind === "keyword" ? "keyword" : "variable", [], token.kind === "keyword" ? "" : "use");
            }
            if (state.relax) {
                token.relax = true;
            }
            eat(state);
            continue;
        }
        if (token.text === "(") {
            const close = findMatch(state, state.index, "(", ")");
            const after = close >= 0 ? nextSignificant(state.tokens, close) : null;
            if (after && after.text === "=>" && paren === 0 && brace === 0 && bracket === 0) {
                parseLambda(state);
                continue;
            }
            paren += 1;
            eat(state);
            continue;
        }
        if (token.text === ")") {
            paren = Math.max(0, paren - 1);
            eat(state);
            continue;
        }
        if (token.text === "[") {
            bracket += 1;
            eat(state);
            continue;
        }
        if (token.text === "]") {
            bracket = Math.max(0, bracket - 1);
            eat(state);
            continue;
        }
        if (token.text === "{") {
            brace += 1;
            eat(state);
            continue;
        }
        if (token.text === "}") {
            if (brace === 0 && paren === 0 && bracket === 0) {
                return;
            }
            brace = Math.max(0, brace - 1);
            eat(state);
            continue;
        }
        if ((token.canonical === "Function") && nextCode(state) && nextCode(state).text === "(") {
            parseLambda(state);
            continue;
        }
        if (canStartType(token) && token.kind !== "identifier") {
            paint(token, "type", ["defaultLibrary"], "type");
        }
        eat(state);
    }
}

function parseLambda(state) {
    if (peek(state) && textOf(peek(state)) === "Function") {
        eat(state);
    }
    const params = parseParameterList(state);
    state.scopes.push({ names: new Map(), kind: "lambda" });
    state.localStack.push([]);
    for (const param of params) {
        addSymbol(state, param);
        trackLocal(state, param);
        paint(param.token, "parameter", ["declaration"], "decl");
    }
    if (peek(state) && peek(state).text === "=>") {
        eat(state);
        if (peek(state) && peek(state).text === "{") {
            parseBlock(state, false);
        } else {
            scanExpression(state, () => peek(state) && (peek(state).text === ";" || peek(state).text === "," || peek(state).text === "}" || peek(state).text === ")"));
        }
    } else if (peek(state) && peek(state).text === "{") {
        parseBlock(state, false);
    }
    closeLocals(state, peek(state) ? peek(state).start : state.source.length);
    state.scopes.pop();
}

function inferExpression(state) {
    const token = peek(state);
    if (!token) {
        return "";
    }
    if (token.kind === "string") {
        return "String";
    }
    if (token.kind === "char") {
        return "Char";
    }
    if (token.kind === "number") {
        return token.text.includes(".") ? "Float" : "Int";
    }
    if (token.canonical === "True" || token.canonical === "False") {
        return "Bool";
    }
    if (token.kind === "identifier" && /^[A-Z]/.test(token.text)) {
        return token.text;
    }
    return "";
}

function previousCode(state) {
    for (let index = state.index - 1; index >= 0; index -= 1) {
        if (state.tokens[index].kind !== "comment") {
            return state.tokens[index];
        }
    }
    return null;
}

function nextCode(state) {
    return nextSignificant(state.tokens, state.index);
}

function nextSignificant(tokens, index) {
    for (let cursor = index + 1; cursor < tokens.length; cursor += 1) {
        if (tokens[cursor].kind !== "comment") {
            return tokens[cursor];
        }
    }
    return null;
}

function findMatch(state, index, open, close) {
    let depth = 0;
    for (let cursor = index; cursor < state.tokens.length; cursor += 1) {
        const token = state.tokens[cursor];
        if (token.kind === "comment" || token.kind === "string") {
            continue;
        }
        if (token.text === open) {
            depth += 1;
        } else if (token.text === close) {
            depth -= 1;
            if (depth === 0) {
                return cursor;
            }
        }
    }
    return -1;
}

function symbolAt(token, name, kind, typeName, fileWide, exported) {
    const start = token ? token.start : 0;
    const end = token ? token.end : 0;
    return {
        name,
        kind,
        typeName: typeName || "",
        detail: "",
        token,
        start,
        end,
        nameStart: start,
        nameEnd: end,
        visibleFrom: end,
        visibleTo: end,
        fileWide,
        exported,
        member: false,
        container: "",
        params: []
    };
}

function sourceEnd(state) {
    return state.source.length;
}

module.exports = {
    analyze,
    BUILTIN_NAMES
};
