class RobixEmitter {
public:
    explicit RobixEmitter(const Program& program) : program_(program) {}

    std::string Generate(const std::string& entry) {
        entry_ = entry;
        out_.str("");
        out_.clear();
        line("#include \"runtime.hpp\"");
        line("#include <cstdint>");
        line("#include <memory>");
        line("");
        for (const auto& item : program_.enums) {
            const std::string name = Safe(item.first);
            line("struct " + name + " {");
            line("    int64_t value = 0;");
            line("    constexpr int64_t ToInt() const { return value; }");
            for (const auto& member : item.second.members) line("    static const " + name + " " + Safe(member.first) + ";");
            line("};");
            line("inline bool operator==(" + name + " left, " + name + " right) { return left.value == right.value; }");
            line("inline bool operator!=(" + name + " left, " + name + " right) { return left.value != right.value; }");
        }
        line("");
        for (const auto& item : program_.enums) {
            for (const auto& member : item.second.members) {
                line("inline const " + Safe(item.first) + " " + Safe(item.first) + "::" + Safe(member.first) + "{" + std::to_string(member.second) + "};");
            }
        }
        line("");
        for (const auto& item : program_.classes) {
            line("struct " + Safe(item.first) + ";");
            line("using " + Safe(item.first) + "Ptr = std::shared_ptr<" + Safe(item.first) + ">;");
        }
        line("");
        for (const auto& item : program_.classes) WriteClass(item.second);
        for (const auto& item : program_.classes) {
            line(Safe(item.first) + "::~" + Safe(item.first) + "() { rx::ObjectLedger() -= 1; }");
        }
        line("");
        for (const FuncInfo& func : program_.functions) {
            if (Included(func)) line(Cpp(Resolve(func.returnSpelling)) + " " + Safe(func.name) + "(" + ParamList(func.params) + ");");
        }
        line("");
        for (const auto& item : program_.classes) WriteBodies(item.second);
        for (const FuncInfo& func : program_.functions) {
            if (!Included(func)) continue;
            Begin(nullptr, Resolve(func.returnSpelling), func.params);
            line(Cpp(result_) + " " + Safe(func.name) + "(" + ParamList(func.params) + ") {");
            for (const Stmt& stmt : func.body.body) WriteStmt(stmt, 1);
            WriteFallback(1);
            line("}");
            line("");
        }
        WriteMain();
        return out_.str();
    }

private:
    const Program& program_;
    std::string entry_;
    std::ostringstream out_;
    std::vector<std::map<std::string, Type>> scopes_;
    const ClassInfo* klass_ = nullptr;
    Type result_{};
    int serial_ = 0;

    bool Included(const FuncInfo& func) const {
        if (func.isExtern) return false;
        if (entry_ == "Robix" && func.file == "Robixc.rx") return false;
        if (entry_ == "Robixc" && func.file == "Robix.rx") return false;
        return true;
    }

    Type Resolve(const std::string& spelling) const { return ResolveType(spelling, program_); }
    std::string Cpp(const Type& type) const { return CppType(type); }
    void line(const std::string& text) { out_ << text << "\n"; }
    std::string Fresh(const char* prefix) { return std::string(prefix) + std::to_string(serial_++); }

    void Begin(const ClassInfo* klass, const Type& type, const std::vector<Param>& params) {
        scopes_.clear();
        scopes_.emplace_back();
        klass_ = klass;
        result_ = type;
        serial_ = 0;
        for (const Param& param : params) scopes_.back()[param.name] = Resolve(param.typeSpelling);
    }

