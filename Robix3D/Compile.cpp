#include "Compile.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace Robix::D3 {
namespace {

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

std::uint32_t gSeed = 1337;

double NextRand() {
    gSeed = gSeed * 1664525u + 1013904223u;
    return (gSeed >> 8) / 16777216.0;
}

std::string Trim(std::string text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
    return text;
}

std::string Attr(const RoboXMLNode& node, const char* key) {
    return RoboXMLAttr(node, key);
}

std::string ReadFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool IsUiTag(const std::string& name) {
    static const char* names[] = {
        "robixui", "window", "screen", "page", "board", "column", "row", "grid", "stack", "container", "button",
        "anchor", "text", "textfield", "image", "icon", "heading", "subhead", "body", "paragraph", "caption",
        "eyebrow", "label", "span", "iconbutton", "switch", "switchgroup", "switchitem", "themetoggle",
        "themeicontoggle", "textfield", "textarea", "passwordreveal", "confirmaction", "select", "option",
        "countryselect", "segmentedcontrol",
        "segmentedtoolbar", "segment", "scrollbar", "svg", "view", "panel", "list", "table", "menu", "dialog",
        "tabs", "checkbox", "radiobutton", "progressbar", "chart", "canvas", "template", "slot", "style", "at",
        "data", "if", "else"};
    for (const char* item : names) {
        if (name == item) return true;
    }
    return false;
}

bool IsSceneTag(const std::string& name) {
    static const char* names[] = {
        "robix3d", "scene", "camera", "lighting", "ambient", "ambientlight", "directional", "directionallight",
        "point", "pointlight", "spot", "spotlight", "hemisphere", "hemispherelight", "arealight", "rectarea",
        "environment", "object", "group", "mesh", "geometry", "material", "transform", "animation", "track",
        "keyframe", "loop", "timeline", "sequence", "play", "wait", "physics", "physicsbody", "collider", "mass",
        "gravity", "restitution", "friction", "generator", "parameters", "number", "foreach", "instance",
        "component", "import", "shader", "uniform", "input", "surface", "fresnel", "noise", "gradient", "output", "glsl",
        "shape", "look", "wobble", "fade", "color", "if", "else", "random", "repeat", "set", "discard",
        "vertex", "fragment", "interaction", "on", "action", "binding", "state", "controls", "orbitcontrols",
        "mapcontrols", "flycontrols", "ui", "colorpicker", "slider", "toggle", "note", "ground", "renderer",
        "constraint", "override", "modifier", "model", "curve", "helper", "fog", "sky", "postprocessing", "bloom",
        "text3d", "vertices", "indices", "let", "view3d", "scene3d", "points", "sprite", "line", "stop", "field"};
    for (const char* item : names) {
        if (name == item) return true;
    }
    return false;
}

struct Scope {
    std::unordered_map<std::string, double> numbers;
    const Scope* parent = nullptr;
};

std::optional<double> Lookup(const Scope& scope, const std::string& name) {
    const auto found = scope.numbers.find(name);
    if (found != scope.numbers.end()) return found->second;
    if (scope.parent) return Lookup(*scope.parent, name);
    return std::nullopt;
}

class Expr {
 public:
    explicit Expr(const std::string& text) : text_(text) {}

    std::optional<double> Run(const Scope& scope, std::string& error) {
        scope_ = &scope;
        index_ = 0;
        error_.clear();
        const double value = ParseCompare();
        Skip();
        if (!error_.empty()) {
            error = error_;
            return std::nullopt;
        }
        if (index_ != text_.size()) {
            error = "could not read '" + text_ + "'";
            return std::nullopt;
        }
        return value;
    }

