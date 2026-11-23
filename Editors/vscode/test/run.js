"use strict";

const assert = require("assert");
const fs = require("fs");
const path = require("path");
const { tokenize, keywordCaseEdits, nameCaseEdits } = require("../lib/scanner");
const { declarationNameTokens, toPascal, namingDiagnostic } = require("../lib/names");
const { formatRobix, formatProject } = require("../lib/format");
const { analyze } = require("../lib/analyze");
const { complete, hover, signature: signatureHelp, definition, semanticSpans } = require("../lib/intellisense");
const { SEMANTIC_TYPES } = require("../lib/keywords");
const { extractTests, functionNames, selectTests, buildCompanion } = require("../lib/run-target");
const { findTool } = require("../lib/tools");

const root = path.resolve(__dirname, "../../..");
let failed = 0;

function check(name, fn) {
    try {
        fn();
        console.log(`ok  ${name}`);
    } catch (error) {
        failed += 1;
        console.error(`FAIL ${name}`);
        console.error(error.stack || error.message);
    }
}

function signature(source) {
    const { tokens } = tokenize(source);
    const declared = new Set(declarationNameTokens(tokens).map((token) => token.start));
    return tokens
        .filter((token) => token.kind !== "comment" && token.kind !== "eof")
        .map((token) => {
            if (token.kind === "keyword") {
                return token.canonical;
            }
            if (declared.has(token.start)) {
                return toPascal(token.text);
            }
            return token.text;
        })
        .join("\n");
}

function commentsOf(source) {
    return tokenize(source).tokens.filter((token) => token.kind === "comment").map((token) => token.text).join("\n");
}

check("keywords keep PascalCase and ASM", () => {
    const { tokens } = tokenize("function IF asm ASM string myValue");
    const words = tokens.filter((token) => token.kind !== "eof").map((token) => token.canonical || token.text);
    assert.deepStrictEqual(words, ["Function", "If", "ASM", "ASM", "String", "myValue"]);
    assert.strictEqual(tokens.find((token) => token.text === "myValue").kind, "identifier");
});

check("strings, comments, and ranges stay whole", () => {
    const source = 'r$"a\\n{name}" // note\n/* block */ 1..=5 1..2 0xFF';
    const kinds = tokenize(source).tokens.filter((token) => token.kind !== "eof").map((token) => token.kind);
    assert.deepStrictEqual(kinds, ["string", "comment", "comment", "number", "operator", "number", "number", "operator", "number", "number"]);
});

check("keyword case waits for a boundary and ignores names", () => {
    assert.strictEqual(keywordCaseEdits("function", 8, "n").length, 0);
    const spaced = keywordCaseEdits("function ", 8, " ");
    assert.strictEqual(spaced.length, 1);
    assert.strictEqual(spaced[0].text, "Function");
    assert.strictEqual(keywordCaseEdits("myfunction ", 11, " ").length, 0);
    const pasted = keywordCaseEdits("if(true) { return false; }", 0, "if(true) { return false; }");
    assert.deepStrictEqual(pasted.map((edit) => edit.text), ["If", "True", "Return", "False"]);
    assert.strictEqual(keywordCaseEdits("if", 1, "f")[0].text, "If");
    assert.strictEqual(keywordCaseEdits("asm", 2, "m")[0].text, "ASM");
    assert.strictEqual(keywordCaseEdits("in", 1, "n").length, 0);
    assert.strictEqual(keywordCaseEdits("myValue", 6, "e").length, 0);
});

check("package, function, and class names become PascalCase", () => {
    const packaged = nameCaseEdits("package robix.", "package robix".length, ".");
    assert.deepStrictEqual(packaged.map((edit) => edit.text), ["Robix"]);
    const imported = nameCaseEdits("import robix.core;", 0, "import robix.core;");
    assert.deepStrictEqual(imported.map((edit) => edit.text), ["Robix", "Core"]);
    const fn = nameCaseEdits("function main(", "function main".length, "(");
    assert.deepStrictEqual(fn.map((edit) => edit.text), ["Main"]);
    const typeName = nameCaseEdits("class navHost {", "class navHost".length, " {");
    assert.deepStrictEqual(typeName.map((edit) => edit.text), ["NavHost"]);
    const structName = nameCaseEdits("struct point ", "struct point".length, " ");
    assert.deepStrictEqual(structName.map((edit) => edit.text), ["Point"]);
    assert.strictEqual(nameCaseEdits("var myValue =", "var myValue ".length, "=").length, 0);
    assert.strictEqual(nameCaseEdits("function main", 12, "n").length, 0);
});