    const Type* Lookup(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return &found->second;
        }
        return nullptr;
    }

    const Field* FindField(const ClassInfo& info, const std::string& name) const {
        for (const Field& field : info.fields) if (field.name == name) return &field;
        return nullptr;
    }

    const Method* FindMethod(const ClassInfo& info, const std::string& name, size_t count, bool constructor) const {
        const Method* found = nullptr;
        for (const Method& method : info.methods) {
            if (method.isConstructor != constructor) continue;
            if (!constructor && method.name != name) continue;
            if (method.params.size() != count) continue;
            found = &method;
        }
        return found;
    }

    std::string NullOf(const Type& type) const {
        switch (type.kind) {
            case Type::Kind::String: return "rx::String::Null()";
            case Type::Kind::Object: return "nullptr";
            case Type::Kind::List: return Cpp(type) + "{}";
            case Type::Kind::Exec: return "rx::ExecutionResult{}";
            case Type::Kind::Bool: return "false";
            case Type::Kind::Float: return "0.0";
            case Type::Kind::Enum: return Safe(type.name) + "{}";
            default: return "0";
        }
    }

    std::string MethodId(const ClassInfo& info, const std::string& name) const {
        for (const Field& field : info.fields) {
            if (field.name == name) return Safe(name) + "_";
        }
        return Safe(name);
    }

    std::string DefaultOf(const Type& type) const {
        if (type.kind == Type::Kind::String) return "rx::String::Lit(\"\")";
        if (type.kind == Type::Kind::Bool) return "false";
        if (type.kind == Type::Kind::Float) return "0.0";
        return NullOf(type);
    }

    std::string TruthOf(const Typed& value) const {
        switch (value.type.kind) {
            case Type::Kind::Bool: return "(" + value.code + ")";
            case Type::Kind::Object: return "((" + value.code + ") != nullptr)";
            case Type::Kind::String:
            case Type::Kind::List: return "(!(" + value.code + ").IsNull())";
            case Type::Kind::Enum: return "((" + value.code + ").ToInt() != 0)";
            default: return "((" + value.code + ") != 0)";
        }
    }

    std::string Shown(const Typed& value) const {
        if (value.type.kind == Type::Kind::String) return value.code;
        if (value.type.kind == Type::Kind::Enum) return "rx::Show((" + value.code + ").ToInt())";
        return "rx::Show(" + value.code + ")";
    }

    std::string ParamList(const std::vector<Param>& params) const {
        std::string text;
        for (size_t i = 0; i < params.size(); i++) {
            if (i) text += ", ";
            text += Cpp(Resolve(params[i].typeSpelling)) + " " + Safe(params[i].name);
        }
        return text;
    }

    std::string Names(const std::vector<Param>& params) const {
        std::string text;
        for (size_t i = 0; i < params.size(); i++) {
            if (i) text += ", ";
            text += Safe(params[i].name);
        }
        return text;
    }

    void WriteClass(const ClassInfo& info) {
        const std::string name = Safe(info.name);
        line("struct " + name + " {");
        for (const Field& field : info.fields) {
            Type type = Resolve(field.typeSpelling);
            line("    " + Cpp(type) + " " + Safe(field.name) + " = " + DefaultOf(type) + ";");
        }
        line("    " + name + "() = default;");
        line("    ~" + name + "();");
        line("    " + name + "(const " + name + "&) = default;");
        line("    " + name + "& operator=(const " + name + "&) = default;");
        bool zero = false;
        std::set<size_t> counts;
        for (const Method& method : info.methods) {
            if (!method.isConstructor || !counts.insert(method.params.size()).second) continue;
            if (method.params.empty()) zero = true;
            line("    static " + name + "Ptr Make(" + ParamList(method.params) + ");");
            line("    void __init" + std::to_string(method.params.size()) + "(" + ParamList(method.params) + ");");
        }
        if (!zero) {
            line("    static " + name + "Ptr Make();");
        }
        for (const Method& method : info.methods) {
            if (method.isConstructor || method.isExtern) continue;
            line("    " + Cpp(Resolve(method.returnSpelling)) + " " + MethodId(info, method.name) + "(" + ParamList(method.params) + ");");
        }
        line("};");
        line("");
    }

    void WriteBodies(const ClassInfo& info) {
        const std::string name = Safe(info.name);
        bool zero = false;
        std::set<size_t> counts;
        for (const Method& method : info.methods) {
            if (!method.isConstructor) continue;
            if (!counts.insert(method.params.size()).second) Die(info.name + " has two constructors with the same number of arguments");
            if (method.params.empty()) zero = true;
            line(name + "Ptr " + name + "::Make(" + ParamList(method.params) + ") {");
            line("    auto created = std::allocate_shared<" + name + ">(rx::PoolAlloc<" + name + ">());");
            line("    rx::ObjectLedger() += 1;");
            line("    created->__init" + std::to_string(method.params.size()) + "(" + Names(method.params) + ");");
            line("    return created;");
            line("}");
            Begin(&info, MakeType(Type::Kind::Void), method.params);
            line("void " + name + "::__init" + std::to_string(method.params.size()) + "(" + ParamList(method.params) + ") {");
            for (const Stmt& stmt : method.body.body) WriteStmt(stmt, 1);
            line("}");
            line("");
        }
        if (!zero) {
            line(name + "Ptr " + name + "::Make() {");
            line("    auto created = std::allocate_shared<" + name + ">(rx::PoolAlloc<" + name + ">());");
            line("    rx::ObjectLedger() += 1;");
            line("    return created;");
            line("}");
            line("");
        }
        for (const Method& method : info.methods) {
            if (method.isConstructor || method.isExtern) continue;
            Begin(&info, Resolve(method.returnSpelling), method.params);
            line(Cpp(result_) + " " + name + "::" + MethodId(info, method.name) + "(" + ParamList(method.params) + ") {");
            for (const Stmt& stmt : method.body.body) WriteStmt(stmt, 1);
            WriteFallback(1);
            line("}");
            line("");
        }
    }

    void WriteFallback(int indent) {
        if (result_.kind == Type::Kind::Void) return;
        line(std::string(static_cast<size_t>(indent) * 4, ' ') + "return " + DefaultOf(result_) + ";");
    }

    void WriteStmt(const Stmt& stmt, int indent) {
        std::string pad(static_cast<size_t>(indent) * 4, ' ');
        switch (stmt.kind) {
            case Stmt::Kind::Block:
                line(pad + "{");
                scopes_.emplace_back();
                for (const Stmt& child : stmt.body) WriteStmt(child, indent + 1);
                scopes_.pop_back();
                line(pad + "}");
                return;
            case Stmt::Kind::If: {
                Typed cond = WriteExpr(stmt.expr);
                line(pad + "if " + TruthOf(cond) + " {");
                scopes_.emplace_back();
                for (const Stmt& child : stmt.body) WriteStmt(child, indent + 1);
                scopes_.pop_back();
                if (!stmt.elseOne.empty()) {
                    line(pad + "} else {");
                    scopes_.emplace_back();
                    for (const Stmt& child : stmt.elseOne) WriteStmt(child, indent + 1);
                    scopes_.pop_back();
                }
                line(pad + "}");
                return;
            }
            case Stmt::Kind::While: {
                Typed cond = WriteExpr(stmt.expr);
                line(pad + "while " + TruthOf(cond) + " {");
                scopes_.emplace_back();
                for (const Stmt& child : stmt.body) WriteStmt(child, indent + 1);
                scopes_.pop_back();
                line(pad + "}");
                return;
            }
            case Stmt::Kind::ForIn: {
                Typed source = WriteExpr(stmt.expr);
                if (source.type.kind != Type::Kind::List) Die("For In requires a list near line " + std::to_string(stmt.line));
                std::string holder = Fresh("__rx_src");
                std::string index = Fresh("__rx_i");
                line(pad + "{");
                line(pad + "    auto " + holder + " = " + source.code + ";");
                line(pad + "    for (int64_t " + index + " = 0; " + index + " < " + holder + ".Count(); " + index + " = " + index + " + 1) {");
                scopes_.emplace_back();
                scopes_.back()[stmt.name] = *source.type.elem;
                line(pad + "        " + Cpp(*source.type.elem) + " " + Safe(stmt.name) + " = rx::At(" + holder + ", " + index + ");");
                for (const Stmt& child : stmt.body) WriteStmt(child, indent + 2);
                scopes_.pop_back();
                line(pad + "    }");
                line(pad + "}");
                return;
            }
            case Stmt::Kind::Return:
                if (!stmt.hasExpr) line(pad + "return;");
                else if (stmt.expr.kind == Expr::Kind::Null) line(pad + "return " + NullOf(result_) + ";");
                else line(pad + "return " + WriteExpr(stmt.expr).code + ";");
                return;
            case Stmt::Kind::Break: line(pad + "break;"); return;
            case Stmt::Kind::Continue: line(pad + "continue;"); return;
            case Stmt::Kind::Decl: WriteDecl(stmt, pad); return;
            case Stmt::Kind::Assign: WriteAssign(stmt, pad); return;
            case Stmt::Kind::ExprStmt: line(pad + WriteExpr(stmt.expr).code + ";"); return;
        }
    }

    void WriteDecl(const Stmt& stmt, const std::string& pad) {
        Type type = stmt.inferred ? Type{} : Resolve(stmt.typeSpelling);
        std::string init;
        if (!stmt.hasExpr) init = DefaultOf(type);
        else if (stmt.expr.kind == Expr::Kind::Null) {
            if (stmt.inferred) type = MakeType(Type::Kind::String);
            init = NullOf(type);
        } else {
            Typed value = WriteExpr(stmt.expr);
            if (stmt.inferred) type = value.type;
            init = value.code;
        }
        if (type.kind == Type::Kind::Invalid) Die("could not infer '" + stmt.name + "'");
        scopes_.back()[stmt.name] = type;
        line(pad + Cpp(type) + " " + Safe(stmt.name) + " = " + init + ";");
    }

    void WriteAssign(const Stmt& stmt, const std::string& pad) {
        Typed left = WriteExpr(stmt.expr);
        if (left.lvalue.empty()) Die("not assignable near line " + std::to_string(stmt.line));
        std::string right = stmt.expr2.kind == Expr::Kind::Null ? NullOf(left.type) : WriteExpr(stmt.expr2).code;
        line(pad + left.lvalue + " = " + right + ";");
    }

    Typed WriteExpr(const Expr& expr) {
        switch (expr.kind) {
            case Expr::Kind::Int: return {MakeType(Type::Kind::Int), std::to_string(expr.integer) + "LL", ""};
            case Expr::Kind::Float: return {MakeType(Type::Kind::Float), std::to_string(expr.number), ""};
            case Expr::Kind::String: return {MakeType(Type::Kind::String), "rx::String::Lit(\"" + Escape(expr.text) + "\")", ""};
            case Expr::Kind::Char: return {MakeType(Type::Kind::Char), std::to_string(expr.integer) + "LL", ""};
            case Expr::Kind::Bool: return {MakeType(Type::Kind::Bool), expr.integer ? "true" : "false", ""};
            case Expr::Kind::Null: return {MakeType(Type::Kind::Invalid), "", ""};
            case Expr::Kind::This: return {MakeType(Type::Kind::Object, klass_->name), "this", ""};
            case Expr::Kind::Name: return WriteName(expr);
            case Expr::Kind::ListMake: {
                Type type = Resolve(expr.typeSpelling);
                return {type, Cpp(type) + "::Make()", ""};
            }
            case Expr::Kind::Unary: return WriteUnary(expr);
            case Expr::Kind::Binary: return WriteBinary(expr);
            case Expr::Kind::Member: return WriteMember(expr);
            case Expr::Kind::Call: return WriteCall(expr);
            case Expr::Kind::Index: return WriteIndex(expr);
            default: Die("empty expression near line " + std::to_string(expr.line));
        }
    }

    Typed WriteName(const Expr& expr) {
        if (const Type* type = Lookup(expr.text)) return {*type, Safe(expr.text), Safe(expr.text)};
        if (klass_ != nullptr) {
            if (const Field* field = FindField(*klass_, expr.text)) {
                Type type = Resolve(field->typeSpelling);
                std::string code = std::string("this->") + Safe(field->name);
                return {type, code, code};
            }
        }
        Die(expr.text + " is not defined near line " + std::to_string(expr.line));
    }

    Typed WriteUnary(const Expr& expr) {
        Typed inner = WriteExpr(expr.kids[0]);
        if (expr.text == "-") {
            Type type = inner.type.kind == Type::Kind::Float ? inner.type : MakeType(Type::Kind::Int);
            return {type, "(-(" + inner.code + "))", ""};
        }
        return {MakeType(Type::Kind::Bool), "(!" + TruthOf(inner) + ")", ""};
    }

    Typed WriteBinary(const Expr& expr) {
        if ((expr.text == "==" || expr.text == "!=") && (expr.kids[0].kind == Expr::Kind::Null || expr.kids[1].kind == Expr::Kind::Null)) {
            if (expr.kids[0].kind == Expr::Kind::Null && expr.kids[1].kind == Expr::Kind::Null) {
                return {MakeType(Type::Kind::Bool), expr.text == "==" ? "true" : "false", ""};
            }
            Typed value = WriteExpr(expr.kids[0].kind == Expr::Kind::Null ? expr.kids[1] : expr.kids[0]);
            std::string check = "false";
            if (value.type.kind == Type::Kind::String || value.type.kind == Type::Kind::List) check = "(" + value.code + ").IsNull()";
            else if (value.type.kind == Type::Kind::Object) check = "(" + value.code + " == nullptr)";
            if (expr.text == "!=") check = "(!" + check + ")";
            return {MakeType(Type::Kind::Bool), check, ""};
        }
        Typed left = WriteExpr(expr.kids[0]);
        Typed right = WriteExpr(expr.kids[1]);
        if (expr.text == "&&" || expr.text == "||") return {MakeType(Type::Kind::Bool), "(" + TruthOf(left) + " " + expr.text + " " + TruthOf(right) + ")", ""};
        if (expr.text == "+" && (left.type.kind == Type::Kind::String || right.type.kind == Type::Kind::String)) {
            return {MakeType(Type::Kind::String), "rx::Concat(" + Shown(left) + ", " + Shown(right) + ")", ""};
        }
        Type::Kind kind = Type::Kind::Int;
        if ((expr.text == "+" || expr.text == "-" || expr.text == "*" || expr.text == "/") &&
            (left.type.kind == Type::Kind::Float || right.type.kind == Type::Kind::Float)) {
            kind = Type::Kind::Float;
        }
        if (expr.text == "==" || expr.text == "!=" || expr.text == "<" || expr.text == ">" || expr.text == "<=" || expr.text == ">=") kind = Type::Kind::Bool;
        return {MakeType(kind), "((" + left.code + ") " + expr.text + " (" + right.code + "))", ""};
    }

    bool NamespaceName(const std::string& name) const {
        return name == "Files" || name == "Directory" || name == "ExitCode" || name == "String" || name == "Byte" || name == "Char";
    }

    Typed WriteMember(const Expr& expr) {
        if (expr.kids[0].kind == Expr::Kind::Name && Lookup(expr.kids[0].text) == nullptr && program_.enums.count(expr.kids[0].text)) {
            return EnumMember(expr.kids[0].text, expr.text);
        }
        if (expr.kids[0].kind == Expr::Kind::Name && Lookup(expr.kids[0].text) == nullptr && NamespaceName(expr.kids[0].text)) {
            return NamespaceValue(expr.kids[0].text, expr.text);
        }
        Typed base = WriteExpr(expr.kids[0]);
        if (base.type.kind == Type::Kind::Exec) {
            if (expr.text == "Code") return {MakeType(Type::Kind::Int), "(" + base.code + ").Code", "(" + base.code + ").Code"};
            if (expr.text == "Message") return {MakeType(Type::Kind::String), "(" + base.code + ").Message", "(" + base.code + ").Message"};
            if (expr.text == "Details") return {MakeType(Type::Kind::String), "(" + base.code + ").Details", "(" + base.code + ").Details"};
        }
        if (base.type.kind != Type::Kind::Object) Die("no field '" + expr.text + "' near line " + std::to_string(expr.line));
        const Field* field = FindField(program_.classes.at(base.type.name), expr.text);
        if (field == nullptr) Die(base.type.name + " has no field '" + expr.text + "'");
        Type type = Resolve(field->typeSpelling);
        std::string code = "(" + base.code + ")->" + Safe(field->name);
        return {type, code, code};
    }

    Typed EnumMember(const std::string& name, const std::string& member) const {
        for (const auto& item : program_.enums.at(name).members) {
            if (item.first == member) return {MakeType(Type::Kind::Enum, name), Safe(name) + "::" + Safe(member), ""};
        }
        Die(name + " has no member " + member);
    }

    Typed NamespaceValue(const std::string& owner, const std::string& member) const {
        static const std::map<std::string, const char*> codes = {
            {"Success", "rx::ExitCode::Success"}, {"SuccessfulExecution", "rx::ExitCode::SuccessfulExecution"},
            {"WarningExecution", "rx::ExitCode::WarningExecution"}, {"ErrorExecution", "rx::ExitCode::ErrorExecution"},
            {"FatalErrorExecution", "rx::ExitCode::FatalErrorExecution"}, {"InvalidArguments", "rx::ExitCode::InvalidArguments"},
            {"PermissionDenied", "rx::ExitCode::PermissionDenied"}, {"ResourceUnavailable", "rx::ExitCode::ResourceUnavailable"}};
        if (owner == "ExitCode") {
            auto found = codes.find(member);
            if (found == codes.end()) Die("unknown ExitCode." + member);
            return {MakeType(Type::Kind::Int), found->second, ""};
        }
        Die(owner + "." + member + " is not a value");
    }

    Typed WriteCall(const Expr& expr) {
        std::vector<const Expr*> args;
        for (size_t index = 1; index < expr.kids.size(); index++) args.push_back(&expr.kids[index]);
        if (expr.kids[0].kind == Expr::Kind::Member) return WriteMemberCall(expr.kids[0], args);
        if (expr.kids[0].kind != Expr::Kind::Name) Die("unsupported call near line " + std::to_string(expr.line));
        return WriteNamedCall(expr.kids[0].text, args, expr.line);
    }

    std::vector<Typed> Bind(const std::vector<const Expr*>& args, const std::vector<Param>& params) {
        std::vector<Typed> values;
        for (size_t index = 0; index < args.size(); index++) {
            if (args[index]->kind == Expr::Kind::Null) {
                Type type = index < params.size() ? Resolve(params[index].typeSpelling) : MakeType(Type::Kind::Object);
                values.push_back({type, NullOf(type), ""});
            } else {
                values.push_back(WriteExpr(*args[index]));
            }
        }
        return values;
    }

    std::string Join(const std::vector<Typed>& values) const {
        std::string text;
        for (size_t index = 0; index < values.size(); index++) {
            if (index) text += ", ";
            text += values[index].code;
        }
        return text;
    }

    Typed WriteNamedCall(const std::string& name, const std::vector<const Expr*>& args, int lineNo) {
        if (name == "Print") {
            std::string text = "rx::String::Lit(\"\")";
            bool first = true;
            for (const Expr* arg : args) {
                std::string shown = Shown(WriteExpr(*arg));
                text = first ? shown : "rx::Concat(rx::Concat(" + text + ", rx::String::Lit(\" \")), " + shown + ")";
                first = false;
            }
            return {MakeType(Type::Kind::Void), "rx::Print(" + text + ")", ""};
        }
        if (name == "ExecutionResult") {
            auto values = Bind(args, {});
            if (values.size() == 2) return {MakeType(Type::Kind::Exec), "rx::ExecutionResult::Make(" + values[0].code + ", " + values[1].code + ")", ""};
            if (values.size() == 3) return {MakeType(Type::Kind::Exec), "rx::ExecutionResult::Make(" + values[0].code + ", " + values[1].code + ", " + values[2].code + ")", ""};
            Die("ExecutionResult expects 2 or 3 arguments");
        }
        if (program_.classes.count(name)) {
            const Method* method = FindMethod(program_.classes.at(name), name, args.size(), true);
            if (method == nullptr && args.empty()) return {MakeType(Type::Kind::Object, name), Safe(name) + "::Make()", ""};
            if (method == nullptr) Die(name + " has no matching constructor near line " + std::to_string(lineNo));
            return {MakeType(Type::Kind::Object, name), Safe(name) + "::Make(" + Join(Bind(args, method->params)) + ")", ""};
        }
        const FuncInfo* func = nullptr;
        for (const FuncInfo& candidate : program_.functions) {
            if (candidate.name != name) continue;
            if (candidate.isExtern) {
                func = &candidate;
                continue;
            }
            if (!Included(candidate)) continue;
            if (candidate.params.size() == args.size()) {
                func = &candidate;
                break;
            }
        }
        if (func != nullptr && func->isExtern) {
            std::vector<Typed> values;
            for (const Expr* arg : args) values.push_back(WriteExpr(*arg));
            return {Resolve(func->returnSpelling), "rx::" + func->name + "(" + Join(values) + ")", ""};
        }
        if (func != nullptr) return {Resolve(func->returnSpelling), Safe(func->name) + "(" + Join(Bind(args, func->params)) + ")", ""};
        if (klass_ != nullptr) {
            if (const Method* method = FindMethod(*klass_, name, args.size(), false)) {
                std::string call = method->isExtern ? "rx::" + method->name : std::string("this->") + Safe(method->name);
                return {Resolve(method->returnSpelling), call + "(" + Join(Bind(args, method->params)) + ")", ""};
            }
        }
        Die("unknown function '" + name + "' near line " + std::to_string(lineNo));
    }

    Typed WriteMemberCall(const Expr& callee, const std::vector<const Expr*>& args) {
        if (callee.kids[0].kind == Expr::Kind::Name && Lookup(callee.kids[0].text) == nullptr && program_.enums.count(callee.kids[0].text)) {
            return EnumMember(callee.kids[0].text, callee.text);
        }
        if (callee.kids[0].kind == Expr::Kind::Name && Lookup(callee.kids[0].text) == nullptr && NamespaceName(callee.kids[0].text)) {
            std::vector<Typed> values;
            for (const Expr* arg : args) values.push_back(WriteExpr(*arg));
            return NamespaceCall(callee.kids[0].text, callee.text, values);
        }
        Typed base = WriteExpr(callee.kids[0]);
        const std::string& member = callee.text;
        if (base.type.kind == Type::Kind::Enum && member == "ToInt") return {MakeType(Type::Kind::Int), "(" + base.code + ").ToInt()", ""};
        if ((base.type.kind == Type::Kind::Int || base.type.kind == Type::Kind::Char || base.type.kind == Type::Kind::Bool || base.type.kind == Type::Kind::Float) && member == "ToString") {
            return {MakeType(Type::Kind::String), Shown(base), ""};
        }
        if ((base.type.kind == Type::Kind::Int || base.type.kind == Type::Kind::Char) && member == "ToInt") return {MakeType(Type::Kind::Int), "(" + base.code + ")", ""};
        if (base.type.kind == Type::Kind::String) return StringCall(base, member, args);
        if (base.type.kind == Type::Kind::List) return ListCall(base, member, args);
        if (base.type.kind != Type::Kind::Object) Die("cannot call " + member);
        const Method* method = FindMethod(program_.classes.at(base.type.name), member, args.size(), false);
        if (method == nullptr) Die(base.type.name + "." + member + " is not defined");
        std::string call = method->isExtern ? "rx::" + method->name + "(" + Join(Bind(args, method->params)) + ")"
                                            : "(" + base.code + ")->" + MethodId(program_.classes.at(base.type.name), method->name) + "(" + Join(Bind(args, method->params)) + ")";
        return {Resolve(method->returnSpelling), call, ""};
    }

    Typed StringCall(const Typed& base, const std::string& member, const std::vector<const Expr*>& args) {
        std::vector<Typed> values;
        for (const Expr* arg : args) values.push_back(WriteExpr(*arg));
        std::string object = "(" + base.code + ")";
        if (member == "Length") return {MakeType(Type::Kind::Int), object + ".Length()", ""};
        if (member == "ToString") return base;
        if (member == "ToLower") return {base.type, object + ".ToLower()", ""};
        if (member == "ToUpper") return {base.type, object + ".ToUpper()", ""};
        if (member == "Trim") return {base.type, object + ".Trim()", ""};
        if (member == "ToInt") return {MakeType(Type::Kind::Int), object + ".ToInt()", ""};
        if (member == "GetChar") return {MakeType(Type::Kind::Char), object + ".GetChar(" + values.at(0).code + ")", ""};
        if (member == "StartsWith") return {MakeType(Type::Kind::Bool), object + ".StartsWith(" + values.at(0).code + ")", ""};
        if (member == "EndsWith") return {MakeType(Type::Kind::Bool), object + ".EndsWith(" + values.at(0).code + ")", ""};
        if (member == "Contains") return {MakeType(Type::Kind::Bool), object + ".Contains(" + values.at(0).code + ")", ""};
        if (member == "IndexOf") return {MakeType(Type::Kind::Int), object + ".IndexOf(" + values.at(0).code + ")", ""};
        if (member == "LastIndexOf") return {MakeType(Type::Kind::Int), object + ".LastIndexOf(" + values.at(0).code + ")", ""};
        if (member == "Replace") return {base.type, object + ".Replace(" + values.at(0).code + ", " + values.at(1).code + ")", ""};
        if (member == "Substring" && values.size() == 1) return {base.type, object + ".Substring(" + values[0].code + ")", ""};
        if (member == "Substring") return {base.type, object + ".Substring(" + values.at(0).code + ", " + values.at(1).code + ")", ""};
        if (member == "Split") {
            Type list = MakeType(Type::Kind::List);
            list.elem = std::make_shared<Type>(MakeType(Type::Kind::String));
            return {list, "rx::SplitString(" + base.code + ", " + values.at(0).code + ")", ""};
        }
        Die("String." + member + " is not supported by the bootstrap");
    }

    Typed ListCall(const Typed& base, const std::string& member, const std::vector<const Expr*>& args) {
        std::vector<Typed> values;
        for (const Expr* arg : args) values.push_back(WriteExpr(*arg));
        std::string object = "(" + (base.lvalue.empty() ? base.code : base.lvalue) + ")";
        if (member == "Count") return {MakeType(Type::Kind::Int), object + ".Count()", ""};
        if (member == "Add") return {MakeType(Type::Kind::Int), object + ".Add(" + values.at(0).code + ")", ""};
        if (member == "Truncate") return {MakeType(Type::Kind::Int), object + ".Truncate(" + values.at(0).code + ")", ""};
        Die("List." + member + " is not supported by the bootstrap");
    }

    Typed NamespaceCall(const std::string& owner, const std::string& member, const std::vector<Typed>& args) const {
        auto code = [&](size_t index) { return args.at(index).code; };
        if (owner == "ExitCode" && member == "Custom") return {MakeType(Type::Kind::Int), code(0), ""};
        if (owner == "String" && member == "From") return {MakeType(Type::Kind::String), "rx::String::From(" + code(0) + ")", ""};
        if (owner == "String" && member == "FromChars") return {MakeType(Type::Kind::String), "rx::FromChars(" + code(0) + ")", ""};
        if ((owner == "Byte" || owner == "Char") && member == "From") return {MakeType(Type::Kind::Char), "(" + code(0) + ")", ""};
        if (owner == "Files" && member == "ReadText") return {MakeType(Type::Kind::String), "rx::Files::ReadText(" + code(0) + ")", ""};
        if (owner == "Files" && member == "WriteText") return {MakeType(Type::Kind::Void), "rx::Files::WriteText(" + code(0) + ", " + code(1) + ")", ""};
        if (owner == "Files" && member == "WriteBytes") return {MakeType(Type::Kind::Void), "rx::Files::WriteBytes(" + code(0) + ", " + code(1) + ")", ""};
        if (owner == "Files" && member == "Delete") return {MakeType(Type::Kind::Void), "rx::Files::Delete(" + code(0) + ")", ""};
        if (owner == "Files" && member == "Exists") return {MakeType(Type::Kind::Bool), "rx::Files::Exists(" + code(0) + ")", ""};
        if (owner == "Files" && member == "Name") return {MakeType(Type::Kind::String), "rx::Files::Name(" + code(0) + ")", ""};
        if (owner == "Files" && member == "Join") return {MakeType(Type::Kind::String), "rx::Files::Join(" + code(0) + ", " + code(1) + ")", ""};
        if (owner == "Directory" && member == "Names") {
            Type list = MakeType(Type::Kind::List);
            list.elem = std::make_shared<Type>(MakeType(Type::Kind::String));
            return {list, "rx::Directory::Names(" + code(0) + ")", ""};
        }
        if (owner == "Directory" && member == "Files") {
            Type list = MakeType(Type::Kind::List);
            list.elem = std::make_shared<Type>(MakeType(Type::Kind::String));
            return {list, "rx::Directory::Files(" + code(0) + ")", ""};
        }
        if (owner == "Directory" && member == "Exists") return {MakeType(Type::Kind::Bool), "rx::Directory::Exists(" + code(0) + ")", ""};
        if (owner == "Directory" && member == "Create") return {MakeType(Type::Kind::Void), "rx::Directory::Create(" + code(0) + ")", ""};
        if (owner == "Directory" && member == "Delete") return {MakeType(Type::Kind::Void), "rx::Directory::Delete(" + code(0) + ")", ""};
        if (owner == "ExitCode") return NamespaceValue(owner, member);
        Die("unknown " + owner + "." + member);
    }

    Typed WriteIndex(const Expr& expr) {
        Typed base = WriteExpr(expr.kids[0]);
        Typed index = WriteExpr(expr.kids[1]);
        if (base.type.kind != Type::Kind::List) Die("indexed value is not a list near line " + std::to_string(expr.line));
        if (!base.lvalue.empty()) {
            std::string code = "rx::At(" + base.lvalue + ", " + index.code + ")";
            return {*base.type.elem, code, code};
        }
        return {*base.type.elem, "rx::AtValue(" + base.code + ", " + index.code + ")", ""};
    }

    void WriteMain() {
        const FuncInfo* entry = nullptr;
        for (const FuncInfo& func : program_.functions) {
            if (func.name == "Main" && Included(func)) entry = &func;
        }
        if (entry == nullptr) Die(entry_ + " has no Main");
        line("int main(int argc, char** argv) {");
        line("    rx::List<rx::String> args = rx::List<rx::String>::Make();");
        line("    for (int index = 1; index < argc; index = index + 1) args.Add(rx::String::Lit(argv[index]));");
        if (Resolve(entry->returnSpelling).kind == Type::Kind::Exec) {
            line("    rx::ExecutionResult result = Main(args);");
            line("    return static_cast<int>(result.Code);");
        } else {
            line("    return static_cast<int>(Main(args));");
        }
        line("}");
    }
};
