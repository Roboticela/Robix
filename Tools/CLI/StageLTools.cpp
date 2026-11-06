#include "StageLTools.hpp"
#include "NativeCompilerDriver.hpp"
#include "RobixCLIAndTestGenerator.hpp"
#include "../Debugger/LSPDebuggerProfilerAndBenchmarks.hpp"
#include "../../Compiler/Frontend/ImportResolver.hpp"
#include "../../Compiler/IR/SSAIR.hpp"
#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Optimizer/IRPassManager.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Semantic/SemanticAnalyzer.hpp"
#include "../../Compiler/Types/TypeChecker.hpp"
#include "../../Interpreter/Bytecode/BytecodeModule.hpp"
#include "../../Interpreter/VM/BytecodeVM.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace Robix::Tools {

namespace {

std::string ReadText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool WriteFile(const fs::path& path, const std::string& text) {
    std::error_code ec;
    if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

std::string ArgAfter(const std::vector<std::string>& args, const std::string& flag) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) return args[i + 1];
    }
    return {};
}

bool HasFlag(const std::vector<std::string>& args, const std::string& flag) {
    for (const auto& arg : args) {
        if (arg == flag) return true;
    }
    return false;
}

std::string FirstFile(const std::vector<std::string>& args) {
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i].empty() || args[i][0] == '-') continue;
        if (i > 1 && (args[i - 1] == "-o" || args[i - 1] == "--out")) continue;
        return args[i];
    }
    return {};
}

std::string JsonEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (unsigned char c : text) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    return out;
}

