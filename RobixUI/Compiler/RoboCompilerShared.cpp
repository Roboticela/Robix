#include "RoboCompilerShared.hpp"

#include "ComponentTier.hpp"

#include "Components/Button/Button.hpp"
#include "Components/Container/Container.hpp"
#include "Components/Icon/Icon.hpp"
#include "Components/Screen/Screen.hpp"
#include "Components/Svg/Svg.hpp"
#include "Components/Text/Text.hpp"

#include "../RoboShared/RoboIconPack.hpp"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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

void RoboCompilerNoteUsedIcon(const std::string& iconName);

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

std::string RoboCompilerTrim(std::string value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n')) {
        value.pop_back();
    }
    return value;
}

bool RoboCompilerIsSharedStyleKey(const std::string& key) {
    return RoboCompilerIsLayoutKey(key) || RoboCompilerIsButtonStyleKey(key) || RoboCompilerIsContainerStyleKey(key) ||
           RoboCompilerIsTextStyleKey(key);
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
        "overflow-y",    "clip",          "bleed",         "paint"};
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
    const std::string paint = RoboXMLAttr(node, "paint");
    if (!paint.empty() && !RoboCompilerIsOneOf(paint, {"bleed", "clip"})) {
        RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path, "Paint must be bleed or clip");
        ok = false;
    }
    const std::string bleed = RoboCompilerLower(RoboXMLAttr(node, "bleed"));
    const bool clipOn = RoboCompilerIsOneOf(clip, {"true", "1", "yes", "on"}) || RoboCompilerLower(paint) == "clip";
    const bool bleedOn = (!bleed.empty() && !RoboCompilerIsOneOf(bleed, {"false", "0", "no", "off"})) || RoboCompilerLower(paint) == "bleed";
    if (clipOn && bleedOn) {
        RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path, "Paint is either bleed or clip");
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
                                 "size",         "clip",         "bleed",        "paint",        "overflow",
                                 "overflowX",    "overflowY",    "overflow-x",   "overflow-y"};
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
    const RoboUITier tier = RoboCompilerComponentTier(name);
    if (tier == RoboUITier::Preset) {
        const char* base = RoboCompilerComponentBase(name);
        if (base != nullptr) RoboCompilerNoteUsedComponent(base);
    }
    const bool overflowOk = RoboCompilerCheckOverflowAttrs(node, path, result);
    if (RoboCompilerLower(name) == "screen") {
        bool ok = true;
        for (const auto& child : node.children) {
            ok = RoboCompilerCheckNode(child, path, result) && ok;
        }
        return overflowOk && ok;
    }
    if (RoboCompilerIsContainerName(name)) {
        return overflowOk && RoboCompilerCheckContainer(node, path, result);
    }
    if (RoboCompilerIsTextName(name)) {
        return overflowOk && RoboCompilerCheckText(node, path, result);
    }
    if (RoboCompilerLower(name) == "svg") {
        return overflowOk && RoboCompilerCheckSvg(node, path, result);
    }
    bool ok = true;
    for (const auto& child : node.children) {
        ok = RoboCompilerCheckNode(child, path, result) && ok;
    }
    return overflowOk && ok;
}