 private:
    void Skip() {
        while (index_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[index_]))) ++index_;
    }

    double Call(const std::string& name, const std::vector<double>& args) {
        const std::string fn = Lower(name);
        const auto need = [&](std::size_t count) {
            if (args.size() < count) {
                error_ = name + " needs more values";
                return false;
            }
            return true;
        };
        if (fn == "abs") return need(1) ? std::fabs(args[0]) : 0;
        if (fn == "min") return need(2) ? std::min(args[0], args[1]) : 0;
        if (fn == "max") return need(2) ? std::max(args[0], args[1]) : 0;
        if (fn == "floor") return need(1) ? std::floor(args[0]) : 0;
        if (fn == "ceil") return need(1) ? std::ceil(args[0]) : 0;
        if (fn == "round") return need(1) ? std::round(args[0]) : 0;
        if (fn == "sqrt") return need(1) ? std::sqrt(std::max(0.0, args[0])) : 0;
        if (fn == "sin") return need(1) ? std::sin(args[0]) : 0;
        if (fn == "cos") return need(1) ? std::cos(args[0]) : 0;
        if (fn == "pow") return need(2) ? std::pow(args[0], args[1]) : 0;
        if (fn == "clamp") return need(3) ? std::min(std::max(args[0], args[1]), args[2]) : 0;
        if (fn == "less") return need(2) ? (args[0] < args[1] ? 1 : 0) : 0;
        if (fn == "more") return need(2) ? (args[0] > args[1] ? 1 : 0) : 0;
        if (fn == "equal") return need(2) ? (args[0] == args[1] ? 1 : 0) : 0;
        if (fn == "between") return need(3) ? (args[0] >= args[1] && args[0] <= args[2] ? 1 : 0) : 0;
        if (fn == "random") {
            const double unit = NextRand();
            if (args.empty()) return unit;
            if (args.size() == 1) return unit * args[0];
            return args[0] + unit * (args[1] - args[0]);
        }
        if (fn == "randomint") {
            if (!need(2)) return 0;
            const int low = static_cast<int>(std::llround(args[0]));
            const int high = static_cast<int>(std::llround(args[1]));
            if (high < low) {
                error_ = "RandomInt max is below min";
                return 0;
            }
            const int span = high - low + 1;
            return low + std::min(span - 1, static_cast<int>(std::floor(NextRand() * span)));
        }
        if (fn == "pick") {
            if (args.size() < 2) {
                error_ = "Pick needs an index and at least one value";
                return 0;
            }
            const int pick = static_cast<int>(std::llround(args[0]));
            if (pick < 0 || pick >= static_cast<int>(args.size()) - 1) {
                error_ = "Pick index is outside the list";
                return 0;
            }
            return args[static_cast<std::size_t>(pick) + 1];
        }
        error_ = "unknown function '" + name + "'";
        return 0;
    }

    double ParseCompare() {
        const double value = ParseAdd();
        Skip();
        if (index_ + 1 < text_.size() && text_[index_] == '=' && text_[index_ + 1] == '=') {
            index_ += 2;
            return value == ParseAdd() ? 1 : 0;
        }
        if (index_ + 1 < text_.size() && text_[index_] == '!' && text_[index_ + 1] == '=') {
            index_ += 2;
            return value != ParseAdd() ? 1 : 0;
        }
        if (index_ + 1 < text_.size() && text_[index_] == '<' && text_[index_ + 1] == '=') {
            index_ += 2;
            return value <= ParseAdd() ? 1 : 0;
        }
        if (index_ + 1 < text_.size() && text_[index_] == '>' && text_[index_ + 1] == '=') {
            index_ += 2;
            return value >= ParseAdd() ? 1 : 0;
        }
        if (index_ < text_.size() && text_[index_] == '<') {
            ++index_;
            return value < ParseAdd() ? 1 : 0;
        }
        if (index_ < text_.size() && text_[index_] == '>') {
            ++index_;
            return value > ParseAdd() ? 1 : 0;
        }
        return value;
    }

    double ParseAdd() {
        double value = ParseMul();
        for (;;) {
            Skip();
            if (index_ < text_.size() && (text_[index_] == '+' || text_[index_] == '-')) {
                const char op = text_[index_++];
                const double right = ParseMul();
                value = op == '+' ? value + right : value - right;
            } else {
                break;
            }
        }
        return value;
    }

    double ParseMul() {
        double value = ParseUnary();
        for (;;) {
            Skip();
            if (index_ < text_.size() && (text_[index_] == '*' || text_[index_] == '/' || text_[index_] == '%')) {
                const char op = text_[index_++];
                const double right = ParseUnary();
                if ((op == '/' || op == '%') && right == 0.0) {
                    error_ = "division by zero";
                    return 0;
                }
                if (op == '*') value *= right;
                else if (op == '/') value /= right;
                else value = std::fmod(value, right);
            } else {
                break;
            }
        }
        return value;
    }

    double ParseUnary() {
        Skip();
        if (index_ < text_.size() && text_[index_] == '-') {
            ++index_;
            return -ParseUnary();
        }
        return ParsePrimary();
    }

    double ParsePrimary() {
        Skip();
        if (index_ >= text_.size()) {
            error_ = "missing value";
            return 0;
        }
        if (text_[index_] == '(') {
            ++index_;
            const double value = ParseAdd();
            Skip();
            if (index_ >= text_.size() || text_[index_] != ')') error_ = "missing )";
            else ++index_;
            return value;
        }
        if (std::isalpha(static_cast<unsigned char>(text_[index_])) || text_[index_] == '_') {
            const std::size_t start = index_;
            while (index_ < text_.size() && (std::isalnum(static_cast<unsigned char>(text_[index_])) || text_[index_] == '_')) ++index_;
            const std::string name = text_.substr(start, index_ - start);
            Skip();
            if (index_ < text_.size() && text_[index_] == '(') {
                ++index_;
                std::vector<double> args;
                Skip();
                if (index_ >= text_.size() || text_[index_] != ')') {
                    for (;;) {
                        args.push_back(ParseCompare());
                        Skip();
                        if (index_ < text_.size() && text_[index_] == ',') {
                            ++index_;
                            continue;
                        }
                        break;
                    }
                }
                if (index_ >= text_.size() || text_[index_] != ')') error_ = "missing )";
                else ++index_;
                return Call(name, args);
            }
            const auto value = Lookup(*scope_, name);
            if (!value) {
                error_ = "unknown value '" + name + "'";
                return 0;
            }
            return *value;
        }
        const std::size_t start = index_;
        if (text_[index_] == '.') ++index_;
        while (index_ < text_.size() && (std::isdigit(static_cast<unsigned char>(text_[index_])) || text_[index_] == '.')) ++index_;
        if (start == index_) {
            error_ = "expected a number";
            return 0;
        }
        try {
            return std::stod(text_.substr(start, index_ - start));
        } catch (...) {
            error_ = "bad number";
            return 0;
        }
    }

    const std::string& text_;
    std::size_t index_ = 0;
    const Scope* scope_ = nullptr;
    std::string error_;
};