std::string UnquoteJsonString(const std::string& json, std::size_t quote) {
    std::string out;
    for (std::size_t i = quote + 1; i < json.size(); ++i) {
        const char c = json[i];
        if (c == '\\' && i + 1 < json.size()) {
            const char n = json[++i];
            if (n == 'n') out.push_back('\n');
            else if (n == 'r') out.push_back('\r');
            else if (n == 't') out.push_back('\t');
            else if (n == 'u' && i + 4 < json.size()) {
                int code = 0;
                for (int k = 0; k < 4; ++k) {
                    const char hex = json[++i];
                    code <<= 4;
                    if (hex >= '0' && hex <= '9') code += hex - '0';
                    else if (hex >= 'a' && hex <= 'f') code += hex - 'a' + 10;
                    else if (hex >= 'A' && hex <= 'F') code += hex - 'A' + 10;
                }
                if (code >= 0 && code <= 0x7F) out.push_back(static_cast<char>(code));
                else out.push_back('?');
            } else {
                out.push_back(n);
            }
        } else if (c == '"') {
            return out;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string JsonString(const std::string& json, const std::string& key) {
    const std::string pattern = "\"" + key + "\"";
    const auto at = json.find(pattern);
    if (at == std::string::npos) return {};
    const auto colon = json.find(':', at + pattern.size());
    if (colon == std::string::npos) return {};
    const auto quote = json.find('"', colon + 1);
    if (quote == std::string::npos) return {};
    return UnquoteJsonString(json, quote);
}

int JsonInt(const std::string& json, const std::string& key, bool* found) {
    const std::string pattern = "\"" + key + "\"";
    const auto at = json.find(pattern);
    if (at == std::string::npos) {
        if (found) *found = false;
        return 0;
    }
    const auto colon = json.find(':', at + pattern.size());
    if (colon == std::string::npos) {
        if (found) *found = false;
        return 0;
    }
    std::size_t i = colon + 1;
    while (i < json.size() && (json[i] == ' ' || json[i] == '\t')) ++i;
    if (i < json.size() && json[i] == 'n') {
        if (found) *found = false;
        return 0;
    }
    if (found) *found = true;
    return std::atoi(json.c_str() + i);
}

void WriteLsp(const std::string& json) {
    std::cout << "Content-Length: " << json.size() << "\r\n\r\n" << json;
    std::cout.flush();
}

std::string ReadLsp() {
    std::string header;
    char c = 0;
    while (std::cin.get(c)) {
        header.push_back(c);
        if (header.size() >= 4 && header.compare(header.size() - 4, 4, "\r\n\r\n") == 0) break;
    }
    if (header.empty()) return {};
    const auto mark = header.find("Content-Length:");
    if (mark == std::string::npos) return {};
    const int length = std::atoi(header.c_str() + mark + 15);
    if (length <= 0) return {};
    std::string body(static_cast<std::size_t>(length), '\0');
    std::cin.read(body.data(), length);
    if (std::cin.gcount() != length) return {};
    return body;
}

std::string WordAt(const std::string& text, int line, int character) {
    int current = 0;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            if (current == line) {
                const std::string row = text.substr(start, i - start);
                if (character < 0 || static_cast<std::size_t>(character) >= row.size()) return {};
                std::size_t left = static_cast<std::size_t>(character);
                std::size_t right = left;
                const auto ident = [](unsigned char ch) {
                    return std::isalnum(ch) || ch == '_';
                };
                if (!ident(static_cast<unsigned char>(row[left]))) return {};
                while (left > 0 && ident(static_cast<unsigned char>(row[left - 1]))) --left;
                while (right < row.size() && ident(static_cast<unsigned char>(row[right]))) ++right;
                return row.substr(left, right - left);
            }
            ++current;
            start = i + 1;
        }
    }
    return {};
}

void AppendExplain(std::ostream& out, const Frontend::DeclNode& decl) {
    if ((decl.kind == Frontend::DeclKind::Function || decl.kind == Frontend::DeclKind::Class ||
         decl.kind == Frontend::DeclKind::Struct) &&
        decl.explainBlock && decl.explainBlock->present) {
        out << "## " << decl.name << "\n\n";
        if (!decl.explainBlock->description.empty()) {
            out << "Description: " << decl.explainBlock->description << "\n\n";
        }
        for (const auto& input : decl.explainBlock->inputs) {
            out << "Input " << input.first << ": " << input.second << "\n\n";
        }
        if (!decl.explainBlock->processing.empty()) {
            out << "Processing: " << decl.explainBlock->processing << "\n\n";
        }
        if (!decl.explainBlock->output.empty()) {
            out << "Output: " << decl.explainBlock->output << "\n\n";
        }
    }
    for (const auto& member : decl.members) {
        if (member) AppendExplain(out, *member);
    }
}

std::vector<std::uint32_t> InterpretLines(const std::string& fileName, const std::string& source, std::string& error) {
    std::vector<std::uint32_t> lines;
    Frontend::DiagnosticBag diags;
    Frontend::Lexer lexer(source, fileName, diags);
    Frontend::Parser parser(lexer.TokenizeAll(), fileName, diags);
    auto ast = parser.ParseCompilationUnit();
    Frontend::ResolveImports(ast, fileName, diags);
    Frontend::TypeCheckerOptions tcOpts;
    Frontend::TypeChecker tc(diags, tcOpts);
    tc.CheckCompilationUnit(ast);
    Frontend::SemanticAnalyzer sem(diags);
    sem.AnalyzeCompilationUnit(ast);
    if (diags.HasErrors()) {
        error = diags.FormatHumanReadable();
        return lines;
    }
    IR::SSAIRBuilder irBuilder(sem, diags);
    auto irMod = irBuilder.LowerCompilationUnit(ast);
    IR::IRPassManager passMgr(sem, diags);
    passMgr.RunOptimizationPipeline(irMod, true);
    if (diags.HasErrors()) {
        error = diags.FormatHumanReadable();
        return lines;
    }
    VM::BytecodeGenerator bcGen(diags);
    auto bcMod = bcGen.GenerateFromIR(irMod);
    VM::BytecodeVM::SetLineTrace(&lines);
    VM::BytecodeVM vm(bcMod);
    const int exitCode = vm.ExecuteMainAndGetExitCode();
    VM::BytecodeVM::SetLineTrace(nullptr);
    if (exitCode != 0 && lines.empty()) {
        error = "interpreter exit " + std::to_string(exitCode);
    }
    return lines;
}

#ifdef _WIN32
std::string CaptureExe(const std::string& exe) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return {};
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    PROCESS_INFORMATION pi{};
    std::string command = "\"" + exe + "\"";
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');
    if (!CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return {};
    }
    CloseHandle(writePipe);
    std::string output;
    char buffer[512];
    DWORD read = 0;
    while (::ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
        output.append(buffer, buffer + read);
    }
    WaitForSingleObject(pi.hProcess, 15000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(readPipe);
    return output;
}
#endif

std::vector<std::uint32_t> NativeLines(const std::string& text) {
    std::vector<std::uint32_t> lines;
    std::size_t at = 0;
    while (at < text.size()) {
        const auto nl = text.find('\n', at);
        const auto row = text.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
        at = nl == std::string::npos ? text.size() : nl + 1;
        if (row.rfind("native ", 0) != 0) continue;
        lines.push_back(static_cast<std::uint32_t>(std::atoi(row.c_str() + 7)));
    }
    return lines;
}

int FormatCommand(const std::vector<std::string>& args) {
    const std::string file = FirstFile(args);
    if (file.empty()) {
        std::cerr << "robix format: expected a .rbx file.\n";
        return 1;
    }
    const std::string source = ReadText(file);
    if (source.empty() && !fs::exists(file)) {
        std::cerr << "robix format: could not open " << file << "\n";
        return 1;
    }
    const std::string formatted = RobixFormatterAndLinter::FormatSource(source);
    if (HasFlag(args, "--check")) {
        if (formatted != source) {
            std::cout << "format: needs changes " << file << "\n";
            return 1;
        }
        std::cout << "format: already formatted " << file << "\n";
        return 0;
    }
    if (!WriteFile(file, formatted)) {
        std::cerr << "robix format: could not write " << file << "\n";
        return 1;
    }
    std::cout << "format: wrote " << file << "\n";
    return 0;
}

int LintCommand(const std::vector<std::string>& args) {
    const std::string file = FirstFile(args);
    if (file.empty()) {
        std::cerr << "robix lint: expected a .rbx file.\n";
        return 1;
    }
    const std::string source = ReadText(file);
    if (!fs::exists(file)) {
        std::cerr << "robix lint: could not open " << file << "\n";
        return 1;
    }
    Frontend::DiagnosticBag diags;
    const bool strict = HasFlag(args, "--strict");
    const bool ok = RobixFormatterAndLinter::LintSource(file, source, strict, diags);
    const std::string text = diags.FormatHumanReadable();
    if (!text.empty()) std::cerr << text;
    std::cout << (ok ? "lint: passed " : "lint: failed ") << file << "\n";
    return ok ? 0 : 1;
}

int CheckCommand(const std::vector<std::string>& args) {
    const std::string file = FirstFile(args);
    if (file.empty()) {
        std::cerr << "robix check: expected a .rbx file.\n";
        return 1;
    }
    if (!fs::exists(file)) {
        std::cerr << "robix check: could not open " << file << "\n";
        return 1;
    }
    Frontend::DiagnosticBag diags;
    const bool ok = RobixFormatterAndLinter::LintSource(file, ReadText(file), false, diags);
    const std::string text = diags.FormatHumanReadable();
    if (!text.empty()) std::cerr << text;
    std::cout << (ok ? "check: passed " : "check: failed ") << file << "\n";
    return ok ? 0 : 1;
}

int DocCommand(const std::vector<std::string>& args) {
    const std::string file = FirstFile(args);
    if (file.empty() || !fs::exists(file)) {
        std::cerr << "robix doc: expected a .rbx file.\n";
        return 1;
    }
    const std::string source = ReadText(file);
    Frontend::DiagnosticBag diags;
    Frontend::Lexer lexer(source, file, diags);
    Frontend::Parser parser(lexer.TokenizeAll(), file, diags);
    const auto ast = parser.ParseCompilationUnit();
    if (diags.HasErrors()) {
        std::cerr << diags.FormatHumanReadable();
        return 1;
    }
    std::ostringstream md;
    md << "# " << fs::path(file).stem().string() << "\n\n";
    bool any = false;
    for (const auto& decl : ast.declarations) {
        if (!decl) continue;
        const auto before = md.tellp();
        AppendExplain(md, *decl);
        if (md.tellp() != before) any = true;
    }
    if (!any) {
        std::cerr << "robix doc: no Explain blocks in " << file << "\n";
        return 1;
    }
    std::string outPath = ArgAfter(args, "-o");
    if (outPath.empty()) {
        outPath = (fs::path("Build/StageL") / (fs::path(file).stem().string() + ".md")).string();
    }
    if (!WriteFile(outPath, md.str())) {
        std::cerr << "robix doc: could not write " << outPath << "\n";
        return 1;
    }
    std::cout << "doc: wrote " << outPath << "\n";
    std::cout << md.str();
    return 0;
}

int DebugCommand(const std::vector<std::string>& args, const std::string& toolDir) {
    const std::string file = FirstFile(args);
    if (file.empty() || !fs::exists(file)) {
        std::cerr << "robix debug: expected a .rbx file.\n";
        return 1;
    }
    const std::string source = ReadText(file);
    std::string error;
    const auto interpreted = InterpretLines(file, source, error);
    if (!error.empty()) {
        std::cerr << error;
        return 1;
    }
    NativeBuildOptions opts;
    opts.toolDirectory = toolDir;
    opts.release = false;
    opts.debugLines = true;
    opts.keepLLVMIR = true;
    opts.outputPath = "Build/StageL/debug.exe";
    auto built = NativeCompilerDriver::CompileSource(file, source, opts);
    if (!built.diagnosticsText.empty()) std::cerr << built.diagnosticsText;
    if (!built.toolOutput.empty()) std::cout << built.toolOutput;
    if (!built.success) {
        std::cerr << "robix debug: native compilation failed.\n";
        return 1;
    }
#ifndef _WIN32
    std::cerr << "robix debug: native capture is implemented for Windows.\n";
    return 1;
#else
    std::string exe = built.executablePath;
    for (char& ch : exe) {
        if (ch == '/') ch = '\\';
    }
    const std::string captured = CaptureExe(exe);
    const auto native = NativeLines(captured);
    std::cout << "interpreter:";
    for (const auto line : interpreted) std::cout << " " << line;
    std::cout << "\nnative:";
    for (const auto line : native) std::cout << " " << line;
    std::cout << "\n";
    if (interpreted.empty() || interpreted != native) {
        std::cerr << "robix debug: source lines differ.\n";
        return 1;
    }
    std::cout << "debug: same source lines\n";
    return 0;
#endif
}

int LspCommand() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    RobixLanguageServer server;
    std::string document;
    for (;;) {
        const std::string body = ReadLsp();
        if (body.empty()) return 0;
        const std::string method = JsonString(body, "method");
        bool hasId = false;
        const int id = JsonInt(body, "id", &hasId);
        if (method == "initialize") {
            WriteLsp("{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) +
                     ",\"result\":{\"capabilities\":{\"hoverProvider\":true,\"textDocumentSync\":1}}}");
        } else if (method == "shutdown") {
            WriteLsp("{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":null}");
        } else if (method == "exit") {
            return 0;
        } else if (method == "textDocument/didOpen") {
            document = JsonString(body, "text");
            server.IndexDocument(JsonString(body, "uri"), document);
        } else if (method == "textDocument/hover") {
            bool hasLine = false;
            bool hasChar = false;
            const int line = JsonInt(body, "line", &hasLine);
            const int character = JsonInt(body, "character", &hasChar);
            const std::string word = WordAt(document, line, character);
            const auto hover = server.GetHoverDocumentation(word);
            if (!hover) {
                WriteLsp("{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":null}");
            } else {
                WriteLsp("{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) +
                         ",\"result\":{\"contents\":{\"kind\":\"markdown\",\"value\":\"" + JsonEscape(*hover) +
                         "\"}}}");
            }
        } else if (hasId) {
            WriteLsp("{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) +
                     ",\"error\":{\"code\":-32601,\"message\":\"method not found\"}}");
        }
        (void)hasId;
    }
}

} // namespace

