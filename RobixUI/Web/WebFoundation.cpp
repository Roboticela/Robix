#include "WebFoundation.hpp"

#include "../../Robix3D/Robix3D.hpp"

#include "../Compiler/Components/Container/Container.hpp"
#include "../Compiler/Components/Container/ContainerWeb.hpp"
#include "../Compiler/Components/Canvas/CanvasWeb.hpp"
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

fs::path gAudioDocument;
fs::path gAudioOut;

std::string Lower(std::string text);

std::string PublishAudio(const std::string& source) {
    if (source.empty()) {
        return source;
    }
    const std::string key = Lower(source);
    if (key.rfind("http://", 0) == 0 || key.rfind("https://", 0) == 0) {
        return source;
    }
    fs::path file(source);
    if (file.is_relative() && !gAudioDocument.empty()) {
        file = gAudioDocument.parent_path() / file;
    }
    std::error_code error;
    if (gAudioOut.empty() || !fs::is_regular_file(file, error)) {
        return source;
    }
    const fs::path folder = gAudioOut / "audio";
    fs::create_directories(folder, error);
    const fs::path dest = folder / file.filename();
    fs::copy_file(file, dest, fs::copy_options::overwrite_existing, error);
    if (error) {
        return source;
    }
    return "audio/" + file.filename().string();
}

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
    if (name == "canvas" && RoboWebEmitCanvas(html, node)) {
        return;
    }
    if (name == "image") {
        RoboWebEmitImage(html, node, library);
        return;
    }
    if (name == "audio") {
        const std::string source = PublishAudio(RoboXMLAttr(node, "source").empty() ? RoboXMLAttr(node, "src") : RoboXMLAttr(node, "source"));
        const std::string playing = Lower(RoboXMLAttr(node, "playing"));
        const bool play = playing == "true" || playing == "yes" || playing == "on" || playing == "1";
        html += "<audio class=\"robo-audio\"";
        if (play) {
            html += " autoplay";
        }
        if (!source.empty()) {
            html += " src=\"";
            html += Escape(source);
            html += "\"";
        }
        const std::string volume = RoboXMLAttr(node, "volume");
        if (!volume.empty()) {
            html += " data-volume=\"";
            html += Escape(volume);
            html += "\"";
        }
        const std::string time = RoboXMLAttr(node, "time");
        if (!time.empty() && time != "0") {
            html += " data-time=\"";
            html += Escape(time);
            html += "\"";
        }
        html += "></audio>";
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
    } else if (paint == "list") {
        className = "robo-list";
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
    if (tag == "input" || tag == "textarea") {
        className = "robo-field";
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
    if (!RoboWebIsTextName(name)) {
        const std::string selectable = Lower(RoboXMLAttr(node, "selectable").empty() ? RoboXMLAttr(node, "userSelect") : RoboXMLAttr(node, "selectable"));
        const std::string picked = Lower(selectable);
        if (picked == "false" || picked == "0" || picked == "no" || picked == "off" || picked == "none") {
            html += " data-select=\"none\"";
        } else if (picked == "all") {
            html += " data-select=\"all\"";
        } else if (picked == "true" || picked == "1" || picked == "yes" || picked == "on" || picked == "text") {
            html += " data-select=\"text\"";
        }
    }
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
        const std::string offset = RoboXMLAttr(node, "offset");
        const std::string offsetX = RoboXMLAttr(node, "offsetx").empty() ? RoboXMLAttr(node, "offsetX") : RoboXMLAttr(node, "offsetx");
        const std::string offsetY = RoboXMLAttr(node, "offsety").empty() ? RoboXMLAttr(node, "offsetY") : RoboXMLAttr(node, "offsety");
        const std::string axis = Lower(RoboXMLAttr(node, "axis").empty() ? "vertical" : RoboXMLAttr(node, "axis"));
        if (!offsetX.empty()) {
            scrollData("offset-x", offsetX);
        } else if (axis == "horizontal" && !offset.empty()) {
            scrollData("offset-x", offset);
        }
        if (!offsetY.empty()) {
            scrollData("offset-y", offsetY);
        } else if (axis != "horizontal" && !offset.empty()) {
            scrollData("offset-y", offset);
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
    if (name == "list") {
        RoboWebAddDecl(style, "display", "flex");
        RoboWebAddDecl(style, "flex-direction", "column");
        RoboWebAddDecl(style, "margin", "0");
        RoboWebAddDecl(style, "padding", "0");
        RoboWebAddDecl(style, "list-style", "none");
    }
    if (name == "scrollview" || name == "scroll") {
        RoboWebAddDecl(style, "position", "relative");
        const std::string axis = Lower(RoboXMLAttr(node, "axis").empty() ? "vertical" : RoboXMLAttr(node, "axis"));
        if (axis == "horizontal") {
            RoboWebAddDecl(style, "overflow-x", "auto");
            RoboWebAddDecl(style, "overflow-y", "hidden");
        } else if (axis == "both") {
            RoboWebAddDecl(style, "overflow", "auto");
        } else {
            RoboWebAddDecl(style, "overflow-x", "hidden");
            RoboWebAddDecl(style, "overflow-y", "auto");
        }
    }
    if (name == "input" || name == "textfield") {
        const std::string caret = RoboXMLAttr(node, "cursorcolor").empty() ? RoboXMLAttr(node, "cursor-color") : RoboXMLAttr(node, "cursorcolor");
        if (!caret.empty()) {
            RoboWebAddDecl(style, "--robo-caret", caret);
        }
    }
    if (!style.empty()) {
        html += " style=\"";
        html += Escape(style);
        html += "\"";
    }
    if (tag == "input" || tag == "textarea") {
        const auto inputAttr = [&](const char* htmlName, const char* key) {
            const std::string value = RoboXMLAttr(node, key);
            if (value.empty()) return;
            html += " ";
            html += htmlName;
            html += "=\"";
            html += Escape(value);
            html += "\"";
        };
        const std::string mask = Lower(RoboXMLAttr(node, "mask"));
        if (tag == "input") {
            if (mask == "password" || mask == "hidden" || mask == "*") {
                html += " type=\"password\"";
            } else if (RoboXMLAttr(node, "type").empty()) {
                html += " type=\"text\"";
            } else {
                inputAttr("type", "type");
            }
            inputAttr("value", "value");
        }
        inputAttr("placeholder", "placeholder");
        const std::string disabled = Lower(RoboXMLAttr(node, "disabled"));
        if (disabled == "true" || disabled == "yes" || disabled == "on" || disabled == "1") {
            html += " disabled";
        }
        html += ">";
        if (tag == "textarea") {
            const std::string value = RoboXMLAttr(node, "value");
            if (!value.empty()) {
                html += Escape(value);
            }
            html += "</textarea>";
        }
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
        const bool listItem = name == "list";
        if (listItem) {
            html += "<li";
            const std::string itemKey = RoboXMLAttr(child, "key");
            if (!itemKey.empty()) {
                html += " data-key=\"";
                html += Escape(itemKey);
                html += "\"";
            }
            html += ">";
        }
        EmitNode(html, child, library, depth + 1);
        if (listItem) {
            html += "</li>";
        }
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
    gAudioDocument = document;
    gAudioOut = outRoot;
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
    css << "html,body{margin:0;height:100%;overflow:hidden;background:var(--bg-primary);border:0;outline:none;box-shadow:none}\n";
    css << "html[data-theme=\"dark\"]{color-scheme:dark}\n";
    css << "html[data-theme=\"light\"]{color-scheme:light}\n";
    css << "html:focus,html:focus-visible,body:focus,body:focus-visible{outline:none;box-shadow:none}\n";
    css << "body{color:var(--text-primary);font-family:var(--font-sans,system-ui,sans-serif)}\n";
    css << ".robo-screen{position:relative;min-height:100%;box-sizing:border-box;z-index:1;background:var(--bg-primary);color:var(--text-primary);border:0;outline:none;box-shadow:none}\n";
    css << ".robo-page{width:100%;height:100%;max-height:100%;min-height:0;min-width:0;overflow:auto;scrollbar-width:none;background:var(--bg-primary);color:var(--text-primary);border:0;outline:none;box-shadow:none}\n";
    css << "[data-paint=bleed]{overflow:visible;contain:none}\n";
    css << "[data-paint=clip]{overflow:clip}\n";
    css << ".robo-page :has([data-paint=bleed]){overflow:visible;isolation:auto;contain:none}\n";
    css << ".robo-screen:focus,.robo-screen:focus-visible,.robo-page:focus,.robo-page:focus-visible{outline:none;box-shadow:none}\n";
    css << ".robo-page::-webkit-scrollbar{width:0;height:0}\n";
    css << ".robo-bars{position:fixed;inset:0;z-index:35;pointer-events:none}\n";
    css << ".robo-bar{position:absolute;pointer-events:auto;opacity:0}\n";
    css << ".robo-bar.is-on{opacity:1}\n";
    css << ".robo-bar::before{content:\"\";position:absolute;inset:-8px}\n";
    css << ".robo-bar.is-y{top:2px;right:2px;width:6px}\n";
    css << ".robo-bar.is-x{left:2px;bottom:2px;height:6px}\n";
    css << ".robo-bar-track{position:absolute;background:var(--border-default);border-radius:999px}\n";
    css << ".robo-bar.is-y .robo-bar-track{left:0;right:0}\n";
    css << ".robo-bar.is-x .robo-bar-track{top:0;bottom:0}\n";
    css << ".robo-bar-thumb{position:absolute;left:0;right:0;border-radius:999px;background:color-mix(in srgb,var(--border-strong) 45%,var(--text-tertiary))}\n";
    css << ".robo-bar.is-x .robo-bar-thumb{top:0;bottom:0;left:auto;right:auto}\n";
    css << ".robo-bar-thumb:hover,.robo-bar-arrow:hover{background:var(--text-tertiary)}\n";
    css << ".robo-bar-thumb:active,.robo-bar.is-drag .robo-bar-thumb,.robo-bar-arrow:active{background:color-mix(in srgb,var(--text-tertiary) 58%,var(--text-primary))}\n";
    css << ".robo-bar-arrow{position:absolute;width:6px;height:6px;padding:0;border:0;background:color-mix(in srgb,var(--border-strong) 45%,var(--text-tertiary))}\n";
    css << ".robo-bar.is-y .robo-bar-arrow.is-up{top:0;left:0;clip-path:polygon(50% 0,100% 100%,0 100%)}\n";
    css << ".robo-bar.is-y .robo-bar-arrow.is-down{bottom:0;left:0;clip-path:polygon(0 0,100% 0,50% 100%)}\n";
    css << ".robo-bar.is-x .robo-bar-arrow.is-left{left:0;top:0;clip-path:polygon(100% 0,0 50%,100% 100%)}\n";
    css << ".robo-bar.is-x .robo-bar-arrow.is-right{right:0;top:0;clip-path:polygon(0 0,100% 50%,0 100%)}\n";
    css << typeRules.str();
    css << "@media (max-width:734px){\n" << mobile.str() << "}\n";
    if (hasScreen) {
        RoboWebAppendScreenCss(css);
        RoboWebAppendContainerCss(css);
        RoboWebAppendTextCss(css);
        RoboWebAppendImageCss(css);
        RoboWebAppendCanvasCss(css);
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
        style += ";height:100%;max-height:100%;min-height:0;min-width:0;overflow:auto;flex-shrink:0";
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
        html << "<div class=\"robo-bars\">";
        html << "<div class=\"robo-bar is-y\" hidden>";
        html << "<button type=\"button\" class=\"robo-bar-arrow is-up\" aria-label=\"Scroll up\"></button>";
        html << "<div class=\"robo-bar-track\"><div class=\"robo-bar-thumb\"></div></div>";
        html << "<button type=\"button\" class=\"robo-bar-arrow is-down\" aria-label=\"Scroll down\"></button>";
        html << "</div>";
        html << "<div class=\"robo-bar is-x\" hidden>";
        html << "<button type=\"button\" class=\"robo-bar-arrow is-left\" aria-label=\"Scroll left\"></button>";
        html << "<div class=\"robo-bar-track\"><div class=\"robo-bar-thumb\"></div></div>";
        html << "<button type=\"button\" class=\"robo-bar-arrow is-right\" aria-label=\"Scroll right\"></button>";
        html << "</div></div>";
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
    js += R"JS(
(function () {
  function mount(port, bars) {
  if (!port || !bars) return;
  var yBar = bars.querySelector(".robo-bar.is-y");
  var xBar = bars.querySelector(".robo-bar.is-x");
  var yThumb = yBar.querySelector(".robo-bar-thumb");
  var xThumb = xBar.querySelector(".robo-bar-thumb");
  var yTrack = yBar.querySelector(".robo-bar-track");
  var xTrack = xBar.querySelector(".robo-bar-track");
  var edge = 2;
  var thick = 6;
  var arrow = 6;
  var gap = 3;
  var minThumb = 28;
  var drag = null;
  function wake(bar) {
    if (!bar || bar.hidden) return;
    bar.classList.add("is-on");
    clearTimeout(bar._idle);
    bar._idle = setTimeout(function () {
      if (!bar.classList.contains("is-drag") && !bar.matches(":hover")) bar.classList.remove("is-on");
    }, 900);
  }
  function place() {
    var maxY = Math.max(0, port.scrollHeight - port.clientHeight);
    var maxX = Math.max(0, port.scrollWidth - port.clientWidth);
    var flow = getComputedStyle(port);
    var yOn = maxY > 1 && flow.overflowY !== "hidden" && flow.overflowY !== "clip";
    var xOn = maxX > 1 && flow.overflowX !== "hidden" && flow.overflowX !== "clip";
    if (!port.classList.contains("robo-page")) {
      var box = port.getBoundingClientRect();
      bars.style.left = box.left + "px";
      bars.style.top = box.top + "px";
      bars.style.width = box.width + "px";
      bars.style.height = box.height + "px";
    }
    var yH = Math.max(thick, port.clientHeight - edge * 2 - (xOn ? thick + edge : 0));
    var xW = Math.max(thick, port.clientWidth - edge * 2 - (yOn ? thick + edge : 0));
    yBar.hidden = !yOn;
    xBar.hidden = !xOn;
    yBar.style.height = yH + "px";
    xBar.style.width = xW + "px";
    var trackInset = arrow + gap;
    var trackH = Math.max(minThumb, yH - trackInset * 2);
    var trackW = Math.max(minThumb, xW - trackInset * 2);
    var yStart = trackH === yH - trackInset * 2 ? trackInset : Math.max(0, (yH - trackH) / 2);
    var xStart = trackW === xW - trackInset * 2 ? trackInset : Math.max(0, (xW - trackW) / 2);
    yTrack.style.top = yStart + "px";
    yTrack.style.height = trackH + "px";
    xTrack.style.left = xStart + "px";
    xTrack.style.width = trackW + "px";
    var thumbH = Math.min(trackH, Math.max(minThumb, trackH * port.clientHeight / Math.max(1, port.scrollHeight)));
    var thumbW = Math.min(trackW, Math.max(minThumb, trackW * port.clientWidth / Math.max(1, port.scrollWidth)));
    var travelY = Math.max(1, trackH - thumbH);
    var travelX = Math.max(1, trackW - thumbW);
    yThumb.style.top = (maxY > 0 ? port.scrollTop * travelY / maxY : 0) + "px";
    yThumb.style.height = thumbH + "px";
    xThumb.style.left = (maxX > 0 ? port.scrollLeft * travelX / maxX : 0) + "px";
    xThumb.style.width = thumbW + "px";
    yBar._max = maxY;
    xBar._max = maxX;
    yBar._start = yStart;
    xBar._start = xStart;
    yBar._track = trackH;
    xBar._track = trackW;
    yBar._thumb = thumbH;
    xBar._thumb = thumbW;
  }
  function step(axis, delta) {
    if (axis === "y") port.scrollTop += delta;
    else port.scrollLeft += delta;
    place();
    wake(axis === "y" ? yBar : xBar);
  }
  function hold(button, axis, delta) {
    step(axis, delta);
    var delay = setTimeout(function () {
      button._repeat = setInterval(function () { step(axis, delta); }, 60);
    }, 220);
    function stop() {
      clearTimeout(delay);
      clearInterval(button._repeat);
      button.removeEventListener("pointerup", stop);
      button.removeEventListener("pointerleave", stop);
    }
    button.addEventListener("pointerup", stop);
    button.addEventListener("pointerleave", stop);
  }
  yBar.querySelector(".is-up").addEventListener("pointerdown", function (event) {
    event.preventDefault();
    hold(event.currentTarget, "y", -56);
  });
  yBar.querySelector(".is-down").addEventListener("pointerdown", function (event) {
    event.preventDefault();
    hold(event.currentTarget, "y", 56);
  });
  xBar.querySelector(".is-left").addEventListener("pointerdown", function (event) {
    event.preventDefault();
    hold(event.currentTarget, "x", -56);
  });
  xBar.querySelector(".is-right").addEventListener("pointerdown", function (event) {
    event.preventDefault();
    hold(event.currentTarget, "x", 56);
  });
  function jump(bar, axis, pointer, start, track) {
    var ratio = (pointer - start) / track;
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;
    if (axis === "y") port.scrollTop = bar._max * ratio;
    else port.scrollLeft = bar._max * ratio;
    place();
    wake(bar);
  }
  yTrack.addEventListener("pointerdown", function (event) {
    if (event.target === yThumb) return;
    var box = yBar.getBoundingClientRect();
    jump(yBar, "y", event.clientY - box.top, yBar._start, yBar._track);
  });
  xTrack.addEventListener("pointerdown", function (event) {
    if (event.target === xThumb) return;
    var box = xBar.getBoundingClientRect();
    jump(xBar, "x", event.clientX - box.left, xBar._start, xBar._track);
  });
  function beginDrag(event, bar, axis) {
    event.preventDefault();
    var box = bar.getBoundingClientRect();
    var thumbAt = parseFloat((axis === "y" ? yThumb.style.top : xThumb.style.left)) || 0;
    var along = axis === "y" ? event.clientY - box.top : event.clientX - box.left;
    drag = { bar: bar, axis: axis, grab: along - bar._start - thumbAt };
    bar.classList.add("is-drag");
    wake(bar);
  }
  yThumb.addEventListener("pointerdown", function (event) { beginDrag(event, yBar, "y"); });
  xThumb.addEventListener("pointerdown", function (event) { beginDrag(event, xBar, "x"); });
  window.addEventListener("pointermove", function (event) {
    if (!drag) return;
    var box = drag.bar.getBoundingClientRect();
    var travel = Math.max(1, drag.bar._track - drag.bar._thumb);
    var pos = (drag.axis === "y" ? event.clientY - box.top : event.clientX - box.left) - drag.grab - drag.bar._start;
    if (pos < 0) pos = 0;
    if (pos > travel) pos = travel;
    if (travel > 4 && pos >= travel - 1) pos = travel;
    if (drag.axis === "y") port.scrollTop = drag.bar._max * pos / travel;
    else port.scrollLeft = drag.bar._max * pos / travel;
    place();
    wake(drag.bar);
  });
  window.addEventListener("pointerup", function () {
    if (!drag) return;
    drag.bar.classList.remove("is-drag");
    wake(drag.bar);
    drag = null;
  });
  bars.addEventListener("pointerenter", function (event) {
    var bar = event.target.closest ? event.target.closest(".robo-bar") : null;
    wake(bar);
  });
  var lastX = 0;
  var lastY = 0;
  port.addEventListener("scroll", function () {
    place();
    if (port.scrollTop !== lastY) wake(yBar);
    if (port.scrollLeft !== lastX) wake(xBar);
    lastX = port.scrollLeft;
    lastY = port.scrollTop;
  }, { passive: true });
  window.addEventListener("resize", function () { place(); wake(yBar); wake(xBar); });
  window.addEventListener("scroll", place, true);
  place();
  wake(yBar);
  wake(xBar);
  }
  var page = document.querySelector(".robo-page");
  var pageBars = page ? page.querySelector(":scope > .robo-bars") : null;
  if (page && pageBars) mount(page, pageBars);
  var markup = "<div class=\"robo-bar is-y\" hidden><button type=\"button\" class=\"robo-bar-arrow is-up\" aria-label=\"Scroll up\"></button><div class=\"robo-bar-track\"><div class=\"robo-bar-thumb\"></div></div><button type=\"button\" class=\"robo-bar-arrow is-down\" aria-label=\"Scroll down\"></button></div><div class=\"robo-bar is-x\" hidden><button type=\"button\" class=\"robo-bar-arrow is-left\" aria-label=\"Scroll left\"></button><div class=\"robo-bar-track\"><div class=\"robo-bar-thumb\"></div></div><button type=\"button\" class=\"robo-bar-arrow is-right\" aria-label=\"Scroll right\"></button></div>";
  document.querySelectorAll(".robo-scroll").forEach(function (node) {
    var x = Number(node.getAttribute("data-offset-x") || 0);
    var y = Number(node.getAttribute("data-offset-y") || 0);
    if (x) node.scrollLeft = x;
    if (y) node.scrollTop = y;
    var bars = document.createElement("div");
    bars.className = "robo-bars";
    bars.innerHTML = markup;
    node.appendChild(bars);
    mount(node, bars);
  });
})();
(function () {
  var fields = document.querySelectorAll("input.robo-field,textarea.robo-field");
  if (!fields.length) return;
  var canvas = document.createElement("canvas");
  var ctx = canvas.getContext("2d");
  var mirror = document.createElement("div");
  mirror.style.cssText = "position:fixed;left:-9999px;top:0;visibility:hidden;white-space:pre-wrap;overflow-wrap:break-word;pointer-events:none";
  var mark = document.createElement("span");
  mirror.appendChild(mark);
  document.body.appendChild(mirror);
  var touched = 0;
  function shown(field, end) {
    var text = field.value.slice(0, end);
    if (field.type === "password") return text.replace(/./g, "•");
    return text;
  }
  function measure(field, text) {
    ctx.font = getComputedStyle(field).font;
    return ctx.measureText(text).width;
  }
  function thick(size, before, after) {
    var bar = Math.max(1, Math.floor((size + 12) / 18));
    var neighbor = before > 0 && after > 0 ? Math.min(before, after) : Math.max(before, after);
    if (neighbor > 0) bar = Math.min(bar, Math.max(1, Math.floor(neighbor / 4)));
    return Math.min(bar, 4);
  }
  function cover(now) {
    var idle = now - touched;
    if (idle < 650) return 1;
    var phase = (idle - 650) % 1200;
    if (phase < 220) return 1 - phase / 220;
    if (phase < 480) return 0;
    if (phase < 700) return (phase - 480) / 220;
    return 1;
  }
  fields.forEach(function (field) {
    if (field.disabled) return;
    var bar = document.createElement("div");
    bar.className = "robo-field-caret";
    var paint = field.style.getPropertyValue("--robo-caret");
    if (paint) bar.style.background = paint;
    document.body.appendChild(bar);
    field._roboCaret = bar;
    function place() {
      if (document.activeElement !== field) {
        bar.style.display = "none";
        return;
      }
      var end = field.selectionDirection === "backward" ? field.selectionStart : field.selectionEnd;
      var text = shown(field, end);
      var style = getComputedStyle(field);
      var fontSize = parseFloat(style.fontSize) || 14;
      var before = end > 0 ? measure(field, text.slice(-1)) : 0;
      var next = field.value.slice(end, end + 1);
      var after = next ? measure(field, field.type === "password" ? "•" : next) : 0;
      var width = thick(fontSize, before, after);
      var box = field.getBoundingClientRect();
      var borderLeft = parseFloat(style.borderLeftWidth) || 0;
      var borderRight = parseFloat(style.borderRightWidth) || 0;
      var borderTop = parseFloat(style.borderTopWidth) || 0;
      var borderBottom = parseFloat(style.borderBottomWidth) || 0;
      var padLeft = parseFloat(style.paddingLeft) || 0;
      var padTop = parseFloat(style.paddingTop) || 0;
      var padBottom = parseFloat(style.paddingBottom) || 0;
      var innerH = box.height - borderTop - borderBottom - padTop - padBottom;
      var caretH = Math.max(1, Math.min(innerH, fontSize));
      var x = 0;
      var y = 0;
      if (field.tagName === "TEXTAREA") {
        mirror.style.font = style.font;
        mirror.style.width = field.clientWidth + "px";
        mirror.style.padding = style.padding;
        mirror.style.lineHeight = style.lineHeight;
        mirror.style.letterSpacing = style.letterSpacing;
        mirror.textContent = "";
        mirror.appendChild(document.createTextNode(text));
        mirror.appendChild(mark);
        var line = parseFloat(style.lineHeight);
        if (!line || line < fontSize) line = fontSize * 1.35;
        x = box.left + mark.offsetLeft - field.scrollLeft;
        y = box.top + mark.offsetTop - field.scrollTop + Math.max(0, (line - caretH) / 2);
      } else {
        x = box.left + borderLeft + padLeft + measure(field, text) - field.scrollLeft;
        y = box.top + borderTop + padTop + (innerH - caretH) / 2;
      }
      var left = box.left + borderLeft;
      var right = box.right - borderRight;
      var top = box.top + borderTop;
      var bottom = box.bottom - borderBottom;
      if (x < left || x > right || y + caretH < top || y > bottom) {
        bar.style.display = "none";
        return;
      }
      bar.style.display = "block";
      bar.style.width = width + "px";
      bar.style.height = caretH + "px";
      bar.style.left = x + "px";
      bar.style.top = y + "px";
      bar.style.opacity = String(cover(performance.now()));
    }
    var wake = function () { touched = performance.now(); place(); };
    field.addEventListener("focus", wake);
    field.addEventListener("input", wake);
    field.addEventListener("keyup", place);
    field.addEventListener("click", place);
    field.addEventListener("scroll", place);
    field.addEventListener("blur", function () { bar.style.display = "none"; });
    field._roboPlace = place;
  });
  document.addEventListener("selectionchange", function () {
    var field = document.activeElement;
    if (field && field._roboPlace) field._roboPlace();
  });
  window.addEventListener("scroll", function () {
    var field = document.activeElement;
    if (field && field._roboPlace) field._roboPlace();
  }, true);
  window.addEventListener("resize", function () {
    var field = document.activeElement;
    if (field && field._roboPlace) field._roboPlace();
  });
  function tick() {
    var field = document.activeElement;
    if (field && field._roboPlace) field._roboPlace();
    requestAnimationFrame(tick);
  }
  requestAnimationFrame(tick);
})();
)JS";
    js += RoboWebReelScript();
    js += RoboWebImageScript();
    js += R"JS(
document.querySelectorAll("audio.robo-audio").forEach(function (node) {
  var apply = function () {
    var volume = Number(node.getAttribute("data-volume"));
    if (volume >= 0 && volume <= 100) node.volume = volume / 100;
    var time = Number(node.getAttribute("data-time"));
    if (time > 0 && !node._roboTime) {
      node.currentTime = time / 1000;
      node._roboTime = true;
    }
  };
  node.addEventListener("loadedmetadata", apply);
  apply();
});
)JS";

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
