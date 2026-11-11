#include "RoboSvg.hpp"

#include "../../../ThirdParty/nanosvg/nanosvg.h"
#include "../../../ThirdParty/nanosvg/nanosvgrast.h"

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

constexpr int ROBO_SVG_DEFAULT_SIZE = 24;
constexpr int ROBO_SVG_MAX_SIZE = 512;
constexpr int ROBO_SVG_PATH_MAX = 32;

const char* const ROBO_SVG_ATTRIBUTES[] = {
    "id", "paths", "d", "fill", "stroke", "strokewidth", "stroke-width", "viewbox", "width", "height",
};

const char* const ROBO_SVG_PATH_ATTRIBUTES[] = {
    "d", "paths", "fill", "stroke", "strokewidth", "stroke-width",
};

std::string RoboSvgLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string RoboSvgFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

bool RoboSvgKnown(const std::string& key, const char* const* names, int count) {
    const std::string folded = RoboSvgLower(key);
    for (int index = 0; index < count; ++index) {
        if (folded == names[index]) {
            return true;
        }
    }
    return false;
}

int RoboSvgHex(char ch) {
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

bool RoboSvgColor(const std::string& text, RoboU32& color) {
    if (RoboSvgLower(text) == "none") {
        return false;
    }
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
        const int digit = RoboSvgHex(ch);
        if (digit < 0) {
            return false;
        }
        value = (value << 4) | static_cast<RoboU32>(digit);
    }
    if (hex.size() == 6) {
        value |= 0xFF000000u;
    }
    color = value;
    return true;
}

bool RoboSvgPaintValue(const std::string& text) {
    if (RoboSvgLower(text) == "none") {
        return true;
    }
    RoboU32 color = 0;
    return RoboSvgColor(text, color);
}

bool RoboSvgInteger(const std::string& text, int& number) {
    if (text.empty()) {
        return false;
    }
    std::size_t index = 0;
    if (text[0] == '+') {
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
    number = value;
    return true;
}

bool RoboSvgNumber(const std::string& text, float& number) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const float value = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || end == nullptr || *end != '\0') {
        return false;
    }
    number = value;
    return true;
}

bool RoboSvgViewBox(const std::string& text, float& x, float& y, float& width, float& height) {
    const char* cursor = text.c_str();
    char* end = nullptr;
    float parsed[4];
    for (int index = 0; index < 4; ++index) {
        while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)) != 0) {
            ++cursor;
        }
        if (*cursor == '\0') {
            return false;
        }
        parsed[index] = std::strtof(cursor, &end);
        if (end == cursor) {
            return false;
        }
        cursor = end;
        while (*cursor == ',') {
            ++cursor;
        }
    }
    while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)) != 0) {
        ++cursor;
    }
    if (*cursor != '\0' || parsed[2] <= 0.0f || parsed[3] <= 0.0f) {
        return false;
    }
    x = parsed[0];
    y = parsed[1];
    width = parsed[2];
    height = parsed[3];
    return true;
}