std::string Substitute(const std::string& text, const Scope& scope, std::vector<std::string>& errors) {
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] != '{') {
            out.push_back(text[i++]);
            continue;
        }
        const std::size_t end = text.find('}', i + 1);
        if (end == std::string::npos) {
            errors.push_back("unclosed { in '" + text + "'");
            return text;
        }
        const std::string inner = Trim(text.substr(i + 1, end - i - 1));
        bool reference = !inner.empty() && (std::isalpha(static_cast<unsigned char>(inner.front())) || inner.front() == '_');
        if (reference) {
            for (char ch : inner) {
                if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') reference = false;
            }
        }
        if (reference && !Lookup(scope, inner)) {
            out.append(text, i, end - i + 1);
            i = end + 1;
            continue;
        }
        std::string error;
        const auto value = Expr(inner).Run(scope, error);
        if (!value) {
            errors.push_back(error.empty() ? "bad expression" : error);
            out += "0";
        } else {
            std::ostringstream printed;
            printed.setf(std::ios::fmtflags(0), std::ios::floatfield);
            printed.precision(9);
            printed << *value;
            out += printed.str();
        }
        i = end + 1;
    }
    return out;
}

struct ComponentDef {
    std::string name;
    std::string extends;
    std::vector<RoboXMLNode> children;
    fs::path directory;
};

using Components = std::unordered_map<std::string, ComponentDef>;

