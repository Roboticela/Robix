#include "NativeCompilerDriver.hpp"

#include "../../Compiler/CodeGen/NativeBackend.hpp"
#include "../../Compiler/IR/SSAIR.hpp"
#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Optimizer/IRPassManager.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Semantic/SemanticAnalyzer.hpp"
#include "../../Compiler/Types/TypeChecker.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace Robix::Tools {

namespace {

bool FileExists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec) && fs::is_regular_file(p, ec);
}

std::string ReadFirstLine(const fs::path& p) {
    std::ifstream in(p);
    std::string line;
    if (in && std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }
    }
    return line;
}

std::string ReadAll(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Runs a command line through the shell, capturing stdout+stderr. Returns the exit code.
int RunProcess(const std::string& commandLine, std::string& capturedOutput) {
    const fs::path logPath = fs::temp_directory_path() / ("robixc_tool_" + std::to_string(std::rand()) + ".log");
    // cmd.exe strips the outermost quotes; wrap the whole command in an extra pair.
    const std::string full = "\"" + commandLine + " > \"" + logPath.string() + "\" 2>&1\"";
    const int code = std::system(full.c_str());
    capturedOutput = FileExists(logPath) ? ReadAll(logPath) : std::string{};
    std::error_code ec;
    fs::remove(logPath, ec);
    return code;
}

std::string Quote(const std::string& s) { return "\"" + s + "\""; }

} // namespace

std::string NativeCompilerDriver::LocateClang(const std::string& hint, const std::string& toolDirectory) {
    if (!hint.empty() && FileExists(hint)) return hint;

    if (const char* env = std::getenv("ROBIX_CLANG")) {
        if (FileExists(env)) return env;
    }

    if (!toolDirectory.empty()) {
        const fs::path bundled = fs::path(toolDirectory) / "toolchain" / "clang.exe";
        if (FileExists(bundled)) return bundled.string();

        const std::string recorded = ReadFirstLine(fs::path(toolDirectory) / "toolchain.txt");
        if (!recorded.empty() && FileExists(recorded)) return recorded;
    }

    if (const char* pathEnv = std::getenv("PATH")) {
        std::stringstream ss(pathEnv);
        std::string dir;
        while (std::getline(ss, dir, ';')) {
            if (dir.empty()) continue;
            const fs::path candidate = fs::path(dir) / "clang.exe";
            if (FileExists(candidate)) return candidate.string();
        }
    }

    if (const char* pf = std::getenv("ProgramFiles")) {
        const fs::path candidate = fs::path(pf) / "LLVM" / "bin" / "clang.exe";
        if (FileExists(candidate)) return candidate.string();
    }
    return {};
}

std::string NativeCompilerDriver::LocateRuntimeObject(const std::string& hint, const std::string& toolDirectory) {
    if (!hint.empty() && FileExists(hint)) return hint;
    if (!toolDirectory.empty()) {
        const fs::path p = fs::path(toolDirectory) / "RobixNativeRuntime.obj";
        if (FileExists(p)) return p.string();
    }
    const fs::path cwdBuild = fs::current_path() / "Build" / "RobixNativeRuntime.obj";
    if (FileExists(cwdBuild)) return cwdBuild.string();
    return {};
}

NativeBuildResult NativeCompilerDriver::CompileSource(std::string_view fileName,
                                                      std::string_view sourceText,
                                                      const NativeBuildOptions& options) {
    NativeBuildResult res;
    Frontend::DiagnosticBag diags;

    Frontend::Lexer lexer(sourceText, fileName, diags);
    Frontend::Parser parser(lexer.TokenizeAll(), fileName, diags);
    auto ast = parser.ParseCompilationUnit();

    Frontend::TypeCheckerOptions tcOpts;
    Frontend::TypeChecker typeChecker(diags, tcOpts);
    typeChecker.CheckCompilationUnit(ast);

    Frontend::SemanticAnalyzer sema(diags);
    sema.AnalyzeCompilationUnit(ast);

    if (diags.HasErrors()) {
        res.diagnosticsText = diags.FormatHumanReadable();
        return res;
    }

    IR::SSAIRBuilder irBuilder(sema, diags);
    IR::IRModule irMod = irBuilder.LowerCompilationUnit(ast);
    if (diags.HasErrors()) {
        res.diagnosticsText = diags.FormatHumanReadable();
        return res;
    }

    IR::IRPassManager passManager(sema, diags);
    passManager.RunOptimizationPipeline(irMod, true);
    if (diags.HasErrors()) {
        res.diagnosticsText = diags.FormatHumanReadable();
        return res;
    }

    CodeGen::NativeEmitOptions emitOpts;
    emitOpts.profile = options.release ? CodeGen::BuildProfile::Release : CodeGen::BuildProfile::Debug;
    emitOpts.target  = CodeGen::TargetPlatform::WindowsX64;
    CodeGen::NativeLLVMEmitter emitter(diags, emitOpts);
    std::optional<std::string> llvmIR = emitter.Emit(irMod);
    res.diagnosticsText = diags.FormatHumanReadable();
    if (!llvmIR.has_value() || diags.HasErrors()) {
        return res;
    }

    fs::path exePath = options.outputPath.empty()
                           ? fs::path(std::string(fileName)).replace_extension(".exe")
                           : fs::path(options.outputPath);
    fs::path llPath  = exePath;
    llPath.replace_extension(".ll");

    if (exePath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(exePath.parent_path(), ec);
    }
    {
        std::ofstream ll(llPath, std::ios::binary | std::ios::trunc);
        if (!ll) {
            res.diagnosticsText += "error: cannot write LLVM IR file '" + llPath.string() + "'\n";
            return res;
        }
        ll << *llvmIR;
    }
    res.llvmIRPath = llPath.string();

    if (options.emitLLVMOnly) {
        res.success = true;
        return res;
    }

    const std::string clang = LocateClang(options.clangPath, options.toolDirectory);
    if (clang.empty()) {
        res.diagnosticsText +=
            "error: clang.exe not found. Set ROBIX_CLANG, pass --clang <path>, or install LLVM (robixc uses clang as its native assembler/linker backend).\n";
        return res;
    }
    const std::string runtimeObj = LocateRuntimeObject(options.runtimeObjectPath, options.toolDirectory);
    if (runtimeObj.empty()) {
        res.diagnosticsText +=
            "error: RobixNativeRuntime.obj not found next to robixc.exe. Rebuild with Scripts\\BuildRelease.ps1.\n";
        return res;
    }

    std::ostringstream cmd;
    cmd << Quote(clang);
    if (options.release) {
        cmd << " -O3 -march=native -ffast-math -funroll-loops -finline-functions -mllvm -inline-threshold=5000 -mllvm -vectorize-loops -mllvm -vectorize-slp ";
    } else {
        cmd << " -O0 ";
    }
    cmd << "-fms-runtime-lib=static -Wno-override-module "
        << Quote(runtimeObj) << " " << Quote(llPath.string()) << " -o " << Quote(exePath.string());

    const int code = RunProcess(cmd.str(), res.toolOutput);
    if (!options.keepLLVMIR) {
        std::error_code ec;
        fs::remove(llPath, ec);
        res.llvmIRPath.clear();
    }
    if (code != 0 || !FileExists(exePath)) {
        res.diagnosticsText += "error: native toolchain (clang) failed with exit code " + std::to_string(code) + ":\n" + res.toolOutput;
        return res;
    }

    res.success        = true;
    res.executablePath = exePath.string();
    return res;
}

} // namespace Robix::Tools