check("variables become camelCase while functions stay PascalCase", () => {
    const field = "Private List<String> PageTitles;";
    assert.deepStrictEqual(nameCaseEdits(field, field.length - 1, ";").map((edit) => edit.text), ["pageTitles"]);
    const local = nameCaseEdits("Var Total =", "Var Total".length, " =");
    assert.deepStrictEqual(local.map((edit) => edit.text), ["total"]);
    const loop = nameCaseEdits("For Index In", "For Index".length, " ");
    assert.deepStrictEqual(loop.map((edit) => edit.text), ["index"]);
    const param = "Function Main(String FileName)";
    assert.deepStrictEqual(nameCaseEdits(param, param.length - 1, ")").map((edit) => edit.text), ["fileName"]);
    assert.strictEqual(nameCaseEdits("Const MAX_SIZE =", "Const MAX_SIZE".length, " =").length, 0);
    assert.deepStrictEqual(nameCaseEdits("function main(", "function main".length, "(").map((edit) => edit.text), ["Main"]);
    const camelField = namingDiagnostic("RX032", "Field 'pageTitles' should use PascalCase.");
    assert.strictEqual(camelField.drop, true);
    const otherField = namingDiagnostic("RX032", "Field 'page_titles' should use PascalCase.");
    assert.strictEqual(otherField.code, "RX031");
    assert.strictEqual(otherField.message, "Field 'page_titles' should use camelCase.");
    const fn = namingDiagnostic("RX032", "Function 'main' should use PascalCase.");
    assert.strictEqual(fn.drop, false);
    assert.strictEqual(fn.code, "RX032");
});

check("functions and known packages are colored, unknown packages are not", () => {
    const source = "Import Robix.Math;\nImport Missing.Thing;\nFunction Main() -> Int {\n    Return Math.Abs(1);\n    Print(1);\n}\n";
    const doc = analyze(source);
    const typed = (text) => doc.tokens.filter((token) => token.text === text).map((token) => token.semType);
    assert.deepStrictEqual(typed("Robix"), ["namespace"]);
    assert.deepStrictEqual(typed("Math"), ["namespace", "namespace"]);
    assert.deepStrictEqual(typed("Missing"), ["variable"]);
    assert.deepStrictEqual(typed("Thing"), ["variable"]);
    assert.deepStrictEqual(typed("Main"), ["function"]);
    assert.deepStrictEqual(typed("Abs"), ["method"]);
    assert.deepStrictEqual(typed("Print"), ["function"]);
    const spans = semanticSpans(source);
    const functionType = SEMANTIC_TYPES.indexOf("function");
    const namespaceType = SEMANTIC_TYPES.indexOf("namespace");
    assert.ok(functionType > 0);
    assert.strictEqual(namespaceType, 0);
    assert.ok(spans.data.includes(functionType));
    assert.ok(spans.data.includes(namespaceType));
});

check("typing a keyword does not offer its template", () => {
    const items = complete("if", 2).items;
    assert.ok(items.some((item) => item.kind === "keyword" && item.label === "If"));
    assert.ok(!items.some((item) => item.snippet && item.label.toLowerCase().startsWith("if")));
    const open = complete("\n", 1).items;
    const keyword = open.find((item) => item.kind === "keyword" && item.label === "If");
    const snippet = open.find((item) => item.snippet);
    assert.ok(keyword);
    assert.ok(snippet);
    assert.ok(snippet.sort > keyword.sort);
});

check("formatter keeps every token and is stable", () => {
    const files = [];
    function walk(dir) {
        for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
            if (entry.name === "Build" || entry.name === "Editors" || entry.name === "node_modules") {
                continue;
            }
            const full = path.join(dir, entry.name);
            if (entry.isDirectory()) {
                walk(full);
            } else if (entry.name.endsWith(".rx") || entry.name.endsWith(".rxp")) {
                files.push(full);
            }
        }
    }
    walk(root);
    assert.ok(files.length > 40, `expected source files, found ${files.length}`);
    for (const file of files) {
        const source = fs.readFileSync(file, "utf8");
        const formatted = file.endsWith(".rxp") ? formatProject(source) : formatRobix(source);
        assert.strictEqual(signature(formatted), signature(source), `tokens changed in ${path.relative(root, file)}`);
        assert.strictEqual(commentsOf(formatted), commentsOf(source), `comments changed in ${path.relative(root, file)}`);
        const again = file.endsWith(".rxp") ? formatProject(formatted) : formatRobix(formatted);
        assert.strictEqual(again, formatted, `unstable format in ${path.relative(root, file)}`);
    }
});

