#include "RoboIcon.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace {

constexpr int ROBO_ICON_DEFAULT_SIZE = 24;
constexpr int ROBO_ICON_MAX_SIZE = 512;
constexpr RoboU32 ROBO_ICON_DEFAULT_COLOR = 0xFFFFFFFFu;

const char* const ROBO_ICON_ATTRIBUTES[] = {
    "id", "name", "size", "color",
};

std::string RoboIconLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string RoboIconFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

bool RoboIconKnown(const std::string& key) {
    const std::string folded = RoboIconLower(key);
    const int count = static_cast<int>(sizeof(ROBO_ICON_ATTRIBUTES) / sizeof(ROBO_ICON_ATTRIBUTES[0]));
    for (int index = 0; index < count; ++index) {
        if (folded == ROBO_ICON_ATTRIBUTES[index]) {
            return true;
        }
    }
    return false;
}

int RoboIconHex(char ch) {
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

bool RoboIconColor(const std::string& text, RoboU32& color) {
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
        const int digit = RoboIconHex(ch);
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

bool RoboIconInteger(const std::string& text, int& number) {
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

bool RoboIconLayer(const RoboIconNode& icon, const RoboIconPackView& pack, const RoboU8*& coverage, int& width, int& height, int& stride, std::string& diagnostic) {
    coverage = nullptr;
    if (!RoboIconPackValidate(pack.bytes, pack.size) || pack.header == ROBO_NULL) {
        diagnostic = RoboIconFail("R7406", "Icon pack is not a cooked pack.");
        return false;
    }
    const RoboIconCookedEntry* entry = RoboIconPackFind(pack, icon.name.c_str());
    if (entry == ROBO_NULL || entry->layerCount < 1) {
        diagnostic = RoboIconFail("R7406", "Icon '" + icon.name + "' is not in the pack.");
        return false;
    }
    if (entry->layerOffset + sizeof(RoboIconCookedLayer) > pack.size) {
        diagnostic = RoboIconFail("R7406", "Icon '" + icon.name + "' has no glyph layer.");
        return false;
    }
    const RoboIconCookedLayer* layer = RoboIconPackLayers(pack, entry);
    if (layer == ROBO_NULL || layer->width < 1 || layer->height < 1 || layer->stride < layer->width) {
        diagnostic = RoboIconFail("R7406", "Icon '" + icon.name + "' has an empty glyph.");
        return false;
    }
    const RoboU64 bytes = static_cast<RoboU64>(layer->pixelOffset) + static_cast<RoboU64>(layer->height) * layer->stride;
    if (bytes > pack.size) {
        diagnostic = RoboIconFail("R7406", "Icon '" + icon.name + "' glyph is outside the pack.");
        return false;
    }
    coverage = pack.bytes + layer->pixelOffset;
    width = static_cast<int>(layer->width);
    height = static_cast<int>(layer->height);
    stride = static_cast<int>(layer->stride);
    return true;
}

RoboU8 RoboIconSample(const RoboU8* coverage, int width, int height, int stride, int size, int x, int y) {
    int sourceX = x * width / size;
    int sourceY = y * height / size;
    if (sourceX >= width) {
        sourceX = width - 1;
    }
    if (sourceY >= height) {
        sourceY = height - 1;
    }
    return coverage[static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(stride) + static_cast<std::size_t>(sourceX)];
}

}  // namespace

bool RoboIconRead(const RoboXMLNode& node, RoboIconNode& icon, std::string& diagnostic) {
    icon = RoboIconNode{};
    diagnostic.clear();
    if (RoboIconLower(node.name) != "icon") {
        diagnostic = RoboIconFail("R7404", "'" + node.name + "' is not an Icon. Use Icon.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!RoboIconKnown(entry.first)) {
            diagnostic = RoboIconFail("R7401", "Icon attribute '" + entry.first + "' is unknown. Use name, size, or color.");
            return false;
        }
    }
    if (!node.children.empty()) {
        diagnostic = RoboIconFail("R7404", "Icon does not take children.");
        return false;
    }
    icon.name = RoboXMLAttr(node, "name");
    if (icon.name.empty()) {
        diagnostic = RoboIconFail("R7405", "Icon needs a name. Use the glyph name from the pack.");
        return false;
    }
    icon.size = ROBO_ICON_DEFAULT_SIZE;
    const std::string size = RoboXMLAttr(node, "size");
    if (!size.empty()) {
        int number = 0;
        if (!RoboIconInteger(size, number) || number < 1 || number > ROBO_ICON_MAX_SIZE) {
            diagnostic = RoboIconFail("R7403", "Icon size '" + size + "' is not a size. Use a whole number of pixels.");
            return false;
        }
        icon.size = number;
    }
    icon.color = ROBO_ICON_DEFAULT_COLOR;
    const std::string color = RoboXMLAttr(node, "color");
    if (!color.empty() && !RoboIconColor(color, icon.color)) {
        diagnostic = RoboIconFail("R7402", "Icon color '" + color + "' is not a color. Use #RRGGBB.");
        return false;
    }
    return true;
}

bool RoboIconGlyph(const RoboIconNode& icon, const RoboIconPackView& pack, RoboIconBox& box, std::string& diagnostic) {
    box = RoboIconBox{};
    diagnostic.clear();
    const RoboU8* coverage = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
    if (!RoboIconLayer(icon, pack, coverage, width, height, stride, diagnostic)) {
        return false;
    }
    int minX = icon.size;
    int minY = icon.size;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < icon.size; ++y) {
        for (int x = 0; x < icon.size; ++x) {
            if (RoboIconSample(coverage, width, height, stride, icon.size, x, y) == 0) {
                continue;
            }
            if (x < minX) {
                minX = x;
            }
            if (y < minY) {
                minY = y;
            }
            if (x > maxX) {
                maxX = x;
            }
            if (y > maxY) {
                maxY = y;
            }
        }
    }
    if (maxX < minX || maxY < minY) {
        diagnostic = RoboIconFail("R7406", "Icon '" + icon.name + "' glyph is blank.");
        return false;
    }
    box.x = minX;
    box.y = minY;
    box.width = maxX - minX + 1;
    box.height = maxY - minY + 1;
    return true;
}

void RoboIconPaint(const RoboIconNode& icon, const RoboIconPackView& pack, RoboIconFrame& frame) {
    frame.width = icon.size > 0 ? icon.size : 0;
    frame.height = frame.width;
    frame.pixels.assign(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height), 0);
    std::string diagnostic;
    const RoboU8* coverage = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
    if (!RoboIconLayer(icon, pack, coverage, width, height, stride, diagnostic)) {
        return;
    }
    const RoboU32 color = icon.color == 0 ? ROBO_ICON_DEFAULT_COLOR : icon.color;
    const int alpha = static_cast<int>((color >> 24) & 0xFFu);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const int coverageAlpha = RoboIconSample(coverage, width, height, stride, icon.size, x, y);
            if (coverageAlpha == 0 || alpha == 0) {
                continue;
            }
            const int mixed = alpha * coverageAlpha / 255;
            frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)] =
                (static_cast<RoboU32>(mixed) << 24) | (color & 0x00FFFFFFu);
        }
    }
}

bool RoboIconPixel(const RoboIconFrame& frame, int x, int y, RoboU32& color) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return false;
    }
    color = frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)];
    return true;
}
