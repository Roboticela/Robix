"use strict";

const { spawn } = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");

let buffer = Buffer.alloc(0);
let seq = 1;

function send(message) {
    const body = Buffer.from(JSON.stringify(message), "utf8");
    process.stdout.write(`Content-Length: ${body.length}\r\n\r\n`);
    process.stdout.write(body);
}

function event(name, body) {
    send({ seq: seq++, type: "event", event: name, body: body || {} });
}

function respond(request, body, success = true, message = "") {
    send({
        seq: seq++,
        type: "response",
        request_seq: request.seq,
        command: request.command,
        success,
        message,
        body: body || {}
    });
}

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
        const request = JSON.parse(buffer.slice(start, start + length).toString("utf8"));
        buffer = buffer.slice(start + length);
        handle(request).catch((error) => {
            respond(request, { error: { id: 1103, format: String(error.message || error) } }, false, String(error.message || error));
        });
    }
});

const session = {
    gdb: null,
    child: null,
    token: 1,
    pending: new Map(),
    stdout: "",
    breakpoints: new Map(),
    functions: [],
    data: [],
    instructions: [],
    config: {},
    stopped: false,
    frame: 0,
    justMyCode: true
};

async function handle(request) {
    const args = request.arguments || {};
    if (request.command === "initialize") {
        respond(request, {
            supportsConfigurationDoneRequest: true,
            supportsConditionalBreakpoints: true,
            supportsHitConditionalBreakpoints: true,
            supportsLogPoints: true,
            supportsFunctionBreakpoints: true,
            supportsDataBreakpoints: true,
            supportsInstructionBreakpoints: true,
            supportsDisassembleRequest: true,
            supportsReadMemoryRequest: true,
            supportsWriteMemoryRequest: true,
            supportsEvaluateForHovers: true,
            supportsSetVariable: true,
            supportsTerminateRequest: true,
            supportsExceptionInfoRequest: true,
            supportsExceptionFilterOptions: true,
            supportsRestartRequest: true,
            supportsValueFormattingOptions: true,
            supportsClipboardContext: true,
            exceptionBreakpointFilters: [
                { filter: "raised", label: "Raised errors", default: false },
                { filter: "unhandled", label: "Unhandled errors", default: true }
            ]
        });
        event("initialized");
        return;
    }
    if (request.command === "launch" || request.command === "attach") {
        session.config = args;
        session.justMyCode = args.justMyCode !== false;
        if (args.noDebug) {
            await runDirect(args);
            respond(request, {});
            return;
        }
        const program = await resolveProgram(args);
        const gdb = findGdb();
        if (!program || !gdb || args.request === "interpreter") {
            event("output", { category: "console", output: "Line records are unavailable, so this session runs the program without stopping.\n" });
            await runDirect(args);
            respond(request, {});
            return;
        }
        await startGdb(gdb, program, args);
        respond(request, {});
        return;
    }
    if (request.command === "setBreakpoints") {
        const file = args.source && args.source.path;
        const lines = [];
        for (const source of args.breakpoints || []) {
            const verified = Boolean(session.gdb);
            lines.push({
                id: source.line,
                verified,
                line: source.line,
                message: verified ? "" : "The compiler has not emitted line records for this file.",
                source: args.source
            });
            if (session.gdb && file) {
                const condition = source.condition ? ` -c ${JSON.stringify(source.condition)}` : "";
                await gdbCommand(`-break-insert -f${condition} ${file}:${source.line}`);
                if (source.logMessage) {
                    await gdbCommand(`-break-commands ${source.line} "silent" "printf \\"${source.logMessage}\\"" "continue"`);
                }
                if (source.hitCondition && /^\d+$/.test(source.hitCondition)) {
                    await gdbCommand(`-break-after ${source.line} ${Number(source.hitCondition) - 1}`);
                }
            }
        }
        session.breakpoints.set(file, lines);
        respond(request, { breakpoints: lines });
        return;
    }
    if (request.command === "setFunctionBreakpoints") {
        session.functions = [];
        for (const item of args.breakpoints || []) {
            if (session.gdb) {
                await gdbCommand(`-break-insert -f ${item.name}`);
            }
            session.functions.push({ verified: Boolean(session.gdb), message: item.name });
        }
        respond(request, { breakpoints: session.functions });
        return;
    }
    if (request.command === "setDataBreakpoints") {
        session.data = [];
        for (const item of args.breakpoints || []) {
            if (session.gdb) {
                await gdbCommand(`-break-watch ${item.dataId}`);
            }
            session.data.push({ verified: Boolean(session.gdb), dataId: item.dataId });
        }
        respond(request, { breakpoints: session.data });
        return;
    }
    if (request.command === "setInstructionBreakpoints") {
        session.instructions = [];
        for (const item of args.breakpoints || []) {
            if (session.gdb) {
                await gdbCommand(`-break-insert -f *${item.instructionReference}`);
            }
            session.instructions.push({ verified: Boolean(session.gdb), instructionReference: item.instructionReference });
        }
        respond(request, { breakpoints: session.instructions });
        return;
    }
    if (request.command === "setExceptionBreakpoints") {
        if (session.gdb && (args.filters || []).includes("raised")) {
            await gdbCommand("-catch-throw");
        }
        respond(request, {});
        return;
    }
    if (request.command === "configurationDone") {
        if (session.gdb) {
            await gdbCommand(session.config.stopOnEntry ? "-exec-step" : "-exec-run");
        }
        respond(request, {});
        return;
    }
    if (request.command === "threads") {
        respond(request, { threads: [{ id: 1, name: "Robix" }] });
        return;
    }
    if (request.command === "stackTrace") {
        const frames = await framesOf();
        respond(request, { stackFrames: frames, totalFrames: frames.length });
        return;
    }
    if (request.command === "scopes") {
        respond(request, {
            scopes: [
                { name: "Locals", presentationHint: "locals", variablesReference: 1000 + (args.frameId || 0), expensive: false },
                { name: "Autos", presentationHint: "locals", variablesReference: 2000 + (args.frameId || 0), expensive: false }
            ]
        });
        return;
    }
    if (request.command === "variables") {
        const list = await localsOf(args.variablesReference);
        respond(request, { variables: list });
        return;
    }
    if (request.command === "evaluate") {
        const value = await evaluate(args.expression);
        respond(request, { result: value, variablesReference: 0 });
        return;
    }
    if (request.command === "setVariable") {
        await gdbCommand(`-gdb-set var ${args.name}=${args.value}`);
        respond(request, { value: args.value });
        return;
    }
    if (request.command === "continue" || request.command === "next" || request.command === "stepIn" || request.command === "stepOut" || request.command === "pause") {
        const command = {
            continue: "-exec-continue",
            next: "-exec-next",
            stepIn: "-exec-step",
            stepOut: "-exec-finish",
            pause: "-exec-interrupt"
        }[request.command];
        if (session.gdb) {
            await gdbCommand(command);
        }
        respond(request, { allThreadsContinued: request.command !== "pause" });
        return;
    }
    if (request.command === "disassemble") {
        const text = session.gdb ? await gdbCommand(`-data-disassemble -s "$pc" -e "$pc+64" -- 0`) : "";
        const instructions = [];
        const pattern = /address="([^"]+)"[^}]*inst="([^"]*)"/g;
        let match = pattern.exec(text);
        while (match) {
            instructions.push({ address: match[1], instruction: match[2].replace(/\\n/g, "") });
            match = pattern.exec(text);
        }
        respond(request, { instructions });
        return;
    }
    if (request.command === "readMemory") {
        const count = args.count || 16;
        const text = session.gdb ? await gdbCommand(`-data-read-memory-bytes ${args.memoryReference} ${count}`) : "";
        const found = /contents="([0-9a-fA-F]*)"/.exec(text);
        const hex = found ? found[1] : "";
        respond(request, {
            address: args.memoryReference,
            data: Buffer.from(hex, "hex").toString("base64")
        });
        return;
    }
    if (request.command === "writeMemory") {
        const bytes = Buffer.from(args.data, "base64");
        if (session.gdb) {
            await gdbCommand(`-data-write-memory-bytes ${args.memoryReference} ${bytes.toString("hex")}`);
        }
        respond(request, { bytesWritten: bytes.length });
        return;
    }
    if (request.command === "disconnect" || request.command === "terminate") {
        await stopAll();
        respond(request, {});
        event("terminated");
        return;
    }
    if (request.command === "restart") {
        await stopAll();
        respond(request, {});
        event("terminated");
        return;
    }
    if (request.command === "exceptionInfo") {
        respond(request, { exceptionId: "error", description: "Raised error", breakMode: "unhandled" });
        return;
    }
    if (request.command === "modules") {
        respond(request, { modules: session.program ? [{ id: 1, name: path.basename(session.program), path: session.program }] : [] });
        return;
    }
    respond(request, {});
}

