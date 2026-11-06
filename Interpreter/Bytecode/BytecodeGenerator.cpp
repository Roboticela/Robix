#include "BytecodeModule.hpp"

#include <unordered_map>

namespace Robix::VM {

const BytecodeChunk* BytecodeModule::FindChunk(std::string_view name) const noexcept {
    for (const auto& chunk : chunks) {
        if (chunk.name == name) {
            return &chunk;
        }
    }
    return nullptr;
}

BytecodeGenerator::BytecodeGenerator(Frontend::DiagnosticBag& diagnostics) noexcept
    : m_diagnostics(diagnostics) {}

BytecodeModule BytecodeGenerator::GenerateFromIR(const IR::IRModule& irMod) {
    BytecodeModule bcMod;
    bcMod.moduleName = irMod.moduleName;

    for (const auto& irFn : irMod.functions) {
        if (irFn.isExtern) continue;
        bcMod.chunks.push_back(LowerIRFunction(irFn));
    }

    return bcMod;
}

BytecodeChunk BytecodeGenerator::LowerIRFunction(const IR::IRFunction& irFn) {
    BytecodeChunk chunk;
    chunk.name           = irFn.name;
    chunk.mangledName    = irFn.mangledName;
    chunk.returnTypeName = irFn.returnType.ToString();
    chunk.isTestChunk    = irFn.isTestBlock;

    for (const auto& p : irFn.parameters) {
        chunk.paramNames.push_back(p.name);
    }

    std::unordered_map<std::string, std::uint32_t> blockStartPC;
    struct PendingPatch {
        std::size_t instIndex = 0;
        std::string targetLabel;
    };
    std::vector<PendingPatch> patches;

    for (const auto& bb : irFn.blocks) {
        blockStartPC[bb.label] = static_cast<std::uint32_t>(chunk.code.size());

        for (const auto& inst : bb.instructions) {
            if (inst.isDead) continue;

            BCInstruction bc;
            bc.destReg    = inst.dest.ssaId;
            bc.srcReg1    = !inst.operandIds.empty() ? inst.operandIds[0] : 0;
            bc.srcReg2    = (inst.operandIds.size() > 1) ? inst.operandIds[1] : 0;
            bc.intImm     = inst.intImm;
            bc.floatImm   = inst.floatImm;
            bc.boolImm    = inst.boolImm;
            bc.symbolImm  = inst.textImm;
            bc.sourceLine = inst.location.line;

            switch (inst.opcode) {
                case IR::IROpcode::Nop:             bc.opcode = BCOpcode::Nop; break;
                case IR::IROpcode::ConstInt:        bc.opcode = BCOpcode::LoadConstInt; break;
                case IR::IROpcode::ConstFloat:      bc.opcode = BCOpcode::LoadConstFloat; break;
                case IR::IROpcode::ConstBool:       bc.opcode = BCOpcode::LoadConstBool; break;
                case IR::IROpcode::ConstString:     bc.opcode = BCOpcode::LoadConstStr; break;
                case IR::IROpcode::Copy:            bc.opcode = BCOpcode::CopyReg; break;
                case IR::IROpcode::Add:             bc.opcode = BCOpcode::Add; break;
                case IR::IROpcode::Sub:             bc.opcode = BCOpcode::Sub; break;
                case IR::IROpcode::Mul:             bc.opcode = BCOpcode::Mul; break;
                case IR::IROpcode::Div:             bc.opcode = BCOpcode::Div; break;
                case IR::IROpcode::Mod:             bc.opcode = BCOpcode::Mod; break;
                case IR::IROpcode::Eq:              bc.opcode = BCOpcode::Eq; break;
                case IR::IROpcode::Ne:              bc.opcode = BCOpcode::Ne; break;
                case IR::IROpcode::Lt:              bc.opcode = BCOpcode::Lt; break;
                case IR::IROpcode::Le:              bc.opcode = BCOpcode::Le; break;
                case IR::IROpcode::Gt:              bc.opcode = BCOpcode::Gt; break;
                case IR::IROpcode::Ge:              bc.opcode = BCOpcode::Ge; break;
                case IR::IROpcode::And:             bc.opcode = BCOpcode::And; break;
                case IR::IROpcode::Or:              bc.opcode = BCOpcode::Or; break;
                case IR::IROpcode::Neg:             bc.opcode = BCOpcode::Neg; break;
                case IR::IROpcode::Not:             bc.opcode = BCOpcode::Not; break;
                case IR::IROpcode::AllocStack:
                case IR::IROpcode::AllocHeap:       bc.opcode = BCOpcode::AllocObj; bc.callArgRegs = inst.operandIds; break;
                case IR::IROpcode::LoadVar:         bc.opcode = BCOpcode::LoadVar; break;
                case IR::IROpcode::StoreVar:        bc.opcode = BCOpcode::StoreVar; break;
                case IR::IROpcode::GetField:        bc.opcode = BCOpcode::GetField; break;
                case IR::IROpcode::SetField:        bc.opcode = BCOpcode::SetField; break;
                case IR::IROpcode::CallDirect:
                case IR::IROpcode::CallVirtual:     bc.opcode = BCOpcode::CallFn; bc.callArgRegs = inst.operandIds; break;
                case IR::IROpcode::CallOSIntrinsic: bc.opcode = BCOpcode::CallOSIntrinsic; bc.callArgRegs = inst.operandIds; break;
                case IR::IROpcode::PrintVal:        bc.opcode = BCOpcode::PrintReg; break;
                case IR::IROpcode::AssertVal:       bc.opcode = BCOpcode::AssertReg; bc.callArgRegs = inst.operandIds; break;
                case IR::IROpcode::ReturnVal:       bc.opcode = BCOpcode::ReturnReg; break;
                case IR::IROpcode::ReturnVoid:      bc.opcode = BCOpcode::ReturnVoid; break;
                case IR::IROpcode::DeferCleanup:    bc.opcode = BCOpcode::DeferCleanup; break;
                case IR::IROpcode::InlineASM:       bc.opcode = BCOpcode::InlineASM; break;
                case IR::IROpcode::LoadIndex:       bc.opcode = BCOpcode::LoadIndex; break;
                case IR::IROpcode::StoreIndex:      bc.opcode = BCOpcode::StoreIndex; bc.callArgRegs = inst.operandIds; break;
                case IR::IROpcode::AddrOf:          bc.opcode = BCOpcode::AddrOf; break;
                case IR::IROpcode::PtrLoad:         bc.opcode = BCOpcode::PtrLoad; break;
                case IR::IROpcode::PtrStore:        bc.opcode = BCOpcode::PtrStore; break;
                case IR::IROpcode::PtrOffset:       bc.opcode = BCOpcode::PtrOffset; break;
                case IR::IROpcode::SliceCopy:       bc.opcode = BCOpcode::SliceCopy; bc.callArgRegs = inst.operandIds; break;

                case IR::IROpcode::Jump: {
                    bc.opcode = BCOpcode::Jump;
                    patches.push_back(PendingPatch{ chunk.code.size(), inst.textImm });
                    break;
                }

                case IR::IROpcode::BranchCond: {
                    // Emit JumpIfFalse to falseLabel (secondaryText), followed by Jump to trueLabel (textImm)
                    bc.opcode = BCOpcode::JumpIfFalse;
                    patches.push_back(PendingPatch{ chunk.code.size(), inst.secondaryText });
                    chunk.code.push_back(std::move(bc));

                    BCInstruction jmpTrue;
                    jmpTrue.opcode     = BCOpcode::Jump;
                    jmpTrue.sourceLine = inst.location.line;
                    patches.push_back(PendingPatch{ chunk.code.size(), inst.textImm });
                    chunk.code.push_back(std::move(jmpTrue));
                    continue;
                }
            }

            chunk.code.push_back(std::move(bc));
        }
    }

    for (const auto& patch : patches) {
        auto it = blockStartPC.find(patch.targetLabel);
        if (it != blockStartPC.end()) {
            chunk.code[patch.instIndex].targetPC = it->second;
        }
    }

    return chunk;
}

bool BytecodeGenerator::VerifyBytecodeModule(const BytecodeModule& bcMod) {
    if (bcMod.magic != BYTECODE_MAGIC || bcMod.version != BYTECODE_VERSION) {
        m_diagnostics.ReportError(
            "V4001",
            "Bytecode Verification failed: Invalid magic header or unsupported bytecode version.",
            Frontend::SourceLocation{});
        return false;
    }

    bool valid = true;
    for (const auto& chunk : bcMod.chunks) {
        if (chunk.code.empty()) {
            m_diagnostics.ReportError(
                "V4001",
                "Bytecode Verification failed: Chunk '" + chunk.name + "' contains 0 instructions.",
                Frontend::SourceLocation{});
            valid = false;
            continue;
        }
        for (const auto& inst : chunk.code) {
            if (inst.opcode == BCOpcode::Jump || inst.opcode == BCOpcode::JumpIfFalse) {
                if (inst.targetPC >= chunk.code.size()) {
                    m_diagnostics.ReportError(
                        "V4001",
                        "Bytecode Verification failed: Out-of-bounds jump target PC in chunk '" + chunk.name + "'.",
                        Frontend::SourceLocation{});
                    valid = false;
                }
            }
        }
    }

    return valid;
}

} // namespace Robix::VM

