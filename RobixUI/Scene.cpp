#include "Scene.hpp"

#include "Components/Board/Board.Web.hpp"
#include "Components/Body/Body.Web.hpp"
#include "Components/Caption/Caption.Web.hpp"
#include "Components/Eyebrow/Eyebrow.Web.hpp"
#include "Components/Heading/Heading.Web.hpp"
#include "Components/Label/Label.Web.hpp"
#include "Components/Paragraph/Paragraph.Web.hpp"
#include "Components/Screen/Screen.Web.hpp"
#include "Components/Scrollbar/Scrollbar.Platform.hpp"
#include "Components/Scrollbar/Scrollbar.Web.hpp"
#include "Components/Subhead/Subhead.Web.hpp"
#include "Components/Text/Text.Platform.hpp"
#include "Components/Text/Text.Web.hpp"
#include "Foundation/FoundationPlatform.hpp"
#include "RoboCompilerWebShared.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
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

constexpr int kScrollbarSlot = 1000000;

bool IsScrollbarSlot(int slot) { return slot >= kScrollbarSlot; }

bool IsTextTag(const std::string& name) {
    return name == "text" || name == "content" || name == "heading" || name == "subhead" || name == "body" ||
           name == "caption" || name == "eyebrow" || name == "label" || name == "paragraph" || name == "p" ||
           name == "h1" || name == "h2" || name == "h3" || name == "h4" || name == "h5" || name == "h6";
}

int HeadingLevel(const std::string& name, const RoboXMLNode& node) {
    if (name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6') {
        return name[1] - '0';
    }
    const std::string level = RoboCompilerLowerCopy(RoboXMLAttr(node, "level"));
    if (level == "display") {
        return 0;
    }
    if (level.size() == 2 && level[0] == 'h' && level[1] >= '1' && level[1] <= '6') {
        return level[1] - '0';
    }
    if (!level.empty() && level[0] >= '1' && level[0] <= '6') {
        return level[0] - '0';
    }
    return 1;
}

std::string TextContent(const RoboXMLNode& node) {
    const std::string content = RoboXMLAttr(node, "content");
    if (!content.empty()) {
        return content;
    }
    const std::string text = RoboXMLAttr(node, "text");
    if (!text.empty()) {
        return text;
    }
    std::string joined = node.text;
    for (const RoboXMLNode& child : node.children) {
        const std::string name = RoboCompilerLowerCopy(child.name);
        if (name == "span") {
            const std::string span = RoboXMLAttr(child, "content");
            joined += span.empty() ? child.text : span;
        }
    }
    return joined;
}

TextNode ParseText(const RoboXMLNode& node) {
    TextNode text;
    text.source = node;
    text.tag = RoboCompilerLowerCopy(node.name);
    text.id = Attr(node, "id", nullptr, text.tag.c_str());
    text.content = TextContent(node);
    text.color = Attr(node, "color", "textColor", "");
    text.tone = RoboXMLAttr(node, "tone");
    text.align = RoboCompilerLowerCopy(Attr(node, "align", "textAlign", "start"));
    text.wrap = RoboCompilerLowerCopy(Attr(node, "wrap", "textWrap", "word"));
    text.font = Attr(node, "font", "fontFamily", "");
    text.size = Attr(node, "size", "fontSize", "");
    text.level = HeadingLevel(text.tag, node);
    text.x = ToInt(RoboXMLAttr(node, "x"), 0);
    text.y = ToInt(RoboXMLAttr(node, "y"), 0);
    const std::string width = RoboXMLAttr(node, "width");
    const std::string height = RoboXMLAttr(node, "height");
    text.widthSet = !width.empty();
    text.heightSet = !height.empty();
    text.width = ToInt(width, 0);
    text.height = ToInt(height, 0);
    text.maxWidth = ToInt(RoboXMLAttr(node, "maxWidth"), 0);
    return text;
}

