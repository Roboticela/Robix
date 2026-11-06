#include "Document.hpp"

#include "../Compiler/AST/ASTNodes.hpp"
#include "../Compiler/Lexer/Lexer.hpp"
#include "../Compiler/Parser/Parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace Robix::UI {

namespace {

struct Node {
    std::string tag;
    std::map<std::string, std::string> attr;
    std::vector<Node> children;
    int line = 1;
};

struct Paint {
    std::string kind;
    std::string name;
    std::string text;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    std::uint32_t color = 0;
    bool fill = false;
};

struct Style {
    std::uint32_t window = 0xF4F5F7;
    std::uint32_t text = 0x1A1A1A;
    std::uint32_t button = 0x1D4E89;
    std::uint32_t buttonText = 0xFFFFFF;
};

struct Size {
    int w = 0;
    int h = 0;
};

int ScaleFor(int fontSize) {
    return std::max(1, fontSize / 8);
}

int FontSizeOf(const Node& node, int fallback) {
    const auto it = node.attr.find("FontSize");
    if (it == node.attr.end()) return fallback;
    try {
        return std::max(8, std::stoi(it->second));
    } catch (...) {
        return fallback;
    }
}

std::string ShownText(const Node& node) {
    if (node.attr.count("Text")) return node.attr.at("Text");
    if (node.attr.count("Value")) return node.attr.at("Value");
    if (node.attr.count("Title")) return node.attr.at("Title");
    return {};
}

int IntAttr(const Node& node, const char* key, int fallback) {
    const auto it = node.attr.find(key);
    if (it == node.attr.end()) return fallback;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return fallback;
    }
}

