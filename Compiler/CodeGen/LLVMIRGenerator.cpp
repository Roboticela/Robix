#include "LLVMIRGenerator.hpp"

#include <sstream>

namespace Robix::CodeGen {

std::string_view GetLLVMTargetTriple(TargetPlatform target) noexcept {
    switch (target) {
        case TargetPlatform::WindowsX64:  return "x86_64-pc-windows-msvc";
        case TargetPlatform::LinuxX64:    return "x86_64-unknown-linux-gnu";
        case TargetPlatform::LinuxARM64:  return "aarch64-unknown-linux-gnu";
        case TargetPlatform::MacOSARM64:  return "arm64-apple-macos14.0";
        case TargetPlatform::AndroidARM64:return "aarch64-linux-android";
        case TargetPlatform::RISCV64:     return "riscv64-unknown-elf";
        case TargetPlatform::WASM32:      return "wasm32-unknown-wasi";
        case TargetPlatform::BareMetalARM:return "thumbv7em-none-eabihf";
    }
    return "x86_64-pc-windows-msvc";
}

std::string_view GetLLVMDataLayout(TargetPlatform target) noexcept {
    switch (target) {
        case TargetPlatform::WindowsX64:
            return "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
        case TargetPlatform::LinuxX64:
            return "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
        case TargetPlatform::LinuxARM64:
        case TargetPlatform::MacOSARM64:
        case TargetPlatform::AndroidARM64:
            return "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128";
        case TargetPlatform::RISCV64:
            return "e-m:e-p:64:64-i64:64-i128:128-n32:64-S128";
        case TargetPlatform::WASM32:
            return "e-m:e-p:32:32-i64:64-n32:64-S128";
        case TargetPlatform::BareMetalARM:
            return "e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64";
    }
    return "e-m:w-i64:64-n8:16:32:64-S128";
}

std::string_view BuildProfileToString(BuildProfile profile) noexcept {
    switch (profile) {
        case BuildProfile::Debug:       return "debug";
        case BuildProfile::Release:     return "release";
        case BuildProfile::Performance: return "performance";
        case BuildProfile::RealTime:    return "realtime";
        case BuildProfile::Embedded:    return "embedded";
        case BuildProfile::Size:        return "size";
        case BuildProfile::Safety:      return "safety";
    }
    return "release";
}

LLVMIRGenerator::LLVMIRGenerator(Frontend::DiagnosticBag& diagnostics, NativeCodeGenOptions options) noexcept
    : m_diagnostics(diagnostics), m_options(options) {}

std::string LLVMIRGenerator::MapRobixTypeToLLVM(const Frontend::TypeNode& type) {
    if (type.isPointer) {
        return "ptr";
    }
    const std::string& b = type.baseName;
    if (b == "Void" || b.empty()) return "void";
    if (b == "Bool") return "i1";
    if (b == "Byte" || b == "Int8" || b == "UInt8") return "i8";
    if (b == "Int16" || b == "UInt16") return "i16";
    if (b == "Int32" || b == "UInt32" || b == "Char") return "i32";
    if (b == "Int64" || b == "UInt64" || b == "Int" || b == "UInt") return "i64";
    if (b == "Float16") return "half";
    if (b == "Float32") return "float";
    if (b == "Float64" || b == "Float" || b == "Double") return "double";
    if (b == "Decimal") return "fp128";
    return "ptr";
}

std::string LLVMIRGenerator::ResolveDirectOSSyscallSymbol(std::string_view robixIntrinsic,
                                                          TargetPlatform target) {
    if (robixIntrinsic == "File.WriteText") {
        switch (target) {
            case TargetPlatform::WindowsX64:   return "Win32_CreateFileW_WriteFile";
            case TargetPlatform::LinuxX64:
            case TargetPlatform::LinuxARM64:   return "Linux_sys_openat_write";
            case TargetPlatform::MacOSARM64:   return "Darwin_open_write";
            case TargetPlatform::AndroidARM64: return "Bionic_openat_write";
            case TargetPlatform::WASM32:       return "wasi_snapshot_preview1_fd_write";
            default:                           return "BareMetal_FlashWrite";
        }
    }
    if (robixIntrinsic == "File.ReadText") {
        switch (target) {
            case TargetPlatform::WindowsX64:   return "Win32_CreateFileW_ReadFile";
            case TargetPlatform::LinuxX64:
            case TargetPlatform::LinuxARM64:   return "Linux_sys_openat_read";
            case TargetPlatform::MacOSARM64:   return "Darwin_open_read";
            case TargetPlatform::AndroidARM64: return "Bionic_openat_read";
            case TargetPlatform::WASM32:       return "wasi_snapshot_preview1_fd_read";
            default:                           return "BareMetal_FlashRead";
        }
    }
    if (robixIntrinsic == "Thread.Sleep") {
        switch (target) {
            case TargetPlatform::WindowsX64:   return "Win32_Sleep";
            case TargetPlatform::LinuxX64:
            case TargetPlatform::LinuxARM64:
            case TargetPlatform::AndroidARM64: return "Linux_nanosleep";
            case TargetPlatform::MacOSARM64:   return "Darwin_nanosleep";
            default:                           return "BareMetal_SpinWaitCycles";
        }
    }
    if (robixIntrinsic == "Time.NowMicroseconds") {
        switch (target) {
            case TargetPlatform::WindowsX64:   return "Win32_QueryPerformanceCounter";
            case TargetPlatform::LinuxX64:
            case TargetPlatform::LinuxARM64:
            case TargetPlatform::AndroidARM64: return "Linux_clock_gettime_monotonic";
            case TargetPlatform::MacOSARM64:   return "Darwin_mach_absolute_time";
            default:                           return "BareMetal_ReadCycleCounterCSR";
        }
    }
    if (robixIntrinsic == "GPIO.Write") {
        switch (target) {
            case TargetPlatform::LinuxX64:
            case TargetPlatform::LinuxARM64:   return "Linux_gpio_mmio_write";
            default:                           return "MMIO_VolatileRegisterWrite";
        }
    }
    return "Robix_OS_" + std::string(robixIntrinsic);
}

std::string LLVMIRGenerator::GenerateLLVMIR(const IR::IRModule& mod) {
    (void)m_diagnostics;
    std::ostringstream out;

    out << "; ModuleID = '" << mod.moduleName << "'\n";
    out << "source_filename = \"" << mod.moduleName << "\"\n";
    out << "target datalayout = \"" << GetLLVMDataLayout(m_options.target) << "\"\n";
    out << "target triple = \"" << GetLLVMTargetTriple(m_options.target) << "\"\n";
    out << "; Build Profile = " << BuildProfileToString(m_options.profile) << "\n";

    if (!mod.requiresFullRuntime ||
        m_options.profile == BuildProfile::RealTime ||
        m_options.profile == BuildProfile::Embedded) {
        out << "; Runtime Profile: Minimal (Zero GC/Reflection Overhead)\n\n";
    } else {
        out << "; Runtime Profile: Full Automatic RC + Cycle Backup\n";
        out << "declare ptr @_RBX1_Runtime_AllocRC(i64)\n\n";
    }

    std::string fnAttrs;
    switch (m_options.profile) {
        case BuildProfile::Debug:       fnAttrs = "optnone noinline uwtable"; break;
        case BuildProfile::Release:     fnAttrs = "nounwind uwtable"; break;
        case BuildProfile::Performance: fnAttrs = "alwaysinline nounwind uwtable"; break;
        case BuildProfile::RealTime:    fnAttrs = "nounwind noimplicitfloat"; break;
        case BuildProfile::Embedded:
        case BuildProfile::Size:        fnAttrs = "optsize minsize nounwind"; break;
        case BuildProfile::Safety:      fnAttrs = "sanitize_address uwtable"; break;
    }

    for (const auto& fn : mod.functions) {
        const std::string retLLVM = MapRobixTypeToLLVM(fn.returnType);
        out << "define " << retLLVM << " @" << fn.mangledName << "(";
        for (std::size_t i = 0; i < fn.parameters.size(); ++i) {
            if (i > 0) out << ", ";
            out << MapRobixTypeToLLVM(fn.parameters[i].type) << " %" << fn.parameters[i].name;
        }
        out << ") " << fnAttrs << " {\n";

        for (const auto& bb : fn.blocks) {
            out << bb.label << ":\n";
            for (const auto& inst : bb.instructions) {
                if (inst.isDead) continue;

                switch (inst.opcode) {
                    case IR::IROpcode::ConstInt:
                        out << "  " << inst.dest.ToString() << " = add i64 0, " << inst.intImm << "\n";
                        break;

                    case IR::IROpcode::ConstFloat:
                        out << "  " << inst.dest.ToString() << " = fadd double 0.0, " << inst.floatImm << "\n";
                        break;

                    case IR::IROpcode::ConstBool:
                        out << "  " << inst.dest.ToString() << " = add i1 0, " << (inst.boolImm ? 1 : 0) << "\n";
                        break;

                    case IR::IROpcode::AllocStack:
                        out << "  " << inst.dest.ToString() << " = alloca %" << inst.textImm
                            << ", align 8 ; [stack-promoted via EscapeAnalysis]\n";
                        break;

                    case IR::IROpcode::AllocHeap:
                        out << "  " << inst.dest.ToString() << " = call ptr @_RBX1_Runtime_AllocRC(i64 64) ; type="
                            << inst.textImm << "\n";
                        break;

                    case IR::IROpcode::Add:
                    case IR::IROpcode::Sub:
                    case IR::IROpcode::Mul:
                    case IR::IROpcode::Div: {
                        const char* llvmOp = (inst.opcode == IR::IROpcode::Add) ? "add"
                                           : (inst.opcode == IR::IROpcode::Sub) ? "sub"
                                           : (inst.opcode == IR::IROpcode::Mul) ? "mul" : "sdiv";
                        out << "  " << inst.dest.ToString() << " = " << llvmOp << " i64 %"
                            << inst.operandIds[0] << ", %" << inst.operandIds[1] << "\n";
                        break;
                    }

                    case IR::IROpcode::CallDirect:
                        out << "  ";
                        if (inst.dest.IsValid() && inst.dest.type.baseName != "Void") {
                            out << inst.dest.ToString() << " = ";
                        }
                        out << "call " << MapRobixTypeToLLVM(inst.dest.type) << " @" << inst.textImm << "()";
                        if (inst.isDevirtualized) {
                            out << " ; [devirtualized direct call]";
                        }
                        out << "\n";
                        break;

                    case IR::IROpcode::CallOSIntrinsic: {
                        const std::string sysSym = ResolveDirectOSSyscallSymbol(inst.textImm, m_options.target);
                        out << "  ";
                        if (inst.dest.IsValid() && inst.dest.type.baseName != "Void") {
                            out << inst.dest.ToString() << " = ";
                        }
                        out << "call " << MapRobixTypeToLLVM(inst.dest.type) << " @" << sysSym
                            << "() ; [zero-cost OS intrinsic: " << inst.textImm << "]\n";
                        break;
                    }

                    case IR::IROpcode::InlineASM:
                        out << "  call void asm sideeffect \"" << inst.textImm << "\", \"\"()\n";
                        break;

                    case IR::IROpcode::Jump:
                        out << "  br label %" << inst.textImm << "\n";
                        break;

                    case IR::IROpcode::BranchCond:
                        out << "  br i1 %" << inst.operandIds[0]
                            << ", label %" << inst.textImm
                            << ", label %" << inst.secondaryText << "\n";
                        break;

                    case IR::IROpcode::ReturnVal:
                        out << "  ret " << retLLVM << " %" << inst.operandIds[0] << "\n";
                        break;

                    case IR::IROpcode::ReturnVoid:
                        out << "  ret void\n";
                        break;

                    default:
                        out << "  ; " << IR::IROpcodeToString(inst.opcode) << "\n";
                        break;
                }
            }
        }

        out << "}\n\n";
    }

    return out.str();
}

// ============================================================================
// DETERMINISTIC INCREMENTAL COMPILATION & BUILD CACHE IMPLEMENTATION
// ============================================================================

std::uint64_t IncrementalBuildCache::ComputeFingerprint(std::string_view sourceText,
                                                        const std::vector<std::string>& importedModules,
                                                        BuildProfile profile,
                                                        TargetPlatform target) noexcept {
    // 64-bit FNV-1a deterministic hash
    std::uint64_t hash = 14695981039346656037ULL;
    auto mixByte = [&](unsigned char b) noexcept {
        hash ^= static_cast<std::uint64_t>(b);
        hash *= 1099511628211ULL;
    };
    auto mixStr = [&](std::string_view s) noexcept {
        for (char c : s) {
            mixByte(static_cast<unsigned char>(c));
        }
        mixByte(0xFF);
    };

    mixStr(sourceText);
    for (const auto& imp : importedModules) {
        mixStr(imp);
    }
    mixByte(static_cast<unsigned char>(profile));
    mixByte(static_cast<unsigned char>(target));
    return hash;
}

bool IncrementalBuildCache::TryGetCachedLLVMIR(const std::string& moduleName,
                                               std::uint64_t fingerprint,
                                               std::string& outLLVMIR) noexcept {
    auto it = m_entries.find(moduleName);
    if (it != m_entries.end() && it->second.combinedHash == fingerprint) {
        outLLVMIR = it->second.cachedLLVMIR;
        ++m_cacheHits;
        return true;
    }
    ++m_cacheMisses;
    return false;
}

void IncrementalBuildCache::StoreCachedLLVMIR(const std::string& moduleName,
                                              std::uint64_t fingerprint,
                                              std::string llvmIR) {
    m_entries[moduleName] = CacheEntry{ fingerprint, std::move(llvmIR) };
}

} // namespace Robix::CodeGen

