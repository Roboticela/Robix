#include "Scene.hpp"

#include "Components/Board/Board.Web.hpp"
#include "Components/Screen/Screen.Web.hpp"
#include "Foundation/FoundationPlatform.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Robix::UI {
namespace {

std::string RoboCompilerLowerCopy(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
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
    if (text.empty()) {
        return fallback;
    }
    const std::string lower = RoboCompilerLowerCopy(text);
    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
        return false;
    }
    return fallback;
}

std::string Attr(const RoboXMLNode& node, const char* key, const char* alias, const char* fallback) {
    const std::string first = RoboXMLAttr(node, key);
    if (!first.empty()) {
        return first;
    }
    if (alias != nullptr) {
        const std::string second = RoboXMLAttr(node, alias);
        if (!second.empty()) {
            return second;
        }
    }
    return fallback == nullptr ? std::string{} : std::string{fallback};
}

BoardNode ParseBoard(const RoboXMLNode& node) {
    BoardNode board;
    board.id = Attr(node, "id", nullptr, "board");
    board.display = RoboCompilerLowerCopy(Attr(node, "display", nullptr, "canvas"));
    board.direction = RoboCompilerLowerCopy(Attr(node, "direction", "flexDirection", "column"));
    board.justify = RoboCompilerLowerCopy(Attr(node, "justify", "justifyContent", "start"));
    board.align = RoboCompilerLowerCopy(Attr(node, "align", "alignItems", "start"));
    board.wrap = RoboCompilerLowerCopy(Attr(node, "wrap", "flexWrap", "nowrap"));
    board.background = Attr(node, "background", "backgroundColor", "");
    if (board.background.empty()) {
        board.background = RoboXMLAttr(node, "bg");
    }
    board.fill = Attr(node, "fill", "fillParent", "");
    board.x = ToInt(RoboXMLAttr(node, "x"), 0);
    board.y = ToInt(RoboXMLAttr(node, "y"), 0);
    const std::string width = RoboXMLAttr(node, "width");
    const std::string height = RoboXMLAttr(node, "height");
    board.widthSet = !width.empty();
    board.heightSet = !height.empty();
    board.width = ToInt(width, 0);
    board.height = ToInt(height, 0);
    board.gap = ToInt(Attr(node, "gap", "gutter", "0"), 0);
    board.padding = ToInt(RoboXMLAttr(node, "padding"), 0);
    board.columns = std::max(1, ToInt(RoboXMLAttr(node, "columns"), 1));
    for (const RoboXMLNode& child : node.children) {
        if (RoboCompilerLowerCopy(child.name) == "board") {
            board.children.push_back(ParseBoard(child));
        }
    }
    return board;
}

void ReadScreen(const RoboXMLNode& node, ScreenNode& screen) {
    screen.id = Attr(node, "id", nullptr, "screen");
    screen.name = Attr(node, "name", "title", "");
    screen.path = Attr(node, "path", "route", "/");
    screen.theme = RoboCompilerLowerCopy(Attr(node, "theme", "appearance", "dark"));
    if (screen.theme != "light") {
        screen.theme = "dark";
    }
    screen.place = RoboCompilerLowerCopy(Attr(node, "place", nullptr, "center"));
    screen.state = RoboCompilerLowerCopy(Attr(node, "state", nullptr, "normal"));
    screen.background = Attr(node, "background", "backgroundColor", "");
    screen.board = RoboXMLAttr(node, "board");
    const std::string radius = RoboXMLAttr(node, "radius");
    if (!radius.empty()) {
        screen.radius = radius;
    }
    screen.x = ToInt(RoboXMLAttr(node, "x"), 0);
    screen.y = ToInt(RoboXMLAttr(node, "y"), 0);
    screen.width = std::max(1, ToInt(RoboXMLAttr(node, "width"), 640));
    screen.height = std::max(1, ToInt(RoboXMLAttr(node, "height"), 400));
    screen.minWidth = ToInt(RoboXMLAttr(node, "minWidth"), 320);
    screen.minHeight = ToInt(RoboXMLAttr(node, "minHeight"), 200);
    screen.maxWidth = ToInt(RoboXMLAttr(node, "maxWidth"), 1920);
    screen.maxHeight = ToInt(RoboXMLAttr(node, "maxHeight"), 1080);
    screen.movable = ToBool(RoboXMLAttr(node, "movable"), true);
    const std::string resizable = RoboXMLAttr(node, "resizable");
    screen.resizable = resizable.empty() ? true : ToBool(resizable, true);
    screen.transparent = ToBool(Attr(node, "transparent", "transparency", "false"), false);
    screen.topmost = ToBool(Attr(node, "topmost", "alwaysOnTop", "false"), false);
    screen.fullscreen = ToBool(RoboXMLAttr(node, "fullscreen"), false);
}

