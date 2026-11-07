#include "Document.hpp"
#include "Foundation.hpp"

#include "Platform/Framebuffer.hpp"
#include "Platform/Windows/Window.hpp"
#include "Platform/Windows/WindowsFoundation.hpp"
#include "Web/Window.hpp"
#include "../Tools/CLI/RobixCLIAndTestGenerator.hpp"

#include "../Compiler/AST/ASTNodes.hpp"
#include "../Compiler/Lexer/Lexer.hpp"
#include "../Compiler/Parser/Parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <map>
#include <sstream>

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
    std::string event;
};

struct Style {
    std::uint32_t primary = 0xFFFFFF;
    std::uint32_t window = 0xF4F5F7;
    std::uint32_t text = 0x1A1A1A;
    std::uint32_t button = 0x1D4E89;
    std::uint32_t buttonText = 0xFFFFFF;
};

struct Size {
    int w = 0;
    int h = 0;
};

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

std::string LowerCopy(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

std::string AttrOf(const Node& node, std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        if (key == nullptr) continue;
        const auto exact = node.attr.find(key);
        if (exact != node.attr.end()) return exact->second;
        std::string cap(key);
        if (!cap.empty() && cap[0] >= 'a' && cap[0] <= 'z') cap[0] = static_cast<char>(cap[0] - 32);
        const auto upper = node.attr.find(cap);
        if (upper != node.attr.end()) return upper->second;
    }
    return {};
}

int AttrIntOf(const Node& node, std::initializer_list<const char*> keys, int fallback) {
    const std::string text = AttrOf(node, keys);
    if (text.empty()) return fallback;
    try {
        return std::stoi(text);
    } catch (...) {
        return fallback;
    }
}

bool AttrFlag(const Node& node, std::initializer_list<const char*> keys, bool fallback) {
    const std::string text = LowerCopy(AttrOf(node, keys));
    if (text.empty()) return fallback;
    return text == "true" || text == "1";
}

std::string TextFace(const Node& node) {
    const std::string chosen = AttrOf(node, {"font", "family", "Font"});
    if (!chosen.empty()) return chosen;
    if (node.tag == "Button") return "Inter-Medium";
    return "Inter-Regular";
}

std::string FontFamilyCss(const std::string& raw) {
    std::string src;
    std::string token;
    if (FoundationFont(raw, src, token)) return "var(--font-" + token + ")";
    const std::string key = LowerCopy(raw);
    if (key.empty() || key == "sans" || key == "sans-serif" || key == "inter" || key == "inter-regular" || key == "geometric" || key == "humanist" || key == "rounded" || key == "display" || key == "expanded") {
        return "var(--font-sans,'Inter',system-ui,sans-serif)";
    }
    if (key == "sans-medium" || key == "inter-medium" || key == "medium") return "var(--font-sans-medium,'Inter',system-ui,sans-serif)";
    if (key == "sans-semibold" || key == "inter-semibold" || key == "semibold" || key == "semi-bold") return "var(--font-sans-semibold,'Inter',system-ui,sans-serif)";
    if (key == "sans-bold" || key == "inter-bold" || key == "bold") return "var(--font-sans-bold,'Inter',system-ui,sans-serif)";
    if (key == "sans-italic" || key == "inter-italic" || key == "italic") return "var(--font-sans-italic,'Inter',system-ui,sans-serif)";
    if (key == "system" || key == "system-default") return "system-ui,sans-serif";
    if (key == "serif") return "Georgia,\"Times New Roman\",serif";
    if (key == "mono" || key == "monospace") return "ui-monospace,\"Cascadia Code\",Consolas,monospace";
    if (key == "condensed") return "\"Arial Narrow\",\"Segoe UI\",sans-serif";
    if (key == "handwriting" || key == "cursive" || key == "decorative") return "\"Segoe Script\",\"Comic Sans MS\",cursive";
    return raw;
}

