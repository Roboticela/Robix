#include "NativeCompilerDriver.hpp"

#include "../../Compiler/Frontend/ImportResolver.hpp"
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

std::string ForwardSlashes(std::string path) {
    for (char& ch : path) {
        if (ch == '\\') ch = '/';
    }
    return path;
}

void WriteText(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
}

NativeBuildResult LinkFreestanding(const std::string& clang,
                                   const fs::path& llvmIR,
                                   const fs::path& output,
                                   const NativeBuildOptions& options,
                                   NativeBuildResult res) {
    const fs::path binDir = fs::path(clang).parent_path();
    const fs::path work = output.has_parent_path() ? output.parent_path() : fs::path(".");
    std::error_code ec;
    fs::create_directories(work, ec);

    const bool optimize = options.release;
    std::string clangTarget = options.clangTarget;
    if (clangTarget.empty()) {
        clangTarget = options.outputKind == NativeOutputKind::BootImage ? "x86_64-elf" : options.targetTriple;
    }
    std::ostringstream compile;
    compile << Quote(clang) << " --target=" << clangTarget
            << " -c -ffreestanding -fno-stack-protector -fno-asynchronous-unwind-tables "
            << (optimize ? "-O3 " : "-O0 ");
    if (options.asmArch == "x86_64") {
        compile << "-mno-red-zone -mgeneral-regs-only -mno-sse -mno-mmx ";
    }
    const fs::path object = work / (output.stem().string() + ".payload.o");
    compile << "-Wno-override-module " << Quote(llvmIR.string()) << " -o " << Quote(object.string());
    if (RunProcess(compile.str(), res.toolOutput) != 0 || !FileExists(object)) {
        res.diagnosticsText += "error: freestanding compile failed:\n" + res.toolOutput;
        return res;
    }

    if (options.outputKind == NativeOutputKind::ObjectFile) {
        std::error_code moveEc;
        fs::rename(object, output, moveEc);
        if (moveEc) {
            fs::copy_file(object, output, fs::copy_options::overwrite_existing, moveEc);
            fs::remove(object, moveEc);
        }
        if (!FileExists(output)) {
            res.diagnosticsText += "error: could not write object '" + output.string() + "'\n";
            return res;
        }
        res.success = true;
        res.executablePath = output.string();
        res.toolOutput += "robixc: freestanding object only. No cross linker was run for " + options.targetTriple + ".\n";
        return res;
    }

    const fs::path lld = binDir / "ld.lld.exe";
    const fs::path objcopy = binDir / "llvm-objcopy.exe";
    if (!FileExists(lld) || !FileExists(objcopy)) {
        res.diagnosticsText += "error: ld.lld.exe or llvm-objcopy.exe was not found next to clang.\n";
        return res;
    }

    const fs::path highLd = work / "high.ld";
    const fs::path lowLd = work / "low.ld";
    const fs::path payloadElf = work / "kernel64.elf";
    const fs::path payloadBin = work / "kernel64.bin";
    const fs::path bootAsm = work / "boot32.s";
    const fs::path bootObj = work / "boot32.o";
    WriteText(highLd,
              "/* Robix " + options.imageRole + " image. Linked at 2MB for the trampoline. */\n"
              "ENTRY(" + (options.entrySymbol.empty() ? "Start" : options.entrySymbol) + ")\n"
              "SECTIONS {\n"
              "  . = 2M;\n"
              "  .text : { *(.text.entry) *(.text) *(.text.*) }\n"
              "  .rodata : { *(.rodata*) }\n"
              "  .data : { *(.data*) }\n"
              "  .bss : { *(.bss) *(COMMON) }\n"
              "}\n");
    WriteText(lowLd,
              "/* Robix " + options.imageRole + " multiboot trampoline. */\n"
              "ENTRY(_start)\n"
              "SECTIONS {\n"
              "  . = 1M;\n"
              "  .multiboot : { *(.multiboot) }\n"
              "  .text : { *(.text) }\n"
              "  .bss : { *(.bss) }\n"
              "  . = 2M;\n"
              "  .payload : { *(.payload) }\n"
              "}\n");
    std::ostringstream linkPayload;
    linkPayload << Quote(lld.string()) << " -nostdlib -e "
                << (options.entrySymbol.empty() ? "Start" : options.entrySymbol)
                << " -T " << Quote(highLd.string()) << " " << Quote(object.string())
                << " -o " << Quote(payloadElf.string());
    if (RunProcess(linkPayload.str(), res.toolOutput) != 0 || !FileExists(payloadElf)) {
        res.diagnosticsText += "error: freestanding link failed:\n" + res.toolOutput;
        return res;
    }
    std::ostringstream copyCmd;
    copyCmd << Quote(objcopy.string()) << " -O binary " << Quote(payloadElf.string()) << " " << Quote(payloadBin.string());
    if (RunProcess(copyCmd.str(), res.toolOutput) != 0 || !FileExists(payloadBin)) {
        res.diagnosticsText += "error: llvm-objcopy failed:\n" + res.toolOutput;
        return res;
    }

    WriteText(bootAsm,
              ".section .multiboot, \"a\"\n"
              ".align 4\n"
              ".long 0x1BADB002\n"
              ".long 0\n"
              ".long 0xE4524FFE\n"
              ".section .text\n"
              ".code32\n"
              ".global _start\n"
              "_start:\n"
              "    cli\n"
              "    mov $0xff, %al\n"
              "    out %al, $0x21\n"
              "    out %al, $0xa1\n"
              "    lidt idt_desc\n"
              "    lgdt gdt_desc\n"
              "    mov $0x10, %ax\n"
              "    mov %ax, %ds\n"
              "    mov %ax, %es\n"
              "    mov %ax, %ss\n"
              "    mov $pml4, %edi\n"
              "    mov $2048, %ecx\n"
              "    xor %eax, %eax\n"
              "    rep stosl\n"
              "    mov $pdpt, %eax\n"
              "    or $3, %eax\n"
              "    mov %eax, pml4\n"
              "    movl $0x83, pdpt\n"
              "    mov $pml4, %eax\n"
              "    mov %eax, %cr3\n"
              "    mov %cr4, %eax\n"
              "    or $0x30, %eax\n"
              "    mov %eax, %cr4\n"
              "    mov $0xC0000080, %ecx\n"
              "    rdmsr\n"
              "    or $0x100, %eax\n"
              "    wrmsr\n"
              "    mov %cr0, %eax\n"
              "    or $0x80000001, %eax\n"
              "    mov %eax, %cr0\n"
              "    lgdt gdt_desc\n"
              "    ljmp $0x08, $long_mode\n"
              ".code64\n"
              "long_mode:\n"
              "    mov $0x10, %ax\n"
              "    mov %ax, %ds\n"
              "    mov %ax, %es\n"
              "    mov %ax, %ss\n"
              "    mov $stack_top, %rsp\n"
              "    movq $0x70000, %rdi\n"
              "    movl %ebx, (%rdi)\n"
              "    mov $0x200000, %rax\n"
              "    jmp *%rax\n"
              ".align 8\n"
              "gdt:\n"
              "    .quad 0\n"
              "    .quad 0x00AF9A000000FFFF\n"
              "    .quad 0x00CF92000000FFFF\n"
              "gdt_desc:\n"
              "    .word gdt_desc - gdt - 1\n"
              "    .long gdt\n"
              "idt_desc:\n"
              "    .word 0\n"
              "    .long 0\n"
              ".section .bss\n"
              ".align 4096\n"
              "pml4: .skip 4096\n"
              "pdpt: .skip 4096\n"
              ".align 16\n"
              ".skip 16384\n"
              "stack_top:\n"
              ".section .payload, \"ax\"\n"
              ".incbin \"" + ForwardSlashes(payloadBin.string()) + "\"\n");

    std::ostringstream asmCmd;
    asmCmd << Quote(clang) << " --target=i386-elf -c " << Quote(bootAsm.string()) << " -o " << Quote(bootObj.string());
    if (RunProcess(asmCmd.str(), res.toolOutput) != 0 || !FileExists(bootObj)) {
        res.diagnosticsText += "error: trampoline assemble failed:\n" + res.toolOutput;
        return res;
    }
    std::ostringstream bootLink;
    bootLink << Quote(lld.string()) << " -m elf_i386 -T " << Quote(lowLd.string()) << " "
             << Quote(bootObj.string()) << " -o " << Quote(output.string());
    if (RunProcess(bootLink.str(), res.toolOutput) != 0 || !FileExists(output)) {
        res.diagnosticsText += "error: boot image link failed:\n" + res.toolOutput;
        return res;
    }
    res.success = true;
    res.executablePath = output.string();
    res.toolOutput += "robixc: linked " + options.imageRole + " image " + output.string()
                      + " (multiboot ELF32, payload at 2MB). This file is a QEMU image, not a hosted executable.\n";
    return res;
}

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
    Frontend::ResolveImports(ast, fileName, diags);

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
    emitOpts.freestanding = options.freestanding;
    emitOpts.entryName = options.entrySymbol.empty()
                             ? (options.freestanding ? "Start" : "Main")
                             : options.entrySymbol;
    emitOpts.asmArch = options.asmArch;
    emitOpts.tripleOverride = options.targetTriple;
    emitOpts.layoutOverride = options.dataLayout;
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
    const std::string runtimeObj = options.outputKind == NativeOutputKind::HostedExecutable
                                       ? LocateRuntimeObject(options.runtimeObjectPath, options.toolDirectory)
                                       : std::string{};
    if (options.outputKind == NativeOutputKind::HostedExecutable && runtimeObj.empty()) {
        res.diagnosticsText +=
            "error: RobixNativeRuntime.obj not found next to robixc.exe. Rebuild with Scripts\\BuildRelease.ps1.\n";
        return res;
    }

    // Release IR uses wrapping integer arithmetic. Run LLVM's O3 pipeline, then
    // reassociate once more: the first reassociate runs before instcombine folds
    // (x * C1 + C2) * C3, and loop unrolling copies the unreduced add chain.
    // A late reassociate pulls constants off the accumulator dependency chain.
    // SLP vectorization is disabled: it packs short scalar reductions (4-wide
    // conjugate gradient, n-body) into horizontal SIMD that is slower than scalar FMA.
    fs::path codegenInput = llPath;
    fs::path optIR = llPath;
    optIR.replace_extension(".opt.ll");
    fs::path reassocIR = llPath;
    reassocIR.replace_extension(".reassoc.ll");
    const fs::path optExe = fs::path(clang).parent_path() / "opt.exe";
    if (options.release && FileExists(optExe)) {
        std::string optLog;
        std::ostringstream optCmd;
        // Horizontal SLP reductions pack independent scalar FMAs into a
        // shuffle-heavy tree (conjugate gradient, n-body). Vertical SLP still
        // packs adjacent integer operations, which is what wins on EP.
        optCmd << Quote(optExe.string()) << " -S -O3 -vectorize-slp=true -slp-vectorize-hor=false "
               << Quote(llPath.string()) << " -o " << Quote(optIR.string());
        if (RunProcess(optCmd.str(), optLog) == 0 && FileExists(optIR)) {
            std::ostringstream reCmd;
            reCmd << Quote(optExe.string())
                  << " -S -passes=reassociate,instcombine,simplifycfg "
                  << Quote(optIR.string()) << " -o " << Quote(reassocIR.string());
            if (RunProcess(reCmd.str(), optLog) == 0 && FileExists(reassocIR)) {
                codegenInput = reassocIR;
            } else {
                codegenInput = optIR;
            }
        }
    }

    if (options.outputKind != NativeOutputKind::HostedExecutable) {
        NativeBuildResult linked = LinkFreestanding(clang, codegenInput, exePath, options, res);
        {
            std::error_code ec;
            fs::remove(optIR, ec);
            fs::remove(reassocIR, ec);
        }
        if (!options.keepLLVMIR) {
            std::error_code ec;
            fs::remove(llPath, ec);
            linked.llvmIRPath.clear();
        }
        return linked;
    }

    std::ostringstream cmd;
    cmd << Quote(clang);
    if (options.release) {
        // opt already applied vertical SLP. A second SLP pass rebuilds the
        // horizontal reductions that pass rejected. 32-byte loop alignment
        // keeps tight loops off a 32-byte boundary. /OPT:REF drops runtime
        // functions this program does not call.
        cmd << " -O3 -march=native -fno-slp-vectorize "
            << "-mllvm -x86-align-branch-boundary=32 -mllvm -x86-align-branch=fused+jcc+jmp "
            << "-mllvm -x86-experimental-pref-innermost-loop-alignment=5 "
            << "-ffunction-sections -fdata-sections -Wl,/OPT:REF -Wl,/OPT:ICF ";
    } else {
        cmd << " -O0 ";
    }
    cmd << "-fms-runtime-lib=static -Wno-override-module "
        << Quote(runtimeObj) << " " << Quote(codegenInput.string()) << " -o " << Quote(exePath.string());

    const int code = RunProcess(cmd.str(), res.toolOutput);
    {
        std::error_code ec;
        fs::remove(optIR, ec);
        fs::remove(reassocIR, ec);
    }
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