Robo::Color ColorFromPacked(std::uint32_t packed) {
    Robo::Color color;
    color.alpha = static_cast<std::uint8_t>((packed >> 24) & 0xFFu);
    color.red = static_cast<std::uint8_t>((packed >> 16) & 0xFFu);
    color.green = static_cast<std::uint8_t>((packed >> 8) & 0xFFu);
    color.blue = static_cast<std::uint8_t>(packed & 0xFFu);
    if (color.alpha == 0) {
        color.alpha = 255;
    }
    return color;
}

Robo::Color ResolveColor(const std::string& text, bool light, const Robo::Color& fallback) {
    if (text.empty()) {
        return fallback;
    }
    if (text[0] == '#') {
        return Robo::Color::FromHex(text);
    }
    const std::uint32_t packed = RoboFoundationPlatformColorForTheme(text.c_str(), light ? ROBO_TRUE : ROBO_FALSE);
    if (packed == 0xFFFFFFFFu) {
        return fallback;
    }
    return ColorFromPacked(packed);
}

std::string HexByte(int value) {
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    out.push_back(digits[(value >> 4) & 0xF]);
    out.push_back(digits[value & 0xF]);
    return out;
}

std::string HexColor(const Robo::Color& color) {
    return HexByte(color.red) + HexByte(color.green) + HexByte(color.blue);
}

void LayoutBoard(BoardNode& board, int x, int y, int width, int height) {
    board.laidX = x;
    board.laidY = y;
    board.laidW = std::max(0, width);
    board.laidH = std::max(0, height);
    const int innerX = board.laidX + board.padding;
    const int innerY = board.laidY + board.padding;
    const int innerW = std::max(0, board.laidW - board.padding * 2);
    const int innerH = std::max(0, board.laidH - board.padding * 2);
    if (board.children.empty()) {
        return;
    }
    const bool row = board.display == "flex" && (board.direction == "row" || board.direction == "row-reverse");
    const bool column = board.display == "flex" && !row;
    const bool grid = board.display == "grid";
    if (column) {
        int cursor = innerY;
        for (BoardNode& child : board.children) {
            const int childW = child.widthSet ? child.width : innerW;
            int childH = child.heightSet ? child.height : 0;
            if (!child.heightSet) {
                LayoutBoard(child, innerX, cursor, childW, 0);
                childH = child.laidH;
                if (childH <= 0 && !child.children.empty()) {
                    childH = board.gap;
                }
            }
            LayoutBoard(child, innerX, cursor, childW, childH);
            cursor += child.laidH + board.gap;
        }
        return;
    }
    if (row) {
        int cursor = innerX;
        const int childH = innerH;
        for (BoardNode& child : board.children) {
            const int childW = child.widthSet ? child.width : 0;
            LayoutBoard(child, cursor, innerY, childW, child.heightSet ? child.height : childH);
            cursor += child.laidW + board.gap;
        }
        return;
    }
    if (grid) {
        const int columns = std::max(1, board.columns);
        const int cellW = columns > 0 ? std::max(0, (innerW - board.gap * (columns - 1)) / columns) : innerW;
        int index = 0;
        for (BoardNode& child : board.children) {
            const int column = index % columns;
            const int rowIndex = index / columns;
            const int cellH = child.heightSet ? child.height : 32;
            LayoutBoard(child, innerX + column * (cellW + board.gap), innerY + rowIndex * (cellH + board.gap),
                        child.widthSet ? child.width : cellW, cellH);
            ++index;
        }
        return;
    }
    for (BoardNode& child : board.children) {
        LayoutBoard(child, innerX + child.x, innerY + child.y, child.widthSet ? child.width : innerW,
                    child.heightSet ? child.height : innerH);
    }
}

void MeasureColumn(BoardNode& board) {
    if (!(board.display == "flex" && board.direction != "row" && board.direction != "row-reverse")) {
        return;
    }
    int content = board.padding * 2;
    int index = 0;
    for (BoardNode& child : board.children) {
        MeasureColumn(child);
        content += std::max(child.laidH, child.height);
        if (index + 1 < static_cast<int>(board.children.size())) {
            content += board.gap;
        }
        ++index;
    }
    if (!board.heightSet) {
        board.laidH = content;
        board.height = content;
    }
}

void PaintBoard(const BoardNode& board, Robo::FrameBuffer& buffer, const Robo::Color& fallback, bool light) {
    if (!board.background.empty() && board.laidW > 0 && board.laidH > 0) {
        buffer.FillRect(board.laidX, board.laidY, board.laidW, board.laidH, ResolveColor(board.background, light, fallback), 1.0f);
    }
    for (const BoardNode& child : board.children) {
        PaintBoard(child, buffer, fallback, light);
    }
}

