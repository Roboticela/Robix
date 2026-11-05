#include "SSAIR.hpp"
#include "../Types/SpecValidator.hpp"

#include <cstdlib>
#include <sstream>

namespace Robix::IR {

namespace {

std::int64_t ParseIntLiteral(std::string_view text) {
    std::string cleaned;
    cleaned.reserve(text.size());
    for (char c : text) {
        if (c != '_') cleaned.push_back(c);
    }
    if (cleaned.size() > 2 && cleaned[0] == '0' && (cleaned[1] == 'x' || cleaned[1] == 'X')) {
        return std::strtoll(cleaned.c_str() + 2, nullptr, 16);
    }
    if (cleaned.size() > 2 && cleaned[0] == '0' && (cleaned[1] == 'b' || cleaned[1] == 'B')) {
        return std::strtoll(cleaned.c_str() + 2, nullptr, 2);
    }
    return std::strtoll(cleaned.c_str(), nullptr, 10);
}

double ParseFloatLiteral(std::string_view text) {
    std::string cleaned;
    cleaned.reserve(text.size());
    for (char c : text) {
        if (c != '_') cleaned.push_back(c);
    }
    return std::strtod(cleaned.c_str(), nullptr);
}

std::string Unquote(std::string_view text) {
    if (text.size() >= 6 && text.substr(0, 3) == "\"\"\"" && text.substr(text.size() - 3) == "\"\"\"") {
        return std::string(text.substr(3, text.size() - 6));
    }
    if (text.size() >= 3 && (text[0] == '$' || text[0] == 'r') && text[1] == '"' && text.back() == '"') {
        return std::string(text.substr(2, text.size() - 3));
    }
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return std::string(text.substr(1, text.size() - 2));
    }
    return std::string(text);
}

Frontend::TypeNode MakePrim(const std::string& name) {
    Frontend::TypeNode t;
    t.baseName = name;
    return t;
}

} // namespace

std::string_view IROpcodeToString(IROpcode op) noexcept {
    switch (op) {
        case IROpcode::Nop:             return "nop";
        case IROpcode::ConstInt:        return "const.int";
        case IROpcode::ConstFloat:      return "const.float";
        case IROpcode::ConstBool:       return "const.bool";
        case IROpcode::ConstString:     return "const.str";
        case IROpcode::Copy:            return "copy";
        case IROpcode::Add:             return "add";
        case IROpcode::Sub:             return "sub";
        case IROpcode::Mul:             return "mul";
        case IROpcode::Div:             return "div";
        case IROpcode::Mod:             return "mod";
        case IROpcode::Eq:              return "cmp.eq";
        case IROpcode::Ne:              return "cmp.ne";
        case IROpcode::Lt:              return "cmp.lt";
        case IROpcode::Le:              return "cmp.le";
        case IROpcode::Gt:              return "cmp.gt";
        case IROpcode::Ge:              return "cmp.ge";
        case IROpcode::And:             return "and";
        case IROpcode::Or:              return "or";
        case IROpcode::Neg:             return "neg";
        case IROpcode::Not:             return "not";
        case IROpcode::AllocStack:      return "alloc.stack";
        case IROpcode::AllocHeap:       return "alloc.heap";
        case IROpcode::LoadVar:         return "load.var";
        case IROpcode::StoreVar:        return "store.var";
        case IROpcode::GetField:        return "get.field";
        case IROpcode::SetField:        return "set.field";
        case IROpcode::CallDirect:      return "call.direct";
        case IROpcode::CallVirtual:     return "call.virtual";
        case IROpcode::CallOSIntrinsic: return "call.os_intrinsic";
        case IROpcode::PrintVal:        return "print";
        case IROpcode::AssertVal:       return "assert";
        case IROpcode::Jump:            return "jmp";
        case IROpcode::BranchCond:      return "br.cond";
        case IROpcode::ReturnVal:       return "ret.val";
        case IROpcode::ReturnVoid:      return "ret.void";
        case IROpcode::DeferCleanup:    return "defer.cleanup";
        case IROpcode::InlineASM:       return "inline.asm";
    }
    return "unknown";
}

bool IsTerminatorOpcode(IROpcode op) noexcept {
    return op == IROpcode::Jump ||
           op == IROpcode::BranchCond ||
           op == IROpcode::ReturnVal ||
           op == IROpcode::ReturnVoid;
}

const IRFunction* IRModule::FindFunction(std::string_view name) const noexcept {
    for (const auto& fn : functions) {
        if (fn.name == name) {
            return &fn;
        }
    }
    return nullptr;
}

IRFunction* IRModule::FindFunctionMutable(std::string_view name) noexcept {
    for (auto& fn : functions) {
        if (fn.name == name) {
            return &fn;
        }
    }
    return nullptr;
}

std::string IRModule::DumpText() const {
    std::ostringstream out;
    out << "; Robix SSA IR Module: " << moduleName << " (Package: " << packageName << ")\n";
    for (const auto& fn : functions) {
        out << "\nfunc @" << fn.name << " (" << fn.mangledName << ") -> " << fn.returnType.ToString() << " {\n";
        for (const auto& bb : fn.blocks) {
            out << bb.label << ":\n";
            for (const auto& inst : bb.instructions) {
                if (inst.isDead) continue;
                out << "  ";
                if (inst.dest.IsValid()) {
                    out << inst.dest.ToString() << " : " << inst.dest.type.ToString() << " = ";
                }
                out << IROpcodeToString(inst.opcode);
                for (std::uint32_t opId : inst.operandIds) {
                    out << " %" << opId;
                }
                if (!inst.textImm.empty()) {
                    out << " [" << inst.textImm << "]";
                }
                if (!inst.secondaryText.empty()) {
                    out << " [" << inst.secondaryText << "]";
                }
                if (inst.opcode == IROpcode::ConstInt) {
                    out << " " << inst.intImm;
                } else if (inst.opcode == IROpcode::ConstFloat) {
                    out << " " << inst.floatImm;
                } else if (inst.opcode == IROpcode::ConstBool) {
                    out << " " << (inst.boolImm ? "true" : "false");
                }
                if (inst.isDevirtualized) out << " ; [devirtualized]";
                if (inst.isStackPromoted) out << " ; [stack-promoted]";
                out << "\n";
            }
        }
        out << "}\n";
    }
    return out.str();
}

