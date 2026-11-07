#include "RoboCompilerShared.hpp"

#include "Components/Board/Board.hpp"
#include "Components/Container/Container.hpp"
#include "Components/Pane/Pane.hpp"
#include "Components/Screen/Screen.hpp"
#include "Components/Scrollbar/Scrollbar.hpp"

#include <cstdio>
#include <sstream>

std::string RoboCompilerLower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

void RoboCompilerAddDiag(RoboCompilerResult& result, RoboCompilerDiagnostic::Severity severity, const std::string& path,
                         const std::string& message) {
    result.diagnostics.push_back(RoboCompilerDiagnostic{severity, path, message});
    if (severity == RoboCompilerDiagnostic::Severity::Error) {
        result.ok = false;
    }
}

void RoboCompilerNoteUsedComponent(const std::string& name) { (void)name; }

void RoboCompilerNoteUsedIcon(const std::string& iconName) { (void)iconName; }

std::string RoboCompilerCppEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() * 2);
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

bool RoboCompilerIsLayoutKey(const std::string& key) {
    static const char* keys[] = {
        "position",      "left",          "top",           "right",         "bottom",        "inset",
        "margin",        "margintop",     "marginright",   "marginbottom",  "marginleft",    "flex",
        "flexgrow",      "grow",          "flexshrink",    "shrink",        "alignself",     "order",
        "display",       "direction",     "flexdirection", "justify",       "justifycontent","align",
        "alignitems",    "aligncontent",  "justifyitems",  "wrap",          "flexwrap",      "gap",
        "rowgap",        "columngap",     "colgap",        "columns",       "rows",          "gridcolumn",
        "gridrow",       "minwidth",      "minheight",     "maxwidth",      "maxheight",     "minsize",
        "maxsize",       "size",          "overflow",      "overflowx",     "overflowy",     "overflow-x",
        "overflow-y",    "clip"};
    for (const char* item : keys) {
        if (key == item) {
            return true;
        }
    }
    return false;
}

bool RoboCompilerIsOneOf(const std::string& value, std::initializer_list<const char*> options) {
    const std::string t = RoboCompilerLower(value);
    for (const char* opt : options) {
        if (t == opt) {
            return true;
        }
    }
    return false;
}

bool RoboCompilerCheckOverflowAttrs(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
    bool ok = true;
    const char* axes[] = {"overflow", "overflowX", "overflowY", "overflow-x", "overflow-y"};
    for (const char* key : axes) {
        const std::string value = RoboXMLAttr(node, key);
        if (value.empty()) {
            continue;
        }
        if (!RoboCompilerIsOneOf(value, {"visible", "hidden", "clip", "scroll", "auto"})) {
            RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                                std::string(key) + " must be visible|hidden|clip|scroll|auto");
            ok = false;
        }
    }
    const std::string clip = RoboXMLAttr(node, "clip");
    if (!clip.empty() && !RoboCompilerIsOneOf(clip, {"true", "false", "1", "0", "yes", "no", "on", "off"})) {
        RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path, "clip must be true or false");
        ok = false;
    }
    return ok;
}

bool RoboCompilerIsPaneFamily(const std::string& name) {
    const std::string n = RoboCompilerLower(name);
    return n == "pane" || n == "sidebar" || n == "paneitem" || n == "sidebaritem" || n == "titlebar" || n == "navbar" ||
           n == "titlebaritem" || n == "navbaritem";
}

std::string RoboCompilerEmitLayoutProps(const RoboXMLNode& node, const std::string& componentExpr) {
    static const char* keys[] = {"position",     "left",         "top",          "right",        "bottom",
                                 "inset",        "width",        "height",       "margin",       "marginTop",
                                 "marginRight",  "marginBottom", "marginLeft",   "flex",         "flexGrow",
                                 "grow",         "flexShrink",   "shrink",       "alignSelf",    "order",
                                 "display",      "direction",    "flexDirection","justify",      "justifyContent",
                                 "align",        "alignItems",   "alignContent", "justifyItems", "wrap",
                                 "flexWrap",     "gap",          "rowGap",       "columnGap",    "columns",
                                 "rows",         "gridColumn",   "gridRow",      "minWidth",
                                 "minHeight",    "maxWidth",     "maxHeight",    "minSize",      "maxSize",
                                 "size",         "clip",         "overflow",     "overflowX",    "overflowY",
                                 "overflow-x",   "overflow-y"};
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

bool RoboCompilerCheckNode(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
    const std::string name = node.name;
    if (RoboCompilerLower(name) == "foundation") {
        return true;
    }
    RoboCompilerNoteUsedComponent(name);
    const bool overflowOk = RoboCompilerCheckOverflowAttrs(node, path, result);
    if (RoboCompilerLower(name) == "screen") {
        bool ok = true;
        for (const auto& child : node.children) {
            ok = RoboCompilerCheckNode(child, path, result) && ok;
        }
        return overflowOk && ok;
    }
    if (RoboCompilerIsBoardName(name)) {
        bool ok = true;
        for (const auto& child : node.children) {
            ok = RoboCompilerCheckNode(child, path, result) && ok;
        }
        return overflowOk && ok;
    }
    if (RoboCompilerIsContainerName(name)) {
        return overflowOk && RoboCompilerCheckContainer(node, path, result);
    }
    if (RoboCompilerIsScrollbarName(name)) {
        for (const auto& kv : node.attrs) {
            const std::string key = RoboCompilerLower(kv.first);
            if (key == "arrows" || key == "autohide" || key == "overlay" || key == "thumbcolor" || key == "trackcolor") {
                RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                                    "<Scrollbar> " + kv.first + " belongs on <ScrollView>");
                break;
            }
        }
        return overflowOk;
    }
    bool ok = true;
    for (const auto& child : node.children) {
        ok = RoboCompilerCheckNode(child, path, result) && ok;
    }
    return overflowOk && ok;
}

std::string RoboCompilerEmitChildCpp(const RoboXMLNode& node, const std::string& parentExpr) {
    if (RoboCompilerIsBoardName(node.name)) {
        return RoboCompilerEmitBoardCpp(node, parentExpr);
    }
    if (RoboCompilerIsContainerName(node.name)) {
        return RoboCompilerEmitContainerCpp(node, parentExpr);
    }
    if (RoboCompilerLower(node.name) == "screen" &&
        (!RoboXMLAttr(node, "path").empty() || !RoboXMLAttr(node, "route").empty() || !RoboXMLAttr(node, "id").empty())) {
        return RoboCompilerEmitScreenCpp(node);
    }
    return {};
}
