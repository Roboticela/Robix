#pragma once

#include "../IR/SSAIR.hpp"
#include "../Semantic/Diagnostics.hpp"
#include "../Semantic/SemanticAnalyzer.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace Robix::IR {

struct OptimizationStats {
    std::size_t constantsFoldedCount            = 0;
    std::size_t stackPromotedAllocsCount        = 0;
    std::size_t devirtualizedCallsCount         = 0;
    std::size_t deadInstructionsEliminatedCount = 0;
    std::size_t verificationRunsCount           = 0;
    std::vector<std::string> executedPassOrder;
};

class IRPassManager {
public:
    IRPassManager(const Frontend::SemanticAnalyzer& sema, Frontend::DiagnosticBag& diagnostics);

    // Verify all SSA, dominance, control-flow terminator, and type invariants
    bool VerifyIRModule(const IRModule& mod);

    // Run individual optimization passes
    std::size_t RunConstantFoldingPass(IRModule& mod);
    std::size_t RunEscapeAnalysisPass(IRModule& mod);
    std::size_t RunDevirtualizationPass(IRModule& mod);
    std::size_t RunDeadCodeEliminationPass(IRModule& mod);

    // Run full optimization pipeline with pass dependency scheduling,
    // post-pass IR verification, and crash recovery protection.
    bool RunOptimizationPipeline(IRModule& mod, bool enableOptimizations = true);

    // Execute an arbitrary compiler callback under structured ICE crash recovery
    bool ExecuteWithCrashRecovery(std::string_view phaseName, const std::function<void()>& action);

    [[nodiscard]] const OptimizationStats& GetStats() const noexcept { return m_stats; }

private:
    const Frontend::SemanticAnalyzer& m_sema;
    Frontend::DiagnosticBag& m_diagnostics;
    OptimizationStats m_stats{};
};

} // namespace Robix::IR

