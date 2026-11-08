#include "WebLanguage.hpp"

#include "../../Compiler/Frontend/ImportResolver.hpp"
#include "../../Compiler/IR/SSAIR.hpp"
#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Optimizer/IRPassManager.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Semantic/SemanticAnalyzer.hpp"
#include "../../Compiler/Types/TypeChecker.hpp"
#include "../../Interpreter/Bytecode/BytecodeModule.hpp"
#include "../../Tools/CLI/NativeCompilerDriver.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace Robix::UI {
namespace {

std::string Trim(std::string text) {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) text.pop_back();
    std::size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) ++start;
    return text.substr(start);
}

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

fs::path ExecutablePath() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) return fs::path(buffer);
#endif
    return {};
}

fs::path FindUp(const fs::path& name) {
    auto walk = [&](fs::path dir) {
        for (int i = 0; i < 8 && !dir.empty(); ++i) {
            const fs::path candidate = dir / name;
            std::error_code error;
            if (fs::exists(candidate, error)) return candidate;
            if (!dir.has_parent_path() || dir.parent_path() == dir) break;
            dir = dir.parent_path();
        }
        return fs::path{};
    };
    std::error_code error;
    fs::path found = walk(fs::current_path(error));
    if (!found.empty()) return found;
    const fs::path exe = ExecutablePath();
    if (!exe.empty()) found = walk(exe.parent_path());
    return found;
}

std::string ReadText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string JsString(const std::string& text) {
    std::string out = "\"";
    for (unsigned char ch : text) {
        if (ch == '\\' || ch == '"') { out.push_back('\\'); out.push_back(static_cast<char>(ch)); }
        else if (ch == '\n') out += "\\n";
        else if (ch == '\r') out += "\\r";
        else if (ch == '\t') out += "\\t";
        else if (ch < 32 || ch > 126) {
            constexpr char hex[] = "0123456789ABCDEF";
            out += "\\u00";
            out.push_back(hex[ch >> 4]);
            out.push_back(hex[ch & 15]);
        } else out.push_back(static_cast<char>(ch));
    }
    out.push_back('"');
    return out;
}

std::string CString(const std::string& text) {
    std::string out = "\"";
    for (unsigned char ch : text) {
        if (ch == '\\' || ch == '"') { out.push_back('\\'); out.push_back(static_cast<char>(ch)); }
        else if (ch == '\n') out += "\\n";
        else if (ch == '\r') out += "\\r";
        else if (ch == '\t') out += "\\t";
        else if (ch < 32 || ch > 126) {
            constexpr char hex[] = "0123456789ABCDEF";
            out += "\\x";
            out.push_back(hex[ch >> 4]);
            out.push_back(hex[ch & 15]);
        } else out.push_back(static_cast<char>(ch));
    }
    out.push_back('"');
    return out;
}