void ApplyOverrides(std::vector<RoboXMLNode>& nodes, const std::vector<RoboXMLNode>& overrides, std::vector<std::string>& errors);

RoboXMLNode* FindNamed(std::vector<RoboXMLNode>& nodes, const std::string& name) {
    for (RoboXMLNode& node : nodes) {
        if (Lower(Attr(node, "name")) == name) return &node;
        if (RoboXMLNode* child = FindNamed(node.children, name)) return child;
    }
    return nullptr;
}

void SetOverride(RoboXMLNode& node, const std::vector<std::string>& parts, std::size_t index, const std::string& value, std::vector<std::string>& errors) {
    if (index >= parts.size()) return;
    if (index + 1 == parts.size()) {
        node.attrs[parts[index]] = value;
        return;
    }
    const std::string want = parts[index];
    for (RoboXMLNode& child : node.children) {
        if (Lower(child.name) == want || Lower(Attr(child, "name")) == want) {
            SetOverride(child, parts, index + 1, value, errors);
            return;
        }
    }
    node.attrs[want] = value;
    if (parts.size() > index + 1) errors.push_back("override could not find '" + want + "'");
}

void ApplyOverrides(std::vector<RoboXMLNode>& nodes, const std::vector<RoboXMLNode>& overrides, std::vector<std::string>& errors) {
    for (const RoboXMLNode& item : overrides) {
        if (Lower(item.name) != "override") continue;
        const std::string target = Lower(Attr(item, "target"));
        const std::string value = Attr(item, "value");
        if (target.empty()) {
            errors.push_back("Override needs Target");
            continue;
        }
        std::vector<std::string> parts;
        std::string token;
        for (char ch : target) {
            if (ch == '.') {
                if (!token.empty()) parts.push_back(token);
                token.clear();
            } else {
                token.push_back(ch);
            }
        }
        if (!token.empty()) parts.push_back(token);
        RoboXMLNode* node = FindNamed(nodes, parts[0]);
        if (node == nullptr) {
            errors.push_back("override target '" + target + "' was not found");
            continue;
        }
        if (parts.size() == 1) {
            errors.push_back("override target '" + target + "' needs a property");
            continue;
        }
        SetOverride(*node, parts, 1, value, errors);
    }
}

std::vector<RoboXMLNode> ComponentBody(const std::string& key, const Components& components, std::unordered_set<std::string>& stack,
                                      std::vector<std::string>& errors) {
    const auto found = components.find(key);
    if (found == components.end()) {
        errors.push_back("component '" + key + "' was not found");
        return {};
    }
    if (!stack.insert(key).second) {
        errors.push_back("component '" + key + "' extends itself");
        return {};
    }
    std::vector<RoboXMLNode> body;
    if (!found->second.extends.empty()) {
        body = ComponentBody(Lower(found->second.extends), components, stack, errors);
    }
    std::vector<RoboXMLNode> overrides;
    for (const RoboXMLNode& child : found->second.children) {
        if (Lower(child.name) == "override") overrides.push_back(child);
        else body.push_back(child);
    }
    ApplyOverrides(body, overrides, errors);
    stack.erase(key);
    return body;
}

void Collect(const RoboXMLNode& node, const fs::path& directory, Components& components, std::unordered_set<std::string>& loading,
             std::unordered_set<std::string>& done, std::vector<std::string>& errors);

void CollectFile(const fs::path& path, Components& components, std::unordered_set<std::string>& loading, std::unordered_set<std::string>& done,
                 std::vector<std::string>& errors) {
    std::error_code code;
    const fs::path absolute = fs::weakly_canonical(path, code);
    const std::string key = code ? path.string() : absolute.string();
    if (done.count(key)) return;
    if (!loading.insert(key).second) {
        errors.push_back("import cycle at " + key);
        return;
    }
    RoboXMLDocument document;
    const std::string text = ReadFile(path);
    if (text.empty() || !RoboXMLParse(text, document)) {
        errors.push_back("could not read '" + path.string() + "'");
    } else if (!IsSceneDocument(document.root)) {
        errors.push_back("'" + path.filename().string() + "' is not a Robix3D document");
    } else {
        done.insert(key);
        Collect(document.root, path.parent_path(), components, loading, done, errors);
    }
    loading.erase(key);
}