check("statements, braces, and keyword case are formatted", () => {
    const messy = 'package robotics.hello;\nimport robix.core;\npublic function main(Args<string> args)->int explain{description:"Hi";input:none;processing:"Add";output:"Zero";}{var total=0;for index in 1..=5{total=total+index;}if(total>0){print(total);}else{print(0);}return 0;}\n';
    const formatted = formatRobix(messy);
    assert.ok(formatted.includes("Package Robotics.Hello;"), formatted);
    assert.ok(formatted.includes("Import Robix.Core;"), formatted);
    assert.ok(formatted.includes("Public Function Main(Args<String> args) -> Int\nExplain {"), formatted);
    assert.ok(formatted.includes("For index In 1..=5 {"), formatted);
    assert.ok(formatted.includes("} Else {"), formatted);
    assert.ok(!formatted.includes("function"), formatted);
    assert.ok(formatted.includes("myValue") === false);
    const lines = formatted.trim().split("\n");
    assert.ok(lines.every((line, index) => index === 0 || !line.endsWith(" ") ), "trailing space");
    assert.strictEqual(formatRobix(formatted), formatted);
});

check("hello robot exposes symbols, completion, hover, and signatures", () => {
    const source = fs.readFileSync(path.join(root, "Examples/HelloRobot.rx"), "utf8");
    const doc = analyze(source);
    const names = doc.symbols.map((symbol) => `${symbol.kind}:${symbol.name}`);
    assert.ok(names.includes("function:Main"), names.join(", "));
    assert.ok(names.includes("parameter:args"));
    assert.ok(names.includes("variable:total"));
    assert.ok(names.includes("variable:index"));
    const errors = doc.diagnostics.filter((item) => item.severity === "error");
    assert.deepStrictEqual(errors.map((item) => item.message), []);
    const atTotal = source.indexOf("total = total");
    const items = complete(source, atTotal).items.map((item) => item.label);
    assert.ok(items.includes("Print"));
    assert.ok(items.includes("Function"));
    assert.ok(items.includes("total"));
    assert.ok(items.includes("Int"));
    const call = source.indexOf("Count");
    const members = complete(source, call + 2).items.map((item) => item.label);
    assert.ok(members.includes("Count") || members.includes("ToString"), members.join(", "));
    const main = hover(source, source.indexOf("Function Main") + "Function ".length);
    assert.ok(main.contents.includes("Main"));
    const sign = signatureHelp(source, source.indexOf("Count()") + "Count(".length);
    assert.ok(sign);
    const defined = definition(source, source.indexOf("total ="));
    assert.ok(defined.some((symbol) => symbol.name === "total"));
});

check("structural diagnostics catch broken code", () => {
    const source = "Package Demo;\n\nPublic Function Main() -> Int {\n    Break;\n    Await work();\n    Return notDeclared;\n}\n";
    const doc = analyze(source);
    const messages = doc.diagnostics.map((item) => item.message);
    assert.ok(messages.some((message) => message.includes("outside a loop")), messages.join("\n"));
    assert.ok(messages.some((message) => message.includes("Await")), messages.join("\n"));
    assert.ok(messages.some((message) => message.includes("notDeclared")), messages.join("\n"));
    const mismatch = analyze("Package Demo;\nPublic Function Main() -> Int {\n    Return 0;\n");
    assert.ok(mismatch.diagnostics.some((item) => item.message.includes("Unclosed")));
});

check("classes, enums, and tests keep their members", () => {
    const objects = analyze(fs.readFileSync(path.join(root, "Examples/Objects.rx"), "utf8"));
    assert.ok(objects.symbols.some((symbol) => symbol.kind === "class" && symbol.name === "RoboRobot"));
    assert.ok(objects.symbols.some((symbol) => symbol.member && symbol.name === "Move"));
    assert.ok(objects.symbols.some((symbol) => symbol.kind === "enumMember" && symbol.name === "Idle"));
    const tests = analyze(fs.readFileSync(path.join(root, "NativeEmit.test.rx"), "utf8"));
    assert.ok(tests.symbols.some((symbol) => symbol.name === "EmitDestructors"));
    assert.ok(!tests.diagnostics.some((item) => item.severity === "error" && item.message.includes("already declared")));
});