std::uint32_t ParseColor(const std::string& text, std::uint32_t fallback) {
    if (text.size() == 7 && text[0] == '#') {
        try {
            return static_cast<std::uint32_t>(std::stoul(text.substr(1), nullptr, 16));
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

// 3x5 glyphs. Bit 2 is the left pixel. Unknown characters use a solid block.
std::uint8_t GlyphRow(char ch, int row) {
    const char upper = (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - 32) : ch;
    static const std::uint8_t kDigits[10][5] = {
        {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
        {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}
    };
    static const std::uint8_t kLetters[26][5] = {
        {7, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {7, 4, 4, 4, 7}, {6, 5, 5, 5, 6}, {7, 4, 7, 4, 7},
        {7, 4, 7, 4, 4}, {7, 4, 5, 5, 7}, {5, 5, 7, 5, 5}, {7, 2, 2, 2, 7}, {1, 1, 1, 5, 7},
        {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7}, {7, 7, 5, 5, 5}, {7, 5, 5, 5, 5}, {7, 5, 5, 5, 7},
        {7, 5, 7, 4, 4}, {7, 5, 5, 7, 1}, {7, 5, 6, 5, 5}, {7, 4, 7, 1, 7}, {7, 2, 2, 2, 2},
        {5, 5, 5, 5, 7}, {5, 5, 5, 5, 2}, {5, 5, 5, 7, 7}, {5, 5, 2, 5, 5}, {5, 5, 7, 2, 2},
        {7, 1, 2, 4, 7}
    };
    if (row < 0 || row > 4) return 0;
    if (upper >= 'A' && upper <= 'Z') return kLetters[upper - 'A'][row];
    if (upper >= '0' && upper <= '9') return kDigits[upper - '0'][row];
    if (upper == ' ') return 0;
    if (upper == '-') return row == 2 ? 7 : 0;
    return 7;
}

Size MeasureText(const std::string& text, int fontSize) {
    const int scale = ScaleFor(fontSize);
    const int count = std::max<int>(1, static_cast<int>(text.size()));
    return {count * (3 * scale + scale), 5 * scale};
}

Size Measure(const Node& node, int maxW) {
    const std::string& tag = node.tag;
    if (tag == "Text" || tag == "Button") {
        const int font = FontSizeOf(node, 16);
        Size text = MeasureText(ShownText(node), font);
        if (tag == "Button") {
            text.w += 8 * ScaleFor(font);
            text.h += 8 * ScaleFor(font);
        }
        if (node.attr.count("Width")) text.w = IntAttr(node, "Width", text.w);
        if (node.attr.count("Height")) text.h = IntAttr(node, "Height", text.h);
        return text;
    }
    const int spacing = IntAttr(node, "Spacing", 0);
    const int pad = IntAttr(node, "Padding", 0);
    int inner = std::max(0, maxW - pad * 2);
    if (tag == "Row") {
        int width = 0;
        int height = 0;
        int lineW = 0;
        int lineH = 0;
        bool any = false;
        for (const auto& child : node.children) {
            Size childSize = Measure(child, inner);
            if (any && lineW + spacing + childSize.w > inner) {
                width = std::max(width, lineW);
                height += lineH + spacing;
                lineW = 0;
                lineH = 0;
            }
            if (lineW > 0) lineW += spacing;
            lineW += childSize.w;
            lineH = std::max(lineH, childSize.h);
            any = true;
        }
        width = std::max(width, lineW);
        height += lineH;
        return {std::min(maxW, width + pad * 2), height + pad * 2};
    }
    int height = pad;
    int width = 0;
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        Size childSize = Measure(node.children[i], inner);
        if (i > 0) height += spacing;
        height += childSize.h;
        width = std::max(width, childSize.w);
    }
    height += pad;
    if (tag == "Window") {
        return {IntAttr(node, "Width", 320), IntAttr(node, "Height", 180)};
    }
    return {std::min(maxW, width + pad * 2), height};
}

void Fill(std::vector<std::uint32_t>& pixels, int frameW, int frameH, int x, int y, int w, int h, std::uint32_t color) {
    for (int row = y; row < y + h; ++row) {
        if (row < 0 || row >= frameH) continue;
        for (int col = x; col < x + w; ++col) {
            if (col < 0 || col >= frameW) continue;
            pixels[static_cast<std::size_t>(row * frameW + col)] = color;
        }
    }
}

void DrawText(std::vector<std::uint32_t>& pixels, int frameW, int frameH, int x, int y, const std::string& text, int fontSize, std::uint32_t color) {
    const int scale = ScaleFor(fontSize);
    int cursor = x;
    for (unsigned char ch : text) {
        for (int row = 0; row < 5; ++row) {
            const std::uint8_t bits = GlyphRow(static_cast<char>(ch), row);
            for (int col = 0; col < 3; ++col) {
                if ((bits & (4 >> col)) == 0) continue;
                Fill(pixels, frameW, frameH, cursor + col * scale, y + row * scale, scale, scale, color);
            }
        }
        cursor += 3 * scale + scale;
    }
}

void Place(const Node& node, std::vector<std::uint32_t>& pixels, std::vector<Paint>& paints, int frameW, int frameH,
           int x, int y, int maxW, const Style& style, std::uint32_t foreground) {
    const int pad = IntAttr(node, "Padding", 0);
    const int spacing = IntAttr(node, "Spacing", 0);
    std::uint32_t color = foreground;
    if (node.attr.count("Color")) color = ParseColor(node.attr.at("Color"), color);
    if (node.tag == "Window") {
        const int width = IntAttr(node, "Width", frameW);
        const int height = IntAttr(node, "Height", frameH);
        Fill(pixels, frameW, frameH, 0, 0, width, height, style.window);
        paints.push_back({"Window", node.attr.count("Name") ? node.attr.at("Name") : "Window", ShownText(node), 0, 0, width, height, style.window, true});
        int cursor = y + pad;
        for (const auto& child : node.children) {
            Place(child, pixels, paints, frameW, frameH, x + pad, cursor, std::max(0, maxW - pad * 2), style, color);
            cursor += Measure(child, std::max(0, maxW - pad * 2)).h + spacing;
        }
        return;
    }
    if (node.tag == "Column" || node.tag == "Stack" || node.tag == "Panel" || node.tag == "View" || node.tag == "Page" || node.tag == "Container") {
        if (node.attr.count("Background")) {
            Size box = Measure(node, maxW);
            const std::uint32_t bg = ParseColor(node.attr.at("Background"), style.window);
            Fill(pixels, frameW, frameH, x, y, box.w, box.h, bg);
            paints.push_back({node.tag, node.attr.count("Name") ? node.attr.at("Name") : node.tag, {}, x, y, box.w, box.h, bg, true});
        }
        int cursor = y + pad;
        for (const auto& child : node.children) {
            Place(child, pixels, paints, frameW, frameH, x + pad, cursor, std::max(0, maxW - pad * 2), style, color);
            cursor += Measure(child, std::max(0, maxW - pad * 2)).h + spacing;
        }
        return;
    }
    if (node.tag == "Row" || node.tag == "Grid") {
        int cursorX = x + pad;
        int cursorY = y + pad;
        int lineH = 0;
        const int right = x + maxW - pad;
        for (const auto& child : node.children) {
            Size childSize = Measure(child, std::max(0, maxW - pad * 2));
            if (cursorX > x + pad && cursorX + childSize.w > right) {
                cursorX = x + pad;
                cursorY += lineH + spacing;
                lineH = 0;
            }
            Place(child, pixels, paints, frameW, frameH, cursorX, cursorY, childSize.w, style, color);
            cursorX += childSize.w + spacing;
            lineH = std::max(lineH, childSize.h);
        }
        return;
    }
    if (node.tag == "Text" || node.tag == "Button") {
        Size box = Measure(node, maxW);
        const bool button = node.tag == "Button";
        const std::uint32_t bg = node.attr.count("Background")
            ? ParseColor(node.attr.at("Background"), button ? style.button : style.window)
            : (button ? style.button : style.window);
        const std::uint32_t fg = button ? style.buttonText : color;
        if (button || node.attr.count("Background")) {
            Fill(pixels, frameW, frameH, x, y, box.w, box.h, bg);
        }
        const int scale = ScaleFor(FontSizeOf(node, 16));
        const int inset = button ? 4 * scale : 0;
        DrawText(pixels, frameW, frameH, x + inset, y + inset, ShownText(node), FontSizeOf(node, 16), fg);
        paints.push_back({node.tag, node.attr.count("Name") ? node.attr.at("Name") : node.tag, ShownText(node), x, y, box.w, box.h, button ? bg : fg, button});
        return;
    }
    Size box = Measure(node, maxW);
    paints.push_back({node.tag, node.attr.count("Name") ? node.attr.at("Name") : node.tag, ShownText(node), x, y, std::max(box.w, 1), std::max(box.h, 1), color, false});
    int cursor = y;
    for (const auto& child : node.children) {
        Place(child, pixels, paints, frameW, frameH, x, cursor, maxW, style, color);
        cursor += Measure(child, maxW).h;
    }
}

struct ParseResult {
    Node root;
    std::string error;
};

ParseResult ParseDocument(const std::string& text) {
    ParseResult result;
    std::size_t i = 0;
    int line = 1;
    auto bump = [&](char ch) {
        if (ch == '\n') ++line;
    };
    auto skip = [&]() {
        while (i < text.size()) {
            if (text.compare(i, 4, "<!--") == 0) {
                const auto end = text.find("-->", i + 4);
                if (end == std::string::npos) return;
                while (i < end + 3 && i < text.size()) bump(text[i++]);
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(text[i]))) {
                bump(text[i++]);
                continue;
            }
            break;
        }
    };
    std::function<bool(Node&)> parseElement = [&](Node& node) {
        skip();
        if (i >= text.size() || text[i] != '<') {
            result.error = "RobixUI: expected an element at line " + std::to_string(line);
            return false;
        }
        if (text.compare(i, 2, "</") == 0) {
            result.error = "RobixUI: unexpected closing tag at line " + std::to_string(line);
            return false;
        }
        ++i;
        if (text.compare(i, 4, "?xml") == 0) {
            const auto end = text.find("?>", i);
            if (end == std::string::npos) {
                result.error = "RobixUI: unclosed xml prolog";
                return false;
            }
            while (i < end + 2) bump(text[i++]);
            return parseElement(node);
        }
        node.line = line;
        while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])))) node.tag.push_back(text[i++]);
        if (node.tag.empty()) {
            result.error = "RobixUI: missing element name at line " + std::to_string(line);
            return false;
        }
        while (true) {
            while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) bump(text[i++]);
            if (i >= text.size()) {
                result.error = "RobixUI: unclosed <" + node.tag + ">";
                return false;
            }
            if (text[i] == '>' || text.compare(i, 2, "/>") == 0) break;
            std::string name;
            while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == ':' || text[i] == '_')) name.push_back(text[i++]);
            while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) bump(text[i++]);
            if (i >= text.size() || text[i] != '=') {
                result.error = "RobixUI: attribute " + name + " needs a value at line " + std::to_string(line);
                return false;
            }
            ++i;
            while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) bump(text[i++]);
            if (i >= text.size() || text[i] != '"') {
                result.error = "RobixUI: attribute " + name + " needs quotes at line " + std::to_string(line);
                return false;
            }
            ++i;
            std::string value;
            while (i < text.size() && text[i] != '"') {
                if (text[i] == '&') {
                    if (text.compare(i, 5, "&amp;") == 0) { value.push_back('&'); i += 5; continue; }
                    if (text.compare(i, 4, "&lt;") == 0) { value.push_back('<'); i += 4; continue; }
                    if (text.compare(i, 4, "&gt;") == 0) { value.push_back('>'); i += 4; continue; }
                    if (text.compare(i, 6, "&quot;") == 0) { value.push_back('"'); i += 6; continue; }
                }
                bump(text[i]);
                value.push_back(text[i++]);
            }
            if (i >= text.size() || text[i] != '"') {
                result.error = "RobixUI: unclosed attribute " + name;
                return false;
            }
            ++i;
            node.attr[name] = value;
        }
        if (text.compare(i, 2, "/>") == 0) {
            i += 2;
            return true;
        }
        ++i;
        while (true) {
            skip();
            if (i >= text.size()) {
                result.error = "RobixUI: missing </" + node.tag + ">";
                return false;
            }
            if (text.compare(i, 2, "</") == 0) {
                i += 2;
                std::string close;
                while (i < text.size() && std::isalnum(static_cast<unsigned char>(text[i]))) close.push_back(text[i++]);
                while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) bump(text[i++]);
                if (i >= text.size() || text[i] != '>' || close != node.tag) {
                    result.error = "RobixUI: expected </" + node.tag + "> at line " + std::to_string(line);
                    return false;
                }
                ++i;
                return true;
            }
            Node child;
            if (!parseElement(child)) return false;
            node.children.push_back(std::move(child));
        }
    };
    if (!parseElement(result.root)) return result;
    if (result.root.tag != "RobixUI") {
        result.error = "RobixUI: the root element is " + result.root.tag;
        result.root = {};
    }
    return result;
}