function findGdb() {
    const names = process.platform === "win32" ? ["gdb.exe"] : ["gdb"];
    const directories = String(process.env.PATH || "").split(path.delimiter);
    for (const directory of directories) {
        for (const name of names) {
            const candidate = path.join(directory, name);
            if (fs.existsSync(candidate)) {
                return candidate;
            }
        }
    }
    return "";
}

async function resolveProgram(args) {
    let program = args.program || "";
    if (program.endsWith(".rx") || program.endsWith(".rxp")) {
        const compiler = args.compilerPath || "";
        if (!compiler) {
            session.interpreter = args.interpreterPath || "";
            session.source = program;
            return "";
        }
        const directory = fs.mkdtempSync(path.join(os.tmpdir(), "robix-debug-"));
        const destination = path.join(directory, "program.app");
        const code = await runProcess(compiler, [program, destination], args.cwd || path.dirname(program));
        if (code !== 0 || !fs.existsSync(destination)) {
            session.interpreter = args.interpreterPath || "";
            session.source = program;
            return "";
        }
        program = destination;
    }
    session.program = program;
    return program;
}

function runProcess(command, args, cwd) {
    return new Promise((resolve) => {
        const child = spawn(command, args, { cwd });
        child.on("error", () => resolve(1));
        child.on("close", (code) => resolve(code == null ? 1 : code));
    });
}