std::string NodeId(const Node& node) {
    const std::string id = AttrOf(node, {"id", "Name", "name"});
    return id.empty() ? node.tag : id;
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

Size Measure(const Node& node, int maxW) {
    const std::string& tag = node.tag;
    if (tag == "Screen" || tag == "screen") {
        return {AttrIntOf(node, {"width", "Width"}, 640), AttrIntOf(node, {"height", "Height"}, 400)};
    }
    if (tag == "Board" || tag == "board") {
        const int pad = AttrIntOf(node, {"padding", "Padding"}, 0);
        const int gap = AttrIntOf(node, {"gap"}, 0);
        std::string display = LowerCopy(AttrOf(node, {"display"}));
        if (display.empty()) display = "canvas";
        std::string direction = LowerCopy(AttrOf(node, {"direction", "flexDirection"}));
        if (direction.empty() || display == "stack") direction = "column";
        const int inner = std::max(0, maxW - pad * 2);
        int width = 0;
        int height = 0;
        const int limit = std::min<int>(64, static_cast<int>(node.children.size()));
        if (display == "flex" && (direction == "row" || direction == "row-reverse")) {
            for (int i = 0; i < limit; ++i) {
                const Size childSize = Measure(node.children[static_cast<std::size_t>(i)], inner);
                if (i > 0) width += gap;
                width += childSize.w;
                height = std::max(height, childSize.h);
            }
        } else if (display == "canvas") {
            for (int i = 0; i < limit; ++i) {
                const Node& child = node.children[static_cast<std::size_t>(i)];
                const Size childSize = Measure(child, inner);
                width = std::max(width, AttrIntOf(child, {"x"}, 0) + childSize.w);
                height = std::max(height, AttrIntOf(child, {"y"}, 0) + childSize.h);
            }
        } else {
            for (int i = 0; i < limit; ++i) {
                const Size childSize = Measure(node.children[static_cast<std::size_t>(i)], inner);
                if (i > 0) height += gap;
                height += childSize.h;
                width = std::max(width, childSize.w);
            }
        }
        width += pad * 2;
        height += pad * 2;
        if (!AttrOf(node, {"width", "Width"}).empty()) width = AttrIntOf(node, {"width", "Width"}, width);
        if (!AttrOf(node, {"height", "Height"}).empty()) height = AttrIntOf(node, {"height", "Height"}, height);
        if (AttrFlag(node, {"fill", "fillParent"}, false)) width = maxW;
        return {width, std::max(1, height)};
    }
    if (tag == "Text" || tag == "Button") {
        const int font = FontSizeOf(node, 16);
        const Platform::TextSize measured = Platform::MeasureText(ShownText(node), font, TextFace(node));
        Size text{measured.w, measured.h};
        if (tag == "Button") {
            text.w += 8 * Platform::ScaleFor(font);
            text.h += 8 * Platform::ScaleFor(font);
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

void Place(const Node& node, std::vector<std::uint32_t>& pixels, std::vector<Paint>& paints, int frameW, int frameH,
           int x, int y, int maxW, const Style& style, std::uint32_t foreground) {
    const int pad = IntAttr(node, "Padding", 0);
    const int spacing = IntAttr(node, "Spacing", 0);
    std::uint32_t color = foreground;
    if (node.attr.count("Color")) color = ParseColor(node.attr.at("Color"), color);
    const bool screen = node.tag == "Screen" || node.tag == "screen";
    const bool board = node.tag == "Board" || node.tag == "board";
    if (screen || board) {
        const int padT = AttrIntOf(node, {"paddingTop"}, AttrIntOf(node, {"padding", "Padding"}, 0));
        const int padR = AttrIntOf(node, {"paddingRight"}, AttrIntOf(node, {"padding", "Padding"}, 0));
        const int padB = AttrIntOf(node, {"paddingBottom"}, AttrIntOf(node, {"padding", "Padding"}, 0));
        const int padL = AttrIntOf(node, {"paddingLeft"}, AttrIntOf(node, {"padding", "Padding"}, 0));
        const int rowGap = AttrIntOf(node, {"rowGap"}, AttrIntOf(node, {"gap", "Spacing"}, 0));
        const int colGap = AttrIntOf(node, {"columnGap", "colGap"}, AttrIntOf(node, {"gap", "Spacing"}, 0));
        std::string display = LowerCopy(AttrOf(node, {"display"}));
        if (display.empty()) display = board ? "canvas" : "flex";
        std::string direction = LowerCopy(AttrOf(node, {"direction", "flexDirection"}));
        if (direction.empty() || display == "stack") direction = "column";
        int boxW = screen ? AttrIntOf(node, {"width", "Width"}, frameW) : maxW;
        int boxH = screen ? AttrIntOf(node, {"height", "Height"}, frameH) : AttrIntOf(node, {"height", "Height"}, 0);
        if (board && (AttrFlag(node, {"fill", "fillParent"}, false) || LowerCopy(AttrOf(node, {"size"})) == "fill")) {
            boxW = maxW;
            if (boxH <= 0) boxH = std::max(0, frameH - y);
        }
        if (board && !AttrOf(node, {"width", "Width"}).empty()) boxW = AttrIntOf(node, {"width", "Width"}, boxW);
        const std::string background = AttrOf(node, {"background", "backgroundColor", "bg", "Background"});
        const bool transparent = screen && AttrFlag(node, {"transparent", "transparency"}, false);
        std::uint32_t bg = screen ? style.primary : style.window;
        if (!background.empty()) bg = ParseColor(background, bg);
        const bool paintBg = !background.empty() || (screen && !transparent);
        if (board && boxH <= 0) {
            int bottom = padT + padB;
            const int limit = std::min<int>(64, static_cast<int>(node.children.size()));
            for (int i = 0; i < limit; ++i) {
                const Size childSize = Measure(node.children[static_cast<std::size_t>(i)], std::max(0, boxW - padL - padR));
                if (display == "canvas") bottom = std::max(bottom, padT + AttrIntOf(node.children[static_cast<std::size_t>(i)], {"y"}, 0) + childSize.h);
                else if (direction == "row" || direction == "row-reverse") bottom = std::max(bottom, padT + padB + childSize.h);
                else bottom += childSize.h + (i > 0 ? rowGap : 0);
            }
            boxH = std::max(1, bottom);
        }
        if (paintBg && boxW > 0 && boxH > 0) Platform::Fill(pixels, frameW, frameH, x, y, boxW, boxH, bg);
        const int border = screen ? AttrIntOf(node, {"border"}, 0) : 0;
        if (border > 0) {
            const std::uint32_t edge = ParseColor(AttrOf(node, {"borderColor"}), style.text);
            Platform::Fill(pixels, frameW, frameH, x, y, boxW, border, edge);
            Platform::Fill(pixels, frameW, frameH, x, y + std::max(0, boxH - border), boxW, border, edge);
            Platform::Fill(pixels, frameW, frameH, x, y, border, boxH, edge);
            Platform::Fill(pixels, frameW, frameH, x + std::max(0, boxW - border), y, border, boxH, edge);
        }
        paints.push_back({screen ? "Screen" : "Board", NodeId(node), ShownText(node), x, y, boxW, boxH, bg, paintBg, {}});
        const std::string overflow = LowerCopy(AttrOf(node, {"overflow"}));
        const bool clip = AttrFlag(node, {"clip"}, false) || overflow == "hidden" || overflow == "clip" || overflow == "scroll" || overflow == "auto";
        const Platform::Clip previous = Platform::CurrentClip();
        if (clip) Platform::SetClip({x, y, boxW, boxH, true});
        const int limit = std::min<int>(64, static_cast<int>(node.children.size()));
        if (static_cast<int>(node.children.size()) > 64) {
            std::cerr << (screen ? "screen" : "board") << ": only the first 64 children are placed\n";
        }
        const int innerW = std::max(0, boxW - padL - padR);
        if (display == "canvas") {
            for (int i = 0; i < limit; ++i) {
                const Node& child = node.children[static_cast<std::size_t>(i)];
                Place(child, pixels, paints, frameW, frameH, x + padL + AttrIntOf(child, {"x"}, 0), y + padT + AttrIntOf(child, {"y"}, 0), innerW, style, color);
            }
        } else if (display == "grid") {
            const int columns = std::max(1, AttrIntOf(node, {"columns"}, 1));
            const int cellW = std::max(1, (innerW - (columns - 1) * colGap) / columns);
            int column = 0;
            int cursorY = y + padT;
            int rowH = 0;
            for (int i = 0; i < limit; ++i) {
                const Node& child = node.children[static_cast<std::size_t>(i)];
                const Size childSize = Measure(child, cellW);
                Place(child, pixels, paints, frameW, frameH, x + padL + column * (cellW + colGap), cursorY, cellW, style, color);
                rowH = std::max(rowH, childSize.h);
                if (++column == columns) {
                    column = 0;
                    cursorY += rowH + rowGap;
                    rowH = 0;
                }
            }
        } else {
            const bool horizontal = direction == "row" || direction == "row-reverse";
            std::vector<Size> sizes;
            int sum = 0;
            int fillCount = 0;
            for (int i = 0; i < limit; ++i) {
                const Node& child = node.children[static_cast<std::size_t>(i)];
                Size childSize = Measure(child, innerW);
                const bool fillChild = (child.tag == "Board" || child.tag == "board") && AttrFlag(child, {"fill", "fillParent"}, false);
                if (fillChild) ++fillCount;
                else sum += horizontal ? childSize.w : childSize.h;
                sizes.push_back(childSize);
            }
            const int gapBefore = horizontal ? colGap : rowGap;
            const int gapsBefore = limit > 1 ? (limit - 1) * gapBefore : 0;
            const int roomBefore = horizontal ? innerW : std::max(0, boxH - padT - padB);
            if (fillCount > 0) {
                const int share = std::max(1, (roomBefore - sum - gapsBefore) / fillCount);
                sum = 0;
                for (int i = 0; i < limit; ++i) {
                    const Node& child = node.children[static_cast<std::size_t>(i)];
                    if ((child.tag == "Board" || child.tag == "board") && AttrFlag(child, {"fill", "fillParent"}, false)) {
                        if (horizontal) sizes[static_cast<std::size_t>(i)].w = share;
                        else sizes[static_cast<std::size_t>(i)].h = share;
                    }
                    sum += horizontal ? sizes[static_cast<std::size_t>(i)].w : sizes[static_cast<std::size_t>(i)].h;
                }
            }
            const int gap = horizontal ? colGap : rowGap;
            const int gaps = limit > 1 ? (limit - 1) * gap : 0;
            const int room = horizontal ? innerW : std::max(0, boxH - padT - padB);
            const int leftover = std::max(0, room - sum - gaps);
            std::string justify = LowerCopy(AttrOf(node, {"justify", "justifyContent"}));
            if (justify.empty()) justify = "start";
            int between = gap;
            int cursor = horizontal ? x + padL : y + padT;
            if (justify == "center") cursor += leftover / 2;
            else if (justify == "end") cursor += leftover;
            else if (justify == "space-between" && limit > 1) between += leftover / (limit - 1);
            else if (justify == "space-around" && limit > 0) {
                cursor += leftover / (limit * 2);
                between += leftover / limit;
            } else if (justify == "space-evenly" && limit > 0) {
                const int slot = leftover / (limit + 1);
                cursor += slot;
                between += slot;
            }
            std::string align = LowerCopy(AttrOf(node, {"align", "alignItems"}));
            if (align.empty()) align = screen ? "stretch" : "start";
            const bool reverse = direction == "row-reverse" || direction == "column-reverse";
            for (int step = 0; step < limit; ++step) {
                const int index = reverse ? limit - 1 - step : step;
                const Node& child = node.children[static_cast<std::size_t>(index)];
                const Size childSize = sizes[static_cast<std::size_t>(index)];
                const int crossRoom = horizontal ? std::max(0, boxH - padT - padB) : innerW;
                const int crossSize = horizontal ? childSize.h : childSize.w;
                int cross = horizontal ? y + padT : x + padL;
                if (align == "center") cross += std::max(0, crossRoom - crossSize) / 2;
                else if (align == "end") cross += std::max(0, crossRoom - crossSize);
                const int childMax = (!horizontal && align == "stretch") ? innerW : childSize.w;
                if (horizontal) Place(child, pixels, paints, frameW, frameH, cursor, cross, childSize.w, style, color);
                else Place(child, pixels, paints, frameW, frameH, cross, cursor, childMax, style, color);
                cursor += (horizontal ? childSize.w : childSize.h) + between;
            }
        }
        if (clip) Platform::SetClip(previous);
        return;
    }
    if (node.tag == "Window") {
        const int width = IntAttr(node, "Width", frameW);
        const int height = IntAttr(node, "Height", frameH);
        Platform::Fill(pixels, frameW, frameH, 0, 0, width, height, style.window);
        paints.push_back({"Window", node.attr.count("Name") ? node.attr.at("Name") : "Window", ShownText(node), 0, 0, width, height, style.window, true, {}});
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
            Platform::Fill(pixels, frameW, frameH, x, y, box.w, box.h, bg);
            paints.push_back({node.tag, node.attr.count("Name") ? node.attr.at("Name") : node.tag, {}, x, y, box.w, box.h, bg, true, {}});
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
            Platform::Fill(pixels, frameW, frameH, x, y, box.w, box.h, bg);
        }
        const int scale = Platform::ScaleFor(FontSizeOf(node, 16));
        const int inset = button ? 4 * scale : 0;
        Platform::DrawText(pixels, frameW, frameH, x + inset, y + inset, ShownText(node), FontSizeOf(node, 16), fg, TextFace(node));
        paints.push_back({node.tag, node.attr.count("Name") ? node.attr.at("Name") : node.tag, ShownText(node), x, y, box.w, box.h, button ? bg : fg, button,
                           button && node.attr.count("OnClick") ? node.attr.at("OnClick") : std::string{}});
        return;
    }
    Size box = Measure(node, maxW);
    paints.push_back({node.tag, node.attr.count("Name") ? node.attr.at("Name") : node.tag, ShownText(node), x, y, std::max(box.w, 1), std::max(box.h, 1), color, false, {}});
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
    FoundationPaint paint;
    std::string foundationError;
    if (Platform::Windows::ApplyFoundation(paint, theme == "dark", foundationError)) {
        style.primary = paint.primary;
        style.window = paint.window;
        style.text = paint.text;
        style.button = paint.button;
        style.buttonText = paint.buttonText;
    }
    return style;
}

std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

bool SameComponentFile(const char* file, const char* marker, std::string& error) {
    namespace fs = std::filesystem;
    fs::path root;
    fs::path cursor = fs::current_path();
    for (int depth = 0; depth < 8 && !cursor.empty(); ++depth) {
        if (fs::is_regular_file(cursor / "RobixUI" / "Components" / file)) {
            root = cursor;
            break;
        }
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor) break;
        cursor = cursor.parent_path();
    }
    if (root.empty()) {
        error = std::string("RobixUI: ") + file + " was not found\n";
        return false;
    }
    const std::string shared = ReadFile((root / "RobixUI" / "Components" / file).string());
    const std::string web = ReadFile((root / "RobixUI" / "Web" / file).string());
    const std::string platform = ReadFile((root / "RobixUI" / "Platform" / "Windows" / file).string());
    if (shared.find(marker) == std::string::npos) {
        error = std::string("RobixUI: the shared component is not ") + file + "\n";
        return false;
    }
    if (shared != web || shared != platform) {
        error = std::string("RobixUI: web and Windows ") + file + " are not the same component\n";
        return false;
    }
    return true;
}

bool SameWindowXml(std::string& error) {
    namespace fs = std::filesystem;
    fs::path root;
    fs::path cursor = fs::current_path();
    for (int depth = 0; depth < 8 && !cursor.empty(); ++depth) {
        if (fs::is_regular_file(cursor / "RobixUI" / "Components" / "Window.rbxu")) {
            root = cursor;
            break;
        }
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor) break;
        cursor = cursor.parent_path();
    }
    if (root.empty()) {
        error = "RobixUI: Window.rbxu was not found\n";
        return false;
    }
    const std::string shared = ReadFile((root / "RobixUI" / "Components" / "Window.rbxu").string());
    const std::string web = ReadFile((root / "RobixUI" / "Web" / "Window.rbxu").string());
    const std::string platform = ReadFile((root / "RobixUI" / "Platform" / "Windows" / "Window.rbxu").string());
    if (shared.find("Name=\"Window\"") == std::string::npos) {
        error = "RobixUI: the shared component is not Window\n";
        return false;
    }
    if (shared != web || shared != platform) {
        error = "RobixUI: web and Windows Window.rbxu are not the same component\n";
        return false;
    }
    return true;
}

struct UiView {
    bool ok = false;
    bool screen = false;
    std::string error;
    std::string title;
    std::string name;
    std::string theme;
    std::string place = "center";
    std::string state = "normal";
    int width = 320;
    int height = 180;
    int x = 0;
    int y = 0;
    bool movable = true;
    bool resizable = true;
    bool topmost = false;
    bool fullscreen = false;
    Style style;
    std::vector<std::uint32_t> pixels;
    std::vector<Paint> paints;
    std::string markup;
};

std::string HtmlEscape(const std::string& text) {
    std::string out;
    for (char ch : text) {
        if (ch == '&') out += "&amp;";
        else if (ch == '<') out += "&lt;";
        else if (ch == '>') out += "&gt;";
        else if (ch == '"') out += "&quot;";
        else out.push_back(ch);
    }
    return out;
}

std::string CssAlign(std::string value, const char* fallback) {
    value = LowerCopy(value);
    if (value.empty()) return fallback;
    if (value == "start") return "flex-start";
    if (value == "end") return "flex-end";
    return value;
}

std::string RadiusCss(const std::string& token) {
    if (token.empty() || token == "radius-lg") return "12px";
    if (token == "control") return "8px";
    for (char ch : token) {
        if (ch < '0' || ch > '9') return "12px";
    }
    return token + "px";
}

std::string HtmlNode(const Node& node, bool root);

std::string HtmlChildren(const Node& node, bool root) {
    std::string html;
    const std::string display = LowerCopy(AttrOf(node, {"display"}));
    const bool canvas = node.tag == "Board" || node.tag == "board" ? (display.empty() || display == "canvas") : display == "canvas";
    const int limit = std::min<int>(64, static_cast<int>(node.children.size()));
    for (int i = 0; i < limit; ++i) {
        const Node& child = node.children[static_cast<std::size_t>(i)];
        if (canvas) {
            html += "<div style=\"position:absolute;left:" + std::to_string(AttrIntOf(child, {"x"}, 0)) + "px;top:"
                    + std::to_string(AttrIntOf(child, {"y"}, 0)) + "px\">";
            html += HtmlNode(child, false);
            html += "</div>\n";
        } else {
            html += HtmlNode(child, false);
        }
    }
    (void)root;
    return html;
}

std::string HtmlNode(const Node& node, bool root) {
    const bool screen = node.tag == "Screen" || node.tag == "screen";
    const bool board = node.tag == "Board" || node.tag == "board";
    if (screen || board) {
        const std::string id = AttrOf(node, {"id", "Name"});
        std::string display = LowerCopy(AttrOf(node, {"display"}));
        if (display.empty()) display = board ? "canvas" : "flex";
        std::string direction = LowerCopy(AttrOf(node, {"direction", "flexDirection"}));
        if (direction.empty() || display == "stack") direction = "column";
        const std::string tag = screen ? (root ? "div" : "section") : "div";
        const std::string cls = screen ? (root ? "robo-screen" : "robo-window") : "robo-board";
        std::ostringstream style;
        style << "box-sizing:border-box;outline:none;border:0;box-shadow:none;";
        if (display == "canvas") style << "position:relative;";
        else if (display == "grid") {
            style << "display:grid;grid-template-columns:repeat(" << std::max(1, AttrIntOf(node, {"columns"}, 1)) << ",minmax(0,1fr));";
        } else style << "display:flex;flex-direction:" << direction << ";";
        style << "justify-content:" << CssAlign(AttrOf(node, {"justify", "justifyContent"}), "flex-start") << ";";
        style << "align-items:" << CssAlign(AttrOf(node, {"align", "alignItems"}), screen ? "stretch" : "flex-start") << ";";
        style << "gap:" << AttrIntOf(node, {"gap"}, 0) << "px;";
        style << "padding:" << AttrIntOf(node, {"padding", "Padding"}, 0) << "px;";
        const std::string overflow = LowerCopy(AttrOf(node, {"overflow"}));
        if (AttrFlag(node, {"clip"}, false)) style << "overflow:clip;";
        else if (!overflow.empty()) style << "overflow:" << overflow << ";";
        if (screen) {
            style << "width:" << AttrIntOf(node, {"width", "Width"}, 640) << "px;";
            style << "height:" << AttrIntOf(node, {"height", "Height"}, 400) << "px;";
            const int border = AttrIntOf(node, {"border"}, 0);
            if (border > 0) style << "border:" << border << "px solid " << (AttrOf(node, {"borderColor"}).empty() ? "currentColor" : AttrOf(node, {"borderColor"})) << ";";
            if (!root) style << "border-radius:" << RadiusCss(AttrOf(node, {"radius"})) << ";";
            if (AttrFlag(node, {"transparent", "transparency"}, false)) style << "background:transparent;";
        }
        if (board && AttrFlag(node, {"fill", "fillParent"}, false)) style << "width:100%;height:100%;";
        const std::string background = AttrOf(node, {"background", "backgroundColor", "bg", "Background"});
        if (!background.empty()) style << "background:" << background << ";";
        const auto lengthDecl = [&](const char* css, std::initializer_list<const char*> keys) {
            const std::string value = AttrOf(node, keys);
            if (value.empty()) return;
            bool number = !value.empty();
            for (char ch : value) number = number && ch >= '0' && ch <= '9';
            style << css << ":" << value << (number ? "px;" : ";");
        };
        lengthDecl("row-gap", {"rowGap"});
        lengthDecl("column-gap", {"columnGap", "colGap"});
        lengthDecl("padding-top", {"paddingTop"});
        lengthDecl("padding-right", {"paddingRight"});
        lengthDecl("padding-bottom", {"paddingBottom"});
        lengthDecl("padding-left", {"paddingLeft"});
        lengthDecl("margin", {"margin"});
        lengthDecl("left", {"left"});
        lengthDecl("top", {"top"});
        lengthDecl("right", {"right"});
        lengthDecl("bottom", {"bottom"});
        lengthDecl("min-width", {"minWidth"});
        lengthDecl("min-height", {"minHeight"});
        lengthDecl("max-width", {"maxWidth"});
        lengthDecl("max-height", {"maxHeight"});
        const std::string position = LowerCopy(AttrOf(node, {"position"}));
        if (!position.empty()) style << "position:" << position << ";";
        const std::string wrap = LowerCopy(AttrOf(node, {"wrap", "flexWrap"}));
        if (!wrap.empty()) style << "flex-wrap:" << wrap << ";";
        const std::string grow = AttrOf(node, {"grow", "flex", "flexGrow"});
        if (!grow.empty()) style << "flex-grow:" << grow << ";";
        const std::string self = LowerCopy(AttrOf(node, {"alignSelf"}));
        if (!self.empty() && self != "auto") style << "align-self:" << CssAlign(self, "auto") << ";";
        if (!AttrOf(node, {"order"}).empty()) style << "order:" << AttrOf(node, {"order"}) << ";";
        if (!AttrOf(node, {"gridColumn"}).empty()) style << "grid-column:" << AttrOf(node, {"gridColumn"}) << ";";
        if (!AttrOf(node, {"gridRow"}).empty()) style << "grid-row:" << AttrOf(node, {"gridRow"}) << ";";
        if (!AttrOf(node, {"inset"}).empty()) style << "inset:" << AttrOf(node, {"inset"}) << ";";
        const std::string overflowX = LowerCopy(AttrOf(node, {"overflowX", "overflow-x"}));
        const std::string overflowY = LowerCopy(AttrOf(node, {"overflowY", "overflow-y"}));
        if (!overflowX.empty()) style << "overflow-x:" << overflowX << ";";
        if (!overflowY.empty()) style << "overflow-y:" << overflowY << ";";
        std::ostringstream html;
        html << "<" << tag << " class=\"" << cls << "\"";
        if (!id.empty()) html << " id=\"" << HtmlEscape(id) << "\"";
        if (screen) {
            html << " data-path=\"" << HtmlEscape(AttrOf(node, {"path", "route"}).empty() ? "/" : AttrOf(node, {"path", "route"})) << "\"";
            html << " data-place=\"" << HtmlEscape(LowerCopy(AttrOf(node, {"place"})).empty() ? "center" : LowerCopy(AttrOf(node, {"place"}))) << "\"";
            html << " data-theme=\"" << HtmlEscape(LowerCopy(AttrOf(node, {"theme", "appearance", "Theme"})).empty() ? "dark" : LowerCopy(AttrOf(node, {"theme", "appearance", "Theme"}))) << "\"";
            html << " data-movable=\"" << (AttrFlag(node, {"movable"}, true) ? "true" : "false") << "\"";
            if (!root) html << " data-open=\"true\"";
        }
        html << " style=\"" << style.str() << "\">\n" << HtmlChildren(node, root) << "</" << tag << ">\n";
        return html.str();
    }
    if (node.tag == "Text" || node.tag == "Button") {
        const std::string face = TextFace(node);
        const std::string key = LowerCopy(face);
        const char* weight = "400";
        if (key.find("semibold") != std::string::npos || key == "semi-bold") weight = "600";
        else if (key.find("medium") != std::string::npos) weight = "500";
        else if (key.find("bold") != std::string::npos) weight = "700";
        else if (node.tag == "Button") weight = "500";
        const std::string tag = node.tag == "Button" ? "button type=\"button\"" : "p";
        return "<" + tag + " style=\"font-family:" + FontFamilyCss(face) + ";font-weight:" + weight + "\">" + HtmlEscape(ShownText(node)) + "</" + (node.tag == "Button" ? "button" : "p") + ">\n";
    }
    return HtmlChildren(node, false);
}

std::string BodyFrom(const std::vector<Paint>& paints) {
    std::string body;
    for (const auto& paint : paints) {
        if (paint.kind == "Text" && !paint.text.empty()) body += "<p>" + HtmlEscape(paint.text) + "</p>\n";
        if (paint.kind == "Button") body += "<button type=\"button\">" + HtmlEscape(paint.text) + "</button>\n";
    }
    return body;
}

UiView LoadUi(const std::string& file, const std::string& codePath, bool validate, const std::string& themeArg,
              const std::string& widthArg, const std::string& heightArg) {
    UiView view;
    const std::string text = ReadFile(file);
    if (text.empty()) {
        view.error = "robix ui: could not read " + file + "\n";
        return view;
    }
    ParseResult parsed = ParseDocument(text);
    if (!parsed.error.empty()) {
        view.error = parsed.error + "\n";
        return view;
    }
    if (!codePath.empty() || validate) {
        const std::string code = codePath.empty() ? std::string{} : ReadFile(codePath);
        if (!codePath.empty() && code.empty()) {
            view.error = "robix ui: could not read " + codePath + "\n";
            return view;
        }
        if (!BindHandlers(parsed.root, codePath, code, view.error)) return view;
    }
    Node window;
    bool screen = false;
    for (const auto& child : parsed.root.children) {
        if (child.tag == "Screen" || child.tag == "screen") {
            window = child;
            screen = true;
            break;
        }
    }
    if (!screen) {
        for (const auto& child : parsed.root.children) {
            if (child.tag == "Window") window = child;
        }
    }
    if (window.tag.empty()) {
        view.error = "RobixUI: the document has no Screen\n";
        return view;
    }
    if (screen) {
        const std::string boardId = AttrOf(window, {"board"});
        bool inside = false;
        if (!boardId.empty()) {
            std::function<bool(const Node&)> has = [&](const Node& node) {
                if (AttrOf(node, {"id", "Name", "name"}) == boardId) return true;
                for (const auto& child : node.children) if (has(child)) return true;
                return false;
            };
            inside = has(window);
            if (!inside) {
                for (const auto& child : parsed.root.children) {
                    if ((child.tag == "Board" || child.tag == "board") && AttrOf(child, {"id", "Name", "name"}) == boardId) {
                        window.children.insert(window.children.begin(), child);
                        break;
                    }
                }
            }
        }
        if (!widthArg.empty()) window.attr["width"] = widthArg;
        if (!heightArg.empty()) window.attr["height"] = heightArg;
        view.screen = true;
        view.width = std::clamp(AttrIntOf(window, {"width", "Width"}, 640), AttrIntOf(window, {"minWidth"}, 320), AttrIntOf(window, {"maxWidth"}, 1920));
        view.height = std::clamp(AttrIntOf(window, {"height", "Height"}, 400), AttrIntOf(window, {"minHeight"}, 200), AttrIntOf(window, {"maxHeight"}, 1080));
        window.attr["width"] = std::to_string(view.width);
        window.attr["height"] = std::to_string(view.height);
        view.place = LowerCopy(AttrOf(window, {"place"}));
        if (view.place.empty()) view.place = "center";
        view.x = AttrIntOf(window, {"x"}, 0);
        view.y = AttrIntOf(window, {"y"}, 0);
        view.state = LowerCopy(AttrOf(window, {"state"}));
        if (view.state.empty()) view.state = "normal";
        view.movable = AttrFlag(window, {"movable"}, true);
        const std::string resizable = LowerCopy(AttrOf(window, {"resizable"}));
        view.resizable = resizable.empty() || resizable == "true" || resizable == "1";
        view.topmost = AttrFlag(window, {"topmost", "alwaysOnTop"}, false);
        view.fullscreen = AttrFlag(window, {"fullscreen"}, false);
        if (!themeArg.empty()) window.attr["theme"] = themeArg;
        view.theme = !themeArg.empty() ? themeArg : AttrOf(window, {"theme", "appearance", "Theme"});
        if (view.theme.empty()) view.theme = "dark";
        window.attr["theme"] = view.theme;
        view.title = AttrOf(window, {"name", "title", "Title"});
        if (view.title.empty()) view.title = AttrOf(window, {"id", "Name"});
        if (view.title.empty()) view.title = "RobixUI";
        view.name = AttrOf(window, {"id", "Name"});
        if (view.name.empty()) view.name = "Screen";
        view.markup = HtmlNode(window, true);
    } else {
        if (!widthArg.empty()) window.attr["Width"] = widthArg;
        if (!heightArg.empty()) window.attr["Height"] = heightArg;
        view.width = IntAttr(window, "Width", 320);
        view.height = IntAttr(window, "Height", 180);
        view.title = window.attr.count("Title") ? window.attr.at("Title") : window.attr.count("Name") ? window.attr.at("Name") : "RobixUI";
        view.name = window.attr.count("Name") ? window.attr.at("Name") : "Window";
        view.theme = !themeArg.empty() ? themeArg : window.attr.count("Theme") ? window.attr.at("Theme") : "Light";
    }
    if (view.width <= 0 || view.height <= 0) {
        view.error = "RobixUI: the screen size is empty\n";
        return view;
    }
    view.style = ThemeFor(parsed.root, view.theme);
    const std::uint32_t clear = screen && !AttrFlag(window, {"transparent", "transparency"}, false) ? view.style.primary : view.style.window;
    view.pixels.assign(static_cast<std::size_t>(view.width) * static_cast<std::size_t>(view.height), clear);
    Place(window, view.pixels, view.paints, view.width, view.height, 0, 0, view.width, view.style, view.style.text);
    view.ok = true;
    return view;
}

void PrintLayout(const UiView& view) {
    for (const auto& paint : view.paints) {
        std::cout << "layout " << paint.kind << " " << paint.name << " " << paint.x << " " << paint.y
                  << " " << paint.w << " " << paint.h << "\n";
    }
}

void RunInterpreted(const std::string& codePath) {
    if (codePath.empty()) return;
    const auto result = Robix::Tools::RobixCLIDispatcher::ExecuteCommand(
        Robix::Tools::CLIBinaryKind::RobixUnifiedCLI, { "run", codePath });
    if (!result.stdoutText.empty()) std::cout << result.stdoutText;
    if (!result.stderrText.empty()) std::cerr << result.stderrText;
}

std::filesystem::file_time_type Stamp(const std::string& path) {
    if (path.empty()) return {};
    std::error_code ec;
    return std::filesystem::last_write_time(path, ec);
}

struct DevWatch {
    std::string file;
    std::string code;
    std::string theme;
    std::string widthArg;
    std::string heightArg;
    std::filesystem::file_time_type rbxu{};
    std::filesystem::file_time_type source{};
    UiView view;
    std::string html;
    int generation = 1;
    bool ready = false;
};

bool RefreshDev(DevWatch& watch, bool windows) {
    const auto rbxu = Stamp(watch.file);
    const auto source = Stamp(watch.code);
    if (watch.ready && rbxu == watch.rbxu && source == watch.source) return false;
    const bool first = !watch.ready;
    watch.rbxu = rbxu;
    watch.source = source;
    watch.ready = true;
    RunInterpreted(watch.code);
    UiView next = LoadUi(watch.file, watch.code, false, watch.theme, watch.widthArg, watch.heightArg);
    if (!next.ok) {
        std::cerr << next.error;
        return false;
    }
    watch.view = std::move(next);
    watch.html = Web::RenderWindow(watch.view.name, watch.view.title, watch.view.width, watch.view.height, watch.view.theme,
                                   watch.view.style.window, watch.view.style.text,
                                   watch.view.markup.empty() ? BodyFrom(watch.view.paints) : watch.view.markup, true);
    PrintLayout(watch.view);
    if (!first) {
        if (watch.generation < 1000000) ++watch.generation;
        std::cout << (windows ? "window: updated " : "web: updated ") << watch.view.title << "\n";
    }
    std::cout.flush();
    return true;
}

bool ResizeWindow(std::vector<std::uint32_t>& pixels, int& width, int& height, std::string& title, void* user) {
    auto* watch = static_cast<DevWatch*>(user);
    if (watch == nullptr || width <= 0 || height <= 0) return false;
    watch->widthArg = std::to_string(width);
    watch->heightArg = std::to_string(height);
    UiView next = LoadUi(watch->file, watch->code, false, watch->theme, watch->widthArg, watch->heightArg);
    if (!next.ok || next.pixels.empty()) return false;
    watch->view = std::move(next);
    pixels = watch->view.pixels;
    width = watch->view.width;
    height = watch->view.height;
    title = watch->view.title;
    return true;
}

bool RefreshWindow(std::vector<std::uint32_t>& pixels, int& width, int& height, std::string& title, void* user) {
    auto* watch = static_cast<DevWatch*>(user);
    if (!RefreshDev(*watch, true)) return false;
    pixels = watch->view.pixels;
    width = watch->view.width;
    height = watch->view.height;
    title = watch->view.title;
    return true;
}

void ClickWindow(int x, int y, void* user) {
    auto* watch = static_cast<DevWatch*>(user);
    for (const auto& paint : watch->view.paints) {
        if (paint.kind != "Button") continue;
        if (x >= paint.x && y >= paint.y && x < paint.x + paint.w && y < paint.y + paint.h) {
            std::cout << "click " << (paint.event.empty() ? paint.name : paint.event) << "\n";
            std::cout.flush();
            return;
        }
    }
}

void ProvideWeb(Web::DevPage& page, void* user) {
    auto* watch = static_cast<DevWatch*>(user);
    RefreshDev(*watch, false);
    page.html = watch->html;
    page.generation = watch->generation;
}

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
        std::cerr << "robix ui: use validate, render -o <file.bmp>, web, or window. Add --dev to keep it open and reload changes.\n";
        return 1;
    }
    const std::string action = args[1];
    const std::string file = args[2];
    const std::string codePath = Arg(args, "--code");
    const std::string themeArg = Arg(args, "--theme");
    const std::string widthArg = Arg(args, "--width");
    const std::string heightArg = Arg(args, "--height");
    bool dev = false;
    for (const auto& arg : args) dev = dev || arg == "--dev";
    UiView view = LoadUi(file, codePath, action == "validate", themeArg, widthArg, heightArg);
    if (!view.ok) {
        std::cerr << view.error;
        return 1;
    }
    PrintLayout(view);
    if (action == "validate") return 0;
    std::string componentError;
    if ((action == "window" || action == "web") && view.screen && (!SameComponentFile("Screen.rbxu", "Name=\"Screen\"", componentError) || !SameComponentFile("Board.rbxu", "Name=\"Board\"", componentError) || !SameFoundation(componentError))) {
        std::cerr << componentError;
        return 1;
    }
    if ((action == "window" || action == "web") && !view.screen && (!SameWindowXml(componentError) || !SameFoundation(componentError))) {
        std::cerr << componentError;
        return 1;
    }
    if (action == "window" || action == "web") std::cout << FoundationLine() << "\n";
    const std::string& title = view.title;
    const std::string& name = view.name;
    const std::string& theme = view.theme;
    const int width = view.width;
    const int height = view.height;
    const Style style = view.style;
    const std::vector<std::uint32_t>& pixels = view.pixels;
    const std::vector<Paint>& paints = view.paints;
    if (action == "web" && dev) {
        RunInterpreted(codePath);
        DevWatch watch;
        watch.file = file;
        watch.code = codePath;
        watch.theme = themeArg;
        watch.widthArg = widthArg;
        watch.heightArg = heightArg;
        watch.view = view;
        watch.rbxu = Stamp(file);
        watch.source = Stamp(codePath);
        watch.ready = true;
        watch.html = Web::RenderWindow(name, title, width, height, theme, style.window, style.text, view.markup.empty() ? BodyFrom(paints) : view.markup, true);
        std::cout << (view.screen ? "component Screen\ncomponent Board\n" : "component Window\n");
        if (!Web::ServeDev(ProvideWeb, &watch)) {
            std::cerr << "robix ui web: the development server did not start\n";
            return 1;
        }
        return 0;
    }
    if (action == "window" && dev) {
        RunInterpreted(codePath);
        DevWatch watch;
        watch.file = file;
        watch.code = codePath;
        watch.theme = themeArg;
        watch.widthArg = widthArg;
        watch.heightArg = heightArg;
        watch.view = view;
        watch.rbxu = Stamp(file);
        watch.source = Stamp(codePath);
        watch.ready = true;
        std::cout << (view.screen ? "component Screen\ncomponent Board\n" : "component Window\n");
        if (!Platform::Windows::OpenWindowDev(title, pixels, width, height, RefreshWindow, ResizeWindow, ClickWindow, &watch)) {
            std::cerr << "robix ui window: the window was not painted\n";
            return 1;
        }
        return 0;
    }
    if (action == "web") {
        const std::string output = Arg(args, "-o");
        if (output.empty()) {
            std::cerr << "robix ui web: -o is required\n";
            return 1;
        }
        const auto slash = output.find_last_of("/\\");
        if (slash != std::string::npos) {
            std::error_code ec;
            std::filesystem::create_directories(output.substr(0, slash), ec);
        }
        if (!Web::WriteWindow(output, name, title, width, height, theme, style.window, style.text, view.markup.empty() ? BodyFrom(paints) : view.markup, false)) {
            std::cerr << "robix ui web: could not write " << output << "\n";
            return 1;
        }
        std::cout << (view.screen ? "component Screen\ncomponent Board\n" : "component Window\n");
        std::cout << "web " << output << " " << width << "x" << height << " title " << title << "\n";
        return 0;
    }
    if (action == "window") {
        if (view.screen) {
            Platform::Windows::ScreenHost host;
            host.title = title;
            host.place = view.place;
            host.state = view.state;
            host.x = view.x;
            host.y = view.y;
            host.width = width;
            host.height = height;
            host.movable = view.movable;
            host.resizable = view.resizable;
            host.topmost = view.topmost;
            host.fullscreen = view.fullscreen;
            host.pixels = pixels;
            for (const auto& paint : paints) {
                if (paint.kind != "Button") continue;
                host.buttons.push_back(paint.x);
                host.buttons.push_back(paint.y);
                host.buttons.push_back(paint.w);
                host.buttons.push_back(paint.h);
            }
            if (view.state != "closed" && view.state != "minimized" && !Platform::Windows::OpenScreen(host)) {
                std::cerr << "robix ui window: the screen was not painted\n";
                return 1;
            }
            if (view.state == "closed" || view.state == "minimized") {
                std::cout << "screen state " << view.state << "\n";
            }
        } else if (!Platform::Windows::OpenWindow(title, width, height, pixels)) {
            std::cerr << "robix ui window: the window was not painted\n";
            return 1;
        }
        std::cout << (view.screen ? "component Screen\ncomponent Board\n" : "component Window\n");
        std::cout << "window: shown " << width << "x" << height << " title " << title << "\n";
        std::cout << "window: closed after paint\n";
        return 0;
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
    if (!Platform::WriteBmp(output, width, height, pixels)) {
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
    std::cout << "renderer platform-framebuffer " << output << "\n";
    return 0;
}

} // namespace Robix::UI