const char* kEvents[] = {"OnClick", "OnDoubleClick", "OnMouseEnter", "OnMouseLeave", "OnKeyDown", "OnKeyUp",
                         "OnFocus", "OnBlur", "OnValueChanged", "OnLoaded", "OnClosed"};

void CollectEvents(const Node& node, std::vector<std::pair<std::string, std::string>>& events) {
    for (const char* name : kEvents) {
        const auto it = node.attr.find(name);
        if (it != node.attr.end()) events.emplace_back(name, it->second);
    }
    for (const auto& child : node.children) CollectEvents(child, events);
}

struct Signature {
    std::string owner;
    std::string name;
    std::string returns = "Void";
    std::size_t parameters = 0;
    bool isPublic = false;
};

void CollectFunctions(const Frontend::DeclNode& decl, const std::string& owner, std::vector<Signature>& out) {
    if (decl.kind == Frontend::DeclKind::Function) {
        Signature signature;
        signature.owner = owner;
        signature.name = decl.name;
        signature.returns = decl.returnType.baseName.empty() ? "Void" : decl.returnType.baseName;
        signature.parameters = decl.parameters.size();
        signature.isPublic = decl.visibility == Spec::AccessVisibility::Public;
        out.push_back(signature);
    }
    const std::string next = (decl.kind == Frontend::DeclKind::Class || decl.kind == Frontend::DeclKind::Struct) ? decl.name : owner;
    for (const auto& member : decl.members) {
        if (member) CollectFunctions(*member, next, out);
    }
}

