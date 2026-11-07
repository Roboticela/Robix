#include "WebFoundation.hpp"

#include "../Compiler/Components/Screen/Screen.hpp"
#include "../Compiler/Components/Screen/ScreenWeb.hpp"
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

namespace fs = std::filesystem;

namespace Robix::UI {
namespace {

fs::path FindUp(const fs::path& relative) {
    fs::path dir = fs::current_path();
    for (int depth = 0; depth < 8; ++depth) {
        const fs::path candidate = dir / relative;
        if (fs::exists(candidate)) {
            return candidate;
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) {
            break;
        }
        dir = dir.parent_path();
    }
    return {};
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

const RoboXMLNode* FindScreen(const RoboXMLNode& node) {
    if (Lower(node.name) == "screen") {
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
    if (!document.empty()) {
        RoboXMLDocument xml;
        const std::string source = ReadText(document);
        if (!source.empty() && RoboXMLParse(source, xml)) {
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
    css << "body{color:var(--text-primary);font-family:var(--font-sans,system-ui,sans-serif)}\n";
    css << ".robo-screen{position:relative;min-height:100%;box-sizing:border-box;background:var(--bg-primary);color:var(--text-primary)}\n";
    if (hasScreen) {
        RoboWebAppendScreenCss(css);
    }
    css << typeRules.str();
    css << "@media (max-width:734px){\n" << mobile.str() << "}\n";

    std::ostringstream html;
    html << "<!DOCTYPE html><html lang=\"en\" data-theme=\"" << Escape(theme) << "\"><head><meta charset=\"utf-8\">";
    html << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html << "<title>" << Escape(title) << "</title>";
    html << "<link rel=\"stylesheet\" href=\"app.css\"></head><body>";
    if (hasScreen) {
        const std::string id = RoboXMLAttr(screenCopy, "id", "robo-app");
        const std::string place = RoboXMLAttr(screenCopy, "place", "center");
        const std::string movable = RoboXMLAttr(screenCopy, "movable", "true");
        const std::string width = RoboXMLAttr(screenCopy, "width", "640");
        const std::string height = RoboXMLAttr(screenCopy, "height", "400");
        html << "<" << RoboWebScreenTag(true) << " id=\"" << Escape(id) << "\" class=\"" << RoboWebScreenClass(true)
             << " robo-page\" data-place=\"" << Escape(place) << "\" data-theme=\"" << Escape(theme)
             << "\" data-movable=\"" << Escape(movable) << "\" style=\"width:" << Escape(width) << "px;height:"
             << Escape(height) << "px\">";
        html << "</" << RoboWebScreenTag(true) << ">";
        std::cout << "component Screen\n";
        std::cout << RoboCompilerEmitScreenCpp(screenCopy);
    } else {
        html << "<div id=\"robo-app\" class=\"robo-screen\"><p class=\"robo-type-body\">" << Escape(title) << "</p></div>";
    }
    html << "<script src=\"app.js\"></script></body></html>\n";

    const char* js = R"JS(window.Robo = window.Robo || {};
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
)JS";

    if (!WriteText(outRoot / "index.html", html.str()) || !WriteText(outRoot / "app.css", css.str()) ||
        !WriteText(outRoot / "app.js", js)) {
        error = "failed to write the web package";
        return false;
    }
    std::cout << "foundation bg-primary light " << Hex(RoboFoundationPlatformColorForTheme("bg-primary", ROBO_TRUE))
              << " dark " << Hex(RoboFoundationPlatformColorForTheme("bg-primary", ROBO_FALSE)) << "\n";
    std::cout << "web " << (outRoot / "index.html").string() << "\n";
    return true;
}

}  // namespace Robix::UI
