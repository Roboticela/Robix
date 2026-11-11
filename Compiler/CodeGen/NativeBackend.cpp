#include "NativeBackend.hpp"

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
                    "lgdt", "lidt", "ljmp", "ljmpl", "rdmsr", "wrmsr", "invlpg",
                    "int", "iret", "iretq", "push", "pop", "ret", "jmp", "call",
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
    std::set<std::string> timerIsrs;
    std::string preemptA;
    std::string preemptB;
    bool usesPreempt = false;
    bool usesSyscall = false;
    bool usesPages = false;
    bool usesSwitch = false;
    bool usesFrame = false;
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
            } else if (inst.textImm == "CPU.ReadCR3" || inst.textImm == "CPU.ReadRSP" ||
                       inst.textImm == "CPU.WriteCR3" || inst.textImm == "CPU.ExchangeRSP" ||
                       inst.textImm == "CPU.SavedRSP" || inst.textImm == "CPU.LGDT" ||
                       inst.textImm == "CPU.LIDT" || inst.textImm == "CPU.BootInfo" ||
                       inst.textImm == "CPU.Function" || inst.textImm == "CPU.Handler" ||
                       inst.textImm == "CPU.Run" || inst.textImm == "Timer.Arm" ||
                       inst.textImm == "Timer.Preempt" || inst.textImm == "System.Call") {
                if (m_ctx.options.asmArch != "x86_64") {
                    Err("N9010", inst.textImm + " exists on x86_64 only.", inst.location);
                    m_vals[id] = Val{ "0", NTy::I64 };
                    break;
                }
                if (inst.textImm == "CPU.ReadCR3" || inst.textImm == "CPU.ReadRSP") {
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
                } else if (inst.textImm == "CPU.Function" || inst.textImm == "CPU.Handler") {
                    const std::string sym = inst.textImm == "CPU.Handler"
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
        if (inst.textImm == "Memory.Pages") ctx.usesPages = true;
        if (inst.textImm.rfind("Frame.", 0) == 0) ctx.usesFrame = true;
        if (inst.textImm == "CPU.ExchangeRSP" || inst.textImm == "CPU.SavedRSP") ctx.usesSwitch = true;
        if (inst.textImm == "System.Call") ctx.usesSyscall = true;
        if (inst.textImm == "CPU.Function" || inst.textImm == "CPU.Handler" || inst.textImm == "CPU.Run" ||
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
                    if (inst.textImm == "CPU.Handler" || inst.textImm == "Timer.Arm") ctx.isrFns.insert(inst.secondaryText);
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
    if (!m_options.freestanding || needs("@llvm.trap") || ctx.usesPages) {
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
        const std::string eoi = ctx.timerIsrs.count(isr)
                                    ? "movb $$32, %al\noutb %al, $$32\n"
                                    : "";
        const std::string text =
            "pushq %rax\npushq %rcx\npushq %rdx\npushq %rsi\npushq %rdi\n"
            "pushq %r8\npushq %r9\npushq %r10\npushq %r11\n" +
            eoi +
            "call rx_fn_" + isr + "\n"
            "popq %r11\npopq %r10\npopq %r9\npopq %r8\n"
            "popq %rdi\npopq %rsi\npopq %rdx\npopq %rcx\npopq %rax\n"
            "iretq";
        out << "define dso_local void @rx_isr_" << isr << "() naked nounwind {\n";
        out << "  call void asm sideeffect \"" << EscapeLLVMString(text) << "\", \"\"()\n";
        out << "  unreachable\n}\n\n";
    }

    if (ctx.usesFrame) {
        if (m_options.freestanding) {
            out << "declare void @llvm.trap()\n";
            out << "%RoboFrame = type { i32, i32, [64000 x i32] }\n";
            out << "@rx.frame = internal global %RoboFrame zeroinitializer, align 16\n";
            out << "define i64 @rx_frame_create(i64 %w, i64 %h) {\n";
            out << "  %okw = icmp ule i64 %w, 320\n";
            out << "  %okh = icmp ule i64 %h, 200\n";
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
            out << "  ret i64 %ret\n}\n\n";
        }
        out << "define void @rx_frame_fill(i64 %frame, i64 %x, i64 %y, i64 %rw, i64 %rh, i64 %color) {\n";
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
        out << "  store i32 %c32, ptr %slot, align 4\n";
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
            out << "define i64 @rx_frame_present(i64 %frame) noinline {\nentry:\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 4, i16 462)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 0, i16 463)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 1, i16 462)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 320, i16 463)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 2, i16 462)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 200, i16 463)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 3, i16 462)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 32, i16 463)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 4, i16 462)\n";
            out << "  call void asm sideeffect \"outw $0, $1\", \"{ax},{dx}\"(i16 65, i16 463)\n";
            out << "  %front = call i64 @rx_display_bar()\n";
            out << "  %p = inttoptr i64 %frame to ptr\n";
            out << "  %fw32 = load i32, ptr %p, align 4\n";
            out << "  %fw = zext i32 %fw32 to i64\n";
            out << "  %hptr = getelementptr i8, ptr %p, i64 4\n";
            out << "  %fh32 = load i32, ptr %hptr, align 4\n";
            out << "  %fh = zext i32 %fh32 to i64\n";
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
        } else {
            out << "define i64 @rx_frame_present(i64 %frame) {\n";
            out << "  ret i64 0\n}\n\n";
        }
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