std::uint32_t PackArgb(const Robo::Color& color) {
    return (static_cast<std::uint32_t>(color.alpha) << 24) | (static_cast<std::uint32_t>(color.red) << 16) |
           (static_cast<std::uint32_t>(color.green) << 8) | static_cast<std::uint32_t>(color.blue);
}

const char* RoleColorToken(RoboUITextRole role) {
    switch (role) {
        case RoboUITextRole::Body:
        case RoboUITextRole::BodySmall:
        case RoboUITextRole::Rich:
        case RoboUITextRole::Subhead:
            return "text-secondary";
        case RoboUITextRole::Caption:
        case RoboUITextRole::Eyebrow:
            return "text-tertiary";
        default:
            return "text-primary";
    }
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
        const std::string name = RoboCompilerLowerCopy(child.name);
        if (name == "board") {
            board.order.push_back(static_cast<int>(board.children.size()));
            board.children.push_back(ParseBoard(child));
        } else if (IsTextTag(name)) {
            board.order.push_back(-static_cast<int>(board.texts.size()) - 1);
            board.texts.push_back(ParseText(child));
        } else if (name == "scrollbar") {
            ScrollbarNode bar;
            bar.id = Attr(child, "id", nullptr, "scrollbar");
            bar.orientation = RoboCompilerLowerCopy(Attr(child, "orientation", nullptr, "vertical"));
            bar.arrows = RoboCompilerLowerCopy(Attr(child, "arrows", nullptr, "hover"));
            bar.thumbColor = RoboXMLAttr(child, "thumbColor");
            bar.trackColor = RoboXMLAttr(child, "trackColor");
            const std::string position = RoboXMLAttr(child, "position");
            if (!position.empty()) {
                bar.position = RoboCompilerLowerCopy(position);
            }
            bar.thickness = std::max(2, ToInt(RoboXMLAttr(child, "thickness"), 6));
            const std::string autoHide = RoboXMLAttr(child, "autoHide");
            bar.autoHide = autoHide.empty() ? true : ToBool(autoHide, true);
            const std::string overlay = RoboXMLAttr(child, "overlay");
            bar.overlay = overlay.empty() ? true : ToBool(overlay, true);
            bar.x = ToInt(RoboXMLAttr(child, "x"), 0);
            bar.y = ToInt(RoboXMLAttr(child, "y"), 0);
            const std::string width = RoboXMLAttr(child, "width");
            const std::string height = RoboXMLAttr(child, "height");
            bar.widthSet = !width.empty();
            bar.heightSet = !height.empty();
            bar.width = ToInt(width, 0);
            bar.height = ToInt(height, 0);
            board.order.push_back(kScrollbarSlot + static_cast<int>(board.bars.size()));
            board.bars.push_back(std::move(bar));
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

void FillText(const TextNode& node, int maxWidth, bool light, RoboUIText& text) {
    RoboUITextInit(&text, node.id.c_str());
    const std::string& tag = node.tag;
    if (tag == "heading" || tag == "h1" || tag == "h2" || tag == "h3" || tag == "h4" || tag == "h5" || tag == "h6") {
        RoboUITextApplyHeadingDefaults(&text, static_cast<RoboU32>(node.level));
    } else if (tag == "subhead") {
        RoboUITextApplyRole(&text, RoboUITextRole::Subhead);
    } else if (tag == "caption") {
        RoboUITextApplyRole(&text, RoboUITextRole::Caption);
    } else if (tag == "eyebrow") {
        RoboUITextApplyRole(&text, RoboUITextRole::Eyebrow);
    } else if (tag == "label") {
        RoboUITextApplyRole(&text, RoboUITextRole::Label);
    } else if (tag == "body" || tag == "paragraph" || tag == "p" || tag == "content") {
        const std::string size = RoboCompilerLowerCopy(node.size);
        RoboUITextApplyRole(&text, size == "sm" || size == "small" ? RoboUITextRole::BodySmall : RoboUITextRole::Body);
    } else {
        RoboUITextApplyRole(&text, RoboUITextRole::Rich);
    }
    if (!node.size.empty() && node.size[0] >= '0' && node.size[0] <= '9') {
        RoboUITextSetSize(&text, static_cast<RoboU16>(ToInt(node.size, text.sizePx)));
    }
    const char* fontName = node.font.empty() ? RoboUITextRoleFont(text.role) : node.font.c_str();
    RoboUITextSetFont(&text, RoboFontForName(fontName));
    if (!node.color.empty()) {
        RoboUITextSetColor(&text, PackArgb(ResolveColor(node.color, light, ColorFromPacked(text.colorARGB))));
    } else if (node.tone.empty()) {
        RoboUITextSetColor(&text, RoboFoundationPlatformColorForTheme(RoleColorToken(text.role), light ? ROBO_TRUE : ROBO_FALSE));
    }
    RoboUITextSetContent(&text, node.content.c_str());
    if (!node.tone.empty() && text.base.vtable != nullptr && text.base.vtable->setProperty != nullptr) {
        text.base.vtable->setProperty(&text.base, "tone", node.tone.c_str());
    }
    if (!node.align.empty() && text.base.vtable != nullptr && text.base.vtable->setProperty != nullptr) {
        text.base.vtable->setProperty(&text.base, "align", node.align.c_str());
    }
    if (!node.wrap.empty() && text.base.vtable != nullptr && text.base.vtable->setProperty != nullptr) {
        text.base.vtable->setProperty(&text.base, "wrap", node.wrap.c_str());
    }
    const int limit = node.maxWidth > 0 ? node.maxWidth : maxWidth;
    RoboUITextSetMaxWidth(&text, limit);
    text.base.width = limit;
}

int MeasureTextHeight(const TextNode& node, int width, bool light) {
    if (node.heightSet) {
        return node.height;
    }
    auto text = std::make_unique<RoboUIText>();
    FillText(node, width, light, *text);
    RoboI32 measuredW = 0;
    RoboI32 measuredH = 0;
    RoboUITextMeasure(text.get(), &measuredW, &measuredH);
    return measuredH > 0 ? measuredH : static_cast<int>(text->sizePx);
}

void PlaceText(TextNode& text, int x, int y, int width, int height, bool light) {
    text.laidX = x;
    text.laidY = y;
    text.laidW = std::max(0, width);
    text.laidH = text.heightSet ? text.height : MeasureTextHeight(text, text.laidW, light);
    if (height > 0 && text.heightSet) {
        text.laidH = height;
    }
}

void PlaceScrollbar(ScrollbarNode& bar, int x, int y, int availW, int availH) {
    const bool vertical = bar.orientation != "horizontal";
    bar.laidX = x;
    bar.laidY = y;
    bar.laidW = bar.widthSet ? bar.width : (vertical ? 16 : std::max(16, availW));
    bar.laidH = bar.heightSet ? bar.height : (vertical ? std::max(28, availH) : 16);
}

void LayoutBoard(BoardNode& board, int x, int y, int width, int height, bool light) {
    board.laidX = x;
    board.laidY = y;
    board.laidW = std::max(0, width);
    board.laidH = std::max(0, height);
    const int innerX = board.laidX + board.padding;
    const int innerY = board.laidY + board.padding;
    const int innerW = std::max(0, board.laidW - board.padding * 2);
    const int innerH = std::max(0, board.laidH - board.padding * 2);
    if (board.order.empty()) {
        return;
    }
    const bool row = board.display == "flex" && (board.direction == "row" || board.direction == "row-reverse");
    const bool column = board.display == "flex" && !row;
    const bool grid = board.display == "grid";
    if (column) {
        int cursor = innerY;
        for (int slot : board.order) {
            if (IsScrollbarSlot(slot)) {
                ScrollbarNode& bar = board.bars[static_cast<std::size_t>(slot - kScrollbarSlot)];
                PlaceScrollbar(bar, innerX, cursor, innerW, innerH);
                cursor += bar.laidH + board.gap;
            } else if (slot >= 0) {
                BoardNode& child = board.children[static_cast<std::size_t>(slot)];
                const int childW = child.widthSet ? child.width : innerW;
                int childH = child.heightSet ? child.height : 0;
                if (!child.heightSet) {
                    LayoutBoard(child, innerX, cursor, childW, 0, light);
                    childH = child.laidH;
                    if (childH <= 0 && !child.order.empty()) {
                        childH = board.gap;
                    }
                }
                LayoutBoard(child, innerX, cursor, childW, childH, light);
                cursor += child.laidH + board.gap;
            } else {
                TextNode& text = board.texts[static_cast<std::size_t>(-slot - 1)];
                const int childW = text.widthSet ? text.width : innerW;
                const int childH = text.heightSet ? text.height : MeasureTextHeight(text, childW, light);
                PlaceText(text, innerX, cursor, childW, childH, light);
                cursor += text.laidH + board.gap;
            }
        }
        return;
    }
    if (row) {
        int cursor = innerX;
        const int childH = innerH;
        for (int slot : board.order) {
            if (IsScrollbarSlot(slot)) {
                ScrollbarNode& bar = board.bars[static_cast<std::size_t>(slot - kScrollbarSlot)];
                PlaceScrollbar(bar, cursor, innerY, innerW, childH);
                cursor += bar.laidW + board.gap;
            } else if (slot >= 0) {
                BoardNode& child = board.children[static_cast<std::size_t>(slot)];
                const int childW = child.widthSet ? child.width : 0;
                LayoutBoard(child, cursor, innerY, childW, child.heightSet ? child.height : childH, light);
                cursor += child.laidW + board.gap;
            } else {
                TextNode& text = board.texts[static_cast<std::size_t>(-slot - 1)];
                const int childW = text.widthSet ? text.width : innerW;
                PlaceText(text, cursor, innerY, childW, text.heightSet ? text.height : childH, light);
                cursor += text.laidW + board.gap;
            }
        }
        return;
    }
    if (grid) {
        const int columns = std::max(1, board.columns);
        const int cellW = columns > 0 ? std::max(0, (innerW - board.gap * (columns - 1)) / columns) : innerW;
        int index = 0;
        for (int slot : board.order) {
            const int columnIndex = index % columns;
            const int rowIndex = index / columns;
            if (IsScrollbarSlot(slot)) {
                ScrollbarNode& bar = board.bars[static_cast<std::size_t>(slot - kScrollbarSlot)];
                const int cellH = bar.heightSet ? bar.height : 32;
                PlaceScrollbar(bar, innerX + columnIndex * (cellW + board.gap), innerY + rowIndex * (cellH + board.gap), cellW,
                               cellH);
            } else if (slot >= 0) {
                BoardNode& child = board.children[static_cast<std::size_t>(slot)];
                const int cellH = child.heightSet ? child.height : 32;
                LayoutBoard(child, innerX + columnIndex * (cellW + board.gap), innerY + rowIndex * (cellH + board.gap),
                            child.widthSet ? child.width : cellW, cellH, light);
            } else {
                TextNode& text = board.texts[static_cast<std::size_t>(-slot - 1)];
                const int cellH = text.heightSet ? text.height : MeasureTextHeight(text, cellW, light);
                PlaceText(text, innerX + columnIndex * (cellW + board.gap), innerY + rowIndex * (cellH + board.gap),
                          text.widthSet ? text.width : cellW, cellH, light);
            }
            ++index;
        }
        return;
    }
    for (int slot : board.order) {
        if (IsScrollbarSlot(slot)) {
            ScrollbarNode& bar = board.bars[static_cast<std::size_t>(slot - kScrollbarSlot)];
            PlaceScrollbar(bar, innerX + bar.x, innerY + bar.y, innerW, innerH);
        } else if (slot >= 0) {
            BoardNode& child = board.children[static_cast<std::size_t>(slot)];
            LayoutBoard(child, innerX + child.x, innerY + child.y, child.widthSet ? child.width : innerW,
                        child.heightSet ? child.height : innerH, light);
        } else {
            TextNode& text = board.texts[static_cast<std::size_t>(-slot - 1)];
            const int childW = text.widthSet ? text.width : innerW;
            const int childH = text.heightSet ? text.height : MeasureTextHeight(text, childW, light);
            PlaceText(text, innerX + text.x, innerY + text.y, childW, childH, light);
        }
    }
}

void MeasureColumn(BoardNode& board) {
    if (!(board.display == "flex" && board.direction != "row" && board.direction != "row-reverse")) {
        return;
    }
    int content = board.padding * 2;
    int index = 0;
    for (int slot : board.order) {
        int itemHeight = 0;
        if (IsScrollbarSlot(slot)) {
            ScrollbarNode& bar = board.bars[static_cast<std::size_t>(slot - kScrollbarSlot)];
            itemHeight = std::max(bar.laidH, bar.height);
        } else if (slot >= 0) {
            BoardNode& child = board.children[static_cast<std::size_t>(slot)];
            MeasureColumn(child);
            itemHeight = std::max(child.laidH, child.height);
        } else {
            TextNode& text = board.texts[static_cast<std::size_t>(-slot - 1)];
            itemHeight = std::max(text.laidH, text.height);
        }
        content += itemHeight;
        if (index + 1 < static_cast<int>(board.order.size())) {
            content += board.gap;
        }
        ++index;
    }
    if (!board.heightSet) {
        board.laidH = content;
        board.height = content;
    }
}

void PaintText(const TextNode& node, Robo::FrameBuffer& buffer, bool light) {
    auto text = std::make_unique<RoboUIText>();
    FillText(node, node.laidW, light, *text);
    if (text->base.vtable == nullptr || text->base.vtable->paint == nullptr || buffer.GetPixels() == nullptr) {
        return;
    }
    RoboGPU gpu;
    gpu.pixels = buffer.GetPixels();
    gpu.width = buffer.GetWidth();
    gpu.height = buffer.GetHeight();
    gpu.stride = gpu.width * 4;
    text->base.vtable->paint(&text->base, &gpu, node.laidX, node.laidY);
}

void PaintBoard(const BoardNode& board, Robo::FrameBuffer& buffer, const Robo::Color& fallback, bool light) {
    if (!board.background.empty() && board.laidW > 0 && board.laidH > 0) {
        buffer.FillRect(board.laidX, board.laidY, board.laidW, board.laidH, ResolveColor(board.background, light, fallback), 1.0f);
    }
    for (int slot : board.order) {
        if (IsScrollbarSlot(slot)) {
            const ScrollbarNode& bar = board.bars[static_cast<std::size_t>(slot - kScrollbarSlot)];
            ScrollbarPaint paint;
            paint.x = bar.laidX;
            paint.y = bar.laidY;
            paint.width = bar.laidW;
            paint.height = bar.laidH;
            paint.thickness = bar.thickness;
            paint.vertical = bar.orientation != "horizontal";
            paint.thumbColor = bar.thumbColor;
            paint.trackColor = bar.trackColor;
            RoboScrollbarPlatformPaint(paint, buffer, light);
        } else if (slot >= 0) {
            PaintBoard(board.children[static_cast<std::size_t>(slot)], buffer, fallback, light);
        } else {
            PaintText(board.texts[static_cast<std::size_t>(-slot - 1)], buffer, light);
        }
    }
}

void AppendLayout(const BoardNode& board, std::vector<std::string>& lines) {
    std::ostringstream line;
    line << "layout Board " << board.id << " " << board.laidX << " " << board.laidY << " " << board.laidW << " "
         << board.laidH;
    lines.push_back(line.str());
    for (int slot : board.order) {
        if (IsScrollbarSlot(slot)) {
            const ScrollbarNode& bar = board.bars[static_cast<std::size_t>(slot - kScrollbarSlot)];
            std::ostringstream barLine;
            barLine << "layout scrollbar " << bar.id << " " << bar.laidX << " " << bar.laidY << " " << bar.laidW << " "
                    << bar.laidH;
            lines.push_back(barLine.str());
        } else if (slot >= 0) {
            AppendLayout(board.children[static_cast<std::size_t>(slot)], lines);
        } else {
            const TextNode& text = board.texts[static_cast<std::size_t>(-slot - 1)];
            std::ostringstream textLine;
            textLine << "layout " << text.tag << " " << text.id << " " << text.laidX << " " << text.laidY << " " << text.laidW
                     << " " << text.laidH;
            lines.push_back(textLine.str());
        }
    }
}

std::string TokenCss(const char* name, bool light) {
    const std::uint32_t packed = RoboFoundationPlatformColorForTheme(name, light ? ROBO_TRUE : ROBO_FALSE);
    const Robo::Color color = ColorFromPacked(packed);
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", color.red, color.green, color.blue);
    return buffer;
}

void AppendTextHtml(std::ostringstream& html, const TextNode& text);
void AppendScrollbarHtml(std::ostringstream& html, const ScrollbarNode& bar);

std::string CssPaint(const std::string& value, bool light) {
    if (value.empty()) {
        return {};
    }
    if (value[0] == '#' || value.rfind("rgb", 0) == 0) {
        return value;
    }
    return TokenCss(value.c_str(), light);
}

void AppendBoardHtml(std::ostringstream& html, const BoardNode& board, bool light) {
    html << "<div class=\"" << RoboWebBoardClass() << "\" data-display=\"" << board.display << "\"";
    if (!board.id.empty()) {
        html << " id=\"" << board.id << "\"";
    }
    html << " style=\"box-sizing:border-box;flex:0 0 auto;display:" << (board.display == "grid" ? "grid" : "flex")
         << ";flex-direction:" << (board.direction == "row" || board.direction == "row-reverse" ? board.direction : "column")
         << ";gap:" << board.gap << "px;padding:" << board.padding << "px";
    if (board.display == "grid") {
        html << ";grid-template-columns:repeat(" << std::max(1, board.columns) << ",minmax(0,1fr))";
    }
    if (board.justify == "center") {
        html << ";justify-content:center";
    }
    if (board.align == "center") {
        html << ";align-items:center";
    }
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
        html << ";background:" << CssPaint(board.background, light);
    }
    html << "\">";
    for (int slot : board.order) {
        if (IsScrollbarSlot(slot)) {
            AppendScrollbarHtml(html, board.bars[static_cast<std::size_t>(slot - kScrollbarSlot)]);
        } else if (slot >= 0) {
            AppendBoardHtml(html, board.children[static_cast<std::size_t>(slot)], light);
        } else {
            AppendTextHtml(html, board.texts[static_cast<std::size_t>(-slot - 1)]);
        }
    }
    html << "</div>";
}

std::string TextClass(const std::string& name) {
    std::string cls = RoboWebTextClass(name);
    if (cls.empty()) {
        cls = RoboWebHeadingClass(name);
    }
    if (cls.empty()) {
        cls = RoboWebBodyClass(name);
    }
    if (cls.empty()) {
        cls = RoboWebParagraphClass(name);
    }
    if (cls.empty()) {
        cls = RoboWebCaptionClass(name);
    }
    if (cls.empty()) {
        cls = RoboWebEyebrowClass(name);
    }
    if (cls.empty()) {
        cls = RoboWebLabelClass(name);
    }
    if (cls.empty()) {
        cls = RoboWebSubheadClass(name);
    }
    if (cls.empty()) {
        cls = "robo-text";
    }
    return cls;
}

void AppendScrollbarHtml(std::ostringstream& html, const ScrollbarNode& bar) {
    const bool vertical = bar.orientation != "horizontal";
    html << "<div class=\"" << RoboWebScrollbarClass() << (vertical ? "" : " is-x") << "\" data-autohide=\""
         << (bar.autoHide ? "true" : "false") << "\" data-arrows=\"" << bar.arrows << "\"";
    if (!bar.id.empty()) {
        html << " id=\"" << bar.id << "\"";
    }
    html << " style=\"position:" << bar.position << ";width:" << bar.laidW << "px;height:" << bar.laidH << "px";
    if (!bar.trackColor.empty()) {
        html << ";--border-default:" << bar.trackColor;
    }
    if (!bar.thumbColor.empty()) {
        html << ";--text-tertiary:" << bar.thumbColor;
    }
    html << "\">";
    if (vertical) {
        html << "<button type=\"button\" class=\"robo-scroll-arrow is-up\" aria-label=\"Scroll up\"></button>";
        html << "<div class=\"robo-scroll-track\"><div class=\"robo-scroll-thumb\"></div></div>";
        html << "<button type=\"button\" class=\"robo-scroll-arrow is-down\" aria-label=\"Scroll down\"></button>";
    } else {
        html << "<button type=\"button\" class=\"robo-scroll-arrow is-left\" aria-label=\"Scroll left\"></button>";
        html << "<div class=\"robo-scroll-track\"><div class=\"robo-scroll-thumb\"></div></div>";
        html << "<button type=\"button\" class=\"robo-scroll-arrow is-right\" aria-label=\"Scroll right\"></button>";
    }
    html << "</div>";
}

void AppendTextHtml(std::ostringstream& html, const TextNode& text) {
    const char* tag = RoboWebTextTag(text.tag);
    if (tag == nullptr) {
        tag = "span";
    }
    RoboFoundationLibrary library;
    std::string style;
    RoboWebAppendTextStyle(style, text.source, library);
    std::string attrs;
    RoboWebTextAttrs(attrs, text.source);
    html << "<" << tag << " class=\"" << TextClass(text.tag) << " " << RoboWebTypeClass(text.source) << "\"";
    if (!text.id.empty()) {
        html << " id=\"" << RoboWebEscape(text.id) << "\"";
    }
    html << attrs;
    if (!style.empty()) {
        html << " style=\"" << style << "\"";
    }
    html << ">" << RoboWebEscape(text.content) << "</" << tag << ">";
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
    const bool light = scene.screen.theme == "light";
    LayoutBoard(board, 0, 0, width, height, light);
    MeasureColumn(board);
    if (board.display == "flex" && board.direction != "row" && board.direction != "row-reverse" && !board.heightSet) {
        LayoutBoard(board, 0, 0, width, std::max(board.laidH, height), light);
    } else {
        LayoutBoard(board, 0, 0, width, height, light);
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
    RoboWebAppendTextCss(css);
    RoboWebAppendScrollbarCss(css);
    std::ostringstream html;
    html << "<!DOCTYPE html><html data-theme=\"" << scene.screen.theme << "\"><head><meta charset=\"utf-8\"/>";
    html << "<title>" << (scene.screen.name.empty() ? "Robix" : scene.screen.name) << "</title><style>";
    const char* tokens[] = {"bg-primary", "bg-secondary", "bg-tertiary", "text-primary", "text-secondary", "text-tertiary",
                             "brand-primary", "danger", "success", "warning", "accent-teal"};
    html << ":root{";
    for (const char* token : tokens) {
        html << "--" << token << ":" << TokenCss(token, light) << ";";
    }
    html << "--border-default:" << TokenCss("border-default", light) << ";--border-strong:" << TokenCss("border-strong", light)
         << ";--space-4:" << RoboFoundationPlatformSpace("space-4") << "px;--body-size:17px;}";
    html << "html,body{margin:0;background:var(--bg-primary);color:var(--text-primary);}";
    html << ".robo-board{box-sizing:border-box;min-width:0;}";
    html << css.str();
    html << "</style></head><body>";
    html << "<div class=\"" << RoboWebScreenClass(true) << "\" data-component=\"Screen\" data-place=\"" << scene.screen.place
         << "\" data-theme=\"" << scene.screen.theme << "\" data-movable=\"" << (scene.screen.movable ? "true" : "false")
         << "\" data-resizable=\"" << (scene.screen.resizable ? "true" : "false") << "\" style=\"width:" << scene.screen.width
         << "px;height:" << scene.screen.height << "px\">";
    if (scene.screen.hasContent) {
        AppendBoardHtml(html, scene.screen.content, light);
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