bool StartsWith(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

bool BlockedOnWeb(const VM::BCInstruction& inst, std::string& feature) {
    if (inst.opcode == VM::BCOpcode::InlineASM) {
        const std::string mnemonic = Lower(inst.symbolImm);
        if (mnemonic.empty() || mnemonic == "nop") return false;
        feature = "InlineASM " + inst.symbolImm;
        return true;
    }
    if (inst.opcode != VM::BCOpcode::CallOSIntrinsic) return false;
    const std::string& name = inst.symbolImm;
    if (StartsWith(name, "Port.") || StartsWith(name, "Cpu.") || StartsWith(name, "Timer.") ||
        StartsWith(name, "System.") || StartsWith(name, "Console.") || StartsWith(name, "GPIO.") ||
        StartsWith(name, "PWM.") || StartsWith(name, "UART.") || StartsWith(name, "SPI.") ||
        StartsWith(name, "I2C.") || StartsWith(name, "CAN.") || StartsWith(name, "USB.") ||
        StartsWith(name, "PCIe.") || StartsWith(name, "DMA.") || StartsWith(name, "Sensor.") ||
        StartsWith(name, "Motor.") || StartsWith(name, "Interrupt.") || StartsWith(name, "HardwareTimer.") ||
        name == "Memory.Pages" || name == "Memory.Address" || name == "Memory.At" ||
        name == "Network.Call" || name == "Network.SendText") {
        feature = name;
        return true;
    }
    return false;
}

struct Loaded {
    VM::BytecodeModule module;
    std::string error;
};

Loaded LoadProgram(const fs::path& file) {
    Loaded loaded;
    const std::string source = ReadText(file);
    if (source.empty() && !fs::is_regular_file(file)) {
        loaded.error = "robix: could not read " + file.string() + "\n";
        return loaded;
    }
    Frontend::DiagnosticBag diags;
    Frontend::Lexer lexer(source, file.string(), diags);
    Frontend::Parser parser(lexer.TokenizeAll(), file.string(), diags);
    auto ast = parser.ParseCompilationUnit();
    Frontend::ResolveImports(ast, file.string(), diags);
    Frontend::TypeChecker checker(diags);
    checker.CheckCompilationUnit(ast);
    Frontend::SemanticAnalyzer semantic(diags);
    semantic.AnalyzeCompilationUnit(ast);
    if (diags.HasErrors()) {
        loaded.error = diags.FormatHumanReadable();
        return loaded;
    }
    IR::SSAIRBuilder ir(semantic, diags);
    auto irModule = ir.LowerCompilationUnit(ast);
    IR::IRPassManager passes(semantic, diags);
    passes.RunOptimizationPipeline(irModule, true);
    if (diags.HasErrors()) {
        loaded.error = diags.FormatHumanReadable();
        return loaded;
    }
    VM::BytecodeGenerator generator(diags);
    loaded.module = generator.GenerateFromIR(irModule);
    if (diags.HasErrors()) loaded.error = diags.FormatHumanReadable();
    return loaded;
}

void SkipBlocked(VM::BytecodeModule& module, std::vector<std::string>& warnings, bool assembly) {
    std::unordered_set<std::string> seen;
    for (VM::BytecodeChunk& chunk : module.chunks) {
        for (VM::BCInstruction& inst : chunk.code) {
            std::string feature;
            if (BlockedOnWeb(inst, feature)) {
                if (seen.insert(feature).second) {
                    warnings.push_back("warning W7001: web: " + feature + " cannot run in a browser and was skipped.");
                }
                if (inst.destReg != 0) {
                    inst.opcode = VM::BCOpcode::LoadConstInt;
                    inst.intImm = 0;
                } else {
                    inst.opcode = VM::BCOpcode::Nop;
                }
                inst.callArgRegs.clear();
            }
            if (!assembly) continue;
            const auto wide = [&](std::uint32_t id) { return id >= 1024; };
            bool wideReg = wide(inst.destReg) || wide(inst.srcReg1) || wide(inst.srcReg2);
            for (std::uint32_t id : inst.callArgRegs) wideReg = wideReg || wide(id);
            if (wideReg && seen.insert("register file").second) {
                warnings.push_back("warning W7001: web: a value outside the WebAssembly register file was skipped.");
            }
            if (wideReg) inst.opcode = VM::BCOpcode::Nop;
            if (inst.callArgRegs.size() > 8 && seen.insert("call arguments").second) {
                warnings.push_back("warning W7001: web: call arguments after the first 8 were skipped in the WebAssembly form.");
            }
        }
    }
}

class StringTable {
public:
    int Add(const std::string& text) {
        const auto found = index.find(text);
        if (found != index.end()) return found->second;
        const int id = static_cast<int>(values.size());
        values.push_back(text);
        index.emplace(text, id);
        return id;
    }
    std::vector<std::string> values;
    std::unordered_map<std::string, int> index;
};

std::string EmitScript(const VM::BytecodeModule& module, const std::vector<std::string>& warnings, const std::string& runtime) {
    std::ostringstream out;
    out << "window.Robo = window.Robo || {};\nRobo.module = {warnings:[";
    for (std::size_t i = 0; i < warnings.size(); ++i) {
        if (i) out << ",";
        out << JsString(warnings[i]);
    }
    out << "],chunks:[";
    for (std::size_t c = 0; c < module.chunks.size(); ++c) {
        const VM::BytecodeChunk& chunk = module.chunks[c];
        if (c) out << ",";
        out << "{name:" << JsString(chunk.name) << ",ret:" << JsString(chunk.returnTypeName) << ",test:"
            << (chunk.isTestChunk ? "true" : "false") << ",params:[";
        for (std::size_t p = 0; p < chunk.paramNames.size(); ++p) {
            if (p) out << ",";
            out << JsString(chunk.paramNames[p]);
        }
        out << "],code:[";
        for (std::size_t n = 0; n < chunk.code.size(); ++n) {
            const VM::BCInstruction& inst = chunk.code[n];
            if (n) out << ",";
            out << "{op:" << static_cast<int>(inst.opcode) << ",d:" << inst.destReg << ",a:" << inst.srcReg1
                << ",b:" << inst.srcReg2 << ",pc:" << inst.targetPC << ",i:\"" << inst.intImm << "\",f:" << inst.floatImm
                << ",bo:" << (inst.boolImm ? "true" : "false") << ",s:" << JsString(inst.symbolImm) << ",args:[";
            for (std::size_t a = 0; a < inst.callArgRegs.size(); ++a) {
                if (a) out << ",";
                out << inst.callArgRegs[a];
            }
            out << "]}";
        }
        out << "]}";
    }
    out << "]};\n" << runtime;
    return out.str();
}

std::string AssemblyLoader() {
    return R"JS(window.Robo = window.Robo || {};
Robo.status = 0;
Robo.log = "";
(function () {
  var previous = Robo.mount;
  var memory = null;
  var instance = null;
  function decode(ptr, len) {
    if (!memory || len <= 0) return "";
    return new TextDecoder().decode(new Uint8Array(memory.buffer, ptr, len));
  }
  function show(text) {
    Robo.log += text + "\n";
    console.log(text);
    var log = document.getElementById("robo-log");
    if (!log) {
      log = document.createElement("pre");
      log.id = "robo-log";
      log.style.cssText = "margin:16px;white-space:pre-wrap;font:16px/1.4 var(--font-sans,system-ui,sans-serif)";
      var host = document.querySelector(".robo-page") || document.body;
      if (host) host.appendChild(log);
    }
    log.textContent = Robo.log;
  }
  Robo.mount = function () {
    if (previous) previous();
    fetch("app.wasm").then(function (response) { return response.arrayBuffer(); }).then(function (bytes) {
      return WebAssembly.instantiate(bytes, { env: {
        rbx_print: function (ptr, len) { show(decode(ptr, len)); },
        rbx_warn: function (ptr, len) { console.warn(decode(ptr, len)); },
        rbx_input: function (promptPtr, promptLen, out, cap) {
          var answer = window.prompt(decode(promptPtr, promptLen) || "") || "";
          var encoded = new TextEncoder().encode(answer);
          var n = Math.min(encoded.length, cap);
          new Uint8Array(memory.buffer, out, n).set(encoded.subarray(0, n));
          return n;
        }
      }});
    }).then(function (ready) {
      instance = ready.instance;
      memory = instance.exports.memory;
      document.querySelectorAll("[data-click]").forEach(function (node) {
        if (node.getAttribute("data-robix-bound") === "true") return;
        node.setAttribute("data-robix-bound", "true");
        node.addEventListener("click", function (event) {
          event.preventDefault();
          var name = node.getAttribute("data-click") || "";
          var ptr = instance.exports.rbx_name();
          var encoded = new TextEncoder().encode(name);
          new Uint8Array(memory.buffer, ptr, encoded.length).set(encoded);
          instance.exports.rbx_call(encoded.length);
        });
      });
      if (instance.exports.start) instance.exports.start();
    }).catch(function (error) { console.warn("warning W7001: web: " + error); });
  };
})();
)JS";
}

