#include "Compose.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Value {
    enum class Kind { Empty, Bool, Number, Text, List, Record };
    Kind kind = Kind::Empty;
    bool flag = false;
    double number = 0;
    std::string text;
    std::vector<Value> list;
    std::unordered_map<std::string, Value> record;
};

struct Scope {
    const Scope* parent = nullptr;
    std::unordered_map<std::string, Value> values;
};

struct Component {
    std::string name;
    std::vector<RoboXMLNode> body;
};

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

std::string Attr(const RoboXMLNode& node, const char* key) {
    for (const auto& entry : node.attrs) {
        if (Lower(entry.first) == key) return entry.second;
    }
    return {};
}

bool Control(const std::string& name) {
    return name == "import" || name == "component" || name == "data" || name == "item" || name == "for" || name == "if" ||
           name == "else" || name == "let" || name == "slot";
}

const Value* Find(const Scope* scope, const std::string& name) {
    const std::string key = Lower(name);
    for (const Scope* cursor = scope; cursor != nullptr; cursor = cursor->parent) {
        const auto it = cursor->values.find(key);
        if (it != cursor->values.end()) return &it->second;
    }
    return nullptr;
}

void Bind(Scope& scope, const std::string& name, Value value) {
    scope.values[Lower(name)] = std::move(value);
}

std::string ReadFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string Trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\n' || text[begin] == '\r')) ++begin;
    std::size_t end = text.size();
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\n' || text[end - 1] == '\r')) --end;
    return text.substr(begin, end - begin);
}

bool IsInt(double value) {
    return std::fabs(value - std::round(value)) < 0.000001;
}

std::string Stringify(const Value& value) {
    if (value.kind == Value::Kind::Bool) return value.flag ? "true" : "false";
    if (value.kind == Value::Kind::Number) {
        if (IsInt(value.number)) return std::to_string(static_cast<long long>(std::llround(value.number)));
        std::ostringstream out;
        out << value.number;
        return out.str();
    }
    if (value.kind == Value::Kind::Text) return value.text;
    if (value.kind == Value::Kind::List) {
        std::string out;
        for (const Value& item : value.list) {
            if (!out.empty()) out += ", ";
            out += Stringify(item);
        }
        return out;
    }
    if (value.kind == Value::Kind::Record) {
        const auto name = value.record.find("name");
        if (name != value.record.end()) return Stringify(name->second);
    }
    return {};
}

bool Truthy(const Value& value) {
    if (value.kind == Value::Kind::Bool) return value.flag;
    if (value.kind == Value::Kind::Number) return value.number != 0;
    if (value.kind == Value::Kind::Text) {
        const std::string text = Lower(value.text);
        return !text.empty() && text != "false" && text != "0" && text != "no" && text != "off";
    }
    if (value.kind == Value::Kind::List) return !value.list.empty();
    if (value.kind == Value::Kind::Record) return !value.record.empty();
    return false;
}

double NumberOf(const Value& value) {
    if (value.kind == Value::Kind::Number) return value.number;
    if (value.kind == Value::Kind::Bool) return value.flag ? 1 : 0;
    if (value.kind == Value::Kind::Text) {
        double number = 0;
        std::size_t index = 0;
        int sign = 1;
        const std::string& text = value.text;
        if (!text.empty() && text[0] == '-') {
            sign = -1;
            index = 1;
        }
        bool digit = false;
        while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
            digit = true;
            number = number * 10 + (text[index] - '0');
            ++index;
        }
        if (index < text.size() && text[index] == '.') {
            ++index;
            double place = 0.1;
            while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
                digit = true;
                number += (text[index] - '0') * place;
                place *= 0.1;
                ++index;
            }
        }
        if (digit && index == text.size()) return sign * number;
    }
    return 0;
}

bool BothNumeric(const Value& left, const Value& right) {
    const auto numeric = [](const Value& value) {
        if (value.kind == Value::Kind::Number || value.kind == Value::Kind::Bool) return true;
        if (value.kind != Value::Kind::Text || value.text.empty()) return false;
        std::size_t index = value.text[0] == '-' ? 1 : 0;
        bool digit = false;
        bool dot = false;
        for (; index < value.text.size(); ++index) {
            if (value.text[index] >= '0' && value.text[index] <= '9') digit = true;
            else if (value.text[index] == '.' && !dot) dot = true;
            else return false;
        }
        return digit;
    };
    return numeric(left) && numeric(right);
}

