#include "RoboCanvas.hpp"

#include "../Text/RoboText.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace {

constexpr int ROBO_CANVAS_MAX_COMMANDS = 64;
constexpr int ROBO_CANVAS_MAX_SIZE = 4096;
constexpr RoboU32 ROBO_CANVAS_DEFAULT_COLOR = 0xFF000000u;

const char* const ROBO_CANVAS_ATTRIBUTES[] = {
    "id", "width", "height",
};

const char* const ROBO_CANVAS_RECT_ATTRIBUTES[] = {
    "x", "y", "width", "height", "color",
};

const char* const ROBO_CANVAS_DISC_ATTRIBUTES[] = {
    "x", "y", "radius", "r", "color",
};

const char* const ROBO_CANVAS_LINE_ATTRIBUTES[] = {
    "x", "y", "x1", "y1", "x2", "y2", "color",
};

const char* const ROBO_CANVAS_TEXT_ATTRIBUTES[] = {
    "x", "y", "value", "content", "text", "color", "size", "fontsize", "family", "font",
};

std::string RoboCanvasLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string RoboCanvasFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

bool RoboCanvasKnown(const std::string& key, const char* const* names, int count) {
    const std::string folded = RoboCanvasLower(key);
    for (int index = 0; index < count; ++index) {
        if (folded == names[index]) {
            return true;
        }
    }
    return false;
}