void Collect(const RoboXMLNode& node, const fs::path& directory, Components& components, std::unordered_set<std::string>& loading,
             std::unordered_set<std::string>& done, std::vector<std::string>& errors) {
    const std::string kind = Lower(node.name);
    if (kind == "import") {
        const std::string source = Attr(node, "source");
        if (source.empty()) errors.push_back("Import needs Source");
        else {
            fs::path path = fs::path(source);
            if (path.is_relative()) path = directory / path;
            const std::string ext = Lower(path.extension().string());
            if (ext == ".rxu" || ext == ".xml") {
                errors.push_back("Robix3D cannot import RobixUI. Host it with <UI Source=\"" + source + "\"/>");
            } else {
                CollectFile(path, components, loading, done, errors);
            }
        }
    } else if (kind == "component") {
        const std::string name = Attr(node, "name");
        if (name.empty()) errors.push_back("Component needs Name");
        else {
            const std::string key = Lower(name);
            if (IsSceneTag(key) || IsUiTag(key)) errors.push_back("component '" + name + "' uses a built-in name");
            else components[key] = ComponentDef{name, Attr(node, "extends"), node.children, directory};
        }
    }
    for (const RoboXMLNode& child : node.children) Collect(child, directory, components, loading, done, errors);
}

void SubstituteNode(RoboXMLNode& node, const Scope& scope, std::vector<std::string>& errors) {
    for (auto& entry : node.attrs) entry.second = Substitute(entry.second, scope, errors);
    node.text = Substitute(node.text, scope, errors);
}

std::vector<RoboXMLNode> Expand(const std::vector<RoboXMLNode>& nodes, Scope& scope, const Components& components,
                                std::vector<std::string>& errors, int depth, bool inShader = false);