SSAIRBuilder::SSAIRBuilder(const Frontend::SemanticAnalyzer& sema, Frontend::DiagnosticBag& diagnostics)
    : m_sema(sema), m_diagnostics(diagnostics) {}

std::size_t SSAIRBuilder::CreateBasicBlock(const std::string& prefix) {
    std::string label = prefix;
    if (prefix != "entry") {
        label += "_" + std::to_string(++m_blockCounter);
    }
    m_currentFn->blocks.push_back(IRBasicBlock{ label, {}, {}, {} });
    return m_currentFn->blocks.size() - 1;
}

IRBasicBlock& SSAIRBuilder::CurrentBlock() {
    return m_currentFn->blocks[m_currentBlockIdx];
}

bool SSAIRBuilder::CurrentBlockHasTerminator() const {
    const auto& insts = m_currentFn->blocks[m_currentBlockIdx].instructions;
    return !insts.empty() && IsTerminatorOpcode(insts.back().opcode);
}

void SSAIRBuilder::Emit(IRInstruction inst) {
    if (!CurrentBlockHasTerminator()) {
        CurrentBlock().instructions.push_back(std::move(inst));
    }
}

IRModule SSAIRBuilder::LowerCompilationUnit(const Frontend::CompilationUnitAST& unit) {
    IRModule mod;
    mod.moduleName  = unit.fileName;
    mod.packageName = unit.packageName.empty() ? "Default" : unit.packageName;

    for (const auto& declPtr : unit.declarations) {
        if (!declPtr) continue;
        const auto& decl = *declPtr;

        if (decl.kind == Frontend::DeclKind::Function) {
            LowerFunctionOrMethod(decl, "", mod);
        } else if (decl.kind == Frontend::DeclKind::Class || decl.kind == Frontend::DeclKind::Struct) {
            for (const auto& memberPtr : decl.members) {
                if (!memberPtr) continue;
                if (memberPtr->kind == Frontend::DeclKind::Function ||
                    memberPtr->kind == Frontend::DeclKind::Constructor) {
                    LowerFunctionOrMethod(*memberPtr, decl.name, mod);
                }
            }
        } else if (decl.kind == Frontend::DeclKind::Test) {
            LowerTestDecl(decl, mod);
        } else if (decl.kind == Frontend::DeclKind::TestFile) {
            for (const auto& memberPtr : decl.members) {
                if (memberPtr && memberPtr->kind == Frontend::DeclKind::Test) {
                    LowerTestDecl(*memberPtr, mod);
                }
            }
        }
    }

    for (const auto& fn : mod.functions) {
        if (fn.usesHeapOrGC || fn.usesReflection) {
            mod.requiresFullRuntime = true;
            break;
        }
    }

    return mod;
}

void SSAIRBuilder::LowerFunctionOrMethod(const Frontend::DeclNode& decl,
                                         const std::string& enclosingClass,
                                         IRModule& outModule) {
    if (!decl.body) {
        return; // Skip bodyless interface/abstract declarations
    }

    IRFunction fn;
    fn.name               = enclosingClass.empty() ? decl.name : (enclosingClass + "." + decl.name);
    fn.enclosingClassName = enclosingClass;
    fn.returnType         = decl.returnType.IsEmpty() ? MakePrim("Void") : decl.returnType;
    fn.location           = decl.location;

    (void)m_diagnostics;
    std::string paramCodes;
    for (const auto& p : decl.parameters) {
        fn.parameters.push_back(Frontend::FunctionParamInfo{
            p.name,
            p.type.IsEmpty() ? MakePrim("Int") : p.type,
            p.defaultValue != nullptr,
            p.isMutable,
            p.isVariadic
        });
        Spec::PrimitiveKind pk{};
        if (Spec::TryResolvePrimitiveName(p.type.baseName, pk)) {
            paramCodes += Spec::GetPrimitiveLayout(pk).mangleCode;
        } else {
            paramCodes += "v";
        }
    }

    std::string retCode = "v";
    Spec::PrimitiveKind retPk{};
    if (Spec::TryResolvePrimitiveName(fn.returnType.baseName, retPk)) {
        retCode = std::string(Spec::GetPrimitiveLayout(retPk).mangleCode);
    }

    fn.mangledName = Spec::MangleRobixSymbol(
        outModule.packageName,
        enclosingClass,
        decl.name,
        paramCodes,
        retCode);

    for (const auto& attr : decl.attributes) {
        if (attr.name == "RealTime") {
            fn.isRealTime = true;
        }
    }

    outModule.functions.push_back(std::move(fn));
    m_currentFn       = &outModule.functions.back();
    m_blockCounter    = 0;
    m_currentBlockIdx = CreateBasicBlock("entry");

    LowerStmt(*decl.body);

    if (!CurrentBlockHasTerminator()) {
        IRInstruction ret;
        ret.opcode   = IROpcode::ReturnVoid;
        ret.location = decl.location;
        Emit(std::move(ret));
    }

    m_currentFn = nullptr;
}

void SSAIRBuilder::LowerTestDecl(const Frontend::DeclNode& testDecl, IRModule& outModule) {
    if (!testDecl.body) return;

    IRFunction fn;
    fn.name        = "Test::" + testDecl.testDescription;
    fn.mangledName = "_RBX1_Test_" + std::to_string(outModule.functions.size() + 1);
    fn.returnType  = MakePrim("Void");
    fn.isTestBlock = true;
    fn.location    = testDecl.location;

    outModule.functions.push_back(std::move(fn));
    m_currentFn       = &outModule.functions.back();
    m_blockCounter    = 0;
    m_currentBlockIdx = CreateBasicBlock("entry");

    LowerStmt(*testDecl.body);

    if (!CurrentBlockHasTerminator()) {
        IRInstruction ret;
        ret.opcode   = IROpcode::ReturnVoid;
        ret.location = testDecl.location;
        Emit(std::move(ret));
    }

    m_currentFn = nullptr;
}