async function runDirect(args) {
    const program = args.program || session.source || "";
    const cwd = args.cwd || (program ? path.dirname(program) : process.cwd());
    let command = program;
    let commandArgs = args.args || [];
    if (program.endsWith(".rx") || program.endsWith(".rxp")) {
        command = args.interpreterPath || "robix";
        commandArgs = program.endsWith(".rxp") ? ["Run"] : ["run", program, ...(args.args || [])];
    }
    if (args.request === "attach" && args.processId) {
        event("output", { category: "console", output: `Attach needs gdb. Process ${args.processId} was not stopped.\n` });
        event("terminated");
        return;
    }
    session.child = spawn(command, commandArgs, { cwd });
    const write = (category) => (chunk) => event("output", { category, output: chunk.toString() });
    session.child.stdout.on("data", write("stdout"));
    session.child.stderr.on("data", write("stderr"));
    session.child.on("close", (code) => {
        event("exited", { exitCode: code == null ? 1 : code });
        event("terminated");
    });
    session.child.on("error", (error) => {
        event("output", { category: "stderr", output: `${error.message}\n` });
        event("terminated");
    });
}

function startGdb(gdb, program, args) {
    return new Promise((resolve, reject) => {
        const gdbArgs = ["--nx", "--quiet", "--interpreter=mi2"];
        if (args.request === "attach" && args.processId) {
            gdbArgs.push("-p", String(args.processId));
        } else if (args.coreDumpPath) {
            gdbArgs.push(program, args.coreDumpPath);
        } else {
            gdbArgs.push("--args", program, ...(args.args || []));
        }
        session.gdb = spawn(gdb, gdbArgs, { cwd: args.cwd || path.dirname(program) });
        session.gdb.stdout.on("data", (chunk) => onGdb(chunk.toString()));
        session.gdb.stderr.on("data", (chunk) => event("output", { category: "stderr", output: chunk.toString() }));
        session.gdb.on("error", reject);
        session.gdb.on("close", (code) => {
            event("exited", { exitCode: code == null ? 0 : code });
            event("terminated");
        });
        session.ready = resolve;
        if (args.request === "attach" && args.host && args.port) {
            gdbCommand(`-target-select remote ${args.host}:${args.port}`);
        }
    });
}

