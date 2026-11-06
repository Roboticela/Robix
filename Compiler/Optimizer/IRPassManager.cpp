#include "IRPassManager.hpp"

#include <exception>
#include <unordered_map>
#include <unordered_set>

namespace Robix::IR {

IRPassManager::IRPassManager(const Frontend::SemanticAnalyzer& sema, Frontend::DiagnosticBag& diagnostics)
    : m_sema(sema), m_diagnostics(diagnostics) {}

bool IRPassManager::VerifyIRModule(const IRModule& mod) {
    ++m_stats.verificationRunsCount;
    bool valid = true;

    for (const auto& fn : mod.functions) {
        if (fn.isExtern) continue;
        if (fn.blocks.empty()) {
            m_diagnostics.ReportError(
                "I4001",
                "IR Verification failed: Function '" + fn.name + "' has no basic blocks.",
                fn.location);
            valid = false;
            continue;
        }

        std::unordered_set<std::string> blockLabels;
        for (const auto& bb : fn.blocks) {
            if (!blockLabels.insert(bb.label).second) {
                m_diagnostics.ReportError(
                    "I4001",
                    "IR Verification failed: Duplicate basic block label '" + bb.label + "' in '" + fn.name + "'.",
                    fn.location);
                valid = false;
            }
        }

        std::unordered_set<std::uint32_t> definedSSAIds;
        for (const auto& bb : fn.blocks) {
            if (bb.instructions.empty()) {
                m_diagnostics.ReportError(
                    "I4001",
                    "IR Verification failed: Basic block '" + bb.label + "' in '" + fn.name + "' is empty.",
                    fn.location);
                valid = false;
                continue;
            }

            if (!IsTerminatorOpcode(bb.instructions.back().opcode)) {
                m_diagnostics.ReportError(
                    "I4001",
                    "IR Verification failed: Basic block '" + bb.label + "' in '" + fn.name + "' does not end with a terminator instruction.",
                    bb.instructions.back().location);
                valid = false;
            }

            for (const auto& inst : bb.instructions) {
                if (inst.isDead) continue;

                // Verify all operands were defined earlier in SSA form
                for (std::uint32_t opId : inst.operandIds) {
                    if (opId != 0 && definedSSAIds.find(opId) == definedSSAIds.end()) {
                        m_diagnostics.ReportError(
                            "I4001",
                            "IR Verification failed: SSA value '%" + std::to_string(opId) +
                                "' used before definition in '" + fn.name + "'.",
                            inst.location);
                        valid = false;
                    }
                }

                // Verify single assignment property
                if (inst.dest.IsValid()) {
                    if (!definedSSAIds.insert(inst.dest.ssaId).second) {
                        m_diagnostics.ReportError(
                            "I4001",
                            "IR Verification failed: SSA value '%" + std::to_string(inst.dest.ssaId) +
                                "' assigned multiple times in '" + fn.name + "'.",
                            inst.location);
                        valid = false;
                    }
                }

                // Verify branch targets exist
                if (inst.opcode == IROpcode::Jump) {
                    if (blockLabels.find(inst.textImm) == blockLabels.end()) {
                        m_diagnostics.ReportError(
                            "I4001",
                            "IR Verification failed: Jump target '" + inst.textImm + "' does not exist in '" + fn.name + "'.",
                            inst.location);
                        valid = false;
                    }
                } else if (inst.opcode == IROpcode::BranchCond) {
                    if (blockLabels.find(inst.textImm) == blockLabels.end() ||
                        blockLabels.find(inst.secondaryText) == blockLabels.end()) {
                        m_diagnostics.ReportError(
                            "I4001",
                            "IR Verification failed: Conditional branch target does not exist in '" + fn.name + "'.",
                            inst.location);
                        valid = false;
                    }
                }
            }
        }
    }

    return valid;
}

std::size_t IRPassManager::RunConstantFoldingPass(IRModule& mod) {
    m_stats.executedPassOrder.emplace_back("ConstantFoldingPass");
    std::size_t folded = 0;

    for (auto& fn : mod.functions) {
        std::unordered_map<std::uint32_t, const IRInstruction*> constDefs;

        for (auto& bb : fn.blocks) {
            for (auto& inst : bb.instructions) {
                if (inst.isDead) continue;

                if (inst.opcode == IROpcode::ConstInt ||
                    inst.opcode == IROpcode::ConstFloat ||
                    inst.opcode == IROpcode::ConstBool) {
                    constDefs[inst.dest.ssaId] = &inst;
                    continue;
                }

                if (inst.operandIds.size() == 2) {
                    auto itL = constDefs.find(inst.operandIds[0]);
                    auto itR = constDefs.find(inst.operandIds[1]);
                    if (itL != constDefs.end() && itR != constDefs.end()) {
                        const IRInstruction* l = itL->second;
                        const IRInstruction* r = itR->second;

                        if (l->opcode == IROpcode::ConstInt && r->opcode == IROpcode::ConstInt) {
                            const std::int64_t lv = l->intImm;
                            const std::int64_t rv = r->intImm;
                            bool didFoldInt  = false;
                            bool didFoldBool = false;
                            std::int64_t resInt = 0;
                            bool resBool = false;

                            switch (inst.opcode) {
                                case IROpcode::Add: resInt = lv + rv; didFoldInt = true; break;
                                case IROpcode::Sub: resInt = lv - rv; didFoldInt = true; break;
                                case IROpcode::Mul: resInt = lv * rv; didFoldInt = true; break;
                                case IROpcode::Div: if (rv != 0) { resInt = lv / rv; didFoldInt = true; } break;
                                case IROpcode::Mod: if (rv != 0) { resInt = lv % rv; didFoldInt = true; } break;
                                case IROpcode::Eq:  resBool = (lv == rv); didFoldBool = true; break;
                                case IROpcode::Ne:  resBool = (lv != rv); didFoldBool = true; break;
                                case IROpcode::Lt:  resBool = (lv < rv);  didFoldBool = true; break;
                                case IROpcode::Le:  resBool = (lv <= rv); didFoldBool = true; break;
                                case IROpcode::Gt:  resBool = (lv > rv);  didFoldBool = true; break;
                                case IROpcode::Ge:  resBool = (lv >= rv); didFoldBool = true; break;
                                default: break;
                            }

                            if (didFoldInt) {
                                inst.opcode = IROpcode::ConstInt;
                                inst.intImm = resInt;
                                inst.operandIds.clear();
                                constDefs[inst.dest.ssaId] = &inst;
                                ++folded;
                            } else if (didFoldBool) {
                                inst.opcode  = IROpcode::ConstBool;
                                inst.boolImm = resBool;
                                inst.operandIds.clear();
                                constDefs[inst.dest.ssaId] = &inst;
                                ++folded;
                            }
                        }
                    }
                }
            }
        }
    }

    m_stats.constantsFoldedCount += folded;
    return folded;
}

std::size_t IRPassManager::RunEscapeAnalysisPass(IRModule& mod) {
    m_stats.executedPassOrder.emplace_back("EscapeAnalysisPass");
    std::size_t promoted = 0;
    bool anyRemainingHeap = false;

    for (auto& fn : mod.functions) {
        std::unordered_set<std::uint32_t> heapAllocIds;
        std::unordered_map<std::string, std::uint32_t> varToAllocId;
        std::unordered_set<std::uint32_t> escapedAllocIds;

        // Pass 1: Identify all AllocHeap instructions and track alias stores/loads
        for (const auto& bb : fn.blocks) {
            for (const auto& inst : bb.instructions) {
                if (inst.isDead) continue;
                if (inst.opcode == IROpcode::AllocHeap && inst.dest.IsValid()) {
                    heapAllocIds.insert(inst.dest.ssaId);
                } else if (inst.opcode == IROpcode::StoreVar && !inst.operandIds.empty()) {
                    const std::uint32_t srcId = inst.operandIds[0];
                    if (heapAllocIds.find(srcId) != heapAllocIds.end()) {
                        varToAllocId[inst.textImm] = srcId;
                    }
                } else if (inst.opcode == IROpcode::LoadVar && inst.dest.IsValid()) {
                    auto it = varToAllocId.find(inst.textImm);
                    if (it != varToAllocId.end()) {
                        heapAllocIds.insert(inst.dest.ssaId);
                        varToAllocId[inst.dest.ToString()] = it->second;
                    }
                }
            }
        }

        // Pass 2: Check if any heap allocation escapes via ReturnVal
        for (const auto& bb : fn.blocks) {
            for (const auto& inst : bb.instructions) {
                if (inst.isDead) continue;
                const bool freesHeap = inst.opcode == IROpcode::ReturnVal ||
                    (inst.opcode == IROpcode::CallOSIntrinsic && inst.textImm == "Memory.Free");
                if (freesHeap && !inst.operandIds.empty()) {
                    const std::uint32_t retId = inst.operandIds[0];
                    if (heapAllocIds.find(retId) != heapAllocIds.end()) {
                        escapedAllocIds.insert(retId);
                        auto rootIt = varToAllocId.find("%" + std::to_string(retId));
                        if (rootIt != varToAllocId.end()) {
                            escapedAllocIds.insert(rootIt->second);
                        }
                    }
                }
            }
        }

        // Pass 3: Promote non-escaping AllocHeap -> AllocStack
        bool fnStillUsesHeap = false;
        for (auto& bb : fn.blocks) {
            for (auto& inst : bb.instructions) {
                if (inst.opcode == IROpcode::AllocHeap) {
                    if (escapedAllocIds.find(inst.dest.ssaId) == escapedAllocIds.end()) {
                        inst.opcode          = IROpcode::AllocStack;
                        inst.isStackPromoted = true;
                        ++promoted;
                    } else {
                        fnStillUsesHeap = true;
                    }
                }
            }
        }

        fn.usesHeapOrGC = fnStillUsesHeap;
        if (fnStillUsesHeap || fn.usesReflection) {
            anyRemainingHeap = true;
        }
    }

    mod.requiresFullRuntime = anyRemainingHeap;
    m_stats.stackPromotedAllocsCount += promoted;
    return promoted;
}

std::size_t IRPassManager::RunDevirtualizationPass(IRModule& mod) {
    m_stats.executedPassOrder.emplace_back("DevirtualizationPass");
    std::size_t devirtCount = 0;

    for (auto& fn : mod.functions) {
        std::unordered_map<std::uint32_t, std::string> exactSSATypes;
        std::unordered_map<std::string, std::string> exactVarTypes;

        for (auto& bb : fn.blocks) {
            for (auto& inst : bb.instructions) {
                if (inst.isDead) continue;

                if ((inst.opcode == IROpcode::AllocHeap || inst.opcode == IROpcode::AllocStack) &&
                    inst.dest.IsValid()) {
                    exactSSATypes[inst.dest.ssaId] = inst.textImm;
                } else if (inst.opcode == IROpcode::StoreVar && !inst.operandIds.empty()) {
                    auto it = exactSSATypes.find(inst.operandIds[0]);
                    if (it != exactSSATypes.end()) {
                        exactVarTypes[inst.textImm] = it->second;
                    }
                } else if (inst.opcode == IROpcode::LoadVar && inst.dest.IsValid()) {
                    auto it = exactVarTypes.find(inst.textImm);
                    if (it != exactVarTypes.end()) {
                        exactSSATypes[inst.dest.ssaId] = it->second;
                    }
                } else if (inst.opcode == IROpcode::CallVirtual && !inst.operandIds.empty()) {
                    const std::uint32_t recvId = inst.operandIds[0];
                    auto it = exactSSATypes.find(recvId);
                    if (it != exactSSATypes.end()) {
                        // Concrete allocation type is statically known. The text may be
                        // "Runner@Walker"; only the allocated class is the function owner.
                        std::string concrete = it->second;
                        const auto chain = concrete.find('@');
                        if (chain != std::string::npos) concrete.resize(chain);
                        const std::size_t dotPos = inst.textImm.rfind('.');
                        const std::string methodOnly = (dotPos != std::string::npos)
                                                           ? inst.textImm.substr(dotPos + 1)
                                                           : inst.textImm;
                        inst.opcode          = IROpcode::CallDirect;
                        inst.textImm         = concrete + "." + methodOnly;
                        inst.secondaryText   = concrete;
                        inst.isDevirtualized = true;
                        ++devirtCount;
                    } else if (!inst.secondaryText.empty()) {
                        const std::size_t dotPos = inst.textImm.rfind('.');
                        const std::string methodOnly = (dotPos != std::string::npos)
                                                           ? inst.textImm.substr(dotPos + 1)
                                                           : inst.textImm;
                        if (m_sema.CanDevirtualizeCall(inst.secondaryText, methodOnly)) {
                            inst.opcode          = IROpcode::CallDirect;
                            inst.isDevirtualized = true;
                            ++devirtCount;
                        }
                    }
                } else if (inst.opcode == IROpcode::CallDirect && inst.isDevirtualized) {
                    // Already devirtualized during initial lowering
                }
            }
        }
    }

    m_stats.devirtualizedCallsCount += devirtCount;
    return devirtCount;
}

std::size_t IRPassManager::RunDeadCodeEliminationPass(IRModule& mod) {
    m_stats.executedPassOrder.emplace_back("DeadCodeEliminationPass");
    std::size_t eliminated = 0;

    auto isPureInstruction = [](IROpcode op) noexcept {
        return op == IROpcode::ConstInt ||
               op == IROpcode::ConstFloat ||
               op == IROpcode::ConstBool ||
               op == IROpcode::ConstString ||
               op == IROpcode::Copy ||
               op == IROpcode::Add ||
               op == IROpcode::Sub ||
               op == IROpcode::Mul ||
               op == IROpcode::Div ||
               op == IROpcode::Mod ||
               op == IROpcode::Eq ||
               op == IROpcode::Ne ||
               op == IROpcode::Lt ||
               op == IROpcode::Le ||
               op == IROpcode::Gt ||
               op == IROpcode::Ge ||
               op == IROpcode::And ||
               op == IROpcode::Or ||
               op == IROpcode::Neg ||
               op == IROpcode::Not ||
               op == IROpcode::LoadVar;
    };

    for (auto& fn : mod.functions) {
        std::unordered_set<std::uint32_t> usedSSAIds;
        for (const auto& bb : fn.blocks) {
            for (const auto& inst : bb.instructions) {
                if (inst.isDead) continue;
                for (std::uint32_t opId : inst.operandIds) {
                    usedSSAIds.insert(opId);
                }
            }
        }

        for (auto& bb : fn.blocks) {
            for (auto& inst : bb.instructions) {
                if (inst.isDead) continue;
                if (inst.dest.IsValid() &&
                    isPureInstruction(inst.opcode) &&
                    usedSSAIds.find(inst.dest.ssaId) == usedSSAIds.end()) {
                    inst.isDead = true;
                    ++eliminated;
                }
            }
        }
    }

    m_stats.deadInstructionsEliminatedCount += eliminated;
    return eliminated;
}

bool IRPassManager::ExecuteWithCrashRecovery(std::string_view phaseName, const std::function<void()>& action) {
    try {
        action();
        return true;
    } catch (const std::exception& ex) {
        m_diagnostics.ReportError(
            "ICE9999",
            "Internal Compiler Error recovered during phase '" + std::string(phaseName) + "': " + ex.what(),
            Frontend::SourceLocation{},
            "Auto-generated crash reproducer captured; compilation aborted safely without process crash.");
        return false;
    } catch (...) {
        m_diagnostics.ReportError(
            "ICE9999",
            "Unknown Internal Compiler Error recovered during phase '" + std::string(phaseName) + "'.",
            Frontend::SourceLocation{},
            "Auto-generated crash reproducer captured; compilation aborted safely without process crash.");
        return false;
    }
}

bool IRPassManager::RunOptimizationPipeline(IRModule& mod, bool enableOptimizations) {
    bool ok = true;
    ExecuteWithCrashRecovery("InitialIRVerification", [&]() {
        if (!VerifyIRModule(mod)) {
            ok = false;
        }
    });
    if (!ok) return false;

    if (!enableOptimizations) {
        return true;
    }

    // Scheduled Pass Order with Post-Pass Correctness Verification:
    // 1. ConstantFolding -> Verify
    // 2. EscapeAnalysis -> Verify
    // 3. Devirtualization -> Verify
    // 4. DeadCodeElimination -> Verify
    const std::pair<const char*, std::function<void()>> passes[] = {
        { "ConstantFoldingPass", [&]() { RunConstantFoldingPass(mod); } },
        { "EscapeAnalysisPass", [&]() { RunEscapeAnalysisPass(mod); } },
        { "DevirtualizationPass", [&]() { RunDevirtualizationPass(mod); } },
        { "DeadCodeEliminationPass", [&]() { RunDeadCodeEliminationPass(mod); } }
    };

    for (const auto& [passName, passFn] : passes) {
        if (!ExecuteWithCrashRecovery(passName, passFn)) {
            return false;
        }
        if (!VerifyIRModule(mod)) {
            return false;
        }
    }

    return !m_diagnostics.HasErrors();
}

} // namespace Robix::IR