check("a function run keeps only that function's tests", () => {
    const source = fs.readFileSync(path.join(root, "NameChecker.rx"), "utf8");
    const tests = extractTests(fs.readFileSync(path.join(root, "NameChecker.test.rx"), "utf8"));
    const selected = selectTests(tests, { name: "Check", kind: "function", container: "" });
    assert.strictEqual(selected.length, 1);
    assert.ok(selected[0].text.includes("check empty module"));
    const companion = buildCompanion(selected, functionNames(source));
    assert.ok(companion.includes("Test Check {"));
    assert.ok(companion.includes("check empty module"));
    assert.ok(!companion.includes("walk empty node"));
    assert.ok(companion.includes("Test Walk {"));
    const dotted = extractTests("Test RxuMessages.Add {\n    Case \"records a code\" {\n        Output: True;\n    }\n}\n");
    assert.deepStrictEqual(dotted.map((test) => `${test.owner}.${test.name}`), ["RxuMessages.Add"]);
    const classTests = selectTests(dotted, { name: "RxuMessages", kind: "class", container: "" });
    assert.strictEqual(classTests.length, 1);
});

check("tools are found on PATH before the project build", () => {
    const files = new Set(["/opt/robix/bin/robix", "/work/Build/bootstrap/robix-run.app"]);
    const found = findTool({
        configured: "",
        pathEnv: "/usr/bin:/opt/robix/bin",
        delimiter: ":",
        names: ["robix", "robix.exe"],
        root: "/work",
        relatives: ["Build/bootstrap/robix-run.app"],
        exists: (file) => files.has(file)
    });
    assert.strictEqual(found, "/opt/robix/bin/robix");
    const fallback = findTool({
        configured: "",
        pathEnv: "/usr/bin",
        delimiter: ":",
        names: ["robix"],
        root: "/work",
        relatives: ["Build/bootstrap/robix-run.app"],
        exists: (file) => files.has(file)
    });
    assert.strictEqual(fallback, "/work/Build/bootstrap/robix-run.app");
    const pinned = findTool({
        configured: "/custom/robix",
        pathEnv: "/opt/robix/bin",
        delimiter: ":",
        names: ["robix"],
        root: "/work",
        relatives: ["Build/bootstrap/robix-run.app"],
        exists: (file) => file === "/custom/robix" || files.has(file)
    });
    assert.strictEqual(pinned, "/custom/robix");
});

check("wrong case still suggests the PascalCase package, function, and class", () => {
    const packages = complete("import robix.core", "import robix.core".length).items;
    assert.ok(packages.some((item) => item.label === "Robix.Core" && item.insert === "Robix.Core"), packages.map((item) => item.label).join(", "));
    const invented = complete("package robotics.hello", "package robotics.hello".length).items;
    assert.ok(invented.some((item) => item.label === "Robotics.Hello" && item.insert === "Robotics.Hello"));
    const fn = complete("function main", "function main".length).items;
    assert.ok(fn.some((item) => item.label === "Main" && item.insert === "Main"));
    const typeName = complete("class navHost", "class navHost".length).items;
    assert.ok(typeName.some((item) => item.label === "NavHost" && item.insert === "NavHost"));
    const variable = complete("Var PageTitles", "Var PageTitles".length).items;
    assert.ok(variable.some((item) => item.label === "pageTitles" && item.insert === "pageTitles"));
    const typed = complete("String FileName", "String FileName".length).items;
    assert.ok(typed.some((item) => item.label === "fileName" && item.insert === "fileName"));
    const call = complete("print", "print".length).items;
    assert.ok(call.some((item) => item.label === "Print" && item.filter.startsWith("print")));
    const members = complete("math.ab", "math.ab".length).items.map((item) => item.label);
    assert.ok(members.includes("Abs"), members.join(", "));
});