std::string SignatureText(const Signature& signature) {
    std::ostringstream out;
    out << (signature.owner.empty() ? signature.name : signature.owner + "." + signature.name) << "(";
    for (std::size_t i = 0; i < signature.parameters; ++i) {
        if (i > 0) out << ", ";
        out << "value";
    }
    out << ") -> " << signature.returns;
    return out.str();
}

bool BindHandlers(const Node& root, const std::string& codePath, const std::string& code, std::string& error) {
    std::vector<std::pair<std::string, std::string>> events;
    CollectEvents(root, events);
    if (events.empty()) return true;
    if (codePath.empty()) {
        error = "RobixUI: an event binding needs --code naming the Robix source.\n";
        return false;
    }
    Frontend::DiagnosticBag diagnostics;
    Frontend::Lexer lexer(code, codePath, diagnostics);
    const auto tokens = lexer.TokenizeAll();
    Frontend::Parser parser(tokens, codePath, diagnostics);
    const auto unit = parser.ParseCompilationUnit();
    if (diagnostics.HasErrors()) {
        error = diagnostics.FormatHumanReadable();
        return false;
    }
    std::vector<Signature> functions;
    for (const auto& decl : unit.declarations) {
        if (decl) CollectFunctions(*decl, {}, functions);
    }
    std::ostringstream messages;
    bool ok = true;
    for (const auto& event : events) {
        const Signature* match = nullptr;
        const Signature* any = nullptr;
        for (const auto& signature : functions) {
            if (signature.name != event.second) continue;
            any = &signature;
            if (signature.isPublic) {
                match = &signature;
                break;
            }
        }
        if (match == nullptr) {
            ok = false;
            if (any != nullptr) {
                messages << "U2003: " << event.first << " " << event.second << " is not public\n";
            } else {
                messages << "U2001: " << event.first << " " << event.second << " does not name a Robix function\n";
            }
            continue;
        }
        if (match->parameters != 0 || match->returns != "Void") {
            ok = false;
            messages << "U2002: " << event.first << " " << event.second
                     << " expected () -> Void, received " << SignatureText(*match) << "\n";
            continue;
        }
        std::cout << "bind " << event.first << " " << event.second << " -> " << SignatureText(*match) << "\n";
    }
    error = messages.str();
    return ok;
}

