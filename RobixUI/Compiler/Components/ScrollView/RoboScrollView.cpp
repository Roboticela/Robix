#include "RoboScrollView.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace {

constexpr int ROBO_SCROLL_MAX_SIZE = 4096;

const char* const ROBO_SCROLL_ATTRIBUTES[] = {
    "id", "axis", "offset", "offsetx", "offsety", "width", "height", "contentwidth", "contentheight", "childcolor", "child-color",
};

std::string RoboScrollLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string RoboScrollFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

bool RoboScrollKnown(const std::string& key) {
    const std::string folded = RoboScrollLower(key);
    for (const char* name : ROBO_SCROLL_ATTRIBUTES) {
        if (folded == name) {
            return true;
        }
    }
    return false;
}

int RoboScrollHex(char ch) {
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

bool RoboScrollColor(const std::string& text, RoboU32& color) {
    if (text.size() != 7 || text[0] != '#') {
        return false;
    }
    RoboU32 value = 0;
    for (int index = 1; index <= 6; ++index) {
        const int digit = RoboScrollHex(text[static_cast<std::size_t>(index)]);
        if (digit < 0) {
            return false;
        }
        value = (value << 4) | static_cast<RoboU32>(digit);
    }
    color = 0xFF000000u | value;
    return true;
}

bool RoboScrollInteger(const std::string& text, int& number) {
    if (text.empty()) {
        return false;
    }
    int value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10 + (ch - '0');
        if (value > ROBO_SCROLL_MAX_SIZE) {
            return false;
        }
    }
    number = value;
    return true;
}

int RoboScrollLimit(int content, int viewport) {
    return std::max(0, content - viewport);
}

void RoboScrollPlace(RoboScrollViewNode& view) {
    const bool vertical = view.axis == "vertical" || view.axis == "both";
    const bool horizontal = view.axis == "horizontal" || view.axis == "both";
    view.verticalBar = vertical && view.contentHeight > view.height;
    view.horizontalBar = horizontal && view.contentWidth > view.width;
    const int maxX = horizontal ? RoboScrollLimit(view.contentWidth, view.width) : 0;
    const int maxY = vertical ? RoboScrollLimit(view.contentHeight, view.height) : 0;
    view.offsetX = std::max(0, std::min(view.offsetX, maxX));
    view.offsetY = std::max(0, std::min(view.offsetY, maxY));
    view.childOriginX = -view.offsetX;
    view.childOriginY = -view.offsetY;

    view.trackX = 0;
    view.trackY = 0;
    view.trackWidth = 0;
    view.trackHeight = 0;
    view.thumbX = 0;
    view.thumbY = 0;
    view.thumbWidth = 0;
    view.thumbHeight = 0;
    if (view.verticalBar) {
        const int trackHeight = view.height - (view.horizontalBar ? ROBO_SCROLL_BAR : 0);
        int thumbHeight = trackHeight * view.height / view.contentHeight;
        thumbHeight = std::max(8, thumbHeight);
        thumbHeight = std::min(thumbHeight, trackHeight);
        const int travel = trackHeight - thumbHeight;
        view.trackX = view.width - ROBO_SCROLL_BAR;
        view.trackY = 0;
        view.trackWidth = ROBO_SCROLL_BAR;
        view.trackHeight = trackHeight;
        view.thumbX = view.trackX;
        view.thumbWidth = ROBO_SCROLL_BAR;
        view.thumbHeight = thumbHeight;
        view.thumbY = maxY == 0 || travel == 0 ? 0 : view.offsetY * travel / maxY;
    } else if (view.horizontalBar) {
        const int trackWidth = view.width;
        int thumbWidth = trackWidth * view.width / view.contentWidth;
        thumbWidth = std::max(8, thumbWidth);
        thumbWidth = std::min(thumbWidth, trackWidth);
        const int travel = trackWidth - thumbWidth;
        view.trackX = 0;
        view.trackY = view.height - ROBO_SCROLL_BAR;
        view.trackWidth = trackWidth;
        view.trackHeight = ROBO_SCROLL_BAR;
        view.thumbY = view.trackY;
        view.thumbHeight = ROBO_SCROLL_BAR;
        view.thumbWidth = thumbWidth;
        view.thumbX = maxX == 0 || travel == 0 ? 0 : view.offsetX * travel / maxX;
    }
}

bool RoboScrollInside(int x, int y, int left, int top, int width, int height) {
    return x >= left && y >= top && x < left + width && y < top + height;
}