void AppendLayout(const BoardNode& board, std::vector<std::string>& lines) {
    std::ostringstream line;
    line << "layout Board " << board.id << " " << board.laidX << " " << board.laidY << " " << board.laidW << " "
         << board.laidH;
    lines.push_back(line.str());
    for (const BoardNode& child : board.children) {
        AppendLayout(child, lines);
    }
}

std::string TokenCss(const char* name, bool light) {
    const std::uint32_t packed = RoboFoundationPlatformColorForTheme(name, light ? ROBO_TRUE : ROBO_FALSE);
    const Robo::Color color = ColorFromPacked(packed);
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", color.red, color.green, color.blue);
    return buffer;
}

void AppendBoardHtml(std::ostringstream& html, const BoardNode& board) {
    html << "<div class=\"" << RoboWebBoardClass() << "\" data-display=\"" << board.display << "\"";
    if (!board.id.empty()) {
        html << " id=\"" << board.id << "\"";
    }
    html << " style=\"box-sizing:border-box;display:" << (board.display == "grid" ? "grid" : "flex")
         << ";flex-direction:" << (board.direction == "row" || board.direction == "row-reverse" ? board.direction : "column")
         << ";gap:" << board.gap << "px;padding:" << board.padding << "px";
    if (board.widthSet) {
        html << ";width:" << board.width << "px";
    }
    if (board.heightSet) {
        html << ";height:" << board.height << "px";
    }
    if (board.fill == "true" || board.fill == "1") {
        html << ";width:100%;height:100%";
    }
    if (!board.background.empty()) {
        html << ";background:" << board.background;
    }
    html << "\">";
    for (const BoardNode& child : board.children) {
        AppendBoardHtml(html, child);
    }
    html << "</div>";
}

const BoardNode* FindBoard(const std::vector<BoardNode>& boards, const std::string& id) {
    for (const BoardNode& board : boards) {
        if (board.id == id) {
            return &board;
        }
    }
    return nullptr;
}

}  // namespace

bool LoadScene(const std::string& source, Scene& scene, std::string& error) {
    RoboXMLDocument document;
    if (!RoboXMLParse(source, document)) {
        error = document.errors.empty() ? "could not parse the UI document" : document.errors.front();
        return false;
    }
    std::vector<BoardNode> loose;
    const RoboXMLNode* screenNode = nullptr;
    if (RoboCompilerLowerCopy(document.root.name) == "screen") {
        screenNode = &document.root;
    } else {
        for (const RoboXMLNode& child : document.root.children) {
            const std::string name = RoboCompilerLowerCopy(child.name);
            if (name == "screen" && screenNode == nullptr) {
                screenNode = &child;
            } else if (name == "board") {
                loose.push_back(ParseBoard(child));
            }
        }
    }
    if (screenNode == nullptr) {
        error = "the document has no Screen";
        return false;
    }
    ReadScreen(*screenNode, scene.screen);
    for (const RoboXMLNode& child : screenNode->children) {
        if (RoboCompilerLowerCopy(child.name) == "board") {
            scene.screen.content = ParseBoard(child);
            scene.screen.hasContent = true;
            break;
        }
    }
    if (!scene.screen.hasContent) {
        const BoardNode* match = FindBoard(loose, scene.screen.board);
        if (match == nullptr && !loose.empty()) {
            match = &loose.front();
        }
        if (match != nullptr) {
            scene.screen.content = *match;
            scene.screen.hasContent = true;
        }
    }
    const bool light = scene.screen.theme == "light";
    const Robo::Color primary = ColorFromPacked(RoboFoundationPlatformColorForTheme("bg-primary", light ? ROBO_TRUE : ROBO_FALSE));
    const Robo::Color dark = ColorFromPacked(RoboFoundationPlatformColorForTheme("bg-primary", ROBO_FALSE));
    std::ostringstream foundation;
    foundation << "foundation bg-primary light " << HexColor(ColorFromPacked(RoboFoundationPlatformColorForTheme("bg-primary", ROBO_TRUE)))
               << " dark " << HexColor(dark);
    scene.foundationLine = foundation.str();
    (void)primary;
    LayoutScene(scene);
    return true;
}

void LayoutScene(Scene& scene) {
    scene.layoutLines.clear();
    std::ostringstream screenLine;
    screenLine << "layout Screen " << scene.screen.id << " 0 0 " << scene.screen.width << " " << scene.screen.height;
    scene.layoutLines.push_back(screenLine.str());
    if (!scene.screen.hasContent) {
        return;
    }
    BoardNode& board = scene.screen.content;
    const bool fill = board.fill == "true" || board.fill == "1" || (!board.widthSet && !board.heightSet);
    const int width = board.widthSet && !fill ? board.width : scene.screen.width;
    const int height = board.heightSet && !fill ? board.height : scene.screen.height;
    LayoutBoard(board, 0, 0, width, height);
    MeasureColumn(board);
    if (board.display == "flex" && board.direction != "row" && board.direction != "row-reverse" && !board.heightSet) {
        LayoutBoard(board, 0, 0, width, std::max(board.laidH, height));
    } else {
        LayoutBoard(board, 0, 0, width, height);
    }
    AppendLayout(board, scene.layoutLines);
}