std::string RoboCompilerEmitChildCpp(const RoboXMLNode& node, const std::string& parentExpr) {
    if (RoboCompilerLower(node.name) == "svg" || RoboCompilerIsIconNode(node) || RoboCompilerLower(node.name) == "icon") {
        return RoboCompilerEmitIconCpp(node, parentExpr);
    }
    if (RoboCompilerIsTextLike(node)) {
        return RoboCompilerEmitTextCpp(node, parentExpr);
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

namespace {

namespace fs = std::filesystem;

struct RoboCompilerIconState {
    std::vector<uint8_t> catalogBytes;
    RoboIconCatalogView catalog{};
    std::unordered_set<std::string> usedIcons;
    std::unordered_map<std::string, std::string> inlineSvg;
    std::unordered_map<const RoboXMLNode*, std::string> svgNames;
    int inlineSerial = 0;
    bool attempted = false;
    bool loaded = false;
};

RoboCompilerIconState gRoboCompilerIcons;

fs::path RoboCompilerFindUpFrom(fs::path dir, const fs::path& relative) {
    for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
        std::error_code error;
        const fs::path candidate = dir / relative;
        if (fs::exists(candidate, error)) {
            return candidate;
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) {
            break;
        }
        dir = dir.parent_path();
    }
    return {};
}

fs::path RoboCompilerFindUp(const fs::path& relative) {
    std::error_code error;
    fs::path found = RoboCompilerFindUpFrom(fs::current_path(error), relative);
    if (!found.empty()) {
        return found;
    }
#ifdef _WIN32
    wchar_t executable[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        found = RoboCompilerFindUpFrom(fs::path(executable).parent_path(), relative);
    }
#endif
    return found;
}

}  // namespace

namespace fs = std::filesystem;

void RoboCompilerNoteUsedIcon(const std::string& iconName) {
    if (!iconName.empty()) {
        gRoboCompilerIcons.usedIcons.insert(iconName);
    }
}

void RoboCompilerEnsureIcons() {
    if (gRoboCompilerIcons.attempted) {
        return;
    }
    gRoboCompilerIcons.attempted = true;
    fs::path catalogPath = RoboCompilerFindUp("Assets/Icons.rpack");
    if (catalogPath.empty()) {
        catalogPath = RoboCompilerFindUp("RefCode/Assets/Icons.rpack");
    }
    if (catalogPath.empty()) {
        return;
    }
    std::ifstream in(catalogPath, std::ios::binary);
    if (!in) {
        return;
    }
    gRoboCompilerIcons.catalogBytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (!RoboIconCatalogValidate(gRoboCompilerIcons.catalogBytes.data(), gRoboCompilerIcons.catalogBytes.size())) {
        gRoboCompilerIcons.catalogBytes.clear();
        return;
    }
    gRoboCompilerIcons.catalog = RoboIconCatalogMakeView(gRoboCompilerIcons.catalogBytes.data(),
                                                         static_cast<RoboSize>(gRoboCompilerIcons.catalogBytes.size()));
    gRoboCompilerIcons.loaded = true;
}

bool RoboCompilerIconsLoaded() { return gRoboCompilerIcons.loaded; }

bool RoboCompilerIconInCatalog(const char* name) {
    RoboCompilerEnsureIcons();
    return name != nullptr && gRoboCompilerIcons.loaded &&
           RoboIconCatalogFind(gRoboCompilerIcons.catalog, name) != ROBO_NULL;
}

std::string RoboCompilerCatalogSvg(const char* name) {
    RoboCompilerEnsureIcons();
    if (name == nullptr || !gRoboCompilerIcons.loaded) {
        return {};
    }
    const RoboIconCatalogEntry* entry = RoboIconCatalogFind(gRoboCompilerIcons.catalog, name);
    if (entry == nullptr || entry->svgSize == 0) {
        return {};
    }
    const RoboSize end = static_cast<RoboSize>(entry->svgOffset) + static_cast<RoboSize>(entry->svgSize);
    if (end > gRoboCompilerIcons.catalogBytes.size()) {
        return {};
    }
    return std::string(reinterpret_cast<const char*>(gRoboCompilerIcons.catalog.bytes + entry->svgOffset), entry->svgSize);
}

void RoboCompilerBindSvgName(const RoboXMLNode* node, const std::string& name) {
    gRoboCompilerIcons.svgNames[node] = name;
}

std::string RoboCompilerLookupSvgName(const RoboXMLNode* node) {
    const auto named = gRoboCompilerIcons.svgNames.find(node);
    if (named == gRoboCompilerIcons.svgNames.end()) {
        return {};
    }
    return named->second;
}

int RoboCompilerTakeInlineSerial() { return ++gRoboCompilerIcons.inlineSerial; }

bool RoboCompilerInlineSvgTaken(const std::string& name) {
    return gRoboCompilerIcons.inlineSvg.find(name) != gRoboCompilerIcons.inlineSvg.end();
}

void RoboCompilerPutInlineSvg(const std::string& name, const std::string& svg) {
    gRoboCompilerIcons.inlineSvg[name] = svg;
}

std::string RoboCompilerIconNameFromNode(const RoboXMLNode& node) {
    const std::string n = RoboCompilerLower(node.name);
    if (n == "icon") {
        std::string named = RoboXMLAttr(node, "name");
        if (named.empty()) {
            named = RoboXMLAttr(node, "icon");
        }
        if (named.empty()) {
            named = RoboXMLAttr(node, "src");
        }
        return named;
    }
    return node.name;
}

bool RoboCompilerIsIconNode(const RoboXMLNode& node) {
    RoboCompilerEnsureIcons();
    if (!gRoboCompilerIcons.loaded) {
        return false;
    }
    const std::string iconName = RoboCompilerIconNameFromNode(node);
    if (iconName.empty()) {
        return false;
    }
    const std::string n = RoboCompilerLower(node.name);
    if (n != "icon" && (n == "screen" || n == "button" || n == "anchor" || n == "iconbutton" || n == "switch" || n == "switchgroup" ||
                        n == "switchitem" || n == "themetoggle" || n == "themeicontoggle" || n == "textfield" ||
                        n == "textarea" || n == "passwordfield" || n == "emailfield" || n == "phonefield" || n == "namefield" || n == "picture" || n == "slide" || n == "brandlogo" || n == "passwordreveal" || n == "confirmaction" || n == "select" || n == "option" ||
                        n == "countryselect" ||
                        n == "segmentedcontrol" || n == "segment" || n == "segmentedtoolbar" ||
                        n == "container" || n == "board" || n == "text" || n == "image" ||
                        n == "heading" || n == "body" || n == "caption" || n == "span")) {
        return false;
    }
    return RoboIconCatalogFind(gRoboCompilerIcons.catalog, iconName.c_str()) != ROBO_NULL;
}

bool RoboCompilerIsIdent(const std::string& value) {
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_')) {
        return false;
    }
    for (char ch : value) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
            return false;
        }
    }
    return true;
}

std::string RoboCompilerSvgIconName(const RoboXMLNode& node) {
    const auto named = gRoboCompilerIcons.svgNames.find(&node);
    if (named != gRoboCompilerIcons.svgNames.end()) {
        return named->second;
    }
    std::string name = RoboXMLAttr(node, "id");
    if (name.empty()) {
        name = RoboXMLAttr(node, "name");
    }
    return name;
}

