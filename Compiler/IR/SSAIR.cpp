#include "SSAIR.hpp"
#include "../Types/SpecValidator.hpp"

#include <algorithm>
#include <cstdlib>
#include <functional>
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
    std::string body;
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        body = std::string(text.substr(1, text.size() - 2));
    } else {
        body = std::string(text);
    }
    std::string out;
    out.reserve(body.size());
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (body[i] == '\\' && i + 1 < body.size()) {
            const char next = body[++i];
            if (next == 'n') out.push_back('\n');
            else if (next == 't') out.push_back('\t');
            else out.push_back(next);
        } else {
            out.push_back(body[i]);
        }
    }
    return out;
}

Frontend::TypeNode MakePrim(const std::string& name) {
    Frontend::TypeNode t;
    t.baseName = name;
    return t;
}

int PointeeBytes(const Frontend::TypeNode& ptr) {
    if (ptr.typeArgs.empty()) return 8;
    const std::string& b = ptr.typeArgs[0].baseName;
    if (b == "Byte" || b == "Int8" || b == "UInt8" || b == "Bool") return 1;
    if (b == "Int16" || b == "UInt16") return 2;
    if (b == "Int32" || b == "UInt32" || b == "Char" || b == "Float32") return 4;
    return 8;
}

bool PointeeSigned(const Frontend::TypeNode& ptr) {
    if (ptr.typeArgs.empty()) return true;
    const std::string& b = ptr.typeArgs[0].baseName;
    if (b == "Byte" || b == "UInt8" || b == "UInt16" || b == "UInt32" || b == "UInt64" || b == "UInt" || b == "Bool") {
        return false;
    }
    return true;
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
        case IROpcode::BitAnd:          return "bit.and";
        case IROpcode::BitOr:           return "bit.or";
        case IROpcode::BitXor:          return "bit.xor";
        case IROpcode::Shl:             return "shl";
        case IROpcode::Shr:             return "shr";
        case IROpcode::Neg:             return "neg";
        case IROpcode::Not:             return "not";
        case IROpcode::BitNot:          return "bit.not";
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
        case IROpcode::LoadIndex:       return "load.index";
        case IROpcode::StoreIndex:      return "store.index";
        case IROpcode::AddrOf:          return "addr";
        case IROpcode::PtrLoad:         return "ptr.load";
        case IROpcode::PtrStore:        return "ptr.store";
        case IROpcode::PtrOffset:       return "ptr.offset";
        case IROpcode::SliceCopy:       return "slice.copy";
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

void SSAIRBuilder::NoteSig(const Frontend::DeclNode& decl, const std::string& enclosing) {
    if (decl.kind != Frontend::DeclKind::Function && decl.kind != Frontend::DeclKind::Constructor) {
        return;
    }
    FnSig sig;
    sig.decl       = &decl;
    sig.sourceName = decl.name;
    sig.arity      = static_cast<int>(decl.parameters.size());
    sig.isExtern   = decl.isExtern;
    sig.irName     = enclosing.empty() ? decl.name : (enclosing + "." + decl.name);

    std::string paramCodes;
    for (const auto& p : decl.parameters) {
        Spec::PrimitiveKind pk{};
        if (Spec::TryResolvePrimitiveName(p.type.baseName, pk)) {
            paramCodes += Spec::GetPrimitiveLayout(pk).mangleCode;
        } else {
            paramCodes += "v";
        }
    }
    std::string retCode = "v";
    Spec::PrimitiveKind retPk{};
    if (Spec::TryResolvePrimitiveName(decl.returnType.baseName, retPk)) {
        retCode = std::string(Spec::GetPrimitiveLayout(retPk).mangleCode);
    }
    sig.mangled = sig.isExtern
        ? decl.name
        : Spec::MangleRobixSymbol(m_package, enclosing, decl.name, paramCodes, retCode);
    m_sigs.push_back(std::move(sig));
}

void SSAIRBuilder::CollectSigs(const Frontend::CompilationUnitAST& unit) {
    m_sigs.clear();
    m_package = unit.packageName.empty() ? "Default" : unit.packageName;

    for (const auto& declPtr : unit.declarations) {
        if (!declPtr) continue;
        const auto& decl = *declPtr;
        if (decl.kind == Frontend::DeclKind::Function) {
            NoteSig(decl, "");
        } else if (decl.kind == Frontend::DeclKind::Class || decl.kind == Frontend::DeclKind::Struct) {
            for (const auto& memberPtr : decl.members) {
                if (memberPtr) NoteSig(*memberPtr, decl.name);
            }
        }
    }

    std::unordered_map<std::string, int> counts;
    for (const auto& sig : m_sigs) {
        counts[sig.irName] += 1;
    }
    for (auto& sig : m_sigs) {
        if (counts[sig.irName] > 1) {
            sig.irName += "/" + std::to_string(sig.arity);
        }
    }
}

const SSAIRBuilder::FnSig* SSAIRBuilder::FindSig(const Frontend::DeclNode* decl) const {
    for (const auto& sig : m_sigs) {
        if (sig.decl == decl) return &sig;
    }
    return nullptr;
}

std::string SSAIRBuilder::ResolveCallable(const std::string& name, int arity, const Frontend::SourceLocation& loc) {
    const FnSig* found = nullptr;
    int matches = 0;
    const std::string overload = name + "/" + std::to_string(arity);
    for (const auto& sig : m_sigs) {
        if (sig.sourceName == name && sig.arity == arity && (sig.irName == name || sig.irName == overload)) {
            found = &sig;
            ++matches;
        }
    }
    if (matches > 1) {
        m_diagnostics.ReportError(
            "I4012",
            "Call to '" + name + "' is ambiguous for " + std::to_string(arity) + " argument(s).",
            loc,
            "Give the overloads different names or different parameter lists.");
    }
    return (matches == 1 && found) ? found->irName : name;
}

bool SSAIRBuilder::IsRefType(const Frontend::TypeNode& type) const {
    if (type.isPointer) return true;
    const std::string& b = type.baseName;
    if (b == "Pointer" || b == "Array" || b == "Slice" || b == "String" ||
        b == "Result" || b == "Option" || b == "Tuple") {
        return true;
    }
    if (const auto* cls = m_sema.FindClassOrType(b)) {
        return cls->kind == Frontend::DeclKind::Struct || cls->kind == Frontend::DeclKind::Class ||
               cls->kind == Frontend::DeclKind::Interface || cls->kind == Frontend::DeclKind::Trait;
    }
    return false;
}

std::string SSAIRBuilder::SlotTypeName(const Frontend::TypeNode& type) const {
    if (IsRefType(type)) return "Pointer";
    if (type.baseName.empty()) return "Int";
    return type.baseName;
}

int SSAIRBuilder::FieldSlot(const std::string& typeName, const std::string& field) const {
    std::string lookup = typeName;
    const auto dollar = lookup.find('$');
    if (dollar != std::string::npos) lookup.resize(dollar);
    if (const auto* cls = m_sema.FindClassOrType(lookup)) {
        std::vector<std::string> names;
        names.reserve(cls->fields.size());
        for (const auto& entry : cls->fields) {
            names.push_back(entry.first);
        }
        std::sort(names.begin(), names.end());
        const int header = cls->kind == Frontend::DeclKind::Class ? 1 : 0;
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (names[i] == field) return static_cast<int>(i) + header;
        }
    }
    if (field == "tag" || field == "t0") return 0;
    if (field == "value" || field == "t1") return 1;
    return 0;
}

bool SSAIRBuilder::IsClassName(const std::string& name) const {
    std::string lookup = name;
    const auto dollar = lookup.find('$');
    if (dollar != std::string::npos) lookup.resize(dollar);
    const auto* cls = m_sema.FindClassOrType(lookup);
    return cls != nullptr && cls->kind == Frontend::DeclKind::Class;
}

Frontend::TypeNode SSAIRBuilder::SubstType(Frontend::TypeNode type) const {
    if (type.typeArgs.empty()) {
        for (const auto& [name, repl] : m_subst) {
            if (type.baseName == name) return repl;
        }
        return type;
    }
    for (auto& arg : type.typeArgs) {
        arg = SubstType(arg);
    }
    return type;
}

std::string SSAIRBuilder::TypeKey(const Frontend::TypeNode& type) const {
    if (type.typeArgs.empty()) return type.baseName;
    std::string key = type.baseName;
    for (const auto& arg : type.typeArgs) {
        key.push_back('$');
        key += TypeKey(arg);
    }
    return key;
}

std::string SSAIRBuilder::ClassChain(const std::string& irName) const {
    std::string lookup = irName;
    const auto dollar = lookup.find('$');
    if (dollar != std::string::npos) lookup.resize(dollar);
    std::string chain = irName;
    const auto* cls = m_sema.FindClassOrType(lookup);
    for (int guard = 0; cls != nullptr && !cls->baseClassName.empty() && guard < 16; ++guard) {
        chain.push_back('@');
        chain += cls->baseClassName;
        cls = m_sema.FindClassOrType(cls->baseClassName);
    }
    return chain;
}

void SSAIRBuilder::NoteGenericUse(const Frontend::TypeNode& type) {
    if (type.typeArgs.empty()) return;
    auto it = m_classDecls.find(type.baseName);
    if (it == m_classDecls.end() || it->second == nullptr || it->second->genericParams.empty()) return;
    if (it->second->genericParams.size() != type.typeArgs.size()) return;
    const std::string key = TypeKey(type);
    if (!m_genericSeen.insert(key).second) return;
    GenericInst inst;
    inst.decl   = it->second;
    inst.irName = key;
    inst.args   = type.typeArgs;
    m_genericQueue.push_back(std::move(inst));
}