Style ThemeFor(const Node& root, const std::string& overrideTheme) {
    std::string theme = overrideTheme;
    if (theme.empty()) {
        for (const auto& child : root.children) {
            if (child.tag == "Window" && child.attr.count("Theme")) theme = child.attr.at("Theme");
        }
    }
    for (char& ch : theme) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    Style style;
    if (theme == "dark") {
        style.window = 0x14161A;
        style.text = 0xE6E6E6;
        style.button = 0x4C8DFF;
        style.buttonText = 0xFFFFFF;
    }
    return style;
}

bool WriteBmp(const std::string& path, int width, int height, const std::vector<std::uint32_t>& pixels) {
    const int stride = (width * 3 + 3) & ~3;
    const std::uint32_t dataSize = static_cast<std::uint32_t>(stride * height);
    const std::uint32_t fileSize = 54 + dataSize;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    unsigned char header[54] = {};
    header[0] = 'B';
    header[1] = 'M';
    header[2] = static_cast<unsigned char>(fileSize);
    header[3] = static_cast<unsigned char>(fileSize >> 8);
    header[4] = static_cast<unsigned char>(fileSize >> 16);
    header[5] = static_cast<unsigned char>(fileSize >> 24);
    header[10] = 54;
    header[14] = 40;
    header[18] = static_cast<unsigned char>(width);
    header[19] = static_cast<unsigned char>(width >> 8);
    header[20] = static_cast<unsigned char>(width >> 16);
    header[21] = static_cast<unsigned char>(width >> 24);
    header[22] = static_cast<unsigned char>(height);
    header[23] = static_cast<unsigned char>(height >> 8);
    header[24] = static_cast<unsigned char>(height >> 16);
    header[25] = static_cast<unsigned char>(height >> 24);
    header[26] = 1;
    header[28] = 24;
    out.write(reinterpret_cast<const char*>(header), 54);
    std::vector<unsigned char> row(static_cast<std::size_t>(stride));
    for (int y = height - 1; y >= 0; --y) {
        std::fill(row.begin(), row.end(), 0);
        for (int x = 0; x < width; ++x) {
            const std::uint32_t color = pixels[static_cast<std::size_t>(y * width + x)];
            row[static_cast<std::size_t>(x * 3)] = static_cast<unsigned char>(color);
            row[static_cast<std::size_t>(x * 3 + 1)] = static_cast<unsigned char>(color >> 8);
            row[static_cast<std::size_t>(x * 3 + 2)] = static_cast<unsigned char>(color >> 16);
        }
        out.write(reinterpret_cast<const char*>(row.data()), stride);
    }
    return static_cast<bool>(out);
}

