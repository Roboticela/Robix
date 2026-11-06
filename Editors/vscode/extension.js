const vscode = require("vscode");
const { spawn } = require("child_process");

function encode(message) {
    const body = JSON.stringify(message);
    return `Content-Length: ${Buffer.byteLength(body, "utf8")}\r\n\r\n${body}`;
}

function activate(context) {
    const server = spawn("robix", ["lsp"], { stdio: ["pipe", "pipe", "pipe"] });
    let buffer = Buffer.alloc(0);
    let nextId = 1;
    const pending = new Map();

    server.stdout.on("data", (chunk) => {
        buffer = Buffer.concat([buffer, chunk]);
        for (;;) {
            const headerEnd = buffer.indexOf("\r\n\r\n");
            if (headerEnd < 0) return;
            const header = buffer.slice(0, headerEnd).toString("utf8");
            const match = header.match(/Content-Length:\s*(\d+)/i);
            if (!match) return;
            const length = Number(match[1]);
            const start = headerEnd + 4;
            if (buffer.length < start + length) return;
            const body = JSON.parse(buffer.slice(start, start + length).toString("utf8"));
            buffer = buffer.slice(start + length);
            if (body.id != null && pending.has(body.id)) {
                pending.get(body.id)(body);
                pending.delete(body.id);
            }
        }
    });

    function request(message) {
        const id = nextId++;
        message.jsonrpc = "2.0";
        message.id = id;
        return new Promise((resolve) => {
            pending.set(id, resolve);
            server.stdin.write(encode(message));
        });
    }

    server.stdin.write(encode({ jsonrpc: "2.0", id: nextId++, method: "initialize", params: {} }));

    const hover = vscode.languages.registerHoverProvider("robix", {
        async provideHover(document, position) {
            const text = document.getText();
            server.stdin.write(encode({
                jsonrpc: "2.0",
                method: "textDocument/didOpen",
                params: {
                    textDocument: {
                        uri: document.uri.toString(),
                        languageId: "robix",
                        version: document.version,
                        text
                    }
                }
            }));
            const response = await request({
                method: "textDocument/hover",
                params: {
                    textDocument: { uri: document.uri.toString() },
                    position: { line: position.line, character: position.character }
                }
            });
            const value = response.result && response.result.contents && response.result.contents.value;
            if (!value) return null;
            return new vscode.Hover(new vscode.MarkdownString(value));
        }
    });
    context.subscriptions.push(hover, { dispose() { server.kill(); } });
}

function deactivate() {}

module.exports = { activate, deactivate };
