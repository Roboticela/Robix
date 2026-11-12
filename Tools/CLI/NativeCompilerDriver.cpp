#include "NativeCompilerDriver.hpp"

#include "../../Compiler/Frontend/ImportResolver.hpp"
#include "../../Compiler/CodeGen/NativeBackend.hpp"
#include "../../Compiler/IR/SSAIR.hpp"
#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Optimizer/IRPassManager.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Semantic/SemanticAnalyzer.hpp"
#include "../../Compiler/Types/TypeChecker.hpp"

#include "../../StdLib/Security/SecurityAndNetwork.hpp"

#include <cstdint>
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
    if (clangTarget.empty() && options.outputKind == NativeOutputKind::BootImage) {
        clangTarget = "x86_64-elf";
    } else if (clangTarget.empty()) {
        clangTarget = options.targetTriple;
    }
    std::ostringstream compile;
    compile << Quote(clang) << " ";
    if (!clangTarget.empty()) {
        compile << "--target=" << clangTarget << " ";
    }
    compile << "-c -ffreestanding -fno-stack-protector -fno-unwind-tables "
            << "-fno-asynchronous-unwind-tables "
            << (optimize ? "-O3 " : "-O0 ");
    if (options.asmArch == "x86_64" && !clangTarget.empty()) {
        compile << "-mno-red-zone -mgeneral-regs-only -mno-sse -mno-mmx ";
    }
    const fs::path object = work / (output.stem().string() + ".payload.o");
    compile << "-Wno-override-module " << Quote(llvmIR.string()) << " -o " << Quote(object.string());
    std::string compileLog;
    if (RunProcess(compile.str(), compileLog) != 0 || !FileExists(object)) {
        res.toolOutput += compileLog;
        res.diagnosticsText += "error: freestanding compile failed:\n" + res.toolOutput;
        return res;
    }
    if (!options.profileName.empty()) {
        res.toolOutput += "compile: " + compile.str() + "\n";
    }
    res.toolOutput += compileLog;

    if (options.outputKind == NativeOutputKind::LinuxExecutable) {
        const fs::path lld = binDir / "Linker.exe";
        if (!FileExists(lld)) {
            res.diagnosticsText += "error: the compiler install is missing Linker.exe.\n";
            return res;
        }
        const fs::path script = work / "linux.ld";
        WriteText(script,
                  "ENTRY(Start)\nSECTIONS {\n  . = 0x400000;\n"
                  "  .text : { *(.text*) }\n  .rodata : { *(.rodata*) }\n"
                  "  .data : { *(.data*) }\n  .bss : { *(.bss) *(COMMON) }\n}\n");
        const std::string entry = options.entrySymbol.empty() ? "Start" : options.entrySymbol;
        std::ostringstream link;
        link << Quote(lld.string()) << " -flavor gnu -m elf_x86_64 -nostdlib -static -e " << entry << " -T "
             << Quote(script.string()) << " " << Quote(object.string()) << " -o " << Quote(output.string());
        std::string linkLog;
        if (RunProcess(link.str(), linkLog) != 0 || !FileExists(output)) {
            res.toolOutput += linkLog;
            res.diagnosticsText += "error: Linux link failed:\n" + res.toolOutput;
            return res;
        }
        res.success = true;
        res.executablePath = output.string();
        res.toolOutput += "linux-executable: " + output.string() + "\n";
        res.toolOutput += linkLog;
        return res;
    }

    if (options.outputKind == NativeOutputKind::BootImage && options.asmArch != "x86_64") {
        const fs::path lld = binDir / "Linker.exe";
        if (!FileExists(lld)) {
            res.diagnosticsText += "error: the compiler install is missing Linker.exe.\n";
            return res;
        }
        const char* origin = "0x40080000";
        const char* emulation = "aarch64elf";
        std::string stub =
            ".global _start\n_start:\n"
            "    ldr x0, =stack_top\n    mov sp, x0\n    bl Start\n"
            "1:  wfi\n    b 1b\n.section .bss\n.align 4\n.skip 16384\nstack_top:\n";
        if (options.asmArch == "arm") {
            emulation = "armelf";
            stub =
                ".syntax unified\n.arch armv7-a\n.global _start\n_start:\n"
                "    ldr sp, =stack_top\n    bl Start\n"
                "1:  wfi\n    b 1b\n.section .bss\n.align 4\n.skip 16384\nstack_top:\n";
        } else if (options.asmArch == "riscv64") {
            origin = "0x80000000";
            emulation = "elf64lriscv";
            stub =
                ".global _start\n_start:\n"
                "    la sp, stack_top\n    call Start\n"
                "1:  wfi\n    j 1b\n.section .bss\n.align 4\n.skip 16384\nstack_top:\n";
        } else if (options.asmArch != "aarch64") {
            res.diagnosticsText += "error: no boot image for architecture " + options.asmArch + ".\n";
            return res;
        }
        const fs::path script = work / "board.ld";
        const fs::path bootAsm = work / "board.s";
        const fs::path bootObj = work / "board.o";
        WriteText(script,
                  "ENTRY(_start)\nSECTIONS {\n  . = " + std::string(origin) + ";\n"
                  "  .text : { *(.text.entry) *(.text) *(.text.*) }\n"
                  "  .rodata : { *(.rodata*) }\n  .data : { *(.data*) }\n"
                  "  .bss : { *(.bss) *(COMMON) }\n}\n");
        WriteText(bootAsm, stub);
        std::ostringstream asmCmd;
        asmCmd << Quote(clang) << " --target=" << clangTarget << " -c " << Quote(bootAsm.string())
               << " -o " << Quote(bootObj.string());
        if (RunProcess(asmCmd.str(), res.toolOutput) != 0 || !FileExists(bootObj)) {
            res.diagnosticsText += "error: board entry assemble failed:\n" + res.toolOutput;
            return res;
        }
        const std::string entry = options.entrySymbol.empty() ? "Start" : options.entrySymbol;
        std::ostringstream link;
        link << Quote(lld.string()) << " -flavor gnu -m " << emulation << " -nostdlib -e _start -T "
             << Quote(script.string()) << " " << Quote(bootObj.string()) << " "
             << Quote(object.string()) << " -o " << Quote(output.string());
        (void)entry;
        if (RunProcess(link.str(), res.toolOutput) != 0 || !FileExists(output)) {
            res.diagnosticsText += "error: board link failed:\n" + res.toolOutput;
            return res;
        }
        res.success = true;
        res.executablePath = output.string();
        res.toolOutput += "robixc: linked " + options.asmArch + " " + options.imageRole
                          + " image " + output.string() + " at " + origin
                          + ". This file is a QEMU image, not a hosted executable.\n";
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

    const fs::path lld = binDir / "Linker.exe";
    const fs::path objcopy = binDir / "ObjectCopy.exe";
    if (!FileExists(lld) || !FileExists(objcopy)) {
        res.diagnosticsText += "error: the compiler install is missing Linker.exe or ObjectCopy.exe.\n";
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
              "  /* Compiler frame: 8 + 1920*1080*4 bytes. Keep FrameFloor in RoboAddressSpace in step. */\n"
              "  rx_frame_store = 0x3000000;\n"
              "  . = 2M;\n"
              "  .text : { *(.text.entry) *(.text) *(.text.*) }\n"
              "  .rodata : { *(.rodata*) }\n"
              "  .data : { *(.data*) }\n"
              "  .bss : { *(.bss) *(COMMON) }\n"
              "  /DISCARD/ : { *(.eh_frame) *(.eh_frame_hdr) *(.comment) }\n"
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
    linkPayload << Quote(lld.string()) << " -flavor gnu -nostdlib -e "
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
        res.diagnosticsText += "error: ObjectCopy failed:\n" + res.toolOutput;
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
              "    movl $0x40000083, pdpt+8\n"
              "    movl $0x80000083, pdpt+16\n"
              "    movl $0xC0000083, pdpt+24\n"
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
              "    mov $0x3000000, %rdi\n"
              "    mov $1036801, %rcx\n"
              "    xor %eax, %eax\n"
              "    rep stosq\n"
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
    bootLink << Quote(lld.string()) << " -flavor gnu -m elf_i386 -T " << Quote(lowLd.string()) << " "
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

NativeBuildResult FinishHostedArtifact(const std::string& clang,
                                       const fs::path& llvmIR,
                                       const fs::path& output,
                                       std::string_view sourceText,
                                       const NativeBuildOptions& options,
                                       NativeBuildResult res) {
    const fs::path binDir = fs::path(clang).parent_path();
    const fs::path work = output.has_parent_path() ? output.parent_path() : fs::path(".");
    std::error_code ec;
    fs::create_directories(work, ec);
    const fs::path object = work / (output.stem().string() + ".hosted.obj");
    std::ostringstream compile;
    compile << Quote(clang) << " -c -Wno-override-module " << Quote(llvmIR.string())
            << " -o " << Quote(object.string());
    std::string log;
    if (RunProcess(compile.str(), log) != 0 || !FileExists(object)) {
        res.toolOutput += log;
        res.diagnosticsText += "error: hosted object compile failed:\n" + res.toolOutput;
        return res;
    }
    res.toolOutput += log;

    if (options.outputKind == NativeOutputKind::HostedObject) {
        fs::copy_file(object, output, fs::copy_options::overwrite_existing, ec);
        if (ec || !FileExists(output)) {
            res.diagnosticsText += "error: could not write object '" + output.string() + "'\n";
            return res;
        }
        fs::remove(object, ec);
        res.success = true;
        res.executablePath = output.string();
        res.toolOutput += "output: object\n";
        return res;
    }

    if (options.outputKind == NativeOutputKind::StaticLibrary) {
        const fs::path ar = binDir / "Archiver.exe";
        if (!FileExists(ar)) {
            res.diagnosticsText += "error: the compiler install is missing Archiver.exe.\n";
            return res;
        }
        std::ostringstream cmd;
        cmd << Quote(ar.string()) << " rcs " << Quote(output.string()) << " " << Quote(object.string());
        std::string arLog;
        if (RunProcess(cmd.str(), arLog) != 0 || !FileExists(output)) {
            res.toolOutput += arLog;
            res.diagnosticsText += "error: static library failed:\n" + res.toolOutput;
            return res;
        }
        res.success = true;
        res.executablePath = output.string();
        res.toolOutput += "output: static-library\n";
        return res;
    }

    if (options.outputKind == NativeOutputKind::SharedLibrary) {
        const fs::path nm = binDir / "Symbols.exe";
        std::string symbols;
        if (FileExists(nm)) {
            RunProcess(Quote(nm.string()) + " --defined-only " + Quote(object.string()), symbols);
        }
        std::string def = "EXPORTS\n";
        std::istringstream symbolLines(symbols);
        std::string symbolLine;
        while (std::getline(symbolLines, symbolLine)) {
            const auto mark = symbolLine.find(" T ");
            if (mark == std::string::npos) continue;
            std::string name = symbolLine.substr(mark + 3);
            while (!name.empty() && (name.back() == '\r' || name.back() == ' ')) name.pop_back();
            if (!name.empty() && name[0] != '.') def += name + "\n";
        }
        const fs::path defPath = work / (output.stem().string() + ".def");
        WriteText(defPath, def);
        std::ostringstream cmd;
        cmd << Quote(clang) << " -shared -o " << Quote(output.string()) << " " << Quote(object.string())
            << " -Wl,/DEF:" << defPath.string();
        std::string linkLog;
        if (RunProcess(cmd.str(), linkLog) != 0 || !FileExists(output)) {
            res.toolOutput += linkLog;
            res.diagnosticsText += "error: shared library failed:\n" + res.toolOutput;
            return res;
        }
        res.success = true;
        res.executablePath = output.string();
        res.toolOutput += "output: shared-library\n";
        return res;
    }

    const std::string sha = StdLib::CryptoEngine::ComputeSHA256Hex(sourceText);
    std::string bytes = "RXPKG 1\nsha256 " + sha + "\nsource " + std::to_string(sourceText.size()) + "\n";
    bytes.append(sourceText.data(), sourceText.size());
    bytes += "\n";
    std::ofstream package(output, std::ios::binary | std::ios::trunc);
    if (!package) {
        res.diagnosticsText += "error: could not write package '" + output.string() + "'\n";
        return res;
    }
    package << bytes;
    res.success = true;
    res.executablePath = output.string();
    res.toolOutput += "output: package\npackage: sha256 " + sha + "\n";
    return res;
}

} // namespace

std::string NativeCompilerDriver::LocateClang(const std::string& hint, const std::string& toolDirectory) {
    if (!hint.empty() && FileExists(hint)) return hint;
    if (toolDirectory.empty()) return {};
    const fs::path tool(toolDirectory);
    if (FileExists(tool / "Toolchain" / "Clang.exe")) return (tool / "Toolchain" / "Clang.exe").string();
    return {};
}

std::string BundledLibrary(const fs::path& toolchainBin, const std::string& name, std::string& error) {
    const fs::path path = toolchainBin / "Libraries" / name;
    if (!FileExists(path)) {
        error += "error: the compiler install is missing '" + path.string() + "'.\n";
        return {};
    }
    return path.string();
}

std::string NativeCompilerDriver::LocateRuntimeObject(const std::string& hint, const std::string& toolDirectory) {
    if (!hint.empty() && FileExists(hint)) return hint;
    const auto accept = [](const fs::path& path) {
        return FileExists(path) ? path.string() : std::string();
    };
    if (!toolDirectory.empty()) {
        const fs::path tool(toolDirectory);
        if (const std::string runtime = accept(tool / "Runtime" / "RobixNativeRuntime.rxm"); !runtime.empty()) {
            return runtime;
        }
    }
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
    emitOpts.debugLines = options.debugLines;
    emitOpts.omitMain = options.omitMain;
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
            "error: the compiler install is missing Toolchain/Clang.exe.\n";
        return res;
    }
    const std::string runtimeObj = options.outputKind == NativeOutputKind::HostedExecutable
                                       ? LocateRuntimeObject(options.runtimeObjectPath, options.toolDirectory)
                                       : std::string{};
    if (options.outputKind == NativeOutputKind::HostedExecutable && runtimeObj.empty()) {
        res.diagnosticsText +=
            "error: Runtime/RobixNativeRuntime.rxm was not found in the compiler install.\n";
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
    const fs::path optExe = fs::path(clang).parent_path() / "Opt.exe";
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

    if (options.outputKind == NativeOutputKind::HostedObject ||
        options.outputKind == NativeOutputKind::StaticLibrary ||
        options.outputKind == NativeOutputKind::SharedLibrary ||
        options.outputKind == NativeOutputKind::PackageArchive) {
        NativeBuildResult built = FinishHostedArtifact(clang, codegenInput, exePath, sourceText, options, res);
        {
            std::error_code ec;
            fs::remove(optIR, ec);
            fs::remove(reassocIR, ec);
        }
        if (!options.keepLLVMIR) {
            std::error_code ec;
            fs::remove(llPath, ec);
            if (built.success) built.llvmIRPath.clear();
        }
        return built;
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
        if (!options.profileName.empty()) {
            linked.toolOutput += "profile: " + options.profileName + "\n";
        }
        return linked;
    }

    if (options.omitMain) {
        std::ostringstream objCmd;
        objCmd << Quote(clang) << " -c -Wno-override-module " << Quote(codegenInput.string())
               << " -o " << Quote(exePath.string());
        if (RunProcess(objCmd.str(), res.toolOutput) != 0 || !FileExists(exePath)) {
            res.diagnosticsText += "error: object compile failed:\n" + res.toolOutput;
            return res;
        }
        res.success = true;
        res.executablePath = exePath.string();
        res.toolOutput += "output: object\n";
        return res;
    }

    std::string compileFlags = " -O0 ";
    std::string linkFlags;
    if (options.profileName == "size") {
        compileFlags = " -Oz -ffunction-sections -fdata-sections ";
        linkFlags = " /OPT:REF";
    } else if (options.profileName == "realtime") {
        compileFlags = " -O2 -fno-strict-aliasing ";
    } else if (options.profileName == "safety") {
        compileFlags = " -O0 ";
    } else if (options.profileName == "performance") {
        compileFlags = " -O3 -march=native -funroll-loops -fno-slp-vectorize "
                       "-ffunction-sections -fdata-sections ";
        linkFlags = " /OPT:REF /OPT:ICF";
    } else if (options.release) {
        // opt already applied vertical SLP. A second SLP pass rebuilds the
        // horizontal reductions that pass rejected. 32-byte loop alignment
        // keeps tight loops off a 32-byte boundary. /OPT:REF drops runtime
        // functions this program does not call.
        compileFlags = " -O3 -march=native -fno-slp-vectorize "
                       "-mllvm -x86-align-branch-boundary=32 -mllvm -x86-align-branch=fused+jcc+jmp "
                       "-mllvm -x86-experimental-pref-innermost-loop-alignment=5 "
                       "-ffunction-sections -fdata-sections ";
        linkFlags = " /OPT:REF /OPT:ICF";
    }
    if (options.linkNetwork) {
        if (options.networkObjectPath.empty() || !FileExists(options.networkObjectPath)) {
            res.diagnosticsText += "error: this project sets Network: true, and Runtime/NetworkRuntime.rxm was not found.\n";
            return res;
        }
    }
    bool linkLoopback = false;
    bool linkEnv = false;
    {
        std::ifstream ir(codegenInput, std::ios::binary);
        const std::string irText((std::istreambuf_iterator<char>(ir)), std::istreambuf_iterator<char>());
        linkLoopback = irText.find("call ptr @rx_network_send_text") != std::string::npos ||
                       irText.find("call ptr @rx_network_call") != std::string::npos;
        linkEnv = irText.find("call ptr @rx_env_get") != std::string::npos ||
                  irText.find("call i64 @rx_env_") != std::string::npos;
    }
    const fs::path toolBin = fs::path(clang).parent_path();
    const fs::path lld = toolBin / "Linker.exe";
    if (!FileExists(lld)) {
        res.diagnosticsText += "error: the compiler install is missing '" + lld.string() + "'.\n";
        return res;
    }
    const fs::path codegenObj = exePath.parent_path() / (exePath.stem().string() + ".codegen.obj");
    std::ostringstream compile;
    compile << Quote(clang) << compileFlags << " -c -Wno-override-module " << Quote(codegenInput.string())
            << " -o " << Quote(codegenObj.string());
    if (RunProcess(compile.str(), res.toolOutput) != 0 || !FileExists(codegenObj)) {
        res.diagnosticsText += "error: the built-in assembler failed:\n" + res.toolOutput;
        return res;
    }
    std::ostringstream cmd;
    cmd << Quote(lld.string()) << " -flavor link /nologo /nodefaultlib /subsystem:console /entry:mainCRTStartup"
        << linkFlags << " /out:" << Quote(exePath.string()) << " " << Quote(codegenObj.string()) << " "
        << Quote(runtimeObj);
    const char* requiredLibs[] = { "LibCmt.lib", "LibVcRuntime.lib", "LibCPP.lib", "OldNames.lib", "LibUcrt.lib",
                                   "Kernel32.lib" };
    bool librariesPresent = true;
    for (const char* name : requiredLibs) {
        const std::string path = BundledLibrary(toolBin, name, res.diagnosticsText);
        if (path.empty()) librariesPresent = false;
        else cmd << " " << Quote(path);
    }
    auto addLibrary = [&](const char* name) {
        const std::string path = BundledLibrary(toolBin, name, res.diagnosticsText);
        if (path.empty()) librariesPresent = false;
        else cmd << " " << Quote(path);
    };
    if (linkLoopback) {
        const fs::path loopback = fs::path(runtimeObj).parent_path() / "LoopbackText.rxm";
        if (!FileExists(loopback)) {
            res.diagnosticsText += "error: Runtime/LoopbackText.rxm was not found in the compiler install.\n";
            return res;
        }
        cmd << " " << Quote(loopback.string());
        addLibrary("Ws232.lib");
    }
    if (linkEnv) {
        const fs::path envStore = fs::path(runtimeObj).parent_path() / "EnvStore.rxm";
        if (!FileExists(envStore)) {
            res.diagnosticsText += "error: Runtime/EnvStore.rxm was not found in the compiler install.\n";
            return res;
        }
        cmd << " " << Quote(envStore.string());
    }
    if (options.linkNetwork) {
        cmd << " " << Quote(options.networkObjectPath);
        addLibrary("Ws232.lib");
        addLibrary("Secur32.lib");
        addLibrary("Crypt32.lib");
    }
    if (!librariesPresent) return res;

    std::string linkLog;
    const int code = RunProcess(cmd.str(), linkLog);
    {
        std::error_code ec;
        fs::remove(codegenObj, ec);
    }
    if (!options.profileName.empty()) {
        res.toolOutput += "link: " + cmd.str() + "\n";
    }
    res.toolOutput += linkLog;
    if (options.linkNetwork) res.toolOutput += "network-runtime: linked " + options.networkObjectPath + "\n";
    else res.toolOutput += "network-runtime: omitted\n";
    if (!options.profileName.empty()) {
        res.toolOutput += "profile: " + options.profileName + "\n";
    }
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
        res.diagnosticsText += "error: the built-in linker failed with exit code " + std::to_string(code) + ":\n" + res.toolOutput;
        return res;
    }

    res.success        = true;
    res.executablePath = exePath.string();
    return res;
}

} // namespace Robix::Tools