std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

#ifdef _WIN32
struct WindowFrame {
    const std::uint32_t* pixels = nullptr;
    int width = 0;
    int height = 0;
    bool painted = false;
};

WindowFrame* g_windowFrame = nullptr;

LRESULT CALLBACK RobixWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_PAINT && g_windowFrame != nullptr && g_windowFrame->pixels != nullptr) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = g_windowFrame->width;
        info.bmiHeader.biHeight = -g_windowFrame->height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        StretchDIBits(
            dc, 0, 0, g_windowFrame->width, g_windowFrame->height,
            0, 0, g_windowFrame->width, g_windowFrame->height,
            g_windowFrame->pixels, &info, DIB_RGB_COLORS, SRCCOPY);
        EndPaint(window, &paint);
        g_windowFrame->painted = true;
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

bool ShowSoftwareWindow(const std::wstring& title, int width, int height, const std::vector<std::uint32_t>& pixels) {
    WindowFrame frame;
    frame.pixels = pixels.data();
    frame.width = width;
    frame.height = height;
    g_windowFrame = &frame;

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = RobixWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"RobixUIFrame";
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    RegisterClassW(&windowClass);

    HWND window = CreateWindowExW(
        0, L"RobixUIFrame", title.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width + 16, height + 40,
        nullptr, nullptr, windowClass.hInstance, nullptr);
    if (window == nullptr) {
        g_windowFrame = nullptr;
        return false;
    }
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message;
    const DWORD started = GetTickCount();
    while (!frame.painted && GetTickCount() - started < 2000) {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) break;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (IsWindow(window)) DestroyWindow(window);
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) break;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    g_windowFrame = nullptr;
    return frame.painted;
}
#endif

std::string Arg(const std::vector<std::string>& args, const std::string& flag) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) return args[i + 1];
    }
    return {};
}

} // namespace