void SSAIRBuilder::LowerStmt(const Frontend::StmtNode& stmt) {
    switch (stmt.kind) {
        case Frontend::StmtKind::Block: {
            for (const auto& s : stmt.statements) {
                if (s) LowerStmt(*s);
            }
            break;
        }

        case Frontend::StmtKind::VarDecl: {
            IRValue initVal{};
            if (stmt.initializer) {
                initVal = LowerExpr(*stmt.initializer);
            } else {
                IRInstruction zero;
                zero.opcode   = IROpcode::ConstInt;
                zero.dest     = IRValue{ m_currentFn->AllocateSSAId(), stmt.declaredType.IsEmpty() ? MakePrim("Int") : stmt.declaredType };
                zero.intImm   = 0;
                zero.location = stmt.location;
                initVal       = zero.dest;
                Emit(std::move(zero));
            }

            IRInstruction store;
            store.opcode      = IROpcode::StoreVar;
            store.textImm     = stmt.varName;
            store.secondaryText = stmt.declaredType.IsEmpty() ? std::string{} : stmt.declaredType.baseName;
            store.operandIds  = { initVal.ssaId };
            store.location    = stmt.location;
            Emit(std::move(store));
            break;
        }

        case Frontend::StmtKind::IfStmt: {
            IRValue condVal = stmt.condition ? LowerExpr(*stmt.condition) : IRValue{};
            const std::size_t thenIdx  = CreateBasicBlock("if_then");
            const std::size_t elseIdx  = stmt.elseBranch ? CreateBasicBlock("if_else") : 0;
            const std::size_t mergeIdx = CreateBasicBlock("if_merge");

            const std::string thenLabel  = m_currentFn->blocks[thenIdx].label;
            const std::string mergeLabel = m_currentFn->blocks[mergeIdx].label;
            const std::string falseLabel = stmt.elseBranch ? m_currentFn->blocks[elseIdx].label : mergeLabel;

            IRInstruction br;
            br.opcode        = IROpcode::BranchCond;
            br.operandIds    = { condVal.ssaId };
            br.textImm       = thenLabel;
            br.secondaryText = falseLabel;
            br.location      = stmt.location;
            Emit(std::move(br));

            // Then block
            m_currentBlockIdx = thenIdx;
            if (stmt.thenBranch) LowerStmt(*stmt.thenBranch);
            if (!CurrentBlockHasTerminator()) {
                IRInstruction jmp;
                jmp.opcode   = IROpcode::Jump;
                jmp.textImm  = mergeLabel;
                jmp.location = stmt.location;
                Emit(std::move(jmp));
            }

            // Else block
            if (stmt.elseBranch) {
                m_currentBlockIdx = elseIdx;
                LowerStmt(*stmt.elseBranch);
                if (!CurrentBlockHasTerminator()) {
                    IRInstruction jmp;
                    jmp.opcode   = IROpcode::Jump;
                    jmp.textImm  = mergeLabel;
                    jmp.location = stmt.location;
                    Emit(std::move(jmp));
                }
            }

            m_currentBlockIdx = mergeIdx;
            break;
        }

        case Frontend::StmtKind::WhileStmt: {
            const std::size_t condIdx = CreateBasicBlock("while_cond");
            const std::size_t bodyIdx = CreateBasicBlock("while_body");
            const std::size_t exitIdx = CreateBasicBlock("while_exit");

            const std::string condLabel = m_currentFn->blocks[condIdx].label;
            const std::string bodyLabel = m_currentFn->blocks[bodyIdx].label;
            const std::string exitLabel = m_currentFn->blocks[exitIdx].label;

            IRInstruction jmpToCond;
            jmpToCond.opcode   = IROpcode::Jump;
            jmpToCond.textImm  = condLabel;
            jmpToCond.location = stmt.location;
            Emit(std::move(jmpToCond));

            m_currentBlockIdx = condIdx;
            IRValue condVal = stmt.condition ? LowerExpr(*stmt.condition) : IRValue{};
            IRInstruction br;
            br.opcode        = IROpcode::BranchCond;
            br.operandIds    = { condVal.ssaId };
            br.textImm       = bodyLabel;
            br.secondaryText = exitLabel;
            br.location      = stmt.location;
            Emit(std::move(br));

            m_currentBlockIdx = bodyIdx;
            m_loopTargets.push_back(LoopTargets{ exitLabel, condLabel });
            if (stmt.thenBranch) LowerStmt(*stmt.thenBranch);
            m_loopTargets.pop_back();
            if (!CurrentBlockHasTerminator()) {
                IRInstruction backJmp;
                backJmp.opcode   = IROpcode::Jump;
                backJmp.textImm  = condLabel;
                backJmp.location = stmt.location;
                Emit(std::move(backJmp));
            }

            m_currentBlockIdx = exitIdx;
            break;
        }

        case Frontend::StmtKind::DoWhileStmt: {
            const std::size_t bodyIdx = CreateBasicBlock("do_body");
            const std::size_t condIdx = CreateBasicBlock("do_cond");
            const std::size_t exitIdx = CreateBasicBlock("do_exit");

            const std::string bodyLabel = m_currentFn->blocks[bodyIdx].label;
            const std::string condLabel = m_currentFn->blocks[condIdx].label;
            const std::string exitLabel = m_currentFn->blocks[exitIdx].label;

            IRInstruction jmpToBody;
            jmpToBody.opcode   = IROpcode::Jump;
            jmpToBody.textImm  = bodyLabel;
            jmpToBody.location = stmt.location;
            Emit(std::move(jmpToBody));

            m_currentBlockIdx = bodyIdx;
            m_loopTargets.push_back(LoopTargets{ exitLabel, condLabel });
            if (stmt.thenBranch) LowerStmt(*stmt.thenBranch);
            m_loopTargets.pop_back();
            if (!CurrentBlockHasTerminator()) {
                IRInstruction jmpCond;
                jmpCond.opcode   = IROpcode::Jump;
                jmpCond.textImm  = condLabel;
                jmpCond.location = stmt.location;
                Emit(std::move(jmpCond));
            }

            m_currentBlockIdx = condIdx;
            IRValue condVal = stmt.condition ? LowerExpr(*stmt.condition) : IRValue{};
            IRInstruction br;
            br.opcode        = IROpcode::BranchCond;
            br.operandIds    = { condVal.ssaId };
            br.textImm       = bodyLabel;
            br.secondaryText = exitLabel;
            br.location      = stmt.location;
            Emit(std::move(br));

            m_currentBlockIdx = exitIdx;
            break;
        }

        case Frontend::StmtKind::LoopStmt: {
            const std::size_t bodyIdx = CreateBasicBlock("loop_body");
            const std::size_t exitIdx = CreateBasicBlock("loop_exit");

            const std::string bodyLabel = m_currentFn->blocks[bodyIdx].label;
            const std::string exitLabel = m_currentFn->blocks[exitIdx].label;

            IRInstruction jmpToBody;
            jmpToBody.opcode   = IROpcode::Jump;
            jmpToBody.textImm  = bodyLabel;
            jmpToBody.location = stmt.location;
            Emit(std::move(jmpToBody));

            m_currentBlockIdx = bodyIdx;
            m_loopTargets.push_back(LoopTargets{ exitLabel, bodyLabel });
            if (stmt.thenBranch) LowerStmt(*stmt.thenBranch);
            m_loopTargets.pop_back();
            if (!CurrentBlockHasTerminator()) {
                IRInstruction back;
                back.opcode   = IROpcode::Jump;
                back.textImm  = bodyLabel;
                back.location = stmt.location;
                Emit(std::move(back));
            }

            m_currentBlockIdx = exitIdx;
            break;
        }

        case Frontend::StmtKind::RepeatStmt: {
            // `repeat N { ... }` -> hidden counter loop evaluated once for N
            IRValue countVal = stmt.condition ? LowerExpr(*stmt.condition) : IRValue{};
            const std::string counterName = "__repeat_" + std::to_string(stmt.location.line) + "_" +
                                            std::to_string(stmt.location.column);

            IRInstruction zero;
            zero.opcode   = IROpcode::ConstInt;
            zero.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
            zero.intImm   = 0;
            zero.location = stmt.location;
            IRValue zeroVal = zero.dest;
            Emit(std::move(zero));

            IRInstruction storeZero;
            storeZero.opcode     = IROpcode::StoreVar;
            storeZero.textImm    = counterName;
            storeZero.operandIds = { zeroVal.ssaId };
            storeZero.location   = stmt.location;
            Emit(std::move(storeZero));

            const std::size_t condIdx = CreateBasicBlock("repeat_cond");
            const std::size_t bodyIdx = CreateBasicBlock("repeat_body");
            const std::size_t stepIdx = CreateBasicBlock("repeat_step");
            const std::size_t exitIdx = CreateBasicBlock("repeat_exit");

            const std::string condLabel = m_currentFn->blocks[condIdx].label;
            const std::string bodyLabel = m_currentFn->blocks[bodyIdx].label;
            const std::string stepLabel = m_currentFn->blocks[stepIdx].label;
            const std::string exitLabel = m_currentFn->blocks[exitIdx].label;

            IRInstruction jmpToCond;
            jmpToCond.opcode   = IROpcode::Jump;
            jmpToCond.textImm  = condLabel;
            jmpToCond.location = stmt.location;
            Emit(std::move(jmpToCond));

            m_currentBlockIdx = condIdx;
            IRInstruction loadCtr;
            loadCtr.opcode   = IROpcode::LoadVar;
            loadCtr.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
            loadCtr.textImm  = counterName;
            loadCtr.location = stmt.location;
            IRValue ctrVal   = loadCtr.dest;
            Emit(std::move(loadCtr));

            IRInstruction cmp;
            cmp.opcode     = IROpcode::Lt;
            cmp.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Bool") };
            cmp.operandIds = { ctrVal.ssaId, countVal.ssaId };
            cmp.location   = stmt.location;
            IRValue cmpVal = cmp.dest;
            Emit(std::move(cmp));

            IRInstruction br;
            br.opcode        = IROpcode::BranchCond;
            br.operandIds    = { cmpVal.ssaId };
            br.textImm       = bodyLabel;
            br.secondaryText = exitLabel;
            br.location      = stmt.location;
            Emit(std::move(br));

            m_currentBlockIdx = bodyIdx;
            m_loopTargets.push_back(LoopTargets{ exitLabel, stepLabel });
            if (stmt.thenBranch) LowerStmt(*stmt.thenBranch);
            m_loopTargets.pop_back();
            if (!CurrentBlockHasTerminator()) {
                IRInstruction toStep;
                toStep.opcode   = IROpcode::Jump;
                toStep.textImm  = stepLabel;
                toStep.location = stmt.location;
                Emit(std::move(toStep));
            }

            m_currentBlockIdx = stepIdx;
            IRInstruction loadCur;
            loadCur.opcode   = IROpcode::LoadVar;
            loadCur.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
            loadCur.textImm  = counterName;
            loadCur.location = stmt.location;
            IRValue curV     = loadCur.dest;
            Emit(std::move(loadCur));

            IRInstruction one;
            one.opcode   = IROpcode::ConstInt;
            one.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
            one.intImm   = 1;
            one.location = stmt.location;
            IRValue oneV = one.dest;
            Emit(std::move(one));

            IRInstruction nextAdd;
            nextAdd.opcode     = IROpcode::Add;
            nextAdd.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
            nextAdd.operandIds = { curV.ssaId, oneV.ssaId };
            nextAdd.location   = stmt.location;
            IRValue nextV      = nextAdd.dest;
            Emit(std::move(nextAdd));

            IRInstruction storeNext;
            storeNext.opcode     = IROpcode::StoreVar;
            storeNext.textImm    = counterName;
            storeNext.operandIds = { nextV.ssaId };
            storeNext.location   = stmt.location;
            Emit(std::move(storeNext));

            IRInstruction backJmp;
            backJmp.opcode   = IROpcode::Jump;
            backJmp.textImm  = condLabel;
            backJmp.location = stmt.location;
            Emit(std::move(backJmp));

            m_currentBlockIdx = exitIdx;
            break;
        }

        case Frontend::StmtKind::ForCStyle: {
            if (stmt.initStmt) LowerStmt(*stmt.initStmt);

            const std::size_t condIdx = CreateBasicBlock("for_cond");
            const std::size_t bodyIdx = CreateBasicBlock("for_body");
            const std::size_t stepIdx = CreateBasicBlock("for_step");
            const std::size_t exitIdx = CreateBasicBlock("for_exit");

            const std::string condLabel = m_currentFn->blocks[condIdx].label;
            const std::string bodyLabel = m_currentFn->blocks[bodyIdx].label;
            const std::string stepLabel = m_currentFn->blocks[stepIdx].label;
            const std::string exitLabel = m_currentFn->blocks[exitIdx].label;

            IRInstruction jmpToCond;
            jmpToCond.opcode   = IROpcode::Jump;
            jmpToCond.textImm  = condLabel;
            jmpToCond.location = stmt.location;
            Emit(std::move(jmpToCond));

            m_currentBlockIdx = condIdx;
            IRValue condVal;
            if (stmt.condition) {
                condVal = LowerExpr(*stmt.condition);
            } else {
                IRInstruction t;
                t.opcode   = IROpcode::ConstBool;
                t.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Bool") };
                t.boolImm  = true;
                t.location = stmt.location;
                condVal    = t.dest;
                Emit(std::move(t));
            }
            IRInstruction br;
            br.opcode        = IROpcode::BranchCond;
            br.operandIds    = { condVal.ssaId };
            br.textImm       = bodyLabel;
            br.secondaryText = exitLabel;
            br.location      = stmt.location;
            Emit(std::move(br));

            m_currentBlockIdx = bodyIdx;
            m_loopTargets.push_back(LoopTargets{ exitLabel, stepLabel });
            if (stmt.thenBranch) LowerStmt(*stmt.thenBranch);
            m_loopTargets.pop_back();
            if (!CurrentBlockHasTerminator()) {
                IRInstruction toStep;
                toStep.opcode   = IROpcode::Jump;
                toStep.textImm  = stepLabel;
                toStep.location = stmt.location;
                Emit(std::move(toStep));
            }

            m_currentBlockIdx = stepIdx;
            if (stmt.stepExpr) (void)LowerExpr(*stmt.stepExpr);
            IRInstruction backJmp;
            backJmp.opcode   = IROpcode::Jump;
            backJmp.textImm  = condLabel;
            backJmp.location = stmt.location;
            Emit(std::move(backJmp));

            m_currentBlockIdx = exitIdx;
            break;
        }

        case Frontend::StmtKind::ForRangeOrCollection: {
            if (stmt.condition && stmt.condition->kind == Frontend::ExprKind::Range &&
                stmt.condition->left && stmt.condition->right) {
                IRValue startVal = LowerExpr(*stmt.condition->left);
                IRValue endVal   = LowerExpr(*stmt.condition->right);
                IRValue stepVal{};
                if (stmt.stepExpr) {
                    stepVal = LowerExpr(*stmt.stepExpr);
                } else {
                    IRInstruction one;
                    one.opcode   = IROpcode::ConstInt;
                    one.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                    one.intImm   = 1;
                    one.location = stmt.location;
                    stepVal      = one.dest;
                    Emit(std::move(one));
                }

                IRInstruction storeInit;
                storeInit.opcode     = IROpcode::StoreVar;
                storeInit.textImm    = stmt.loopVar1;
                storeInit.operandIds = { startVal.ssaId };
                storeInit.location   = stmt.location;
                Emit(std::move(storeInit));

                const std::size_t condIdx = CreateBasicBlock("for_cond");
                const std::size_t bodyIdx = CreateBasicBlock("for_body");
                const std::size_t stepIdx = CreateBasicBlock("for_step");
                const std::size_t exitIdx = CreateBasicBlock("for_exit");

                const std::string condLabel = m_currentFn->blocks[condIdx].label;
                const std::string bodyLabel = m_currentFn->blocks[bodyIdx].label;
                const std::string stepLabel = m_currentFn->blocks[stepIdx].label;
                const std::string exitLabel = m_currentFn->blocks[exitIdx].label;

                IRInstruction jmpToCond;
                jmpToCond.opcode   = IROpcode::Jump;
                jmpToCond.textImm  = condLabel;
                jmpToCond.location = stmt.location;
                Emit(std::move(jmpToCond));

                m_currentBlockIdx = condIdx;
                IRInstruction loadIter;
                loadIter.opcode   = IROpcode::LoadVar;
                loadIter.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                loadIter.textImm  = stmt.loopVar1;
                loadIter.location = stmt.location;
                IRValue curIter   = loadIter.dest;
                Emit(std::move(loadIter));

                IRInstruction cmp;
                cmp.opcode     = stmt.condition->isInclusiveRange ? IROpcode::Le : IROpcode::Lt;
                cmp.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Bool") };
                cmp.operandIds = { curIter.ssaId, endVal.ssaId };
                cmp.location   = stmt.location;
                IRValue cmpVal = cmp.dest;
                Emit(std::move(cmp));

                IRInstruction br;
                br.opcode        = IROpcode::BranchCond;
                br.operandIds    = { cmpVal.ssaId };
                br.textImm       = bodyLabel;
                br.secondaryText = exitLabel;
                br.location      = stmt.location;
                Emit(std::move(br));

                m_currentBlockIdx = bodyIdx;
                m_loopTargets.push_back(LoopTargets{ exitLabel, stepLabel });
                if (stmt.thenBranch) LowerStmt(*stmt.thenBranch);
                m_loopTargets.pop_back();
                if (!CurrentBlockHasTerminator()) {
                    IRInstruction toStep;
                    toStep.opcode   = IROpcode::Jump;
                    toStep.textImm  = stepLabel;
                    toStep.location = stmt.location;
                    Emit(std::move(toStep));
                }

                m_currentBlockIdx = stepIdx;
                IRInstruction loadCur;
                loadCur.opcode   = IROpcode::LoadVar;
                loadCur.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                loadCur.textImm  = stmt.loopVar1;
                loadCur.location = stmt.location;
                IRValue curV     = loadCur.dest;
                Emit(std::move(loadCur));

                IRInstruction nextAdd;
                nextAdd.opcode     = IROpcode::Add;
                nextAdd.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                nextAdd.operandIds = { curV.ssaId, stepVal.ssaId };
                nextAdd.location   = stmt.location;
                IRValue nextV      = nextAdd.dest;
                Emit(std::move(nextAdd));

                IRInstruction storeNext;
                storeNext.opcode     = IROpcode::StoreVar;
                storeNext.textImm    = stmt.loopVar1;
                storeNext.operandIds = { nextV.ssaId };
                storeNext.location   = stmt.location;
                Emit(std::move(storeNext));

                IRInstruction backJmp;
                backJmp.opcode   = IROpcode::Jump;
                backJmp.textImm  = condLabel;
                backJmp.location = stmt.location;
                Emit(std::move(backJmp));

                m_currentBlockIdx = exitIdx;
            } else {
                m_diagnostics.ReportError(
                    "I4010",
                    "Collection iteration ('for x in collection') is not yet supported by the SSA IR backend.",
                    stmt.location,
                    "Use a numeric range loop ('for i in 0..N') or a 'while' loop.");
            }
            break;
        }

        case Frontend::StmtKind::BreakStmt: {
            if (m_loopTargets.empty()) {
                m_diagnostics.ReportError(
                    "I4012",
                    "'break' used outside of a loop.",
                    stmt.location,
                    "Move 'break' inside a for, while, do-while, repeat, or loop statement.");
            } else {
                IRInstruction jmp;
                jmp.opcode   = IROpcode::Jump;
                jmp.textImm  = m_loopTargets.back().breakLabel;
                jmp.location = stmt.location;
                Emit(std::move(jmp));
            }
            break;
        }

        case Frontend::StmtKind::ContinueStmt: {
            if (m_loopTargets.empty()) {
                m_diagnostics.ReportError(
                    "I4012",
                    "'continue' used outside of a loop.",
                    stmt.location,
                    "Move 'continue' inside a for, while, do-while, repeat, or loop statement.");
            } else {
                IRInstruction jmp;
                jmp.opcode   = IROpcode::Jump;
                jmp.textImm  = m_loopTargets.back().continueLabel;
                jmp.location = stmt.location;
                Emit(std::move(jmp));
            }
            break;
        }

        case Frontend::StmtKind::MatchStmt:
        case Frontend::StmtKind::TryCatchStmt:
        case Frontend::StmtKind::ThrowStmt: {
            m_diagnostics.ReportError(
                "I4010",
                "This statement kind (match/try/throw) is not yet supported by the SSA IR backend.",
                stmt.location,
                "Use if/else chains instead; match/try/throw lowering is not implemented yet.");
            break;
        }

        case Frontend::StmtKind::ReturnStmt: {
            if (stmt.condition) {
                IRValue retVal = LowerExpr(*stmt.condition);
                IRInstruction ret;
                ret.opcode     = IROpcode::ReturnVal;
                ret.operandIds = { retVal.ssaId };
                ret.location   = stmt.location;
                Emit(std::move(ret));
            } else {
                IRInstruction ret;
                ret.opcode   = IROpcode::ReturnVoid;
                ret.location = stmt.location;
                Emit(std::move(ret));
            }
            break;
        }

        case Frontend::StmtKind::DeferStmt:
        case Frontend::StmtKind::UsingStmt: {
            if (stmt.initializer) {
                IRValue v = LowerExpr(*stmt.initializer);
                if (!stmt.varName.empty()) {
                    IRInstruction st;
                    st.opcode     = IROpcode::StoreVar;
                    st.textImm    = stmt.varName;
                    st.operandIds = { v.ssaId };
                    st.location   = stmt.location;
                    Emit(std::move(st));
                }
            }
            if (stmt.thenBranch) {
                LowerStmt(*stmt.thenBranch);
            }
            IRInstruction def;
            def.opcode   = IROpcode::DeferCleanup;
            def.textImm  = stmt.varName.empty() ? "defer" : stmt.varName;
            def.location = stmt.location;
            Emit(std::move(def));
            break;
        }

        case Frontend::StmtKind::UnsafeStmt: {
            if (stmt.thenBranch) LowerStmt(*stmt.thenBranch);
            break;
        }

        case Frontend::StmtKind::ASMStmt: {
            IRInstruction asmInst;
            asmInst.opcode   = IROpcode::InlineASM;
            asmInst.textImm  = stmt.asmInstruction;
            asmInst.location = stmt.location;
            Emit(std::move(asmInst));
            break;
        }

        case Frontend::StmtKind::ExprStmt: {
            if (stmt.condition) {
                (void)LowerExpr(*stmt.condition);
            }
            break;
        }

        default:
            break;
    }
}

