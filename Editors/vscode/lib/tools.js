"use strict";

const fs = require("fs");
const path = require("path");

function findTool(options) {
    const exists = options.exists || isExecutable;
    const configured = String(options.configured || "").trim();
    if (configured && exists(configured)) {
        return configured;
    }
    const pathEnv = options.pathEnv != null ? options.pathEnv : process.env.PATH || "";
    const delimiter = options.delimiter || path.delimiter;
    for (const directory of String(pathEnv).split(delimiter)) {
        if (!directory) {
            continue;
        }
        for (const name of options.names || []) {
            const candidate = path.join(directory, name);
            if (exists(candidate)) {
                return candidate;
            }
        }
    }
    const root = options.root || "";
    for (const relative of options.relatives || []) {
        const candidate = path.join(root, relative);
        if (exists(candidate)) {
            return candidate;
        }
    }
    return "";
}

function isExecutable(file) {
    try {
        const stat = fs.statSync(file);
        if (!stat.isFile()) {
            return false;
        }
        if (process.platform === "win32") {
            return true;
        }
        return (stat.mode & 0o111) !== 0;
    } catch (error) {
        return false;
    }
}

module.exports = { findTool, isExecutable };