std::string RoboSvgEscape(const std::string& value) {
    std::string out;
    for (char ch : value) {
        if (ch == '&') {
            out += "&amp;";
        } else if (ch == '"') {
            out += "&quot;";
        } else if (ch == '<') {
            out += "&lt;";
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

std::string RoboSvgHexColor(const std::string& text) {
    if (RoboSvgLower(text) == "none") {
        return "none";
    }
    RoboU32 color = 0;
    if (!RoboSvgColor(text, color)) {
        return "#000000";
    }
    const char* digits = "0123456789ABCDEF";
    std::string out = "#";
    const int shifts[3] = {16, 8, 0};
    for (int shift : shifts) {
        const int channel = static_cast<int>((color >> shift) & 0xFFu);
        out.push_back(digits[(channel >> 4) & 0xF]);
        out.push_back(digits[channel & 0xF]);
    }
    return out;
}

bool RoboSvgReadPath(const RoboXMLNode& node, RoboSvgPath& path, std::string& diagnostic) {
    for (const auto& entry : node.attrs) {
        if (!RoboSvgKnown(entry.first, ROBO_SVG_PATH_ATTRIBUTES,
                          static_cast<int>(sizeof(ROBO_SVG_PATH_ATTRIBUTES) / sizeof(ROBO_SVG_PATH_ATTRIBUTES[0])))) {
            diagnostic = RoboSvgFail("R7501", "Path attribute '" + entry.first + "' is unknown. Use d, fill, or stroke.");
            return false;
        }
    }
    path.data = RoboXMLAttr(node, "d");
    if (path.data.empty()) {
        path.data = RoboXMLAttr(node, "paths");
    }
    if (path.data.empty()) {
        diagnostic = RoboSvgFail("R7505", "Path needs d. Use a path command such as M1 1 H3 V3 Z.");
        return false;
    }
    const std::string fill = RoboXMLAttr(node, "fill");
    if (!fill.empty()) {
        if (!RoboSvgPaintValue(fill)) {
            diagnostic = RoboSvgFail("R7502", "Path fill '" + fill + "' is not a color. Use #RRGGBB or none.");
            return false;
        }
        path.fill = fill;
        path.hasFill = true;
    }
    const std::string stroke = RoboXMLAttr(node, "stroke");
    if (!stroke.empty()) {
        if (!RoboSvgPaintValue(stroke)) {
            diagnostic = RoboSvgFail("R7502", "Path stroke '" + stroke + "' is not a color. Use #RRGGBB or none.");
            return false;
        }
        path.stroke = stroke;
        path.hasStroke = true;
    }
    const std::string width = RoboXMLAttr(node, "strokeWidth");
    if (!width.empty()) {
        float number = 0.0f;
        if (!RoboSvgNumber(width, number) || number < 0.0f) {
            diagnostic = RoboSvgFail("R7503", "Path strokeWidth '" + width + "' is not a size. Use a number of pixels.");
            return false;
        }
        path.strokeWidth = number;
        path.hasStrokeWidth = true;
    }
    return true;
}

void RoboSvgScale(NSVGimage* picture, float scaleX, float scaleY) {
    for (NSVGshape* shape = picture->shapes; shape != nullptr; shape = shape->next) {
        for (NSVGpath* path = shape->paths; path != nullptr; path = path->next) {
            for (int point = 0; point < path->npts; ++point) {
                path->pts[point * 2] *= scaleX;
                path->pts[point * 2 + 1] *= scaleY;
            }
        }
    }
    picture->width *= scaleX;
    picture->height *= scaleY;
}

}  // namespace

bool RoboSvgRead(const RoboXMLNode& node, RoboSvgNode& svg, std::string& diagnostic) {
    svg = RoboSvgNode{};
    diagnostic.clear();
    if (RoboSvgLower(node.name) != "svg") {
        diagnostic = RoboSvgFail("R7504", "'" + node.name + "' is not an Svg. Use Svg.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!RoboSvgKnown(entry.first, ROBO_SVG_ATTRIBUTES,
                          static_cast<int>(sizeof(ROBO_SVG_ATTRIBUTES) / sizeof(ROBO_SVG_ATTRIBUTES[0])))) {
            diagnostic = RoboSvgFail("R7501", "Svg attribute '" + entry.first + "' is unknown. Use paths, fill, stroke, or viewBox.");
            return false;
        }
    }
    svg.width = ROBO_SVG_DEFAULT_SIZE;
    svg.height = ROBO_SVG_DEFAULT_SIZE;
    const std::string width = RoboXMLAttr(node, "width");
    if (!width.empty()) {
        int number = 0;
        if (!RoboSvgInteger(width, number) || number < 1 || number > ROBO_SVG_MAX_SIZE) {
            diagnostic = RoboSvgFail("R7503", "Svg width '" + width + "' is not a size. Use a whole number of pixels.");
            return false;
        }
        svg.width = number;
        svg.height = number;
    }
    const std::string height = RoboXMLAttr(node, "height");
    if (!height.empty()) {
        int number = 0;
        if (!RoboSvgInteger(height, number) || number < 1 || number > ROBO_SVG_MAX_SIZE) {
            diagnostic = RoboSvgFail("R7503", "Svg height '" + height + "' is not a size. Use a whole number of pixels.");
            return false;
        }
        svg.height = number;
    }
    const std::string fill = RoboXMLAttr(node, "fill");
    if (!fill.empty()) {
        if (!RoboSvgPaintValue(fill)) {
            diagnostic = RoboSvgFail("R7502", "Svg fill '" + fill + "' is not a color. Use #RRGGBB or none.");
            return false;
        }
        svg.fill = fill;
    }
    const std::string stroke = RoboXMLAttr(node, "stroke");
    if (!stroke.empty()) {
        if (!RoboSvgPaintValue(stroke)) {
            diagnostic = RoboSvgFail("R7502", "Svg stroke '" + stroke + "' is not a color. Use #RRGGBB or none.");
            return false;
        }
        svg.stroke = stroke;
    }
    const std::string strokeWidth = RoboXMLAttr(node, "strokeWidth");
    if (!strokeWidth.empty()) {
        if (!RoboSvgNumber(strokeWidth, svg.strokeWidth) || svg.strokeWidth < 0.0f) {
            diagnostic = RoboSvgFail("R7503", "Svg strokeWidth '" + strokeWidth + "' is not a size. Use a number of pixels.");
            return false;
        }
        svg.hasStrokeWidth = true;
    }
    const std::string viewBox = RoboXMLAttr(node, "viewBox");
    if (!viewBox.empty()) {
        if (!RoboSvgViewBox(viewBox, svg.viewX, svg.viewY, svg.viewWidth, svg.viewHeight)) {
            diagnostic = RoboSvgFail("R7502", "Svg viewBox '" + viewBox + "' is not a view box. Use four numbers, such as 0 0 24 24.");
            return false;
        }
    } else {
        svg.viewX = 0.0f;
        svg.viewY = 0.0f;
        svg.viewWidth = static_cast<float>(svg.width);
        svg.viewHeight = static_cast<float>(svg.height);
    }
    const std::string paths = RoboXMLAttr(node, "paths");
    const std::string data = RoboXMLAttr(node, "d");
    if (!paths.empty() || !data.empty()) {
        RoboSvgPath path;
        path.data = !paths.empty() ? paths : data;
        svg.paths.push_back(path);
    }
    if (static_cast<int>(node.children.size()) > ROBO_SVG_PATH_MAX) {
        diagnostic = RoboSvgFail("R7506", "Svg has more than 32 paths. Remove the extra paths.");
        return false;
    }
    for (const RoboXMLNode& child : node.children) {
        if (RoboSvgLower(child.name) != "path") {
            diagnostic = RoboSvgFail("R7504", "'" + child.name + "' is not a Path. Svg only contains Path.");
            return false;
        }
        RoboSvgPath path;
        if (!RoboSvgReadPath(child, path, diagnostic)) {
            return false;
        }
        svg.paths.push_back(path);
    }
    if (svg.paths.empty()) {
        diagnostic = RoboSvgFail("R7505", "Svg needs a path. Use paths or a Path child.");
        return false;
    }
    if (static_cast<int>(svg.paths.size()) > ROBO_SVG_PATH_MAX) {
        diagnostic = RoboSvgFail("R7506", "Svg has more than 32 paths. Remove the extra paths.");
        return false;
    }
    return true;
}

void RoboSvgPaint(const RoboSvgNode& svg, RoboSvgFrame& frame) {
    frame.width = svg.width > 0 ? svg.width : 0;
    frame.height = svg.height > 0 ? svg.height : 0;
    frame.pixels.assign(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height), 0);
    if (frame.width < 1 || frame.height < 1 || svg.paths.empty() || svg.viewWidth <= 0.0f || svg.viewHeight <= 0.0f) {
        return;
    }
    std::string text = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"";
    text += std::to_string(svg.viewWidth);
    text += "\" height=\"";
    text += std::to_string(svg.viewHeight);
    text += "\" viewBox=\"";
    text += std::to_string(svg.viewX);
    text += " ";
    text += std::to_string(svg.viewY);
    text += " ";
    text += std::to_string(svg.viewWidth);
    text += " ";
    text += std::to_string(svg.viewHeight);
    text += "\">";
    for (const RoboSvgPath& path : svg.paths) {
        const std::string fill = path.hasFill ? path.fill : svg.fill;
        const std::string stroke = path.hasStroke ? path.stroke : svg.stroke;
        const float strokeWidth = path.hasStrokeWidth ? path.strokeWidth : (svg.hasStrokeWidth ? svg.strokeWidth : 1.0f);
        text += "<path d=\"";
        text += RoboSvgEscape(path.data);
        text += "\" fill=\"";
        text += RoboSvgHexColor(fill.empty() ? std::string("#000000") : fill);
        text += "\"";
        if (!stroke.empty() && RoboSvgLower(stroke) != "none") {
            text += " stroke=\"";
            text += RoboSvgHexColor(stroke);
            text += "\" stroke-width=\"";
            text += std::to_string(strokeWidth);
            text += "\"";
        }
        text += "/>";
    }
    text += "</svg>";
    NSVGimage* parsed = nsvgParse(text.data(), "px", 96.0f);
    if (parsed == nullptr || parsed->width <= 0.0f || parsed->height <= 0.0f) {
        nsvgDelete(parsed);
        return;
    }
    RoboSvgScale(parsed, static_cast<float>(frame.width) / parsed->width, static_cast<float>(frame.height) / parsed->height);
    NSVGrasterizer* raster = nsvgCreateRasterizer();
    if (raster == nullptr) {
        nsvgDelete(parsed);
        return;
    }
    std::vector<RoboU8> rgba(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4u, 0);
    nsvgRasterize(raster, parsed, 0.0f, 0.0f, 1.0f, rgba.data(), frame.width, frame.height, frame.width * 4);
    nsvgDeleteRasterizer(raster);
    nsvgDelete(parsed);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const RoboU8* pixel = rgba.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)) * 4u;
            frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)] =
                (static_cast<RoboU32>(pixel[3]) << 24) | (static_cast<RoboU32>(pixel[0]) << 16) | (static_cast<RoboU32>(pixel[1]) << 8) | pixel[2];
        }
    }
}

bool RoboSvgPixel(const RoboSvgFrame& frame, int x, int y, RoboU32& color) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return false;
    }
    color = frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)];
    return true;
}