std::vector<RoboXMLNode> ExpandOne(const RoboXMLNode& node, Scope& scope, const Components& components, std::vector<std::string>& errors, int depth,
                                   bool inShader = false) {
    if (depth > 32) {
        errors.push_back("component nesting stopped at 32");
        return {};
    }
    const std::string kind = Lower(node.name);
    if (kind == "import" || kind == "component") return {};
    if (kind == "parameters") {
        for (const RoboXMLNode& child : node.children) {
            if (Lower(child.name) != "number") continue;
            const std::string name = Attr(child, "name");
            std::string error;
            const auto value = Expr(Substitute(Attr(child, "value"), scope, errors)).Run(scope, error);
            if (name.empty() || !value) errors.push_back(error.empty() ? "Number needs Name and Value" : error);
            else scope.numbers[name] = *value;
        }
        return {};
    }
    if (kind == "shader") {
        RoboXMLNode copy = node;
        SubstituteNode(copy, scope, errors);
        copy.children = Expand(node.children, scope, components, errors, depth + 1, true);
        return {copy};
    }
    if (!inShader && (kind == "let" || kind == "number")) {
        const std::string name = Attr(node, "name");
        if (kind == "let" && name.empty()) return Expand(node.children, scope, components, errors, depth + 1);
        std::string error;
        const auto value = Expr(Substitute(Attr(node, "value"), scope, errors)).Run(scope, error);
        if (name.empty() || !value) errors.push_back(error.empty() ? kind + " needs Name and Value" : error);
        else scope.numbers[name] = *value;
        if (kind == "let") return Expand(node.children, scope, components, errors, depth + 1);
        return {};
    }
    if (kind == "random") {
        std::string error;
        const auto value = Expr(Substitute(Attr(node, "seed"), scope, errors)).Run(scope, error);
        if (!value) errors.push_back(error.empty() ? "Random needs Seed" : error);
        else gSeed = static_cast<std::uint32_t>(std::llround(*value));
        return {};
    }
    if (!inShader && (kind == "if" || kind == "else")) return {};
    if (kind == "foreach") {
        const std::string variable = Attr(node, "variable");
        std::string fromError;
        std::string toError;
        const auto from = Expr(Substitute(Attr(node, "from").empty() ? "0" : Attr(node, "from"), scope, errors)).Run(scope, fromError);
        const auto to = Expr(Substitute(Attr(node, "to"), scope, errors)).Run(scope, toError);
        if (variable.empty() || !from || !to) {
            errors.push_back("ForEach needs Variable and To");
            return {};
        }
        std::vector<RoboXMLNode> produced;
        const int start = static_cast<int>(std::llround(*from));
        const int end = static_cast<int>(std::llround(*to));
        int count = 0;
        for (int value = start; value <= end; ++value) {
            if (++count > 10000) {
                errors.push_back("ForEach stopped at 10000 steps");
                break;
            }
            Scope inner;
            inner.parent = &scope;
            inner.numbers[variable] = value;
            auto part = Expand(node.children, inner, components, errors, depth + 1);
            for (RoboXMLNode& item : part) {
                if (Attr(item, "name").empty()) item.attrs["name"] = variable + std::to_string(value);
            }
            produced.insert(produced.end(), part.begin(), part.end());
        }
        return produced;
    }
    if (kind == "generator") {
        Scope inner;
        inner.parent = &scope;
        RoboXMLNode group;
        group.name = "Group";
        if (!Attr(node, "name").empty()) group.attrs["name"] = Attr(node, "name");
        group.children = Expand(node.children, inner, components, errors, depth + 1);
        SubstituteNode(group, inner, errors);
        return {group};
    }
    const bool instance = kind == "instance" || components.count(kind) != 0;
    if (instance) {
        const std::string componentName = kind == "instance" ? Lower(Attr(node, "component")) : kind;
        std::unordered_set<std::string> stack;
        std::vector<RoboXMLNode> body = ComponentBody(componentName, components, stack, errors);
        Scope inner;
        inner.parent = &scope;
        for (const auto& entry : node.attrs) {
            if (Lower(entry.first) == "component" || Lower(entry.first) == "position" || Lower(entry.first) == "rotation" ||
                Lower(entry.first) == "scale" || Lower(entry.first) == "name") {
                continue;
            }
            std::string error;
            const auto value = Expr(Substitute(entry.second, scope, errors)).Run(scope, error);
            if (value) inner.numbers[entry.first] = *value;
        }
        RoboXMLNode group;
        group.name = "Group";
        const std::string name = Attr(node, "name");
        if (!name.empty()) group.attrs["name"] = name;
        if (!Attr(node, "position").empty()) group.attrs["position"] = Attr(node, "position");
        if (!Attr(node, "rotation").empty()) group.attrs["rotation"] = Attr(node, "rotation");
        if (!Attr(node, "scale").empty()) group.attrs["scale"] = Attr(node, "scale");
        group.children = Expand(body, inner, components, errors, depth + 1);
        SubstituteNode(group, inner, errors);
        return {group};
    }
    RoboXMLNode copy = node;
    if (kind != "vertex" && kind != "fragment" && kind != "glsl") SubstituteNode(copy, scope, errors);
    Scope inner;
    inner.parent = &scope;
    copy.children = Expand(copy.children, inner, components, errors, depth + 1, inShader);
    return {copy};
}

std::vector<RoboXMLNode> Expand(const std::vector<RoboXMLNode>& nodes, Scope& scope, const Components& components,
                                std::vector<std::string>& errors, int depth, bool inShader) {
    std::vector<RoboXMLNode> out;
    bool sawIf = false;
    bool taken = false;
    for (const RoboXMLNode& node : nodes) {
        const std::string kind = Lower(node.name);
        if (!inShader && kind == "else") {
            if (!sawIf) errors.push_back("Else needs an If before it");
            else if (!taken) {
                const auto part = Expand(node.children, scope, components, errors, depth, false);
                out.insert(out.end(), part.begin(), part.end());
            }
            sawIf = false;
            continue;
        }
        if (!inShader) sawIf = false;
        if (!inShader && kind == "if") {
            std::string error;
            const auto value = Expr(Substitute(Attr(node, "test"), scope, errors)).Run(scope, error);
            if (!value) errors.push_back(error.empty() ? "If needs Test" : error);
            taken = value && *value != 0.0;
            sawIf = true;
            if (taken) {
                const auto part = Expand(node.children, scope, components, errors, depth, false);
                out.insert(out.end(), part.begin(), part.end());
            }
            continue;
        }
        const auto part = ExpandOne(node, scope, components, errors, depth, inShader);
        out.insert(out.end(), part.begin(), part.end());
    }
    return out;
}

