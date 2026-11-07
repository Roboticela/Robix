#include "RoboCompilerShared.hpp"

#include "Components/Board/Board.Platform.hpp"
#include "Components/Screen/Screen.Platform.hpp"

#include <cctype>
#include <cstdio>
#include <sstream>

void RoboCompilerAddDiag(RoboCompilerResult& result, RoboCompilerDiagnostic::Severity severity, const std::string& path,
                         const std::string& message) {
    RoboCompilerDiagnostic note;
    note.severity = severity;
    note.path = path;
    note.message = message;
    result.diagnostics.push_back(std::move(note));
}

std::string RoboCompilerLower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

bool RoboCompilerIsLayoutKey(const std::string& key) {
    static const char* keys[] = {
        "position", "left", "top", "right", "bottom", "inset", "margin", "width", "height", "gap", "padding",
        "display", "direction", "justify", "align", "wrap", "columns", "rows"};
    const std::string lower = RoboCompilerLower(key);
    for (const char* item : keys) {
        if (lower == item) {
            return true;
        }
    }
    return false;
}

bool RoboCompilerIsOneOf(const std::string& value, std::initializer_list<const char*> options) {
    const std::string text = RoboCompilerLower(value);
    for (const char* option : options) {
        if (text == option) {
            return true;
        }
    }
    return false;
}

bool RoboCompilerCheckOverflowAttrs(const RoboXMLNode&, const std::string&, RoboCompilerResult&) { return true; }

void RoboCompilerNoteUsedComponent(const std::string&) {}
void RoboCompilerNoteUsedIcon(const std::string&) {}

std::string RoboCompilerCppEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        if (c == '\\' || c == '"') {
            out.push_back('\\');
            out.push_back(static_cast<char>(c));
        } else if (c < 0x20 || c >= 0x7F) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\x%02X\"\"", c);
            out += buf;
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

std::string RoboCompilerTrim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string RoboCompilerEmitLayoutProps(const RoboXMLNode& node, const std::string& componentExpr) {
    static const char* keys[] = {
        "position", "left", "top", "right", "bottom", "inset", "width", "height", "margin", "marginTop",
        "marginRight", "marginBottom", "marginLeft", "flex", "flexGrow", "grow", "flexShrink", "shrink",
        "alignSelf", "order", "display", "direction", "flexDirection", "justify", "justifyContent", "align",
        "alignItems", "alignContent", "justifyItems", "wrap", "flexWrap", "gap", "rowGap", "columnGap",
        "columns", "rows", "gridColumn", "gridRow", "minWidth", "minHeight", "maxWidth", "maxHeight",
        "minSize", "maxSize", "size", "clip", "overflow", "overflowX", "overflowY", "overflow-x", "overflow-y"};
    std::ostringstream ss;
    for (const char* key : keys) {
        const std::string value = RoboXMLAttr(node, key);
        if (value.empty()) {
            continue;
        }
        ss << "  RoboUISetProperty(" << componentExpr << ", \"" << key << "\", \"" << RoboCompilerCppEscape(value)
           << "\");\n";
    }
    return ss.str();
}

bool RoboCompilerCheckNode(const RoboXMLNode&, const std::string&, RoboCompilerResult&) { return true; }

std::string RoboCompilerEmitChildCpp(const RoboXMLNode& node, const std::string& parentExpr) {
    const std::string tag = RoboCompilerLower(node.name);
    if (RoboCompilerIsBoardName(node.name)) {
        return RoboCompilerEmitBoardCpp(node, parentExpr);
    }
    if (tag == "screen") {
        return RoboCompilerEmitScreenCpp(node);
    }
    return {};
}

bool RoboCompilerIsIdent(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    for (char ch : value) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
            return false;
        }
    }
    return true;
}

std::string RoboCompilerIconNameFromNode(const RoboXMLNode&) { return {}; }
bool RoboCompilerIsIconNode(const RoboXMLNode&) { return false; }
bool RoboCompilerIconsLoaded() { return false; }
bool RoboCompilerIconInCatalog(const char*) { return false; }
std::string RoboCompilerCatalogSvg(const char*) { return {}; }
void RoboCompilerBindSvgName(const RoboXMLNode*, const std::string&) {}
std::string RoboCompilerLookupSvgName(const RoboXMLNode*) { return {}; }
int RoboCompilerTakeInlineSerial() { return 0; }
bool RoboCompilerInlineSvgTaken(const std::string&) { return false; }
void RoboCompilerPutInlineSvg(const std::string&, const std::string&) {}
std::string RoboCompilerSvgIconName(const RoboXMLNode&) { return {}; }