check("unimported functions stay below local names and bring their package", () => {
    const source = "Package Demo;\n\nPublic Function Main() -> Int {\n    Var total = 0;\n    Return \n}\n";
    const offset = source.indexOf("Return ") + "Return ".length;
    const items = complete(source, offset).items;
    const total = items.find((item) => item.label === "total");
    const main = items.find((item) => item.label === "Main" && item.kind === "function");
    const abs = items.find((item) => item.label === "Abs");
    assert.ok(total && main && abs);
    assert.ok(total.sort < main.sort, `${total.sort} ${main.sort}`);
    assert.ok(main.sort < abs.sort, `${main.sort} ${abs.sort}`);
    assert.ok(abs.documentation.includes("Explain {"));
    assert.ok(abs.documentation.includes("Absolute value"));
    assert.ok(abs.importEdit && abs.importEdit.text.includes("Import Robix.Math;"));
    const imported = "Package Demo;\nImport Robix.Math;\n\nPublic Function Main() -> Int {\n    Return \n}\n";
    const known = complete(imported, imported.indexOf("Return ") + "Return ".length).items.find((item) => item.label === "Abs");
    assert.ok(known);
    assert.ok(!known.importEdit);
    assert.ok(known.sort < abs.sort);
    const hello = fs.readFileSync(path.join(root, "Examples/HelloRobot.rx"), "utf8");
    const explained = hover(hello, hello.indexOf("Function Main") + "Function ".length);
    assert.ok(explained.contents.includes("Explain {"));
    assert.ok(explained.contents.includes("Greet the operator"));
});

check("a package requires a description and the editor shows it", () => {
    const missing = analyze("Package Demo;\n");
    assert.ok(missing.diagnostics.some((item) => item.code === "RX173"), missing.diagnostics.map((item) => item.message).join("\n"));
    const source = "Package Robix.Console\nExplain {\n    Description: \"Terminal text and color.\";\n};\nImport Robix.Math;\n";
    const doc = analyze(source);
    assert.ok(!doc.diagnostics.some((item) => item.code === "RX173"));
    const own = hover(source, source.indexOf("Robix.Console"));
    assert.ok(own.contents.includes("Terminal text and color."));
    const imported = hover(source, source.indexOf("Robix.Math"));
    assert.ok(imported.contents.includes("Absolute value") || imported.contents.includes("logarithms"), imported.contents);
    const suggested = complete("import robix.console", "import robix.console".length).items.find((item) => item.label === "Robix.Console");
    assert.ok(suggested && suggested.documentation.includes("Terminal text"));
});

check("an import alias completes that package", () => {
    const source = "Package Demo\nExplain {\n    Description: \"Demo.\";\n};\nImport Robix.Core As Nm;\nPublic Function Main() -> Int {\n    Nm.\n    Return 0;\n}\n";
    const labels = complete(source, source.indexOf("Nm.") + 3).items.map((item) => item.label);
    assert.ok(labels.includes("Print"), labels.join(", "));
    assert.ok(labels.includes("Some"), labels.join(", "));
    assert.ok(!labels.includes("Length"), labels.join(", "));
    assert.ok(!labels.includes("ToString"), labels.join(", "));
    const math = "Package Demo\nExplain {\n    Description: \"Demo.\";\n};\nImport Robix.Math As Calc;\nPublic Function Main() -> Int {\n    Calc.\n    Return 0;\n}\n";
    const mathLabels = complete(math, math.indexOf("Calc.") + 5).items.map((item) => item.label);
    assert.ok(mathLabels.includes("Abs"), mathLabels.join(", "));
    assert.ok(!mathLabels.includes("Length"), mathLabels.join(", "));
    const plain = "Package Demo\nExplain {\n    Description: \"Demo.\";\n};\nImport Robix.Core;\nPublic Function Main() -> Int {\n    Core.\n    Return 0;\n}\n";
    const coreLabels = complete(plain, plain.indexOf("Core.") + 5).items.map((item) => item.label);
    assert.ok(coreLabels.includes("Print"), coreLabels.join(", "));
    assert.ok(!coreLabels.includes("ToString"), coreLabels.join(", "));
    const text = "Package Demo\nExplain {\n    Description: \"Demo.\";\n};\nPublic Function Main() -> Int {\n    Var name = \"Robix\";\n    name.\n    Return 0;\n}\n";
    const textLabels = complete(text, text.indexOf("name.") + 5).items.map((item) => item.label);
    assert.ok(textLabels.includes("Length"), textLabels.join(", "));
    assert.ok(textLabels.includes("ToString"), textLabels.join(", "));
});

check("user names are not recased", () => {
    const source = "Package Demo;\n\nPublic Function Main() -> Int {\n    Var myValue = 1;\n    Return myValue;\n}\n";
    const formatted = formatRobix(source);
    assert.ok(formatted.includes("myValue"));
    assert.ok(!formatted.includes("MyValue"));
});

if (failed > 0) {
    console.error(`\n${failed} failed`);
    process.exit(1);
}
console.log("\nall passed");