std::optional<double> NumberAttr(const RoboXMLNode& node, const char* key) {
    const std::string text = Trim(Attr(node, key));
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const double value = std::stod(text, &used);
        if (used != text.size()) return std::nullopt;
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

void RequirePositive(const RoboXMLNode& node, const char* key, const std::string& where, std::vector<std::string>& errors, bool allowZero) {
    const auto value = NumberAttr(node, key);
    if (!value) return;
    if (allowZero ? *value < 0 : *value <= 0) errors.push_back(where + " " + key + " must be greater than " + (allowZero ? "or equal to 0" : "0"));
}

void ValidateGeometry(const RoboXMLNode& node, std::vector<std::string>& errors) {
    const std::string type = Lower(Attr(node, "type").empty() ? "box" : Attr(node, "type"));
    static const char* known[] = {"box", "sphere", "cylinder", "cone", "capsule", "plane", "circle", "ring", "torus", "torusknot",
                                  "tube", "lathe", "extrude", "rounded-box", "roundedbox", "icosahedron", "octahedron",
                                  "dodecahedron", "tetrahedron", "custom"};
    bool ok = false;
    for (const char* item : known) {
        if (type == item) ok = true;
    }
    if (!ok) errors.push_back("geometry type '" + type + "' is not supported");
    RequirePositive(node, "width", "geometry", errors, false);
    RequirePositive(node, "height", "geometry", errors, false);
    RequirePositive(node, "depth", "geometry", errors, false);
    RequirePositive(node, "radius", "geometry", errors, false);
    RequirePositive(node, "radiusTop", "geometry", errors, true);
    RequirePositive(node, "radiusBottom", "geometry", errors, true);
    RequirePositive(node, "tube", "geometry", errors, false);
    const auto segments = NumberAttr(node, "segments");
    if (segments && *segments < 3) errors.push_back("geometry segments must be at least 3");
}

void ValidateTree(const RoboXMLNode& node, bool inUi, std::unordered_set<std::string>& names, const std::string& path, std::vector<std::string>& errors) {
    const std::string kind = Lower(node.name);
    const bool physicsBody = kind == "physicsbody" || (kind == "body" && !Attr(node, "type").empty());
    if (IsUiTag(kind) && !IsSceneTag(kind) && !physicsBody) {
        errors.push_back("RobixUI element <" + node.name + "> cannot be used in a Robix3D document");
        return;
    }
    if (kind == "view3d" || kind == "scene3d") {
        errors.push_back("View3D belongs in a RobixUI document");
        return;
    }
    if (kind == "glsl") errors.push_back("Raw GLSL is not part of Robix3D. Describe the shader with Let and Output.");
    if (!IsSceneTag(kind) && !physicsBody) errors.push_back("unknown Robix3D element <" + node.name + ">");
    if (kind == "ui") {
        for (const RoboXMLNode& child : node.children) {
            const std::string childKind = Lower(child.name);
            if (childKind == "colorpicker" || childKind == "slider" || childKind == "toggle" || childKind == "note") continue;
            errors.push_back("UI in Robix3D can host a RobixUI file with Source, or ColorPicker, Slider, Toggle, and Note");
        }
        return;
    }
    if (kind == "geometry") ValidateGeometry(node, errors);
    for (const auto& entry : node.attrs) {
        if (entry.second.find('{') == std::string::npos) continue;
        const std::string key = Lower(entry.first);
        const bool binding = kind == "binding" && key == "value";
        const bool gradient = kind == "gradient" && (key == "from" || key == "to");
        const bool action = kind == "action" && key == "value";
        if (!binding && !gradient && !action) errors.push_back("could not evaluate '" + entry.second + "'");
    }
    const std::string name = Attr(node, "name");
    std::string next = path;
    if (!name.empty() && (kind == "mesh" || kind == "group" || kind == "object" || kind == "model" || kind == "text3d" || kind == "camera" || kind == "ground")) {
        next = path.empty() ? name : path + "." + name;
        if (!names.insert(Lower(next)).second) errors.push_back("duplicate name '" + next + "'");
    }
    for (const RoboXMLNode& child : node.children) ValidateTree(child, inUi, names, next, errors);
}

void WalkUi(const RoboXMLNode& node, bool hosted, std::vector<std::string>& errors) {
    const std::string kind = Lower(node.name);
    if (kind == "view3d" || kind == "scene3d") {
        if (Attr(node, "source").empty()) errors.push_back("View3D needs Source pointing at a .r3d document");
        for (const RoboXMLNode& child : node.children) {
            errors.push_back("View3D hosts a Robix3D file. <" + child.name + "> cannot be written inside it");
        }
        return;
    }
    if (!hosted && IsSceneTag(kind) && kind != "import" && kind != "component" && kind != "let" && kind != "foreach" && kind != "if" &&
        kind != "else" && kind != "random") {
        errors.push_back("Robix3D element <" + node.name + "> cannot be used in a RobixUI document. Host the scene with <View3D Source=\"file.r3d\"/>");
    }
    for (const RoboXMLNode& child : node.children) WalkUi(child, hosted, errors);
}

}  // namespace