Value TextValue(std::string text) {
    Value value;
    value.kind = Value::Kind::Text;
    value.text = std::move(text);
    return value;
}

Value NumberValue(double number) {
    Value value;
    value.kind = Value::Kind::Number;
    value.number = number;
    return value;
}

Value BoolValue(bool flag) {
    Value value;
    value.kind = Value::Kind::Bool;
    value.flag = flag;
    return value;
}

struct Parser {
    const std::string& text;
    const Scope* scope = nullptr;
    std::size_t index = 0;

    void Skip() {
        while (index < text.size() && (text[index] == ' ' || text[index] == '\t')) ++index;
    }
    bool Take(char ch) {
        Skip();
        if (index < text.size() && text[index] == ch) {
            ++index;
            return true;
        }
        return false;
    }
    bool TakeWord(const char* word) {
        Skip();
        const std::size_t length = std::char_traits<char>::length(word);
        if (text.compare(index, length, word) != 0) return false;
        const std::size_t after = index + length;
        if (after < text.size()) {
            const char ch = text[after];
            if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_') return false;
        }
        index = after;
        return true;
    }

    Value Parse() {
        Value value = Or();
        Skip();
        return value;
    }
    Value Or() {
        Value left = And();
        while (true) {
            Skip();
            if (text.compare(index, 2, "||") != 0) break;
            index += 2;
            const Value right = And();
            left = BoolValue(Truthy(left) || Truthy(right));
        }
        return left;
    }
    Value And() {
        Value left = Compare();
        while (true) {
            Skip();
            if (text.compare(index, 2, "&&") != 0) break;
            index += 2;
            const Value right = Compare();
            left = BoolValue(Truthy(left) && Truthy(right));
        }
        return left;
    }
    Value Compare() {
        Value left = Sum();
        while (true) {
            Skip();
            std::string op;
            if (text.compare(index, 2, "==") == 0 || text.compare(index, 2, "!=") == 0 || text.compare(index, 2, ">=") == 0 ||
                text.compare(index, 2, "<=") == 0) {
                op = text.substr(index, 2);
                index += 2;
            } else if (index < text.size() && (text[index] == '>' || text[index] == '<')) {
                op = text.substr(index, 1);
                ++index;
            } else {
                break;
            }
            const Value right = Sum();
            bool same = false;
            if (BothNumeric(left, right)) same = NumberOf(left) == NumberOf(right);
            else same = Stringify(left) == Stringify(right);
            bool result = false;
            if (op == "==") result = same;
            else if (op == "!=") result = !same;
            else if (BothNumeric(left, right)) {
                const double a = NumberOf(left);
                const double b = NumberOf(right);
                result = op == ">" ? a > b : op == "<" ? a < b : op == ">=" ? a >= b : a <= b;
            }
            left = BoolValue(result);
        }
        return left;
    }
    Value Sum() {
        Value left = Product();
        while (true) {
            Skip();
            if (index >= text.size() || (text[index] != '+' && text[index] != '-')) break;
            const char op = text[index++];
            const Value right = Product();
            if (op == '+' && !BothNumeric(left, right)) left = TextValue(Stringify(left) + Stringify(right));
            else left = NumberValue(op == '+' ? NumberOf(left) + NumberOf(right) : NumberOf(left) - NumberOf(right));
        }
        return left;
    }
    Value Product() {
        Value left = Unary();
        while (true) {
            Skip();
            if (index >= text.size() || (text[index] != '*' && text[index] != '/')) break;
            const char op = text[index++];
            const Value right = Unary();
            if (op == '*') left = NumberValue(NumberOf(left) * NumberOf(right));
            else if (NumberOf(right) == 0) {
                std::cerr << "warning: division by zero in a .rxu expression was skipped.\n";
                left = NumberValue(0);
            } else {
                left = NumberValue(NumberOf(left) / NumberOf(right));
            }
        }
        return left;
    }
    Value Unary() {
        Skip();
        if (Take('!')) return BoolValue(!Truthy(Unary()));
        if (Take('-')) return NumberValue(-NumberOf(Unary()));
        return Primary();
    }
    Value Primary() {
        Skip();
        if (Take('(')) {
            Value value = Or();
            Take(')');
            return value;
        }
        if (TakeWord("true")) return BoolValue(true);
        if (TakeWord("false")) return BoolValue(false);
        if (index < text.size() && (text[index] == '\'' || text[index] == '"')) {
            const char quote = text[index++];
            std::string literal;
            while (index < text.size() && text[index] != quote) {
                if (text[index] == '\\' && index + 1 < text.size()) ++index;
                literal.push_back(text[index++]);
            }
            if (index < text.size()) ++index;
            return TextValue(literal);
        }
        if (index < text.size() && ((text[index] >= '0' && text[index] <= '9') || text[index] == '.')) {
            const std::size_t start = index;
            while (index < text.size() && ((text[index] >= '0' && text[index] <= '9') || text[index] == '.')) ++index;
            return NumberValue(NumberOf(TextValue(text.substr(start, index - start))));
        }
        if (index < text.size() && ((text[index] >= 'A' && text[index] <= 'Z') || (text[index] >= 'a' && text[index] <= 'z') || text[index] == '_')) {
            const std::size_t start = index;
            while (index < text.size() && ((text[index] >= 'A' && text[index] <= 'Z') || (text[index] >= 'a' && text[index] <= 'z') ||
                                          (text[index] >= '0' && text[index] <= '9') || text[index] == '_')) {
                ++index;
            }
            const std::string name = text.substr(start, index - start);
            const Value* value = Find(scope, name);
            Value current = value != nullptr ? *value : Value{};
            if (value == nullptr) std::cerr << "warning: '" << name << "' was not found in this .rxu scope and was left empty.\n";
            while (Take('.')) {
                Skip();
                const std::size_t fieldStart = index;
                while (index < text.size() && ((text[index] >= 'A' && text[index] <= 'Z') || (text[index] >= 'a' && text[index] <= 'z') ||
                                              (text[index] >= '0' && text[index] <= '9') || text[index] == '_')) {
                    ++index;
                }
                const std::string field = Lower(text.substr(fieldStart, index - fieldStart));
                if (current.kind == Value::Kind::Record) {
                    const auto it = current.record.find(field);
                    current = it == current.record.end() ? Value{} : it->second;
                } else {
                    current = Value{};
                }
            }
            return current;
        }
        return Value{};
    }
};

