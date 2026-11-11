#include "NativeBackend.hpp"
#include "../../RobixUI/CookedLayout.hpp"
#include "../Runtime/SceneRaster.hpp"
#include "../Runtime/InputScript.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace Robix::CodeGen {

namespace {

enum class NTy : std::uint8_t { Void, I1, I8, I16, I32, I64, F32, F64, Ptr };

const char* TyStr(NTy t) noexcept {
    switch (t) {
        case NTy::Void: return "void";
        case NTy::I1:   return "i1";
        case NTy::I8:   return "i8";
        case NTy::I16:  return "i16";
        case NTy::I32:  return "i32";
        case NTy::I64:  return "i64";
        case NTy::F32:  return "float";
        case NTy::F64:  return "double";
        case NTy::Ptr:  return "ptr";
    }
    return "void";
}

bool IsIntTy(NTy t) noexcept {
    return t == NTy::I1 || t == NTy::I8 || t == NTy::I16 || t == NTy::I32 || t == NTy::I64;
}

bool IsFloatTy(NTy t) noexcept { return t == NTy::F32 || t == NTy::F64; }

int UnavailableDevice(const std::string& name) noexcept {
    if (name == "PWM.SetDuty") return 1;
    if (name == "UART.Write") return 2;
    if (name == "SPI.Transfer") return 3;
    if (name == "I2C.Write") return 4;
    if (name == "CAN.Send") return 5;
    if (name == "USB.Write") return 6;
    if (name == "PCIe.Read") return 7;
    if (name == "DMA.Start") return 8;
    if (name == "Sensor.Read") return 9;
    if (name == "Motor.SetSpeed") return 10;
    if (name == "Interrupt.Enable") return 11;
    if (name == "HardwareTimer.Read") return 12;
    return 0;
}

int IntBits(NTy t) noexcept {
    switch (t) {
        case NTy::I1:  return 1;
        case NTy::I8:  return 8;
        case NTy::I16: return 16;
        case NTy::I32: return 32;
        case NTy::I64: return 64;
        default:       return 0;
    }
}

int AlignOf(NTy t) noexcept {
    switch (t) {
        case NTy::I64:
        case NTy::F64:
        case NTy::Ptr: return 8;
        case NTy::I32:
        case NTy::F32: return 4;
        case NTy::I16: return 2;
        default:       return 1;
    }
}

// Maps a Robix type to a native scalar type. Returns false for unsupported types.
bool IsUnsignedName(const std::string& b) noexcept {
    return b == "Byte" || b == "UInt8" || b == "UInt16" || b == "UInt32" || b == "UInt64" || b == "UInt";
}

bool MapType(const Frontend::TypeNode& t, NTy& out, std::string& why) {
    if (t.isNullable) {
        why = "nullable types";
        return false;
    }
    const std::string& b = t.baseName;
    if (t.isPointer || b == "Pointer" || b == "Array" || b == "Slice" || b == "String" ||
        b == "Result" || b == "Option" || b == "Tuple") {
        out = NTy::Ptr;
        return true;
    }
    if (!t.typeArgs.empty()) {
        why = "generic type '" + b + "'";
        return false;
    }
    if (b.empty() || b == "Void")                          { out = NTy::Void; return true; }
    if (b == "Bool")                                       { out = NTy::I1;   return true; }
    if (b == "Int8" || b == "Byte" || b == "UInt8")        { out = NTy::I8;   return true; }
    if (b == "Int16" || b == "UInt16")                     { out = NTy::I16;  return true; }
    if (b == "Int32" || b == "UInt32" || b == "Char")      { out = NTy::I32;  return true; }
    if (b == "Int" || b == "Int64" || b == "UInt" || b == "UInt64") { out = NTy::I64; return true; }
    if (b == "Float32")                                    { out = NTy::F32;  return true; }
    if (b == "Float" || b == "Float64" || b == "Double")   { out = NTy::F64;  return true; }
    if (b == "Any" || b == "Object") {
        why = "type '" + b + "'";
        return false;
    }
    // Enum tags and other named scalars. Structs and pointers are marked isPointer.
    out = NTy::I64;
    return true;
}

NTy Promote(NTy a, NTy b) noexcept {
    if (a == NTy::Ptr || b == NTy::Ptr) return NTy::Ptr;
    if (a == NTy::F64 || b == NTy::F64) return NTy::F64;
    if (a == NTy::F32 || b == NTy::F32) return NTy::F32;
    return IntBits(a) >= IntBits(b) ? a : b;
}

NTy ArithPromote(NTy a, NTy b) noexcept {
    NTy p = Promote(a, b);
    return (p == NTy::I1) ? NTy::I64 : p;
}

std::string HexDouble(double v) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    char buf[40];
    std::snprintf(buf, sizeof(buf), "0x%016llX", static_cast<unsigned long long>(bits));
    return buf;
}

std::string FirstMnemonic(const std::string& text) {
    std::size_t i = 0;
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
        ++i;
    }
    const std::size_t begin = i;
    while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i])) && text[i] != ',' && text[i] != ';') {
        ++i;
    }
    std::string mnemonic = text.substr(begin, i - begin);
    for (char& ch : mnemonic) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return mnemonic;
}

bool AsmMnemonicAllowed(std::string_view arch, std::string_view mnemonic) {
    auto listed = [&](std::initializer_list<const char*> names) {
        for (const char* name : names) {
            if (mnemonic == name) return true;
        }
        return false;
    };
    if (arch == "aarch64") {
        return listed({ "nop", "ret", "mov", "add", "sub", "ldr", "str", "blr", "br",
                        "svc", "mrs", "msr", "dsb", "isb", "wfi", "wfe" });
    }
    if (arch == "riscv64") {
        return listed({ "nop", "ret", "li", "mv", "add", "sub", "lw", "sw", "ld", "sd",
                        "jal", "jalr", "ecall", "ebreak", "wfi", "fence" });
    }
    return listed({ "nop", "cli", "sti", "hlt", "mov", "movb", "movw", "movl", "movq",
                    "out", "outb", "outw", "outl", "in", "inb", "inw", "inl",
                    "lgdt", "lidt", "ljmp", "ljmpl", "ltr", "rdmsr", "wrmsr", "invlpg",
                    "int", "iret", "iretq", "push", "pushq", "pop", "popq", "ret", "jmp", "call",
                    "add", "addq", "sub", "subq", "and", "or", "xor", "shl", "shr", "sar",
                    "lea", "cmp", "test", "xchg", "cpuid", "pause", "mfence", "lfence", "sfence" });
}

std::string EscapeLLVMString(const std::string& s) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (c >= 0x20 && c < 0x7F && c != '"' && c != '\\') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('\\');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0xF]);
        }
    }
    return out;
}

struct Val {
    std::string repr;
    NTy ty = NTy::Void;
    bool uns = false;
};

struct ModuleCtx {
    Frontend::DiagnosticBag& diags;
    std::unordered_map<std::string, const IR::IRFunction*> byName;
    std::vector<std::string> stringLiterals;
    std::set<std::string> cpuFns;
    std::set<std::string> isrFns;
    std::set<std::string> faultIsrs;
    std::set<std::string> timerIsrs;
    std::string preemptA;
    std::string preemptB;
    bool usesPreempt = false;
    bool usesSyscall = false;
    bool usesPages = false;
    bool usesSwitch = false;
    bool usesFrame = false;
    bool usesUI = false;
    std::vector<Robix::UI::RoboCookedBox> uiBoxes;
    bool usesInput = false;
    bool usesSched = false;
    bool usesPort = false;
    bool usesProc = false;
    bool usesDispatch = false;
    bool usesFs = false;
    bool usesPackage = false;
    bool usesGpu = false;
    std::vector<RoboPlayedEvent> inputScript;
    bool usesScene = false;
    std::vector<std::uint32_t> scenePixels;
    int printFmt = -1;
    const IR::IRModule* module = nullptr;
    const NativeEmitOptions& options;
    explicit ModuleCtx(Frontend::DiagnosticBag& d, const NativeEmitOptions& opts)
        : diags(d), options(opts) {}
};

bool FunctionSignature(const IR::IRFunction& fn, NTy& ret, std::vector<NTy>& params, std::string& why) {
    if (!MapType(fn.returnType, ret, why)) return false;
    for (const auto& p : fn.parameters) {
        NTy pt{};
        if (!MapType(p.type, pt, why) || pt == NTy::Void) {
            if (why.empty()) why = "void parameter";
            return false;
        }
        params.push_back(pt);
    }
    return true;
}

class FnEmitter {
public:
    FnEmitter(const IR::IRFunction& fn, ModuleCtx& ctx, std::ostringstream& out)
        : m_fn(fn), m_ctx(ctx), m_out(out) {}

    bool Emit();

private:
    const IR::IRFunction& m_fn;
    ModuleCtx& m_ctx;
    std::ostringstream& m_out;

    std::ostringstream m_allocas;
    std::ostringstream m_body;
    std::unordered_map<std::string, NTy> m_varTy;
    std::unordered_map<std::uint32_t, Val> m_vals;
    int m_tmp  = 0;
    int m_sw   = 0;
    bool m_ok  = true;
    NTy m_ret  = NTy::Void;
    std::string m_blockLabel;
    std::uint32_t m_lastDebugLine = 0;

    struct DefInfo {
        IR::IROpcode op = IR::IROpcode::Nop;
        std::uint32_t op0 = 0;
        std::uint32_t op1 = 0;
        std::int64_t imm = 0;
        std::string text;
        bool uns = false;
    };
    std::unordered_map<std::uint32_t, DefInfo> m_defs;
    std::unordered_map<std::string, std::size_t> m_blockOf;

    struct SwitchKey {
        bool ok = false;
        bool isLoad = false;
        std::uint32_t id = 0;
        std::string var;
    };

    std::string ConstText(std::uint32_t id) const {
        auto it = m_defs.find(id);
        if (it == m_defs.end()) return {};
        if (it->second.op == IR::IROpcode::Copy && it->second.text != "ToString") return ConstText(it->second.op0);
        if (it->second.op == IR::IROpcode::ConstString) return it->second.text;
        return {};
    }

    void IndexDefs() {
        m_defs.clear();
        m_blockOf.clear();
        for (std::size_t bi = 0; bi < m_fn.blocks.size(); ++bi) {
            const auto& bb = m_fn.blocks[bi];
            m_blockOf[bb.label] = bi;
            for (const auto& inst : bb.instructions) {
                if (!inst.dest.IsValid()) continue;
                DefInfo d;
                d.op = inst.opcode;
                d.imm = inst.intImm;
                d.text = inst.textImm;
                if (!inst.operandIds.empty()) d.op0 = inst.operandIds[0];
                if (inst.operandIds.size() > 1) d.op1 = inst.operandIds[1];
                d.uns = IsUnsignedName(inst.dest.type.baseName);
                m_defs[inst.dest.ssaId] = std::move(d);
            }
        }
    }

    SwitchKey MakeKey(std::uint32_t id) const {
        SwitchKey k;
        auto it = m_defs.find(id);
        if (it == m_defs.end()) return k;
        if (it->second.op == IR::IROpcode::Copy) return MakeKey(it->second.op0);
        k.ok = true;
        if (it->second.op == IR::IROpcode::LoadVar) {
            k.isLoad = true;
            k.var = it->second.text;
        } else {
            k.id = id;
        }
        return k;
    }

    static bool SameKey(const SwitchKey& a, const SwitchKey& b) {
        if (!a.ok || !b.ok) return false;
        if (a.isLoad || b.isLoad) return a.isLoad && b.isLoad && a.var == b.var;
        return a.id == b.id;
    }

    // Equality test of a value against a constant. Used to rebuild else-if
    // cascades as one LLVM switch, which the backend lowers to a jump table.
    bool DecodeEq(std::uint32_t cmpId, SwitchKey& key, std::int64_t& imm) const {
        auto it = m_defs.find(cmpId);
        if (it == m_defs.end() || it->second.op != IR::IROpcode::Eq) return false;
        auto lhs = m_defs.find(it->second.op0);
        auto rhs = m_defs.find(it->second.op1);
        const bool lhsC = lhs != m_defs.end() && lhs->second.op == IR::IROpcode::ConstInt;
        const bool rhsC = rhs != m_defs.end() && rhs->second.op == IR::IROpcode::ConstInt;
        if (rhsC && !lhsC) {
            imm = rhs->second.imm;
            key = MakeKey(it->second.op0);
            return key.ok;
        }
        if (lhsC && !rhsC) {
            imm = lhs->second.imm;
            key = MakeKey(it->second.op1);
            return key.ok;
        }
        return false;
    }

    bool DecodeUpperBound(std::uint32_t cmpId, SwitchKey& key, std::int64_t& exclusive) const {
        auto it = m_defs.find(cmpId);
        if (it == m_defs.end()) return false;
        if (it->second.op != IR::IROpcode::Lt && it->second.op != IR::IROpcode::Le) return false;
        auto rhs = m_defs.find(it->second.op1);
        if (rhs == m_defs.end() || rhs->second.op != IR::IROpcode::ConstInt) return false;
        key = MakeKey(it->second.op0);
        if (!key.ok) return false;
        if (it->second.op == IR::IROpcode::Lt) {
            exclusive = rhs->second.imm;
        } else {
            if (rhs->second.imm == INT64_MAX) return false;
            exclusive = rhs->second.imm + 1;
        }
        return true;
    }

    // A block that only compares a value with a constant and branches.
    bool EqDispatch(std::size_t bi, SwitchKey& key, std::int64_t& imm,
                    std::string& taken, std::string& next) const {
        const auto& bb = m_fn.blocks[bi];
        const IR::IRInstruction* cmp = nullptr;
        const IR::IRInstruction* br = nullptr;
        for (const auto& inst : bb.instructions) {
            if (inst.isDead) continue;
            if (IR::IsTerminatorOpcode(inst.opcode)) {
                if (inst.opcode != IR::IROpcode::BranchCond || br) return false;
                br = &inst;
                continue;
            }
            if (inst.opcode == IR::IROpcode::LoadVar || inst.opcode == IR::IROpcode::ConstInt) continue;
            if (inst.opcode == IR::IROpcode::Eq) {
                if (cmp) return false;
                cmp = &inst;
                continue;
            }
            return false;
        }
        if (!cmp || !br || br->operandIds.empty() || br->operandIds[0] != cmp->dest.ssaId) return false;
        if (!DecodeEq(cmp->dest.ssaId, key, imm)) return false;
        taken = br->textImm;
        next = br->secondaryText;
        return true;
    }

    struct EqChain {
        bool ok = false;
        SwitchKey key;
        std::vector<std::pair<std::int64_t, std::string>> cases;
        std::string fallback;
    };

    EqChain WalkEqChain(const std::string& start) const {
        EqChain chain;
        std::set<std::string> seen;
        std::string cur = start;
        while (seen.size() < 64) {
            if (cur.empty() || !seen.insert(cur).second) return {};
            auto bit = m_blockOf.find(cur);
            if (bit == m_blockOf.end()) {
                chain.fallback = cur;
                chain.ok = !chain.cases.empty();
                return chain;
            }
            SwitchKey key;
            std::int64_t imm = 0;
            std::string taken;
            std::string next;
            if (!EqDispatch(bit->second, key, imm, taken, next)) {
                chain.fallback = cur;
                chain.ok = !chain.cases.empty();
                return chain;
            }
            if (!chain.key.ok) chain.key = key;
            else if (!SameKey(chain.key, key)) return {};
            chain.cases.push_back({ imm, taken });
            cur = next;
        }
        return {};
    }

    bool EmitSwitch(const std::string& value, const std::string& cond,
                    const std::map<std::int64_t, std::string>& cases,
                    const std::string& lowFallback, const std::string& highFallback) {
        if (cases.size() < 4 || cases.size() > 64) return false;
        if (lowFallback.empty() || highFallback.empty()) return false;
        const std::string def = "swdef_" + std::to_string(m_sw++);
        m_body << "  switch i64 " << value << ", label %" << def << " [\n";
        for (const auto& [imm, lab] : cases) {
            m_body << "    i64 " << imm << ", label %" << lab << "\n";
        }
        m_body << "  ]\n";
        m_body << def << ":\n";
        if (lowFallback == highFallback) {
            m_body << "  br label %" << lowFallback << "\n";
        } else {
            m_body << "  br i1 " << cond << ", label %" << lowFallback << ", label %" << highFallback << "\n";
        }
        return true;
    }

    // else-if on one value, or `if (x < N)` splitting two such chains, becomes
    // a single switch. Hot cases are explicit so the default split is rare.
    bool TryEmitSwitch(const IR::IRInstruction& inst) {
        if (inst.operandIds.empty()) return false;
        const std::uint32_t condId = inst.operandIds[0];
        SwitchKey rangeKey;
        std::int64_t exclusive = 0;
        if (DecodeUpperBound(condId, rangeKey, exclusive)) {
            EqChain low = WalkEqChain(inst.textImm);
            EqChain high = WalkEqChain(inst.secondaryText);
            if (!low.ok || !high.ok) return false;
            if (!SameKey(rangeKey, low.key) || !SameKey(rangeKey, high.key)) return false;
            auto lit = m_defs.find(condId);
            if (lit == m_defs.end()) return false;
            auto vit = m_vals.find(lit->second.op0);
            if (vit == m_vals.end()) return false;
            Val value = Coerce(vit->second, NTy::I64, inst.location);
            Val cond = Coerce(Operand(inst, 0), NTy::I1, inst.location);
            std::map<std::int64_t, std::string> cases;
            for (const auto& c : low.cases) {
                if (c.first < 0 || c.first >= exclusive) return false;
                if (!cases.emplace(c.first, c.second).second) return false;
            }
            for (const auto& c : high.cases) {
                if (c.first < exclusive) return false;
                if (!cases.emplace(c.first, c.second).second) return false;
            }
            if (exclusive > 0 && exclusive <= 32) {
                for (std::int64_t v = 0; v < exclusive; ++v) {
                    cases.emplace(v, low.fallback);
                }
            }
            bool highContig = !high.cases.empty();
            std::set<std::int64_t> highVals;
            for (const auto& c : high.cases) highVals.insert(c.first);
            for (std::int64_t v = exclusive; highContig && v < exclusive + static_cast<std::int64_t>(high.cases.size()); ++v) {
                if (!highVals.count(v)) highContig = false;
            }
            if (highContig && exclusive + static_cast<std::int64_t>(high.cases.size()) < exclusive + 32) {
                cases.emplace(exclusive + static_cast<std::int64_t>(high.cases.size()), high.fallback);
            }
            return EmitSwitch(value.repr, cond.repr, cases, low.fallback, high.fallback);
        }

        EqChain flat = WalkEqChain(m_blockLabel);
        if (!flat.ok || flat.cases.size() < 4) return false;
        auto eit = m_defs.find(condId);
        if (eit == m_defs.end() || eit->second.op != IR::IROpcode::Eq) return false;
        SwitchKey key;
        std::int64_t imm = 0;
        if (!DecodeEq(condId, key, imm) || !SameKey(key, flat.key)) return false;
        std::uint32_t lhs = eit->second.op0;
        auto rhs = m_defs.find(eit->second.op1);
        if (rhs == m_defs.end() || rhs->second.op != IR::IROpcode::ConstInt) {
            lhs = eit->second.op1;
        }
        auto vit = m_vals.find(lhs);
        if (vit == m_vals.end()) return false;
        Val value = Coerce(vit->second, NTy::I64, inst.location);
        std::map<std::int64_t, std::string> cases;
        for (const auto& c : flat.cases) {
            if (!cases.emplace(c.first, c.second).second) return false;
        }
        return EmitSwitch(value.repr, "false", cases, flat.fallback, flat.fallback);
    }

    void Err(const std::string& code, const std::string& msg, const Frontend::SourceLocation& loc,
             const std::string& hint = {}) {
        m_ok = false;
        m_ctx.diags.ReportError(code, msg, loc, hint);
    }

    std::string NewTmp() { return "%t" + std::to_string(m_tmp++); }
    static std::string SlotName(const std::string& var) { return "%\"slot." + var + "\""; }

    Val Operand(const IR::IRInstruction& inst, std::size_t idx) {
        if (idx >= inst.operandIds.size()) {
            Err("N9006", "Malformed IR: missing operand " + std::to_string(idx) + ".", inst.location);
            return Val{ "0", NTy::I64 };
        }
        auto it = m_vals.find(inst.operandIds[idx]);
        if (it == m_vals.end()) {
            Err("N9006", "Malformed IR: use of undefined value %" + std::to_string(inst.operandIds[idx]) + ".", inst.location);
            return Val{ "0", NTy::I64 };
        }
        return it->second;
    }

    Val Coerce(const Val& v, NTy to, const Frontend::SourceLocation& loc) {
        if (v.ty == to) return v;
        if (v.ty == NTy::Void || to == NTy::Void || v.ty == NTy::Ptr || to == NTy::Ptr) {
            Err("N9007", "Cannot convert between '" + std::string(TyStr(v.ty)) + "' and '" + TyStr(to) + "' in native code.", loc);
            return Val{ "0", to == NTy::Void ? NTy::I64 : to };
        }
        const std::string t = NewTmp();
        if (IsIntTy(v.ty) && IsIntTy(to)) {
            if (to == NTy::I1) {
                m_body << "  " << t << " = icmp ne " << TyStr(v.ty) << " " << v.repr << ", 0\n";
            } else if (IntBits(to) > IntBits(v.ty)) {
                const bool zero = v.uns || v.ty == NTy::I1;
                m_body << "  " << t << " = " << (zero ? "zext " : "sext ")
                       << TyStr(v.ty) << " " << v.repr << " to " << TyStr(to) << "\n";
            } else {
                m_body << "  " << t << " = trunc " << TyStr(v.ty) << " " << v.repr << " to " << TyStr(to) << "\n";
            }
        } else if (IsIntTy(v.ty) && IsFloatTy(to)) {
            m_body << "  " << t << " = " << ((v.uns || v.ty == NTy::I1) ? "uitofp " : "sitofp ")
                   << TyStr(v.ty) << " " << v.repr << " to " << TyStr(to) << "\n";
        } else if (IsFloatTy(v.ty) && IsIntTy(to)) {
            if (to == NTy::I1) {
                m_body << "  " << t << " = fcmp une " << TyStr(v.ty) << " " << v.repr << ", 0.0\n";
            } else {
                m_body << "  " << t << " = fptosi " << TyStr(v.ty) << " " << v.repr << " to " << TyStr(to) << "\n";
            }
        } else {
            m_body << "  " << t << " = " << (to == NTy::F64 ? "fpext " : "fptrunc ")
                   << TyStr(v.ty) << " " << v.repr << " to " << TyStr(to) << "\n";
        }
        return Val{ t, to, v.uns && IsIntTy(to) };
    }

    std::string Zero(NTy t) const {
        return IsFloatTy(t) ? "0.0" : (t == NTy::Ptr ? "null" : "0");
    }

    NTy EnsureVar(const std::string& name, NTy ty) {
        auto it = m_varTy.find(name);
        if (it != m_varTy.end()) return it->second;
        m_varTy[name] = ty;
        m_allocas << "  " << SlotName(name) << " = alloca " << TyStr(ty) << ", align " << AlignOf(ty) << "\n";
        return ty;
    }

    bool ConstOf(std::uint32_t id, std::int64_t& out) const {
        auto it = m_defs.find(id);
        if (it == m_defs.end() || it->second.op != IR::IROpcode::ConstInt) return false;
        out = it->second.imm;
        return true;
    }

    bool OperandUnsigned(const IR::IRInstruction& inst, std::size_t index) const {
        if (index >= inst.operandIds.size()) return false;
        auto it = m_defs.find(inst.operandIds[index]);
        return it != m_defs.end() && it->second.uns;
    }

    // True when this condition's taken edge means "the variable is divisible by 2".
    // Toward-zero division of an even integer by 2 is an arithmetic shift.
    bool EdgeProvesEven(std::uint32_t condId, const std::string& var, bool trueEdge) const {
        auto it = m_defs.find(condId);
        if (it == m_defs.end()) return false;
        if (it->second.op != IR::IROpcode::Eq && it->second.op != IR::IROpcode::Ne) return false;
        std::int64_t imm = 0;
        std::uint32_t other = 0;
        if (ConstOf(it->second.op0, imm)) other = it->second.op1;
        else if (ConstOf(it->second.op1, imm)) other = it->second.op0;
        else return false;
        if (imm != 0) return false;
        const bool eq = it->second.op == IR::IROpcode::Eq;
        if (trueEdge != eq) return false;
        auto mod = m_defs.find(other);
        if (mod == m_defs.end() || mod->second.op != IR::IROpcode::Mod) return false;
        std::int64_t divisor = 0;
        if (!ConstOf(mod->second.op1, divisor) || divisor != 2) return false;
        const SwitchKey key = MakeKey(mod->second.op0);
        return key.ok && key.isLoad && key.var == var;
    }

    bool ModUsedOnlyAsZeroTest(std::uint32_t modId) const {
        bool saw = false;
        for (const auto& bb : m_fn.blocks) {
            for (const auto& inst : bb.instructions) {
                if (inst.isDead) continue;
                bool uses = false;
                for (std::uint32_t op : inst.operandIds) {
                    if (op == modId) uses = true;
                }
                if (!uses) continue;
                saw = true;
                if (inst.opcode != IR::IROpcode::Eq && inst.opcode != IR::IROpcode::Ne) return false;
                std::int64_t imm = 0;
                const bool left = inst.operandIds.size() > 0 && inst.operandIds[0] == modId;
                const std::uint32_t other = left ? (inst.operandIds.size() > 1 ? inst.operandIds[1] : 0)
                                                 : (inst.operandIds.empty() ? 0 : inst.operandIds[0]);
                if (!ConstOf(other, imm) || imm != 0) return false;
            }
        }
        return saw;
    }

    bool DividendKnownEven(std::uint32_t ssa) const {
        const SwitchKey key = MakeKey(ssa);
        if (!key.ok || !key.isLoad || m_blockLabel.empty()) return false;
        bool saw = false;
        for (const auto& bb : m_fn.blocks) {
            const IR::IRInstruction* term = nullptr;
            for (const auto& inst : bb.instructions) {
                if (inst.isDead) continue;
                if (IR::IsTerminatorOpcode(inst.opcode)) term = &inst;
            }
            if (term == nullptr) continue;
            if (term->opcode == IR::IROpcode::Jump && term->textImm == m_blockLabel) return false;
            if (term->opcode != IR::IROpcode::BranchCond || term->operandIds.empty()) continue;
            if (term->textImm == m_blockLabel) {
                saw = true;
                if (!EdgeProvesEven(term->operandIds[0], key.var, true)) return false;
            } else if (term->secondaryText == m_blockLabel) {
                saw = true;
                if (!EdgeProvesEven(term->operandIds[0], key.var, false)) return false;
            }
        }
        if (!saw) return false;
        const auto bit = m_blockOf.find(m_blockLabel);
        if (bit == m_blockOf.end()) return false;
        for (const auto& inst : m_fn.blocks[bit->second].instructions) {
            if (inst.opcode == IR::IROpcode::Div && !inst.operandIds.empty() && inst.operandIds[0] == ssa) {
                return true;
            }
            if (inst.opcode == IR::IROpcode::StoreVar && inst.textImm == key.var) return false;
        }
        return false;
    }

    const char* WidthTy(std::int64_t width) const noexcept {
        if (width == 1) return "i8";
        if (width == 2) return "i16";
        if (width == 4) return "i32";
        return "i64";
    }

    void EmitInstruction(const IR::IRInstruction& inst);
};

