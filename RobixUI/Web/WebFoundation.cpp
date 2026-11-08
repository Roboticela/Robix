#include "WebFoundation.hpp"

#include "../Compiler/Components/Board/Board.hpp"
#include "../Compiler/Components/Button/ButtonWeb.hpp"
#include "../Compiler/Components/IconButton/IconButtonWeb.hpp"
#include "../Compiler/Components/Switch/SwitchWeb.hpp"
#include "../Compiler/Components/SegmentedControl/SegmentedControlWeb.hpp"
#include "../Compiler/Components/Board/BoardWeb.hpp"
#include "../Compiler/Components/Container/Container.hpp"
#include "../Compiler/Components/Container/ContainerWeb.hpp"
#include "../Compiler/Components/Scrollbar/ScrollbarWeb.hpp"
#include "../Compiler/Components/Body/BodyWeb.hpp"
#include "../Compiler/Components/Caption/CaptionWeb.hpp"
#include "../Compiler/Components/Eyebrow/EyebrowWeb.hpp"
#include "../Compiler/Components/Heading/HeadingWeb.hpp"
#include "../Compiler/Components/Icon/IconWeb.hpp"
#include "../Compiler/Components/Svg/SvgWeb.hpp"
#include "../Compiler/Components/Label/LabelWeb.hpp"
#include "../Compiler/Components/Paragraph/ParagraphWeb.hpp"
#include "../Compiler/Components/Screen/Screen.hpp"
#include "../Compiler/Compose.hpp"
#include "../Compiler/Motion.hpp"
#include "../Compiler/ScreenSize.hpp"
#include "../Compiler/Components/Subhead/SubheadWeb.hpp"
#include "../Compiler/Components/Screen/ScreenWeb.hpp"
#include "../Compiler/Components/ScrollView/ScrollViewWeb.hpp"
#include "../Compiler/Components/Text/TextWeb.hpp"
#include "../Compiler/Foundation/FoundationPlatform.hpp"
#include "../Compiler/RoboCompilerShared.hpp"
#include "../Compiler/RoboFoundation.hpp"
#include "../Compiler/RoboXML.hpp"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace Robix::UI {
namespace {

fs::path FindUpFrom(fs::path dir, const fs::path& relative) {
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

fs::path FindUp(const fs::path& relative) {
    std::error_code error;
    fs::path found = FindUpFrom(fs::current_path(error), relative);
    if (!found.empty()) {
        return found;
    }
#ifdef _WIN32
    wchar_t executable[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        found = FindUpFrom(fs::path(executable).parent_path(), relative);
    }
#endif
    return found;
}

std::string ReadText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool WriteText(const fs::path& path, const std::string& body) {
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << body;
    return static_cast<bool>(out);
}

std::string Escape(const std::string& text) {
    std::string out;
    for (char ch : text) {
        if (ch == '&') {
            out += "&amp;";
        } else if (ch == '<') {
            out += "&lt;";
        } else if (ch == '>') {
            out += "&gt;";
        } else if (ch == '"') {
            out += "&quot;";
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

std::string Hex(RoboU32 color) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02X%02X%02X", (color >> 16) & 255u, (color >> 8) & 255u, color & 255u);
    return buffer;
}

void LowerKeys(RoboXMLNode& node) {
    std::vector<std::pair<std::string, std::string>> extra;
    for (const auto& entry : node.attrs) {
        const std::string lower = Lower(entry.first);
        if (node.attrs.find(lower) == node.attrs.end()) {
            extra.emplace_back(lower, entry.second);
        }
    }
    for (const auto& entry : extra) {
        node.attrs[entry.first] = entry.second;
    }
    for (RoboXMLNode& child : node.children) {
        LowerKeys(child);
    }
}

std::string CssSize(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    for (char ch : value) {
        if (ch < '0' || ch > '9') {
            return value;
        }
    }
    return value + "px";
}

void EmitNode(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library) {
    const std::string name = Lower(node.name);
    if (name == "at") {
        return;
    }
    if (name.empty() || name == "screen") {
        for (const RoboXMLNode& child : node.children) {
            EmitNode(html, child, library);
        }
        return;
    }
    if (name == "button") {
        fs::path assets = FindUp("Assets");
        if (assets.empty()) {
            assets = FindUp("RefCode/Assets");
        }
        RoboWebEmitButton(html, node, library, assets);
        return;
    }
    if (name == "iconbutton") {
        fs::path assets = FindUp("Assets");
        if (assets.empty()) {
            assets = FindUp("RefCode/Assets");
        }
        RoboWebEmitIconButton(html, node, library, assets);
        return;
    }
    if (name == "switch") {
        fs::path assets = FindUp("Assets");
        if (assets.empty()) {
            assets = FindUp("RefCode/Assets");
        }
        RoboWebEmitSwitch(html, node, library, assets);
        return;
    }
    if (name == "switchitem") {
        fs::path assets = FindUp("Assets");
        if (assets.empty()) {
            assets = FindUp("RefCode/Assets");
        }
        RoboWebEmitSwitchItem(html, node, library, assets);
        return;
    }
    if (name == "switchgroup") {
        fs::path assets = FindUp("Assets");
        if (assets.empty()) {
            assets = FindUp("RefCode/Assets");
        }
        bool warnedVideo = false;
        RoboWebEmitSwitchGroup(html, node, library, {}, assets, warnedVideo, nullptr, 0);
        return;
    }
    if (name == "segmentedcontrol") {
        fs::path assets = FindUp("Assets");
        if (assets.empty()) {
            assets = FindUp("RefCode/Assets");
        }
        RoboWebEmitSegmentedControl(html, node, library, assets);
        return;
    }
    if (name == "segment") {
        return;
    }
    if (name == "segmentedtoolbar") {
        fs::path assets = FindUp("Assets");
        if (assets.empty()) {
            assets = FindUp("RefCode/Assets");
        }
        bool warnedVideo = false;
        RoboWebEmitSegmentedToolbar(html, node, library, {}, assets, warnedVideo, nullptr, 0);
        return;
    }
    if (name == "icon" || name == "svg" || RoboCompilerIsIconNode(node)) {
        fs::path assets = FindUp("Assets");
        if (assets.empty()) {
            assets = FindUp("RefCode/Assets");
        }
        std::string iconSize = RoboXMLAttr(node, "size");
        if (iconSize.empty()) {
            iconSize = RoboXMLAttr(node, "width", "24");
        }
        std::string style = RoboWebStyle(node, library);
        if (!iconSize.empty()) {
            const std::string sized = CssSize(iconSize);
            RoboWebAddDecl(style, "width", sized);
            RoboWebAddDecl(style, "height", sized);
        }
        const std::string height = RoboXMLAttr(node, "height");
        if (!height.empty() && height != iconSize) {
            RoboWebAddDecl(style, "height", CssSize(height));
        }
        RoboWebAppendMotion(style, node);
        html += "<div class=\"";
        html += name == "svg" ? RoboWebSvgClass() : RoboWebIconClass();
        html += "\"";
        const std::string id = RoboXMLAttr(node, "id");
        if (!id.empty()) {
            html += " id=\"";
            html += Escape(id);
            html += "\"";
        }
        if (!style.empty()) {
            html += " style=\"";
            html += Escape(style);
            html += "\"";
        }
        html += RoboWebMotionAttrs(node);
        html += ">";
        html += RoboWebIconMarkup(assets, node);
        if (name != "icon" && name != "svg") {
            html += RoboWebSvg(assets, node.name);
        }
        html += "</div>";
        return;
    }
    const char* textTag = RoboWebTextTag(name);
    const char* tag = textTag != nullptr ? textTag : "div";
    if (name == "heading") {
        const std::string level = Lower(RoboXMLAttr(node, "level", "1"));
        if (level == "2") {
            tag = "h2";
        } else if (level == "3") {
            tag = "h3";
        } else if (level == "4") {
            tag = "h4";
        } else if (level == "5") {
            tag = "h5";
        } else if (level == "6") {
            tag = "h6";
        } else {
            tag = "h1";
        }
    }
    std::string className;
    if (name == "board") {
        className = RoboWebBoardClass();
    } else if (name == "scrollbar") {
        className = std::string(RoboWebScrollbarClass()) + " is-needed";
        if (Lower(RoboXMLAttr(node, "orientation")) == "horizontal") {
            className += " is-x";
        }
    } else if (RoboWebIsContainerBox(name)) {
        className = RoboWebContainerClass();
    } else {
        std::string textClass = RoboWebTextClass(name);
        if (textClass.empty()) textClass = RoboWebHeadingClass(name);
        if (textClass.empty()) textClass = RoboWebSubheadClass(name);
        if (textClass.empty()) textClass = RoboWebBodyClass(name);
        if (textClass.empty()) textClass = RoboWebParagraphClass(name);
        if (textClass.empty()) textClass = RoboWebCaptionClass(name);
        if (textClass.empty()) textClass = RoboWebEyebrowClass(name);
        if (textClass.empty()) textClass = RoboWebLabelClass(name);
        className = textClass.empty() ? "robo-" + name : textClass;
    }
    if (RoboWebIsTextName(name)) {
        const std::string typeClass = RoboWebTypeClass(node);
        if (!typeClass.empty()) {
            className += " ";
            className += typeClass;
        }
        className += " robo-type";
    }
    html += "<";
    html += tag;
    const std::string id = RoboXMLAttr(node, "id");
    if (!id.empty()) {
        html += " id=\"";
        html += Escape(id);
        html += "\"";
    }
    html += " class=\"";
    html += Escape(className);
    html += "\"";
    if (name == "scrollbar") {
        if (RoboXMLAttr(node, "autoHide").empty()) {
            html += " data-autohide=\"true\"";
        } else {
            html += " data-autohide=\"";
            html += Escape(RoboXMLAttr(node, "autoHide"));
            html += "\"";
        }
        if (RoboXMLAttr(node, "arrows").empty()) {
            html += " data-arrows=\"always\"";
        } else {
            html += " data-arrows=\"";
            html += Escape(RoboXMLAttr(node, "arrows"));
            html += "\"";
        }
    }
    if (RoboWebIsContainerBox(name)) {
        RoboWebContainerAttrs(html, node);
    }
    html += RoboWebScreenAttrs(node, library, "block");
    html += RoboWebMotionAttrs(node);
    if (RoboWebIsTextName(name)) {
        const auto data = [&](const char* key, const char* attrName) {
            const std::string value = RoboXMLAttr(node, attrName);
            if (value.empty()) {
                return;
            }
            html += " data-";
            html += key;
            html += "=\"";
            html += Escape(value);
            html += "\"";
        };
        data("size", "size");
        data("level", "level");
        data("font", "font");
        RoboWebTextAttrs(html, node);
    }
    std::string style = RoboWebStyle(node, library);
    if (name == "board") {
        const std::string display = Lower(RoboXMLAttr(node, "display", "canvas"));
        RoboWebAddDecl(style, "display", display == "canvas" || display == "flex" ? "flex" : display);
        RoboWebAddDecl(style, "flex-direction", Lower(RoboXMLAttr(node, "direction", "column")));
        RoboWebAddDecl(style, "gap", RoboWebCssValue(library, RoboXMLAttr(node, "gap"), false));
        RoboWebAddDecl(style, "padding", RoboWebCssValue(library, RoboXMLAttr(node, "padding"), false));
    }
    if (RoboWebIsContainerBox(name)) {
        RoboWebAppendContainerStyle(style, node, library);
    }
    if (!RoboWebIsContainerBox(name)) {
        RoboWebAppendTextStyle(style, node, library);
    }
    RoboWebAppendMotion(style, node);
    if (!style.empty()) {
        html += " style=\"";
        html += Escape(style);
        html += "\"";
    }
    html += ">";
    if (name == "scrollbar") {
        html += "<button type=\"button\" class=\"robo-scroll-arrow is-up\" aria-label=\"Scroll up\"></button>";
        html += "<div class=\"robo-scroll-track\"><div class=\"robo-scroll-thumb\"></div></div>";
        html += "<button type=\"button\" class=\"robo-scroll-arrow is-down\" aria-label=\"Scroll down\"></button></div>";
        return;
    }
    if (RoboWebIsContainerBox(name)) {
        html += RoboWebContainerVideo(node);
    }
    std::string body = node.text;
    if (RoboWebIsTextName(name)) {
        bool blank = true;
        for (char ch : body) {
            if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
                blank = false;
                break;
            }
        }
        if (blank) {
            body = RoboXMLAttr(node, "content");
            if (body.empty()) body = RoboXMLAttr(node, "text");
            if (body.empty()) body = RoboXMLAttr(node, "value");
        }
    }
    if (!body.empty()) {
        html += Escape(body);
    }
    for (const RoboXMLNode& child : node.children) {
        EmitNode(html, child, library);
    }
    html += "</";
    html += tag;
    html += ">";
}

const RoboXMLNode* FindScreen(const RoboXMLNode& node) {
    const std::string name = Lower(node.name);
    if (name == "screen" || name == "window" || name == "page" || name == "board") {
        return &node;
    }
    for (const RoboXMLNode& child : node.children) {
        if (const RoboXMLNode* found = FindScreen(child)) {
            return found;
        }
    }
    return nullptr;
}

void CopyFont(const fs::path& source, const fs::path& destination) {
    if (source.empty() || !fs::is_regular_file(source)) {
        return;
    }
    std::error_code error;
    fs::create_directories(destination.parent_path(), error);
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);
}

}  // namespace

std::string AssetsFontsRoot() {
    const fs::path found = FindUp("Assets/Fonts");
    return found.empty() ? std::string("Assets/Fonts") : found.string();
}

bool WriteWebFoundation(const fs::path& outRoot, const fs::path& document, std::string& error) {
    return WriteWebFoundation(outRoot, document, error, std::string{});
}

bool WriteWebFoundation(const fs::path& outRoot, const fs::path& document, std::string& error, const std::string& programScript) {
    const fs::path foundationPath = FindUp("RobixUI/Compiler/Foundation/RoboFoundation.xml");
    RoboFoundationLibrary library;
    std::string loadError;
    if (foundationPath.empty() || !RoboFoundationLoadFile(foundationPath.string(), library, true, loadError)) {
        error = loadError.empty() ? "foundation file was not found" : loadError;
        return false;
    }

    std::string title = "Robix";
    std::string theme = "dark";
    RoboXMLNode screenCopy;
    bool hasScreen = false;
    RoboWebScreenReset("");
    RoboWebMotionReset();
    if (!document.empty()) {
        RoboXMLDocument xml;
        const std::string source = ReadText(document);
        if (!source.empty() && RoboXMLParse(source, xml)) {
            RoboUICompose(xml.root, document.string());
            LowerKeys(xml.root);
            if (const RoboXMLNode* screen = FindScreen(xml.root)) {
                screenCopy = *screen;
                hasScreen = true;
                const std::string named = RoboXMLAttr(screenCopy, "name", RoboXMLAttr(screenCopy, "title", "").c_str());
                if (!named.empty()) {
                    title = named;
                }
                const std::string namedTheme = Lower(RoboXMLAttr(screenCopy, "theme", RoboXMLAttr(screenCopy, "appearance", "dark").c_str()));
                theme = namedTheme == "light" ? "light" : "dark";
                RoboWebScreenReset(RoboXMLAttr(screenCopy, "sizes"));
            }
        }
    }

    std::ostringstream fontCss;
    const fs::path fontRoot = FindUp("Assets/Fonts");
    for (const RoboFoundationSet& set : library.sets) {
        for (const auto& entry : set.tokens) {
            const RoboFoundationToken& token = entry.second;
            if (token.kind != RoboFoundationKind::Font || token.value.empty()) {
                continue;
            }
            const std::string webPath = "Assets/Fonts/" + token.value;
            if (!fontRoot.empty()) {
                CopyFont(fontRoot / token.value, outRoot / webPath);
            }
            const std::string face = Lower(token.name);
            int weight = 400;
            const char* style = "normal";
            if (face.find("semibold") != std::string::npos) {
                weight = 600;
            } else if (face.find("medium") != std::string::npos) {
                weight = 500;
            } else if (face.find("bold") != std::string::npos) {
                weight = 700;
            }
            if (face.find("italic") != std::string::npos) {
                style = "italic";
            }
            fontCss << "@font-face{font-family:\"Inter\";src:url(\"" << webPath
                    << "\") format(\"truetype\");font-weight:" << weight << ";font-style:" << style
                    << ";font-display:swap}\n";
        }
    }

    std::ostringstream common;
    std::ostringstream dark;
    std::ostringstream light;
    std::ostringstream typeRules;
    std::ostringstream mobile;
    for (const RoboFoundationSet& set : library.sets) {
        for (const auto& entry : set.tokens) {
            const RoboFoundationToken& token = entry.second;
            if (token.kind == RoboFoundationKind::Color) {
                dark << "  --" << token.name << ":" << token.dark << ";\n";
                light << "  --" << token.name << ":" << (token.light.empty() ? token.dark : token.light) << ";\n";
            } else if (token.kind == RoboFoundationKind::Space || token.kind == RoboFoundationKind::Radius) {
                const std::string value = !token.value.empty() && token.value.find_first_not_of("0123456789") == std::string::npos
                                              ? token.value + "px"
                                              : token.value;
                common << "  --" << token.name << ":" << value << ";\n";
            } else if (token.kind == RoboFoundationKind::Font) {
                common << "  --font-" << token.name << ":\"Inter\",system-ui,sans-serif;\n";
            } else if (token.kind == RoboFoundationKind::Type) {
                common << "  --" << token.name << "-size:" << (token.size.empty() ? "16" : token.size) << "px;\n";
                common << "  --" << token.name << "-weight:" << (token.weight.empty() ? "400" : token.weight) << ";\n";
                common << "  --" << token.name << "-tracking:" << (token.tracking.empty() ? "0" : token.tracking) << ";\n";
                common << "  --" << token.name << "-leading:" << (token.leading.empty() ? "1.4" : token.leading) << ";\n";
                const std::string mobileSize = token.mobileSize.empty() ? token.size : token.mobileSize;
                if (!mobileSize.empty() && mobileSize != token.size) {
                    mobile << "  .robo-type-" << token.name << "{font-size:" << mobileSize << "px}\n";
                }
                typeRules << ".robo-type-" << token.name
                          << "{font-family:var(--robo-font,inherit);font-size:var(--robo-font-size,var(--" << token.name
                          << "-size));font-weight:var(--robo-font-weight,var(--" << token.name
                          << "-weight));font-style:var(--robo-font-style,normal);letter-spacing:var(--robo-tracking,calc(var(--"
                          << token.name << "-tracking) * 1em));line-height:var(--robo-leading,var(--" << token.name
                          << "-leading));word-spacing:var(--robo-word-spacing,normal)}\n";
            }
        }
    }

    std::ostringstream css;
    css << fontCss.str();
    css << ":root{\n" << common.str() << "}\nhtml[data-theme=\"dark\"]{\n"
        << dark.str() << "}\nhtml[data-theme=\"light\"],.robo-window[data-theme=\"light\"]{\n"
        << light.str() << "}\n";
    css << "html,body{margin:0;height:100%;overflow:hidden;background:var(--bg-primary);border:0;outline:none;box-shadow:none}\n";
    css << "html[data-theme=\"dark\"]{color-scheme:dark}\n";
    css << "html[data-theme=\"light\"]{color-scheme:light}\n";
    css << "html:focus,html:focus-visible,body:focus,body:focus-visible{outline:none;box-shadow:none}\n";
    css << "body{color:var(--text-primary);font-family:var(--font-sans,system-ui,sans-serif)}\n";
    css << ".robo-screen{position:relative;min-height:100%;box-sizing:border-box;z-index:1;background:var(--bg-primary);color:var(--text-primary);border:0;outline:none;box-shadow:none}\n";
    css << ".robo-page{height:100%;overflow:auto;scrollbar-width:none;background:var(--bg-primary);color:var(--text-primary);border:0;outline:none;box-shadow:none}\n";
    css << ".robo-screen:focus,.robo-screen:focus-visible,.robo-page:focus,.robo-page:focus-visible{outline:none;box-shadow:none}\n";
    css << ".robo-page::-webkit-scrollbar{width:0;height:0}\n";
    css << typeRules.str();
    css << "@media (max-width:734px){\n" << mobile.str() << "}\n";
    if (hasScreen) {
        RoboWebAppendScreenCss(css);
        RoboWebAppendContainerCss(css);
        RoboWebAppendTextCss(css);
        RoboWebAppendButtonCss(css);
        RoboWebAppendIconButtonCss(css);
        RoboWebAppendSwitchCss(css);
        RoboWebAppendSegmentedCss(css);
        RoboWebAppendControlCss(css);
        RoboWebAppendIconCss(css);
        RoboWebAppendScrollCss(css);
    }

    std::ostringstream html;
    html << "<!DOCTYPE html><html lang=\"en\" data-theme=\"" << Escape(theme) << "\"><head><meta charset=\"utf-8\">";
    html << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html << "<title>" << Escape(title) << "</title>";
    html << "<link rel=\"stylesheet\" href=\"app.css\"></head><body>";
    if (hasScreen) {
        const std::string id = RoboXMLAttr(screenCopy, "id", "robo-app");
        const std::string place = RoboXMLAttr(screenCopy, "place", "center");
        const std::string movable = RoboXMLAttr(screenCopy, "movable", "true");
        const std::string width = CssSize(RoboXMLAttr(screenCopy, "width"));
        const std::string height = CssSize(RoboXMLAttr(screenCopy, "height"));
        const std::string screenDisplay = Lower(RoboXMLAttr(screenCopy, "display"));
        const std::string screenDirection = Lower(RoboXMLAttr(screenCopy, "direction"));
        std::string style = "box-sizing:border-box";
        if (screenDisplay.empty() && screenDirection.empty()) {
            style += ";display:flex;flex-direction:column";
        }
        const std::string layout = RoboWebStyle(screenCopy, library);
        if (!layout.empty()) {
            style += ";";
            style += layout;
        }
        if (!width.empty()) {
            style += ";width:" + width;
        } else {
            style += ";width:100%";
        }
        if (!height.empty()) {
            style += ";height:" + height;
        } else {
            style += ";min-height:100%";
        }
        RoboWebAppendMotion(style, screenCopy);
        html << "<" << RoboWebScreenTag(true) << " id=\"" << Escape(id) << "\" class=\"" << RoboWebScreenClass(true)
             << " robo-page\" data-place=\"" << Escape(place) << "\" data-theme=\"" << Escape(theme)
             << "\" data-movable=\"" << Escape(movable) << "\"" << RoboWebScreenAttrs(screenCopy, library, "flex")
             << RoboWebMotionAttrs(screenCopy)
             << " style=\"" << Escape(style) << "\">";
        std::string body;
        for (const RoboXMLNode& child : screenCopy.children) {
            EmitNode(body, child, library);
        }
        html << body;
        html << "</" << RoboWebScreenTag(true) << ">";
        std::cout << "component Screen\n";
    } else {
        html << "<div id=\"robo-app\" class=\"robo-screen\"><p class=\"robo-type-body\">" << Escape(title) << "</p></div>";
    }
    html << "<script src=\"app.js\"></script></body></html>\n";

    std::string js = programScript.empty() ? R"JS(window.Robo = window.Robo || {};
Robo.status = 0;
Robo.render = function () {};
Robo.mount = function () {};
Robo.set = function (id, key, value) {
  var node = document.getElementById(id);
  if (!node) return;
  if (key === "text" || key === "content") node.textContent = value;
  else if (key === "hidden") node.hidden = value === "true" || value === true;
  else node.style[key] = value;
};
)JS" : programScript;
    js += RoboWebScrollScript();
    js += RoboWebIconButtonScript();
    js += RoboWebSwitchScript();
    js += RoboWebSegmentedScript();
    js += "Robo.mount();\nif (Robo.start) Robo.start();\n";
    js += RoboWebMotionScript();

    if (!WriteText(outRoot / "index.html", html.str()) || !WriteText(outRoot / "app.css", css.str() + RoboWebScreenCss() + RoboWebMotionCss()) ||
        !WriteText(outRoot / "app.js", js)) {
        error = "failed to write the web package";
        return false;
    }
    std::cout << "foundation bg-primary light " << Hex(RoboFoundationPlatformColorForTheme("bg-primary", ROBO_TRUE))
              << " dark " << Hex(RoboFoundationPlatformColorForTheme("bg-primary", ROBO_FALSE)) << "\n";
    std::cout << "web " << (outRoot / "index.html").string() << "\n";
    return true;
}

void WebEmitNode(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library) {
    EmitNode(html, node, library);
}

}  // namespace Robix::UI

void RoboWebEmit(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                 const std::string& source, const std::filesystem::path& assetsRoot, bool root, bool& warnedVideo,
                 RoboWebPackage* package, int depth) {
    (void)source;
    (void)assetsRoot;
    (void)root;
    (void)warnedVideo;
    (void)package;
    (void)depth;
    Robix::UI::WebEmitNode(html, node, library);
}