Value Eval(const std::string& text, const Scope& scope) {
    Parser parser{text, &scope, 0};
    return parser.Parse();
}

std::string Substitute(const std::string& text, const Scope& scope) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '{') {
            out.push_back(text[i]);
            continue;
        }
        int depth = 1;
        std::size_t end = i + 1;
        while (end < text.size() && depth > 0) {
            if (text[end] == '{') ++depth;
            else if (text[end] == '}') --depth;
            if (depth > 0) ++end;
        }
        if (depth != 0) {
            out.push_back(text[i]);
            continue;
        }
        out += Stringify(Eval(text.substr(i + 1, end - i - 1), scope));
        i = end;
    }
    return out;
}

Value EvalText(const std::string& text, const Scope& scope) {
    const std::string trimmed = Trim(text);
    if (trimmed.size() >= 2 && trimmed.front() == '{' && trimmed.back() == '}') {
        int depth = 0;
        bool whole = true;
        for (std::size_t i = 0; i < trimmed.size(); ++i) {
            if (trimmed[i] == '{') ++depth;
            else if (trimmed[i] == '}') --depth;
            if (depth == 0 && i + 1 != trimmed.size()) whole = false;
        }
        if (whole) return Eval(trimmed.substr(1, trimmed.size() - 2), scope);
    }
    return TextValue(Substitute(text, scope));
}

std::vector<Value> AsList(const Value& value) {
    if (value.kind == Value::Kind::List) return value.list;
    if (value.kind == Value::Kind::Empty) return {};
    if (value.kind == Value::Kind::Text) {
        std::vector<Value> items;
        std::string token;
        for (std::size_t i = 0; i <= value.text.size(); ++i) {
            if (i == value.text.size() || value.text[i] == ',') {
                const std::string part = Trim(token);
                if (!part.empty()) items.push_back(TextValue(part));
                token.clear();
            } else {
                token.push_back(value.text[i]);
            }
        }
        return items;
    }
    return {value};
}

using Components = std::unordered_map<std::string, Component>;

std::vector<RoboXMLNode> ExpandList(const std::vector<RoboXMLNode>& nodes, Scope& scope, const Components& components,
                                    const std::vector<RoboXMLNode>* slot, int depth);