bool CompileAssembly(const VM::BytecodeModule& module, const std::vector<std::string>& warnings,
                     const std::string& toolDirectory, std::vector<std::uint8_t>& wasm, std::string& error) {
    const fs::path runtimePath = FindUp("RobixUI/Web/WebWasmRuntime.c");
    const std::string runtime = ReadText(runtimePath);
    if (runtime.empty()) {
        error = "the WebAssembly runtime was not found.\n";
        return false;
    }
    const std::string clang = Tools::NativeCompilerDriver::LocateClang("", toolDirectory);
    if (clang.empty()) {
        error = "assembly form needs clang with a wasm32 target.\n";
        return false;
    }
    StringTable strings;
    const int empty = strings.Add("");
    (void)empty;
    std::ostringstream data;
    data << "typedef struct { int op, dest, a, b, target, bo, str, argc; long long imm; double fimm; int args[8]; } Ins;\n";
    data << "typedef struct { int name, nparam, begin, count, test; int params[8]; } Chunk;\n";
    struct Flat {
        VM::BCInstruction inst;
        int str = 0;
    };
    std::vector<Flat> instructions;
    struct ChunkRow {
        int name = 0;
        int nparam = 0;
        int begin = 0;
        int count = 0;
        int test = 0;
        int params[8]{};
    };
    std::vector<ChunkRow> rows;
    for (const VM::BytecodeChunk& chunk : module.chunks) {
        ChunkRow row;
        row.name = strings.Add(chunk.name);
        row.nparam = static_cast<int>(chunk.paramNames.size() > 8 ? 8 : chunk.paramNames.size());
        for (int i = 0; i < row.nparam; ++i) row.params[i] = strings.Add(chunk.paramNames[static_cast<std::size_t>(i)]);
        row.begin = static_cast<int>(instructions.size());
        row.test = chunk.isTestChunk ? 1 : 0;
        for (const VM::BCInstruction& inst : chunk.code) {
            Flat flat;
            flat.inst = inst;
            flat.str = strings.Add(inst.symbolImm);
            if (flat.inst.callArgRegs.size() > 8) flat.inst.callArgRegs.resize(8);
            instructions.push_back(flat);
        }
        row.count = static_cast<int>(instructions.size()) - row.begin;
        rows.push_back(row);
    }
    std::vector<int> warningIds;
    for (const std::string& warning : warnings) warningIds.push_back(strings.Add(warning));
    if (instructions.empty()) {
        Flat nop;
        nop.str = strings.Add("");
        instructions.push_back(nop);
    }
    data << "static const char* STR[] = {";
    for (std::size_t i = 0; i < strings.values.size(); ++i) {
        if (i) data << ",";
        data << CString(strings.values[i]);
    }
    data << "};\nstatic const int SLEN[] = {";
    for (std::size_t i = 0; i < strings.values.size(); ++i) {
        if (i) data << ",";
        data << strings.values[i].size();
    }
    data << "};\nstatic const int STR_COUNT = " << strings.values.size() << ";\n";
    data << "static const Ins INS[] = {";
    for (std::size_t i = 0; i < instructions.size(); ++i) {
        const Flat& flat = instructions[i];
        if (i) data << ",";
        data << "{" << static_cast<int>(flat.inst.opcode) << "," << flat.inst.destReg << "," << flat.inst.srcReg1 << ","
             << flat.inst.srcReg2 << "," << flat.inst.targetPC << "," << (flat.inst.boolImm ? 1 : 0) << "," << flat.str << ","
             << flat.inst.callArgRegs.size() << "," << flat.inst.intImm << "," << flat.inst.floatImm << ",{";
        for (int a = 0; a < 8; ++a) {
            if (a) data << ",";
            data << (a < static_cast<int>(flat.inst.callArgRegs.size()) ? flat.inst.callArgRegs[static_cast<std::size_t>(a)] : 0);
        }
        data << "}}";
    }
    data << "};\nstatic const int INS_COUNT = " << instructions.size() << ";\n";
    if (rows.empty()) {
        data << "static const Chunk CHUNKS[1] = {{0,0,0,0,0,{0,0,0,0,0,0,0,0}}};\nstatic const int CHUNK_COUNT = 0;\n";
    } else {
        data << "static const Chunk CHUNKS[] = {";
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const ChunkRow& row = rows[i];
            if (i) data << ",";
            data << "{" << row.name << "," << row.nparam << "," << row.begin << "," << row.count << "," << row.test << ",{";
            for (int p = 0; p < 8; ++p) {
                if (p) data << ",";
                data << row.params[p];
            }
            data << "}}";
        }
        data << "};\nstatic const int CHUNK_COUNT = " << rows.size() << ";\n";
    }
    if (warningIds.empty()) data << "static const int WARN[1] = {0};\nstatic const int WARN_COUNT = 0;\n";
    else {
        data << "static const int WARN[] = {";
        for (std::size_t i = 0; i < warningIds.size(); ++i) {
            if (i) data << ",";
            data << warningIds[i];
        }
        data << "};\nstatic const int WARN_COUNT = " << warningIds.size() << ";\n";
    }
    const fs::path tool(toolDirectory);
    const fs::path cache = (Lower(tool.filename().string()) == "bin" ? tool.parent_path() : tool) / "Cache" / "Wasm";
    std::error_code mkdirError;
    fs::create_directories(cache, mkdirError);
    const fs::path source = cache / "program.c";
    const fs::path binary = cache / "program.wasm";
    {
        std::ofstream out(source, std::ios::binary);
        out << data.str() << runtime;
        if (!out) {
            error = "could not write the WebAssembly source.\n";
            return false;
        }
    }
    const std::string command = clang + " --target=wasm32-unknown-unknown -O2 -nostdlib -x c "
                                "-Wl,--no-entry -Wl,--export=start -Wl,--export=rbx_call "
                                "-Wl,--export=rbx_name -Wl,--export=memory -Wl,--allow-undefined " +
                                source.string() + " -o " + binary.string();
    const int status = std::system(command.c_str());
    if (status != 0 || !fs::is_regular_file(binary)) {
        error = "WebAssembly compilation failed.\n";
        return false;
    }
    std::ifstream in(binary, std::ios::binary);
    wasm.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (wasm.size() < 8 || wasm[0] != 0 || wasm[1] != 'a' || wasm[2] != 's' || wasm[3] != 'm') {
        error = "WebAssembly compilation did not write a module.\n";
        wasm.clear();
        return false;
    }
    return true;
}

}  // namespace