void PaintScene(Scene& scene, Robo::FrameBuffer& buffer) {
    const bool light = scene.screen.theme == "light";
    const Robo::Color background = scene.screen.transparent
                                       ? Robo::Color::FromRgba(0, 0, 0, 0)
                                       : ResolveColor(scene.screen.background, light,
                                                      ColorFromPacked(RoboFoundationPlatformColorForTheme(
                                                          "bg-primary", light ? ROBO_TRUE : ROBO_FALSE)));
    buffer.Resize(scene.screen.width, scene.screen.height);
    buffer.Clear(background);
    const int radius = RoboFoundationPlatformRadius(scene.screen.radius.c_str());
    if (radius > 0 && !scene.screen.transparent) {
        buffer.FillRoundedRect(0.0f, 0.0f, static_cast<float>(scene.screen.width), static_cast<float>(scene.screen.height),
                               static_cast<float>(radius), background, 1.0f);
    }
    if (scene.screen.hasContent) {
        PaintBoard(scene.screen.content, buffer, background, light);
    }
}

std::string SceneHtml(const Scene& scene) {
    const bool light = scene.screen.theme == "light";
    std::ostringstream css;
    RoboWebAppendScreenCss(css);
    std::ostringstream html;
    html << "<!DOCTYPE html><html data-theme=\"" << scene.screen.theme << "\"><head><meta charset=\"utf-8\"/>";
    html << "<title>" << (scene.screen.name.empty() ? "Robix" : scene.screen.name) << "</title><style>";
    html << ":root{--bg-primary:" << TokenCss("bg-primary", light) << ";--bg-secondary:" << TokenCss("bg-secondary", light)
         << ";--text-primary:" << TokenCss("text-primary", light) << ";--brand-primary:" << TokenCss("brand-primary", light)
         << ";--space-4:" << RoboFoundationPlatformSpace("space-4") << "px;--body-size:"
         << "17px;}";
    html << "html,body{margin:0;background:var(--bg-primary);color:var(--text-primary);}";
    html << css.str();
    html << "</style></head><body>";
    html << "<div class=\"" << RoboWebScreenClass(true) << "\" data-component=\"Screen\" data-place=\"" << scene.screen.place
         << "\" data-theme=\"" << scene.screen.theme << "\" data-movable=\"" << (scene.screen.movable ? "true" : "false")
         << "\" data-resizable=\"" << (scene.screen.resizable ? "true" : "false") << "\" style=\"width:" << scene.screen.width
         << "px;height:" << scene.screen.height << "px\">";
    if (scene.screen.hasContent) {
        AppendBoardHtml(html, scene.screen.content);
    }
    html << "</div></body></html>";
    return html.str();
}

bool WriteBmp(const std::string& path, const Robo::FrameBuffer& buffer, std::string& error) {
    const int width = buffer.GetWidth();
    const int height = buffer.GetHeight();
    if (width <= 0 || height <= 0 || buffer.GetPixels() == nullptr) {
        error = "the framebuffer is empty";
        return false;
    }
    const int rowStride = width * 4;
    const std::uint32_t pixelBytes = static_cast<std::uint32_t>(rowStride * height);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        error = "could not write " + path;
        return false;
    }
    const std::uint32_t fileSize = 54u + pixelBytes;
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
    header[22] = static_cast<unsigned char>(height);
    header[23] = static_cast<unsigned char>(height >> 8);
    header[26] = 1;
    header[28] = 32;
    out.write(reinterpret_cast<const char*>(header), 54);
    const auto* pixels = buffer.GetPixels();
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4u;
            const unsigned char bgra[4] = {pixels[index + 2], pixels[index + 1], pixels[index], pixels[index + 3]};
            out.write(reinterpret_cast<const char*>(bgra), 4);
        }
    }
    return true;
}

std::string AssetsFontsRoot() {
    namespace fs = std::filesystem;
    fs::path cursor = fs::current_path();
    for (int depth = 0; depth < 6; ++depth) {
        const fs::path candidate = cursor / "Assets" / "Fonts";
        if (fs::is_directory(candidate)) {
            return candidate.string();
        }
        if (!cursor.has_parent_path() || cursor == cursor.parent_path()) {
            break;
        }
        cursor = cursor.parent_path();
    }
    return "Assets/Fonts";
}

}  // namespace Robix::UI