int RunStageLCommand(const std::vector<std::string>& args, const std::string& toolDir) {
    if (args.empty()) return -1;
    const std::string& cmd = args[0];
    if (cmd == "format" || cmd == "fmt") return FormatCommand(args);
    if (cmd == "lint") return LintCommand(args);
    if (cmd == "check") return CheckCommand(args);
    if (cmd == "doc") return DocCommand(args);
    if (cmd == "debug") return DebugCommand(args, toolDir);
    if (cmd == "lsp") return LspCommand();
    if (cmd == "repl") {
        std::string body;
        std::string line;
        int status = 0;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line != "run") {
                body += line + "\n";
                continue;
            }
            const std::string source = "package Repl;\n\npublic Function Main() -> Void {\n" + body + "}\n";
            const fs::path file = "Build/StageM/Repl.rbx";
            if (!WriteFile(file, source)) {
                std::cerr << "robix repl: could not write the input.\n";
                return 1;
            }
            const auto result = RobixCLIDispatcher::ExecuteCommand(
                CLIBinaryKind::RobixUnifiedCLI, { "run", file.string() });
            if (!result.stdoutText.empty()) std::cout << result.stdoutText;
            if (!result.stderrText.empty()) std::cerr << result.stderrText;
            status = result.exitCode;
            body.clear();
        }
        return status;
    }
    return -1;
}

} // namespace Robix::Tools