std::vector<RoboXMLNode> ExpandNode(const RoboXMLNode& node, Scope& scope, const Components& components,
                                    const std::vector<RoboXMLNode>* slot, int depth) {
    if (depth > 32) {
        std::cerr << "warning: .rxu component nesting stopped at 32 levels.\n";
        return {};
    }
    const std::string kind = Lower(node.name);
    if (kind == "import" || kind == "component" || kind == "item") return {};
    if (kind == "data") {
        const std::string name = Attr(node, "name");
        if (name.empty()) {
            std::cerr << "warning: Data has no Name and was skipped.\n";
            return {};
        }
        if (node.children.empty()) {
            Bind(scope, name, EvalText(Attr(node, "value"), scope));
            return {};
        }
        Value list;
        list.kind = Value::Kind::List;
        for (const RoboXMLNode& child : node.children) {
            Value record;
            record.kind = Value::Kind::Record;
            record.record["type"] = TextValue(child.name);
            for (const auto& entry : child.attrs) {
                record.record[Lower(entry.first)] = EvalText(entry.second, scope);
            }
            list.list.push_back(std::move(record));
        }
        Bind(scope, name, std::move(list));
        return {};
    }
    if (kind == "let") {
        const std::string name = Attr(node, "name");
        if (name.empty()) {
            std::cerr << "warning: Let has no Name and was skipped.\n";
            return {};
        }
        Scope inner;
        inner.parent = &scope;
        Bind(inner, name, EvalText(Attr(node, "value"), scope));
        return ExpandList(node.children, inner, components, slot, depth + 1);
    }
    if (kind == "for") {
        const std::string each = Attr(node, "each");
        if (each.empty()) {
            std::cerr << "warning: For has no Each and was skipped.\n";
            return {};
        }
        const std::string indexName = Attr(node, "index");
        std::vector<RoboXMLNode> produced;
        int count = 0;
        const auto run = [&](const Value& item, int index) {
            if (count >= 1000) return;
            ++count;
            Scope inner;
            inner.parent = &scope;
            Bind(inner, each, item);
            if (!indexName.empty()) Bind(inner, indexName, NumberValue(index));
            const auto part = ExpandList(node.children, inner, components, slot, depth + 1);
            produced.insert(produced.end(), part.begin(), part.end());
        };
        if (!Attr(node, "from").empty() || !Attr(node, "to").empty()) {
            const int from = static_cast<int>(std::llround(NumberOf(EvalText(Attr(node, "from"), scope))));
            const int to = static_cast<int>(std::llround(NumberOf(EvalText(Attr(node, "to"), scope))));
            int step = Attr(node, "step").empty() ? (to >= from ? 1 : -1) : static_cast<int>(std::llround(NumberOf(EvalText(Attr(node, "step"), scope))));
            if (step == 0) {
                std::cerr << "warning: For Step is 0 and the loop was skipped.\n";
                return {};
            }
            int index = 0;
            for (int value = from; step > 0 ? value <= to : value >= to; value += step) {
                run(NumberValue(value), index++);
                if (count >= 1000) break;
            }
        } else {
            const std::vector<Value> items = AsList(EvalText(Attr(node, "in"), scope));
            for (std::size_t index = 0; index < items.size() && count < 1000; ++index) run(items[index], static_cast<int>(index));
        }
        if (count >= 1000) std::cerr << "warning: a .rxu For loop stopped at 1000 items.\n";
        return produced;
    }
    if (kind == "if") {
        const bool yes = Truthy(EvalText(Attr(node, "test"), scope));
        std::vector<RoboXMLNode> produced;
        for (const RoboXMLNode& child : node.children) {
            if (Lower(child.name) == "else") {
                if (!yes) {
                    const auto part = ExpandList(child.children, scope, components, slot, depth + 1);
                    produced.insert(produced.end(), part.begin(), part.end());
                }
            } else if (yes) {
                const auto part = ExpandNode(child, scope, components, slot, depth + 1);
                produced.insert(produced.end(), part.begin(), part.end());
            }
        }
        return produced;
    }
    if (kind == "else") return {};
    if (kind == "slot") {
        if (slot == nullptr) return {};
        return ExpandList(*slot, scope, components, nullptr, depth + 1);
    }
    const auto component = components.find(kind);
    if (component != components.end()) {
        Scope inner;
        inner.parent = &scope;
        for (const auto& entry : node.attrs) Bind(inner, entry.first, EvalText(entry.second, scope));
        auto produced = ExpandList(component->second.body, inner, components, &node.children, depth + 1);
        if (!produced.empty()) {
            for (const auto& entry : node.attrs) {
                const std::string key = Lower(entry.first);
                const bool forward = key == "name" || key == "id" || key.rfind("on", 0) == 0;
                if (!forward) continue;
                bool present = false;
                for (const auto& have : produced[0].attrs) {
                    if (Lower(have.first) == key) present = true;
                }
                if (!present) produced[0].attrs[entry.first] = Stringify(EvalText(entry.second, scope));
            }
        }
        return produced;
    }
    RoboXMLNode out;
    out.name = node.name;
    out.text = Substitute(node.text, scope);
    for (const auto& entry : node.attrs) out.attrs[entry.first] = Substitute(entry.second, scope);
    out.children = ExpandList(node.children, scope, components, slot, depth + 1);
    return {std::move(out)};
}

