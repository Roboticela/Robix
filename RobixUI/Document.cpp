#include "Document.hpp"

#include "Compiler/Compose.hpp"
#include "Compiler/RoboXML.hpp"
#include "Platform/Windows/WindowsHost.hpp"
#include "Web/WebFoundation.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace Robix::UI {
namespace {

std::string Option(const std::vector<std::string>& args, const std::string& name, const std::string& fallback) {
    for (std::size_t index = 0; index + 1 < args.size(); ++index) {
        if (args[index] == name) {
            return args[index + 1];
        }
    }
    return fallback;
}

bool Has(const std::vector<std::string>& args, const std::string& name) {
    for (const std::string& arg : args) {
        if (arg == name) {
            return true;
        }
    }
    return false;
}

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

std::string ReadText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
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

std::string Attr(const RoboXMLNode& node, const char* key, const char* fallback) {
    const std::string value = RoboXMLAttr(node, key);
    return value.empty() ? fallback : value;
}

int ToInt(const std::string& text, int fallback) {
    if (text.empty()) {
        return fallback;
    }
    try {
        return std::stoi(text);
    } catch (...) {
        return fallback;
    }
}

bool ToBool(const std::string& text, bool fallback) {
    const std::string lower = Lower(text);
    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
        return false;
    }
    return fallback;
}

void ApplyScreen(const std::string& document, WindowRunOptions& options) {
    const std::string source = ReadText(document);
    RoboXMLDocument xml;
    if (source.empty() || !RoboXMLParse(source, xml)) {
        return;
    }
    RoboUICompose(xml.root, document);
    LowerKeys(xml.root);
    const RoboXMLNode* screen = FindScreen(xml.root);
    if (screen == nullptr) {
        return;
    }
    options.screen = true;
    options.width = ToInt(Attr(*screen, "width", "960"), 960);
    options.height = ToInt(Attr(*screen, "height", "640"), 640);
    options.x = ToInt(Attr(*screen, "x", "0"), 0);
    options.y = ToInt(Attr(*screen, "y", "0"), 0);
    options.place = Attr(*screen, "place", "center");
    options.movable = ToBool(Attr(*screen, "movable", "true"), true);
    options.resizable = ToBool(Attr(*screen, "resizable", "true"), true);
    const std::string theme = Lower(Attr(*screen, "theme", Attr(*screen, "appearance", "dark").c_str()));
    options.light = theme == "light";
    const std::string title = Attr(*screen, "name", Attr(*screen, "title", "Robix").c_str());
    if (!title.empty()) {
        options.title = title;
    }
}

}  // namespace

void LoadScreenOptions(const std::string& document, WindowRunOptions& options) {
    ApplyScreen(document, options);
}

int RunUiCommand(const std::vector<std::string>& args) {
    if (args.empty() || args[0] != "ui" || args.size() < 2) {
        return -1;
    }
    const std::string command = args[1];
    std::string document;
    for (std::size_t index = 2; index < args.size(); ++index) {
        if (args[index].size() > 0 && args[index][0] == '-') {
            if (index + 1 < args.size() && (args[index] == "-o" || args[index] == "--code" || args[index] == "--out")) {
                ++index;
            }
            continue;
        }
        document = args[index];
        break;
    }

    if (command == "render") {
        const std::string output = Option(args, "-o", "framebuffer.bmp");
        std::string error;
        if (!WriteFoundationBmp(output, error)) {
            std::cerr << "robix ui: " << error << "\n";
            return 1;
        }
        std::cout << "bmp " << output << "\n";
        return 0;
    }
    if (command == "window") {
        WindowRunOptions options;
        options.stayOpen = Has(args, "--dev");
        if (!document.empty()) {
            ApplyScreen(document, options);
            options.document = document;
        }
        return RunWindowsFoundation(options);
    }
    if (command == "web") {
        const std::string output = Option(args, "-o", "Build/web");
        std::string error;
        if (!WriteWebFoundation(output, document, error, {}, true)) {
            std::cerr << "robix ui: " << error << "\n";
            return 1;
        }
        return 0;
    }
    if (command == "validate") {
        if (document.empty()) {
            std::cerr << "robix ui: expected a .rbxu file\n";
            return 1;
        }
        std::ifstream in(document, std::ios::binary);
        if (!in) {
            std::cerr << "robix ui: could not open '" << document << "'\n";
            return 1;
        }
        std::cout << "foundation validated " << document << "\n";
        return 0;
    }
    return -1;
}

}  // namespace Robix::UI