int RoboCanvasHex(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

bool RoboCanvasColor(const std::string& text, RoboU32& color) {
    if (text.size() < 4 || text[0] != '#') {
        return false;
    }
    std::string hex = text.substr(1);
    if (hex.size() == 3 || hex.size() == 4) {
        std::string expanded;
        for (char ch : hex) {
            expanded.push_back(ch);
            expanded.push_back(ch);
        }
        hex = expanded;
    }
    if (hex.size() != 6 && hex.size() != 8) {
        return false;
    }
    RoboU32 value = 0;
    for (char ch : hex) {
        const int digit = RoboCanvasHex(ch);
        if (digit < 0) {
            return false;
        }
        value = (value << 4) | static_cast<RoboU32>(digit);
    }
    if (hex.size() == 6) {
        value |= 0xFF000000u;
    } else {
        const RoboU32 red = (value >> 24) & 0xFFu;
        const RoboU32 green = (value >> 16) & 0xFFu;
        const RoboU32 blue = (value >> 8) & 0xFFu;
        const RoboU32 alpha = value & 0xFFu;
        value = (alpha << 24) | (red << 16) | (green << 8) | blue;
    }
    color = value;
    return true;
}

bool RoboCanvasInteger(const std::string& text, int& number, bool allowNegative) {
    if (text.empty()) {
        return false;
    }
    std::size_t index = 0;
    int sign = 1;
    if (text[0] == '+') {
        index = 1;
    } else if (text[0] == '-') {
        if (!allowNegative) {
            return false;
        }
        sign = -1;
        index = 1;
    }
    if (index >= text.size()) {
        return false;
    }
    int value = 0;
    for (; index < text.size(); ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
        value = value * 10 + (text[index] - '0');
    }
    number = value * sign;
    return true;
}

bool RoboCanvasReadColor(const RoboXMLNode& node, RoboU32& color, const char* what, std::string& diagnostic) {
    const std::string text = RoboXMLAttr(node, "color");
    color = ROBO_CANVAS_DEFAULT_COLOR;
    if (text.empty()) {
        return true;
    }
    if (!RoboCanvasColor(text, color)) {
        diagnostic = RoboCanvasFail("R7602", std::string(what) + " color '" + text + "' is not a color. Use #RRGGBB.");
        return false;
    }
    return true;
}

bool RoboCanvasReadPoint(const RoboXMLNode& node, const char* key, int& number, const char* what, std::string& diagnostic) {
    const std::string text = RoboXMLAttr(node, key);
    if (text.empty()) {
        number = 0;
        return true;
    }
    if (!RoboCanvasInteger(text, number, true)) {
        diagnostic = RoboCanvasFail("R7603", std::string(what) + " " + key + " '" + text + "' is not a position. Use a whole number.");
        return false;
    }
    return true;
}

bool RoboCanvasReadRect(const RoboXMLNode& node, RoboCanvasCommand& command, std::string& diagnostic) {
    for (const auto& entry : node.attrs) {
        if (!RoboCanvasKnown(entry.first, ROBO_CANVAS_RECT_ATTRIBUTES,
                             static_cast<int>(sizeof(ROBO_CANVAS_RECT_ATTRIBUTES) / sizeof(ROBO_CANVAS_RECT_ATTRIBUTES[0])))) {
            diagnostic = RoboCanvasFail("R7601", "Rect attribute '" + entry.first + "' is unknown. Use x, y, width, height, or color.");
            return false;
        }
    }
    command.kind = RoboCanvasKind::Rect;
    if (!RoboCanvasReadPoint(node, "x", command.x, "Rect", diagnostic) || !RoboCanvasReadPoint(node, "y", command.y, "Rect", diagnostic)) {
        return false;
    }
    const std::string width = RoboXMLAttr(node, "width");
    const std::string height = RoboXMLAttr(node, "height");
    if (!RoboCanvasInteger(width, command.width, false) || command.width < 1 || command.width > ROBO_CANVAS_MAX_SIZE) {
        diagnostic = RoboCanvasFail("R7603", "Rect width '" + width + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    if (!RoboCanvasInteger(height, command.height, false) || command.height < 1 || command.height > ROBO_CANVAS_MAX_SIZE) {
        diagnostic = RoboCanvasFail("R7603", "Rect height '" + height + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    return RoboCanvasReadColor(node, command.color, "Rect", diagnostic);
}

bool RoboCanvasReadDisc(const RoboXMLNode& node, RoboCanvasCommand& command, std::string& diagnostic) {
    for (const auto& entry : node.attrs) {
        if (!RoboCanvasKnown(entry.first, ROBO_CANVAS_DISC_ATTRIBUTES,
                             static_cast<int>(sizeof(ROBO_CANVAS_DISC_ATTRIBUTES) / sizeof(ROBO_CANVAS_DISC_ATTRIBUTES[0])))) {
            diagnostic = RoboCanvasFail("R7601", "Disc attribute '" + entry.first + "' is unknown. Use x, y, radius, or color.");
            return false;
        }
    }
    command.kind = RoboCanvasKind::Disc;
    if (!RoboCanvasReadPoint(node, "x", command.x, "Disc", diagnostic) || !RoboCanvasReadPoint(node, "y", command.y, "Disc", diagnostic)) {
        return false;
    }
    std::string radius = RoboXMLAttr(node, "radius");
    if (radius.empty()) {
        radius = RoboXMLAttr(node, "r");
    }
    if (!RoboCanvasInteger(radius, command.radius, false) || command.radius < 1 || command.radius > ROBO_CANVAS_MAX_SIZE) {
        diagnostic = RoboCanvasFail("R7603", "Disc radius '" + radius + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    return RoboCanvasReadColor(node, command.color, "Disc", diagnostic);
}

bool RoboCanvasReadLine(const RoboXMLNode& node, RoboCanvasCommand& command, std::string& diagnostic) {
    for (const auto& entry : node.attrs) {
        if (!RoboCanvasKnown(entry.first, ROBO_CANVAS_LINE_ATTRIBUTES,
                             static_cast<int>(sizeof(ROBO_CANVAS_LINE_ATTRIBUTES) / sizeof(ROBO_CANVAS_LINE_ATTRIBUTES[0])))) {
            diagnostic = RoboCanvasFail("R7601", "Line attribute '" + entry.first + "' is unknown. Use x1, y1, x2, y2, or color.");
            return false;
        }
    }
    command.kind = RoboCanvasKind::Line;
    if (!RoboCanvasReadPoint(node, "x1", command.x, "Line", diagnostic) || !RoboCanvasReadPoint(node, "y1", command.y, "Line", diagnostic) ||
        !RoboCanvasReadPoint(node, "x2", command.x2, "Line", diagnostic) || !RoboCanvasReadPoint(node, "y2", command.y2, "Line", diagnostic)) {
        return false;
    }
    if (RoboXMLAttr(node, "x1").empty()) {
        if (!RoboCanvasReadPoint(node, "x", command.x, "Line", diagnostic)) {
            return false;
        }
    }
    if (RoboXMLAttr(node, "y1").empty()) {
        if (!RoboCanvasReadPoint(node, "y", command.y, "Line", diagnostic)) {
            return false;
        }
    }
    return RoboCanvasReadColor(node, command.color, "Line", diagnostic);
}

bool RoboCanvasReadText(const RoboXMLNode& node, RoboCanvasCommand& command, std::string& diagnostic) {
    for (const auto& entry : node.attrs) {
        if (!RoboCanvasKnown(entry.first, ROBO_CANVAS_TEXT_ATTRIBUTES,
                             static_cast<int>(sizeof(ROBO_CANVAS_TEXT_ATTRIBUTES) / sizeof(ROBO_CANVAS_TEXT_ATTRIBUTES[0])))) {
            diagnostic = RoboCanvasFail("R7601", "Text attribute '" + entry.first + "' is unknown. Use value, x, y, size, or color.");
            return false;
        }
    }
    command.kind = RoboCanvasKind::Text;
    if (!RoboCanvasReadPoint(node, "x", command.x, "Text", diagnostic) || !RoboCanvasReadPoint(node, "y", command.y, "Text", diagnostic)) {
        return false;
    }
    command.value = RoboXMLAttr(node, "value");
    if (command.value.empty()) {
        command.value = RoboXMLAttr(node, "content");
    }
    if (command.value.empty()) {
        command.value = RoboXMLAttr(node, "text");
    }
    if (command.value.empty()) {
        diagnostic = RoboCanvasFail("R7605", "Text needs a value.");
        return false;
    }
    command.family = RoboXMLAttr(node, "family");
    if (command.family.empty()) {
        command.family = RoboXMLAttr(node, "font");
    }
    const std::string size = RoboXMLAttr(node, "size");
    const std::string fontSize = RoboXMLAttr(node, "fontSize");
    const std::string sizeText = !size.empty() ? size : fontSize;
    command.size = 16;
    if (!sizeText.empty()) {
        if (!RoboCanvasInteger(sizeText, command.size, false) || command.size < 1 || command.size > ROBO_CANVAS_MAX_SIZE) {
            diagnostic = RoboCanvasFail("R7603", "Text size '" + sizeText + "' is not a size. Use a whole number of pixels.");
            return false;
        }
    }
    return RoboCanvasReadColor(node, command.color, "Text", diagnostic);
}

void RoboCanvasPlot(RoboCanvasFrame& frame, int x, int y, RoboU32 color) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return;
    }
    frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)] = color;
}

void RoboCanvasFillRect(RoboCanvasFrame& frame, const RoboCanvasCommand& command) {
    for (int row = 0; row < command.height; ++row) {
        for (int column = 0; column < command.width; ++column) {
            RoboCanvasPlot(frame, command.x + column, command.y + row, command.color);
        }
    }
}

void RoboCanvasFillDisc(RoboCanvasFrame& frame, const RoboCanvasCommand& command) {
    const int radius = command.radius;
    const int limit = radius * radius;
    for (int row = -radius; row <= radius; ++row) {
        for (int column = -radius; column <= radius; ++column) {
            if (column * column + row * row <= limit) {
                RoboCanvasPlot(frame, command.x + column, command.y + row, command.color);
            }
        }
    }
}

void RoboCanvasStrokeLine(RoboCanvasFrame& frame, const RoboCanvasCommand& command) {
    int x = command.x;
    int y = command.y;
    const int x2 = command.x2;
    const int y2 = command.y2;
    const int dx = x2 >= x ? x2 - x : x - x2;
    const int dy = y2 >= y ? y2 - y : y - y2;
    const int stepX = x < x2 ? 1 : -1;
    const int stepY = y < y2 ? 1 : -1;
    int error = dx - dy;
    for (;;) {
        RoboCanvasPlot(frame, x, y, command.color);
        if (x == x2 && y == y2) {
            break;
        }
        const int doubled = error * 2;
        if (doubled > -dy) {
            error -= dy;
            x += stepX;
        }
        if (doubled < dx) {
            error += dx;
            y += stepY;
        }
    }
}

void RoboCanvasDrawText(RoboCanvasFrame& frame, const RoboCanvasCommand& command, const RoboFontPackView& pack) {
    if (pack.header == nullptr || command.value.empty()) {
        return;
    }
    RoboTextNode text;
    text.value = command.value;
    text.family = command.family.empty() ? "Inter" : command.family;
    text.weight = "regular";
    text.size = command.size;
    text.color = command.color;
    text.wrap = "none";
    text.align = "start";
    text.face = text.family;
    if (text.face.find('-') == std::string::npos) {
        text.face += "-Regular";
    }
    RoboTextRun run;
    run.value = command.value;
    run.size = command.size;
    run.hasSize = true;
    run.color = command.color;
    text.runs.push_back(run);
    RoboTextFrame glyphs;
    RoboTextPaint(text, pack, glyphs);
    for (int row = 0; row < glyphs.height; ++row) {
        for (int column = 0; column < glyphs.width; ++column) {
            const RoboU32 pixel = glyphs.pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(glyphs.width) + static_cast<std::size_t>(column)];
            if (((pixel >> 24) & 0xFFu) == 0) {
                continue;
            }
            RoboCanvasPlot(frame, command.x + column, command.y + row, pixel);
        }
    }
}

}  // namespace

bool RoboCanvasRead(const RoboXMLNode& node, RoboCanvasNode& canvas, std::string& diagnostic) {
    canvas = RoboCanvasNode{};
    diagnostic.clear();
    if (RoboCanvasLower(node.name) != "canvas") {
        diagnostic = RoboCanvasFail("R7604", "'" + node.name + "' is not a Canvas. Use Canvas.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!RoboCanvasKnown(entry.first, ROBO_CANVAS_ATTRIBUTES,
                             static_cast<int>(sizeof(ROBO_CANVAS_ATTRIBUTES) / sizeof(ROBO_CANVAS_ATTRIBUTES[0])))) {
            diagnostic = RoboCanvasFail("R7601", "Canvas attribute '" + entry.first + "' is unknown. Use width, height, and a draw list.");
            return false;
        }
    }
    const std::string width = RoboXMLAttr(node, "width");
    const std::string height = RoboXMLAttr(node, "height");
    if (!RoboCanvasInteger(width, canvas.width, false) || canvas.width < 1 || canvas.width > ROBO_CANVAS_MAX_SIZE) {
        diagnostic = RoboCanvasFail("R7603", "Canvas width '" + width + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    if (!RoboCanvasInteger(height, canvas.height, false) || canvas.height < 1 || canvas.height > ROBO_CANVAS_MAX_SIZE) {
        diagnostic = RoboCanvasFail("R7603", "Canvas height '" + height + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    if (node.children.empty()) {
        diagnostic = RoboCanvasFail("R7605", "Canvas needs a draw list. Use Rect, Disc, Line, or Text.");
        return false;
    }
    if (static_cast<int>(node.children.size()) > ROBO_CANVAS_MAX_COMMANDS) {
        diagnostic = RoboCanvasFail("R7606", "Canvas has more than 64 draws. Remove the extra draws.");
        return false;
    }
    for (const RoboXMLNode& child : node.children) {
        const std::string name = RoboCanvasLower(child.name);
        RoboCanvasCommand command;
        bool ok = false;
        if (name == "rect") {
            ok = RoboCanvasReadRect(child, command, diagnostic);
        } else if (name == "disc" || name == "circle") {
            ok = RoboCanvasReadDisc(child, command, diagnostic);
        } else if (name == "line") {
            ok = RoboCanvasReadLine(child, command, diagnostic);
        } else if (name == "text") {
            ok = RoboCanvasReadText(child, command, diagnostic);
        } else {
            diagnostic = RoboCanvasFail("R7604", "'" + child.name + "' is not a draw. Use Rect, Disc, Line, or Text.");
            return false;
        }
        if (!ok) {
            return false;
        }
        canvas.commands.push_back(command);
    }
    return true;
}

void RoboCanvasPaint(const RoboCanvasNode& canvas, const RoboFontPackView& pack, RoboCanvasFrame& frame) {
    frame.width = canvas.width > 0 ? canvas.width : 0;
    frame.height = canvas.height > 0 ? canvas.height : 0;
    frame.pixels.assign(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height), 0);
    for (const RoboCanvasCommand& command : canvas.commands) {
        if (command.kind == RoboCanvasKind::Rect) {
            RoboCanvasFillRect(frame, command);
        } else if (command.kind == RoboCanvasKind::Disc) {
            RoboCanvasFillDisc(frame, command);
        } else if (command.kind == RoboCanvasKind::Line) {
            RoboCanvasStrokeLine(frame, command);
        } else if (command.kind == RoboCanvasKind::Text) {
            RoboCanvasDrawText(frame, command, pack);
        }
    }
}

bool RoboCanvasPixel(const RoboCanvasFrame& frame, int x, int y, RoboU32& color) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return false;
    }
    color = frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)];
    return true;
}