void FnEmitter::EmitInstruction(const IR::IRInstruction& inst) {
    using IR::IROpcode;
    const std::uint32_t id = inst.dest.ssaId;
    const std::string vname = "%v" + std::to_string(id);

    switch (inst.opcode) {
        case IROpcode::Nop:
            break;

        case IROpcode::ConstInt:
            m_vals[id] = Val{ std::to_string(inst.intImm), NTy::I64, IsUnsignedName(inst.dest.type.baseName) };
            break;

        case IROpcode::ConstFloat:
            m_vals[id] = Val{ HexDouble(inst.floatImm), NTy::F64 };
            break;

        case IROpcode::ConstBool:
            m_vals[id] = Val{ inst.boolImm ? "true" : "false", NTy::I1 };
            break;

        case IROpcode::ConstString: {
            const std::size_t idx = m_ctx.stringLiterals.size();
            m_ctx.stringLiterals.push_back(inst.textImm);
            m_vals[id] = Val{ "@.str." + std::to_string(idx), NTy::Ptr };
            break;
        }

        case IROpcode::Copy: {
            Val v = Operand(inst, 0);
            if (inst.textImm == "ToFloat")      v = Coerce(v, NTy::F64, inst.location);
            else if (inst.textImm == "ToInt") {
                v = Coerce(v, NTy::I64, inst.location);
                v.uns = false;
            }
            else if (inst.textImm == "ToString") {
                Err("N9001", "'.ToString()' is not yet supported by the native backend.", inst.location);
            }
            m_vals[id] = v;
            break;
        }

        case IROpcode::LoadVar: {
            auto it = m_varTy.find(inst.textImm);
            if (it == m_varTy.end()) {
                Err("N9003", "Use of undeclared or unsupported variable '" + inst.textImm +
                                 "' (global constants and class members are not yet supported natively).",
                    inst.location);
                m_vals[id] = Val{ "0", NTy::I64 };
                break;
            }
            m_body << "  " << vname << " = load " << TyStr(it->second) << ", ptr " << SlotName(inst.textImm)
                   << ", align " << AlignOf(it->second) << "\n";
            m_vals[id] = Val{ vname, it->second, IsUnsignedName(inst.dest.type.baseName) };
            break;
        }

        case IROpcode::StoreVar: {
            Val v = Operand(inst, 0);
            NTy declared = v.ty;
            if (!inst.secondaryText.empty()) {
                Frontend::TypeNode tn;
                tn.baseName = inst.secondaryText;
                std::string why;
                NTy mapped{};
                if (MapType(tn, mapped, why) && mapped != NTy::Void) {
                    declared = mapped;
                } else {
                    Err("N9008", "Variable '" + inst.textImm + "' has unsupported native type: " + why + ".", inst.location);
                }
            }
            if (declared == NTy::Void) {
                Err("N9007", "Cannot store a void value into variable '" + inst.textImm + "'.", inst.location);
                break;
            }
            const NTy slotTy = EnsureVar(inst.textImm, declared);
            Val cv = Coerce(v, slotTy, inst.location);
            m_body << "  store " << TyStr(slotTy) << " " << cv.repr << ", ptr " << SlotName(inst.textImm)
                   << ", align " << AlignOf(slotTy) << "\n";
            break;
        }

        case IROpcode::Add:
        case IROpcode::Sub:
        case IROpcode::Mul:
        case IROpcode::Div:
        case IROpcode::Mod: {
            Val a = Operand(inst, 0);
            Val b = Operand(inst, 1);
            if (inst.opcode == IROpcode::Add && inst.dest.type.baseName == "String") {
                std::int64_t ignored = 0;
                auto left = m_defs.find(inst.operandIds.size() > 0 ? inst.operandIds[0] : 0);
                auto right = m_defs.find(inst.operandIds.size() > 1 ? inst.operandIds[1] : 0);
                if (left != m_defs.end() && right != m_defs.end() &&
                    left->second.op == IROpcode::ConstString && right->second.op == IROpcode::ConstString) {
                    const std::size_t idx = m_ctx.stringLiterals.size();
                    m_ctx.stringLiterals.push_back(left->second.text + right->second.text);
                    m_vals[id] = Val{ "@.str." + std::to_string(idx), NTy::Ptr };
                } else {
                    m_body << "  " << vname << " = call ptr @rx_strcat(ptr " << a.repr << ", ptr " << b.repr << ")\n";
                    m_vals[id] = Val{ vname, NTy::Ptr };
                }
                (void)ignored;
                break;
            }
            if (a.ty == NTy::Ptr || b.ty == NTy::Ptr || a.ty == NTy::Void || b.ty == NTy::Void) {
                Err("N9001", "Arithmetic on this pointer or void value is not yet supported by the native backend.", inst.location);
                m_vals[id] = Val{ "0", NTy::I64 };
                break;
            }
            const NTy t = ArithPromote(a.ty, b.ty);
            a = Coerce(a, t, inst.location);
            b = Coerce(b, t, inst.location);
            std::int64_t divisor = 0;
            if (!IsFloatTy(t) &&
                (inst.opcode == IROpcode::Div || inst.opcode == IROpcode::Mod) &&
                inst.operandIds.size() > 1 &&
                ConstOf(inst.operandIds[1], divisor) &&
                divisor > 0 && (divisor & (divisor - 1)) == 0) {
                int shift = 0;
                for (std::int64_t n = divisor; n > 1; n >>= 1) ++shift;
                const bool uns = OperandUnsigned(inst, 0) || OperandUnsigned(inst, 1);
                if (shift == 0) {
                    m_vals[id] = (inst.opcode == IROpcode::Div) ? Val{ a.repr, t, uns } : Val{ "0", t, uns };
                    break;
                }
                if (uns) {
                    if (inst.opcode == IROpcode::Div) {
                        m_body << "  " << vname << " = lshr " << TyStr(t) << " " << a.repr << ", " << shift << "\n";
                    } else {
                        m_body << "  " << vname << " = and " << TyStr(t) << " " << a.repr << ", " << (divisor - 1) << "\n";
                    }
                } else if (divisor == 2 && inst.opcode == IROpcode::Mod && ModUsedOnlyAsZeroTest(id)) {
                    // (n % 2) == 0 is (n & 1) == 0 for every two's-complement n.
                    m_body << "  " << vname << " = and " << TyStr(t) << " " << a.repr << ", 1\n";
                } else if (divisor == 2 && inst.opcode == IROpcode::Mod) {
                    // Toward-zero n%2 is 0, 1, or -1.
                    const std::string bit = NewTmp();
                    const std::string neg = NewTmp();
                    const std::string nbit = NewTmp();
                    m_body << "  " << bit << " = and " << TyStr(t) << " " << a.repr << ", 1\n";
                    m_body << "  " << neg << " = icmp slt " << TyStr(t) << " " << a.repr << ", 0\n";
                    m_body << "  " << nbit << " = sub " << TyStr(t) << " 0, " << bit << "\n";
                    m_body << "  " << vname << " = select i1 " << neg << ", " << TyStr(t) << " " << nbit
                           << ", " << TyStr(t) << " " << bit << "\n";
                } else if (inst.opcode == IROpcode::Div && shift == 1 &&
                           !inst.operandIds.empty() && DividendKnownEven(inst.operandIds[0])) {
                    m_body << "  " << vname << " = ashr " << TyStr(t) << " " << a.repr << ", 1\n";
                } else {
                    const std::string sign = NewTmp();
                    const std::string mask = NewTmp();
                    const std::string adj = NewTmp();
                    const std::string quot = (inst.opcode == IROpcode::Div) ? vname : NewTmp();
                    m_body << "  " << sign << " = ashr " << TyStr(t) << " " << a.repr << ", " << (IntBits(t) - 1) << "\n";
                    m_body << "  " << mask << " = and " << TyStr(t) << " " << sign << ", " << (divisor - 1) << "\n";
                    m_body << "  " << adj << " = add " << TyStr(t) << " " << a.repr << ", " << mask << "\n";
                    m_body << "  " << quot << " = ashr " << TyStr(t) << " " << adj << ", " << shift << "\n";
                    if (inst.opcode == IROpcode::Mod) {
                        const std::string prod = NewTmp();
                        m_body << "  " << prod << " = shl " << TyStr(t) << " " << quot << ", " << shift << "\n";
                        m_body << "  " << vname << " = sub " << TyStr(t) << " " << a.repr << ", " << prod << "\n";
                    }
                }
                m_vals[id] = Val{ vname, t, uns };
                break;
            }
            const bool fastFp = (m_ctx.options.profile == BuildProfile::Release);
            const char* op = nullptr;
            if (IsFloatTy(t)) {
                if (fastFp) {
                    op = (inst.opcode == IROpcode::Add) ? "fadd fast" : (inst.opcode == IROpcode::Sub) ? "fsub fast"
                       : (inst.opcode == IROpcode::Mul) ? "fmul fast" : (inst.opcode == IROpcode::Div) ? "fdiv fast" : "frem fast";
                } else {
                    op = (inst.opcode == IROpcode::Add) ? "fadd" : (inst.opcode == IROpcode::Sub) ? "fsub"
                       : (inst.opcode == IROpcode::Mul) ? "fmul" : (inst.opcode == IROpcode::Div) ? "fdiv" : "frem";
                }
            } else {
                // Release integer arithmetic is defined two's-complement wrap.
                // `nsw` is signed-overflow UB and blocks reassociation of
                // expressions such as (x * C1 + C2) * C3 + y.
                const bool uns = OperandUnsigned(inst, 0) || OperandUnsigned(inst, 1);
                op = (inst.opcode == IROpcode::Add) ? "add" : (inst.opcode == IROpcode::Sub) ? "sub"
                   : (inst.opcode == IROpcode::Mul) ? "mul"
                   : (inst.opcode == IROpcode::Div) ? (uns ? "udiv" : "sdiv")
                                                    : (uns ? "urem" : "srem");
            }
            m_body << "  " << vname << " = " << op << " " << TyStr(t) << " " << a.repr << ", " << b.repr << "\n";
            m_vals[id] = Val{ vname, t };
            break;
        }

        case IROpcode::Eq:
        case IROpcode::Ne:
        case IROpcode::Lt:
        case IROpcode::Le:
        case IROpcode::Gt:
        case IROpcode::Ge: {
            Val a = Operand(inst, 0);
            Val b = Operand(inst, 1);
            if (a.ty == NTy::Void || b.ty == NTy::Void) {
                Err("N9001", "Cannot compare a void value.", inst.location);
                m_vals[id] = Val{ "false", NTy::I1 };
                break;
            }
            if (a.ty == NTy::Ptr && b.ty == NTy::Ptr &&
                (inst.opcode == IROpcode::Eq || inst.opcode == IROpcode::Ne)) {
                const std::string cmp = NewTmp();
                m_body << "  " << cmp << " = call i32 @strcmp(ptr " << a.repr << ", ptr " << b.repr << ")\n";
                const char* pred = inst.opcode == IROpcode::Eq ? "eq" : "ne";
                m_body << "  " << vname << " = icmp " << pred << " i32 " << cmp << ", 0\n";
                m_vals[id] = Val{ vname, NTy::I1 };
                break;
            }
            if (a.ty == NTy::Ptr || b.ty == NTy::Ptr) {
                Err("N9001", "This pointer comparison is not yet supported by the native backend.", inst.location);
                m_vals[id] = Val{ "false", NTy::I1 };
                break;
            }
            const NTy t = Promote(a.ty, b.ty);
            a = Coerce(a, t, inst.location);
            b = Coerce(b, t, inst.location);
            if (IsFloatTy(t)) {
                const char* cc = (inst.opcode == IROpcode::Eq) ? "oeq" : (inst.opcode == IROpcode::Ne) ? "une"
                               : (inst.opcode == IROpcode::Lt) ? "olt" : (inst.opcode == IROpcode::Le) ? "ole"
                               : (inst.opcode == IROpcode::Gt) ? "ogt" : "oge";
                const char* fcmpOp = (m_ctx.options.profile == BuildProfile::Release) ? "fcmp fast " : "fcmp ";
                m_body << "  " << vname << " = " << fcmpOp << cc << " " << TyStr(t) << " " << a.repr << ", " << b.repr << "\n";
            } else {
                const bool uns = OperandUnsigned(inst, 0) || OperandUnsigned(inst, 1);
                const char* cc = (inst.opcode == IROpcode::Eq) ? "eq" : (inst.opcode == IROpcode::Ne) ? "ne"
                               : (inst.opcode == IROpcode::Lt) ? (uns ? "ult" : "slt")
                               : (inst.opcode == IROpcode::Le) ? (uns ? "ule" : "sle")
                               : (inst.opcode == IROpcode::Gt) ? (uns ? "ugt" : "sgt")
                                                               : (uns ? "uge" : "sge");
                m_body << "  " << vname << " = icmp " << cc << " " << TyStr(t) << " " << a.repr << ", " << b.repr << "\n";
            }
            m_vals[id] = Val{ vname, NTy::I1 };
            break;
        }

        case IROpcode::And:
        case IROpcode::Or: {
            Val a = Coerce(Operand(inst, 0), NTy::I1, inst.location);
            Val b = Coerce(Operand(inst, 1), NTy::I1, inst.location);
            m_body << "  " << vname << " = " << (inst.opcode == IROpcode::And ? "and" : "or")
                   << " i1 " << a.repr << ", " << b.repr << "\n";
            m_vals[id] = Val{ vname, NTy::I1 };
            break;
        }

        case IROpcode::Not: {
            Val a = Coerce(Operand(inst, 0), NTy::I1, inst.location);
            m_body << "  " << vname << " = xor i1 " << a.repr << ", true\n";
            m_vals[id] = Val{ vname, NTy::I1 };
            break;
        }

        case IROpcode::BitAnd:
        case IROpcode::BitOr:
        case IROpcode::BitXor:
        case IROpcode::Shl:
        case IROpcode::Shr: {
            Val a = Coerce(Operand(inst, 0), NTy::I64, inst.location);
            Val b = Coerce(Operand(inst, 1), NTy::I64, inst.location);
            const char* op = "and";
            if (inst.opcode == IROpcode::BitOr) op = "or";
            else if (inst.opcode == IROpcode::BitXor) op = "xor";
            else if (inst.opcode == IROpcode::Shl) op = "shl";
            else if (inst.opcode == IROpcode::Shr) op = OperandUnsigned(inst, 0) ? "lshr" : "ashr";
            m_body << "  " << vname << " = " << op << " i64 " << a.repr << ", " << b.repr << "\n";
            m_vals[id] = Val{ vname, NTy::I64, OperandUnsigned(inst, 0) };
            break;
        }

        case IROpcode::BitNot: {
            Val a = Coerce(Operand(inst, 0), NTy::I64, inst.location);
            m_body << "  " << vname << " = xor i64 " << a.repr << ", -1\n";
            m_vals[id] = Val{ vname, NTy::I64, OperandUnsigned(inst, 0) };
            break;
        }

        case IROpcode::Neg: {
            Val a = Operand(inst, 0);
            if (a.ty == NTy::Ptr || a.ty == NTy::Void) {
                Err("N9001", "Negation of strings or void values is not supported.", inst.location);
                m_vals[id] = Val{ "0", NTy::I64 };
                break;
            }
            if (a.ty == NTy::I1) a = Coerce(a, NTy::I64, inst.location);
            if (IsFloatTy(a.ty)) {
                const char* fnegOp = (m_ctx.options.profile == BuildProfile::Release) ? "fneg fast " : "fneg ";
                m_body << "  " << vname << " = " << fnegOp << TyStr(a.ty) << " " << a.repr << "\n";
            } else {
                m_body << "  " << vname << " = sub " << TyStr(a.ty) << " 0, " << a.repr << "\n";
            }
            m_vals[id] = Val{ vname, a.ty };
            break;
        }

        case IROpcode::CallDirect: {
            auto cit = m_ctx.byName.find(inst.textImm);
            if (cit == m_ctx.byName.end()) {
                Err("N9004", "Call to unknown or unsupported function '" + inst.textImm + "'.", inst.location,
                    "Native backend can only call functions defined in the same file.");
                m_vals[id] = Val{ "0", NTy::I64 };
                break;
            }
            NTy ret{};
            std::vector<NTy> ptys;
            std::string why;
            if (!FunctionSignature(*cit->second, ret, ptys, why)) {
                Err("N9008", "Callee '" + inst.textImm + "' uses unsupported native types: " + why + ".", inst.location);
                m_vals[id] = Val{ "0", NTy::I64 };
                break;
            }
            if (inst.operandIds.size() != ptys.size()) {
                Err("N9005", "Call to '" + inst.textImm + "' passes " + std::to_string(inst.operandIds.size()) +
                                 " argument(s) but the function declares " + std::to_string(ptys.size()) + ".",
                    inst.location);
                m_vals[id] = Val{ "0", NTy::I64 };
                break;
            }
            std::string args;
            for (std::size_t i = 0; i < ptys.size(); ++i) {
                Val a = Coerce(Operand(inst, i), ptys[i], inst.location);
                if (i > 0) args += ", ";
                args += std::string(TyStr(ptys[i])) + " " + a.repr;
            }
            const bool cpuFn = m_ctx.cpuFns.count(cit->second->name) != 0;
            const bool ext = cit->second->isExtern || cpuFn;
            const std::string callee = cpuFn ? ("@rx_fn_" + cit->second->name)
                                             : ext ? ("@" + cit->second->mangledName)
                                                   : ("@\"" + cit->second->mangledName + "\"");
            const char* cc = ext ? "" : "fastcc ";
            if (ret == NTy::Void) {
                m_body << "  call " << cc << "void " << callee << "(" << args << ")\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else {
                m_body << "  " << vname << " = call " << cc << TyStr(ret) << " " << callee << "(" << args << ")\n";
                m_vals[id] = Val{ vname, ret, IsUnsignedName(cit->second->returnType.baseName) };
            }
            break;
        }

        case IROpcode::PrintVal: {
            Val a = Operand(inst, 0);
            if (a.ty == NTy::Ptr) {
                m_body << "  call void @rx_print_str(ptr " << a.repr << ")\n";
            } else if (a.ty == NTy::I1) {
                const std::string t = NewTmp();
                m_body << "  " << t << " = zext i1 " << a.repr << " to i32\n";
                m_body << "  call void @rx_print_bool(i32 " << t << ")\n";
            } else if (IsIntTy(a.ty)) {
                Val w = Coerce(a, NTy::I64, inst.location);
                if (m_ctx.printFmt < 0) {
                    m_ctx.printFmt = static_cast<int>(m_ctx.stringLiterals.size());
                    m_ctx.stringLiterals.emplace_back("%lld\n");
                }
                m_body << "  call i32 (ptr, ...) @printf(ptr @.str." << m_ctx.printFmt
                       << ", i64 " << w.repr << ")\n";
            } else if (IsFloatTy(a.ty)) {
                Val w = Coerce(a, NTy::F64, inst.location);
                m_body << "  call void @rx_print_f64(double " << w.repr << ")\n";
            } else {
                Err("N9007", "Cannot Print a void value.", inst.location);
            }
            break;
        }

        case IROpcode::InlineASM: {
            const std::string mnemonic = FirstMnemonic(inst.textImm);
            if (mnemonic.empty() || !AsmMnemonicAllowed(m_ctx.options.asmArch, mnemonic)) {
                Err("N9010",
                    "Instruction '" + mnemonic + "' is not in the " + m_ctx.options.asmArch + " assembly allowlist.",
                    inst.location);
                break;
            }
            const auto split = inst.secondaryText.find('\n');
            const std::string output = split == std::string::npos ? inst.secondaryText
                                                                  : inst.secondaryText.substr(0, split);
            const std::string clobber = split == std::string::npos ? std::string{}
                                                                   : inst.secondaryText.substr(split + 1);
            std::string text = inst.textImm;
            if (inst.operandIds.empty() && output.empty()) {
                std::string doubled;
                doubled.reserve(text.size() + 4);
                for (char ch : text) {
                    if (ch == '$') doubled.push_back('$');
                    doubled.push_back(ch);
                }
                text = std::move(doubled);
            }
            std::string constraints;
            auto addConstraint = [&](const std::string& piece) {
                if (!constraints.empty()) constraints += ",";
                constraints += piece;
            };
            if (!output.empty()) addConstraint("=r");
            for (std::size_t i = 0; i < inst.operandIds.size(); ++i) addConstraint("r");
            std::size_t cursor = 0;
            while (cursor < clobber.size()) {
                const std::size_t comma = clobber.find(',', cursor);
                std::string name = clobber.substr(cursor, comma == std::string::npos ? std::string::npos : comma - cursor);
                const auto first = name.find_first_not_of(" \t");
                const auto last = name.find_last_not_of(" \t");
                if (first != std::string::npos) {
                    name = name.substr(first, last - first + 1);
                    addConstraint("~{" + name + "}");
                }
                if (comma == std::string::npos) break;
                cursor = comma + 1;
            }
            std::string args;
            for (std::size_t i = 0; i < inst.operandIds.size(); ++i) {
                if (i > 0) args += ", ";
                Val in = Coerce(Operand(inst, i), NTy::I64, inst.location);
                args += "i64 " + in.repr;
            }
            const std::string escaped = EscapeLLVMString(text);
            if (output.empty()) {
                m_body << "  call void asm sideeffect \"" << escaped << "\", \"" << constraints << "\"(" << args << ")\n";
            } else {
                m_body << "  " << vname << " = call i64 asm sideeffect \"" << escaped << "\", \""
                       << constraints << "\"(" << args << ")\n";
                if (m_varTy.find(output) == m_varTy.end()) {
                    EnsureVar(output, NTy::I64);
                }
                m_body << "  store i64 " << vname << ", ptr " << SlotName(output) << ", align 8\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            }
            break;
        }

        case IROpcode::Jump:
            m_body << "  br label %" << inst.textImm << "\n";
            break;

        case IROpcode::BranchCond: {
            if (!TryEmitSwitch(inst)) {
                Val c = Coerce(Operand(inst, 0), NTy::I1, inst.location);
                m_body << "  br i1 " << c.repr << ", label %" << inst.textImm << ", label %" << inst.secondaryText << "\n";
            }
            break;
        }

        case IROpcode::ReturnVal: {
            if (m_ret == NTy::Void) {
                m_body << "  ret void\n";
            } else {
                Val v = Coerce(Operand(inst, 0), m_ret, inst.location);
                m_body << "  ret " << TyStr(m_ret) << " " << v.repr << "\n";
            }
            break;
        }

        case IROpcode::ReturnVoid:
            if (m_ret == NTy::Void) {
                m_body << "  ret void\n";
            } else {
                m_body << "  ret " << TyStr(m_ret) << " " << Zero(m_ret) << "\n";
            }
            break;

        case IROpcode::AssertVal: {
            const bool isEqual = inst.textImm == "Assert.Equal" || inst.textImm == "AssertEqual";
            const bool isTrue = inst.textImm == "Assert.True" || inst.textImm == "AssertTrue" ||
                                inst.textImm == "Assert";
            const bool isPixel = inst.textImm == "AssertPixel";
            if (!isEqual && !isTrue && !isPixel) {
                Err("N9001", "Native tests compile Assert.Equal, Assert.True, and AssertPixel. '" + inst.textImm + "' stays on the interpreter.",
                    inst.location);
                break;
            }
            if (isPixel) {
                if (inst.operandIds.size() < 4) {
                    Err("N9001", "AssertPixel is missing an argument.", inst.location);
                    break;
                }
                m_ctx.usesFrame = true;
                Val frame = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val x = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                Val y = Coerce(Operand(inst, 2), NTy::I64, inst.location);
                Val expected = Coerce(Operand(inst, 3), NTy::I64, inst.location);
                const std::string got = NewTmp();
                const std::string cmp = NewTmp();
                m_body << "  " << got << " = call i64 @rx_frame_pixel(i64 " << frame.repr << ", i64 " << x.repr
                       << ", i64 " << y.repr << ")\n";
                m_body << "  " << cmp << " = icmp eq i64 " << got << ", " << expected.repr << "\n";
                const std::string bad = "assert_bad_" + std::to_string(m_tmp);
                const std::string ok = "assert_ok_" + std::to_string(m_tmp);
                ++m_tmp;
                m_body << "  br i1 " << cmp << ", label %" << ok << ", label %" << bad << "\n";
                m_body << bad << ":\n  call void @exit(i32 1)\n  unreachable\n";
                m_body << ok << ":\n";
                break;
            }
            const std::size_t need = isEqual ? 2 : 1;
            if (inst.operandIds.size() < need) {
                Err("N9001", inst.textImm + " is missing an argument.", inst.location);
                break;
            }
            Val left = Coerce(Operand(inst, 0), NTy::I64, inst.location);
            std::string cmp;
            if (isEqual) {
                Val right = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                cmp = NewTmp();
                m_body << "  " << cmp << " = icmp eq i64 " << left.repr << ", " << right.repr << "\n";
            } else {
                cmp = NewTmp();
                m_body << "  " << cmp << " = icmp ne i64 " << left.repr << ", 0\n";
            }
            const std::string bad = "assert_bad_" + std::to_string(m_tmp);
            const std::string ok = "assert_ok_" + std::to_string(m_tmp);
            ++m_tmp;
            m_body << "  br i1 " << cmp << ", label %" << ok << ", label %" << bad << "\n";
            m_body << bad << ":\n  call void @exit(i32 1)\n  unreachable\n";
            m_body << ok << ":\n";
            break;
        }

        case IROpcode::DeferCleanup:
            Err("N9001", "'defer'/'using' cleanup is not yet supported by the native backend.", inst.location);
            break;

        case IROpcode::CallVirtual: {
            if (inst.operandIds.empty()) {
                Err("N9005", "Virtual call '" + inst.textImm + "' is missing its receiver.", inst.location);
                m_vals[id] = Val{ "0", NTy::I64 };
                break;
            }
            Val obj = Operand(inst, 0);
            const std::string vt = NewTmp();
            const std::string slotp = NewTmp();
            const std::string fp = NewTmp();
            m_body << "  " << vt << " = load ptr, ptr " << obj.repr << ", align 8\n";
            m_body << "  " << slotp << " = getelementptr ptr, ptr " << vt << ", i64 " << inst.intImm << "\n";
            m_body << "  " << fp << " = load ptr, ptr " << slotp << ", align 8\n";
            NTy ret = NTy::Void;
            std::string why;
            if (!inst.dest.type.baseName.empty() && !MapType(inst.dest.type, ret, why)) {
                ret = NTy::Void;
            }
            std::string args;
            for (std::size_t i = 0; i < inst.operandIds.size(); ++i) {
                Val a = Operand(inst, i);
                NTy ty = a.ty == NTy::Void ? NTy::I64 : a.ty;
                Val c = Coerce(a, ty, inst.location);
                if (i > 0) args += ", ";
                args += std::string(TyStr(ty)) + " " + c.repr;
            }
            if (ret == NTy::Void) {
                m_body << "  call fastcc void " << fp << "(" << args << ")\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else {
                m_body << "  " << vname << " = call fastcc " << TyStr(ret) << " " << fp << "(" << args << ")\n";
                m_vals[id] = Val{ vname, ret };
            }
            break;
        }

        case IROpcode::CallOSIntrinsic: {
            if (inst.textImm == "Memory.Allocate" && !inst.operandIds.empty()) {
                Val n = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                const std::string p = NewTmp();
                m_body << "  " << p << " = call ptr @malloc(i64 " << n.repr << ")\n";
                m_body << "  call void @llvm.memset.p0.i64(ptr " << p << ", i8 0, i64 " << n.repr << ", i1 false)\n";
                m_vals[id] = Val{ p, NTy::Ptr };
            } else if (inst.textImm == "Memory.Free" && !inst.operandIds.empty()) {
                Val p = Operand(inst, 0);
                m_body << "  call void @free(ptr " << p.repr << ")\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "String.Length" && !inst.operandIds.empty()) {
                Val s = Operand(inst, 0);
                m_body << "  " << vname << " = call i64 @strlen(ptr " << s.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Args.Count") {
                m_body << "  " << vname << " = call i64 @rx_args_count()\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Args.Get" && !inst.operandIds.empty()) {
                Val index = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call ptr @rx_args_get(i64 " << index.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::Ptr };
            } else if (inst.textImm == "Env.Get" && !inst.operandIds.empty()) {
                Val name = Operand(inst, 0);
                m_body << "  " << vname << " = call ptr @rx_env_get(ptr " << name.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::Ptr };
            } else if (inst.textImm == "Env.Exists" && !inst.operandIds.empty()) {
                Val name = Operand(inst, 0);
                m_body << "  " << vname << " = call i64 @rx_env_exists(ptr " << name.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if ((inst.textImm == "Env.Set" || inst.textImm == "Env.Add") && inst.operandIds.size() >= 2) {
                Val name = Operand(inst, 0);
                Val value = Operand(inst, 1);
                const char* symbol = inst.textImm == "Env.Set" ? "@rx_env_set" : "@rx_env_add";
                m_body << "  " << vname << " = call i64 " << symbol << "(ptr " << name.repr << ", ptr " << value.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Env.Remove" && !inst.operandIds.empty()) {
                Val name = Operand(inst, 0);
                m_body << "  " << vname << " = call i64 @rx_env_remove(ptr " << name.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Network.Call" && inst.operandIds.size() >= 3) {
                Val service = Operand(inst, 0);
                Val method = Operand(inst, 1);
                Val text = Operand(inst, 2);
                m_body << "  " << vname << " = call ptr @rx_network_call(ptr " << service.repr
                       << ", ptr " << method.repr << ", ptr " << text.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::Ptr };
            } else if (inst.textImm == "Network.SendText" && !inst.operandIds.empty()) {
                Val text = Operand(inst, 0);
                m_body << "  " << vname << " = call ptr @rx_network_send_text(ptr " << text.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::Ptr };
            } else if (inst.textImm == "Input" || inst.textImm == "Input.Int") {
                const std::string promptRepr = inst.operandIds.empty() ? "null" : Operand(inst, 0).repr;
                if (inst.textImm == "Input.Int") {
                    m_body << "  " << vname << " = call i64 @rx_input_i64(ptr " << promptRepr << ")\n";
                    m_vals[id] = Val{ vname, NTy::I64 };
                } else {
                    m_body << "  " << vname << " = call ptr @rx_input_line(ptr " << promptRepr << ")\n";
                    m_vals[id] = Val{ vname, NTy::Ptr };
                }
            } else if (inst.textImm == "File.WriteText" && inst.operandIds.size() >= 2) {
                Val path = Operand(inst, 0);
                Val text = Operand(inst, 1);
                const std::string wrote = NewTmp();
                m_body << "  " << wrote << " = call i32 @rx_write_text(ptr " << path.repr << ", ptr " << text.repr << ")\n";
                m_body << "  " << vname << " = icmp ne i32 " << wrote << ", 0\n";
                m_vals[id] = Val{ vname, NTy::I1 };
            } else if (inst.textImm == "UI.LoadCooked" && !inst.operandIds.empty()) {
                m_ctx.usesUI = true;
                const std::string path = ConstText(inst.operandIds[0]);
                if (path.empty()) {
                    Err("N9011", "UI.LoadCooked needs a string literal path.", inst.location);
                    break;
                }
                std::vector<Robix::UI::RoboCookedBox> loaded;
                if (!Robix::UI::RoboCookedLoad(path, loaded)) {
                    Err("N9011", "UI.LoadCooked could not read '" + path + "'.", inst.location);
                    break;
                }
                m_ctx.uiBoxes = std::move(loaded);
                m_body << "  " << vname << " = call i64 @rx_ui_load()\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "UI.Role" || inst.textImm == "UI.BoxX" || inst.textImm == "UI.BoxY" ||
                       inst.textImm == "UI.BoxWidth" || inst.textImm == "UI.BoxHeight") {
                m_ctx.usesUI = true;
                int field = 0;
                if (inst.textImm == "UI.BoxX") field = 1;
                else if (inst.textImm == "UI.BoxY") field = 2;
                else if (inst.textImm == "UI.BoxWidth") field = 3;
                else if (inst.textImm == "UI.BoxHeight") field = 4;
                Val index = inst.operandIds.empty() ? Val{ "0", NTy::I64 }
                                                    : Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_ui_field(i64 " << index.repr << ", i64 " << field << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "UI.Hit" && inst.operandIds.size() >= 2) {
                m_ctx.usesUI = true;
                Val x = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val y = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_ui_hit(i64 " << x.repr << ", i64 " << y.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "UI.Drag" && inst.operandIds.size() >= 3) {
                m_ctx.usesUI = true;
                Val index = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val dx = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                Val dy = Coerce(Operand(inst, 2), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_ui_drag(i64 " << index.repr << ", i64 " << dx.repr
                       << ", i64 " << dy.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "UI.Snap" && inst.operandIds.size() >= 2) {
                m_ctx.usesUI = true;
                Val index = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val side = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_ui_snap(i64 " << index.repr << ", i64 " << side.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "UI.Place" && inst.operandIds.size() >= 5) {
                m_ctx.usesUI = true;
                Val index = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val x = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                Val y = Coerce(Operand(inst, 2), NTy::I64, inst.location);
                Val width = Coerce(Operand(inst, 3), NTy::I64, inst.location);
                Val height = Coerce(Operand(inst, 4), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_ui_place(i64 " << index.repr << ", i64 " << x.repr
                       << ", i64 " << y.repr << ", i64 " << width.repr << ", i64 " << height.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "UI.Focus" && !inst.operandIds.empty()) {
                m_ctx.usesUI = true;
                Val index = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_ui_focus(i64 " << index.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "UI.Focused" || inst.textImm == "UI.MenuOpen") {
                m_ctx.usesUI = true;
                const char* fn = inst.textImm == "UI.Focused" ? "rx_ui_focused" : "rx_ui_menu_open";
                m_body << "  " << vname << " = call i64 @" << fn << "()\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "UI.Menu" && !inst.operandIds.empty()) {
                m_ctx.usesUI = true;
                Val flag = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_ui_menu(i64 " << flag.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Scene.Render" && inst.operandIds.size() >= 3) {
                m_ctx.usesScene = true;
                const std::string path = ConstText(inst.operandIds[0]);
                const std::string camera = ConstText(inst.operandIds[2]);
                if (path.empty() || camera.empty() || !RoboRasterizeScene(path, camera, 320, 200, m_ctx.scenePixels)) {
                    Err("N9011", "Scene.Render needs a scene file, a frame, and a camera name.", inst.location);
                    break;
                }
                Val frame = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_scene_render(i64 " << frame.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Input.Play" && !inst.operandIds.empty()) {
                m_ctx.usesInput = true;
                const std::string script = ConstText(inst.operandIds[0]);
                if (script.empty() || !RoboParseInputScript(script, m_ctx.inputScript)) {
                    Err("N9011", "Input.Play needs a move, click, or key script.", inst.location);
                    break;
                }
                m_body << "  " << vname << " = call i64 @rx_input_play()\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Input.Count" || inst.textImm == "Input.Clicks" ||
                       inst.textImm == "Input.MouseX" || inst.textImm == "Input.MouseY" ||
                       inst.textImm == "Input.Pop") {
                m_ctx.usesInput = true;
                const char* fn = "rx_input_count";
                if (inst.textImm == "Input.Clicks") fn = "rx_input_clicks";
                else if (inst.textImm == "Input.MouseX") fn = "rx_input_mouse_x";
                else if (inst.textImm == "Input.MouseY") fn = "rx_input_mouse_y";
                else if (inst.textImm == "Input.Pop") fn = "rx_input_pop";
                m_body << "  " << vname << " = call i64 @" << fn << "()\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Input.Kind" || inst.textImm == "Input.Code" || inst.textImm == "Input.Button" ||
                       inst.textImm == "Input.X" || inst.textImm == "Input.Y") {
                m_ctx.usesInput = true;
                int field = 0;
                if (inst.textImm == "Input.Code") field = 1;
                else if (inst.textImm == "Input.Button") field = 2;
                else if (inst.textImm == "Input.X") field = 3;
                else if (inst.textImm == "Input.Y") field = 4;
                Val index = inst.operandIds.empty() ? Val{ "0", NTy::I64 }
                                                    : Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_input_field(i64 " << index.repr << ", i64 " << field << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Input.Push" && inst.operandIds.size() >= 5) {
                m_ctx.usesInput = true;
                Val type = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val code = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                Val value = Coerce(Operand(inst, 2), NTy::I64, inst.location);
                Val x = Coerce(Operand(inst, 3), NTy::I64, inst.location);
                Val y = Coerce(Operand(inst, 4), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_input_push(i64 " << type.repr << ", i64 " << code.repr
                       << ", i64 " << value.repr << ", i64 " << x.repr << ", i64 " << y.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Input.Clear") {
                m_ctx.usesInput = true;
                m_body << "  call void @rx_input_clear()\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "Input.SetMouse" && inst.operandIds.size() >= 2) {
                m_ctx.usesInput = true;
                Val x = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val y = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  call void @rx_input_set_mouse(i64 " << x.repr << ", i64 " << y.repr << ")\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "Input.AddClick") {
                m_ctx.usesInput = true;
                m_body << "  call void @rx_input_add_click()\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "File.ReadText" && !inst.operandIds.empty()) {
                Val path = Operand(inst, 0);
                m_body << "  " << vname << " = call ptr @rx_read_text(ptr " << path.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::Ptr };
            } else if (inst.textImm == "Volatile.Load" && !inst.operandIds.empty()) {
                Val p = Operand(inst, 0);
                m_body << "  " << vname << " = load volatile i64, ptr " << p.repr << ", align 1\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Volatile.Store" && inst.operandIds.size() >= 2) {
                Val p = Operand(inst, 0);
                Val stored = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  store volatile i64 " << stored.repr << ", ptr " << p.repr << ", align 1\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "Volatile.Load32" && !inst.operandIds.empty()) {
                Val p = Operand(inst, 0);
                const std::string raw = NewTmp();
                m_body << "  " << raw << " = load volatile i32, ptr " << p.repr << ", align 1\n";
                m_body << "  " << vname << " = zext i32 " << raw << " to i64\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Volatile.Store32" && inst.operandIds.size() >= 2) {
                Val p = Operand(inst, 0);
                Val stored = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                const std::string narrow = NewTmp();
                m_body << "  " << narrow << " = trunc i64 " << stored.repr << " to i32\n";
                m_body << "  store volatile i32 " << narrow << ", ptr " << p.repr << ", align 1\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "CPU.Gate" && !inst.operandIds.empty()) {
                Val handler = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                const std::string low = NewTmp();
                const std::string shifted = NewTmp();
                const std::string mid = NewTmp();
                const std::string placed = NewTmp();
                const std::string withSel = NewTmp();
                const std::string withType = NewTmp();
                m_body << "  " << low << " = and i64 " << handler.repr << ", 65535\n";
                m_body << "  " << shifted << " = lshr i64 " << handler.repr << ", 16\n";
                m_body << "  " << mid << " = and i64 " << shifted << ", 65535\n";
                m_body << "  " << placed << " = shl i64 " << mid << ", 48\n";
                m_body << "  " << withSel << " = or i64 " << low << ", 524288\n";
                m_body << "  " << withType << " = or i64 " << withSel << ", 156130651144192\n";
                m_body << "  " << vname << " = or i64 " << withType << ", " << placed << "\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "CPU.GateHigh" && !inst.operandIds.empty()) {
                Val handler = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = lshr i64 " << handler.repr << ", 32\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Sched.Enqueue" && inst.operandIds.size() >= 2) {
                m_ctx.usesSched = true;
                Val ident = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val pri = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_sched_enqueue(i64 " << ident.repr << ", i64 " << pri.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Sched.Pick" && !inst.operandIds.empty()) {
                m_ctx.usesSched = true;
                Val core = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_sched_pick(i64 " << core.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Sched.Block" && !inst.operandIds.empty()) {
                m_ctx.usesSched = true;
                Val ident = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_sched_block(i64 " << ident.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Port.Create" && !inst.operandIds.empty()) {
                m_ctx.usesPort = true;
                Val owner = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_port_create(i64 " << owner.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Port.Send" && inst.operandIds.size() >= 2) {
                m_ctx.usesPort = true;
                Val port = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val word = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_port_send(i64 " << port.repr << ", i64 " << word.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Port.Receive" && !inst.operandIds.empty()) {
                m_ctx.usesPort = true;
                Val port = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_port_receive(i64 " << port.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Proc.Create" && inst.operandIds.size() >= 2) {
                m_ctx.usesProc = true;
                Val parent = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val user = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_proc_create(i64 " << parent.repr << ", i64 " << user.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Proc.Exit" && inst.operandIds.size() >= 2) {
                m_ctx.usesProc = true;
                Val ident = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val status = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_proc_exit(i64 " << ident.repr << ", i64 " << status.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Proc.Wait" && !inst.operandIds.empty()) {
                m_ctx.usesProc = true;
                Val ident = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_proc_wait(i64 " << ident.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Cred.Allow" && inst.operandIds.size() >= 2) {
                m_ctx.usesProc = true;
                Val user = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val right = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_cred_allow(i64 " << user.repr << ", i64 " << right.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Syscall.Dispatch" && inst.operandIds.size() >= 3) {
                m_ctx.usesDispatch = true;
                Val caller = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val number = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                Val argument = Coerce(Operand(inst, 2), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_syscall_dispatch(i64 " << caller.repr << ", i64 " << number.repr
                       << ", i64 " << argument.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "FS.Lookup" && !inst.operandIds.empty()) {
                m_ctx.usesFs = true;
                Val path = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_fs_lookup(i64 " << path.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "FS.Create" && inst.operandIds.size() >= 2) {
                m_ctx.usesFs = true;
                Val path = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val value = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_fs_create(i64 " << path.repr << ", i64 " << value.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "FS.Read" && !inst.operandIds.empty()) {
                m_ctx.usesFs = true;
                Val path = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_fs_read(i64 " << path.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "FS.Write" && inst.operandIds.size() >= 2) {
                m_ctx.usesFs = true;
                Val path = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val value = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_fs_write(i64 " << path.repr << ", i64 " << value.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "FS.List" && !inst.operandIds.empty()) {
                m_ctx.usesFs = true;
                Val index = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_fs_list(i64 " << index.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Clock.Seconds" && !inst.operandIds.empty()) {
                Val cmos = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                const std::string tens = NewTmp();
                const std::string ones = NewTmp();
                const std::string scaled = NewTmp();
                m_body << "  " << tens << " = lshr i64 " << cmos.repr << ", 4\n";
                m_body << "  " << tens << "m = and i64 " << tens << ", 15\n";
                m_body << "  " << ones << " = and i64 " << cmos.repr << ", 15\n";
                m_body << "  " << scaled << " = mul i64 " << tens << "m, 10\n";
                m_body << "  " << vname << " = add i64 " << scaled << ", " << ones << "\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Port.Write8" && inst.operandIds.size() >= 2) {
                if (m_ctx.options.asmArch != "x86_64") {
                    Err("N9010", "Port.Write8 exists on x86_64 only.", inst.location);
                    break;
                }
                Val port = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val value = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                const std::string p16 = NewTmp();
                const std::string v8 = NewTmp();
                m_body << "  " << p16 << " = trunc i64 " << port.repr << " to i16\n";
                m_body << "  " << v8 << " = trunc i64 " << value.repr << " to i8\n";
                m_body << "  call void asm sideeffect \"outb $0, $1\", \"{ax},{dx}\"(i8 " << v8 << ", i16 " << p16 << ")\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "Port.Write16" && inst.operandIds.size() >= 2) {
                if (m_ctx.options.asmArch != "x86_64") {
                    Err("N9010", "Port.Write16 exists on x86_64 only.", inst.location);
                    break;
                }
                Val port = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val value = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                const std::string p16 = NewTmp();
                const std::string v16 = NewTmp();
                m_body << "  " << p16 << " = trunc i64 " << port.repr << " to i16\n";
                m_body << "  " << v16 << " = trunc i64 " << value.repr << " to i16\n";
                m_body << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 " << v16 << ", i16 " << p16 << ")\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "Port.Read8" && !inst.operandIds.empty()) {
                if (m_ctx.options.asmArch != "x86_64") {
                    Err("N9010", "Port.Read8 exists on x86_64 only.", inst.location);
                    break;
                }
                Val port = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                const std::string p16 = NewTmp();
                const std::string raw = NewTmp();
                m_body << "  " << p16 << " = trunc i64 " << port.repr << " to i16\n";
                m_body << "  " << raw << " = call i8 asm sideeffect \"inb $1, $0\", \"={ax},{dx}\"(i16 " << p16 << ")\n";
                m_body << "  " << vname << " = zext i8 " << raw << " to i64\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Port.Read16" && !inst.operandIds.empty()) {
                if (m_ctx.options.asmArch != "x86_64") {
                    Err("N9010", "Port.Read16 exists on x86_64 only.", inst.location);
                    break;
                }
                Val port = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                const std::string p16 = NewTmp();
                const std::string raw = NewTmp();
                m_body << "  " << p16 << " = trunc i64 " << port.repr << " to i16\n";
                m_body << "  " << raw << " = call i16 asm sideeffect \"inw $1, $0\", \"={ax},{dx}\"(i16 " << p16 << ")\n";
                m_body << "  " << vname << " = zext i16 " << raw << " to i64\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Port.Write32" && inst.operandIds.size() >= 2) {
                if (m_ctx.options.asmArch != "x86_64") {
                    Err("N9010", "Port.Write32 exists on x86_64 only.", inst.location);
                    break;
                }
                Val port = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val value = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                const std::string p16 = NewTmp();
                const std::string v32 = NewTmp();
                m_body << "  " << p16 << " = trunc i64 " << port.repr << " to i16\n";
                m_body << "  " << v32 << " = trunc i64 " << value.repr << " to i32\n";
                m_body << "  call void asm sideeffect \"outl $0, $1\", \"{ax},{dx}\"(i32 " << v32 << ", i16 " << p16 << ")\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "Port.Read32" && !inst.operandIds.empty()) {
                if (m_ctx.options.asmArch != "x86_64") {
                    Err("N9010", "Port.Read32 exists on x86_64 only.", inst.location);
                    break;
                }
                Val port = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                const std::string p16 = NewTmp();
                const std::string raw = NewTmp();
                m_body << "  " << p16 << " = trunc i64 " << port.repr << " to i16\n";
                m_body << "  " << raw << " = call i32 asm sideeffect \"inl $1, $0\", \"={ax},{dx}\"(i16 " << p16 << ")\n";
                m_body << "  " << vname << " = zext i32 " << raw << " to i64\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "PCI.Decode" && inst.operandIds.size() >= 2) {
                Val word0 = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val word8 = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                const std::string vendor = NewTmp();
                const std::string missing = NewTmp();
                const std::string device = NewTmp();
                const std::string deviceShift = NewTmp();
                const std::string klass = NewTmp();
                const std::string klassShift = NewTmp();
                const std::string low = NewTmp();
                const std::string packed = NewTmp();
                m_body << "  " << vendor << " = and i64 " << word0.repr << ", 65535\n";
                m_body << "  " << missing << " = icmp eq i64 " << vendor << ", 65535\n";
                m_body << "  " << deviceShift << " = lshr i64 " << word0.repr << ", 16\n";
                m_body << "  " << device << " = and i64 " << deviceShift << ", 65535\n";
                m_body << "  " << device << "p = shl i64 " << device << ", 16\n";
                m_body << "  " << klassShift << " = lshr i64 " << word8.repr << ", 24\n";
                m_body << "  " << klass << " = and i64 " << klassShift << ", 255\n";
                m_body << "  " << klass << "p = shl i64 " << klass << ", 32\n";
                m_body << "  " << low << " = or i64 " << vendor << ", " << device << "p\n";
                m_body << "  " << packed << " = or i64 " << low << ", " << klass << "p\n";
                m_body << "  " << vname << " = select i1 " << missing << ", i64 -1, i64 " << packed << "\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "HID.Parse" && inst.operandIds.size() >= 2) {
                Val kind = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val report = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                const std::string mouse = NewTmp();
                const std::string wheelKind = NewTmp();
                const std::string touch = NewTmp();
                const std::string bytes = NewTmp();
                const std::string wheelShift = NewTmp();
                const std::string wheel = NewTmp();
                const std::string high = NewTmp();
                const std::string signedWheel = NewTmp();
                const std::string chosen = NewTmp();
                const std::string touched = NewTmp();
                m_body << "  " << mouse << " = icmp eq i64 " << kind.repr << ", 0\n";
                m_body << "  " << wheelKind << " = icmp eq i64 " << kind.repr << ", 1\n";
                m_body << "  " << touch << " = icmp eq i64 " << kind.repr << ", 2\n";
                m_body << "  " << bytes << " = and i64 " << report.repr << ", 16777215\n";
                m_body << "  " << wheelShift << " = lshr i64 " << report.repr << ", 24\n";
                m_body << "  " << wheel << " = and i64 " << wheelShift << ", 255\n";
                m_body << "  " << high << " = icmp uge i64 " << wheel << ", 128\n";
                m_body << "  " << signedWheel << " = select i1 " << high << ", i64 " << wheel << ", i64 " << wheel << "\n";
                m_body << "  " << signedWheel << "s = sub i64 " << signedWheel << ", 256\n";
                m_body << "  " << signedWheel << "v = select i1 " << high << ", i64 " << signedWheel << "s, i64 " << wheel << "\n";
                m_body << "  " << chosen << " = select i1 " << mouse << ", i64 " << bytes << ", i64 -1\n";
                m_body << "  " << chosen << "w = select i1 " << wheelKind << ", i64 " << signedWheel << "v, i64 " << chosen << "\n";
                m_body << "  " << touched << " = select i1 " << touch << ", i64 " << bytes << ", i64 " << chosen << "w\n";
                m_body << "  " << vname << " = add i64 " << touched << ", 0\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Driver.Match" && inst.operandIds.size() >= 2) {
                Val device = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val personalities = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                const std::string count = NewTmp();
                const std::string none = NewTmp();
                const std::string many = NewTmp();
                const std::string bad = NewTmp();
                const std::string devShift = NewTmp();
                const std::string devClass = NewTmp();
                const std::string perShift = NewTmp();
                const std::string perClass = NewTmp();
                const std::string same = NewTmp();
                const std::string idShift = NewTmp();
                const std::string ident = NewTmp();
                const std::string matched = NewTmp();
                m_body << "  " << count << " = and i64 " << personalities.repr << ", 255\n";
                m_body << "  " << none << " = icmp eq i64 " << count << ", 0\n";
                m_body << "  " << many << " = icmp sgt i64 " << count << ", 1\n";
                m_body << "  " << bad << " = or i1 " << none << ", " << many << "\n";
                m_body << "  " << devShift << " = lshr i64 " << device.repr << ", 32\n";
                m_body << "  " << devClass << " = and i64 " << devShift << ", 255\n";
                m_body << "  " << perShift << " = lshr i64 " << personalities.repr << ", 16\n";
                m_body << "  " << perClass << " = and i64 " << perShift << ", 255\n";
                m_body << "  " << same << " = icmp eq i64 " << devClass << ", " << perClass << "\n";
                m_body << "  " << idShift << " = lshr i64 " << personalities.repr << ", 8\n";
                m_body << "  " << ident << " = and i64 " << idShift << ", 255\n";
                m_body << "  " << matched << " = select i1 " << same << ", i64 " << ident << ", i64 -1\n";
                m_body << "  " << vname << " = select i1 " << bad << ", i64 -1, i64 " << matched << "\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Driver.Bind" && inst.operandIds.size() >= 3) {
                Val device = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val builtIn = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                Val installed = Coerce(Operand(inst, 2), NTy::I64, inst.location);
                const std::string vendor = NewTmp();
                const std::string devShift = NewTmp();
                const std::string devId = NewTmp();
                const std::string pack = NewTmp();
                const std::string venShift = NewTmp();
                const std::string packVen = NewTmp();
                const std::string idShift = NewTmp();
                const std::string packDev = NewTmp();
                const std::string has = NewTmp();
                const std::string venOk = NewTmp();
                const std::string devOk = NewTmp();
                const std::string idOk = NewTmp();
                const std::string pair = NewTmp();
                const std::string hit = NewTmp();
                const std::string built = NewTmp();
                const std::string fallback = NewTmp();
                m_body << "  " << vendor << " = and i64 " << device.repr << ", 65535\n";
                m_body << "  " << devShift << " = lshr i64 " << device.repr << ", 16\n";
                m_body << "  " << devId << " = and i64 " << devShift << ", 65535\n";
                m_body << "  " << pack << " = and i64 " << installed.repr << ", 65535\n";
                m_body << "  " << venShift << " = lshr i64 " << installed.repr << ", 16\n";
                m_body << "  " << packVen << " = and i64 " << venShift << ", 65535\n";
                m_body << "  " << idShift << " = lshr i64 " << installed.repr << ", 32\n";
                m_body << "  " << packDev << " = and i64 " << idShift << ", 65535\n";
                m_body << "  " << has << " = icmp ne i64 " << installed.repr << ", 0\n";
                m_body << "  " << venOk << " = icmp eq i64 " << packVen << ", " << vendor << "\n";
                m_body << "  " << devOk << " = icmp eq i64 " << packDev << ", " << devId << "\n";
                m_body << "  " << idOk << " = icmp ne i64 " << pack << ", 0\n";
                m_body << "  " << pair << " = and i1 " << venOk << ", " << devOk << "\n";
                m_body << "  " << pair << "b = and i1 " << pair << ", " << idOk << "\n";
                m_body << "  " << hit << " = and i1 " << has << ", " << pair << "b\n";
                m_body << "  " << built << " = icmp ne i64 " << builtIn.repr << ", 0\n";
                m_body << "  " << fallback << " = select i1 " << built << ", i64 " << builtIn.repr << ", i64 -1\n";
                m_body << "  " << vname << " = select i1 " << hit << ", i64 " << pack << ", i64 " << fallback << "\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Driver.Package" && !inst.operandIds.empty()) {
                m_ctx.usesPackage = true;
                Val bytes = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_pkg(i64 " << bytes.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Driver.Newer" && inst.operandIds.size() >= 2) {
                Val current = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val offered = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                const std::string newer = NewTmp();
                m_body << "  " << newer << " = icmp sgt i64 " << offered.repr << ", " << current.repr << "\n";
                m_body << "  " << vname << " = zext i1 " << newer << " to i64\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Vendor.Match" && inst.operandIds.size() >= 4) {
                m_ctx.usesPackage = true;
                Val catalog = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val vendor = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                Val device = Coerce(Operand(inst, 2), NTy::I64, inst.location);
                Val klass = Coerce(Operand(inst, 3), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_vendor_match(i64 " << catalog.repr << ", i64 " << vendor.repr
                       << ", i64 " << device.repr << ", i64 " << klass.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "GPU.Command" && inst.operandIds.size() >= 3) {
                m_ctx.usesGpu = true;
                m_ctx.usesFrame = true;
                Val op = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val rect = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                Val color = Coerce(Operand(inst, 2), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_gpu_command(i64 " << op.repr << ", i64 " << rect.repr
                       << ", i64 " << color.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "GPU.Submit" && !inst.operandIds.empty()) {
                m_ctx.usesGpu = true;
                m_ctx.usesFrame = true;
                Val buffer = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_gpu_submit(i64 " << buffer.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Page.Entry" && inst.operandIds.size() >= 2) {
                Val phys = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val flags = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                const std::string masked = NewTmp();
                m_body << "  " << masked << " = and i64 " << phys.repr << ", -4096\n";
                m_body << "  " << vname << " = or i64 " << masked << ", " << flags.repr << "\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Page.Split" && inst.operandIds.size() >= 2) {
                Val root = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val userStart = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                const std::string aligned = NewTmp();
                const std::string user = NewTmp();
                const std::string hasUser = NewTmp();
                const std::string userFlags = NewTmp();
                const std::string placed = NewTmp();
                const std::string withKernel = NewTmp();
                m_body << "  " << aligned << " = and i64 " << root.repr << ", -4096\n";
                m_body << "  " << hasUser << " = icmp sgt i64 " << userStart.repr << ", 0\n";
                m_body << "  " << userFlags << " = select i1 " << hasUser << ", i64 7, i64 3\n";
                m_body << "  " << placed << " = shl i64 " << userFlags << ", 8\n";
                m_body << "  " << withKernel << " = or i64 " << aligned << ", 3\n";
                m_body << "  " << vname << " = or i64 " << withKernel << ", " << placed << "\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Fault.Decide" && !inst.operandIds.empty()) {
                Val fromUser = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                const std::string user = NewTmp();
                m_body << "  " << user << " = icmp ne i64 " << fromUser.repr << ", 0\n";
                m_body << "  " << vname << " = select i1 " << user << ", i64 1, i64 0\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Memory.Frame") {
                m_ctx.usesPages = true;
                m_body << "  " << vname << " = call i64 @rx_memory_frame()\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Memory.Pages" && !inst.operandIds.empty()) {
                Val count = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call ptr @rx_pages_take(i64 " << count.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::Ptr };
            } else if (inst.textImm == "Memory.Address" && !inst.operandIds.empty()) {
                Val p = Operand(inst, 0);
                if (p.ty == NTy::Ptr) {
                    m_body << "  " << vname << " = ptrtoint ptr " << p.repr << " to i64\n";
                } else {
                    Val n = Coerce(p, NTy::I64, inst.location);
                    m_body << "  " << vname << " = add i64 " << n.repr << ", 0\n";
                }
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Memory.At" && !inst.operandIds.empty()) {
                Val addr = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = inttoptr i64 " << addr.repr << " to ptr\n";
                m_vals[id] = Val{ vname, NTy::Ptr };
            } else if (inst.textImm == "CPU.TSC" || inst.textImm == "CPU.ReadCR3" || inst.textImm == "CPU.ReadRSP" ||
                       inst.textImm == "CPU.WriteCR3" || inst.textImm == "CPU.ExchangeRSP" ||
                       inst.textImm == "CPU.SavedRSP" || inst.textImm == "CPU.LGDT" ||
                       inst.textImm == "CPU.LIDT" || inst.textImm == "CPU.BootInfo" ||
                       inst.textImm == "CPU.Function" || inst.textImm == "CPU.Handler" ||
                       inst.textImm == "CPU.Fault" ||
                       inst.textImm == "CPU.Run" || inst.textImm == "CPU.Int3" || inst.textImm == "Timer.Arm" ||
                       inst.textImm == "Timer.Preempt" || inst.textImm == "System.Call") {
                if (m_ctx.options.asmArch != "x86_64") {
                    Err("N9010", inst.textImm + " exists on x86_64 only.", inst.location);
                    m_vals[id] = Val{ "0", NTy::I64 };
                    break;
                }
                if (inst.textImm == "CPU.TSC") {
                    m_body << "  " << vname
                           << " = call i64 asm sideeffect \"rdtsc\", \"=A,~{dirflag},~{fpsr},~{flags}\"()\n";
                    m_vals[id] = Val{ vname, NTy::I64 };
                } else if (inst.textImm == "CPU.Int3") {
                    m_body << "  call void asm sideeffect \"int3\", \"~{dirflag},~{fpsr},~{flags},~{memory}\"()\n";
                    m_vals[id] = Val{ "0", NTy::Void };
                } else if (inst.textImm == "CPU.ReadCR3" || inst.textImm == "CPU.ReadRSP") {
                    const char* text = inst.textImm == "CPU.ReadCR3" ? "movq %cr3, $0" : "movq %rsp, $0";
                    m_body << "  " << vname << " = call i64 asm sideeffect \"" << text
                           << "\", \"=r,~{dirflag},~{fpsr},~{flags}\"()\n";
                    m_vals[id] = Val{ vname, NTy::I64 };
                } else if (inst.textImm == "CPU.WriteCR3" && !inst.operandIds.empty()) {
                    Val addr = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                    m_body << "  call void asm sideeffect \"movq $0, %cr3\", \"r,~{dirflag},~{fpsr},~{flags},~{memory}\"(i64 "
                           << addr.repr << ")\n";
                    m_vals[id] = Val{ "0", NTy::Void };
                } else if (inst.textImm == "CPU.ExchangeRSP" && !inst.operandIds.empty()) {
                    Val next = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                    m_body << "  " << vname << " = call i64 @rx_exchange_rsp(i64 " << next.repr << ")\n";
                    m_vals[id] = Val{ vname, NTy::I64 };
                } else if (inst.textImm == "CPU.SavedRSP") {
                    m_body << "  " << vname << " = load i64, ptr @rx.switch.cell, align 8\n";
                    m_vals[id] = Val{ vname, NTy::I64 };
                } else if (inst.textImm == "CPU.BootInfo") {
                    const std::string raw = NewTmp();
                    m_body << "  " << raw << " = load i32, ptr inttoptr (i64 458752 to ptr), align 4\n";
                    m_body << "  " << vname << " = zext i32 " << raw << " to i64\n";
                    m_vals[id] = Val{ vname, NTy::I64 };
                } else if ((inst.textImm == "CPU.LGDT" || inst.textImm == "CPU.LIDT") && inst.operandIds.size() >= 2) {
                    Val base = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                    Val limit = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                    const std::string mem = NewTmp();
                    const std::string lim16 = NewTmp();
                    const std::string slot = NewTmp();
                    m_allocas << "  " << mem << " = alloca i8, i64 16, align 8\n";
                    m_body << "  " << lim16 << " = trunc i64 " << limit.repr << " to i16\n";
                    m_body << "  store i16 " << lim16 << ", ptr " << mem << ", align 2\n";
                    m_body << "  " << slot << " = getelementptr i8, ptr " << mem << ", i64 2\n";
                    m_body << "  store i64 " << base.repr << ", ptr " << slot << ", align 2\n";
                    const char* op = inst.textImm == "CPU.LGDT" ? "lgdt ($0)" : "lidt ($0)";
                    m_body << "  call void asm sideeffect \"" << op << "\", \"r,~{dirflag},~{fpsr},~{flags},~{memory}\"(ptr "
                           << mem << ")\n";
                    m_vals[id] = Val{ "0", NTy::Void };
                } else if (inst.textImm == "CPU.Function" || inst.textImm == "CPU.Handler" || inst.textImm == "CPU.Fault") {
                    const bool isr = inst.textImm == "CPU.Handler" || inst.textImm == "CPU.Fault";
                    const std::string sym = isr
                                                ? ("@rx_isr_" + inst.secondaryText)
                                                : ("@rx_fn_" + inst.secondaryText);
                    m_body << "  " << vname << " = ptrtoint ptr " << sym << " to i64\n";
                    m_vals[id] = Val{ vname, NTy::I64 };
                } else if (inst.textImm == "Timer.Arm") {
                    const std::string idt = NewTmp();
                    const std::string handler = NewTmp();
                    const std::string lowMask = NewTmp();
                    const std::string midShift = NewTmp();
                    const std::string midMask = NewTmp();
                    const std::string midPlace = NewTmp();
                    const std::string selector = NewTmp();
                    const std::string typeBit = NewTmp();
                    const std::string low0 = NewTmp();
                    const std::string low1 = NewTmp();
                    const std::string low = NewTmp();
                    const std::string high = NewTmp();
                    const std::string gate = NewTmp();
                    const std::string gateHi = NewTmp();
                    const std::string base = NewTmp();
                    const std::string mem = NewTmp();
                    const std::string lim16 = NewTmp();
                    const std::string slot = NewTmp();
                    auto outb = [&](int port, int value) {
                        const std::string p16 = NewTmp();
                        const std::string v8 = NewTmp();
                        m_body << "  " << p16 << " = trunc i64 " << port << " to i16\n";
                        m_body << "  " << v8 << " = trunc i64 " << value << " to i8\n";
                        m_body << "  call void asm sideeffect \"outb $0, $1\", \"{ax},{dx}\"(i8 " << v8 << ", i16 " << p16 << ")\n";
                    };
                    m_body << "  " << idt << " = call ptr @rx_pages_take(i64 1)\n";
                    m_body << "  " << handler << " = ptrtoint ptr @rx_isr_" << inst.secondaryText << " to i64\n";
                    m_body << "  " << lowMask << " = and i64 " << handler << ", 65535\n";
                    m_body << "  " << midShift << " = lshr i64 " << handler << ", 16\n";
                    m_body << "  " << midMask << " = and i64 " << midShift << ", 65535\n";
                    m_body << "  " << midPlace << " = shl i64 " << midMask << ", 48\n";
                    m_body << "  " << selector << " = shl i64 8, 16\n";
                    m_body << "  " << typeBit << " = shl i64 142, 40\n";
                    m_body << "  " << low0 << " = or i64 " << lowMask << ", " << selector << "\n";
                    m_body << "  " << low1 << " = or i64 " << low0 << ", " << typeBit << "\n";
                    m_body << "  " << low << " = or i64 " << low1 << ", " << midPlace << "\n";
                    m_body << "  " << high << " = lshr i64 " << handler << ", 32\n";
                    m_body << "  " << gate << " = getelementptr i8, ptr " << idt << ", i64 512\n";
                    m_body << "  store i64 " << low << ", ptr " << gate << ", align 8\n";
                    m_body << "  " << gateHi << " = getelementptr i8, ptr " << gate << ", i64 8\n";
                    m_body << "  store i64 " << high << ", ptr " << gateHi << ", align 8\n";
                    m_body << "  " << base << " = ptrtoint ptr " << idt << " to i64\n";
                    m_allocas << "  " << mem << " = alloca i8, i64 16, align 8\n";
                    m_body << "  " << lim16 << " = trunc i64 4095 to i16\n";
                    m_body << "  store i16 " << lim16 << ", ptr " << mem << ", align 2\n";
                    m_body << "  " << slot << " = getelementptr i8, ptr " << mem << ", i64 2\n";
                    m_body << "  store i64 " << base << ", ptr " << slot << ", align 2\n";
                    m_body << "  call void asm sideeffect \"lidt ($0)\", \"r,~{dirflag},~{fpsr},~{flags},~{memory}\"(ptr "
                           << mem << ")\n";
                    outb(0x20, 0x11);
                    outb(0xA0, 0x11);
                    outb(0x21, 32);
                    outb(0xA1, 40);
                    outb(0x21, 4);
                    outb(0xA1, 2);
                    outb(0x21, 1);
                    outb(0xA1, 1);
                    outb(0x21, 0xFE);
                    outb(0xA1, 0xFF);
                    m_body << "  call void asm sideeffect \"sti\", \"\"()\n";
                    m_body << "  call void asm sideeffect \"hlt\", \"\"()\n";
                    m_vals[id] = Val{ "0", NTy::Void };
                } else if (inst.textImm == "System.Call" && !inst.operandIds.empty()) {
                    Val number = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                    m_body << "  call void asm sideeffect \"movq $0, %rax\\0Aint $$128\", \"r,~{rax},~{flags},~{dirflag},~{fpsr},~{memory}\"(i64 "
                           << number.repr << ")\n";
                    m_vals[id] = Val{ "0", NTy::Void };
                } else if (inst.textImm == "Timer.Preempt") {
                    auto outb = [&](int port, int value) {
                        const std::string p16 = NewTmp();
                        const std::string v8 = NewTmp();
                        m_body << "  " << p16 << " = trunc i64 " << port << " to i16\n";
                        m_body << "  " << v8 << " = trunc i64 " << value << " to i8\n";
                        m_body << "  call void asm sideeffect \"outb $0, $1\", \"{ax},{dx}\"(i8 " << v8 << ", i16 " << p16 << ")\n";
                    };
                    auto storeQ = [&](const std::string& base, int offset, const std::string& value) {
                        const std::string slot = NewTmp();
                        m_body << "  " << slot << " = getelementptr i8, ptr " << base << ", i64 " << offset << "\n";
                        m_body << "  store i64 " << value << ", ptr " << slot << ", align 8\n";
                    };
                    auto gate = [&](const std::string& idt, int vector, const std::string& handler) {
                        const std::string lowMask = NewTmp();
                        const std::string midShift = NewTmp();
                        const std::string midMask = NewTmp();
                        const std::string midPlace = NewTmp();
                        const std::string selector = NewTmp();
                        const std::string typeBit = NewTmp();
                        const std::string low0 = NewTmp();
                        const std::string low1 = NewTmp();
                        const std::string low = NewTmp();
                        const std::string high = NewTmp();
                        const std::string at = NewTmp();
                        const std::string atHi = NewTmp();
                        m_body << "  " << lowMask << " = and i64 " << handler << ", 65535\n";
                        m_body << "  " << midShift << " = lshr i64 " << handler << ", 16\n";
                        m_body << "  " << midMask << " = and i64 " << midShift << ", 65535\n";
                        m_body << "  " << midPlace << " = shl i64 " << midMask << ", 48\n";
                        m_body << "  " << selector << " = shl i64 8, 16\n";
                        m_body << "  " << typeBit << " = shl i64 142, 40\n";
                        m_body << "  " << low0 << " = or i64 " << lowMask << ", " << selector << "\n";
                        m_body << "  " << low1 << " = or i64 " << low0 << ", " << typeBit << "\n";
                        m_body << "  " << low << " = or i64 " << low1 << ", " << midPlace << "\n";
                        m_body << "  " << high << " = lshr i64 " << handler << ", 32\n";
                        m_body << "  " << at << " = getelementptr i8, ptr " << idt << ", i64 " << (vector * 16) << "\n";
                        m_body << "  store i64 " << low << ", ptr " << at << ", align 8\n";
                        m_body << "  " << atHi << " = getelementptr i8, ptr " << at << ", i64 8\n";
                        m_body << "  store i64 " << high << ", ptr " << atHi << ", align 8\n";
                    };
                    const std::string pageB = NewTmp();
                    const std::string taskB = NewTmp();
                    const std::string taskA = NewTmp();
                    const std::string topB = NewTmp();
                    const std::string pageBAddr = NewTmp();
                    const std::string pageA = NewTmp();
                    const std::string pageAAddr = NewTmp();
                    const std::string topA = NewTmp();
                    const std::string idt = NewTmp();
                    const std::string timer = NewTmp();
                    const std::string syscall = NewTmp();
                    const std::string base = NewTmp();
                    const std::string mem = NewTmp();
                    const std::string lim16 = NewTmp();
                    const std::string slot = NewTmp();
                    m_body << "  " << pageB << " = call ptr @rx_pages_take(i64 1)\n";
                    for (int i = 0; i < 9; ++i) storeQ(pageB, i * 8, "0");
                    m_body << "  " << taskB << " = ptrtoint ptr @rx_fn_" << m_ctx.preemptB << " to i64\n";
                    m_body << "  " << taskA << " = ptrtoint ptr @rx_fn_" << m_ctx.preemptA << " to i64\n";
                    m_body << "  " << pageBAddr << " = ptrtoint ptr " << pageB << " to i64\n";
                    m_body << "  " << topB << " = add i64 " << pageBAddr << ", 4096\n";
                    storeQ(pageB, 72, taskB);
                    storeQ(pageB, 80, "8");
                    storeQ(pageB, 88, "514");
                    storeQ(pageB, 96, topB);
                    storeQ(pageB, 104, "16");
                    m_body << "  store i64 " << pageBAddr << ", ptr @rx_preempt_rsp1, align 8\n";
                    m_body << "  store i64 " << taskA << ", ptr @rx_preempt_entry, align 8\n";
                    m_body << "  store i64 0, ptr @rx_preempt_current, align 8\n";
                    m_body << "  store i64 0, ptr @rx_preempt_count, align 8\n";
                    m_body << "  " << idt << " = call ptr @rx_pages_take(i64 1)\n";
                    m_body << "  " << timer << " = ptrtoint ptr @rx_isr_preempt to i64\n";
                    m_body << "  " << syscall << " = ptrtoint ptr @rx_isr_syscall to i64\n";
                    gate(idt, 32, timer);
                    gate(idt, 128, syscall);
                    m_body << "  " << base << " = ptrtoint ptr " << idt << " to i64\n";
                    m_allocas << "  " << mem << " = alloca i8, i64 16, align 8\n";
                    m_body << "  " << lim16 << " = trunc i64 4095 to i16\n";
                    m_body << "  store i16 " << lim16 << ", ptr " << mem << ", align 2\n";
                    m_body << "  " << slot << " = getelementptr i8, ptr " << mem << ", i64 2\n";
                    m_body << "  store i64 " << base << ", ptr " << slot << ", align 2\n";
                    m_body << "  call void asm sideeffect \"lidt ($0)\", \"r,~{dirflag},~{fpsr},~{flags},~{memory}\"(ptr "
                           << mem << ")\n";
                    outb(0x20, 0x11);
                    outb(0xA0, 0x11);
                    outb(0x21, 32);
                    outb(0xA1, 40);
                    outb(0x21, 4);
                    outb(0xA1, 2);
                    outb(0x21, 1);
                    outb(0xA1, 1);
                    outb(0x21, 0xFE);
                    outb(0xA1, 0xFF);
                    m_body << "  call void asm sideeffect \"sti\", \"\"()\n";
                    m_body << "  " << pageA << " = call ptr @rx_pages_take(i64 1)\n";
                    m_body << "  " << pageAAddr << " = ptrtoint ptr " << pageA << " to i64\n";
                    m_body << "  " << topA << " = add i64 " << pageAAddr << ", 4096\n";
                    const std::string run = "movq %rsp, %rax\nmovq $0, %rsp\ncall rx_fn_" + m_ctx.preemptA + "\nmovq %rax, %rsp";
                    m_body << "  call void asm sideeffect \"" << EscapeLLVMString(run)
                           << "\", \"r,~{rax},~{rcx},~{rdx},~{rsi},~{rdi},~{r8},~{r9},~{r10},~{r11},~{flags},~{dirflag},~{fpsr},~{memory}\"(i64 "
                           << topA << ")\n";
                    m_vals[id] = Val{ "0", NTy::Void };
                } else if (inst.textImm == "CPU.Run" && !inst.operandIds.empty()) {
                    Val top = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                    const std::string text =
                        "movq %rsp, %rax\nmovq $0, %rsp\ncall rx_fn_" + inst.secondaryText + "\nmovq %rax, %rsp";
                    m_body << "  call void asm sideeffect \"" << EscapeLLVMString(text)
                           << "\", \"r,~{rax},~{rcx},~{rdx},~{rsi},~{rdi},~{r8},~{r9},~{r10},~{r11},~{flags},~{dirflag},~{fpsr},~{memory}\"(i64 "
                           << top.repr << ")\n";
                    m_vals[id] = Val{ "0", NTy::Void };
                } else {
                    Err("N9001", "OS intrinsic '" + inst.textImm + "' is missing an argument.", inst.location);
                }
            } else if (inst.textImm == "Console.Write" && !inst.operandIds.empty()) {
                Val value = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                const std::string& arch = m_ctx.options.asmArch;
                if (arch == "x86_64") {
                    const std::string p16 = NewTmp();
                    const std::string v8 = NewTmp();
                    m_body << "  " << p16 << " = trunc i64 1016 to i16\n";
                    m_body << "  " << v8 << " = trunc i64 " << value.repr << " to i8\n";
                    m_body << "  call void asm sideeffect \"outb $0, $1\", \"{ax},{dx}\"(i8 " << v8 << ", i16 " << p16 << ")\n";
                } else if (arch == "aarch64" || arch == "arm" || arch == "riscv64") {
                    const char* addr = arch == "riscv64" ? "268435456" : "150994944";
                    const std::string v8 = NewTmp();
                    m_body << "  " << v8 << " = trunc i64 " << value.repr << " to i8\n";
                    m_body << "  store i8 " << v8 << ", ptr inttoptr (i64 " << addr << " to ptr), align 1\n";
                } else {
                    Err("N9010", "Console.Write has no serial device for " + arch + ".", inst.location);
                    break;
                }
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "CPU.Halt" || inst.textImm == "CPU.CLI" || inst.textImm == "CPU.STI") {
                const std::string& arch = m_ctx.options.asmArch;
                const char* mnemonic = nullptr;
                if (inst.textImm == "CPU.Halt" && (arch == "aarch64" || arch == "arm" || arch == "riscv64")) {
                    mnemonic = "wfi";
                } else if (arch == "x86_64") {
                    mnemonic = inst.textImm == "CPU.Halt" ? "hlt" : inst.textImm == "CPU.CLI" ? "cli" : "sti";
                }
                if (mnemonic == nullptr) {
                    Err("N9010", inst.textImm + " exists on x86_64 only.", inst.location);
                    break;
                }
                m_body << "  call void asm sideeffect \"" << mnemonic << "\", \"\"()\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "Frame.Create" && !inst.operandIds.empty()) {
                m_ctx.usesFrame = true;
                Val width = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val height = inst.operandIds.size() > 1 ? Coerce(Operand(inst, 1), NTy::I64, inst.location)
                                                        : Val{ "0", NTy::I64 };
                m_body << "  " << vname << " = call i64 @rx_frame_create(i64 " << width.repr << ", i64 " << height.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Frame.Fill" && inst.operandIds.size() >= 6) {
                m_ctx.usesFrame = true;
                Val frame = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val x = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                Val y = Coerce(Operand(inst, 2), NTy::I64, inst.location);
                Val width = Coerce(Operand(inst, 3), NTy::I64, inst.location);
                Val height = Coerce(Operand(inst, 4), NTy::I64, inst.location);
                Val color = Coerce(Operand(inst, 5), NTy::I64, inst.location);
                m_body << "  call void @rx_frame_fill(i64 " << frame.repr << ", i64 " << x.repr << ", i64 " << y.repr
                       << ", i64 " << width.repr << ", i64 " << height.repr << ", i64 " << color.repr << ")\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "Frame.Pixel" && inst.operandIds.size() >= 3) {
                m_ctx.usesFrame = true;
                Val frame = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val x = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                Val y = Coerce(Operand(inst, 2), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_frame_pixel(i64 " << frame.repr << ", i64 " << x.repr
                       << ", i64 " << y.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Frame.Present" && !inst.operandIds.empty()) {
                m_ctx.usesFrame = true;
                Val frame = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_frame_present(i64 " << frame.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Frame.Flush" && inst.operandIds.size() >= 5) {
                m_ctx.usesFrame = true;
                Val frame = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                Val x = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                Val y = Coerce(Operand(inst, 2), NTy::I64, inst.location);
                Val width = Coerce(Operand(inst, 3), NTy::I64, inst.location);
                Val height = Coerce(Operand(inst, 4), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_frame_flush(i64 " << frame.repr << ", i64 " << x.repr
                       << ", i64 " << y.repr << ", i64 " << width.repr << ", i64 " << height.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Abort" || inst.textImm == "Panic") {
                m_body << "  call void @llvm.trap()\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else if (inst.textImm == "Math.Abs" || inst.textImm == "Math.Min" ||
                       inst.textImm == "Math.Max" || inst.textImm == "Math.Clamp") {
                auto asI64 = [&](std::size_t index) {
                    return Coerce(Operand(inst, index), NTy::I64, inst.location).repr;
                };
                const std::string a = asI64(0);
                if (inst.textImm == "Math.Abs") {
                    const std::string neg = NewTmp();
                    const std::string lt = NewTmp();
                    m_body << "  " << neg << " = sub i64 0, " << a << "\n";
                    m_body << "  " << lt << " = icmp slt i64 " << a << ", 0\n";
                    m_body << "  " << vname << " = select i1 " << lt << ", i64 " << neg << ", i64 " << a << "\n";
                } else if (inst.textImm == "Math.Clamp") {
                    const std::string lo = asI64(1);
                    const std::string hi = asI64(2);
                    const std::string ltLo = NewTmp();
                    const std::string gtHi = NewTmp();
                    const std::string raised = NewTmp();
                    m_body << "  " << ltLo << " = icmp slt i64 " << a << ", " << lo << "\n";
                    m_body << "  " << raised << " = select i1 " << ltLo << ", i64 " << lo << ", i64 " << a << "\n";
                    m_body << "  " << gtHi << " = icmp sgt i64 " << raised << ", " << hi << "\n";
                    m_body << "  " << vname << " = select i1 " << gtHi << ", i64 " << hi << ", i64 " << raised << "\n";
                } else {
                    const std::string b = asI64(1);
                    const std::string cmp = NewTmp();
                    const char* pred = inst.textImm == "Math.Min" ? "slt" : "sgt";
                    m_body << "  " << cmp << " = icmp " << pred << " i64 " << a << ", " << b << "\n";
                    m_body << "  " << vname << " = select i1 " << cmp << ", i64 " << a << ", i64 " << b << "\n";
                }
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if ((inst.textImm == "GPIO.Read" || UnavailableDevice(inst.textImm) != 0) &&
                       !inst.operandIds.empty()) {
                Val pin = Coerce(Operand(inst, 0), NTy::I64, inst.location);
                if (m_ctx.options.freestanding) {
                    m_vals[id] = Val{ "1", NTy::I64 };
                    (void)pin;
                } else if (inst.textImm == "GPIO.Read") {
                    m_body << "  " << vname << " = call i64 @rx_gpio_read(i64 " << pin.repr << ")\n";
                    m_vals[id] = Val{ vname, NTy::I64 };
                } else {
                    m_body << "  " << vname << " = call i64 @rx_device_unavailable(i64 "
                           << UnavailableDevice(inst.textImm) << ", i64 " << pin.repr << ")\n";
                    m_vals[id] = Val{ vname, NTy::I64 };
                }
            } else if (inst.textImm == "Atomic.FetchAdd" && inst.operandIds.size() >= 2) {
                Val cell = Operand(inst, 0);
                Val delta = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                m_body << "  " << vname << " = atomicrmw add ptr " << cell.repr << ", i64 " << delta.repr << " seq_cst\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else if (inst.textImm == "Thread.AddPair" && inst.operandIds.size() >= 3) {
                Val cell = Operand(inst, 0);
                Val left = Coerce(Operand(inst, 1), NTy::I64, inst.location);
                Val right = Coerce(Operand(inst, 2), NTy::I64, inst.location);
                m_body << "  " << vname << " = call i64 @rx_add_pair(ptr " << cell.repr << ", i64 " << left.repr
                       << ", i64 " << right.repr << ")\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            } else {
                Err("N9001",
                    "OS intrinsic '" + inst.textImm + "' is not yet supported by the native backend.",
                    inst.location);
            }
            break;
        }

        case IROpcode::AllocHeap:
        case IROpcode::AllocStack: {
            const bool onStack = inst.opcode == IROpcode::AllocStack;
            const bool isArray = inst.textImm == "Array";
            const std::int64_t n = inst.intImm < 0 ? 0 : inst.intImm;
            const std::string bytes = isArray ? std::to_string(8 + n * 8)
                                              : std::to_string((n < 1 ? 1 : n) * 8);
            const std::string p = NewTmp();
            if (onStack) {
                m_allocas << "  " << p << " = alloca i8, i64 " << bytes << ", align 8\n";
            } else {
                m_body << "  " << p << " = call ptr @malloc(i64 " << bytes << ")\n";
            }
            m_body << "  call void @llvm.memset.p0.i64(ptr " << p << ", i8 0, i64 " << bytes << ", i1 false)\n";
            if (isArray) {
                m_body << "  store i64 " << n << ", ptr " << p << ", align 8\n";
            } else if (inst.boolImm) {
                std::string concrete = inst.textImm;
                const auto at = concrete.find('@');
                if (at != std::string::npos) concrete.resize(at);
                const IR::IRVTable* vt = nullptr;
                if (m_ctx.module) {
                    for (const auto& candidate : m_ctx.module->vtables) {
                        if (candidate.typeName == concrete) {
                            vt = &candidate;
                            break;
                        }
                    }
                }
                if (vt && !vt->slots.empty()) {
                    m_body << "  store ptr @\"vt." << concrete << "\", ptr " << p << ", align 8\n";
                } else {
                    m_body << "  store ptr null, ptr " << p << ", align 8\n";
                }
            }
            m_vals[id] = Val{ p, NTy::Ptr };
            break;
        }

        case IROpcode::GetField: {
            Val obj = Operand(inst, 0);
            const std::string gep = NewTmp();
            if (inst.intImm < 0 || inst.textImm == "len") {
                m_body << "  " << vname << " = load i64, ptr " << obj.repr << ", align 8\n";
                m_vals[id] = Val{ vname, NTy::I64 };
                break;
            }
            m_body << "  " << gep << " = getelementptr i8, ptr " << obj.repr << ", i64 " << (inst.intImm * 8) << "\n";
            NTy ft = NTy::I64;
            std::string why;
            if (!MapType(inst.dest.type, ft, why) || ft == NTy::Void) ft = NTy::I64;
            if (ft == NTy::Ptr) {
                m_body << "  " << vname << " = load ptr, ptr " << gep << ", align 8\n";
                m_vals[id] = Val{ vname, NTy::Ptr };
            } else if (ft == NTy::F64 || ft == NTy::F32) {
                m_body << "  " << vname << " = load " << TyStr(ft) << ", ptr " << gep << ", align 8\n";
                m_vals[id] = Val{ vname, ft };
            } else {
                m_body << "  " << vname << " = load i64, ptr " << gep << ", align 8\n";
                m_vals[id] = Val{ vname, NTy::I64, IsUnsignedName(inst.dest.type.baseName) };
            }
            break;
        }

        case IROpcode::SetField: {
            Val obj = Operand(inst, 0);
            Val rhs = Operand(inst, 1);
            const std::string gep = NewTmp();
            m_body << "  " << gep << " = getelementptr i8, ptr " << obj.repr << ", i64 " << (inst.intImm * 8) << "\n";
            if (rhs.ty == NTy::Ptr) {
                m_body << "  store ptr " << rhs.repr << ", ptr " << gep << ", align 8\n";
            } else if (IsFloatTy(rhs.ty)) {
                Val w = Coerce(rhs, NTy::F64, inst.location);
                m_body << "  store double " << w.repr << ", ptr " << gep << ", align 8\n";
            } else {
                Val w = Coerce(rhs, NTy::I64, inst.location);
                m_body << "  store i64 " << w.repr << ", ptr " << gep << ", align 8\n";
            }
            break;
        }

        case IROpcode::LoadIndex:
        case IROpcode::StoreIndex: {
            const bool storing = inst.opcode == IROpcode::StoreIndex;
            Val arr = Operand(inst, 0);
            Val idx = Coerce(Operand(inst, 1), NTy::I64, inst.location);
            const std::string len = NewTmp();
            const std::string ok = NewTmp();
            const int fence = m_tmp++;
            m_body << "  " << len << " = load i64, ptr " << arr.repr << ", align 8\n";
            m_body << "  " << ok << " = icmp ult i64 " << idx.repr << ", " << len << "\n";
            m_body << "  br i1 " << ok << ", label %idx_ok_" << fence << ", label %idx_bad_" << fence << "\n";
            m_body << "idx_bad_" << fence << ":\n";
            m_body << "  call void @llvm.trap()\n";
            m_body << "  unreachable\n";
            m_body << "idx_ok_" << fence << ":\n";
            const std::string plus = NewTmp();
            const std::string gep = NewTmp();
            m_body << "  " << plus << " = add i64 " << idx.repr << ", 1\n";
            m_body << "  " << gep << " = getelementptr i64, ptr " << arr.repr << ", i64 " << plus << "\n";
            if (storing) {
                Val rhs = Operand(inst, 2);
                if (IsFloatTy(rhs.ty)) {
                    Val w = Coerce(rhs, NTy::F64, inst.location);
                    m_body << "  store double " << w.repr << ", ptr " << gep << ", align 8\n";
                } else {
                    Val w = Coerce(rhs, NTy::I64, inst.location);
                    m_body << "  store i64 " << w.repr << ", ptr " << gep << ", align 8\n";
                }
            } else {
                m_body << "  " << vname << " = load i64, ptr " << gep << ", align 8\n";
                m_vals[id] = Val{ vname, NTy::I64 };
            }
            break;
        }

        case IROpcode::AddrOf: {
            NTy slot = NTy::I64;
            if (inst.intImm == 1) slot = NTy::I8;
            else if (inst.intImm == 2) slot = NTy::I16;
            else if (inst.intImm == 4) slot = NTy::I32;
            if (m_varTy.find(inst.textImm) == m_varTy.end()) {
                EnsureVar(inst.textImm, slot);
            }
            m_vals[id] = Val{ SlotName(inst.textImm), NTy::Ptr };
            break;
        }

        case IROpcode::PtrLoad: {
            Val p = Operand(inst, 0);
            const std::int64_t width = inst.intImm <= 0 ? 8 : inst.intImm;
            const char* wty = WidthTy(width);
            const std::string raw = NewTmp();
            m_body << "  " << raw << " = load " << wty << ", ptr " << p.repr << ", align 1\n";
            if (width >= 8) {
                m_vals[id] = Val{ raw, NTy::I64, !inst.boolImm };
            } else {
                m_body << "  " << vname << " = " << (inst.boolImm ? "sext " : "zext ")
                       << wty << " " << raw << " to i64\n";
                m_vals[id] = Val{ vname, NTy::I64, !inst.boolImm };
            }
            break;
        }

        case IROpcode::PtrStore: {
            Val p = Operand(inst, 0);
            Val rhs = Operand(inst, 1);
            const std::int64_t width = inst.intImm <= 0 ? 8 : inst.intImm;
            const char* wty = WidthTy(width);
            NTy to = width == 1 ? NTy::I8 : width == 2 ? NTy::I16 : width == 4 ? NTy::I32 : NTy::I64;
            Val w = IsFloatTy(rhs.ty) ? rhs : Coerce(rhs, to, inst.location);
            m_body << "  store " << TyStr(w.ty) << " " << w.repr << ", ptr " << p.repr << ", align 1\n";
            break;
        }

        case IROpcode::PtrOffset: {
            Val p = Operand(inst, 0);
            Val idx = Coerce(Operand(inst, 1), NTy::I64, inst.location);
            const std::int64_t width = inst.intImm <= 0 ? 1 : inst.intImm;
            std::string scaled = idx.repr;
            if (width != 1) {
                scaled = NewTmp();
                m_body << "  " << scaled << " = mul i64 " << idx.repr << ", " << width << "\n";
            }
            m_body << "  " << vname << " = getelementptr i8, ptr " << p.repr << ", i64 " << scaled << "\n";
            m_vals[id] = Val{ vname, NTy::Ptr };
            break;
        }

        case IROpcode::SliceCopy: {
            Val arr = Operand(inst, 0);
            Val start = Coerce(Operand(inst, 1), NTy::I64, inst.location);
            Val end = Coerce(Operand(inst, 2), NTy::I64, inst.location);
            const std::string len = NewTmp();
            const std::string endAdj = NewTmp();
            const std::string nonNeg = NewTmp();
            const std::string ordered = NewTmp();
            const std::string inLen = NewTmp();
            const std::string ok1 = NewTmp();
            const std::string ok = NewTmp();
            const int fence = m_tmp++;
            m_body << "  " << len << " = load i64, ptr " << arr.repr << ", align 8\n";
            if (inst.intImm != 0) {
                m_body << "  " << endAdj << " = add i64 " << end.repr << ", 1\n";
            } else {
                m_body << "  " << endAdj << " = add i64 " << end.repr << ", 0\n";
            }
            m_body << "  " << nonNeg << " = icmp sge i64 " << start.repr << ", 0\n";
            m_body << "  " << ordered << " = icmp sle i64 " << start.repr << ", " << endAdj << "\n";
            m_body << "  " << inLen << " = icmp sle i64 " << endAdj << ", " << len << "\n";
            m_body << "  " << ok1 << " = and i1 " << nonNeg << ", " << ordered << "\n";
            m_body << "  " << ok << " = and i1 " << ok1 << ", " << inLen << "\n";
            m_body << "  br i1 " << ok << ", label %sl_ok_" << fence << ", label %sl_bad_" << fence << "\n";
            m_body << "sl_bad_" << fence << ":\n";
            m_body << "  call void @llvm.trap()\n";
            m_body << "  unreachable\n";
            m_body << "sl_ok_" << fence << ":\n";
            const std::string count = NewTmp();
            const std::string bytes = NewTmp();
            const std::string total = NewTmp();
            const std::string dst = NewTmp();
            const std::string srcOff = NewTmp();
            const std::string src = NewTmp();
            const std::string dstEl = NewTmp();
            m_body << "  " << count << " = sub i64 " << endAdj << ", " << start.repr << "\n";
            m_body << "  " << bytes << " = mul i64 " << count << ", 8\n";
            m_body << "  " << total << " = add i64 " << bytes << ", 8\n";
            m_body << "  " << dst << " = call ptr @malloc(i64 " << total << ")\n";
            m_body << "  store i64 " << count << ", ptr " << dst << ", align 8\n";
            m_body << "  " << srcOff << " = add i64 " << start.repr << ", 1\n";
            m_body << "  " << src << " = getelementptr i64, ptr " << arr.repr << ", i64 " << srcOff << "\n";
            m_body << "  " << dstEl << " = getelementptr i64, ptr " << dst << ", i64 1\n";
            m_body << "  call void @llvm.memcpy.p0.p0.i64(ptr " << dstEl << ", ptr " << src
                   << ", i64 " << bytes << ", i1 false)\n";
            m_vals[id] = Val{ dst, NTy::Ptr };
            break;
        }
    }
}

bool FnEmitter::Emit() {
    NTy ret{};
    std::vector<NTy> ptys;
    std::string why;
    if (!FunctionSignature(m_fn, ret, ptys, why)) {
        Err("N9008", "Function '" + m_fn.name + "' uses unsupported native types: " + why + ".", m_fn.location);
        return false;
    }
    m_ret = ret;
    const bool exportC = m_fn.isExtern && !m_fn.blocks.empty();
    if (m_fn.isExtern && !exportC) {
        m_out << "declare " << TyStr(ret) << " @" << m_fn.mangledName << "(";
        for (std::size_t i = 0; i < ptys.size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << TyStr(ptys[i]);
        }
        m_out << ")\n\n";
        return true;
    }
    IndexDefs();
    m_lastDebugLine = 0;

    std::ostringstream paramStores;
    for (std::size_t i = 0; i < m_fn.parameters.size(); ++i) {
        const std::string& pname = m_fn.parameters[i].name;
        EnsureVar(pname, ptys[i]);
        paramStores << "  store " << TyStr(ptys[i]) << " %a" << i << ", ptr " << SlotName(pname)
                    << ", align " << AlignOf(ptys[i]) << "\n";
    }

    for (std::size_t bi = 0; bi < m_fn.blocks.size(); ++bi) {
        const auto& bb = m_fn.blocks[bi];
        if (bi != 0) {
            m_body << bb.label << ":\n";
        }
        m_blockLabel = bb.label;
        bool terminated = false;
        for (const auto& inst : bb.instructions) {
            if (inst.isDead) continue;
            if (terminated) break;
            if (m_ctx.options.debugLines && inst.location.line != 0 && inst.location.line != m_lastDebugLine) {
                m_lastDebugLine = inst.location.line;
                m_body << "  call void @rx_debug_line(i64 " << m_lastDebugLine << ")\n";
            }
            EmitInstruction(inst);
            terminated = IR::IsTerminatorOpcode(inst.opcode);
        }
        if (!terminated) {
            m_body << "  unreachable\n";
        }
    }

    if (!m_ok) return false;

    const bool isEntry = m_ctx.options.freestanding && m_fn.name == m_ctx.options.entryName;
    const bool cpuFn = m_ctx.cpuFns.count(m_fn.name) != 0;
    if (exportC) {
        m_out << "define dso_local " << TyStr(ret) << " @" << m_fn.mangledName << "(";
    } else if (isEntry) {
        m_out << "define dso_local " << TyStr(ret) << " @" << m_fn.name << "(";
    } else if (cpuFn) {
        m_out << "define dso_local " << TyStr(ret) << " @rx_fn_" << m_fn.name << "(";
    } else {
        m_out << "define internal fastcc " << TyStr(ret) << " @\"" << m_fn.mangledName << "\"(";
    }
    for (std::size_t i = 0; i < ptys.size(); ++i) {
        if (i > 0) m_out << ", ";
        m_out << TyStr(ptys[i]) << " %a" << i;
    }
    bool isRecursive = false;
    for (const auto& blk : m_fn.blocks) {
        for (const auto& inst : blk.instructions) {
            if (inst.opcode == IR::IROpcode::CallDirect && inst.textImm == m_fn.name) {
                isRecursive = true;
                break;
            }
        }
        if (isRecursive) break;
    }
    if (exportC) {
        m_out << ") nounwind {\nentry:\n";
    } else if (isEntry) {
        m_out << ") nounwind #0 section \".text.entry\" {\nentry:\n";
    } else if (cpuFn) {
        m_out << ") nounwind #0 {\nentry:\n";
    } else if (m_ctx.options.profile == BuildProfile::Release && !isRecursive) {
        m_out << ") alwaysinline nounwind #0 align 64 {\nentry:\n";
    } else {
        m_out << ") nounwind #0 align 64 {\nentry:\n";
    }
    m_out << m_allocas.str()
          << paramStores.str()
          << m_body.str()
          << "}\n\n";
    return true;
}

} // namespace

NativeLLVMEmitter::NativeLLVMEmitter(Frontend::DiagnosticBag& diagnostics, NativeEmitOptions options) noexcept
    : m_diagnostics(diagnostics), m_options(options) {}

std::optional<std::string> NativeLLVMEmitter::Emit(const IR::IRModule& mod) {
    ModuleCtx ctx(m_diagnostics, m_options);
    ctx.module = &mod;
    for (const auto& fn : mod.functions) {
        if (!fn.isTestBlock) {
            ctx.byName[fn.name] = &fn;
        }
    }

    std::ostringstream functions;
    bool ok = true;
    const IR::IRFunction* mainFn = nullptr;
    std::vector<std::string> nativeTests;
    const std::string& entryName = m_options.entryName;

    auto noteIntrinsic = [&](const IR::IRInstruction& inst, std::vector<std::string>* pending) {
        if (inst.opcode != IR::IROpcode::CallOSIntrinsic) return;
        if (inst.textImm == "Memory.Pages" || inst.textImm == "Memory.Frame") ctx.usesPages = true;
        if (inst.textImm.rfind("Frame.", 0) == 0) ctx.usesFrame = true;
        if (inst.textImm == "CPU.ExchangeRSP" || inst.textImm == "CPU.SavedRSP") ctx.usesSwitch = true;
        if (inst.textImm == "System.Call") ctx.usesSyscall = true;
        if (inst.textImm == "CPU.Function" || inst.textImm == "CPU.Handler" || inst.textImm == "CPU.Fault" || inst.textImm == "CPU.Run" ||
            inst.textImm == "Timer.Arm" || inst.textImm == "Timer.Preempt") {
            if (!inst.secondaryText.empty()) {
                if (inst.textImm == "Timer.Preempt") {
                    const auto split = inst.secondaryText.find('\n');
                    if (split != std::string::npos) {
                        ctx.preemptA = inst.secondaryText.substr(0, split);
                        ctx.preemptB = inst.secondaryText.substr(split + 1);
                        ctx.cpuFns.insert(ctx.preemptA);
                        ctx.cpuFns.insert(ctx.preemptB);
                        ctx.usesPreempt = true;
                        ctx.usesPages = true;
                        ctx.usesSyscall = true;
                        if (pending) {
                            pending->push_back(ctx.preemptA);
                            pending->push_back(ctx.preemptB);
                        }
                    }
                } else {
                    ctx.cpuFns.insert(inst.secondaryText);
                    if (inst.textImm == "CPU.Handler" || inst.textImm == "CPU.Fault" || inst.textImm == "Timer.Arm") ctx.isrFns.insert(inst.secondaryText);
                    if (inst.textImm == "CPU.Fault") ctx.faultIsrs.insert(inst.secondaryText);
                    if (inst.textImm == "Timer.Arm") {
                        ctx.timerIsrs.insert(inst.secondaryText);
                        ctx.usesPages = true;
                    }
                    if (pending) pending->push_back(inst.secondaryText);
                }
            }
        }
    };
    for (const auto& fn : mod.functions) {
        for (const auto& block : fn.blocks) {
            for (const auto& inst : block.instructions) noteIntrinsic(inst, nullptr);
        }
    }

    std::set<std::string> reachable;
    if (m_options.freestanding) {
        std::vector<std::string> pending;
        pending.push_back(entryName);
        while (!pending.empty()) {
            const std::string name = pending.back();
            pending.pop_back();
            if (!reachable.insert(name).second) continue;
            const auto found = ctx.byName.find(name);
            if (found == ctx.byName.end()) continue;
            for (const auto& block : found->second->blocks) {
                for (const auto& inst : block.instructions) {
                    if (inst.opcode == IR::IROpcode::CallDirect || inst.opcode == IR::IROpcode::CallVirtual) {
                        pending.push_back(inst.textImm);
                    }
                    noteIntrinsic(inst, &pending);
                }
            }
        }
    }

    for (const auto& fn : mod.functions) {
        if (fn.isTestBlock && !fn.runOnNative) continue;
        if (fn.isTestBlock) nativeTests.push_back(fn.mangledName);
        if (m_options.freestanding && reachable.find(fn.name) == reachable.end()) continue;
        FnEmitter emitter(fn, ctx, functions);
        if (!emitter.Emit()) {
            ok = false;
        }
        const bool isEntry = !fn.isTestBlock && (m_options.freestanding
                                 ? (fn.name == entryName && fn.parameters.empty())
                                 : (fn.name == "Main" && fn.parameters.empty()));
        if (isEntry) {
            mainFn = &fn;
        }
    }

    NTy mainRet = NTy::Void;
    if (!mainFn && nativeTests.empty() && !m_options.omitMain) {
        if (m_options.freestanding) {
            m_diagnostics.ReportError(
                "N9002",
                "No freestanding entry: expected a parameterless function named '" + entryName + "'.",
                Frontend::SourceLocation{},
                "Pass --entry Name or declare 'public Function " + entryName + "() -> Void'.");
        } else {
            m_diagnostics.ReportError(
                "N9002",
                "No entry point: native executables require a parameterless 'Function Main()'.",
                Frontend::SourceLocation{},
                "Add 'public Function Main() -> Int { ... }'.");
        }
        ok = false;
    } else if (mainFn) {
        NTy rt{};
        std::vector<NTy> pt;
        std::string why;
        if (FunctionSignature(*mainFn, rt, pt, why)) {
            mainRet = rt;
        }
    }

    if (!ok) {
        return std::nullopt;
    }

    std::ostringstream out;
    out << "; Robix native LLVM IR - module '" << mod.moduleName << "' (profile: "
        << BuildProfileToString(m_options.profile)
        << (m_options.freestanding ? ", freestanding" : "") << ")\n";
    const std::string layout = m_options.layoutOverride.empty()
                                   ? std::string(GetLLVMDataLayout(m_options.target))
                                   : m_options.layoutOverride;
    const std::string triple = m_options.tripleOverride.empty()
                                   ? std::string(GetLLVMTargetTriple(m_options.target))
                                   : m_options.tripleOverride;
    out << "target datalayout = \"" << layout << "\"\n";
    out << "target triple = \"" << triple << "\"\n\n";
    const std::string body = functions.str();
    const auto needs = [&](const char* symbol) {
        return body.find(symbol) != std::string::npos;
    };
    if (!m_options.freestanding || needs("@rx_print_i64")) {
        out << "declare void @rx_print_i64(i64)\n";
    }
    if (!m_options.freestanding || needs("@printf")) {
        out << "declare i32 @printf(ptr, ...)\n";
        out << "declare void @rx_print_f64(double)\n";
        out << "declare void @rx_print_bool(i32)\n";
        out << "declare void @rx_print_str(ptr)\n";
        out << "declare ptr @malloc(i64)\n";
        out << "declare void @free(ptr)\n";
        out << "declare i64 @strlen(ptr)\n";
        out << "declare i32 @strcmp(ptr, ptr)\n";
        out << "declare ptr @rx_input_line(ptr)\n";
        out << "declare i64 @rx_input_i64(ptr)\n";
        out << "declare i64 @rx_args_count()\n";
        out << "declare ptr @rx_args_get(i64)\n";
        out << "declare i32 @rx_write_text(ptr, ptr)\n";
        out << "declare ptr @rx_read_text(ptr)\n";
        out << "declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)\n";
        out << "declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)\n";
    }
    if (!m_options.freestanding || needs("@llvm.trap") || ctx.usesPages || ctx.usesFrame) {
        out << "declare void @llvm.trap()\n";
    }
    if (needs("@exit(")) {
        out << "declare void @exit(i32) noreturn\n";
    }
    if (needs("@rx_gpio_read")) {
        out << "declare i64 @rx_gpio_read(i64)\n";
    }
    if (needs("@rx_network_send_text")) {
        out << "declare ptr @rx_network_send_text(ptr)\n";
    }
    if (needs("@rx_network_call")) {
        out << "declare ptr @rx_network_call(ptr, ptr, ptr)\n";
    }
    if (needs("@rx_env_get")) {
        out << "declare ptr @rx_env_get(ptr)\n";
    }
    if (needs("@rx_env_exists")) {
        out << "declare i64 @rx_env_exists(ptr)\n";
    }
    if (needs("@rx_env_set")) {
        out << "declare i64 @rx_env_set(ptr, ptr)\n";
    }
    if (needs("@rx_env_add")) {
        out << "declare i64 @rx_env_add(ptr, ptr)\n";
    }
    if (needs("@rx_env_remove")) {
        out << "declare i64 @rx_env_remove(ptr)\n";
    }
    if (needs("@rx_device_unavailable")) {
        out << "declare i64 @rx_device_unavailable(i64, i64)\n";
    }
    if (needs("@rx_add_pair")) {
        out << "declare i64 @rx_add_pair(ptr, i64, i64)\n";
    }
    if (needs("@rx_debug_line")) {
        out << "declare void @rx_debug_line(i64)\n";
    }
    out << "\n";
    for (const auto& vt : mod.vtables) {
        if (vt.slots.empty()) continue;
        out << "@\"vt." << vt.typeName << "\" = private constant [" << vt.slots.size() << " x ptr] [";
        for (std::size_t i = 0; i < vt.slots.size(); ++i) {
            if (i > 0) out << ", ";
            auto it = ctx.byName.find(vt.slots[i]);
            if (vt.slots[i].empty()) {
                out << "ptr null";
            } else if (it == ctx.byName.end()) {
                m_diagnostics.ReportError(
                    "N9004",
                    "Vtable '" + vt.typeName + "' slot '" + vt.slots[i] + "' has no function.",
                    Frontend::SourceLocation{});
                ok = false;
                out << "ptr null";
            } else {
                out << "ptr @\"" << it->second->mangledName << "\"";
            }
        }
        out << "]\n";
    }
    if (!mod.vtables.empty()) out << "\n";
    if (!m_options.freestanding || needs("@rx_strcat")) {
    out << "define internal ptr @rx_strcat(ptr %a, ptr %b) {\n";
    out << "entry:\n";
    out << "  %na = call i64 @strlen(ptr %a)\n";
    out << "  %nb = call i64 @strlen(ptr %b)\n";
    out << "  %n = add i64 %na, %nb\n";
    out << "  %n1 = add i64 %n, 1\n";
    out << "  %p = call ptr @malloc(i64 %n1)\n";
    out << "  call void @llvm.memcpy.p0.p0.i64(ptr %p, ptr %a, i64 %na, i1 false)\n";
    out << "  %dst = getelementptr i8, ptr %p, i64 %na\n";
    out << "  call void @llvm.memcpy.p0.p0.i64(ptr %dst, ptr %b, i64 %nb, i1 false)\n";
    out << "  %end = getelementptr i8, ptr %dst, i64 %nb\n";
    out << "  store i8 0, ptr %end, align 1\n";
    out << "  ret ptr %p\n";
    out << "}\n\n";
    }

    for (std::size_t i = 0; i < ctx.stringLiterals.size(); ++i) {
        const std::string& s = ctx.stringLiterals[i];
        out << "@.str." << i << " = private unnamed_addr constant [" << (s.size() + 1) << " x i8] c\""
            << EscapeLLVMString(s) << "\\00\"\n";
    }
    if (!ctx.stringLiterals.empty()) out << "\n";

    out << body;
    if (ctx.usesPages) {
        out << "@rx.pages = internal global [65536 x i8] zeroinitializer, align 4096\n";
        out << "@rx.page.used = internal global i64 0, align 8\n";
        out << "define internal ptr @rx_pages_take(i64 %count) nounwind {\n";
        out << "entry:\n";
        out << "  %bytes = mul i64 %count, 4096\n";
        out << "  %used = load i64, ptr @rx.page.used, align 8\n";
        out << "  %next = add i64 %used, %bytes\n";
        out << "  %ok = icmp ule i64 %next, 65536\n";
        out << "  br i1 %ok, label %have, label %die\n";
        out << "die:\n";
        out << "  call void @llvm.trap()\n";
        out << "  unreachable\n";
        out << "have:\n";
        out << "  store i64 %next, ptr @rx.page.used, align 8\n";
        out << "  %p = getelementptr i8, ptr @rx.pages, i64 %used\n";
        out << "  ret ptr %p\n";
        out << "}\n\n";
        out << "define i64 @rx_memory_frame() nounwind {\n";
        out << "entry:\n";
        if (m_options.freestanding) {
            out << "  %used = load i64, ptr @rx.page.used, align 8\n";
            out << "  %next = add i64 %used, 4096\n";
            out << "  %ok = icmp ule i64 %next, 65536\n";
            out << "  br i1 %ok, label %have, label %none\n";
            out << "none:\n  ret i64 0\n";
            out << "have:\n";
            out << "  store i64 %next, ptr @rx.page.used, align 8\n";
            out << "  %p = getelementptr i8, ptr @rx.pages, i64 %used\n";
            out << "  %addr = ptrtoint ptr %p to i64\n";
            out << "  ret i64 %addr\n}\n\n";
        } else {
            out << "  %used = load i64, ptr @rx.frame.used, align 8\n";
            out << "  %ok = icmp ult i64 %used, 8\n";
            out << "  br i1 %ok, label %have, label %none\n";
            out << "none:\n  ret i64 0\n";
            out << "have:\n";
            out << "  %next = add i64 %used, 1\n";
            out << "  store i64 %next, ptr @rx.frame.used, align 8\n";
            out << "  %off = mul i64 %used, 4096\n";
            out << "  %addr = add i64 %off, 2097152\n";
            out << "  ret i64 %addr\n}\n\n";
            out << "@rx.frame.used = internal global i64 0, align 8\n\n";
        }
    }
    if (ctx.usesSwitch) {
        out << "@rx.switch.cell = internal global i64 0, align 8\n";
        out << "define dso_local i64 @rx_exchange_rsp(i64 %new) nounwind noinline {\n";
        out << "entry:\n";
        out << "  %old = call i64 asm sideeffect \"movq %rsp, $0; movq $1, %rsp\", \"=r,r\"(i64 %new)\n";
        out << "  store i64 %old, ptr @rx.switch.cell, align 8\n";
        out << "  ret i64 %old\n";
        out << "}\n\n";
    }
    if (ctx.usesPreempt) {
        out << "@rx_preempt_current = global i64 0, align 8\n";
        out << "@rx_preempt_rsp0 = global i64 0, align 8\n";
        out << "@rx_preempt_rsp1 = global i64 0, align 8\n";
        out << "@rx_preempt_count = global i64 0, align 8\n";
        out << "@rx_preempt_entry = global i64 0, align 8\n";
        const std::string preempt =
            "pushq %rax\npushq %rcx\npushq %rdx\npushq %rsi\npushq %rdi\n"
            "pushq %r8\npushq %r9\npushq %r10\npushq %r11\n"
            "movb $$32, %al\noutb %al, $$32\n"
            "movq %rsp, %rax\nmovq rx_preempt_current(%rip), %rcx\n"
            "testq %rcx, %rcx\njnz 1f\nmovq %rax, rx_preempt_rsp0(%rip)\njmp 2f\n"
            "1:\nmovq %rax, rx_preempt_rsp1(%rip)\n2:\n"
            "incq rx_preempt_count(%rip)\ncmpq $$4, rx_preempt_count(%rip)\njae 3f\n"
            "xorq $$1, %rcx\nmovq %rcx, rx_preempt_current(%rip)\n"
            "testq %rcx, %rcx\njnz 4f\nmovq rx_preempt_rsp0(%rip), %rsp\njmp 5f\n"
            "4:\nmovq rx_preempt_rsp1(%rip), %rsp\n5:\n"
            "popq %r11\npopq %r10\npopq %r9\npopq %r8\n"
            "popq %rdi\npopq %rsi\npopq %rdx\npopq %rcx\npopq %rax\niretq\n"
            "3:\ncli\n6:\nhlt\njmp 6b\n";
        out << "define dso_local void @rx_isr_preempt() naked nounwind {\n";
        out << "  call void asm sideeffect \"" << EscapeLLVMString(preempt) << "\", \"\"()\n";
        out << "  unreachable\n}\n\n";
    }
    if (ctx.usesSyscall) {
        const std::string syscall = "movw $$1016, %dx\noutb %al, %dx\niretq\n";
        out << "define dso_local void @rx_isr_syscall() naked nounwind {\n";
        out << "  call void asm sideeffect \"" << EscapeLLVMString(syscall) << "\", \"\"()\n";
        out << "  unreachable\n}\n\n";
    }
    for (const std::string& isr : ctx.isrFns) {
        if (ctx.faultIsrs.count(isr)) {
            const std::string text =
                "popq %rax\n"
                "movabsq $20992, %rdx\n"
                "movq %rax, (%rdx)\n"
                "testb $4, %al\n"
                "jz 1f\n"
                "pushq %rcx\npushq %rdx\npushq %rsi\npushq %rdi\n"
                "pushq %r8\npushq %r9\npushq %r10\npushq %r11\n"
                "call rx_fn_" + isr + "\n"
                "popq %r11\npopq %r10\npopq %r9\npopq %r8\n"
                "popq %rdi\npopq %rsi\npopq %rdx\npopq %rcx\n"
                "movabsq $21008, %rdx\n"
                "movq (%rdx), %rax\n"
                "movq %rax, (%rsp)\n"
                "movq $8, 8(%rsp)\n"
                "movabsq $21016, %rdx\n"
                "movq (%rdx), %rax\n"
                "movq %rax, 24(%rsp)\n"
                "movq $16, 32(%rsp)\n"
                "iretq\n"
                "1:\ncli\n2:\nhlt\njmp 2b\n";
            std::string doubled;
            for (char ch : text) {
                if (ch == '$') doubled.push_back('$');
                doubled.push_back(ch);
            }
            out << "define dso_local void @rx_isr_" << isr << "() naked nounwind {\n";
            out << "  call void asm sideeffect \"" << EscapeLLVMString(doubled) << "\", \"\"()\n";
            out << "  unreachable\n}\n\n";
            continue;
        }
        const std::string eoi = ctx.timerIsrs.count(isr)
                                    ? "movb $$32, %al\noutb %al, $$32\n"
                                    : "";
        const std::string text =
            "pushq %rax\npushq %rcx\npushq %rdx\npushq %rsi\npushq %rdi\n"
            "pushq %r8\npushq %r9\npushq %r10\npushq %r11\n"
            "movabsq $$21248, %r11\nmovq %rax, (%r11)\n"
            "movabsq $$21256, %r11\nmovq %rdi, (%r11)\n" +
            eoi +
            "call rx_fn_" + isr + "\n"
            "popq %r11\npopq %r10\npopq %r9\npopq %r8\n"
            "popq %rdi\npopq %rsi\npopq %rdx\npopq %rcx\npopq %rax\n"
            "iretq";
        out << "define dso_local void @rx_isr_" << isr << "() naked nounwind {\n";
        out << "  call void asm sideeffect \"" << EscapeLLVMString(text) << "\", \"\"()\n";
        out << "  unreachable\n}\n\n";
    }

    if (ctx.usesGpu) {
        out << "@rx.gpu.count = internal global i64 0, align 8\n";
        out << "@rx.gpu.frame = internal global i64 0, align 8\n";
        out << "@rx.gpu.copy = internal global i64 0, align 8\n";
        out << "@rx.gpu.op = internal global [4 x i64] zeroinitializer, align 8\n";
        out << "@rx.gpu.rect = internal global [4 x i64] zeroinitializer, align 8\n";
        out << "@rx.gpu.color = internal global [4 x i64] zeroinitializer, align 8\n";
        out << "@rx.gpu.pix = internal global [64 x i32] zeroinitializer, align 4\n\n";
    }

    if (ctx.usesFrame) {
        if (m_options.freestanding) {
            out << "%RoboFrame = type { i32, i32, [2073600 x i32] }\n";
            out << "@rx.frame = internal global %RoboFrame zeroinitializer, align 16\n";
            out << "define i64 @rx_frame_create(i64 %w, i64 %h) {\n";
            out << "  %okw = icmp ule i64 %w, 1920\n";
            out << "  %okh = icmp ule i64 %h, 1080\n";
            out << "  %ok = and i1 %okw, %okh\n";
            out << "  br i1 %ok, label %store, label %die\n";
            out << "die:\n  call void @llvm.trap()\n  unreachable\n";
            out << "store:\n";
            out << "  %w32 = trunc i64 %w to i32\n";
            out << "  store i32 %w32, ptr @rx.frame, align 4\n";
            out << "  %hp = getelementptr i8, ptr @rx.frame, i64 4\n";
            out << "  %h32 = trunc i64 %h to i32\n";
            out << "  store i32 %h32, ptr %hp, align 4\n";
            out << "  %ret = ptrtoint ptr @rx.frame to i64\n";
            if (ctx.usesGpu) {
                out << "  store i64 %ret, ptr @rx.gpu.frame, align 8\n";
            }
            out << "  ret i64 %ret\n}\n\n";
        } else {
            out << "define i64 @rx_frame_create(i64 %w, i64 %h) {\n";
            out << "  %count = mul i64 %w, %h\n";
            out << "  %pix = mul i64 %count, 4\n";
            out << "  %bytes = add i64 %pix, 8\n";
            out << "  %mem = call ptr @malloc(i64 %bytes)\n";
            out << "  call void @llvm.memset.p0.i64(ptr %mem, i8 0, i64 %bytes, i1 false)\n";
            out << "  %w32 = trunc i64 %w to i32\n";
            out << "  store i32 %w32, ptr %mem, align 4\n";
            out << "  %hp = getelementptr i8, ptr %mem, i64 4\n";
            out << "  %h32 = trunc i64 %h to i32\n";
            out << "  store i32 %h32, ptr %hp, align 4\n";
            out << "  %ret = ptrtoint ptr %mem to i64\n";
            if (ctx.usesGpu) {
                out << "  store i64 %ret, ptr @rx.gpu.frame, align 8\n";
            }
            out << "  ret i64 %ret\n}\n\n";
        }
        out << "define void @rx_frame_fill(i64 %frame, i64 %x, i64 %y, i64 %rw, i64 %rh, i64 %color) noinline {\n";
        out << "entry:\n";
        out << "  %p = inttoptr i64 %frame to ptr\n";
        out << "  %fw32 = load i32, ptr %p, align 4\n";
        out << "  %fw = zext i32 %fw32 to i64\n";
        out << "  %hptr = getelementptr i8, ptr %p, i64 4\n";
        out << "  %fh32 = load i32, ptr %hptr, align 4\n";
        out << "  %fh = zext i32 %fh32 to i64\n";
        out << "  %c32 = trunc i64 %color to i32\n";
        out << "  br label %yloop\n";
        out << "yloop:\n";
        out << "  %yy = phi i64 [ 0, %entry ], [ %ynext, %xdone ]\n";
        out << "  %ydone = icmp uge i64 %yy, %rh\n";
        out << "  br i1 %ydone, label %end, label %xloop\n";
        out << "xloop:\n";
        out << "  %xx = phi i64 [ 0, %yloop ], [ %xnext, %xcont ]\n";
        out << "  %xdoneb = icmp uge i64 %xx, %rw\n";
        out << "  br i1 %xdoneb, label %xdone, label %plot\n";
        out << "plot:\n";
        out << "  %px = add i64 %x, %xx\n";
        out << "  %py = add i64 %y, %yy\n";
        out << "  %xin = icmp ult i64 %px, %fw\n";
        out << "  %yin = icmp ult i64 %py, %fh\n";
        out << "  %inside = and i1 %xin, %yin\n";
        out << "  br i1 %inside, label %storepx, label %xcont\n";
        out << "storepx:\n";
        out << "  %row = mul i64 %py, %fw\n";
        out << "  %idx = add i64 %row, %px\n";
        out << "  %base = getelementptr i8, ptr %p, i64 8\n";
        out << "  %slot = getelementptr i32, ptr %base, i64 %idx\n";
        out << "  store volatile i32 %c32, ptr %slot, align 4\n";
        out << "  br label %xcont\n";
        out << "xcont:\n";
        out << "  %xnext = add i64 %xx, 1\n";
        out << "  br label %xloop\n";
        out << "xdone:\n";
        out << "  %ynext = add i64 %yy, 1\n";
        out << "  br label %yloop\n";
        out << "end:\n  ret void\n}\n\n";
        out << "define i64 @rx_frame_pixel(i64 %frame, i64 %x, i64 %y) {\n";
        out << "  %p = inttoptr i64 %frame to ptr\n";
        out << "  %fw32 = load i32, ptr %p, align 4\n";
        out << "  %fw = zext i32 %fw32 to i64\n";
        out << "  %hptr = getelementptr i8, ptr %p, i64 4\n";
        out << "  %fh32 = load i32, ptr %hptr, align 4\n";
        out << "  %fh = zext i32 %fh32 to i64\n";
        out << "  %xin = icmp ult i64 %x, %fw\n";
        out << "  %yin = icmp ult i64 %y, %fh\n";
        out << "  %inside = and i1 %xin, %yin\n";
        out << "  br i1 %inside, label %load, label %miss\n";
        out << "miss:\n  ret i64 0\n";
        out << "load:\n";
        out << "  %row = mul i64 %y, %fw\n";
        out << "  %idx = add i64 %row, %x\n";
        out << "  %base = getelementptr i8, ptr %p, i64 8\n";
        out << "  %slot = getelementptr i32, ptr %base, i64 %idx\n";
        out << "  %v = load i32, ptr %slot, align 4\n";
        out << "  %wide = zext i32 %v to i64\n";
        out << "  ret i64 %wide\n}\n\n";
        if (m_options.freestanding) {
            out << "define i64 @rx_display_bar() {\nentry:\n  br label %slot\n";
            out << "slot:\n  %dev = phi i64 [ 0, %entry ], [ %nextdev, %advance ]\n";
            out << "  %finished = icmp uge i64 %dev, 32\n";
            out << "  br i1 %finished, label %fallback, label %probe\n";
            out << "probe:\n  %shifted = shl i64 %dev, 11\n";
            out << "  %cfg = or i64 %shifted, 2147483648\n";
            out << "  %cfg32 = trunc i64 %cfg to i32\n";
            out << "  call void asm sideeffect \"outl $0, $1\", \"{ax},{dx}\"(i32 %cfg32, i16 3320)\n";
            out << "  %id32 = call i32 asm sideeffect \"inl $1, $0\", \"={ax},{dx}\"(i16 3324)\n";
            out << "  %id = zext i32 %id32 to i64\n";
            out << "  %vendor = and i64 %id, 65535\n";
            out << "  %classCfg = or i64 %cfg, 8\n";
            out << "  %classCfg32 = trunc i64 %classCfg to i32\n";
            out << "  call void asm sideeffect \"outl $0, $1\", \"{ax},{dx}\"(i32 %classCfg32, i16 3320)\n";
            out << "  %class32 = call i32 asm sideeffect \"inl $1, $0\", \"={ax},{dx}\"(i16 3324)\n";
            out << "  %class64 = zext i32 %class32 to i64\n";
            out << "  %class = lshr i64 %class64, 24\n";
            out << "  %display = icmp eq i64 %class, 3\n";
            out << "  %bochs = icmp eq i64 %vendor, 4660\n";
            out << "  %hit = or i1 %display, %bochs\n";
            out << "  br i1 %hit, label %readbar, label %advance\n";
            out << "readbar:\n  %barCfg = or i64 %cfg, 16\n";
            out << "  %barCfg32 = trunc i64 %barCfg to i32\n";
            out << "  call void asm sideeffect \"outl $0, $1\", \"{ax},{dx}\"(i32 %barCfg32, i16 3320)\n";
            out << "  %bar32 = call i32 asm sideeffect \"inl $1, $0\", \"={ax},{dx}\"(i16 3324)\n";
            out << "  %bar = zext i32 %bar32 to i64\n";
            out << "  %io = and i64 %bar, 1\n";
            out << "  %isIo = icmp eq i64 %io, 1\n";
            out << "  br i1 %isIo, label %advance, label %memory\n";
            out << "memory:\n  %base = and i64 %bar, 4294967280\n";
            out << "  %empty = icmp eq i64 %base, 0\n";
            out << "  br i1 %empty, label %advance, label %enable\n";
            out << "enable:\n  %cmdCfg = or i64 %cfg, 4\n";
            out << "  %cmdCfg32 = trunc i64 %cmdCfg to i32\n";
            out << "  call void asm sideeffect \"outl $0, $1\", \"{ax},{dx}\"(i32 %cmdCfg32, i16 3320)\n";
            out << "  %cmd32 = call i32 asm sideeffect \"inl $1, $0\", \"={ax},{dx}\"(i16 3324)\n";
            out << "  %cmdOn = or i32 %cmd32, 2\n";
            out << "  call void asm sideeffect \"outl $0, $1\", \"{ax},{dx}\"(i32 %cmdCfg32, i16 3320)\n";
            out << "  call void asm sideeffect \"outl $0, $1\", \"{ax},{dx}\"(i32 %cmdOn, i16 3324)\n";
            out << "  ret i64 %base\n";
            out << "advance:\n  %nextdev = add i64 %dev, 1\n  br label %slot\n";
            out << "fallback:\n  ret i64 3758096384\n}\n\n";
            out << "@rx.frame.front = internal global i64 0, align 8\n";
            out << "define i64 @rx_frame_present(i64 %frame) noinline {\nentry:\n";
            out << "  %p = inttoptr i64 %frame to ptr\n";
            out << "  %fw32 = load i32, ptr %p, align 4\n";
            out << "  %fw = zext i32 %fw32 to i64\n";
            out << "  %hptr = getelementptr i8, ptr %p, i64 4\n";
            out << "  %fh32 = load i32, ptr %hptr, align 4\n";
            out << "  %fh = zext i32 %fh32 to i64\n";
            out << "  %fw16 = trunc i64 %fw to i16\n";
            out << "  %fh16 = trunc i64 %fh to i16\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 4, i16 462)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 0, i16 463)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 1, i16 462)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 %fw16, i16 463)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 2, i16 462)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 %fh16, i16 463)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 3, i16 462)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 32, i16 463)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 6, i16 462)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 %fw16, i16 463)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 7, i16 462)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 %fh16, i16 463)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 4, i16 462)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 65, i16 463)\n";
            out << "  %front = call i64 @rx_display_bar()\n";
            out << "  store i64 %front, ptr @rx.frame.front, align 8\n";
            out << "  %count = mul i64 %fw, %fh\n";
            out << "  %src = getelementptr i8, ptr %p, i64 8\n";
            out << "  %dst = inttoptr i64 %front to ptr\n";
            out << "  br label %copy\n";
            out << "copy:\n";
            out << "  %i = phi i64 [ 0, %entry ], [ %next, %one ]\n";
            out << "  %done = icmp uge i64 %i, %count\n";
            out << "  br i1 %done, label %copied, label %one\n";
            out << "one:\n";
            out << "  %ss = getelementptr i32, ptr %src, i64 %i\n";
            out << "  %dd = getelementptr i32, ptr %dst, i64 %i\n";
            out << "  %pix = load i32, ptr %ss, align 4\n";
            out << "  store volatile i32 %pix, ptr %dd, align 4\n";
            out << "  %next = add i64 %i, 1\n";
            out << "  br label %copy\n";
            out << "copied:\n  %shown = load volatile i8, ptr %dst, align 1\n";
            out << "  %byte = zext i8 %shown to i64\n  ret i64 %byte\n}\n\n";
            out << "define i64 @rx_frame_flush(i64 %frame, i64 %x, i64 %y, i64 %rw, i64 %rh) noinline {\nentry:\n";
            out << "  %p = inttoptr i64 %frame to ptr\n";
            out << "  %fw32 = load i32, ptr %p, align 4\n";
            out << "  %fw = zext i32 %fw32 to i64\n";
            out << "  %hptr = getelementptr i8, ptr %p, i64 4\n";
            out << "  %fh32 = load i32, ptr %hptr, align 4\n";
            out << "  %fh = zext i32 %fh32 to i64\n";
            out << "  %front = load i64, ptr @rx.frame.front, align 8\n";
            out << "  %dst = inttoptr i64 %front to ptr\n";
            out << "  %src = getelementptr i8, ptr %p, i64 8\n";
            out << "  br label %yloop\n";
            out << "yloop:\n  %yy = phi i64 [ 0, %entry ], [ %ynext, %xdone ]\n";
            out << "  %ydone = icmp uge i64 %yy, %rh\n";
            out << "  br i1 %ydone, label %end, label %xloop\n";
            out << "xloop:\n  %xx = phi i64 [ 0, %yloop ], [ %xnext, %xcont ]\n";
            out << "  %xdoneb = icmp uge i64 %xx, %rw\n";
            out << "  br i1 %xdoneb, label %xdone, label %plot\n";
            out << "plot:\n  %px = add i64 %x, %xx\n  %py = add i64 %y, %yy\n";
            out << "  %xin = icmp ult i64 %px, %fw\n  %yin = icmp ult i64 %py, %fh\n";
            out << "  %inside = and i1 %xin, %yin\n";
            out << "  br i1 %inside, label %storepx, label %xcont\n";
            out << "storepx:\n  %row = mul i64 %py, %fw\n  %idx = add i64 %row, %px\n";
            out << "  %ss = getelementptr i32, ptr %src, i64 %idx\n";
            out << "  %dd = getelementptr i32, ptr %dst, i64 %idx\n";
            out << "  %pix = load i32, ptr %ss, align 4\n";
            out << "  store volatile i32 %pix, ptr %dd, align 4\n";
            out << "  br label %xcont\n";
            out << "xcont:\n  %xnext = add i64 %xx, 1\n  br label %xloop\n";
            out << "xdone:\n  %ynext = add i64 %yy, 1\n  br label %yloop\n";
            out << "end:\n  ret i64 %rw\n}\n\n";
        } else {
            out << "define i64 @rx_frame_present(i64 %frame) {\n";
            out << "  ret i64 0\n}\n\n";
            out << "define i64 @rx_frame_flush(i64 %frame, i64 %x, i64 %y, i64 %rw, i64 %rh) {\n";
            out << "  ret i64 0\n}\n\n";
        }
    }

    if (ctx.usesGpu) {
        out << "define void @rx_gpu_plot(i64 %x, i64 %y, i64 %color) {\n";
        out << "  %xok = icmp ult i64 %x, 8\n  %yok = icmp ult i64 %y, 8\n  %ok = and i1 %xok, %yok\n";
        out << "  br i1 %ok, label %put, label %out\n";
        out << "put:\n  %row = mul i64 %y, 8\n  %idx = add i64 %row, %x\n  %c32 = trunc i64 %color to i32\n";
        out << "  %slot = getelementptr [64 x i32], ptr @rx.gpu.pix, i64 0, i64 %idx\n";
        out << "  store i32 %c32, ptr %slot, align 4\n";
        out << "  %copy = load i64, ptr @rx.gpu.copy, align 8\n  %on = icmp eq i64 %copy, 1\n";
        out << "  br i1 %on, label %frame, label %out\n";
        out << "frame:\n  %framep = load i64, ptr @rx.gpu.frame, align 8\n  %p = inttoptr i64 %framep to ptr\n";
        out << "  %fw32 = load i32, ptr %p, align 4\n  %fw = zext i32 %fw32 to i64\n";
        out << "  %xin = icmp ult i64 %x, %fw\n  %hp = getelementptr i8, ptr %p, i64 4\n";
        out << "  %fh32 = load i32, ptr %hp, align 4\n  %fh = zext i32 %fh32 to i64\n";
        out << "  %yin = icmp ult i64 %y, %fh\n  %inside = and i1 %xin, %yin\n";
        out << "  br i1 %inside, label %storepx, label %out\n";
        out << "storepx:\n  %frow = mul i64 %y, %fw\n  %fidx = add i64 %frow, %x\n";
        out << "  %base = getelementptr i8, ptr %p, i64 8\n  %fs = getelementptr i32, ptr %base, i64 %fidx\n";
        out << "  store volatile i32 %c32, ptr %fs, align 4\n  br label %out\n";
        out << "out:\n  ret void\n}\n\n";
        out << "define i64 @rx_gpu_read(i64 %x, i64 %y) {\n";
        out << "  %xok = icmp ult i64 %x, 8\n  %yok = icmp ult i64 %y, 8\n  %ok = and i1 %xok, %yok\n";
        out << "  br i1 %ok, label %load, label %miss\n";
        out << "miss:\n  ret i64 0\n";
        out << "load:\n  %row = mul i64 %y, 8\n  %idx = add i64 %row, %x\n";
        out << "  %slot = getelementptr [64 x i32], ptr @rx.gpu.pix, i64 0, i64 %idx\n";
        out << "  %v = load i32, ptr %slot, align 4\n  %wide = zext i32 %v to i64\n  ret i64 %wide\n}\n\n";
        out << "define i64 @rx_gpu_command(i64 %op, i64 %rect, i64 %color) {\n";
        out << "  %count = load i64, ptr @rx.gpu.count, align 8\n  %full = icmp uge i64 %count, 4\n";
        out << "  br i1 %full, label %deny, label %store\n";
        out << "deny:\n  ret i64 -1\n";
        out << "store:\n  %opP = getelementptr [4 x i64], ptr @rx.gpu.op, i64 0, i64 %count\n";
        out << "  store i64 %op, ptr %opP, align 8\n";
        out << "  %rectP = getelementptr [4 x i64], ptr @rx.gpu.rect, i64 0, i64 %count\n";
        out << "  store i64 %rect, ptr %rectP, align 8\n";
        out << "  %colorP = getelementptr [4 x i64], ptr @rx.gpu.color, i64 0, i64 %count\n";
        out << "  store i64 %color, ptr %colorP, align 8\n";
        out << "  %next = add i64 %count, 1\n  store i64 %next, ptr @rx.gpu.count, align 8\n  ret i64 %next\n}\n\n";
        out << "define i64 @rx_gpu_submit(i64 %buffer) {\n";
        out << "  %callerShift = lshr i64 %buffer, 16\n  %caller = and i64 %callerShift, 65535\n";
        out << "  %count = and i64 %buffer, 255\n  %isServer = icmp eq i64 %caller, 2\n";
        out << "  br i1 %isServer, label %check, label %deny\n";
        out << "deny:\n  ret i64 -1\n";
        out << "check:\n  %stored = load i64, ptr @rx.gpu.count, align 8\n";
        out << "  %same = icmp eq i64 %stored, %count\n  %some = icmp sgt i64 %count, 0\n";
        out << "  %okc = and i1 %same, %some\n  br i1 %okc, label %prepare, label %deny\n";
        out << "prepare:\n  %framep = load i64, ptr @rx.gpu.frame, align 8\n";
        out << "  %hasFrame = icmp ne i64 %framep, 0\n  br i1 %hasFrame, label %measure, label %nocopy\n";
        out << "measure:\n  %fp = inttoptr i64 %framep to ptr\n  %fhp = getelementptr i8, ptr %fp, i64 4\n";
        out << "  %fh32 = load i32, ptr %fhp, align 4\n  %fh = zext i32 %fh32 to i64\n";
        out << "  %small = icmp ule i64 %fh, 8\n  %doCopy = select i1 %small, i64 1, i64 0\n";
        out << "  store i64 %doCopy, ptr @rx.gpu.copy, align 8\n  br label %clear\n";
        out << "nocopy:\n  store i64 0, ptr @rx.gpu.copy, align 8\n  br label %clear\n";
        out << "clear:\n  br label %clr\n";
        out << "clr:\n  %k = phi i64 [ 0, %clear ], [ %knext, %clrbody ]\n";
        out << "  %kdone = icmp uge i64 %k, 64\n  br i1 %kdone, label %cmds, label %clrbody\n";
        out << "clrbody:\n  %ks = getelementptr [64 x i32], ptr @rx.gpu.pix, i64 0, i64 %k\n";
        out << "  store i32 0, ptr %ks, align 4\n  %knext = add i64 %k, 1\n  br label %clr\n";
        out << "cmds:\n  br label %cmd\n";
        out << "cmd:\n  %ci = phi i64 [ 0, %cmds ], [ %cnext, %cmdnext ]\n";
        out << "  %cdone = icmp uge i64 %ci, %count\n  br i1 %cdone, label %origin, label %loadcmd\n";
        out << "loadcmd:\n  %opP = getelementptr [4 x i64], ptr @rx.gpu.op, i64 0, i64 %ci\n";
        out << "  %op = load i64, ptr %opP, align 8\n";
        out << "  %rectP = getelementptr [4 x i64], ptr @rx.gpu.rect, i64 0, i64 %ci\n";
        out << "  %rect = load i64, ptr %rectP, align 8\n";
        out << "  %colorP = getelementptr [4 x i64], ptr @rx.gpu.color, i64 0, i64 %ci\n";
        out << "  %col = load i64, ptr %colorP, align 8\n";
        out << "  %x = and i64 %rect, 255\n  %ys = lshr i64 %rect, 8\n  %y = and i64 %ys, 255\n";
        out << "  %ws = lshr i64 %rect, 16\n  %w = and i64 %ws, 255\n";
        out << "  %hs = lshr i64 %rect, 24\n  %rh = and i64 %hs, 255\n";
        out << "  %isFill = icmp eq i64 %op, 1\n  br i1 %isFill, label %fill, label %checkblit\n";
        out << "checkblit:\n  %isBlit = icmp eq i64 %op, 2\n  br i1 %isBlit, label %blit, label %cmdnext\n";
        out << "fill:\n  br label %fy\n";
        out << "fy:\n  %yy = phi i64 [ 0, %fill ], [ %ynext, %fxdone ]\n";
        out << "  %ydone = icmp uge i64 %yy, %rh\n  br i1 %ydone, label %cmdnext, label %fx\n";
        out << "fx:\n  %xx = phi i64 [ 0, %fy ], [ %xnext, %fxcont ]\n";
        out << "  %xdone = icmp uge i64 %xx, %w\n  br i1 %xdone, label %fxdone, label %fplot\n";
        out << "fplot:\n  %px = add i64 %x, %xx\n  %py = add i64 %y, %yy\n";
        out << "  call void @rx_gpu_plot(i64 %px, i64 %py, i64 %col)\n  br label %fxcont\n";
        out << "fxcont:\n  %xnext = add i64 %xx, 1\n  br label %fx\n";
        out << "fxdone:\n  %ynext = add i64 %yy, 1\n  br label %fy\n";
        out << "blit:\n  %sx0 = and i64 %col, 255\n  %sys = lshr i64 %col, 8\n  %sy0 = and i64 %sys, 255\n";
        out << "  br label %by\n";
        out << "by:\n  %byy = phi i64 [ 0, %blit ], [ %bynext, %bxdone ]\n";
        out << "  %bydone = icmp uge i64 %byy, %rh\n  br i1 %bydone, label %cmdnext, label %bx\n";
        out << "bx:\n  %bxx = phi i64 [ 0, %by ], [ %bxnext, %bxcont ]\n";
        out << "  %bxdoneb = icmp uge i64 %bxx, %w\n  br i1 %bxdoneb, label %bxdone, label %bplot\n";
        out << "bplot:\n  %dpx = add i64 %x, %bxx\n  %dpy = add i64 %y, %byy\n";
        out << "  %spx = add i64 %sx0, %bxx\n  %spy = add i64 %sy0, %byy\n";
        out << "  %sample = call i64 @rx_gpu_read(i64 %spx, i64 %spy)\n";
        out << "  call void @rx_gpu_plot(i64 %dpx, i64 %dpy, i64 %sample)\n  br label %bxcont\n";
        out << "bxcont:\n  %bxnext = add i64 %bxx, 1\n  br label %bx\n";
        out << "bxdone:\n  %bynext = add i64 %byy, 1\n  br label %by\n";
        out << "cmdnext:\n  %cnext = add i64 %ci, 1\n  br label %cmd\n";
        out << "origin:\n  %originP = getelementptr [64 x i32], ptr @rx.gpu.pix, i64 0, i64 0\n";
        out << "  %origin32 = load i32, ptr %originP, align 4\n  %originWide = zext i32 %origin32 to i64\n";
        out << "  ret i64 %originWide\n}\n\n";
    }

    if (ctx.usesUI) {
        const std::size_t count = ctx.uiBoxes.size();
        const std::size_t slots = count * 5;
        if (slots == 0) {
            out << "define i64 @rx_ui_count() {\n  ret i64 0\n}\n\n";
            out << "define i64 @rx_ui_load() {\n  ret i64 0\n}\n\n";
            out << "define i64 @rx_ui_field(i64 %i, i64 %f) {\n  ret i64 0\n}\n\n";
            out << "define i64 @rx_ui_hit(i64 %x, i64 %y) {\n  ret i64 0\n}\n\n";
            out << "define i64 @rx_ui_drag(i64 %i, i64 %dx, i64 %dy) {\n  ret i64 0\n}\n\n";
            out << "define i64 @rx_ui_snap(i64 %i, i64 %side) {\n  ret i64 0\n}\n\n";
            out << "define i64 @rx_ui_place(i64 %i, i64 %x, i64 %y, i64 %w, i64 %h) {\n  ret i64 0\n}\n\n";
            out << "define i64 @rx_ui_focus(i64 %i) {\n  ret i64 %i\n}\n\n";
            out << "define i64 @rx_ui_focused() {\n  ret i64 -1\n}\n\n";
            out << "define i64 @rx_ui_menu(i64 %flag) {\n  ret i64 %flag\n}\n\n";
            out << "define i64 @rx_ui_menu_open() {\n  ret i64 0\n}\n\n";
        } else {
            out << "@rx.ui.initial = internal constant [" << slots << " x i32] [";
            for (std::size_t index = 0; index < count; ++index) {
                const Robix::UI::RoboCookedBox& box = ctx.uiBoxes[index];
                const int fields[5] = { box.role, box.x, box.y, box.width, box.height };
                for (int field = 0; field < 5; ++field) {
                    if (index != 0 || field != 0) out << ", ";
                    out << "i32 " << fields[field];
                }
            }
            out << "]\n";
            out << "@rx.ui.boxes = internal global [" << slots << " x i32] zeroinitializer\n";
            out << "define i64 @rx_ui_count() {\n  ret i64 " << count << "\n}\n\n";
            out << "define i64 @rx_ui_field(i64 %i, i64 %f) {\n";
            out << "  %in = icmp ult i64 %i, " << count << "\n";
            out << "  %fn = icmp ult i64 %f, 5\n";
            out << "  %ok = and i1 %in, %fn\n";
            out << "  br i1 %ok, label %hit, label %miss\n";
            out << "miss:\n  ret i64 0\n";
            out << "hit:\n  %base = mul i64 %i, 5\n  %slot = add i64 %base, %f\n";
            out << "  %p = getelementptr [" << slots << " x i32], ptr @rx.ui.boxes, i64 0, i64 %slot\n";
            out << "  %v = load i32, ptr %p, align 4\n  %z = sext i32 %v to i64\n  ret i64 %z\n}\n\n";
            int screenWidth = 320;
            int screenHeight = 200;
            for (const Robix::UI::RoboCookedBox& box : ctx.uiBoxes) {
                if (box.role == 6) {
                    screenWidth = box.width;
                    screenHeight = box.height;
                }
            }
            const int halfWidth = screenWidth / 2;
            out << "@rx.ui.focus = internal global i32 -1\n";
            out << "@rx.ui.menu = internal global i32 0\n";
            out << "define i64 @rx_ui_load() {\n";
            out << "entry:\n  br label %loop\n";
            out << "loop:\n  %i = phi i64 [ 0, %entry ], [ %next, %step ]\n";
            out << "  %done = icmp uge i64 %i, " << slots << "\n";
            out << "  br i1 %done, label %reset, label %step\n";
            out << "step:\n  %src = getelementptr [" << slots << " x i32], ptr @rx.ui.initial, i64 0, i64 %i\n";
            out << "  %dst = getelementptr [" << slots << " x i32], ptr @rx.ui.boxes, i64 0, i64 %i\n";
            out << "  %v = load i32, ptr %src, align 4\n  store volatile i32 %v, ptr %dst, align 4\n";
            out << "  %next = add i64 %i, 1\n  br label %loop\n";
            out << "reset:\n  store i32 -1, ptr @rx.ui.focus, align 4\n";
            out << "  store i32 0, ptr @rx.ui.menu, align 4\n";
            out << "  ret i64 " << count << "\n}\n\n";
            out << "define void @rx_ui_set(i64 %i, i64 %f, i64 %v) {\n";
            out << "  %in = icmp ult i64 %i, " << count << "\n";
            out << "  %fn = icmp ult i64 %f, 5\n";
            out << "  %ok = and i1 %in, %fn\n";
            out << "  br i1 %ok, label %store, label %skip\n";
            out << "skip:\n  ret void\n";
            out << "store:\n  %base = mul i64 %i, 5\n  %slot = add i64 %base, %f\n";
            out << "  %p = getelementptr [" << slots << " x i32], ptr @rx.ui.boxes, i64 0, i64 %slot\n";
            out << "  %v32 = trunc i64 %v to i32\n  store i32 %v32, ptr %p, align 4\n  ret void\n}\n\n";
            out << "define i64 @rx_ui_place(i64 %i, i64 %x, i64 %y, i64 %w, i64 %h) {\n";
            out << "  %in = icmp ult i64 %i, " << count << "\n  br i1 %in, label %put, label %no\n";
            out << "no:\n  ret i64 0\n";
            out << "put:\n  call void @rx_ui_set(i64 %i, i64 1, i64 %x)\n";
            out << "  call void @rx_ui_set(i64 %i, i64 2, i64 %y)\n";
            out << "  call void @rx_ui_set(i64 %i, i64 3, i64 %w)\n";
            out << "  call void @rx_ui_set(i64 %i, i64 4, i64 %h)\n";
            out << "  %role = call i64 @rx_ui_field(i64 %i, i64 0)\n";
            out << "  %text = icmp eq i64 %role, 5\n  br i1 %text, label %textk, label %icon\n";
            out << "textk:\n  ret i64 4\n";
            out << "icon:\n  %isIcon = icmp eq i64 %role, 2\n  %isButton = icmp eq i64 %role, 4\n";
            out << "  %ptr = or i1 %isIcon, %isButton\n  br i1 %ptr, label %ptrk, label %plain\n";
            out << "ptrk:\n  ret i64 1\nplain:\n  ret i64 0\n}\n\n";
            out << "define i64 @rx_ui_drag(i64 %i, i64 %dx, i64 %dy) {\n";
            out << "  %in = icmp ult i64 %i, " << count << "\n  br i1 %in, label %move, label %no\n";
            out << "no:\n  ret i64 0\n";
            out << "move:\n  %x = call i64 @rx_ui_field(i64 %i, i64 1)\n";
            out << "  %y = call i64 @rx_ui_field(i64 %i, i64 2)\n";
            out << "  %nx = add i64 %x, %dx\n  %ny = add i64 %y, %dy\n";
            out << "  call void @rx_ui_set(i64 %i, i64 1, i64 %nx)\n";
            out << "  call void @rx_ui_set(i64 %i, i64 2, i64 %ny)\n  ret i64 2\n}\n\n";
            out << "define i64 @rx_ui_snap(i64 %i, i64 %side) {\n";
            out << "  %in = icmp ult i64 %i, " << count << "\n  br i1 %in, label %fit, label %no\n";
            out << "no:\n  ret i64 0\n";
            out << "fit:\n  %right = icmp eq i64 %side, 1\n  br i1 %right, label %east, label %west\n";
            out << "east:\n  call void @rx_ui_set(i64 %i, i64 1, i64 " << halfWidth << ")\n  br label %size\n";
            out << "west:\n  call void @rx_ui_set(i64 %i, i64 1, i64 0)\n  br label %size\n";
            out << "size:\n  call void @rx_ui_set(i64 %i, i64 2, i64 0)\n";
            out << "  call void @rx_ui_set(i64 %i, i64 3, i64 " << halfWidth << ")\n";
            out << "  call void @rx_ui_set(i64 %i, i64 4, i64 " << screenHeight << ")\n  ret i64 6\n}\n\n";
            out << "define i64 @rx_ui_hit(i64 %x, i64 %y) {\n";
            out << "entry:\n  br label %loop\n";
            out << "loop:\n  %i = phi i64 [ " << count << ", %entry ], [ %prev, %miss ]\n";
            out << "  %done = icmp eq i64 %i, 0\n  br i1 %done, label %none, label %check\n";
            out << "check:\n  %prev = sub i64 %i, 1\n";
            out << "  %role = call i64 @rx_ui_field(i64 %prev, i64 0)\n";
            out << "  %x0 = call i64 @rx_ui_field(i64 %prev, i64 1)\n";
            out << "  %y0 = call i64 @rx_ui_field(i64 %prev, i64 2)\n";
            out << "  %bw = call i64 @rx_ui_field(i64 %prev, i64 3)\n";
            out << "  %bh = call i64 @rx_ui_field(i64 %prev, i64 4)\n";
            out << "  %x1 = add i64 %x0, %bw\n  %y1 = add i64 %y0, %bh\n";
            out << "  %geL = icmp sge i64 %x, %x0\n  %ltR = icmp slt i64 %x, %x1\n";
            out << "  %geT = icmp sge i64 %y, %y0\n  %ltB = icmp slt i64 %y, %y1\n";
            out << "  %hx = and i1 %geL, %ltR\n  %hy = and i1 %geT, %ltB\n  %inside = and i1 %hx, %hy\n";
            out << "  %screen = icmp ne i64 %role, 6\n  %pick = and i1 %inside, %screen\n";
            out << "  br i1 %pick, label %kind, label %miss\n";
            out << "miss:\n  br label %loop\n";
            out << "none:\n  ret i64 0\n";
            out << "kind:\n  %isText = icmp eq i64 %role, 5\n  br i1 %isText, label %textk, label %icon\n";
            out << "textk:\n  ret i64 4\n";
            out << "icon:\n  %isIcon = icmp eq i64 %role, 2\n  %isButton = icmp eq i64 %role, 4\n";
            out << "  %ptr = or i1 %isIcon, %isButton\n  br i1 %ptr, label %ptrk, label %window\n";
            out << "ptrk:\n  ret i64 1\n";
            out << "window:\n  %isWin = icmp eq i64 %role, 3\n  br i1 %isWin, label %edge, label %plain\n";
            out << "plain:\n  ret i64 0\n";
            out << "edge:\n  %leftLim = add i64 %x0, 4\n  %rightLim = sub i64 %x1, 4\n";
            out << "  %topLim = add i64 %y0, 4\n  %botLim = sub i64 %y1, 4\n  %titleLim = add i64 %y0, 16\n";
            out << "  %left = icmp slt i64 %x, %leftLim\n  %right = icmp sge i64 %x, %rightLim\n";
            out << "  %top = icmp slt i64 %y, %topLim\n  %bottom = icmp sge i64 %y, %botLim\n";
            out << "  %lr = or i1 %left, %right\n  %tb = or i1 %top, %bottom\n  %corner = and i1 %lr, %tb\n";
            out << "  br i1 %corner, label %cornerk, label %side\n";
            out << "side:\n  br i1 %lr, label %col, label %rows\n";
            out << "col:\n  ret i64 6\n";
            out << "rows:\n  br i1 %tb, label %rowk, label %title\n";
            out << "rowk:\n  ret i64 7\n";
            out << "title:\n  %grab = icmp slt i64 %y, %titleLim\n  br i1 %grab, label %grabk, label %plain2\n";
            out << "grabk:\n  ret i64 2\nplain2:\n  ret i64 0\n";
            out << "cornerk:\n  %nw = and i1 %left, %top\n  %se = and i1 %right, %bottom\n  %nwse = or i1 %nw, %se\n";
            out << "  br i1 %nwse, label %nwsek, label %neswk\n";
            out << "nwsek:\n  ret i64 9\nneswk:\n  ret i64 10\n}\n\n";
            out << "define i64 @rx_ui_focus(i64 %i) {\n  %v = trunc i64 %i to i32\n";
            out << "  store i32 %v, ptr @rx.ui.focus, align 4\n  ret i64 %i\n}\n\n";
            out << "define i64 @rx_ui_focused() {\n  %v = load i32, ptr @rx.ui.focus, align 4\n";
            out << "  %z = sext i32 %v to i64\n  ret i64 %z\n}\n\n";
            out << "define i64 @rx_ui_menu(i64 %flag) {\n  %v = trunc i64 %flag to i32\n";
            out << "  store i32 %v, ptr @rx.ui.menu, align 4\n  ret i64 %flag\n}\n\n";
            out << "define i64 @rx_ui_menu_open() {\n  %v = load i32, ptr @rx.ui.menu, align 4\n";
            out << "  %z = sext i32 %v to i64\n  ret i64 %z\n}\n\n";
        }
    }

    if (ctx.usesSched) {
        out << "@rx.sched.lock = internal global i64 0\n";
        out << "@rx.sched.id = internal global [8 x i64] zeroinitializer\n";
        out << "@rx.sched.pri = internal global [8 x i64] zeroinitializer\n";
        out << "@rx.sched.state = internal global [8 x i64] zeroinitializer\n";
        out << "define internal void @rx_sched_lock() {\n";
        out << "entry:\n  br label %spin\n";
        out << "spin:\n  %got = atomicrmw xchg ptr @rx.sched.lock, i64 1 seq_cst\n";
        out << "  %free = icmp eq i64 %got, 0\n  br i1 %free, label %out, label %spin\n";
        out << "out:\n  ret void\n}\n\n";
        out << "define internal void @rx_sched_unlock() {\n";
        out << "  store atomic i64 0, ptr @rx.sched.lock seq_cst, align 8\n  ret void\n}\n\n";
        out << "define i64 @rx_sched_enqueue(i64 %id, i64 %pri) {\n";
        out << "entry:\n  call void @rx_sched_lock()\n  br label %find\n";
        out << "find:\n  %i = phi i64 [ 0, %entry ], [ %inext, %findnext ]\n";
        out << "  %done = icmp uge i64 %i, 8\n  br i1 %done, label %insert, label %check\n";
        out << "check:\n  %idp = getelementptr [8 x i64], ptr @rx.sched.id, i64 0, i64 %i\n";
        out << "  %got = load i64, ptr %idp, align 8\n";
        out << "  %stp = getelementptr [8 x i64], ptr @rx.sched.state, i64 0, i64 %i\n";
        out << "  %state = load i64, ptr %stp, align 8\n";
        out << "  %alive = icmp ne i64 %state, 0\n  %same = icmp eq i64 %got, %id\n";
        out << "  %hit = and i1 %alive, %same\n  br i1 %hit, label %wake, label %findnext\n";
        out << "findnext:\n  %inext = add i64 %i, 1\n  br label %find\n";
        out << "wake:\n  %pp = getelementptr [8 x i64], ptr @rx.sched.pri, i64 0, i64 %i\n";
        out << "  store i64 %pri, ptr %pp, align 8\n  store i64 1, ptr %stp, align 8\n  br label %count\n";
        out << "insert:\n  br label %scan\n";
        out << "scan:\n  %j = phi i64 [ 0, %insert ], [ %jnext, %scannext ]\n";
        out << "  %full = icmp uge i64 %j, 8\n  br i1 %full, label %count, label %empty\n";
        out << "empty:\n  %est = getelementptr [8 x i64], ptr @rx.sched.state, i64 0, i64 %j\n";
        out << "  %estv = load i64, ptr %est, align 8\n  %is0 = icmp eq i64 %estv, 0\n";
        out << "  br i1 %is0, label %fill, label %scannext\n";
        out << "scannext:\n  %jnext = add i64 %j, 1\n  br label %scan\n";
        out << "fill:\n  %eid = getelementptr [8 x i64], ptr @rx.sched.id, i64 0, i64 %j\n";
        out << "  %epr = getelementptr [8 x i64], ptr @rx.sched.pri, i64 0, i64 %j\n";
        out << "  store i64 %id, ptr %eid, align 8\n  store i64 %pri, ptr %epr, align 8\n";
        out << "  store i64 1, ptr %est, align 8\n  br label %count\n";
        out << "count:\n  br label %cloop\n";
        out << "cloop:\n  %k = phi i64 [ 0, %count ], [ %knext, %cstep ]\n";
        out << "  %n = phi i64 [ 0, %count ], [ %n2, %cstep ]\n";
        out << "  %cdone = icmp uge i64 %k, 8\n  br i1 %cdone, label %ret, label %cbody\n";
        out << "cbody:\n  %csp = getelementptr [8 x i64], ptr @rx.sched.state, i64 0, i64 %k\n";
        out << "  %csv = load i64, ptr %csp, align 8\n  %ready = icmp eq i64 %csv, 1\n";
        out << "  %add = zext i1 %ready to i64\n  %n2 = add i64 %n, %add\n  br label %cstep\n";
        out << "cstep:\n  %knext = add i64 %k, 1\n  br label %cloop\n";
        out << "ret:\n  call void @rx_sched_unlock()\n  ret i64 %n\n}\n\n";
        out << "define i64 @rx_sched_pick(i64 %core) {\n";
        out << "entry:\n  call void @rx_sched_lock()\n  br label %scan\n";
        out << "scan:\n  %i = phi i64 [ 0, %entry ], [ %inext, %step ]\n";
        out << "  %best = phi i64 [ -1, %entry ], [ %best2, %step ]\n";
        out << "  %bestpri = phi i64 [ -1, %entry ], [ %bestpri2, %step ]\n";
        out << "  %done = icmp uge i64 %i, 8\n  br i1 %done, label %take, label %body\n";
        out << "body:\n  %stp = getelementptr [8 x i64], ptr @rx.sched.state, i64 0, i64 %i\n";
        out << "  %state = load i64, ptr %stp, align 8\n  %ready = icmp eq i64 %state, 1\n";
        out << "  %pp = getelementptr [8 x i64], ptr @rx.sched.pri, i64 0, i64 %i\n";
        out << "  %pri = load i64, ptr %pp, align 8\n  %higher = icmp sgt i64 %pri, %bestpri\n";
        out << "  %use = and i1 %ready, %higher\n  br i1 %use, label %better, label %keep\n";
        out << "better:\n  br label %step\n";
        out << "keep:\n  br label %step\n";
        out << "step:\n  %best2 = phi i64 [ %i, %better ], [ %best, %keep ]\n";
        out << "  %bestpri2 = phi i64 [ %pri, %better ], [ %bestpri, %keep ]\n";
        out << "  %inext = add i64 %i, 1\n  br label %scan\n";
        out << "take:\n  %none = icmp slt i64 %best, 0\n  br i1 %none, label %miss, label %hit\n";
        out << "miss:\n  call void @rx_sched_unlock()\n  ret i64 0\n";
        out << "hit:\n  %hst = getelementptr [8 x i64], ptr @rx.sched.state, i64 0, i64 %best\n";
        out << "  store i64 3, ptr %hst, align 8\n";
        out << "  %hid = getelementptr [8 x i64], ptr @rx.sched.id, i64 0, i64 %best\n";
        out << "  %id = load i64, ptr %hid, align 8\n";
        out << "  call void @rx_sched_unlock()\n  ret i64 %id\n}\n\n";
        out << "define i64 @rx_sched_block(i64 %id) {\n";
        out << "entry:\n  call void @rx_sched_lock()\n  br label %find\n";
        out << "find:\n  %i = phi i64 [ 0, %entry ], [ %inext, %next ]\n";
        out << "  %done = icmp uge i64 %i, 8\n  br i1 %done, label %miss, label %check\n";
        out << "check:\n  %idp = getelementptr [8 x i64], ptr @rx.sched.id, i64 0, i64 %i\n";
        out << "  %got = load i64, ptr %idp, align 8\n";
        out << "  %stp = getelementptr [8 x i64], ptr @rx.sched.state, i64 0, i64 %i\n";
        out << "  %state = load i64, ptr %stp, align 8\n";
        out << "  %alive = icmp ne i64 %state, 0\n  %same = icmp eq i64 %got, %id\n";
        out << "  %hit = and i1 %alive, %same\n  br i1 %hit, label %stop, label %next\n";
        out << "next:\n  %inext = add i64 %i, 1\n  br label %find\n";
        out << "stop:\n  store i64 2, ptr %stp, align 8\n  call void @rx_sched_unlock()\n  ret i64 1\n";
        out << "miss:\n  call void @rx_sched_unlock()\n  ret i64 0\n}\n\n";
    }

    if (ctx.usesPort) {
        out << "@rx.port.lock = internal global i64 0\n";
        out << "@rx.port.owner = internal global [8 x i64] zeroinitializer\n";
        out << "@rx.port.state = internal global [8 x i64] zeroinitializer\n";
        out << "@rx.port.word = internal global [8 x i64] zeroinitializer\n";
        out << "@rx.port.full = internal global [8 x i64] zeroinitializer\n";
        out << "define internal void @rx_port_lock() {\n";
        out << "entry:\n  br label %spin\n";
        out << "spin:\n  %got = atomicrmw xchg ptr @rx.port.lock, i64 1 seq_cst\n";
        out << "  %free = icmp eq i64 %got, 0\n  br i1 %free, label %out, label %spin\n";
        out << "out:\n  ret void\n}\n\n";
        out << "define internal void @rx_port_unlock() {\n";
        out << "  store atomic i64 0, ptr @rx.port.lock seq_cst, align 8\n  ret void\n}\n\n";
        out << "define i64 @rx_port_create(i64 %owner) {\n";
        out << "entry:\n  call void @rx_port_lock()\n  br label %scan\n";
        out << "scan:\n  %i = phi i64 [ 0, %entry ], [ %next, %step ]\n";
        out << "  %done = icmp uge i64 %i, 8\n  br i1 %done, label %fail, label %check\n";
        out << "check:\n  %sp = getelementptr [8 x i64], ptr @rx.port.state, i64 0, i64 %i\n";
        out << "  %st = load i64, ptr %sp, align 8\n  %empty = icmp eq i64 %st, 0\n";
        out << "  br i1 %empty, label %fill, label %step\n";
        out << "step:\n  %next = add i64 %i, 1\n  br label %scan\n";
        out << "fill:\n  %op = getelementptr [8 x i64], ptr @rx.port.owner, i64 0, i64 %i\n";
        out << "  store i64 %owner, ptr %op, align 8\n";
        out << "  %closed = icmp eq i64 %owner, 0\n  %mark = select i1 %closed, i64 2, i64 1\n";
        out << "  store i64 %mark, ptr %sp, align 8\n";
        out << "  %wp = getelementptr [8 x i64], ptr @rx.port.word, i64 0, i64 %i\n";
        out << "  %fp = getelementptr [8 x i64], ptr @rx.port.full, i64 0, i64 %i\n";
        out << "  store i64 0, ptr %wp, align 8\n  store i64 0, ptr %fp, align 8\n";
        out << "  %id = add i64 %i, 1\n  call void @rx_port_unlock()\n  ret i64 %id\n";
        out << "fail:\n  call void @rx_port_unlock()\n  ret i64 0\n}\n\n";
        out << "define i64 @rx_port_send(i64 %id, i64 %word) {\n";
        out << "entry:\n  %pos = icmp sgt i64 %id, 0\n  %lim = icmp sle i64 %id, 8\n";
        out << "  %range = and i1 %pos, %lim\n  br i1 %range, label %lock, label %closed\n";
        out << "closed:\n  ret i64 -1\n";
        out << "lock:\n  call void @rx_port_lock()\n  %slot = sub i64 %id, 1\n";
        out << "  %sp = getelementptr [8 x i64], ptr @rx.port.state, i64 0, i64 %slot\n";
        out << "  %st = load i64, ptr %sp, align 8\n  %open = icmp eq i64 %st, 1\n";
        out << "  br i1 %open, label %checkfull, label %denied\n";
        out << "denied:\n  call void @rx_port_unlock()\n  ret i64 -1\n";
        out << "checkfull:\n  %fp = getelementptr [8 x i64], ptr @rx.port.full, i64 0, i64 %slot\n";
        out << "  %full = load i64, ptr %fp, align 8\n  %busy = icmp ne i64 %full, 0\n";
        out << "  br i1 %busy, label %wait, label %put\n";
        out << "wait:\n  call void @rx_port_unlock()\n  ret i64 -2\n";
        out << "put:\n  %masked = and i64 %word, 4294967295\n";
        out << "  %wp = getelementptr [8 x i64], ptr @rx.port.word, i64 0, i64 %slot\n";
        out << "  store i64 %masked, ptr %wp, align 8\n  store i64 1, ptr %fp, align 8\n";
        out << "  call void @rx_port_unlock()\n  ret i64 4\n}\n\n";
        out << "define i64 @rx_port_receive(i64 %id) {\n";
        out << "entry:\n  %pos = icmp sgt i64 %id, 0\n  %lim = icmp sle i64 %id, 8\n";
        out << "  %range = and i1 %pos, %lim\n  br i1 %range, label %lock, label %closed\n";
        out << "closed:\n  ret i64 -1\n";
        out << "lock:\n  call void @rx_port_lock()\n  %slot = sub i64 %id, 1\n";
        out << "  %sp = getelementptr [8 x i64], ptr @rx.port.state, i64 0, i64 %slot\n";
        out << "  %st = load i64, ptr %sp, align 8\n  %open = icmp eq i64 %st, 1\n";
        out << "  br i1 %open, label %checkfull, label %denied\n";
        out << "denied:\n  call void @rx_port_unlock()\n  ret i64 -1\n";
        out << "checkfull:\n  %fp = getelementptr [8 x i64], ptr @rx.port.full, i64 0, i64 %slot\n";
        out << "  %full = load i64, ptr %fp, align 8\n  %has = icmp ne i64 %full, 0\n";
        out << "  br i1 %has, label %take, label %empty\n";
        out << "empty:\n  call void @rx_port_unlock()\n  ret i64 -2\n";
        out << "take:\n  %wp = getelementptr [8 x i64], ptr @rx.port.word, i64 0, i64 %slot\n";
        out << "  %word = load i64, ptr %wp, align 8\n  store i64 0, ptr %fp, align 8\n";
        out << "  call void @rx_port_unlock()\n  ret i64 %word\n}\n\n";
    }

    if (ctx.usesProc) {
        out << "@rx.proc.lock = internal global i64 0\n";
        out << "@rx.proc.parent = internal global [8 x i64] zeroinitializer\n";
        out << "@rx.proc.user = internal global [8 x i64] zeroinitializer\n";
        out << "@rx.proc.state = internal global [8 x i64] zeroinitializer\n";
        out << "@rx.proc.status = internal global [8 x i64] zeroinitializer\n";
        out << "define internal void @rx_proc_lock() {\n";
        out << "entry:\n  br label %spin\n";
        out << "spin:\n  %got = atomicrmw xchg ptr @rx.proc.lock, i64 1 seq_cst\n";
        out << "  %free = icmp eq i64 %got, 0\n  br i1 %free, label %out, label %spin\n";
        out << "out:\n  ret void\n}\n\n";
        out << "define internal void @rx_proc_unlock() {\n";
        out << "  store atomic i64 0, ptr @rx.proc.lock seq_cst, align 8\n  ret void\n}\n\n";
        out << "define i64 @rx_proc_create(i64 %parent, i64 %user) {\n";
        out << "entry:\n  call void @rx_proc_lock()\n";
        out << "  %kernel = icmp eq i64 %parent, 0\n  br i1 %kernel, label %ready, label %check\n";
        out << "check:\n  %pos = icmp sgt i64 %parent, 0\n  %lim = icmp sle i64 %parent, 8\n";
        out << "  %range = and i1 %pos, %lim\n  br i1 %range, label %load, label %fail\n";
        out << "load:\n  %pslot = sub i64 %parent, 1\n";
        out << "  %pp = getelementptr [8 x i64], ptr @rx.proc.state, i64 0, i64 %pslot\n";
        out << "  %pst = load i64, ptr %pp, align 8\n  %alive = icmp eq i64 %pst, 1\n";
        out << "  br i1 %alive, label %ready, label %fail\n";
        out << "ready:\n  br label %scan\n";
        out << "scan:\n  %i = phi i64 [ 0, %ready ], [ %next, %step ]\n";
        out << "  %done = icmp uge i64 %i, 8\n  br i1 %done, label %fail, label %slot\n";
        out << "slot:\n  %sp = getelementptr [8 x i64], ptr @rx.proc.state, i64 0, i64 %i\n";
        out << "  %st = load i64, ptr %sp, align 8\n  %empty = icmp eq i64 %st, 0\n";
        out << "  br i1 %empty, label %fill, label %step\n";
        out << "step:\n  %next = add i64 %i, 1\n  br label %scan\n";
        out << "fill:\n  %par = getelementptr [8 x i64], ptr @rx.proc.parent, i64 0, i64 %i\n";
        out << "  %usr = getelementptr [8 x i64], ptr @rx.proc.user, i64 0, i64 %i\n";
        out << "  %code = getelementptr [8 x i64], ptr @rx.proc.status, i64 0, i64 %i\n";
        out << "  store i64 %parent, ptr %par, align 8\n  store i64 %user, ptr %usr, align 8\n";
        out << "  store i64 1, ptr %sp, align 8\n  store i64 0, ptr %code, align 8\n";
        out << "  %id = add i64 %i, 1\n  call void @rx_proc_unlock()\n  ret i64 %id\n";
        out << "fail:\n  call void @rx_proc_unlock()\n  ret i64 0\n}\n\n";
        out << "define i64 @rx_proc_exit(i64 %id, i64 %status) {\n";
        out << "entry:\n  %pos = icmp sgt i64 %id, 0\n  %lim = icmp sle i64 %id, 8\n";
        out << "  %range = and i1 %pos, %lim\n  br i1 %range, label %lock, label %miss\n";
        out << "miss:\n  ret i64 0\n";
        out << "lock:\n  call void @rx_proc_lock()\n  %slot = sub i64 %id, 1\n";
        out << "  %sp = getelementptr [8 x i64], ptr @rx.proc.state, i64 0, i64 %slot\n";
        out << "  %st = load i64, ptr %sp, align 8\n  %alive = icmp eq i64 %st, 1\n";
        out << "  br i1 %alive, label %stop, label %deny\n";
        out << "deny:\n  call void @rx_proc_unlock()\n  ret i64 0\n";
        out << "stop:\n  %cp = getelementptr [8 x i64], ptr @rx.proc.status, i64 0, i64 %slot\n";
        out << "  store i64 2, ptr %sp, align 8\n  store i64 %status, ptr %cp, align 8\n";
        out << "  call void @rx_proc_unlock()\n  ret i64 1\n}\n\n";
        out << "define i64 @rx_proc_wait(i64 %id) {\n";
        out << "entry:\n  %pos = icmp sgt i64 %id, 0\n  %lim = icmp sle i64 %id, 8\n";
        out << "  %range = and i1 %pos, %lim\n  br i1 %range, label %lock, label %miss\n";
        out << "miss:\n  ret i64 -1\n";
        out << "lock:\n  call void @rx_proc_lock()\n  %slot = sub i64 %id, 1\n";
        out << "  %sp = getelementptr [8 x i64], ptr @rx.proc.state, i64 0, i64 %slot\n";
        out << "  %st = load i64, ptr %sp, align 8\n  %dead = icmp eq i64 %st, 2\n";
        out << "  br i1 %dead, label %done, label %living\n";
        out << "living:\n  %alive = icmp eq i64 %st, 1\n  br i1 %alive, label %run, label %gone\n";
        out << "run:\n  call void @rx_proc_unlock()\n  ret i64 -2\n";
        out << "gone:\n  call void @rx_proc_unlock()\n  ret i64 -1\n";
        out << "done:\n  %cp = getelementptr [8 x i64], ptr @rx.proc.status, i64 0, i64 %slot\n";
        out << "  %code = load i64, ptr %cp, align 8\n  call void @rx_proc_unlock()\n  ret i64 %code\n}\n\n";
        out << "define i64 @rx_cred_allow(i64 %user, i64 %right) {\n";
        out << "  %sys = icmp eq i64 %right, 1\n  %root = icmp eq i64 %user, 0\n";
        out << "  %sysok = and i1 %sys, %root\n";
        out << "  %write = icmp eq i64 %right, 2\n  %read = icmp eq i64 %right, 4\n";
        out << "  %nonneg = icmp sge i64 %user, 0\n  %below = icmp slt i64 %user, 2\n";
        out << "  %known = and i1 %nonneg, %below\n  %guest = icmp eq i64 %user, 2\n";
        out << "  %writer = and i1 %write, %known\n  %reader = or i1 %known, %guest\n";
        out << "  %readok = and i1 %read, %reader\n  %any = or i1 %sysok, %writer\n";
        out << "  %allow = or i1 %any, %readok\n  %bit = zext i1 %allow to i64\n  ret i64 %bit\n}\n\n";
    }

    if (ctx.usesPackage) {
        out << "@rx.pkg.active = internal global i64 0\n";
        out << "@rx.pkg.prev = internal global i64 0\n";
        out << "define i64 @rx_pkg(i64 %bytes) {\n";
        out << "entry:\n  %magicShift = lshr i64 %bytes, 48\n  %magic = and i64 %magicShift, 65535\n";
        out << "  %verShift = lshr i64 %bytes, 32\n  %version = and i64 %verShift, 255\n";
        out << "  %hmacShift = lshr i64 %bytes, 40\n  %hmac = and i64 %hmacShift, 255\n";
        out << "  %venShift = lshr i64 %bytes, 16\n  %vendor = and i64 %venShift, 65535\n";
        out << "  %device = and i64 %bytes, 65535\n  %badMagic = icmp ne i64 %magic, 21072\n";
        out << "  br i1 %badMagic, label %err, label %okmag\n";
        out << "err:\n  ret i64 -1\n";
        out << "okmag:\n  %rollback = icmp eq i64 %version, 0\n  br i1 %rollback, label %back, label %check\n";
        out << "back:\n  %prev = load i64, ptr @rx.pkg.prev, align 8\n";
        out << "  store i64 %prev, ptr @rx.pkg.active, align 8\n  store i64 0, ptr @rx.pkg.prev, align 8\n  ret i64 %prev\n";
        out << "check:\n  %x1 = xor i64 21072, %version\n  %x2 = xor i64 %x1, %vendor\n  %x3 = xor i64 %x2, %device\n";
        out << "  %expect = and i64 %x3, 255\n  %bad = icmp ne i64 %hmac, %expect\n";
        out << "  br i1 %bad, label %err, label %newer\n";
        out << "newer:\n  %active = load i64, ptr @rx.pkg.active, align 8\n";
        out << "  %empty = icmp eq i64 %active, 0\n  %gt = icmp sgt i64 %version, %active\n";
        out << "  %ok = or i1 %empty, %gt\n  br i1 %ok, label %store, label %err\n";
        out << "store:\n  store i64 %active, ptr @rx.pkg.prev, align 8\n  store i64 %version, ptr @rx.pkg.active, align 8\n";
        out << "  %high = shl i64 %active, 16\n  %packed = or i64 %high, %version\n  ret i64 %packed\n}\n\n";
        static const std::pair<int, int> kIntel[] = {
            {22802, 1}, {40304, 2}, {4110, 3}, {4307, 3}, {5618, 3}, {4297, 3}, {4347, 3}, {5490, 3},
            {10019, 4}, {17392, 4}, {2602, 5}, {752, 5}, {40288, 6}, {40239, 7}, {39443, 7}, {5586, 7},
            {39435, 8}, {10274, 8}, {6425, 9}, {40224, 10}, {6403, 10}, {40227, 10}
        };
        out << "define i64 @rx_vendor_match(i64 %catalog, i64 %vendor, i64 %device, i64 %class) {\n";
        out << "entry:\n  %c1 = icmp eq i64 %catalog, 1\n  br i1 %c1, label %intel, label %other\n";
        out << "intel:\n  %isIntel = icmp eq i64 %vendor, 32902\n  br i1 %isIntel, label %id0, label %none\n";
        for (std::size_t i = 0; i < sizeof(kIntel) / sizeof(kIntel[0]); ++i) {
            out << "id" << i << ":\n  %hit" << i << " = icmp eq i64 %device, " << kIntel[i].first << "\n";
            if (i + 1 < sizeof(kIntel) / sizeof(kIntel[0])) {
                out << "  br i1 %hit" << i << ", label %fam" << i << ", label %id" << (i + 1) << "\n";
            } else {
                out << "  br i1 %hit" << i << ", label %fam" << i << ", label %none\n";
            }
            out << "fam" << i << ":\n  ret i64 " << kIntel[i].second << "\n";
        }
        out << "other:\n  %c2 = icmp eq i64 %catalog, 2\n  br i1 %c2, label %amd, label %c3\n";
        out << "amd:\n  %a1 = icmp eq i64 %vendor, 4130\n  %a2 = icmp eq i64 %vendor, 4098\n";
        out << "  %amdHit = or i1 %a1, %a2\n  %amdBit = zext i1 %amdHit to i64\n  ret i64 %amdBit\n";
        out << "c3:\n  %c3b = icmp eq i64 %catalog, 3\n  br i1 %c3b, label %nvidia, label %c4\n";
        out << "nvidia:\n  %nv = icmp eq i64 %vendor, 4318\n  %nvBit = zext i1 %nv to i64\n  ret i64 %nvBit\n";
        out << "c4:\n  %c4b = icmp eq i64 %catalog, 4\n  br i1 %c4b, label %realtek, label %c5\n";
        out << "realtek:\n  %rt = icmp eq i64 %vendor, 4332\n  %rtBit = zext i1 %rt to i64\n  ret i64 %rtBit\n";
        out << "c5:\n  %c5b = icmp eq i64 %catalog, 5\n  br i1 %c5b, label %broadcom, label %c6\n";
        out << "broadcom:\n  %bc = icmp eq i64 %vendor, 5348\n  %bcBit = zext i1 %bc to i64\n  ret i64 %bcBit\n";
        out << "c6:\n  %c6b = icmp eq i64 %catalog, 6\n  br i1 %c6b, label %qualcomm, label %none\n";
        out << "qualcomm:\n  %qc = icmp eq i64 %vendor, 6091\n  %qcBit = zext i1 %qc to i64\n  ret i64 %qcBit\n";
        out << "none:\n  ret i64 0\n}\n\n";
        (void)0;
    }

    if (ctx.usesFs || ctx.usesDispatch) {
        out << "@rx.fs.lock = internal global i64 0\n";
        out << "@rx.fs.path = internal global [4 x i64] zeroinitializer\n";
        out << "@rx.fs.value = internal global [4 x i64] zeroinitializer\n";
        out << "@rx.fs.state = internal global [4 x i64] zeroinitializer\n";
        out << "define internal void @rx_fs_lock() {\n";
        out << "entry:\n  br label %spin\n";
        out << "spin:\n  %got = atomicrmw xchg ptr @rx.fs.lock, i64 1 seq_cst\n";
        out << "  %free = icmp eq i64 %got, 0\n  br i1 %free, label %out, label %spin\n";
        out << "out:\n  ret void\n}\n\n";
        out << "define internal void @rx_fs_unlock() {\n";
        out << "  store atomic i64 0, ptr @rx.fs.lock seq_cst, align 8\n  ret void\n}\n\n";
        out << "define i64 @rx_fs_lookup(i64 %path) {\n";
        out << "entry:\n  call void @rx_fs_lock()\n  br label %scan\n";
        out << "scan:\n  %i = phi i64 [ 0, %entry ], [ %next, %step ]\n";
        out << "  %done = icmp uge i64 %i, 4\n  br i1 %done, label %miss, label %check\n";
        out << "check:\n  %sp = getelementptr [4 x i64], ptr @rx.fs.state, i64 0, i64 %i\n";
        out << "  %st = load i64, ptr %sp, align 8\n  %pp = getelementptr [4 x i64], ptr @rx.fs.path, i64 0, i64 %i\n";
        out << "  %got = load i64, ptr %pp, align 8\n  %live = icmp eq i64 %st, 1\n";
        out << "  %same = icmp eq i64 %got, %path\n  %hit = and i1 %live, %same\n";
        out << "  br i1 %hit, label %yes, label %step\n";
        out << "step:\n  %next = add i64 %i, 1\n  br label %scan\n";
        out << "yes:\n  call void @rx_fs_unlock()\n  ret i64 1\n";
        out << "miss:\n  call void @rx_fs_unlock()\n  ret i64 -1\n}\n\n";
        out << "define i64 @rx_fs_create(i64 %path, i64 %value) {\n";
        out << "entry:\n  %bad = icmp sle i64 %path, 0\n  br i1 %bad, label %err, label %lock\n";
        out << "err:\n  ret i64 -1\n";
        out << "lock:\n  call void @rx_fs_lock()\n  br label %find\n";
        out << "find:\n  %i = phi i64 [ 0, %lock ], [ %inext, %fstep ]\n";
        out << "  %hole = phi i64 [ -1, %lock ], [ %hole2, %fstep ]\n";
        out << "  %done = icmp uge i64 %i, 4\n  br i1 %done, label %place, label %body\n";
        out << "body:\n  %sp = getelementptr [4 x i64], ptr @rx.fs.state, i64 0, i64 %i\n";
        out << "  %st = load i64, ptr %sp, align 8\n  %pp = getelementptr [4 x i64], ptr @rx.fs.path, i64 0, i64 %i\n";
        out << "  %got = load i64, ptr %pp, align 8\n  %live = icmp eq i64 %st, 1\n";
        out << "  %same = icmp eq i64 %got, %path\n  %hit = and i1 %live, %same\n";
        out << "  br i1 %hit, label %update, label %fstep\n";
        out << "fstep:\n  %empty = icmp eq i64 %st, 0\n  %need = icmp slt i64 %hole, 0\n";
        out << "  %take = and i1 %empty, %need\n  %hole2 = select i1 %take, i64 %i, i64 %hole\n";
        out << "  %inext = add i64 %i, 1\n  br label %find\n";
        out << "update:\n  %vp = getelementptr [4 x i64], ptr @rx.fs.value, i64 0, i64 %i\n";
        out << "  store i64 %value, ptr %vp, align 8\n  call void @rx_fs_unlock()\n  ret i64 1\n";
        out << "place:\n  %full = icmp slt i64 %hole, 0\n  br i1 %full, label %nofit, label %fill\n";
        out << "nofit:\n  call void @rx_fs_unlock()\n  ret i64 -1\n";
        out << "fill:\n  %hp = getelementptr [4 x i64], ptr @rx.fs.path, i64 0, i64 %hole\n";
        out << "  %hv = getelementptr [4 x i64], ptr @rx.fs.value, i64 0, i64 %hole\n";
        out << "  %hs = getelementptr [4 x i64], ptr @rx.fs.state, i64 0, i64 %hole\n";
        out << "  store i64 %path, ptr %hp, align 8\n  store i64 %value, ptr %hv, align 8\n";
        out << "  store i64 1, ptr %hs, align 8\n  call void @rx_fs_unlock()\n  ret i64 1\n}\n\n";
        out << "define i64 @rx_fs_read(i64 %path) {\n";
        out << "entry:\n  call void @rx_fs_lock()\n  br label %scan\n";
        out << "scan:\n  %i = phi i64 [ 0, %entry ], [ %next, %step ]\n";
        out << "  %done = icmp uge i64 %i, 4\n  br i1 %done, label %miss, label %check\n";
        out << "check:\n  %sp = getelementptr [4 x i64], ptr @rx.fs.state, i64 0, i64 %i\n";
        out << "  %st = load i64, ptr %sp, align 8\n  %pp = getelementptr [4 x i64], ptr @rx.fs.path, i64 0, i64 %i\n";
        out << "  %got = load i64, ptr %pp, align 8\n  %live = icmp eq i64 %st, 1\n";
        out << "  %same = icmp eq i64 %got, %path\n  %hit = and i1 %live, %same\n";
        out << "  br i1 %hit, label %take, label %step\n";
        out << "step:\n  %next = add i64 %i, 1\n  br label %scan\n";
        out << "take:\n  %vp = getelementptr [4 x i64], ptr @rx.fs.value, i64 0, i64 %i\n";
        out << "  %value = load i64, ptr %vp, align 8\n  call void @rx_fs_unlock()\n  ret i64 %value\n";
        out << "miss:\n  call void @rx_fs_unlock()\n  ret i64 -1\n}\n\n";
        out << "define i64 @rx_fs_write(i64 %path, i64 %value) {\n";
        out << "entry:\n  call void @rx_fs_lock()\n  br label %scan\n";
        out << "scan:\n  %i = phi i64 [ 0, %entry ], [ %next, %step ]\n";
        out << "  %done = icmp uge i64 %i, 4\n  br i1 %done, label %miss, label %check\n";
        out << "check:\n  %sp = getelementptr [4 x i64], ptr @rx.fs.state, i64 0, i64 %i\n";
        out << "  %st = load i64, ptr %sp, align 8\n  %pp = getelementptr [4 x i64], ptr @rx.fs.path, i64 0, i64 %i\n";
        out << "  %got = load i64, ptr %pp, align 8\n  %live = icmp eq i64 %st, 1\n";
        out << "  %same = icmp eq i64 %got, %path\n  %hit = and i1 %live, %same\n";
        out << "  br i1 %hit, label %store, label %step\n";
        out << "step:\n  %next = add i64 %i, 1\n  br label %scan\n";
        out << "store:\n  %vp = getelementptr [4 x i64], ptr @rx.fs.value, i64 0, i64 %i\n";
        out << "  store i64 %value, ptr %vp, align 8\n  call void @rx_fs_unlock()\n  ret i64 1\n";
        out << "miss:\n  call void @rx_fs_unlock()\n  ret i64 -1\n}\n\n";
        out << "define i64 @rx_fs_list(i64 %index) {\n";
        out << "entry:\n  %bad = icmp slt i64 %index, 0\n  br i1 %bad, label %none, label %lock\n";
        out << "none:\n  ret i64 0\n";
        out << "lock:\n  call void @rx_fs_lock()\n  %lim = icmp uge i64 %index, 4\n  br i1 %lim, label %empty, label %load\n";
        out << "load:\n  %sp = getelementptr [4 x i64], ptr @rx.fs.state, i64 0, i64 %index\n";
        out << "  %st = load i64, ptr %sp, align 8\n  %live = icmp eq i64 %st, 1\n";
        out << "  br i1 %live, label %path, label %empty\n";
        out << "path:\n  %pp = getelementptr [4 x i64], ptr @rx.fs.path, i64 0, i64 %index\n";
        out << "  %got = load i64, ptr %pp, align 8\n  call void @rx_fs_unlock()\n  ret i64 %got\n";
        out << "empty:\n  call void @rx_fs_unlock()\n  ret i64 0\n}\n\n";
    }

    if (ctx.usesDispatch) {
        out << "define i64 @rx_syscall_dispatch(i64 %caller, i64 %number, i64 %arg) {\n";
        out << "entry:\n  %echo = icmp eq i64 %number, 0\n  br i1 %echo, label %doecho, label %n1\n";
        out << "doecho:\n  %sum = add i64 %arg, 4919\n  ret i64 %sum\n";
        out << "n1:\n  %exit = icmp eq i64 %number, 1\n  br i1 %exit, label %dovalue, label %n2\n";
        out << "n2:\n  %log = icmp eq i64 %number, 2\n  br i1 %log, label %dovalue, label %n3\n";
        out << "n3:\n  %yield = icmp eq i64 %number, 3\n  br i1 %yield, label %doyield, label %n4\n";
        out << "doyield:\n  ret i64 0\n";
        out << "n4:\n  %port = icmp eq i64 %number, 4\n  br i1 %port, label %dovalue, label %n5\n";
        out << "dovalue:\n  ret i64 %arg\n";
        out << "n5:\n  %map = icmp eq i64 %number, 5\n  br i1 %map, label %domap, label %n6\n";
        out << "domap:\n  %low = icmp sge i64 %arg, 8388608\n  %high = icmp slt i64 %arg, 10485760\n";
        out << "  %user = and i1 %low, %high\n  br i1 %user, label %dovalue, label %err\n";
        out << "n6:\n  %file = icmp eq i64 %number, 6\n  br i1 %file, label %dofile, label %n7\n";
        out << "dofile:\n  %found = call i64 @rx_fs_lookup(i64 %arg)\n  ret i64 %found\n";
        out << "n7:\n  %draw = icmp eq i64 %number, 7\n  br i1 %draw, label %dodraw, label %err\n";
        out << "dodraw:\n  %server = icmp eq i64 %caller, 2\n  br i1 %server, label %dovalue, label %err\n";
        out << "err:\n  ret i64 -1\n}\n\n";
    }

    if (ctx.usesInput) {
        const std::size_t count = ctx.inputScript.size();
        int mouseX = 0;
        int mouseY = 0;
        RoboPlayedMouse(ctx.inputScript, mouseX, mouseY);
        out << "@rx.input.live = internal global [128 x [5 x i32]] zeroinitializer\n";
        out << "@rx.input.head = internal global i32 0\n";
        out << "@rx.input.tail = internal global i32 0\n";
        out << "@rx.input.mouseX = internal global i32 0\n";
        out << "@rx.input.mouseY = internal global i32 0\n";
        out << "@rx.input.clicks = internal global i32 0\n";
        out << "define i64 @rx_input_play() {\n";
        out << "  store i32 0, ptr @rx.input.head, align 4\n";
        out << "  store i32 " << count << ", ptr @rx.input.tail, align 4\n";
        for (std::size_t index = 0; index < count; ++index) {
            const RoboPlayedEvent& event = ctx.inputScript[index];
            const int fields[5] = { event.type, event.code, event.value, event.x, event.y };
            for (int field = 0; field < 5; ++field) {
                out << "  %p" << index << "_" << field
                    << " = getelementptr [128 x [5 x i32]], ptr @rx.input.live, i64 0, i64 " << index
                    << ", i64 " << field << "\n";
                out << "  store i32 " << fields[field] << ", ptr %p" << index << "_" << field << ", align 4\n";
            }
        }
        out << "  store i32 " << mouseX << ", ptr @rx.input.mouseX, align 4\n";
        out << "  store i32 " << mouseY << ", ptr @rx.input.mouseY, align 4\n";
        out << "  ret i64 " << count << "\n}\n\n";
        out << "define i64 @rx_input_count() {\n";
        out << "  %h = load i32, ptr @rx.input.head, align 4\n";
        out << "  %t = load i32, ptr @rx.input.tail, align 4\n";
        out << "  %n = sub i32 %t, %h\n  %z = zext i32 %n to i64\n  ret i64 %z\n}\n\n";
        out << "define i64 @rx_input_clicks() {\n";
        out << "  %c = load i32, ptr @rx.input.clicks, align 4\n  %z = zext i32 %c to i64\n  ret i64 %z\n}\n\n";
        out << "define i64 @rx_input_mouse_x() {\n";
        out << "  %v = load i32, ptr @rx.input.mouseX, align 4\n  %z = sext i32 %v to i64\n  ret i64 %z\n}\n\n";
        out << "define i64 @rx_input_mouse_y() {\n";
        out << "  %v = load i32, ptr @rx.input.mouseY, align 4\n  %z = sext i32 %v to i64\n  ret i64 %z\n}\n\n";
        out << "define void @rx_input_clear() {\n";
        out << "  store i32 0, ptr @rx.input.head, align 4\n";
        out << "  store i32 0, ptr @rx.input.tail, align 4\n";
        out << "  store i32 0, ptr @rx.input.mouseX, align 4\n";
        out << "  store i32 0, ptr @rx.input.mouseY, align 4\n";
        out << "  store i32 0, ptr @rx.input.clicks, align 4\n  ret void\n}\n\n";
        out << "define void @rx_input_set_mouse(i64 %x, i64 %y) {\n";
        out << "  %x32 = trunc i64 %x to i32\n  %y32 = trunc i64 %y to i32\n";
        out << "  store i32 %x32, ptr @rx.input.mouseX, align 4\n";
        out << "  store i32 %y32, ptr @rx.input.mouseY, align 4\n  ret void\n}\n\n";
        out << "define void @rx_input_add_click() {\n";
        out << "  %c = load i32, ptr @rx.input.clicks, align 4\n  %n = add i32 %c, 1\n";
        out << "  store i32 %n, ptr @rx.input.clicks, align 4\n  ret void\n}\n\n";
        out << "define i64 @rx_input_pop() {\n";
        out << "  %h = load i32, ptr @rx.input.head, align 4\n";
        out << "  %t = load i32, ptr @rx.input.tail, align 4\n";
        out << "  %empty = icmp uge i32 %h, %t\n";
        out << "  br i1 %empty, label %miss, label %hit\n";
        out << "miss:\n  ret i64 0\n";
        out << "hit:\n  %n = add i32 %h, 1\n  store i32 %n, ptr @rx.input.head, align 4\n  ret i64 1\n}\n\n";
        out << "define i64 @rx_input_field(i64 %i, i64 %f) {\n";
        out << "  %h = load i32, ptr @rx.input.head, align 4\n";
        out << "  %t = load i32, ptr @rx.input.tail, align 4\n";
        out << "  %i32 = trunc i64 %i to i32\n";
        out << "  %at = add i32 %h, %i32\n";
        out << "  %in = icmp ult i32 %at, %t\n";
        out << "  %fn = icmp ult i64 %f, 5\n";
        out << "  %ok = and i1 %in, %fn\n";
        out << "  br i1 %ok, label %hit, label %miss\n";
        out << "miss:\n  ret i64 0\n";
        out << "hit:\n  %slot = zext i32 %at to i64\n";
        out << "  %p = getelementptr [128 x [5 x i32]], ptr @rx.input.live, i64 0, i64 %slot, i64 %f\n";
        out << "  %v = load i32, ptr %p, align 4\n  %z = sext i32 %v to i64\n  ret i64 %z\n}\n\n";
        out << "define i64 @rx_input_push(i64 %type, i64 %code, i64 %value, i64 %x, i64 %y) {\n";
        out << "  %t = load i32, ptr @rx.input.tail, align 4\n";
        out << "  %full = icmp uge i32 %t, 128\n";
        out << "  br i1 %full, label %done, label %put\n";
        out << "put:\n  %slot = zext i32 %t to i64\n";
        out << "  %p0 = getelementptr [128 x [5 x i32]], ptr @rx.input.live, i64 0, i64 %slot, i64 0\n";
        out << "  %p1 = getelementptr [128 x [5 x i32]], ptr @rx.input.live, i64 0, i64 %slot, i64 1\n";
        out << "  %p2 = getelementptr [128 x [5 x i32]], ptr @rx.input.live, i64 0, i64 %slot, i64 2\n";
        out << "  %p3 = getelementptr [128 x [5 x i32]], ptr @rx.input.live, i64 0, i64 %slot, i64 3\n";
        out << "  %p4 = getelementptr [128 x [5 x i32]], ptr @rx.input.live, i64 0, i64 %slot, i64 4\n";
        out << "  %t32 = trunc i64 %type to i32\n  store i32 %t32, ptr %p0, align 4\n";
        out << "  %c32 = trunc i64 %code to i32\n  store i32 %c32, ptr %p1, align 4\n";
        out << "  %v32 = trunc i64 %value to i32\n  store i32 %v32, ptr %p2, align 4\n";
        out << "  %x32 = trunc i64 %x to i32\n  store i32 %x32, ptr %p3, align 4\n";
        out << "  %y32 = trunc i64 %y to i32\n  store i32 %y32, ptr %p4, align 4\n";
        out << "  %n = add i32 %t, 1\n  store i32 %n, ptr @rx.input.tail, align 4\n";
        out << "  br label %done\n";
        out << "done:\n  %h2 = load i32, ptr @rx.input.head, align 4\n";
        out << "  %t2 = load i32, ptr @rx.input.tail, align 4\n";
        out << "  %left = sub i32 %t2, %h2\n  %z = zext i32 %left to i64\n  ret i64 %z\n}\n\n";
    }

    if (ctx.usesScene && ctx.scenePixels.size() == 64000) {
        out << "@rx.scene.pix = internal constant [64000 x i32] [";
        for (std::size_t index = 0; index < ctx.scenePixels.size(); ++index) {
            if (index != 0) {
                out << ",";
            }
            if (index % 12 == 0) {
                out << "\n  ";
            }
            out << "i32 " << ctx.scenePixels[index];
        }
        out << "\n]\n";
        out << "define i64 @rx_scene_render(i64 %frame) {\n";
        out << "  %p = inttoptr i64 %frame to ptr\n";
        out << "  %w32 = load i32, ptr %p, align 4\n";
        out << "  %hp = getelementptr i8, ptr %p, i64 4\n";
        out << "  %h32 = load i32, ptr %hp, align 4\n";
        out << "  %okw = icmp eq i32 %w32, 320\n";
        out << "  %okh = icmp eq i32 %h32, 200\n";
        out << "  %ok = and i1 %okw, %okh\n";
        out << "  br i1 %ok, label %copy, label %miss\n";
        out << "miss:\n  ret i64 0\n";
        out << "copy:\n  br label %loop\n";
        out << "loop:\n  %i = phi i64 [ 0, %copy ], [ %next, %step ]\n";
        out << "  %done = icmp uge i64 %i, 64000\n";
        out << "  br i1 %done, label %copied, label %step\n";
        out << "step:\n  %src = getelementptr [64000 x i32], ptr @rx.scene.pix, i64 0, i64 %i\n";
        out << "  %color = load i32, ptr %src, align 4\n";
        out << "  %base = getelementptr i8, ptr %p, i64 8\n";
        out << "  %dst = getelementptr i32, ptr %base, i64 %i\n";
        out << "  store volatile i32 %color, ptr %dst, align 4\n";
        out << "  %next = add i64 %i, 1\n";
        out << "  br label %loop\n";
        out << "copied:\n  ret i64 1\n}\n\n";
    }

    if (!m_options.freestanding && !nativeTests.empty()) {
        out << "define i32 @main() #0 {\nentry:\n";
        for (const auto& testName : nativeTests) {
            out << "  call fastcc void @\"" << testName << "\"()\n";
        }
        if (mainFn && mainRet == NTy::Void) {
            out << "  call fastcc void @\"" << mainFn->mangledName << "\"()\n";
        }
        out << "  ret i32 0\n}\n\n";
    } else if (!m_options.freestanding && !m_options.omitMain && mainFn) {
    out << "define i32 @main() #0 {\nentry:\n";
    if (mainRet == NTy::Void) {
        out << "  call fastcc void @\"" << mainFn->mangledName << "\"()\n  ret i32 0\n";
    } else if (IsIntTy(mainRet)) {
        out << "  %r = call fastcc " << TyStr(mainRet) << " @\"" << mainFn->mangledName << "\"()\n";
        if (mainRet == NTy::I32) {
            out << "  ret i32 %r\n";
        } else if (mainRet == NTy::I1) {
            out << "  %c = zext i1 %r to i32\n  ret i32 %c\n";
        } else if (IntBits(mainRet) > 32) {
            out << "  %c = trunc " << TyStr(mainRet) << " %r to i32\n  ret i32 %c\n";
        } else {
            out << "  %c = sext " << TyStr(mainRet) << " %r to i32\n  ret i32 %c\n";
        }
    } else {
        out << "  %r = call " << TyStr(mainRet) << " @\"" << mainFn->mangledName << "\"()\n  ret i32 0\n";
    }
    out << "}\n\n";
    }
    if (m_options.freestanding) {
        const char* cpu = "x86-64";
        if (m_options.asmArch == "aarch64") cpu = "generic";
        else if (m_options.asmArch == "arm") cpu = "cortex-a15";
        else if (m_options.asmArch == "riscv64") cpu = "generic-rv64";
        out << "attributes #0 = { nounwind "
            << (m_options.asmArch == "x86_64" ? "\"no-red-zone\"=\"true\" " : "")
            << "\"target-cpu\"=\"" << cpu << "\" \"no-trapping-math\"=\"true\" }\n";
    } else if (m_options.profile == BuildProfile::Release) {
        out << "attributes #0 = { nounwind \"target-cpu\"=\"skylake\" \"target-features\"=\"+64bit,+adx,+aes,+avx,+avx2,+bmi,+bmi2,+f16c,+fma,+fsgsbase,+fxsr,+invpcid,+lzcnt,+mmx,+movbe,+pclmul,+popcnt,+prfchw,+rdrnd,+rdseed,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave,+xsavec,+xsaveopt,+xsaves\" \"no-trapping-math\"=\"true\" \"unsafe-fp-math\"=\"true\" \"approx-func-fp-math\"=\"true\" \"stack-protector-buffer-size\"=\"8\" hot }\n";
    } else {
        out << "attributes #0 = { nounwind \"no-trapping-math\"=\"true\" \"stack-protector-buffer-size\"=\"8\" }\n";
    }

    if (!ok) return std::nullopt;
    return out.str();
}

} // namespace Robix::CodeGen

