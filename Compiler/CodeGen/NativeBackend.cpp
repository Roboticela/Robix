#include "NativeBackend.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
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
                    m_body << "  " << vname << " = call ptr @rbx_strcat(ptr " << a.repr << ", ptr " << b.repr << ")\n";
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
            const bool ext = cit->second->isExtern;
            const std::string callee = ext ? ("@" + cit->second->mangledName)
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
                m_body << "  call void @rbx_print_str(ptr " << a.repr << ")\n";
            } else if (a.ty == NTy::I1) {
                const std::string t = NewTmp();
                m_body << "  " << t << " = zext i1 " << a.repr << " to i32\n";
                m_body << "  call void @rbx_print_bool(i32 " << t << ")\n";
            } else if (IsIntTy(a.ty)) {
                Val w = Coerce(a, NTy::I64, inst.location);
                m_body << "  call void @rbx_print_i64(i64 " << w.repr << ")\n";
            } else if (IsFloatTy(a.ty)) {
                Val w = Coerce(a, NTy::F64, inst.location);
                m_body << "  call void @rbx_print_f64(double " << w.repr << ")\n";
            } else {
                Err("N9007", "Cannot Print a void value.", inst.location);
            }
            break;
        }

        case IROpcode::InlineASM:
            m_body << "  call void asm sideeffect \"" << EscapeLLVMString(inst.textImm) << "\", \"\"()\n";
            break;

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

        case IROpcode::AssertVal:
            Err("N9001", "Assertions are only valid inside Test blocks (use 'robix test'); they cannot be compiled natively.",
                inst.location);
            break;

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
    if (m_fn.isExtern) {
        m_out << "declare " << TyStr(ret) << " @" << m_fn.mangledName << "(";
        for (std::size_t i = 0; i < ptys.size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << TyStr(ptys[i]);
        }
        m_out << ")\n\n";
        return true;
    }
    IndexDefs();

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
            EmitInstruction(inst);
            terminated = IR::IsTerminatorOpcode(inst.opcode);
        }
        if (!terminated) {
            m_body << "  unreachable\n";
        }
    }

    if (!m_ok) return false;

    m_out << "define internal fastcc " << TyStr(ret) << " @\"" << m_fn.mangledName << "\"(";
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
    if (m_ctx.options.profile == BuildProfile::Release && !isRecursive) {
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

    for (const auto& fn : mod.functions) {
        if (fn.isTestBlock) continue;
        FnEmitter emitter(fn, ctx, functions);
        if (!emitter.Emit()) {
            ok = false;
        }
        if (fn.name == "Main" && fn.parameters.empty()) {
            mainFn = &fn;
        }
    }

    NTy mainRet = NTy::Void;
    if (!mainFn) {
        m_diagnostics.ReportError(
            "N9002",
            "No entry point: native executables require a parameterless 'Function Main()'.",
            Frontend::SourceLocation{},
            "Add 'public Function Main() -> Int { ... }'.");
        ok = false;
    } else {
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
        << BuildProfileToString(m_options.profile) << ")\n";
    out << "target datalayout = \"" << GetLLVMDataLayout(m_options.target) << "\"\n";
    out << "target triple = \"" << GetLLVMTargetTriple(m_options.target) << "\"\n\n";
    out << "declare void @rbx_print_i64(i64)\n";
    out << "declare void @rbx_print_f64(double)\n";
    out << "declare void @rbx_print_bool(i32)\n";
    out << "declare void @rbx_print_str(ptr)\n";
    out << "declare ptr @malloc(i64)\n";
    out << "declare void @free(ptr)\n";
    out << "declare i64 @strlen(ptr)\n";
    out << "declare i32 @strcmp(ptr, ptr)\n";
    out << "declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)\n";
    out << "declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)\n";
    out << "declare void @llvm.trap()\n\n";
    for (const auto& vt : mod.vtables) {
        if (vt.slots.empty()) continue;
        out << "@\"vt." << vt.typeName << "\" = private constant [" << vt.slots.size() << " x ptr] [";
        for (std::size_t i = 0; i < vt.slots.size(); ++i) {
            if (i > 0) out << ", ";
            auto it = ctx.byName.find(vt.slots[i]);
            if (it == ctx.byName.end()) {
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
    out << "define internal ptr @rbx_strcat(ptr %a, ptr %b) {\n";
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

    for (std::size_t i = 0; i < ctx.stringLiterals.size(); ++i) {
        const std::string& s = ctx.stringLiterals[i];
        out << "@.str." << i << " = private unnamed_addr constant [" << (s.size() + 1) << " x i8] c\""
            << EscapeLLVMString(s) << "\\00\"\n";
    }
    if (!ctx.stringLiterals.empty()) out << "\n";

    out << functions.str();

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
    if (m_options.profile == BuildProfile::Release) {
        out << "attributes #0 = { nounwind \"target-cpu\"=\"skylake\" \"target-features\"=\"+64bit,+adx,+aes,+avx,+avx2,+bmi,+bmi2,+f16c,+fma,+fsgsbase,+fxsr,+invpcid,+lzcnt,+mmx,+movbe,+pclmul,+popcnt,+prfchw,+rdrnd,+rdseed,+sahf,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+x87,+xsave,+xsavec,+xsaveopt,+xsaves\" \"no-trapping-math\"=\"true\" \"unsafe-fp-math\"=\"true\" \"approx-func-fp-math\"=\"true\" \"stack-protector-buffer-size\"=\"8\" hot }\n";
    } else {
        out << "attributes #0 = { nounwind \"no-trapping-math\"=\"true\" \"stack-protector-buffer-size\"=\"8\" }\n";
    }

    if (!ok) return std::nullopt;
    return out.str();
}

} // namespace Robix::CodeGen