std::vector<std::string> SSAIRBuilder::InterfacePrefix() const {
    std::vector<std::string> names;
    for (const auto& [typeName, decl] : m_classDecls) {
        (void)decl;
        const auto* info = m_sema.FindClassOrType(typeName);
        if (!info) continue;
        if (info->kind != Frontend::DeclKind::Interface && info->kind != Frontend::DeclKind::Trait) continue;
        for (const auto& [name, method] : info->methods) {
            if (!method.isStatic && !name.empty()) names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

std::vector<std::string> SSAIRBuilder::MethodOrder(const Frontend::SemanticClassInfo& cls) const {
    std::vector<std::string> order;
    if (!cls.baseClassName.empty()) {
        if (const auto* base = m_sema.FindClassOrType(cls.baseClassName)) {
            order = MethodOrder(*base);
        }
    }
    const std::vector<std::string> prefix = InterfacePrefix();
    if (order.empty()) order.assign(prefix.size(), std::string{});
    for (std::size_t i = 0; i < prefix.size() && i < order.size(); ++i) {
        if (order[i].empty() && cls.methods.find(prefix[i]) != cls.methods.end()) {
            order[i] = prefix[i];
        }
    }

    std::vector<std::string> own;
    for (const auto& [name, method] : cls.methods) {
        if (method.isStatic || name == cls.name || name.empty() || name[0] == '~') continue;
        own.push_back(name);
    }
    std::sort(own.begin(), own.end());
    for (const auto& name : own) {
        if (std::find(order.begin(), order.end(), name) == order.end()) {
            order.push_back(name);
        }
    }
    return order;
}

std::string SSAIRBuilder::DefiningClass(const Frontend::SemanticClassInfo& cls, const std::string& method) const {
    if (cls.methods.find(method) != cls.methods.end()) return cls.name;
    if (!cls.baseClassName.empty()) {
        if (const auto* base = m_sema.FindClassOrType(cls.baseClassName)) {
            return DefiningClass(*base, method);
        }
    }
    return cls.name;
}

int SSAIRBuilder::VirtualSlot(const std::string& typeName, const std::string& method) const {
    const auto* cls = m_sema.FindClassOrType(typeName);
    if (!cls) return 0;
    auto indexOf = [&](const std::vector<std::string>& order) {
        for (std::size_t i = 0; i < order.size(); ++i) {
            if (order[i] == method) return static_cast<int>(i);
        }
        return 0;
    };
    if (cls->kind == Frontend::DeclKind::Interface || cls->kind == Frontend::DeclKind::Trait) {
        return indexOf(InterfacePrefix());
    }
    return indexOf(MethodOrder(*cls));
}

void SSAIRBuilder::RecordVTable(const std::string& irName, const std::string& lookupName) {
    if (!m_module) return;
    const auto* cls = m_sema.FindClassOrType(lookupName);
    if (!cls || cls->kind != Frontend::DeclKind::Class) return;
    IRVTable vt;
    vt.typeName = irName;
    const bool instantiated = irName != lookupName;
    for (const auto& method : MethodOrder(*cls)) {
        if (method.empty()) {
            vt.slots.push_back("");
            continue;
        }
        if (instantiated) {
            vt.slots.push_back(irName + "." + method);
        } else {
            vt.slots.push_back(DefiningClass(*cls, method) + "." + method);
        }
    }
    m_module->vtables.push_back(std::move(vt));
}

void SSAIRBuilder::EmitScopeCleanups(std::size_t scopes, bool clear) {
    if (!m_currentFn || m_scopes.empty() || scopes == 0 || CurrentBlockHasTerminator()) return;
    const std::size_t count = std::min(scopes, m_scopes.size());
    const std::size_t start = m_scopes.size() - count;
    for (std::size_t s = m_scopes.size(); s-- > start; ) {
        // Copy first. A defer body is a nested block and pushes another scope.
        const std::vector<LiveObj> objs = m_scopes[s];
        for (auto it = objs.rbegin(); it != objs.rend(); ++it) {
            if (it->deferBody != nullptr) {
                LowerStmt(*it->deferBody);
                continue;
            }
            if (it->varName.empty()) continue;
            Frontend::TypeNode ty;
            ty.baseName  = "Pointer";
            ty.isPointer = true;
            IRInstruction load;
            load.opcode  = IROpcode::LoadVar;
            load.dest    = IRValue{ m_currentFn->AllocateSSAId(), ty };
            load.textImm = it->varName;
            const std::uint32_t obj = load.dest.ssaId;
            Emit(std::move(load));
            if (!it->dtorIrName.empty()) {
                IRInstruction call;
                call.opcode     = IROpcode::CallDirect;
                call.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Void") };
                call.textImm    = it->dtorIrName;
                call.operandIds = { obj };
                Emit(std::move(call));
            }
            IRInstruction freeInst;
            freeInst.opcode     = IROpcode::CallOSIntrinsic;
            freeInst.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Void") };
            freeInst.textImm    = "Memory.Free";
            freeInst.operandIds = { obj };
            Emit(std::move(freeInst));
        }
        if (clear && s < m_scopes.size()) m_scopes[s].clear();
    }
}

void SSAIRBuilder::LowerClassBody(const Frontend::DeclNode& decl,
                                  const std::string& irName,
                                  const std::vector<Frontend::TypeNode>& args) {
    const std::string prevClass = m_activeIrClass;
    const auto prevSubst = m_subst;
    m_activeIrClass = (irName == decl.name) ? std::string{} : irName;
    m_subst.clear();
    for (std::size_t i = 0; i < decl.genericParams.size() && i < args.size(); ++i) {
        m_subst.emplace_back(decl.genericParams[i], args[i]);
    }
    for (const auto& memberPtr : decl.members) {
        if (!memberPtr) continue;
        if (memberPtr->kind == Frontend::DeclKind::Function ||
            memberPtr->kind == Frontend::DeclKind::Constructor ||
            memberPtr->kind == Frontend::DeclKind::Destructor) {
            LowerFunctionOrMethod(*memberPtr, decl.name, *m_module);
        }
    }
    if (decl.kind == Frontend::DeclKind::Class) {
        RecordVTable(irName, decl.name);
    }
    m_activeIrClass = prevClass;
    m_subst = std::move(prevSubst);
}

IRValue SSAIRBuilder::EmitAggregate(const std::string& kind, std::int64_t slots, Frontend::TypeNode type) {
    m_currentFn->usesHeapOrGC = true;
    if (type.baseName.empty()) type.baseName = kind;
    type.isPointer = true;
    IRInstruction alloc;
    alloc.opcode   = IROpcode::AllocHeap;
    alloc.dest     = IRValue{ m_currentFn->AllocateSSAId(), type };
    alloc.textImm  = kind;
    alloc.intImm   = slots < 1 ? 1 : slots;
    IRValue out    = alloc.dest;
    Emit(std::move(alloc));
    return out;
}

namespace {

bool IsPowerOfTwo(std::int64_t value) {
    return value > 0 && (value & (value - 1)) == 0;
}

std::int64_t AlignUp(std::int64_t value, std::int64_t align) {
    if (align <= 1) return value;
    const std::int64_t mask = align - 1;
    return (value + mask) & ~mask;
}

bool AttributeInt(const Frontend::DeclNode& decl, const std::string& name, std::int64_t& out) {
    for (const auto& attr : decl.attributes) {
        if (attr.name == name && !attr.args.empty()) {
            out = ParseIntLiteral(attr.args[0]);
            return true;
        }
    }
    return false;
}

bool HasAttribute(const Frontend::DeclNode& decl, const std::string& name) {
    for (const auto& attr : decl.attributes) {
        if (attr.name == name) return true;
    }
    return false;
}

bool PrimitiveLayout(const std::string& name, std::int64_t& size, std::int64_t& align) {
    Spec::PrimitiveKind kind{};
    if (!Spec::TryResolvePrimitiveName(name, kind)) return false;
    const Spec::PrimitiveTypeLayout& layout = Spec::GetPrimitiveLayout(kind);
    size = static_cast<std::int64_t>(layout.sizeBytes64);
    align = static_cast<std::int64_t>(layout.alignment64);
    if (align < 1) align = 1;
    return size >= 0;
}

} // namespace

SSAIRBuilder::TypeLayout SSAIRBuilder::LayoutOf(const std::string& name) {
    const auto cached = m_layouts.find(name);
    if (cached != m_layouts.end()) return cached->second;

    TypeLayout layout;
    std::int64_t primSize = 0;
    std::int64_t primAlign = 1;
    if (PrimitiveLayout(name, primSize, primAlign)) {
        layout.size = primSize;
        layout.align = primAlign;
        layout.ok = true;
        m_layouts[name] = layout;
        return layout;
    }
    if (name == "Pointer") {
        layout.size = 8;
        layout.align = 8;
        layout.ok = true;
        m_layouts[name] = layout;
        return layout;
    }

    const auto found = m_classDecls.find(name);
    if (found == m_classDecls.end() || found->second == nullptr ||
        found->second->kind != Frontend::DeclKind::Struct) {
        m_diagnostics.ReportError(
            "L1011",
            "Memory.SizeOf cannot see a struct or primitive named '" + name + "'.",
            Frontend::SourceLocation{},
            "Declare the struct before asking for its size.");
        m_layouts[name] = layout;
        return layout;
    }
    if (!m_layingOut.insert(name).second) {
        m_diagnostics.ReportError(
            "L1011",
            "Struct '" + name + "' contains itself.",
            found->second->location,
            "Break the cycle with a pointer field.");
        return layout;
    }

    const Frontend::DeclNode& decl = *found->second;
    const bool packed = HasAttribute(decl, "Packed");
    std::int64_t forced = 0;
    const bool hasAlign = AttributeInt(decl, "Align", forced);
    if (hasAlign && !IsPowerOfTwo(forced)) {
        m_diagnostics.ReportError(
            "L1009",
            "@Align on '" + name + "' must be a power of two.",
            decl.location,
            "Use 1, 2, 4, 8, 16, or another power of two.");
    }
    std::int64_t cursor = 0;
    std::int64_t structAlign = packed ? 1 : 1;
    if (hasAlign && IsPowerOfTwo(forced)) structAlign = forced;

    for (const auto& member : decl.members) {
        if (!member || member->kind != Frontend::DeclKind::Field) continue;
        TypeLayout fieldLayout = LayoutOf(member->fieldType.baseName);
        std::int64_t fieldAlign = packed ? 1 : fieldLayout.align;
        std::int64_t fieldForced = 0;
        if (AttributeInt(*member, "Align", fieldForced)) {
            if (!IsPowerOfTwo(fieldForced)) {
                m_diagnostics.ReportError(
                    "L1009",
                    "@Align on '" + member->name + "' must be a power of two.",
                    member->location,
                    "Use 1, 2, 4, 8, 16, or another power of two.");
            } else if (fieldForced > fieldAlign) {
                fieldAlign = fieldForced;
            }
        }
        if (fieldAlign < 1) fieldAlign = 1;
        const std::int64_t count = member->fieldType.arrayLength > 0 ? member->fieldType.arrayLength : 1;
        const std::int64_t fieldSize = fieldLayout.size * count;
        cursor = AlignUp(cursor, fieldAlign);
        layout.fields[member->name] = cursor;
        cursor += fieldSize;
        if (fieldAlign > structAlign) structAlign = fieldAlign;
    }
    if (structAlign < 1) structAlign = 1;
    layout.size = AlignUp(cursor, structAlign);
    layout.align = structAlign;
    layout.ok = true;
    std::int64_t expected = 0;
    if (AttributeInt(decl, "Size", expected) && expected != layout.size) {
        m_diagnostics.ReportError(
            "L1008",
            "Struct '" + name + "' is " + std::to_string(layout.size) +
                " bytes, and @Size asked for " + std::to_string(expected) + ".",
            decl.location,
            "Update @Size or the fields so the packed layout matches.");
        layout.ok = false;
    }
    m_layingOut.erase(name);
    m_layouts[name] = layout;
    return layout;
}

void SSAIRBuilder::NoteLayouts() {
    for (const auto& entry : m_classDecls) {
        if (entry.second != nullptr && entry.second->kind == Frontend::DeclKind::Struct) {
            (void)LayoutOf(entry.first);
        }
    }
}

std::optional<std::int64_t> SSAIRBuilder::EvalConst(const Frontend::ExprNode& expr) {
    if (expr.kind == Frontend::ExprKind::Literal && expr.token.kind == Frontend::TokenKind::IntegerLiteral) {
        return ParseIntLiteral(expr.text);
    }
    if (expr.kind == Frontend::ExprKind::Identifier) {
        const auto known = m_consts.find(expr.text);
        if (known != m_consts.end()) return known->second;
        return std::nullopt;
    }
    if (expr.kind == Frontend::ExprKind::Call && expr.left &&
        expr.left->kind == Frontend::ExprKind::MemberAccess && expr.left->left &&
        expr.left->left->kind == Frontend::ExprKind::Identifier && expr.left->left->text == "Memory") {
        if (expr.left->text == "SizeOf" && !expr.arguments.empty() && expr.arguments[0] &&
            expr.arguments[0]->kind == Frontend::ExprKind::Identifier) {
            const TypeLayout layout = LayoutOf(expr.arguments[0]->text);
            if (!layout.ok && layout.size == 0) return std::nullopt;
            return layout.size;
        }
        if (expr.left->text == "OffsetOf" && expr.arguments.size() >= 2 && expr.arguments[0] && expr.arguments[1] &&
            expr.arguments[0]->kind == Frontend::ExprKind::Identifier &&
            expr.arguments[1]->kind == Frontend::ExprKind::Identifier) {
            const TypeLayout layout = LayoutOf(expr.arguments[0]->text);
            const auto field = layout.fields.find(expr.arguments[1]->text);
            if (field == layout.fields.end()) return std::nullopt;
            return field->second;
        }
    }
    return std::nullopt;
}

void SSAIRBuilder::CollectConsts(const Frontend::CompilationUnitAST& unit) {
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto& declPtr : unit.declarations) {
            if (!declPtr || declPtr->kind != Frontend::DeclKind::TopLevelVar) continue;
            if (declPtr->fieldMutability != Frontend::VarMutability::Constant) continue;
            if (!declPtr->fieldInitializer) continue;
            if (m_consts.find(declPtr->name) != m_consts.end()) continue;
            if (const auto value = EvalConst(*declPtr->fieldInitializer)) {
                m_consts[declPtr->name] = *value;
            }
        }
    }
}

IRModule SSAIRBuilder::LowerCompilationUnit(const Frontend::CompilationUnitAST& unit) {
    IRModule mod;
    mod.moduleName  = unit.fileName;
    mod.packageName = unit.packageName.empty() ? "Default" : unit.packageName;
    m_module = &mod;
    m_classDecls.clear();
    m_layouts.clear();
    m_layingOut.clear();
    m_consts.clear();
    m_genericQueue.clear();
    m_genericSeen.clear();
    m_scopes.clear();
    CollectSigs(unit);

    for (const auto& declPtr : unit.declarations) {
        if (!declPtr) continue;
        const auto& decl = *declPtr;
        if (decl.kind == Frontend::DeclKind::Class || decl.kind == Frontend::DeclKind::Struct ||
            decl.kind == Frontend::DeclKind::Interface || decl.kind == Frontend::DeclKind::Trait) {
            m_classDecls[decl.name] = &decl;
        }
    }
    NoteLayouts();
    CollectConsts(unit);

    for (const auto& declPtr : unit.declarations) {
        if (!declPtr) continue;
        const auto& decl = *declPtr;

        if (decl.kind == Frontend::DeclKind::Function) {
            if (!decl.genericParams.empty()) continue;
            LowerFunctionOrMethod(decl, "", mod);
        } else if (decl.kind == Frontend::DeclKind::Class || decl.kind == Frontend::DeclKind::Struct) {
            if (!decl.genericParams.empty()) continue;
            LowerClassBody(decl, decl.name, {});
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

    for (std::size_t i = 0; i < m_genericQueue.size(); ++i) {
        const GenericInst& inst = m_genericQueue[i];
        if (inst.decl) LowerClassBody(*inst.decl, inst.irName, inst.args);
    }

    for (const auto& fn : mod.functions) {
        if (fn.usesHeapOrGC || fn.usesReflection) {
            mod.requiresFullRuntime = true;
            break;
        }
    }

    m_module = nullptr;
    return mod;
}

void SSAIRBuilder::LowerFunctionOrMethod(const Frontend::DeclNode& decl,
                                         const std::string& enclosingClass,
                                         IRModule& outModule) {
    const FnSig* sig = FindSig(&decl);
    if (!decl.body && !(sig && sig->isExtern)) {
        return; // Skip bodyless interface/abstract declarations
    }

    const std::string irClass = m_activeIrClass.empty() ? enclosingClass : m_activeIrClass;
    IRFunction fn;
    fn.name               = irClass.empty() ? decl.name : (irClass + "." + decl.name);
    fn.enclosingClassName = irClass;
    fn.returnType         = SubstType(decl.returnType.IsEmpty() ? MakePrim("Void") : decl.returnType);
    fn.location           = decl.location;
    if (IsRefType(fn.returnType)) fn.returnType.isPointer = true;

    (void)m_diagnostics;
    std::string paramCodes;
    for (const auto& p : decl.parameters) {
        Frontend::TypeNode pty = SubstType(p.type.IsEmpty() ? MakePrim("Int") : p.type);
        if (IsRefType(pty)) pty.isPointer = true;
        fn.parameters.push_back(Frontend::FunctionParamInfo{
            p.name,
            pty,
            p.defaultValue != nullptr,
            p.isMutable,
            p.isVariadic
        });
        Spec::PrimitiveKind pk{};
        if (Spec::TryResolvePrimitiveName(pty.baseName, pk)) {
            paramCodes += Spec::GetPrimitiveLayout(pk).mangleCode;
        } else {
            paramCodes += "v";
        }
    }

    if (!irClass.empty() && !decl.isStatic) {
        Frontend::TypeNode selfTy;
        selfTy.baseName  = irClass;
        selfTy.isPointer = true;
        fn.parameters.insert(fn.parameters.begin(), Frontend::FunctionParamInfo{
            "this", selfTy, false, false, false
        });
    }

    std::string retCode = "v";
    Spec::PrimitiveKind retPk{};
    if (Spec::TryResolvePrimitiveName(fn.returnType.baseName, retPk)) {
        retCode = std::string(Spec::GetPrimitiveLayout(retPk).mangleCode);
    }

    fn.mangledName = Spec::MangleRobixSymbol(
        outModule.packageName,
        irClass,
        decl.name,
        paramCodes,
        retCode);
    if (sig && m_activeIrClass.empty()) {
        fn.name        = sig->irName;
        fn.mangledName = sig->mangled;
        fn.isExtern    = sig->isExtern;
        if (sig->isExtern && decl.body) fn.mangledName = decl.name;
    }

    for (const auto& attr : decl.attributes) {
        if (attr.name == "RealTime") {
            fn.isRealTime = true;
        }
    }

    if (!decl.body) {
        outModule.functions.push_back(std::move(fn));
        return;
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
    fn.mangledName = "_RX1_Test_" + std::to_string(outModule.functions.size() + 1);
    fn.returnType  = MakePrim("Void");
    fn.isTestBlock = true;
    fn.runOnNative = testDecl.runOnNative;
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
            m_scopes.emplace_back();
            for (const auto& s : stmt.statements) {
                if (s) LowerStmt(*s);
            }
            if (!CurrentBlockHasTerminator()) {
                EmitScopeCleanups(1, true);
            }
            if (!m_scopes.empty()) m_scopes.pop_back();
            break;
        }

        case Frontend::StmtKind::VarDecl: {
            IRValue initVal{};
            if (stmt.initializer) {
                initVal = LowerExpr(*stmt.initializer);
            } else if (IsClassName(stmt.declaredType.baseName)) {
                int fields = 0;
                if (const auto* cls = m_sema.FindClassOrType(stmt.declaredType.baseName)) {
                    fields = static_cast<int>(cls->fields.size());
                }
                Frontend::TypeNode objTy = stmt.declaredType;
                objTy.isPointer = true;
                IRInstruction alloc;
                alloc.opcode  = IROpcode::AllocHeap;
                alloc.dest    = IRValue{ m_currentFn->AllocateSSAId(), objTy };
                alloc.textImm = ClassChain(TypeKey(stmt.declaredType));
                alloc.boolImm = true;
                alloc.intImm  = fields + 1;
                alloc.location = stmt.location;
                initVal = alloc.dest;
                m_currentFn->usesHeapOrGC = true;
                Emit(std::move(alloc));
            } else if (IsRefType(stmt.declaredType)) {
                int slots = 1;
                if (const auto* cls = m_sema.FindClassOrType(stmt.declaredType.baseName)) {
                    slots = std::max(1, static_cast<int>(cls->fields.size()));
                }
                initVal = EmitAggregate(stmt.declaredType.baseName, slots, stmt.declaredType);
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
            store.secondaryText = SlotTypeName(stmt.declaredType.IsEmpty() ? initVal.type : stmt.declaredType);
            store.operandIds  = { initVal.ssaId };
            store.location    = stmt.location;
            Emit(std::move(store));
            if (!m_scopes.empty() && IsClassName(stmt.declaredType.baseName)) {
                LiveObj live;
                live.varName = stmt.varName;
                if (const auto* cls = m_sema.FindClassOrType(stmt.declaredType.baseName)) {
                    if (cls->hasCustomDestructor) {
                        live.dtorIrName = TypeKey(stmt.declaredType) + ".~" + stmt.declaredType.baseName;
                    }
                }
                m_scopes.back().push_back(std::move(live));
            }
            NoteGenericUse(stmt.declaredType);
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
            } else if (stmt.condition) {
                IRValue coll = LowerExpr(*stmt.condition);
                IRInstruction lenInst;
                lenInst.opcode     = IROpcode::GetField;
                lenInst.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                lenInst.textImm    = "len";
                lenInst.intImm     = -1;
                lenInst.operandIds = { coll.ssaId };
                lenInst.location   = stmt.location;
                IRValue lenVal     = lenInst.dest;
                Emit(std::move(lenInst));

                const std::string indexName = "__index_" + stmt.loopVar1 + "_" + std::to_string(stmt.location.line);
                IRInstruction zero;
                zero.opcode   = IROpcode::ConstInt;
                zero.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                zero.intImm   = 0;
                zero.location = stmt.location;
                IRValue zeroVal = zero.dest;
                Emit(std::move(zero));
                IRInstruction storeInit;
                storeInit.opcode     = IROpcode::StoreVar;
                storeInit.textImm    = indexName;
                storeInit.operandIds = { zeroVal.ssaId };
                storeInit.location   = stmt.location;
                Emit(std::move(storeInit));

                const std::size_t condIdx = CreateBasicBlock("in_cond");
                const std::size_t bodyIdx = CreateBasicBlock("in_body");
                const std::size_t stepIdx = CreateBasicBlock("in_step");
                const std::size_t exitIdx = CreateBasicBlock("in_exit");
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
                loadIter.textImm  = indexName;
                loadIter.location = stmt.location;
                IRValue curIter   = loadIter.dest;
                Emit(std::move(loadIter));
                IRInstruction cmp;
                cmp.opcode     = IROpcode::Lt;
                cmp.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Bool") };
                cmp.operandIds = { curIter.ssaId, lenVal.ssaId };
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
                IRInstruction loaded;
                loaded.opcode     = IROpcode::LoadIndex;
                loaded.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                loaded.operandIds = { coll.ssaId, curIter.ssaId };
                loaded.location   = stmt.location;
                IRValue elem      = loaded.dest;
                Emit(std::move(loaded));
                IRInstruction storeElem;
                storeElem.opcode     = IROpcode::StoreVar;
                storeElem.textImm    = stmt.loopVar1;
                storeElem.operandIds = { elem.ssaId };
                storeElem.location   = stmt.location;
                Emit(std::move(storeElem));
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
                loadCur.textImm  = indexName;
                loadCur.location = stmt.location;
                IRValue curV     = loadCur.dest;
                Emit(std::move(loadCur));
                IRInstruction one;
                one.opcode   = IROpcode::ConstInt;
                one.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                one.intImm   = 1;
                one.location = stmt.location;
                IRValue oneVal = one.dest;
                Emit(std::move(one));
                IRInstruction nextAdd;
                nextAdd.opcode     = IROpcode::Add;
                nextAdd.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                nextAdd.operandIds = { curV.ssaId, oneVal.ssaId };
                nextAdd.location   = stmt.location;
                IRValue nextV      = nextAdd.dest;
                Emit(std::move(nextAdd));
                IRInstruction storeNext;
                storeNext.opcode     = IROpcode::StoreVar;
                storeNext.textImm    = indexName;
                storeNext.operandIds = { nextV.ssaId };
                storeNext.location   = stmt.location;
                Emit(std::move(storeNext));
                IRInstruction backJmp;
                backJmp.opcode   = IROpcode::Jump;
                backJmp.textImm  = condLabel;
                backJmp.location = stmt.location;
                Emit(std::move(backJmp));
                m_currentBlockIdx = exitIdx;
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

        case Frontend::StmtKind::MatchStmt: {
            if (!stmt.condition || !m_currentFn) break;
            IRValue scrutinee = LowerExpr(*stmt.condition);
            IRInstruction tagInst;
            tagInst.opcode     = IROpcode::GetField;
            tagInst.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
            tagInst.textImm    = "tag";
            tagInst.intImm     = 0;
            tagInst.operandIds = { scrutinee.ssaId };
            tagInst.location   = stmt.location;
            IRValue tagVal     = tagInst.dest;
            Emit(std::move(tagInst));
            IRInstruction valueInst;
            valueInst.opcode     = IROpcode::GetField;
            valueInst.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
            valueInst.textImm    = "value";
            valueInst.intImm     = 1;
            valueInst.operandIds = { scrutinee.ssaId };
            valueInst.location   = stmt.location;
            IRValue valueVal     = valueInst.dest;
            Emit(std::move(valueInst));

            const std::size_t mergeIdx = CreateBasicBlock("match_merge");
            const std::string mergeLabel = m_currentFn->blocks[mergeIdx].label;
            for (std::size_t armIndex = 0; armIndex < stmt.matchArms.size(); ++armIndex) {
                const auto& arm = stmt.matchArms[armIndex];
                const std::size_t bodyIdx = CreateBasicBlock("match_arm");
                const bool last = armIndex + 1 == stmt.matchArms.size();
                const std::size_t nextIdx = last ? mergeIdx : CreateBasicBlock("match_next");
                const std::string bodyLabel = m_currentFn->blocks[bodyIdx].label;
                const std::string nextLabel = m_currentFn->blocks[nextIdx].label;
                const bool isDefault = arm.patternName == "_" || arm.patternName == "default";
                if (!isDefault && arm.patternName != "Success" && arm.patternName != "Error" &&
                    arm.patternName != "Some" && arm.patternName != "None") {
                    m_diagnostics.ReportError(
                        "I4010",
                        "Match pattern '" + arm.patternName + "' is not supported.",
                        arm.location,
                        "Use Success, Error, Some, None, or _.");
                }
                if (isDefault) {
                    IRInstruction jmp;
                    jmp.opcode   = IROpcode::Jump;
                    jmp.textImm  = bodyLabel;
                    jmp.location = arm.location;
                    Emit(std::move(jmp));
                } else {
                    const std::int64_t expected =
                        (arm.patternName == "Success" || arm.patternName == "Some") ? 1 : 0;
                    IRInstruction expect;
                    expect.opcode   = IROpcode::ConstInt;
                    expect.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                    expect.intImm   = expected;
                    expect.location = arm.location;
                    IRValue expectVal = expect.dest;
                    Emit(std::move(expect));
                    IRInstruction eq;
                    eq.opcode     = IROpcode::Eq;
                    eq.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Bool") };
                    eq.operandIds = { tagVal.ssaId, expectVal.ssaId };
                    eq.location   = arm.location;
                    IRValue eqVal = eq.dest;
                    Emit(std::move(eq));
                    IRInstruction brArm;
                    brArm.opcode        = IROpcode::BranchCond;
                    brArm.operandIds    = { eqVal.ssaId };
                    brArm.textImm       = bodyLabel;
                    brArm.secondaryText = nextLabel;
                    brArm.location      = arm.location;
                    Emit(std::move(brArm));
                }
                m_currentBlockIdx = bodyIdx;
                if (!arm.bindingVar.empty()) {
                    IRInstruction storeBind;
                    storeBind.opcode     = IROpcode::StoreVar;
                    storeBind.textImm    = arm.bindingVar;
                    storeBind.operandIds = { valueVal.ssaId };
                    storeBind.location   = arm.location;
                    Emit(std::move(storeBind));
                }
                if (arm.bodyStmt) LowerStmt(*arm.bodyStmt);
                else if (arm.bodyExpr) (void)LowerExpr(*arm.bodyExpr);
                if (!CurrentBlockHasTerminator()) {
                    IRInstruction toMerge;
                    toMerge.opcode   = IROpcode::Jump;
                    toMerge.textImm  = mergeLabel;
                    toMerge.location = arm.location;
                    Emit(std::move(toMerge));
                }
                m_currentBlockIdx = nextIdx;
            }
            m_currentBlockIdx = mergeIdx;
            break;
        }

        case Frontend::StmtKind::TryCatchStmt: {
            const std::size_t catchIdx = CreateBasicBlock("catch");
            const std::size_t contIdx = CreateBasicBlock("try_cont");
            const std::size_t finallyIdx = stmt.finallyBranch ? CreateBasicBlock("finally") : contIdx;
            const std::string previousLabel = m_catchLabel;
            const std::string previousVar = m_catchVar;
            m_catchLabel = m_currentFn->blocks[catchIdx].label;
            m_catchVar = stmt.catchVar;
            if (stmt.thenBranch) LowerStmt(*stmt.thenBranch);
            if (!CurrentBlockHasTerminator()) {
                IRInstruction toNext;
                toNext.opcode   = IROpcode::Jump;
                toNext.textImm  = m_currentFn->blocks[finallyIdx].label;
                toNext.location = stmt.location;
                Emit(std::move(toNext));
            }
            m_catchLabel = previousLabel;
            m_catchVar = previousVar;
            m_currentBlockIdx = catchIdx;
            if (stmt.catchBranch) LowerStmt(*stmt.catchBranch);
            if (!CurrentBlockHasTerminator()) {
                IRInstruction toNext;
                toNext.opcode   = IROpcode::Jump;
                toNext.textImm  = m_currentFn->blocks[finallyIdx].label;
                toNext.location = stmt.location;
                Emit(std::move(toNext));
            }
            if (stmt.finallyBranch) {
                m_currentBlockIdx = finallyIdx;
                LowerStmt(*stmt.finallyBranch);
                if (!CurrentBlockHasTerminator()) {
                    IRInstruction toCont;
                    toCont.opcode   = IROpcode::Jump;
                    toCont.textImm  = m_currentFn->blocks[contIdx].label;
                    toCont.location = stmt.location;
                    Emit(std::move(toCont));
                }
            }
            m_currentBlockIdx = contIdx;
            break;
        }

        case Frontend::StmtKind::ThrowStmt: {
            if (m_catchLabel.empty()) {
                m_diagnostics.ReportError(
                    "I4010",
                    "throw is outside try.",
                    stmt.location,
                    "Put throw inside a try block in the same function.");
                break;
            }
            if (stmt.condition && !m_catchVar.empty()) {
                IRValue thrown = LowerExpr(*stmt.condition);
                IRInstruction storeThrown;
                storeThrown.opcode     = IROpcode::StoreVar;
                storeThrown.textImm    = m_catchVar;
                storeThrown.operandIds = { thrown.ssaId };
                storeThrown.location   = stmt.location;
                Emit(std::move(storeThrown));
            }
            IRInstruction jmp;
            jmp.opcode   = IROpcode::Jump;
            jmp.textImm  = m_catchLabel;
            jmp.location = stmt.location;
            Emit(std::move(jmp));
            break;
        }

        case Frontend::StmtKind::ReturnStmt: {
            if (stmt.condition) {
                IRValue retVal = LowerExpr(*stmt.condition);
                EmitScopeCleanups(m_scopes.size(), false);
                IRInstruction ret;
                ret.opcode     = IROpcode::ReturnVal;
                ret.operandIds = { retVal.ssaId };
                ret.location   = stmt.location;
                Emit(std::move(ret));
            } else {
                EmitScopeCleanups(m_scopes.size(), false);
                IRInstruction ret;
                ret.opcode   = IROpcode::ReturnVoid;
                ret.location = stmt.location;
                Emit(std::move(ret));
            }
            break;
        }

        case Frontend::StmtKind::DeferStmt: {
            if (m_scopes.empty()) m_scopes.emplace_back();
            if (stmt.thenBranch) {
                LiveObj live;
                live.deferBody = stmt.thenBranch.get();
                m_scopes.back().push_back(std::move(live));
            }
            break;
        }

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
            if (stmt.thenBranch) LowerStmt(*stmt.thenBranch);
            break;
        }

        case Frontend::StmtKind::UnsafeStmt: {
            if (stmt.thenBranch) LowerStmt(*stmt.thenBranch);
            break;
        }

        case Frontend::StmtKind::ASMStmt: {
            IRInstruction asmInst;
            asmInst.opcode        = IROpcode::InlineASM;
            asmInst.textImm       = stmt.asmInstruction;
            asmInst.secondaryText = stmt.asmOutput + "\n" + stmt.asmClobber;
            asmInst.location      = stmt.location;
            for (const auto& input : stmt.asmInputs) {
                if (input) {
                    asmInst.operandIds.push_back(LowerExpr(*input).ssaId);
                }
            }
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

IRValue SSAIRBuilder::OutlineLambda(const Frontend::ExprNode& call, const Frontend::ExprNode& lambda,
                                    const std::vector<std::uint32_t>& argIds) {
    if (!m_module || !m_currentFn) return IRValue{};
    std::unordered_set<std::string> bound(lambda.lambdaParams.begin(), lambda.lambdaParams.end());
    std::vector<std::string> captures;
    std::unordered_set<std::string> seen;
    const auto remember = [&](const std::string& name) {
        if (name.empty() || bound.count(name) != 0 || seen.count(name) != 0) return;
        for (const auto& sig : m_sigs) {
            if (sig.sourceName == name) return;
        }
        if (name == "Print" || name == "Success" || name == "Error" || name == "Some" || name == "None" ||
            name == "this") {
            return;
        }
        seen.insert(name);
        captures.push_back(name);
    };
    const auto walkExpr = [&](const auto& self, const Frontend::ExprNode& expr) -> void {
        if (expr.kind == Frontend::ExprKind::Identifier) remember(expr.text);
        if (expr.kind == Frontend::ExprKind::Call && expr.left &&
            expr.left->kind == Frontend::ExprKind::Identifier) {
            for (const auto& arg : expr.arguments) {
                if (arg) self(self, *arg);
            }
            return;
        }
        if (expr.left) self(self, *expr.left);
        if (expr.right) self(self, *expr.right);
        for (const auto& arg : expr.arguments) {
            if (arg) self(self, *arg);
        }
    };
    const auto walkStmt = [&](const auto& self, const Frontend::StmtNode& stmt) -> void {
        if (stmt.condition) walkExpr(walkExpr, *stmt.condition);
        if (stmt.initializer) walkExpr(walkExpr, *stmt.initializer);
        if (stmt.stepExpr) walkExpr(walkExpr, *stmt.stepExpr);
        if (stmt.thenBranch) self(self, *stmt.thenBranch);
        if (stmt.elseBranch) self(self, *stmt.elseBranch);
        if (stmt.catchBranch) self(self, *stmt.catchBranch);
        if (stmt.finallyBranch) self(self, *stmt.finallyBranch);
        for (const auto& child : stmt.statements) {
            if (child) self(self, *child);
        }
    };
    if (lambda.left) walkExpr(walkExpr, *lambda.left);
    if (lambda.lambdaBody) walkStmt(walkStmt, *lambda.lambdaBody);

    const std::size_t callerIndex = static_cast<std::size_t>(m_currentFn - m_module->functions.data());
    const int savedCounter = m_blockCounter;
    const std::size_t savedBlock = m_currentBlockIdx;
    auto savedScopes = std::move(m_scopes);
    const std::string savedCatch = m_catchLabel;
    const std::string savedCatchVar = m_catchVar;
    m_scopes.clear();
    m_catchLabel.clear();
    m_catchVar.clear();

    IRFunction fn;
    fn.name = "__lambda_" + std::to_string(++m_lambdaSerial);
    fn.mangledName = fn.name;
    fn.returnType = lambda.left != nullptr ? MakePrim("Int") : MakePrim("Int");
    fn.location = lambda.location;
    for (const auto& name : captures) {
        fn.parameters.push_back(Frontend::FunctionParamInfo{ name, MakePrim("Int"), false, false, false });
    }
    for (const auto& name : lambda.lambdaParams) {
        fn.parameters.push_back(Frontend::FunctionParamInfo{ name, MakePrim("Int"), false, false, false });
    }
    m_module->functions.push_back(std::move(fn));
    m_currentFn = &m_module->functions.back();
    m_blockCounter = 0;
    m_currentBlockIdx = CreateBasicBlock("entry");
    if (lambda.lambdaBody) {
        LowerStmt(*lambda.lambdaBody);
    } else if (lambda.left) {
        IRValue result = LowerExpr(*lambda.left);
        IRInstruction ret;
        ret.opcode = IROpcode::ReturnVal;
        ret.operandIds = { result.ssaId };
        ret.location = lambda.location;
        Emit(std::move(ret));
    }
    if (!CurrentBlockHasTerminator()) {
        IRInstruction ret;
        ret.opcode = IROpcode::ReturnVoid;
        ret.location = lambda.location;
        Emit(std::move(ret));
        m_currentFn->returnType = MakePrim("Void");
    }
    const std::string lambdaName = m_currentFn->name;
    const auto returnType = m_currentFn->returnType;

    m_currentFn = &m_module->functions[callerIndex];
    m_blockCounter = savedCounter;
    m_currentBlockIdx = savedBlock;
    m_scopes = std::move(savedScopes);
    m_catchLabel = savedCatch;
    m_catchVar = savedCatchVar;

    std::vector<std::uint32_t> ids;
    for (const auto& name : captures) {
        IRInstruction load;
        load.opcode = IROpcode::LoadVar;
        load.dest = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
        load.textImm = name;
        load.location = call.location;
        ids.push_back(load.dest.ssaId);
        Emit(std::move(load));
    }
    ids.insert(ids.end(), argIds.begin(), argIds.end());
    IRInstruction invoke;
    invoke.opcode = IROpcode::CallDirect;
    invoke.dest = IRValue{ m_currentFn->AllocateSSAId(), returnType };
    invoke.textImm = lambdaName;
    invoke.operandIds = std::move(ids);
    invoke.location = call.location;
    IRValue out = invoke.dest;
    Emit(std::move(invoke));
    return out;
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
            const auto folded = m_consts.find(expr.text);
            if (folded != m_consts.end()) {
                IRInstruction inst;
                inst.opcode   = IROpcode::ConstInt;
                inst.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                inst.intImm   = folded->second;
                inst.location = expr.location;
                IRValue out   = inst.dest;
                Emit(std::move(inst));
                return out;
            }
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

            if (uop == "&" && expr.left && expr.left->kind == Frontend::ExprKind::Identifier) {
                IRInstruction addr;
                addr.opcode   = IROpcode::AddrOf;
                addr.dest     = IRValue{ m_currentFn->AllocateSSAId(), expr.inferredType };
                addr.textImm  = expr.left->text;
                addr.intImm   = PointeeBytes(expr.inferredType);
                addr.boolImm  = PointeeSigned(expr.inferredType);
                addr.location = expr.location;
                IRValue out   = addr.dest;
                Emit(std::move(addr));
                return out;
            }
            if (uop == "*" && expr.left) {
                IRValue ptr = LowerExpr(*expr.left);
                IRInstruction load;
                load.opcode     = IROpcode::PtrLoad;
                load.dest       = IRValue{ m_currentFn->AllocateSSAId(), expr.inferredType.IsEmpty() ? MakePrim("Int") : expr.inferredType };
                load.operandIds = { ptr.ssaId };
                load.intImm     = PointeeBytes(expr.left->inferredType);
                load.boolImm    = PointeeSigned(expr.left->inferredType);
                load.location   = expr.location;
                IRValue out     = load.dest;
                Emit(std::move(load));
                return out;
            }

            if (uop != "!" && uop != "-" && uop != "+" && uop != "~") {
                m_diagnostics.ReportError(
                    "I4011",
                    "Unary operator '" + uop + "' is not yet supported by the SSA IR backend.",
                    expr.location,
                    "Use '-', '!', '~', '&', '*', '++' or '--' on variables.");
                return IRValue{};
            }

            IRValue opVal = expr.left ? LowerExpr(*expr.left) : IRValue{};
            if (uop == "+") {
                return opVal;
            }
            IRInstruction un;
            if (uop == "~") un.opcode = IROpcode::BitNot;
            else if (uop == "!") un.opcode = IROpcode::Not;
            else un.opcode = IROpcode::Neg;
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

            const std::string& t = expr.text;
            const bool leftPtr = expr.left && (expr.left->inferredType.isPointer || expr.left->inferredType.baseName == "Pointer");
            if ((t == "+" || t == "-") && leftPtr) {
                IRValue idx = r;
                if (t == "-") {
                    IRInstruction neg;
                    neg.opcode     = IROpcode::Neg;
                    neg.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                    neg.operandIds = { r.ssaId };
                    neg.location   = expr.location;
                    idx            = neg.dest;
                    Emit(std::move(neg));
                }
                Frontend::TypeNode pty = expr.left->inferredType;
                pty.isPointer = true;
                IRInstruction off;
                off.opcode     = IROpcode::PtrOffset;
                off.dest       = IRValue{ m_currentFn->AllocateSSAId(), pty };
                off.operandIds = { l.ssaId, idx.ssaId };
                off.intImm     = PointeeBytes(expr.left->inferredType);
                off.boolImm    = PointeeSigned(expr.left->inferredType);
                off.location   = expr.location;
                IRValue out    = off.dest;
                Emit(std::move(off));
                return out;
            }

            IROpcode op = IROpcode::Add;
            Frontend::TypeNode resType = l.type;
            if (t == "+" && expr.inferredType.baseName == "String") resType = MakePrim("String");
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
            else if (t == "&") op = IROpcode::BitAnd;
            else if (t == "|") op = IROpcode::BitOr;
            else if (t == "^") op = IROpcode::BitXor;
            else if (t == "<<") op = IROpcode::Shl;
            else if (t == ">>") op = IROpcode::Shr;
            else {
                m_diagnostics.ReportError(
                    "I4011",
                    "Binary operator '" + t + "' is not yet supported by the SSA IR backend.",
                    expr.location,
                    "Supported operators: + - * / % & | ^ << >> == != < <= > >= && ||");
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
            if (expr.left && expr.left->kind == Frontend::ExprKind::Index && expr.left->left &&
                !(expr.left->right && expr.left->right->kind == Frontend::ExprKind::Range)) {
                IRValue base = LowerExpr(*expr.left->left);
                IRValue idx  = expr.left->right ? LowerExpr(*expr.left->right) : IRValue{};
                IRInstruction st;
                st.opcode     = IROpcode::StoreIndex;
                st.operandIds = { base.ssaId, idx.ssaId, rhs.ssaId };
                st.location   = expr.location;
                Emit(std::move(st));
                return rhs;
            }
            if (expr.left && expr.left->kind == Frontend::ExprKind::Unary && expr.left->text == "*" && expr.left->left) {
                IRValue ptr = LowerExpr(*expr.left->left);
                IRInstruction st;
                st.opcode     = IROpcode::PtrStore;
                st.operandIds = { ptr.ssaId, rhs.ssaId };
                st.intImm     = PointeeBytes(expr.left->left->inferredType);
                st.boolImm    = PointeeSigned(expr.left->left->inferredType);
                st.location   = expr.location;
                Emit(std::move(st));
                return rhs;
            }
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
                setF.intImm     = FieldSlot(expr.left->left->inferredType.baseName, expr.left->text);
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
            NoteGenericUse(expr.targetType);
            const std::string lookup = expr.targetType.baseName;
            const std::string irName = TypeKey(expr.targetType);
            const auto* cls = m_sema.FindClassOrType(lookup);
            const bool isClass = cls != nullptr && cls->kind == Frontend::DeclKind::Class;
            int slots = 1;
            if (cls) {
                slots = isClass ? static_cast<int>(cls->fields.size()) + 1
                                : std::max(1, static_cast<int>(cls->fields.size()));
            }
            Frontend::TypeNode objTy = expr.targetType;
            objTy.isPointer = true;
            IRInstruction alloc;
            alloc.opcode     = IROpcode::AllocHeap;
            alloc.dest       = IRValue{ m_currentFn->AllocateSSAId(), objTy };
            alloc.textImm    = isClass ? ClassChain(irName) : lookup;
            alloc.boolImm    = isClass;
            alloc.intImm     = slots;
            alloc.location   = expr.location;
            IRValue out      = alloc.dest;
            Emit(std::move(alloc));
            if (cls) {
                auto ctor = cls->methods.find(cls->name);
                if (ctor != cls->methods.end() && ctor->second.astNode && ctor->second.astNode->body) {
                    IRInstruction call;
                    call.opcode     = IROpcode::CallDirect;
                    call.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Void") };
                    call.textImm    = irName + "." + lookup;
                    call.operandIds = { out.ssaId };
                    call.operandIds.insert(call.operandIds.end(), argIds.begin(), argIds.end());
                    call.location   = expr.location;
                    Emit(std::move(call));
                }
            }
            return out;
        }

        case Frontend::ExprKind::MemberAccess: {
            if (expr.left && expr.left->kind == Frontend::ExprKind::Identifier) {
                if (const auto* cls = m_sema.FindClassOrType(expr.left->text)) {
                    if (cls->kind == Frontend::DeclKind::Enum) {
                        for (std::size_t i = 0; i < cls->enumVariants.size(); ++i) {
                            if (cls->enumVariants[i] == expr.text) {
                                IRInstruction tag;
                                tag.opcode   = IROpcode::ConstInt;
                                tag.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim(cls->name) };
                                tag.intImm   = static_cast<std::int64_t>(i);
                                tag.location = expr.location;
                                IRValue out  = tag.dest;
                                Emit(std::move(tag));
                                return out;
                            }
                        }
                    }
                }
            }
            IRValue objVal = expr.left ? LowerExpr(*expr.left) : IRValue{};
            IRInstruction getF;
            getF.opcode     = IROpcode::GetField;
            getF.dest       = IRValue{ m_currentFn->AllocateSSAId(), exprType };
            getF.textImm    = expr.text;
            getF.intImm     = FieldSlot(expr.left ? expr.left->inferredType.baseName : std::string{}, expr.text);
            getF.operandIds = { objVal.ssaId };
            getF.location   = expr.location;
            IRValue out     = getF.dest;
            Emit(std::move(getF));
            return out;
        }

        case Frontend::ExprKind::Call: {
            if (expr.left && expr.left->kind == Frontend::ExprKind::MemberAccess && expr.left->left &&
                expr.left->left->kind == Frontend::ExprKind::Identifier && expr.left->left->text == "Memory" &&
                (expr.left->text == "SizeOf" || expr.left->text == "OffsetOf")) {
                const auto folded = EvalConst(expr);
                if (!folded) {
                    m_diagnostics.ReportError(
                        "L1010",
                        "Memory." + expr.left->text + " needs a visible type and, for OffsetOf, a field of that type.",
                        expr.location,
                        "Write Memory.SizeOf(TypeName) or Memory.OffsetOf(TypeName, fieldName).");
                    return IRValue{};
                }
                IRInstruction inst;
                inst.opcode   = IROpcode::ConstInt;
                inst.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                inst.intImm   = *folded;
                inst.location = expr.location;
                IRValue out   = inst.dest;
                Emit(std::move(inst));
                return out;
            }
            if (expr.left && expr.left->kind == Frontend::ExprKind::MemberAccess && expr.left->left &&
                expr.left->left->kind == Frontend::ExprKind::Identifier &&
                ((expr.left->left->text == "CPU" &&
                  (expr.left->text == "Function" || expr.left->text == "Handler" || expr.left->text == "Fault" || expr.left->text == "Run")) ||
                 (expr.left->left->text == "Timer" && (expr.left->text == "Arm" || expr.left->text == "Preempt")))) {
                const std::string member = expr.left->left->text + "." + expr.left->text;
                if (member == "Timer.Preempt") {
                    if (expr.arguments.size() < 2 || !expr.arguments[0] || !expr.arguments[1] ||
                        expr.arguments[0]->kind != Frontend::ExprKind::Identifier ||
                        expr.arguments[1]->kind != Frontend::ExprKind::Identifier) {
                        m_diagnostics.ReportError(
                            "I4011",
                            "Timer.Preempt takes two function names.",
                            expr.location,
                            "Write Timer.Preempt(FirstTask, SecondTask).");
                        return IRValue{};
                    }
                    auto known = [&](const std::string& name) {
                        for (const auto& sig : m_sigs) {
                            if (sig.sourceName == name && sig.arity == 0 && !sig.isExtern) return true;
                        }
                        return false;
                    };
                    const std::string first = expr.arguments[0]->text;
                    const std::string second = expr.arguments[1]->text;
                    if (!known(first) || !known(second)) {
                        m_diagnostics.ReportError(
                            "R1003",
                            "Timer.Preempt cannot see both '" + first + "' and '" + second + "'.",
                            expr.location,
                            "Declare both as parameterless functions.");
                        return IRValue{};
                    }
                    IRInstruction osCall;
                    osCall.opcode = IROpcode::CallOSIntrinsic;
                    osCall.textImm = "Timer.Preempt";
                    osCall.secondaryText = first + "\n" + second;
                    osCall.location = expr.location;
                    osCall.dest = IRValue{ 0, MakePrim("Void") };
                    Emit(std::move(osCall));
                    return IRValue{ 0, MakePrim("Void") };
                }
                if (expr.arguments.empty() || !expr.arguments[0] ||
                    expr.arguments[0]->kind != Frontend::ExprKind::Identifier) {
                    m_diagnostics.ReportError(
                        "I4011",
                        member + " takes a function name.",
                        expr.location,
                        "Write " + member + "(FunctionName) for a parameterless function.");
                    return IRValue{};
                }
                const std::string target = expr.arguments[0]->text;
                bool found = false;
                for (const auto& sig : m_sigs) {
                    if (sig.sourceName == target && sig.arity == 0 && !sig.isExtern) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    m_diagnostics.ReportError(
                        "R1003",
                        member + " cannot see a parameterless function named '" + target + "'.",
                        expr.location,
                        "Declare 'public Function " + target + "() -> Void' in this program.");
                    return IRValue{};
                }
                IRInstruction osCall;
                osCall.opcode   = IROpcode::CallOSIntrinsic;
                osCall.textImm  = member;
                osCall.secondaryText = target;
                osCall.location = expr.location;
                if (member == "CPU.Run") {
                    if (expr.arguments.size() < 2 || !expr.arguments[1]) {
                        m_diagnostics.ReportError(
                            "I4011",
                            "CPU.Run takes a function name and a stack address.",
                            expr.location,
                            "Write CPU.Run(FunctionName, Memory.Address(stackTop)).");
                        return IRValue{};
                    }
                    osCall.operandIds = { LowerExpr(*expr.arguments[1]).ssaId };
                }
                if (member == "CPU.Run" || member == "Timer.Arm") {
                    osCall.dest = IRValue{ 0, MakePrim("Void") };
                    Emit(std::move(osCall));
                    return IRValue{ 0, MakePrim("Void") };
                }
                osCall.dest = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                IRValue out = osCall.dest;
                Emit(std::move(osCall));
                return out;
            }

            std::vector<std::uint32_t> argIds;
            for (const auto& arg : expr.arguments) {
                if (arg) {
                    argIds.push_back(LowerExpr(*arg).ssaId);
                }
            }

            if (expr.left && expr.left->kind == Frontend::ExprKind::MemberAccess && expr.left->left &&
                expr.left->left->kind == Frontend::ExprKind::Identifier) {
                const auto declared = m_classDecls.find(expr.left->left->text);
                if (declared != m_classDecls.end() && declared->second != nullptr &&
                    declared->second->kind == Frontend::DeclKind::Interface) {
                    auto constant = [&](const std::string& text) {
                        IRInstruction value;
                        value.opcode   = IROpcode::ConstString;
                        value.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("String") };
                        value.textImm  = text;
                        value.location = expr.location;
                        IRValue out    = value.dest;
                        Emit(std::move(value));
                        return out.ssaId;
                    };
                    if (argIds.empty()) argIds.push_back(constant(""));
                    IRInstruction call;
                    call.opcode     = IROpcode::CallOSIntrinsic;
                    call.textImm    = "Network.Call";
                    Frontend::TypeNode result = MakePrim("String");
                    result.isPointer = true;
                    call.dest       = IRValue{ m_currentFn->AllocateSSAId(), result };
                    call.operandIds = { constant(expr.left->left->text), constant(expr.left->text), argIds[0] };
                    call.location   = expr.location;
                    IRValue out     = call.dest;
                    Emit(std::move(call));
                    return out;
                }
            }

            if (expr.left && expr.left->kind == Frontend::ExprKind::Lambda) {
                return OutlineLambda(expr, *expr.left, argIds);
            }

            if (expr.left && expr.left->kind == Frontend::ExprKind::Identifier) {
                const std::string& fnName = expr.left->text;
                if (fnName == "Abort" || fnName == "Panic") {
                    IRInstruction trap;
                    trap.opcode   = IROpcode::CallOSIntrinsic;
                    trap.textImm  = fnName;
                    trap.location = expr.location;
                    Emit(std::move(trap));
                    return IRValue{ 0, MakePrim("Void") };
                }
                if (fnName == "Input") {
                    const bool asInt = expr.left->targetType.baseName == "Int" ||
                                       expr.left->targetType.baseName == "Int64";
                    Frontend::TypeNode ty = asInt ? MakePrim("Int") : MakePrim("String");
                    if (!asInt) ty.isPointer = true;
                    IRInstruction in;
                    in.opcode     = IROpcode::CallOSIntrinsic;
                    in.dest       = IRValue{ m_currentFn->AllocateSSAId(), ty };
                    in.textImm    = asInt ? "Input.Int" : "Input";
                    in.operandIds = std::move(argIds);
                    in.location   = expr.location;
                    IRValue out   = in.dest;
                    Emit(std::move(in));
                    return out;
                }
                if (fnName == "Print" && !argIds.empty()) {
                    IRInstruction pr;
                    pr.opcode     = IROpcode::PrintVal;
                    pr.operandIds = std::move(argIds);
                    pr.location   = expr.location;
                    if (!expr.arguments.empty() && expr.arguments[0] &&
                        (expr.arguments[0]->inferredType.baseName == "Array" ||
                         expr.arguments[0]->inferredType.baseName == "Slice")) {
                        pr.textImm = "Array";
                    }
                    Emit(std::move(pr));
                    return IRValue{ 0, MakePrim("Void") };
                }
                if ((fnName == "Assert" || fnName == "AssertTrue" || fnName == "AssertFalse" ||
                     fnName == "AssertEqual" || fnName == "AssertPixel") && !argIds.empty()) {
                    IRInstruction asrt;
                    asrt.opcode     = IROpcode::AssertVal;
                    asrt.textImm    = fnName;
                    asrt.operandIds = std::move(argIds);
                    asrt.location   = expr.location;
                    Emit(std::move(asrt));
                    return IRValue{ 0, MakePrim("Void") };
                }
                auto emitTagged = [&](const char* kind, std::int64_t tag, std::uint32_t valueId) {
                    IRValue obj = EmitAggregate(kind, 2, MakePrim(kind));
                    IRInstruction tagC;
                    tagC.opcode   = IROpcode::ConstInt;
                    tagC.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                    tagC.intImm   = tag;
                    tagC.location = expr.location;
                    IRValue tagV  = tagC.dest;
                    Emit(std::move(tagC));
                    IRInstruction setTag;
                    setTag.opcode     = IROpcode::SetField;
                    setTag.textImm    = "tag";
                    setTag.intImm     = 0;
                    setTag.operandIds = { obj.ssaId, tagV.ssaId };
                    setTag.location   = expr.location;
                    Emit(std::move(setTag));
                    IRInstruction setVal;
                    setVal.opcode     = IROpcode::SetField;
                    setVal.textImm    = (std::string(kind) == "Tuple") ? "t0" : "value";
                    setVal.intImm     = 1;
                    setVal.operandIds = { obj.ssaId, valueId };
                    setVal.location   = expr.location;
                    Emit(std::move(setVal));
                    return obj;
                };

                if (fnName == "Success" || fnName == "Error" || fnName == "Some" || fnName == "None") {
                    const bool isNone = fnName == "None";
                    const bool isOk = fnName == "Success" || fnName == "Some";
                    std::uint32_t valueId = 0;
                    if (!isNone && !argIds.empty()) {
                        valueId = argIds[0];
                    } else {
                        IRInstruction zero;
                        zero.opcode   = IROpcode::ConstInt;
                        zero.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                        zero.intImm   = 0;
                        zero.location = expr.location;
                        valueId       = zero.dest.ssaId;
                        Emit(std::move(zero));
                    }
                    const char* kind = (fnName == "Some" || fnName == "None") ? "Option" : "Result";
                    return emitTagged(kind, isOk ? 1 : 0, valueId);
                }
                if (fnName == "Tuple" && argIds.size() >= 2) {
                    IRValue obj = EmitAggregate("Tuple", 2, MakePrim("Tuple"));
                    IRInstruction a;
                    a.opcode = IROpcode::SetField;
                    a.textImm = "t0";
                    a.intImm = 0;
                    a.operandIds = { obj.ssaId, argIds[0] };
                    a.location = expr.location;
                    Emit(std::move(a));
                    IRInstruction b;
                    b.opcode = IROpcode::SetField;
                    b.textImm = "t1";
                    b.intImm = 1;
                    b.operandIds = { obj.ssaId, argIds[1] };
                    b.location = expr.location;
                    Emit(std::move(b));
                    return obj;
                }

                IRInstruction call;
                call.opcode          = IROpcode::CallDirect;
                call.dest            = IRValue{ m_currentFn->AllocateSSAId(), exprType };
                call.textImm         = ResolveCallable(fnName, static_cast<int>(argIds.size()), expr.location);
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
                    (receiverExpr.text == "File" || receiverExpr.text == "Args" ||
                     receiverExpr.text == "Env" ||
                     receiverExpr.text == "Network" || receiverExpr.text == "Thread" ||
                     receiverExpr.text == "Time" || receiverExpr.text == "GPIO" ||
                     receiverExpr.text == "PWM" || receiverExpr.text == "UART" ||
                     receiverExpr.text == "SPI" || receiverExpr.text == "I2C" ||
                     receiverExpr.text == "CAN" || receiverExpr.text == "USB" ||
                     receiverExpr.text == "PCIe" || receiverExpr.text == "DMA" ||
                     receiverExpr.text == "Sensor" || receiverExpr.text == "Motor" ||
                     receiverExpr.text == "Interrupt" || receiverExpr.text == "HardwareTimer" ||
                     receiverExpr.text == "Memory" || receiverExpr.text == "Math" ||
                     receiverExpr.text == "UI" || receiverExpr.text == "Scene" ||
                     receiverExpr.text == "Volatile" || receiverExpr.text == "Port" ||
                     receiverExpr.text == "CPU" || receiverExpr.text == "Clock" || receiverExpr.text == "System" ||
                     receiverExpr.text == "Timer" || receiverExpr.text == "Console" ||
                     receiverExpr.text == "Atomic" ||                      receiverExpr.text == "Frame" ||
                     receiverExpr.text == "Input" || receiverExpr.text == "Page" ||
                     receiverExpr.text == "Sched" || receiverExpr.text == "Fault" ||
                     receiverExpr.text == "Proc" || receiverExpr.text == "Cred" ||
                     receiverExpr.text == "Syscall" || receiverExpr.text == "FS" ||
                     receiverExpr.text == "PCI" || receiverExpr.text == "HID" ||
                     receiverExpr.text == "Driver" || receiverExpr.text == "Vendor" ||
                     receiverExpr.text == "GPU" || receiverExpr.text == "Launch" ||
                     receiverExpr.text == "Auth" || receiverExpr.text == "ACL" ||
                     receiverExpr.text == "Settings" || receiverExpr.text == "Search" ||
                     receiverExpr.text == "Array")) {
                    if (receiverExpr.text == "Array") {
                        m_currentFn->usesHeapOrGC = true;
                    }
                    Frontend::TypeNode ty = exprType;
                    if (receiverExpr.text == "UI" || receiverExpr.text == "Scene") {
                        ty = MakePrim("Void");
                    } else if (receiverExpr.text == "Port" &&
                               (memberName == "Create" || memberName == "Send" || memberName == "Receive" ||
                                memberName == "Read8" || memberName == "Read16" || memberName == "Read32")) {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "CPU" &&
                               (memberName == "TSC" || memberName == "ReadCR3" || memberName == "ReadRSP" ||
                                memberName == "ExchangeRSP" || memberName == "SavedRSP" || memberName == "BootInfo" ||
                                memberName == "Function" || memberName == "Handler" || memberName == "Fault" || memberName == "Catch" ||
                                memberName == "Gate" ||
                                memberName == "GateHigh")) {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Clock" && memberName == "Seconds") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Sched" &&
                               (memberName == "Enqueue" || memberName == "Pick" || memberName == "Block")) {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Proc" &&
                               (memberName == "Create" || memberName == "Exit" || memberName == "Wait")) {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Cred" && memberName == "Allow") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Syscall" && memberName == "Dispatch") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "FS" &&
                               (memberName == "Lookup" || memberName == "Create" || memberName == "Read" ||
                                memberName == "Write" || memberName == "List")) {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "PCI" && memberName == "Decode") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "HID" && memberName == "Parse") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Driver" && (memberName == "Match" || memberName == "Bind" || memberName == "Package" || memberName == "Newer")) {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Vendor" && memberName == "Match") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "GPU" && (memberName == "Command" || memberName == "Submit")) {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Launch" && memberName == "Plan") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Auth" && memberName == "Check") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "ACL" && memberName == "Allow") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Settings" &&
                               (memberName == "Get" || memberName == "Set" || memberName == "Reset")) {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Search" && memberName == "Query") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Volatile" && memberName == "Load32") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Volatile" && memberName == "Store32") {
                        ty = MakePrim("Void");
                    } else if (receiverExpr.text == "Port" || receiverExpr.text == "CPU") {
                        ty = MakePrim("Void");
                    } else if (receiverExpr.text == "Memory" && memberName == "Pages") {
                        ty = MakePrim("Pointer");
                        ty.isPointer = true;
                        ty.typeArgs.push_back(MakePrim("Int"));
                    } else if (receiverExpr.text == "Memory" && memberName == "At") {
                        ty = MakePrim("Pointer");
                        ty.isPointer = true;
                        ty.typeArgs.push_back(MakePrim("Byte"));
                    } else if (receiverExpr.text == "Memory" &&
                               (memberName == "Address" || memberName == "Frame")) {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Page" && (memberName == "Entry" || memberName == "Split")) {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Fault" && memberName == "Decide") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Frame" &&
                               (memberName == "Create" || memberName == "Pixel" || memberName == "Present" ||
                                memberName == "Flush")) {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Frame") {
                        ty = MakePrim("Void");
                    }
                    if ((receiverExpr.text == "File" && memberName == "ReadText") ||
                        (receiverExpr.text == "Args" && memberName == "Get") ||
                        (receiverExpr.text == "Env" && memberName == "Get") ||
                        (receiverExpr.text == "Network" && memberName == "SendText")) {
                        ty = MakePrim("String");
                        ty.isPointer = true;
                    } else if (receiverExpr.text == "Args" && memberName == "Count") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "File" && memberName == "WriteText") {
                        ty = MakePrim("Bool");
                    } else if (receiverExpr.text == "Scene" && memberName == "Render") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "UI" &&
                               (memberName == "LoadCooked" || memberName == "PresentPlatform" || memberName == "PlatformScroll" ||
                                memberName == "PlatformHeight" || memberName == "PlatformSpot" || memberName == "PlatformMotion" ||
                                memberName == "PlatformSample" || memberName == "Role" || memberName == "BoxX" ||
                                memberName == "BoxY" || memberName == "BoxWidth" || memberName == "BoxHeight" ||
                                memberName == "Place" || memberName == "Hit" || memberName == "Drag" ||
                                memberName == "Snap" || memberName == "Focus" || memberName == "Focused" ||
                                memberName == "Menu" || memberName == "MenuOpen" || memberName == "Stack" ||
                                memberName == "Grid")) {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Input" &&
                               (memberName == "Clear" || memberName == "SetMouse" || memberName == "AddClick")) {
                        ty = MakePrim("Void");
                    } else if (receiverExpr.text == "Input") {
                        ty = MakePrim("Int");
                    } else if (receiverExpr.text == "Math" ||
                               (receiverExpr.text == "GPIO" && memberName == "Read") ||
                               receiverExpr.text == "PWM" || receiverExpr.text == "UART" ||
                               receiverExpr.text == "SPI" || receiverExpr.text == "I2C" ||
                               receiverExpr.text == "CAN" || receiverExpr.text == "USB" ||
                               receiverExpr.text == "PCIe" || receiverExpr.text == "DMA" ||
                               receiverExpr.text == "Sensor" || receiverExpr.text == "Motor" ||
                               receiverExpr.text == "Interrupt" || receiverExpr.text == "HardwareTimer" ||
                               (receiverExpr.text == "Atomic" && memberName == "FetchAdd") ||
                               (receiverExpr.text == "Thread" && memberName == "AddPair")) {
                        ty = MakePrim("Int");
                    }
                    IRInstruction osCall;
                    osCall.opcode     = IROpcode::CallOSIntrinsic;
                    osCall.dest       = IRValue{ m_currentFn->AllocateSSAId(), ty };
                    osCall.textImm    = receiverExpr.text + "." + memberName;
                    osCall.operandIds = std::move(argIds);
                    osCall.location   = expr.location;
                    IRValue out       = osCall.dest;
                    Emit(std::move(osCall));
                    return out;
                }

                IRValue recvVal = LowerExpr(receiverExpr);
                const std::string recvClass = receiverExpr.inferredType.baseName;
                const std::string recvKey = TypeKey(receiverExpr.inferredType);
                NoteGenericUse(receiverExpr.inferredType);

                // Built-in numeric conversions (.ToFloat(), .ToInt())
                if (memberName == "Length" || memberName == "Count") {
                    const std::string& owner = receiverExpr.inferredType.baseName;
                    if (owner == "Array" || owner == "Slice") {
                        IRInstruction len;
                        len.opcode     = IROpcode::GetField;
                        len.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                        len.textImm    = "len";
                        len.intImm     = -1;
                        len.operandIds = { recvVal.ssaId };
                        len.location   = expr.location;
                        IRValue out    = len.dest;
                        Emit(std::move(len));
                        return out;
                    }
                    if (owner == "String") {
                        IRInstruction len;
                        len.opcode     = IROpcode::CallOSIntrinsic;
                        len.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                        len.textImm    = "String.Length";
                        len.operandIds = { recvVal.ssaId };
                        len.location   = expr.location;
                        IRValue out    = len.dest;
                        Emit(std::move(len));
                        return out;
                    }
                }

                const bool arrayMethod =
                    (recvClass == "Array" || recvClass == "Slice") &&
                    (memberName == "Swap" || memberName == "Reverse" || memberName == "Sort" ||
                     memberName == "Rotate" || memberName == "Fill" || memberName == "Shuffle" ||
                     memberName == "Sum" || memberName == "Min" || memberName == "Max" ||
                     memberName == "Product" || memberName == "IndexOf" || memberName == "LastIndexOf" ||
                     memberName == "CountOf" || memberName == "BinarySearch" || memberName == "Contains" ||
                     memberName == "IsSorted" || memberName == "IsEmpty" || memberName == "Copy" ||
                     memberName == "Reversed" || memberName == "Sorted" || memberName == "Rotated" ||
                     memberName == "Unique" || memberName == "Concat");
                if (arrayMethod) {
                    if (memberName == "Copy" || memberName == "Reversed" || memberName == "Sorted" ||
                        memberName == "Rotated" || memberName == "Unique" || memberName == "Concat") {
                        m_currentFn->usesHeapOrGC = true;
                    }
                    std::vector<std::uint32_t> arrayArgs = { recvVal.ssaId };
                    arrayArgs.insert(arrayArgs.end(), argIds.begin(), argIds.end());
                    IRInstruction arrayCall;
                    arrayCall.opcode     = IROpcode::CallOSIntrinsic;
                    arrayCall.dest       = IRValue{ m_currentFn->AllocateSSAId(), exprType };
                    arrayCall.textImm    = "Array." + memberName;
                    arrayCall.operandIds = std::move(arrayArgs);
                    arrayCall.location   = expr.location;
                    IRValue arrayOut     = arrayCall.dest;
                    Emit(std::move(arrayCall));
                    return arrayOut;
                }

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
                mCall.textImm         = recvKey.empty() ? memberName : (recvKey + "." + memberName);
                mCall.secondaryText   = recvClass;
                mCall.intImm          = VirtualSlot(recvClass, memberName);
                mCall.operandIds      = std::move(methodArgs);
                mCall.isDevirtualized = devirt;
                mCall.location        = expr.location;
                IRValue out           = mCall.dest;
                Emit(std::move(mCall));
                return out;
            }
            break;
        }

        case Frontend::ExprKind::ArrayLiteral: {
            Frontend::TypeNode ty = expr.inferredType;
            if (ty.baseName.empty()) ty.baseName = "Array";
            ty.isPointer = true;
            m_currentFn->usesHeapOrGC = true;
            IRInstruction alloc;
            alloc.opcode   = IROpcode::AllocHeap;
            alloc.dest     = IRValue{ m_currentFn->AllocateSSAId(), ty };
            alloc.textImm  = "Array";
            alloc.intImm   = static_cast<std::int64_t>(expr.arguments.size());
            alloc.location = expr.location;
            IRValue arr    = alloc.dest;
            Emit(std::move(alloc));
            for (std::size_t i = 0; i < expr.arguments.size(); ++i) {
                if (!expr.arguments[i]) continue;
                IRValue elem = LowerExpr(*expr.arguments[i]);
                IRInstruction idx;
                idx.opcode   = IROpcode::ConstInt;
                idx.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                idx.intImm   = static_cast<std::int64_t>(i);
                idx.location = expr.location;
                IRValue iv   = idx.dest;
                Emit(std::move(idx));
                IRInstruction st;
                st.opcode     = IROpcode::StoreIndex;
                st.operandIds = { arr.ssaId, iv.ssaId, elem.ssaId };
                st.location   = expr.location;
                Emit(std::move(st));
            }
            return arr;
        }

        case Frontend::ExprKind::Index: {
            IRValue base = expr.left ? LowerExpr(*expr.left) : IRValue{};
            if (expr.right && expr.right->kind == Frontend::ExprKind::Range) {
                IRValue start;
                if (expr.right->left) {
                    start = LowerExpr(*expr.right->left);
                } else {
                    IRInstruction z;
                    z.opcode   = IROpcode::ConstInt;
                    z.dest     = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                    z.intImm   = 0;
                    z.location = expr.location;
                    start      = z.dest;
                    Emit(std::move(z));
                }
                IRValue end;
                if (expr.right->right) {
                    end = LowerExpr(*expr.right->right);
                } else {
                    IRInstruction len;
                    len.opcode     = IROpcode::GetField;
                    len.dest       = IRValue{ m_currentFn->AllocateSSAId(), MakePrim("Int") };
                    len.textImm    = "len";
                    len.intImm     = -1;
                    len.operandIds = { base.ssaId };
                    len.location   = expr.location;
                    end            = len.dest;
                    Emit(std::move(len));
                }
                Frontend::TypeNode ty = expr.inferredType;
                if (ty.baseName.empty()) ty.baseName = "Array";
                ty.isPointer = true;
                IRInstruction slice;
                slice.opcode     = IROpcode::SliceCopy;
                slice.dest       = IRValue{ m_currentFn->AllocateSSAId(), ty };
                slice.operandIds = { base.ssaId, start.ssaId, end.ssaId };
                slice.intImm     = expr.right->isInclusiveRange ? 1 : 0;
                slice.location   = expr.location;
                IRValue out      = slice.dest;
                Emit(std::move(slice));
                return out;
            }
            IRValue idx = expr.right ? LowerExpr(*expr.right) : IRValue{};
            IRInstruction load;
            load.opcode     = IROpcode::LoadIndex;
            load.dest       = IRValue{ m_currentFn->AllocateSSAId(), expr.inferredType.IsEmpty() ? MakePrim("Int") : expr.inferredType };
            load.operandIds = { base.ssaId, idx.ssaId };
            load.location   = expr.location;
            IRValue out     = load.dest;
            Emit(std::move(load));
            return out;
        }

        default:
            break;
    }

    m_diagnostics.ReportError(
        "I4011",
        "This expression form (object literal, lambda, match expression) is not yet supported by the SSA IR backend.",
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

