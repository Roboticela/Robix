#include "WebFoundation.hpp"

#include "../../Robix3D/Robix3D.hpp"

#include "../Compiler/Components/Container/Container.hpp"
#include "../Compiler/Components/Container/ContainerWeb.hpp"
#include "../Compiler/Components/Image/ImageWeb.hpp"
#include "../Compiler/Components/Svg/SvgWeb.hpp"
#include "../Compiler/Components/Screen/Screen.hpp"
#include "../Compiler/Compose.hpp"
#include "../Compiler/Motion.hpp"
#include "../Compiler/ScreenSize.hpp"
#include "../Compiler/Components/Screen/ScreenWeb.hpp"
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

bool UsesCountrySelect(const RoboXMLNode& node) {
    if (Lower(node.name) == "countryselect") {
        return true;
    }
    for (const RoboXMLNode& child : node.children) {
        if (UsesCountrySelect(child)) {
            return true;
        }
    }
    return false;
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

void EmitNode(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library, int depth) {
    RoboXMLNode prepared = node;
    if (RoboUIRewritePreset(prepared)) {
        EmitNode(html, prepared, library, depth);
        return;
    }
    const std::string name = Lower(prepared.name);
    const std::string alias = Lower(RoboXMLAttr(prepared, "alias"));
    const std::string paint = alias.empty() ? name : alias;
    if (name == "at") {
        return;
    }
    if (name.empty()) {
        for (const RoboXMLNode& child : node.children) {
            EmitNode(html, child, library, depth);
        }
        return;
    }
    if (name == "screen" || name == "window" || name == "page") {
        const std::string theme = Lower(RoboXMLAttr(node, "theme", RoboXMLAttr(node, "appearance", "").c_str()));
        std::string style = RoboWebStyle(node, library);
        RoboWebAddDecl(style, "box-sizing", "border-box");
        RoboWebAddDecl(style, "overflow", "hidden");
        if (RoboXMLAttr(node, "border").empty()) {
            RoboWebAddDecl(style, "border", "1px solid #2A3142");
        }
        html += "<div class=\"robo-screen robo-preview\"";
        if (!theme.empty()) {
            html += " data-theme=\"";
            html += Escape(theme);
            html += "\"";
        }
        if (!style.empty()) {
            html += " style=\"";
            html += Escape(style);
            html += "\"";
        }
        html += ">";
        for (const RoboXMLNode& child : node.children) {
            EmitNode(html, child, library, depth + 1);
        }
        html += "</div>";
        return;
    }
    if (name == "view3d" || name == "scene3d") {
        Robix::D3::EmitView(html, node);
        return;
    }
    if (name == "image") {
        RoboWebEmitImage(html, node, library);
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
        html += "<div class=\"";
        html += name == "svg" ? "robo-svg" : "robo-icon";
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
        RoboWebBehaviorAttrs(html, node);
        html += ">";
        html += RoboWebIconMarkup(assets, node);
        if (name != "icon" && name != "svg") {
            html += RoboWebSvg(assets, node.name);
        }
        html += "</div>";
        return;
    }
    const std::string tag = RoboWebElementTag(prepared);
    std::string className;
    if (paint == "board") {
        className = "robo-board";
    } else if (paint == "scrollbar") {
        className = "robo-scrollbar is-needed";
        if (Lower(RoboXMLAttr(node, "orientation")) == "horizontal") {
            className += " is-x";
        }
    } else if (paint == "scrollview" || paint == "scroll") {
        className = "robo-scroll";
    } else if (RoboWebIsContainerBox(name) || RoboWebIsContainerBox(paint)) {
        className = RoboWebContainerClass();
    } else {
        std::string textClass = RoboWebTextClass(paint);
        if (textClass.empty()) textClass = RoboWebTextRoleClass(paint);
        className = textClass.empty() ? "robo-" + paint : textClass;
    }
    const std::string role = Lower(RoboXMLAttr(node, "role"));
    if ((role == "textbox" || role == "searchbox") && className.find("robo-field") == std::string::npos) {
        className += " robo-field";
    }
    if (!alias.empty() && className.find("robo-" + alias) == std::string::npos) {
        className += " robo-";
        className += alias;
    }
    const std::string overlay = Lower(RoboXMLAttr(node, "overlay"));
    if (overlay == "true" || overlay == "1" || overlay == "yes") {
        className += " robo-overlay";
    }
    if (RoboWebIsTextName(paint) || RoboWebIsTextName(name)) {
        const std::string typeClass = RoboWebTypeClass(node);
        if (!typeClass.empty()) {
            className += " ";
            className += typeClass;
        }
        className += " robo-type";
    }
    html += "<";
    html += tag;
    std::string id = RoboXMLAttr(node, "id");
    if (id.empty()) id = RoboXMLAttr(node, "name");
    if (!id.empty()) {
        html += " id=\"";
        html += Escape(id);
        html += "\"";
        html += " data-name=\"";
        html += Escape(RoboXMLAttr(node, "name").empty() ? id : RoboXMLAttr(node, "name"));
        html += "\"";
    }
    html += " class=\"";
    html += Escape(className);
    html += "\"";
    RoboWebBehaviorAttrs(html, node);
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
    if (name == "scrollview" || name == "scroll") {
        const auto scrollData = [&](const char* key, const std::string& value) {
            if (value.empty()) {
                return;
            }
            html += " data-";
            html += key;
            html += "=\"";
            html += Escape(value);
            html += "\"";
        };
        scrollData("wheel", RoboXMLAttr(node, "wheel"));
        scrollData("touchpad", RoboXMLAttr(node, "touchpad"));
        scrollData("touch", RoboXMLAttr(node, "touch"));
        scrollData("keyboard", RoboXMLAttr(node, "keyboard"));
        scrollData("pointer", RoboXMLAttr(node, "pointer"));
        scrollData("scrollx", RoboXMLAttr(node, "scrollX").empty() ? RoboXMLAttr(node, "horizontal") : RoboXMLAttr(node, "scrollX"));
        scrollData("scrolly", RoboXMLAttr(node, "scrollY").empty() ? RoboXMLAttr(node, "vertical") : RoboXMLAttr(node, "scrollY"));
        scrollData("arrows", RoboXMLAttr(node, "arrows"));
        scrollData("autohide", RoboXMLAttr(node, "autoHide"));
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
    if (!style.empty()) {
        html += " style=\"";
        html += Escape(style);
        html += "\"";
    }
    if (tag == "input") {
        const auto inputAttr = [&](const char* htmlName, const char* key) {
            const std::string value = RoboXMLAttr(node, key);
            if (value.empty()) return;
            html += " ";
            html += htmlName;
            html += "=\"";
            html += Escape(value);
            html += "\"";
        };
        if (RoboXMLAttr(node, "type").empty()) {
            html += " type=\"text\"";
        }
        inputAttr("type", "type");
        inputAttr("value", "value");
        inputAttr("placeholder", "placeholder");
        html += ">";
        return;
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
        EmitNode(html, child, library, depth + 1);
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
    return WriteWebFoundation(outRoot, document, error, std::string{}, false);
}

bool WriteWebFoundation(const fs::path& outRoot, const fs::path& document, std::string& error, const std::string& programScript) {
    return WriteWebFoundation(outRoot, document, error, programScript, false);
}

bool WriteWebFoundation(const fs::path& outRoot, const fs::path& document, std::string& error, const std::string& programScript,
                       bool compileImages) {
    Robix::D3::Reset();
    Robix::D3::UseDocument(document, outRoot);
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
    RoboWebUseDocument(document);
    RoboWebImageReset(document, outRoot, compileImages);
    if (!document.empty()) {
        RoboXMLDocument xml;
        const std::string source = ReadText(document);
        if (!source.empty() && RoboXMLParse(source, xml)) {
            if (Robix::D3::IsSceneDocument(xml.root)) {
                return Robix::D3::WritePage(outRoot, document, error);
            }
            RoboUICompose(xml.root, document.string());
            LowerKeys(xml.root);
            const std::vector<std::string> boundary = Robix::D3::UiBoundaryErrors(xml.root);
            if (!boundary.empty()) {
                error = boundary.front();
                for (std::size_t index = 1; index < boundary.size(); ++index) error += "\n" + boundary[index];
                return false;
            }
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
    css << "html,body{margin:0;height:100%;overflow:auto;background:var(--bg-primary);border:0;outline:none;box-shadow:none}\n";
    css << "html[data-theme=\"dark\"]{color-scheme:dark}\n";
    css << "html[data-theme=\"light\"]{color-scheme:light}\n";
    css << "html:focus,html:focus-visible,body:focus,body:focus-visible{outline:none;box-shadow:none}\n";
    css << "body{color:var(--text-primary);font-family:var(--font-sans,system-ui,sans-serif)}\n";
    css << ".robo-screen{position:relative;min-height:100%;box-sizing:border-box;z-index:1;background:var(--bg-primary);color:var(--text-primary);border:0;outline:none;box-shadow:none}\n";
    css << ".robo-page{min-height:100%;height:auto;overflow:auto;scrollbar-width:none;background:var(--bg-primary);color:var(--text-primary);border:0;outline:none;box-shadow:none}\n";
    css << "[data-paint=bleed]{overflow:visible;contain:none}\n";
    css << "[data-paint=clip]{overflow:clip}\n";
    css << ".robo-page :has([data-paint=bleed]){overflow:visible;isolation:auto;contain:none}\n";
    css << ".robo-screen:focus,.robo-screen:focus-visible,.robo-page:focus,.robo-page:focus-visible{outline:none;box-shadow:none}\n";
    css << ".robo-page::-webkit-scrollbar{width:0;height:0}\n";
    css << typeRules.str();
    css << "@media (max-width:734px){\n" << mobile.str() << "}\n";
    if (hasScreen) {
        RoboWebAppendScreenCss(css);
        RoboWebAppendContainerCss(css);
        RoboWebAppendTextCss(css);
        RoboWebAppendImageCss(css);
        css << ".robo-overlay-host{position:fixed;inset:0;z-index:40;pointer-events:none}\n";
        css << ".robo-overlay-host .robo-overlay{pointer-events:auto}\n";
        css << ".robo-icon,.robo-svg{display:inline-flex;align-items:center;justify-content:center;line-height:0}\n";
        css << ".robo-icon svg,.robo-svg svg{width:100%;height:100%}\n";
    }

    std::ostringstream html;
    html << "<!DOCTYPE html><html lang=\"en\" data-theme=\"" << Escape(theme) << "\" class=\""
         << (theme == "light" ? "" : "dark") << "\"><head><meta charset=\"utf-8\">";
    html << "<script>try{var t=localStorage.getItem(\"theme\");if(t===\"light\"||t===\"dark\"){document.documentElement.setAttribute(\"data-theme\",t);document.documentElement.classList.toggle(\"dark\",t===\"dark\");}}catch(e){}</script>";
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
        if (width.empty()) {
            style += ";width:100%";
        }
        style += ";min-height:100%;height:auto;flex-shrink:0";
        (void)height;
        const std::string zoomControls =
            RoboXMLAttr(screenCopy, "zoomControls", RoboXMLAttr(screenCopy, "zoom-controls", "bottom").c_str());
        html << "<" << RoboWebScreenTag(true) << " id=\"" << Escape(id) << "\" class=\"" << RoboWebScreenClass(true)
             << " robo-page\" data-place=\"" << Escape(place) << "\" data-theme=\"" << Escape(theme)
             << "\" data-movable=\"" << Escape(movable) << "\" data-zoom-controls=\"" << Escape(zoomControls) << "\""
             << RoboWebScreenAttrs(screenCopy, library, "flex")
             << RoboWebMotionAttrs(screenCopy)
             << " style=\"" << Escape(style) << "\">";
        std::string body;
        std::string overlay;
        for (const RoboXMLNode& child : screenCopy.children) {
            const std::string layer = Lower(RoboXMLAttr(child, "overlay"));
            if (layer == "true" || layer == "1" || layer == "yes") {
                EmitNode(overlay, child, library, 0);
            } else {
                EmitNode(body, child, library, 0);
            }
        }
        html << body;
        html << "<div class=\"robo-overlay-host\">" << overlay << "</div>";
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
  if (!node) node = document.querySelector("[data-name=\"" + id + "\"]");
  if (!node) return;
  if (key === "text" || key === "content") {
    if (node.tagName === "INPUT" || node.tagName === "TEXTAREA") node.value = value;
    else node.textContent = value;
  } else if (key === "value") node.value = value;
  else if (key === "placeholder") node.placeholder = value;
  else if (key === "hidden") node.hidden = value === "true" || value === "1";
  else if (key === "disabled") node.disabled = value === "true" || value === "1";
  else if (key === "checked") node.checked = value === "true" || value === "1";
  else if (key === "color" || key === "background" || key === "display" || key === "opacity" || key === "width" || key === "height" || String(key).indexOf("--") === 0) node.style.setProperty(key, value);
  else node.setAttribute(key, value);
};
Robo.sceneSet = function (name, key, value) {
  var scenes = Robo.scenes || [];
  if (!scenes.length) {
    Robo.sceneQueue = Robo.sceneQueue || [];
    Robo.sceneQueue.push([name, key, value]);
    return;
  }
  scenes.forEach(function (scene) { if (scene.set) scene.set(name, key, value); });
};
)JS" : programScript;
    js += "Robo.mount();\nif (Robo.start) Robo.start();\n";
    js += RoboWebMotionScript();
    js += RoboWebReelScript();
    js += RoboWebImageScript();

    if (!Robix::D3::CommitViews(outRoot, error)) return false;
    std::string page = html.str();
    const std::string sceneMap = Robix::D3::ModuleSource();
    if (!sceneMap.empty()) {
        const auto head = page.find("</head>");
        if (head != std::string::npos) page.insert(head, sceneMap);
        const auto body = page.find("</body>");
        if (body != std::string::npos) page.insert(body, "<script type=\"module\" src=\"robix3d.scene.js\"></script>");
        css << Robix::D3::CssSource();
    }
    if (!WriteText(outRoot / "index.html", page) || !WriteText(outRoot / "app.css", css.str() + RoboWebScreenCss() + RoboWebMotionCss()) ||
        !WriteText(outRoot / "app.js", js)) {
        error = "failed to write the web package";
        return false;
    }
    std::cout << "foundation bg-primary light " << Hex(RoboFoundationPlatformColorForTheme("bg-primary", ROBO_TRUE))
              << " dark " << Hex(RoboFoundationPlatformColorForTheme("bg-primary", ROBO_FALSE)) << "\n";
    std::cout << "web " << (outRoot / "index.html").string() << "\n";
    return true;
}

void WebEmitNode(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library, int depth) {
    EmitNode(html, node, library, depth);
}

}  // namespace Robix::UI

void RoboWebNoteFamily(RoboWebPackage* package, const std::string& name) {
    if (package != nullptr && !name.empty()) {
        package->used.insert(name);
    }
}

std::string RoboWebCopyAsset(RoboWebPackage& package, const std::string& src, const std::string& webPath) {
    const std::string published = webPath.empty() ? src : webPath;
    if (src.empty()) {
        return published;
    }
    package.copied.insert(published);
    if (package.options == nullptr || package.options->outputRoot.empty()) {
        return published;
    }
    std::filesystem::path from(src);
    if (!from.is_absolute()) {
        const std::filesystem::path inApp = std::filesystem::path(package.options->appRoot) / from;
        const std::filesystem::path inAssets = std::filesystem::path(package.options->assetsRoot) / from;
        if (std::filesystem::is_regular_file(inApp)) {
            from = inApp;
        } else if (std::filesystem::is_regular_file(inAssets)) {
            from = inAssets;
        }
    }
    if (!std::filesystem::is_regular_file(from)) {
        return published;
    }
    const std::filesystem::path dest = std::filesystem::path(package.options->outputRoot) / std::filesystem::path(published);
    std::error_code error;
    std::filesystem::create_directories(dest.parent_path(), error);
    std::filesystem::copy_file(from, dest, std::filesystem::copy_options::overwrite_existing, error);
    return published;
}

void RoboWebEmit(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                 const std::string& source, const std::filesystem::path& assetsRoot, bool root, bool& warnedVideo,
                 RoboWebPackage* package, int depth) {
    (void)source;
    (void)assetsRoot;
    (void)root;
    (void)warnedVideo;
    (void)package;
    Robix::UI::WebEmitNode(html, node, library, depth);
}