function onGdb(text) {
    session.stdout += text;
    if (session.ready && /\(gdb\)/.test(session.stdout)) {
        const ready = session.ready;
        session.ready = null;
        ready();
    }
    const stopped = /\*stopped,/.exec(session.stdout);
    if (stopped && !session.stopped) {
        session.stopped = true;
        event("stopped", { reason: "breakpoint", threadId: 1, allThreadsStopped: true });
    }
    if (/\*running,/.test(session.stdout)) {
        session.stopped = false;
    }
    const lines = session.stdout.split("\n");
    session.stdout = lines.pop() || "";
    for (const line of lines) {
        const match = /^(\d+)\^/.exec(line);
        if (match && session.pending.has(Number(match[1]))) {
            const pending = session.pending.get(Number(match[1]));
            session.pending.delete(Number(match[1]));
            pending(`${line}\n${session.stdout}`);
        }
    }
}

function gdbCommand(command) {
    if (!session.gdb) {
        return Promise.resolve("");
    }
    const token = session.token++;
    return new Promise((resolve) => {
        session.pending.set(token, resolve);
        session.gdb.stdin.write(`${token}${command}\n`);
        setTimeout(() => {
            if (session.pending.has(token)) {
                session.pending.delete(token);
                resolve("");
            }
        }, 4000);
    });
}

async function framesOf() {
    if (!session.gdb) {
        return [];
    }
    const text = await gdbCommand("-stack-list-frames");
    const frames = [];
    const pattern = /frame=\{level="(\d+)",addr="([^"]*)",func="([^"]*)"(?:,file="([^"]*)")?(?:,fullname="([^"]*)")?(?:,line="(\d+)")?/g;
    let match = pattern.exec(text);
    while (match) {
        const full = match[5] || match[4] || "";
        if (session.justMyCode && full && !full.endsWith(".rx") && !full.endsWith(".rxp")) {
            match = pattern.exec(text);
            continue;
        }
        frames.push({
            id: Number(match[1]),
            name: match[3],
            source: full ? { path: full, name: path.basename(full) } : undefined,
            line: Number(match[6] || 1),
            column: 1,
            instructionPointerReference: match[2]
        });
        match = pattern.exec(text);
    }
    return frames;
}

async function localsOf() {
    if (!session.gdb) {
        return [];
    }
    const text = await gdbCommand("-stack-list-locals 1");
    const variables = [];
    const pattern = /name="([^"]+)",value="([^"]*)"/g;
    let match = pattern.exec(text);
    while (match) {
        variables.push(present(match[1], match[2].replace(/\\"/g, "\"")));
        match = pattern.exec(text);
    }
    const registers = await gdbCommand("-data-list-register-values x");
    const names = await gdbCommand("-data-list-register-names");
    if (registers && variables.length === 0) {
        variables.push({ name: "registers", value: registers.slice(0, 200), variablesReference: 0 });
        variables.push({ name: "registerNames", value: names.slice(0, 200), variablesReference: 0 });
    }
    return variables;
}

function present(name, value) {
    let shown = value;
    if (value.startsWith("0x") && /option|result|list|array|string/i.test(name)) {
        shown = value;
    }
    return { name, value: shown, variablesReference: 0 };
}

async function evaluate(expression) {
    if (!session.gdb) {
        return "<not stopped>";
    }
    const text = await gdbCommand(`-data-evaluate-expression ${expression}`);
    const found = /value="([^"]*)"/.exec(text);
    return found ? found[1].replace(/\\"/g, "\"") : text.trim();
}

async function stopAll() {
    if (session.gdb) {
        session.gdb.kill();
        session.gdb = null;
    }
    if (session.child) {
        session.child.kill();
        session.child = null;
    }
}