void RoboScrollFill(RoboScrollViewFrame& frame, int x, int y, int width, int height, RoboU32 color) {
    for (int row = y; row < y + height; ++row) {
        if (row < 0 || row >= frame.height) {
            continue;
        }
        for (int column = x; column < x + width; ++column) {
            if (column < 0 || column >= frame.width) {
                continue;
            }
            frame.pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(column)] = color;
        }
    }
}

}  // namespace

bool RoboScrollViewRead(const RoboXMLNode& node, RoboScrollViewNode& view, std::string& diagnostic) {
    view = RoboScrollViewNode{};
    diagnostic.clear();
    if (RoboScrollLower(node.name) != "scrollview") {
        diagnostic = RoboScrollFail("R8104", "'" + node.name + "' is not a ScrollView.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!RoboScrollKnown(entry.first)) {
            diagnostic = RoboScrollFail("R8101", "ScrollView attribute '" + entry.first + "' is unknown. Use axis and offset.");
            return false;
        }
    }
    const std::string axis = RoboXMLAttr(node, "axis");
    if (axis.empty()) {
        view.axis = "vertical";
    } else {
        view.axis = RoboScrollLower(axis);
        if (view.axis != "vertical" && view.axis != "horizontal" && view.axis != "both") {
            diagnostic = RoboScrollFail("R8102", "ScrollView axis '" + axis + "' is unknown. Use vertical, horizontal, or both.");
            return false;
        }
    }
    const std::string width = RoboXMLAttr(node, "width");
    const std::string height = RoboXMLAttr(node, "height");
    const std::string contentWidth = RoboXMLAttr(node, "contentwidth");
    const std::string contentHeight = RoboXMLAttr(node, "contentheight");
    if (!RoboScrollInteger(width, view.width) || view.width < 1) {
        diagnostic = RoboScrollFail("R8103", "ScrollView width '" + width + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    if (!RoboScrollInteger(height, view.height) || view.height < 1) {
        diagnostic = RoboScrollFail("R8103", "ScrollView height '" + height + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    if (!RoboScrollInteger(contentWidth, view.contentWidth) || view.contentWidth < 1) {
        diagnostic = RoboScrollFail("R8103", "ScrollView content width '" + contentWidth + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    if (!RoboScrollInteger(contentHeight, view.contentHeight) || view.contentHeight < 1) {
        diagnostic = RoboScrollFail("R8103", "ScrollView content height '" + contentHeight + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    const std::string offset = RoboXMLAttr(node, "offset");
    const std::string offsetX = RoboXMLAttr(node, "offsetx");
    const std::string offsetY = RoboXMLAttr(node, "offsety");
    int shared = 0;
    if (!offset.empty() && !RoboScrollInteger(offset, shared)) {
        diagnostic = RoboScrollFail("R8102", "ScrollView offset '" + offset + "' is not a whole number.");
        return false;
    }
    if (!offsetX.empty() && !RoboScrollInteger(offsetX, view.offsetX)) {
        diagnostic = RoboScrollFail("R8102", "ScrollView offset '" + offsetX + "' is not a whole number.");
        return false;
    }
    if (!offsetY.empty() && !RoboScrollInteger(offsetY, view.offsetY)) {
        diagnostic = RoboScrollFail("R8102", "ScrollView offset '" + offsetY + "' is not a whole number.");
        return false;
    }
    if (!offset.empty()) {
        if (view.axis == "horizontal") {
            view.offsetX = shared;
        } else if (view.axis == "vertical") {
            view.offsetY = shared;
        } else if (offsetY.empty()) {
            view.offsetY = shared;
        }
    }
    const std::string child = RoboXMLAttr(node, "childcolor");
    const std::string childValue = child.empty() ? RoboXMLAttr(node, "child-color") : child;
    if (!childValue.empty() && !RoboScrollColor(childValue, view.childColor)) {
        diagnostic = RoboScrollFail("R8102", "ScrollView child color '" + childValue + "' is not a color. Use #RRGGBB.");
        return false;
    }
    RoboScrollPlace(view);
    return true;
}

bool RoboScrollViewWheel(RoboScrollViewNode& view, int deltaX, int deltaY) {
    const int beforeX = view.offsetX;
    const int beforeY = view.offsetY;
    const bool vertical = view.axis == "vertical" || view.axis == "both";
    const bool horizontal = view.axis == "horizontal" || view.axis == "both";
    if (vertical) {
        view.offsetY += deltaY;
    }
    if (horizontal) {
        view.offsetX += deltaX;
    }
    RoboScrollPlace(view);
    return view.offsetX != beforeX || view.offsetY != beforeY;
}

bool RoboScrollViewDrag(RoboScrollViewNode& view, int fromX, int fromY, int toX, int toY) {
    RoboScrollPlace(view);
    const bool onThumb = RoboScrollInside(fromX, fromY, view.thumbX, view.thumbY, view.thumbWidth, view.thumbHeight);
    if (!onThumb) {
        return false;
    }
    const int beforeX = view.offsetX;
    const int beforeY = view.offsetY;
    if (view.verticalBar && view.thumbX == view.width - ROBO_SCROLL_BAR) {
        const int maxY = RoboScrollLimit(view.contentHeight, view.height);
        const int travel = view.trackHeight - view.thumbHeight;
        if (travel > 0 && maxY > 0) {
            const int next = std::max(0, std::min(view.thumbY + (toY - fromY), travel));
            view.offsetY = next * maxY / travel;
        }
    }
    if (view.horizontalBar && view.thumbY == view.height - ROBO_SCROLL_BAR) {
        const int maxX = RoboScrollLimit(view.contentWidth, view.width);
        const int travel = view.trackWidth - view.thumbWidth;
        if (travel > 0 && maxX > 0) {
            const int next = std::max(0, std::min(view.thumbX + (toX - fromX), travel));
            view.offsetX = next * maxX / travel;
        }
    }
    RoboScrollPlace(view);
    return view.offsetX != beforeX || view.offsetY != beforeY;
}

void RoboScrollViewPaint(const RoboScrollViewNode& view, RoboScrollViewFrame& frame) {
    RoboScrollViewNode placed = view;
    RoboScrollPlace(placed);
    frame = RoboScrollViewFrame{};
    frame.width = std::max(1, placed.width);
    frame.height = std::max(1, placed.height);
    frame.pixels.assign(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height), ROBO_SCROLL_PLATE);
    const int clipRight = placed.width - (placed.verticalBar ? ROBO_SCROLL_BAR : 0);
    const int clipBottom = placed.height - (placed.horizontalBar ? ROBO_SCROLL_BAR : 0);
    for (int y = 0; y < clipBottom; ++y) {
        for (int x = 0; x < clipRight; ++x) {
            const int localX = x - placed.childOriginX;
            const int localY = y - placed.childOriginY;
            if (localX < 0 || localY < 0 || localX >= placed.contentWidth || localY >= placed.contentHeight) {
                continue;
            }
            const RoboU32 color = localX < ROBO_SCROLL_MARKER_SIZE && localY < ROBO_SCROLL_MARKER_SIZE ? ROBO_SCROLL_MARKER : placed.childColor;
            frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)] = color;
        }
    }
    if (placed.verticalBar) {
        RoboScrollFill(frame, placed.trackX, placed.trackY, placed.trackWidth, placed.trackHeight, ROBO_SCROLL_TRACK);
        RoboScrollFill(frame, placed.thumbX, placed.thumbY, placed.thumbWidth, placed.thumbHeight, ROBO_SCROLL_THUMB);
    }
    if (placed.horizontalBar) {
        RoboScrollFill(frame, 0, placed.height - ROBO_SCROLL_BAR, placed.width - (placed.verticalBar ? ROBO_SCROLL_BAR : 0), ROBO_SCROLL_BAR, ROBO_SCROLL_TRACK);
        const int maxX = RoboScrollLimit(placed.contentWidth, placed.width);
        const int trackWidth = placed.width - (placed.verticalBar ? ROBO_SCROLL_BAR : 0);
        int thumbWidth = trackWidth * placed.width / std::max(1, placed.contentWidth);
        thumbWidth = std::max(8, std::min(thumbWidth, trackWidth));
        const int travel = trackWidth - thumbWidth;
        const int thumbX = maxX == 0 || travel == 0 ? 0 : placed.offsetX * travel / maxX;
        RoboScrollFill(frame, thumbX, placed.height - ROBO_SCROLL_BAR, thumbWidth, ROBO_SCROLL_BAR, ROBO_SCROLL_THUMB);
    }
}

bool RoboScrollViewPixel(const RoboScrollViewFrame& frame, int x, int y, RoboU32& color) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return false;
    }
    color = frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)];
    return true;
}
