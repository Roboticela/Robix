#include "NativeBackend.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
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

// Maps a Robix type to a native scalar type. Returns false for unsupported types.
bool MapType(const Frontend::TypeNode& t, NTy& out, std::string& why) {
    if (t.isPointer || t.isNullable || !t.typeArgs.empty()) {
        why = "pointer, nullable and generic types";
        return false;
    }
    const std::string& b = t.baseName;
    if (b.empty() || b == "Void")                          { out = NTy::Void; return true; }
    if (b == "Bool")                                       { out = NTy::I1;   return true; }
    if (b == "Byte" || b == "Int8")                        { out = NTy::I8;   return true; }
    if (b == "Int16")                                      { out = NTy::I16;  return true; }
    if (b == "Int32" || b == "Char")                       { out = NTy::I32;  return true; }
    if (b == "Int" || b == "Int64")                        { out = NTy::I64;  return true; }
    if (b == "Float32")                                    { out = NTy::F32;  return true; }
    if (b == "Float" || b == "Float64" || b == "Double")   { out = NTy::F64;  return true; }
    if (b == "String")                                     { out = NTy::Ptr;  return true; }
    why = "type '" + b + "' (unsigned integers, classes, structs and collections)";
    return false;
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
};

struct ModuleCtx {
    Frontend::DiagnosticBag& diags;
    std::unordered_map<std::string, const IR::IRFunction*> byName;
    std::vector<std::string> stringLiterals;
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
    bool m_ok  = true;
    NTy m_ret  = NTy::Void;

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
                m_body << "  " << t << " = " << (v.ty == NTy::I1 ? "zext " : "sext ")
                       << TyStr(v.ty) << " " << v.repr << " to " << TyStr(to) << "\n";
            } else {
                m_body << "  " << t << " = trunc " << TyStr(v.ty) << " " << v.repr << " to " << TyStr(to) << "\n";
            }
        } else if (IsIntTy(v.ty) && IsFloatTy(to)) {
            m_body << "  " << t << " = " << (v.ty == NTy::I1 ? "uitofp " : "sitofp ")
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
        return Val{ t, to };
    }

    std::string Zero(NTy t) const {
        return IsFloatTy(t) ? "0.0" : (t == NTy::Ptr ? "null" : "0");
    }

    NTy EnsureVar(const std::string& name, NTy ty) {
        auto it = m_varTy.find(name);
        if (it != m_varTy.end()) return it->second;
        m_varTy[name] = ty;
        m_allocas << "  " << SlotName(name) << " = alloca " << TyStr(ty) << "\n";
        return ty;
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
            m_vals[id] = Val{ std::to_string(inst.intImm), NTy::I64 };
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
            else if (inst.textImm == "ToInt")   v = Coerce(v, NTy::I64, inst.location);
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
            m_body << "  " << vname << " = load " << TyStr(it->second) << ", ptr " << SlotName(inst.textImm) << "\n";
            m_vals[id] = Val{ vname, it->second };
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
            m_body << "  store " << TyStr(slotTy) << " " << cv.repr << ", ptr " << SlotName(inst.textImm) << "\n";
            break;
        }

        case IROpcode::Add:
        case IROpcode::Sub:
        case IROpcode::Mul:
        case IROpcode::Div:
        case IROpcode::Mod: {
            Val a = Operand(inst, 0);
            Val b = Operand(inst, 1);
            if (a.ty == NTy::Ptr || b.ty == NTy::Ptr || a.ty == NTy::Void || b.ty == NTy::Void) {
                Err("N9001", "Arithmetic on strings or void values is not yet supported by the native backend.", inst.location);
                m_vals[id] = Val{ "0", NTy::I64 };
                break;
            }
            const NTy t = ArithPromote(a.ty, b.ty);
            a = Coerce(a, t, inst.location);
            b = Coerce(b, t, inst.location);
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
                op = (inst.opcode == IROpcode::Add) ? "add" : (inst.opcode == IROpcode::Sub) ? "sub"
                   : (inst.opcode == IROpcode::Mul) ? "mul" : (inst.opcode == IROpcode::Div) ? "sdiv" : "srem";
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
            if (a.ty == NTy::Ptr || b.ty == NTy::Ptr || a.ty == NTy::Void || b.ty == NTy::Void) {
                Err("N9001", "String comparison is not yet supported by the native backend.", inst.location);
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
                const char* cc = (inst.opcode == IROpcode::Eq) ? "eq" : (inst.opcode == IROpcode::Ne) ? "ne"
                               : (inst.opcode == IROpcode::Lt) ? "slt" : (inst.opcode == IROpcode::Le) ? "sle"
                               : (inst.opcode == IROpcode::Gt) ? "sgt" : "sge";
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
            if (ret == NTy::Void) {
                m_body << "  call fastcc void @\"" << cit->second->mangledName << "\"(" << args << ")\n";
                m_vals[id] = Val{ "0", NTy::Void };
            } else {
                m_body << "  " << vname << " = call fastcc " << TyStr(ret) << " @\"" << cit->second->mangledName
                       << "\"(" << args << ")\n";
                m_vals[id] = Val{ vname, ret };
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
            Val c = Coerce(Operand(inst, 0), NTy::I1, inst.location);
            m_body << "  br i1 " << c.repr << ", label %" << inst.textImm << ", label %" << inst.secondaryText << "\n";
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

        case IROpcode::CallVirtual:
        case IROpcode::CallOSIntrinsic:
        case IROpcode::AllocHeap:
        case IROpcode::AllocStack:
        case IROpcode::GetField:
        case IROpcode::SetField:
            Err("N9001",
                "Objects, virtual calls, fields and OS intrinsics are not yet supported by the native backend ('" +
                    std::string(IR::IROpcodeToString(inst.opcode)) + "').",
                inst.location);
            break;
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

    std::ostringstream paramStores;
    for (std::size_t i = 0; i < m_fn.parameters.size(); ++i) {
        const std::string& pname = m_fn.parameters[i].name;
        EnsureVar(pname, ptys[i]);
        paramStores << "  store " << TyStr(ptys[i]) << " %a" << i << ", ptr " << SlotName(pname) << "\n";
    }

    for (std::size_t bi = 0; bi < m_fn.blocks.size(); ++bi) {
        const auto& bb = m_fn.blocks[bi];
        if (bi != 0) {
            m_body << bb.label << ":\n";
        }
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

    m_out << "define fastcc " << TyStr(ret) << " @\"" << m_fn.mangledName << "\"(";
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
    if (m_ctx.options.profile == BuildProfile::Release && m_fn.name != "Main" && !isRecursive) {
        m_out << ") alwaysinline nounwind #0 {\nentry:\n";
    } else {
        m_out << ") nounwind #0 {\nentry:\n";
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
        if (!fn.enclosingClassName.empty()) {
            m_diagnostics.ReportError(
                "N9001",
                "Class method '" + fn.name + "' cannot be compiled natively: classes are not yet supported by the native backend.",
                fn.location,
                "Use top-level functions with scalar types for native builds.");
            ok = false;
            continue;
        }
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
    out << "declare void @rbx_print_str(ptr)\n\n";

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
        out << "attributes #0 = { nounwind \"no-trapping-math\"=\"true\" \"unsafe-fp-math\"=\"true\" \"approx-func-fp-math\"=\"true\" \"stack-protector-buffer-size\"=\"8\" hot \"tune-cpu\"=\"native\" }\n";
    } else {
        out << "attributes #0 = { nounwind \"no-trapping-math\"=\"true\" \"stack-protector-buffer-size\"=\"8\" }\n";
    }

    return out.str();
}

} // namespace Robix::CodeGen