int RunUiCommand(const std::vector<std::string>& args) {
    if (args.empty() || args[0] != "ui") return -1;
    if (args.size() < 3) {
        std::cerr << "robix ui: use validate <file.rbxu> --code <file.rbx> or render <file.rbxu> -o <file.bmp>\n";
        return 1;
    }
    const std::string action = args[1];
    const std::string file = args[2];
    const std::string text = ReadFile(file);
    if (text.empty()) {
        std::cerr << "robix ui: could not read " << file << "\n";
        return 1;
    }
    ParseResult parsed = ParseDocument(text);
    if (!parsed.error.empty()) {
        std::cerr << parsed.error << "\n";
        return 1;
    }
    const std::string codePath = Arg(args, "--code");
    std::string bindError;
    if (!codePath.empty() || action == "validate") {
        const std::string code = codePath.empty() ? std::string{} : ReadFile(codePath);
        if (!codePath.empty() && code.empty()) {
            std::cerr << "robix ui: could not read " << codePath << "\n";
            return 1;
        }
        if (!BindHandlers(parsed.root, codePath, code, bindError)) {
            std::cerr << bindError;
            return 1;
        }
    }
    Node window;
    for (const auto& child : parsed.root.children) {
        if (child.tag == "Window") window = child;
    }
    if (window.tag.empty()) {
        std::cerr << "RobixUI: the document has no Window\n";
        return 1;
    }
    const std::string widthArg = Arg(args, "--width");
    const std::string heightArg = Arg(args, "--height");
    if (!widthArg.empty()) window.attr["Width"] = widthArg;
    if (!heightArg.empty()) window.attr["Height"] = heightArg;
    const int width = IntAttr(window, "Width", 320);
    const int height = IntAttr(window, "Height", 180);
    const Style style = ThemeFor(parsed.root, Arg(args, "--theme"));
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width * height), style.window);
    std::vector<Paint> paints;
    Place(window, pixels, paints, width, height, 0, 0, width, style, style.text);
    for (const auto& paint : paints) {
        std::cout << "layout " << paint.kind << " " << paint.name << " " << paint.x << " " << paint.y
                  << " " << paint.w << " " << paint.h << "\n";
    }
    if (action == "validate") return 0;
    if (action == "window") {
#ifdef _WIN32
        const std::string title = window.attr.count("Title") ? window.attr.at("Title") : window.attr.count("Name") ? window.attr.at("Name") : "RobixUI";
        std::wstring wide(title.begin(), title.end());
        if (!ShowSoftwareWindow(wide, width, height, pixels)) {
            std::cerr << "robix ui window: the window was not painted\n";
            return 1;
        }
        std::cout << "window: shown " << width << "x" << height << " title " << title << "\n";
        std::cout << "window: closed after paint\n";
        return 0;
#else
        std::cerr << "robix ui window: no window system is linked on this target\n";
        return 1;
#endif
    }
    if (action != "render") {
        std::cerr << "robix ui: unknown action " << action << "\n";
        return 1;
    }
    const std::string output = Arg(args, "-o");
    if (output.empty()) {
        std::cerr << "robix ui render: -o is required\n";
        return 1;
    }
    const auto slash = output.find_last_of("/\\");
    if (slash != std::string::npos) {
        std::error_code ec;
        std::filesystem::create_directories(output.substr(0, slash), ec);
    }
    if (!WriteBmp(output, width, height, pixels)) {
        std::cerr << "robix ui render: could not write " << output << "\n";
        return 1;
    }
    auto report = [&](const std::string& label, int x, int y) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        const std::uint32_t color = pixels[static_cast<std::size_t>(y * width + x)];
        std::ostringstream hex;
        hex << std::hex << std::uppercase;
        hex.width(6);
        hex.fill('0');
        hex << color;
        std::cout << "pixel " << label << " " << hex.str() << "\n";
    };
    report("origin", 0, 0);
    for (const auto& paint : paints) {
        if (paint.kind == "Button") report(paint.name, paint.x + 2, paint.y + 2);
        if (paint.kind == "Text" && !paint.text.empty()) report(paint.name, paint.x, paint.y);
    }
    std::cout << "renderer software-bmp " << output << "\n";
    return 0;
}

} // namespace Robix::UI
