"use strict";

const fs = require("fs");
const path = require("path");
const { pathToFileURL } = require("url");
const { analyze } = require("./analyze");

let rootOverride = "";
let cache = null;

function setPackageRoot(dir) {
    const next = dir ? path.resolve(dir) : "";
    if (next !== rootOverride) {
        rootOverride = next;
        cache = null;
    }
}

function clearPackageCache() {
    cache = null;
}

function compilerRoot() {
    const candidates = [];
    if (rootOverride) {
        candidates.push(rootOverride);
    }
    const discovered = findRoot(path.resolve(__dirname, "../../.."));
    if (discovered) {
        candidates.push(discovered);
    }
    for (const dir of candidates) {
        if (fs.existsSync(path.join(dir, "Packages"))) {
            return dir;
        }
    }
    return "";
}

function findRoot(start) {
    let dir = start;
    for (let step = 0; step < 8; step += 1) {
        if (fs.existsSync(path.join(dir, "Packages")) && (fs.existsSync(path.join(dir, "SyntaxCompiler.rx")) || fs.existsSync(path.join(dir, "Runtime", "Runtime.cpp")))) {
            return dir;
        }
        const parent = path.dirname(dir);
        if (parent === dir) {
            break;
        }
        dir = parent;
    }
    return "";
}

function packageFile(root, packageName) {
    const leaf = String(packageName).split(".").pop();
    return path.join(root, "Packages", packageName, `${leaf}.rx`);
}

function loadCatalog() {
    const root = compilerRoot();
    if (!root) {
        cache = { root: "", stamp: 0, packages: new Map(), symbols: [] };
        return cache;
    }
    const dir = path.join(root, "Packages");
    const stamp = directoryStamp(dir);
    if (cache && cache.root === root && cache.stamp === stamp) {
        return cache;
    }
    const packages = new Map();
    const symbols = [];
    let names = [];
    try {
        names = fs.readdirSync(dir);
    } catch (error) {
        names = [];
    }
    names.sort();
    for (const packageName of names) {
        const file = packageFile(root, packageName);
        if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
            continue;
        }
        let text = "";
        try {
            text = fs.readFileSync(file, "utf8");
        } catch (error) {
            continue;
        }
        let doc;
        try {
            doc = analyze(text);
        } catch (error) {
            continue;
        }
        const uri = pathToFileURL(file).href;
        const loaded = [];
        for (const symbol of doc.symbols) {
            const copy = {
                ...symbol,
                token: undefined,
                packageName: symbol.packageName || packageName,
                uri
            };
            loaded.push(copy);
            symbols.push(copy);
        }
        const namespace = loaded.find((symbol) => symbol.kind === "namespace" && symbol.summary && String(symbol.detail || "").startsWith("Package "));
        packages.set(packageName, {
            name: packageName,
            file,
            uri,
            description: namespace ? namespace.summary : "",
            symbols: loaded
        });
    }
    cache = { root, stamp, packages, symbols };
    return cache;
}

function directoryStamp(dir) {
    let newest = 0;
    let names = [];
    try {
        names = fs.readdirSync(dir);
    } catch (error) {
        return 0;
    }
    for (const packageName of names) {
        const direct = path.join(dir, packageName, `${packageName.split(".").pop()}.rx`);
        if (!fs.existsSync(direct)) {
            continue;
        }
        try {
            newest = Math.max(newest, fs.statSync(direct).mtimeMs);
        } catch (error) {
            newest += 1;
        }
    }
    return newest;
}

function packageNames() {
    return [...loadCatalog().packages.keys()];
}

function packageRecord(name) {
    const catalog = loadCatalog();
    const lower = String(name || "").toLowerCase();
    if (!lower) {
        return null;
    }
    for (const [key, record] of catalog.packages) {
        if (key.toLowerCase() === lower) {
            return record;
        }
    }
    return null;
}

function packageByName(name) {
    const exact = packageRecord(name);
    if (exact) {
        return exact.name;
    }
    const lower = String(name || "").toLowerCase();
    if (!lower) {
        return "";
    }
    let found = "";
    for (const key of loadCatalog().packages.keys()) {
        if (key.split(".").pop().toLowerCase() !== lower) {
            continue;
        }
        if (found) {
            return "";
        }
        found = key;
    }
    return found;
}

function packageSummary(name) {
    const record = packageRecord(name) || packageRecord(packageByName(name));
    return record ? record.description : "";
}

function catalogSymbols() {
    return loadCatalog().symbols;
}

module.exports = {
    setPackageRoot,
    clearPackageCache,
    compilerRoot,
    packageNames,
    packageByName,
    packageSummary,
    packageRecord,
    catalogSymbols
};