std::vector<RoboXMLNode> ExpandList(const std::vector<RoboXMLNode>& nodes, Scope& scope, const Components& components,
                                    const std::vector<RoboXMLNode>* slot, int depth) {
    std::vector<RoboXMLNode> produced;
    for (const RoboXMLNode& node : nodes) {
        const auto part = ExpandNode(node, scope, components, slot, depth);
        produced.insert(produced.end(), part.begin(), part.end());
    }
    return produced;
}

void Collect(const RoboXMLNode& node, const fs::path& base, Components& components, std::unordered_set<std::string>& loading,
             std::unordered_set<std::string>& done) {
    const std::string kind = Lower(node.name);
    if (kind == "import") {
        const std::string source = Attr(node, "source");
        if (source.empty()) {
            std::cerr << "warning: Import has no Source and was skipped.\n";
        } else {
            fs::path path = fs::path(source);
            if (path.is_relative()) path = base / path;
            std::error_code error;
            const fs::path absolute = fs::weakly_canonical(path, error);
            const std::string key = error ? path.string() : absolute.string();
            if (done.count(key) == 0) {
                if (loading.count(key) != 0) {
                    std::cerr << "warning: Import cycle at " << key << " was skipped.\n";
                } else {
                    loading.insert(key);
                    RoboXMLDocument document;
                    const std::string text = ReadFile(path);
                    if (text.empty() || !RoboXMLParse(text, document)) {
                        std::cerr << "warning: Import '" << source << "' could not be read and was skipped.\n";
                    } else {
                        done.insert(key);
                        Collect(document.root, absolute.parent_path(), components, loading, done);
                    }
                    loading.erase(key);
                }
            }
        }
    } else if (kind == "component") {
        const std::string name = Attr(node, "name");
        if (name.empty()) {
            std::cerr << "warning: Component has no Name and was skipped.\n";
        } else {
            const std::string key = Lower(name);
            static const char* reserved[] = {
                "window", "screen", "page", "board", "column", "row", "grid", "stack", "container", "button", "anchor", "text",
                "textfield", "image", "icon", "heading", "subhead", "body", "paragraph", "caption", "eyebrow", "label",
                "span", "iconbutton", "switch", "switchgroup", "switchitem", "themetoggle", "themeicontoggle",
                "textfield", "textarea", "passwordreveal", "confirmaction", "select", "option", "countryselect",
                "segmentedcontrol", "segmentedtoolbar",
                "segment", "scrollbar", "svg", "at", "slot", "for", "if", "else", "let", "data", "import", "component"};
            bool blocked = false;
            for (const char* item : reserved) {
                if (key == item) blocked = true;
            }
            if (blocked) {
                std::cerr << "warning: component '" << name << "' uses a built-in name and was skipped.\n";
            } else {
                if (components.count(key) != 0) std::cerr << "warning: component '" << name << "' replaced an earlier component of the same name.\n";
                components[key] = Component{name, node.children};
            }
        }
    }
    for (const RoboXMLNode& child : node.children) Collect(child, base, components, loading, done);
}

}  // namespace

void RoboUICompose(RoboXMLNode& root, const std::string& documentPath) {
    const fs::path base = documentPath.empty() ? fs::current_path() : fs::path(documentPath).parent_path();
    Components components;
    std::unordered_set<std::string> loading;
    std::unordered_set<std::string> done;
    Collect(root, base, components, loading, done);
    Scope scope;
    root.children = ExpandList(root.children, scope, components, nullptr, 0);
}
