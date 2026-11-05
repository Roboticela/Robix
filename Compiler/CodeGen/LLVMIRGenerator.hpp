#pragma once

#include "../IR/SSAIR.hpp"
#include "../Semantic/Diagnostics.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Robix::CodeGen {

enum class BuildProfile : std::uint8_t {
    Debug,
    Release,
    Performance,
    RealTime,
    Embedded,
    Size,
    Safety
};

enum class TargetPlatform : std::uint8_t {
    WindowsX64,
    LinuxX64,
    LinuxARM64,
    MacOSARM64,
    AndroidARM64,
    RISCV64,
    WASM32,
    BareMetalARM
};

[[nodiscard]] std::string_view GetLLVMTargetTriple(TargetPlatform target) noexcept;
[[nodiscard]] std::string_view GetLLVMDataLayout(TargetPlatform target) noexcept;
[[nodiscard]] std::string_view BuildProfileToString(BuildProfile profile) noexcept;

struct NativeCodeGenOptions {
    BuildProfile profile      = BuildProfile::Release;
    TargetPlatform target     = TargetPlatform::WindowsX64;
    bool emitDebugInfo        = false;
    bool enableLTO            = false;
};

class LLVMIRGenerator {
public:
    LLVMIRGenerator(Frontend::DiagnosticBag& diagnostics, NativeCodeGenOptions options = {}) noexcept;

    // Lower a validated SSA IRModule into target-specific native LLVM IR (.ll)
    [[nodiscard]] std::string GenerateLLVMIR(const IR::IRModule& mod);

    // Resolve zero-cost native OS system call symbol for an OS-independent Robix intrinsic
    [[nodiscard]] static std::string ResolveDirectOSSyscallSymbol(std::string_view robixIntrinsic,
                                                                  TargetPlatform target);

private:
    Frontend::DiagnosticBag& m_diagnostics;
    NativeCodeGenOptions m_options;

    static std::string MapRobixTypeToLLVM(const Frontend::TypeNode& type);
};

// ============================================================================
// DETERMINISTIC INCREMENTAL COMPILATION & BUILD CACHE
// ============================================================================

struct CacheEntry {
    std::uint64_t combinedHash = 0;
    std::string cachedLLVMIR;
};

class IncrementalBuildCache {
public:
    [[nodiscard]] static std::uint64_t ComputeFingerprint(std::string_view sourceText,
                                                          const std::vector<std::string>& importedModules,
                                                          BuildProfile profile,
                                                          TargetPlatform target) noexcept;

    [[nodiscard]] bool TryGetCachedLLVMIR(const std::string& moduleName,
                                          std::uint64_t fingerprint,
                                          std::string& outLLVMIR) noexcept;

    void StoreCachedLLVMIR(const std::string& moduleName,
                           std::uint64_t fingerprint,
                           std::string llvmIR);

    void ClearCache() noexcept { m_entries.clear(); m_cacheHits = 0; m_cacheMisses = 0; }
    [[nodiscard]] std::size_t GetCacheHits() const noexcept { return m_cacheHits; }
    [[nodiscard]] std::size_t GetCacheMisses() const noexcept { return m_cacheMisses; }

private:
    std::unordered_map<std::string, CacheEntry> m_entries;
    std::size_t m_cacheHits   = 0;
    std::size_t m_cacheMisses = 0;
};

} // namespace Robix::CodeGen