WebForm ParseWebForm(const std::string& text, bool& recognized) {
    const std::string key = Lower(text);
    recognized = true;
    if (key == "script" || key == "js" || key == "html" || key == "javascript") return WebForm::Script;
    if (key == "assembly" || key == "wasm" || key == "webassembly") return WebForm::Assembly;
    recognized = false;
    return WebForm::Script;
}

const char* WebFormName(WebForm form) {
    return form == WebForm::Assembly ? "assembly" : "script";
}

WebBuildResult BuildWebProgram(const fs::path& source, WebForm form, const std::string& toolDirectory) {
    WebBuildResult result;
    Loaded loaded = LoadProgram(source);
    if (!loaded.error.empty()) {
        result.error = loaded.error;
        return result;
    }
    std::vector<std::string> warnings;
    SkipBlocked(loaded.module, warnings, form == WebForm::Assembly);
    for (const std::string& warning : warnings) std::cerr << warning << "\n";
    if (form == WebForm::Assembly) {
        if (!CompileAssembly(loaded.module, warnings, toolDirectory, result.wasm, result.error)) return result;
        result.script = AssemblyLoader();
        result.ok = true;
        return result;
    }
    const std::string runtime = ReadText(FindUp("RobixUI/Web/WebRuntime.js"));
    if (runtime.empty()) {
        result.error = "robix: the JavaScript runtime was not found.\n";
        return result;
    }
    result.script = EmitScript(loaded.module, warnings, runtime);
    result.ok = true;
    return result;
}

fs::path FindWebSource(const fs::path& document) {
    std::error_code error;
    fs::path dir = fs::absolute(document, error);
    if (fs::is_regular_file(dir, error)) dir = dir.parent_path();
    for (int step = 0; step < 5 && !dir.empty(); ++step) {
        for (const auto& entry : fs::directory_iterator(dir, error)) {
            if (entry.path().extension() != ".rbxb") continue;
            std::ifstream in(entry.path());
            std::string line;
            while (std::getline(in, line)) {
                line = Trim(line);
                if (line.rfind("Source:", 0) != 0) continue;
                fs::path source(Trim(line.substr(7)));
                if (source.is_relative()) source = dir / source;
                if (fs::is_regular_file(source, error)) return source;
            }
        }
        if (fs::is_regular_file(dir / "Source" / "Program.rbx", error)) return dir / "Source" / "Program.rbx";
        if (fs::is_regular_file(dir / "Program.rbx", error)) return dir / "Program.rbx";
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    return {};
}

}  // namespace Robix::UI