bool IsSceneDocument(const RoboXMLNode& root) {
    return Lower(root.name) == "robix3d";
}

std::vector<std::string> UiBoundaryErrors(const RoboXMLNode& root) {
    std::vector<std::string> errors;
    if (IsSceneDocument(root)) return errors;
    WalkUi(root, false, errors);
    return errors;
}

Prepared Prepare(const fs::path& document) {
    Prepared prepared;
    prepared.directory = document.parent_path();
    RoboXMLDocument xml;
    const std::string text = ReadFile(document);
    if (text.empty() || !RoboXMLParse(text, xml)) {
        prepared.errors.push_back(text.empty() ? "could not read " + document.string() : "could not parse " + document.filename().string());
        if (!xml.errors.empty()) prepared.errors.insert(prepared.errors.end(), xml.errors.begin(), xml.errors.end());
        return prepared;
    }
    if (!IsSceneDocument(xml.root)) {
        prepared.errors.push_back(document.filename().string() + " must start with <Robix3D>");
        return prepared;
    }
    prepared.degrees = Lower(Attr(xml.root, "degrees")) == "true";
    if (!Attr(xml.root, "units").empty()) prepared.units = Lower(Attr(xml.root, "units"));
    if (!Attr(xml.root, "three").empty()) prepared.three = Attr(xml.root, "three");
    Components components;
    std::unordered_set<std::string> loading;
    std::unordered_set<std::string> done;
    Collect(xml.root, document.parent_path(), components, loading, done, prepared.errors);
    gSeed = 1337;
    if (!Attr(xml.root, "seed").empty()) {
        try {
            gSeed = static_cast<std::uint32_t>(std::stoll(Attr(xml.root, "seed")));
        } catch (...) {
            prepared.errors.push_back("Seed must be a whole number");
        }
    }
    Scope scope;
    RoboXMLNode root = xml.root;
    root.children = Expand(xml.root.children, scope, components, prepared.errors, 0);
    int scenes = 0;
    for (const RoboXMLNode& child : root.children) {
        if (Lower(child.name) == "scene") {
            ++scenes;
            if (!Attr(child, "name").empty()) prepared.title = Attr(child, "name");
        }
    }
    if (scenes == 0) prepared.errors.push_back("Robix3D needs a Scene");
    if (scenes > 1) prepared.errors.push_back("Robix3D can contain one Scene");
    std::unordered_set<std::string> names;
    for (const RoboXMLNode& child : root.children) ValidateTree(child, false, names, "", prepared.errors);
    prepared.root = std::move(root);
    return prepared;
}

}  // namespace Robix::D3