IRValue SSAIRBuilder::LowerExpr(const Frontend::ExprNode& expr) {
    Frontend::TypeNode exprType = expr.inferredType.IsEmpty() ? MakePrim("Int") : expr.inferredType;

    switch (expr.kind) {
        case Frontend::ExprKind::Literal: {
            IRInstruction inst;
            inst.dest     = IRValue{ m_currentFn->AllocateSSAId(), exprType };
            inst.location = expr.location;

            switch (expr.token.kind) {
                case Frontend::TokenKind::IntegerLiteral:
                    inst.opcode    = IROpcode::ConstInt;
                    inst.intImm    = ParseIntLiteral(expr.text);
                    inst.dest.type = MakePrim("Int");
                    break;
                case Frontend::TokenKind::FloatLiteral:
                    inst.opcode    = IROpcode::ConstFloat;
                    inst.floatImm  = ParseFloatLiteral(expr.text);
                    inst.dest.type = MakePrim("Float");
                    break;
                case Frontend::TokenKind::BoolLiteral:
                    inst.opcode    = IROpcode::ConstBool;
                    inst.boolImm   = (expr.text == "true");
                    inst.dest.type = MakePrim("Bool");
                    break;
                default:
                    inst.opcode    = IROpcode::ConstString;
                    inst.textImm   = Unquote(expr.text);
                    inst.dest.type = MakePrim("String");
                    break;
            }
            IRValue out = inst.dest;
            Emit(std::move(inst));
            return out;
        }

        case Frontend::ExprKind::Identifier: {
            IRInstruction load;
            load.opcode   = IROpcode::LoadVar;
            load.dest     = IRValue{ m_currentFn->AllocateSSAId(), exprType };
            load.textImm  = expr.text;
            load.location = expr.location;
            IRValue out   = load.dest;
            Emit(std::move(load));
            return out;
        }

        case Frontend::ExprKind::Unary: {
            const std::string& uop = expr.text;

            // Prefix/postfix increment and decrement on a plain variable
            if ((uop == "++" || uop == "--") && expr.left &&
                expr.left->kind == Frontend::ExprKind::Identifier) {
                const auto& target = *expr.left;
                const bool isPostfix =
                    (expr.location.line > target.location.line) ||
                    (expr.location.line == target.location.line && expr.location.column > target.location.column);

                IRInstruction loadOld;
                loadOld.opcode   = IROpcode::LoadVar;
                loadOld.dest     = IRValue{ m_currentFn->AllocateSSAId(), target.inferredType.IsEmpty() ? MakePrim("Int") : target.inferredType };
                loadOld.textImm  = target.text;
                loadOld.location = expr.location;
                IRValue oldVal   = loadOld.dest;
                Emit(std::move(loadOld));

                IRInstruction one;
                one.opcode   = IROpcode::ConstInt;
                one.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                one.intImm   = 1;
                one.location = expr.location;
                IRValue oneVal = one.dest;
                Emit(std::move(one));

                IRInstruction upd;
                upd.opcode     = (uop == "++") ? IROpcode::Add : IROpcode::Sub;
                upd.dest       = IRValue{ m_currentFn->AllocateSSAId(), oldVal.type };
                upd.operandIds = { oldVal.ssaId, oneVal.ssaId };
                upd.location   = expr.location;
                IRValue newVal = upd.dest;
                Emit(std::move(upd));

                IRInstruction store;
                store.opcode     = IROpcode::StoreVar;
                store.textImm    = target.text;
                store.operandIds = { newVal.ssaId };
                store.location   = expr.location;
                Emit(std::move(store));
                return isPostfix ? oldVal : newVal;
            }

            if (uop != "!" && uop != "-" && uop != "+") {
                m_diagnostics.ReportError(
                    "I4011",
                    "Unary operator '" + uop + "' is not yet supported by the SSA IR backend.",
                    expr.location,
                    "Use '-', '!', '++' or '--' on variables.");
            }

            IRValue opVal = expr.left ? LowerExpr(*expr.left) : IRValue{};
            if (uop == "+") {
                return opVal;
            }
            IRInstruction un;
            un.opcode     = (uop == "!") ? IROpcode::Not : IROpcode::Neg;
            un.dest       = IRValue{ m_currentFn->AllocateSSAId(), opVal.type };
            un.operandIds = { opVal.ssaId };
            un.location   = expr.location;
            IRValue out   = un.dest;
            Emit(std::move(un));
            return out;
        }

        case Frontend::ExprKind::Binary: {
            IRValue l = expr.left ? LowerExpr(*expr.left) : IRValue{};
            IRValue r = expr.right ? LowerExpr(*expr.right) : IRValue{};

            IROpcode op = IROpcode::Add;
            Frontend::TypeNode resType = l.type;
            const std::string& t = expr.text;
            if (t == "+") op = IROpcode::Add;
            else if (t == "-") op = IROpcode::Sub;
            else if (t == "*") op = IROpcode::Mul;
            else if (t == "/") op = IROpcode::Div;
            else if (t == "%") op = IROpcode::Mod;
            else if (t == "==") { op = IROpcode::Eq; resType = MakePrim("Bool"); }
            else if (t == "!=") { op = IROpcode::Ne; resType = MakePrim("Bool"); }
            else if (t == "<")  { op = IROpcode::Lt; resType = MakePrim("Bool"); }
            else if (t == "<=") { op = IROpcode::Le; resType = MakePrim("Bool"); }
            else if (t == ">")  { op = IROpcode::Gt; resType = MakePrim("Bool"); }
            else if (t == ">=") { op = IROpcode::Ge; resType = MakePrim("Bool"); }
            else if (t == "&&") { op = IROpcode::And; resType = MakePrim("Bool"); }
            else if (t == "||") { op = IROpcode::Or; resType = MakePrim("Bool"); }
            else {
                m_diagnostics.ReportError(
                    "I4011",
                    "Binary operator '" + t + "' is not yet supported by the SSA IR backend.",
                    expr.location,
                    "Supported operators: + - * / % == != < <= > >= && ||");
            }

            IRInstruction bin;
            bin.opcode     = op;
            bin.dest       = IRValue{ m_currentFn->AllocateSSAId(), resType };
            bin.operandIds = { l.ssaId, r.ssaId };
            bin.location   = expr.location;
            IRValue out    = bin.dest;
            Emit(std::move(bin));
            return out;
        }

        case Frontend::ExprKind::Assignment: {
            IRValue rhs = expr.right ? LowerExpr(*expr.right) : IRValue{};
            if (expr.left && expr.left->kind == Frontend::ExprKind::Identifier) {
                const std::string& varName = expr.left->text;
                IRValue finalVal = rhs;

                if (expr.text != "=") {
                    IRInstruction loadOld;
                    loadOld.opcode   = IROpcode::LoadVar;
                    loadOld.dest     = IRValue{ m_currentFn->AllocateSSAId(), rhs.type };
                    loadOld.textImm  = varName;
                    loadOld.location = expr.location;
                    IRValue oldVal   = loadOld.dest;
                    Emit(std::move(loadOld));

                    IRInstruction opInst;
                    if (expr.text == "+=") opInst.opcode = IROpcode::Add;
                    else if (expr.text == "-=") opInst.opcode = IROpcode::Sub;
                    else if (expr.text == "*=") opInst.opcode = IROpcode::Mul;
                    else if (expr.text == "/=") opInst.opcode = IROpcode::Div;
                    else if (expr.text == "%=") opInst.opcode = IROpcode::Mod;
                    else {
                        m_diagnostics.ReportError(
                            "I4011",
                            "Compound assignment operator '" + expr.text + "' is not yet supported by the SSA IR backend.",
                            expr.location,
                            "Supported compound assignments: += -= *= /= %=");
                        opInst.opcode = IROpcode::Add;
                    }
                    opInst.dest       = IRValue{ m_currentFn->AllocateSSAId(), rhs.type };
                    opInst.operandIds = { oldVal.ssaId, rhs.ssaId };
                    opInst.location   = expr.location;
                    finalVal          = opInst.dest;
                    Emit(std::move(opInst));
                }

                IRInstruction store;
                store.opcode     = IROpcode::StoreVar;
                store.textImm    = varName;
                store.operandIds = { finalVal.ssaId };
                store.location   = expr.location;
                Emit(std::move(store));
                return finalVal;
            }
            if (expr.left && expr.left->kind == Frontend::ExprKind::MemberAccess && expr.left->left) {
                IRValue objVal = LowerExpr(*expr.left->left);
                IRInstruction setF;
                setF.opcode     = IROpcode::SetField;
                setF.textImm    = expr.left->text;
                setF.operandIds = { objVal.ssaId, rhs.ssaId };
                setF.location   = expr.location;
                Emit(std::move(setF));
                return rhs;
            }
            return rhs;
        }

        case Frontend::ExprKind::NewObject: {
            m_currentFn->usesHeapOrGC = true;
            std::vector<std::uint32_t> argIds;
            for (const auto& arg : expr.arguments) {
                if (arg) {
                    argIds.push_back(LowerExpr(*arg).ssaId);
                }
            }
            IRInstruction alloc;
            alloc.opcode     = IROpcode::AllocHeap;
            alloc.dest       = IRValue{ m_currentFn->AllocateSSAId(), expr.targetType };
            alloc.textImm    = expr.targetType.baseName;
            alloc.operandIds = std::move(argIds);
            alloc.location   = expr.location;
            IRValue out      = alloc.dest;
            Emit(std::move(alloc));
            return out;
        }

        case Frontend::ExprKind::MemberAccess: {
            IRValue objVal = expr.left ? LowerExpr(*expr.left) : IRValue{};
            IRInstruction getF;
            getF.opcode     = IROpcode::GetField;
            getF.dest       = IRValue{ m_currentFn->AllocateSSAId(), exprType };
            getF.textImm    = expr.text;
            getF.operandIds = { objVal.ssaId };
            getF.location   = expr.location;
            IRValue out     = getF.dest;
            Emit(std::move(getF));
            return out;
        }

        case Frontend::ExprKind::Call: {
            std::vector<std::uint32_t> argIds;
            for (const auto& arg : expr.arguments) {
                if (arg) {
                    argIds.push_back(LowerExpr(*arg).ssaId);
                }
            }

            if (expr.left && expr.left->kind == Frontend::ExprKind::Identifier) {
                const std::string& fnName = expr.left->text;
                if (fnName == "Print" && !argIds.empty()) {
                    IRInstruction pr;
                    pr.opcode     = IROpcode::PrintVal;
                    pr.operandIds = std::move(argIds);
                    pr.location   = expr.location;
                    Emit(std::move(pr));
                    return IRValue{ 0, MakePrim("Void") };
                }
                if ((fnName == "Assert" || fnName == "AssertTrue" || fnName == "AssertFalse" || fnName == "AssertEqual") && !argIds.empty()) {
                    IRInstruction asrt;
                    asrt.opcode     = IROpcode::AssertVal;
                    asrt.textImm    = fnName;
                    asrt.operandIds = std::move(argIds);
                    asrt.location   = expr.location;
                    Emit(std::move(asrt));
                    return IRValue{ 0, MakePrim("Void") };
                }
                if (fnName == "Success") {
                    IRInstruction res;
                    res.opcode   = IROpcode::ConstString;
                    res.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Result") };
                    res.textImm  = "Success";
                    res.location = expr.location;
                    IRValue out  = res.dest;
                    Emit(std::move(res));
                    return out;
                }
                if (fnName == "Error") {
                    IRInstruction res;
                    res.opcode   = IROpcode::ConstString;
                    res.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Result") };
                    res.textImm  = "Error";
                    res.location = expr.location;
                    IRValue out  = res.dest;
                    Emit(std::move(res));
                    return out;
                }

                IRInstruction call;
                call.opcode          = IROpcode::CallDirect;
                call.dest            = IRValue{ m_currentFn->AllocateSSAId(), exprType };
                call.textImm         = fnName;
                call.operandIds      = std::move(argIds);
                call.isDevirtualized = true;
                call.location        = expr.location;
                IRValue out          = call.dest;
                Emit(std::move(call));
                return out;
            }

            if (expr.left && expr.left->kind == Frontend::ExprKind::MemberAccess && expr.left->left) {
                const std::string& memberName = expr.left->text;
                const auto& receiverExpr      = *expr.left->left;

                // First-class Robix.Testing Assert.<Method>(...) lowering
                if (receiverExpr.kind == Frontend::ExprKind::Identifier && receiverExpr.text == "Assert") {
                    IRInstruction asrt;
                    asrt.opcode     = IROpcode::AssertVal;
                    asrt.textImm    = "Assert." + memberName;
                    asrt.operandIds = std::move(argIds);
                    asrt.location   = expr.location;
                    Emit(std::move(asrt));
                    return IRValue{ 0, MakePrim("Void") };
                }

                // Zero-cost OS-independent platform intrinsics (File, Thread, Time, GPIO)
                if (receiverExpr.kind == Frontend::ExprKind::Identifier &&
                    (receiverExpr.text == "File" || receiverExpr.text == "Thread" ||
                     receiverExpr.text == "Time" || receiverExpr.text == "GPIO")) {
                    IRInstruction osCall;
                    osCall.opcode     = IROpcode::CallOSIntrinsic;
                    osCall.dest       = IRValue{ m_currentFn->AllocateSSAId(), exprType };
                    osCall.textImm    = receiverExpr.text + "." + memberName;
                    osCall.operandIds = std::move(argIds);
                    osCall.location   = expr.location;
                    IRValue out       = osCall.dest;
                    Emit(std::move(osCall));
                    return out;
                }

                IRValue recvVal = LowerExpr(receiverExpr);
                const std::string recvClass = receiverExpr.inferredType.baseName;

                // Built-in numeric conversions (.ToFloat(), .ToInt())
                if (memberName == "ToFloat" || memberName == "ToInt" || memberName == "ToString") {
                    IRInstruction conv;
                    conv.opcode     = IROpcode::Copy;
                    conv.dest       = IRValue{ m_currentFn->AllocateSSAId(), exprType };
                    conv.operandIds = { recvVal.ssaId };
                    conv.textImm    = memberName;
                    conv.location   = expr.location;
                    IRValue out     = conv.dest;
                    Emit(std::move(conv));
                    return out;
                }

                std::vector<std::uint32_t> methodArgs = { recvVal.ssaId };
                methodArgs.insert(methodArgs.end(), argIds.begin(), argIds.end());

                const bool devirt = m_sema.CanDevirtualizeCall(recvClass, memberName);
                IRInstruction mCall;
                mCall.opcode          = devirt ? IROpcode::CallDirect : IROpcode::CallVirtual;
                mCall.dest            = IRValue{ m_currentFn->AllocateSSAId(), exprType };
                mCall.textImm         = recvClass.empty() ? memberName : (recvClass + "." + memberName);
                mCall.secondaryText   = recvClass;
                mCall.operandIds      = std::move(methodArgs);
                mCall.isDevirtualized = devirt;
                mCall.location        = expr.location;
                IRValue out           = mCall.dest;
                Emit(std::move(mCall));
                return out;
            }
            break;
        }

        default:
            break;
    }

    m_diagnostics.ReportError(
        "I4011",
        "This expression form (index, array/object literal, lambda, match expression, range) is not yet supported by the SSA IR backend.",
        expr.location,
        "Rewrite using scalar arithmetic, calls, and variables.");

    IRInstruction fallback;
    fallback.opcode   = IROpcode::ConstInt;
    fallback.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
    fallback.intImm   = 0;
    fallback.location = expr.location;
    IRValue out       = fallback.dest;
    Emit(std::move(fallback));
    return out;
}

} // namespace Robix::IR

