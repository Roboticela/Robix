#include "RoboScreen.hpp"

#include "../../ComponentTier.hpp"
#include "../../Foundation/FoundationPlatform.hpp"

#include <cctype>
#include <cstdlib>

namespace {

constexpr int ROBO_SCREEN_DEFAULT_WIDTH = 640;
constexpr int ROBO_SCREEN_DEFAULT_HEIGHT = 400;
constexpr int ROBO_SCREEN_DEFAULT_MIN_WIDTH = 320;
constexpr int ROBO_SCREEN_DEFAULT_MIN_HEIGHT = 200;
constexpr int ROBO_SCREEN_DEFAULT_MAX_WIDTH = 1920;
constexpr int ROBO_SCREEN_DEFAULT_MAX_HEIGHT = 1080;

const char* const ROBO_SCREEN_ATTRIBUTES[] = {
    "id",
    "path",
    "route",
    "name",
    "title",
    "theme",
    "appearance",
    "transparent",
    "transparency",
    "place",
    "x",
    "y",
    "width",
    "height",
    "minwidth",
    "minheight",
    "maxwidth",
    "maxheight",
    "minsize",
    "maxsize",
    "radius",
    "foundation",
    "contentwidth",
    "gutter",
    "border",
    "overflow",
    "overflowx",
    "overflow-x",
    "overflowy",
    "overflow-y",
    "clip",
    "background",
    "backgroundcolor",
    "bg",
    "bordercolor",
    "display",
    "direction",
    "flexdirection",
    "justify",
    "justifycontent",
    "align",
    "alignitems",
    "gap",
    "padding",
    "columns",
    "rows",
    "wrap",
    "flexwrap",
    "aligncontent",
    "justifyitems",
    "position",
    "inset",
    "left",
    "top",
    "right",
    "bottom",
    "margin",
    "alignself",
    "order",
    "gridcolumn",
    "gridrow",
    "grow",
    "flex",
    "flexgrow",
    "state",
    "resizable",
    "resize",
    "noresize",
    "resizedisabled",
    "resizeleft",
    "resizeright",
    "resizetop",
    "resizebottom",
    "resizetopleft",
    "resizetopright",
    "resizebottomleft",
    "resizebottomright",
    "movable",
    "topmost",
    "alwaysontop",
    "fullscreen",
    "zoomcontrols",
    "zoom-controls",
    "pinch",
    "pinchzoom",
    "pinch-zoom",
    "zoom",
    "viewportzoom",
    "viewport-zoom",
    "board",
};

std::string RoboScreenLower(std::string value) {
    for (char& ch : value) {
        const unsigned char letter = static_cast<unsigned char>(ch);
        ch = static_cast<char>(std::tolower(letter));
    }
    return value;
}

bool RoboScreenIsScreenTag(const std::string& name) {
    const char* base = RoboCompilerComponentBase(name);
    return base != nullptr && std::string(base) == "Screen";
}

bool RoboScreenAttributeKnown(const std::string& key) {
    const std::string folded = RoboScreenLower(key);
    for (const char* name : ROBO_SCREEN_ATTRIBUTES) {
        if (folded == name) {
            return true;
        }
    }
    return false;
}

std::string RoboScreenFirst(const RoboXMLNode& node, const char* key, const char* alias = nullptr) {
    const std::string value = RoboXMLAttr(node, key);
    if (!value.empty() || alias == nullptr) {
        return value;
    }
    return RoboXMLAttr(node, alias);
}

bool RoboScreenOneOf(const std::string& value, const char* const* choices, int count) {
    const std::string folded = RoboScreenLower(value);
    for (int index = 0; index < count; ++index) {
        if (folded == choices[index]) {
            return true;
        }
    }
    return false;
}

bool RoboScreenInteger(const std::string& text, int& number) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || end == nullptr || *end != '\0') {
        return false;
    }
    number = static_cast<int>(parsed);
    return true;
}

bool RoboScreenPair(const std::string& text, int& first, int& second) {
    std::string left;
    std::string right;
    bool split = false;
    for (char ch : text) {
        if (ch == ' ' || ch == ',' || ch == 'x' || ch == 'X') {
            if (!left.empty()) {
                split = true;
            }
            continue;
        }
        if (!split) {
            left.push_back(ch);
        } else {
            right.push_back(ch);
        }
    }
    if (!RoboScreenInteger(left, first)) {
        return false;
    }
    if (right.empty()) {
        second = first;
        return true;
    }
    return RoboScreenInteger(right, second);
}

int RoboScreenHex(char ch) {
    const unsigned char letter = static_cast<unsigned char>(ch);
    if (letter >= '0' && letter <= '9') {
        return letter - '0';
    }
    if (letter >= 'a' && letter <= 'f') {
        return letter - 'a' + 10;
    }
    if (letter >= 'A' && letter <= 'F') {
        return letter - 'A' + 10;
    }
    return -1;
}

bool RoboScreenColor(const std::string& text, RoboU32& color) {
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
        const int digit = RoboScreenHex(ch);
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

std::string RoboScreenFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

bool RoboScreenReadNumber(const RoboXMLNode& node, const char* key, int fallback, int& number, std::string& diagnostic) {
    const std::string text = RoboXMLAttr(node, key);
    if (text.empty()) {
        number = fallback;
        return true;
    }
    if (!RoboScreenInteger(text, number)) {
        diagnostic = RoboScreenFail("R7003", std::string("Screen ") + key + " '" + text + "' is not an integer. Use a whole number.");
        return false;
    }
    return true;
}

}  // namespace

const RoboXMLNode* RoboScreenFind(const RoboXMLNode& node) {
    if (RoboScreenIsScreenTag(node.name)) {
        return &node;
    }
    for (const RoboXMLNode& child : node.children) {
        if (const RoboXMLNode* found = RoboScreenFind(child)) {
            return found;
        }
    }
    return nullptr;
}

bool RoboScreenRead(const RoboXMLNode& node, RoboScreenRecord& screen, std::string& diagnostic) {
    diagnostic.clear();
    if (!RoboScreenIsScreenTag(node.name)) {
        diagnostic = RoboScreenFail("R7004", "'" + node.name + "' is not a Screen. Use Screen, Window, or Page.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!RoboScreenAttributeKnown(entry.first)) {
            diagnostic = RoboScreenFail("R7001", "Screen attribute '" + entry.first + "' is unknown. Remove it or use a Screen attribute.");
            return false;
        }
    }

    screen = RoboScreenRecord{};
    screen.tag = "Screen";
    const std::string title = RoboScreenFirst(node, "title", "name");
    screen.title = title;
    screen.path = RoboScreenFirst(node, "path", "route");
    if (screen.path.empty()) {
        screen.path = "/";
    }

    std::string theme = RoboScreenFirst(node, "theme", "appearance");
    if (theme.empty()) {
        theme = "dark";
    }
    theme = RoboScreenLower(theme);
    const char* themes[] = {"dark", "light"};
    if (!RoboScreenOneOf(theme, themes, 2)) {
        diagnostic = RoboScreenFail("R7002", "Screen theme '" + theme + "' is unknown. Use dark or light.");
        return false;
    }
    screen.theme = theme;

    std::string place = RoboXMLAttr(node, "place");
    if (place.empty()) {
        place = "center";
    }
    place = RoboScreenLower(place);
    const char* places[] = {"center", "top", "bottom", "left", "right", "top-left", "top-right", "bottom-left", "bottom-right", "absolute"};
    if (!RoboScreenOneOf(place, places, 10)) {
        diagnostic = RoboScreenFail("R7002", "Screen place '" + place + "' is unknown. Use center, an edge, a corner, or absolute.");
        return false;
    }
    screen.place = place;

    std::string state = RoboXMLAttr(node, "state");
    if (state.empty()) {
        state = "normal";
    }
    state = RoboScreenLower(state);
    const char* states[] = {"normal", "minimized", "maximized", "closed"};
    if (!RoboScreenOneOf(state, states, 4)) {
        diagnostic = RoboScreenFail("R7002", "Screen state '" + state + "' is unknown. Use normal, minimized, maximized, or closed.");
        return false;
    }
    screen.state = state;

    const std::string minSize = RoboXMLAttr(node, "minSize");
    if (!minSize.empty() && !RoboScreenPair(minSize, screen.minWidth, screen.minHeight)) {
        diagnostic = RoboScreenFail("R7003", "Screen minSize '" + minSize + "' is not a size. Use one number or two numbers.");
        return false;
    }
    const std::string maxSize = RoboXMLAttr(node, "maxSize");
    if (!maxSize.empty() && !RoboScreenPair(maxSize, screen.maxWidth, screen.maxHeight)) {
        diagnostic = RoboScreenFail("R7003", "Screen maxSize '" + maxSize + "' is not a size. Use one number or two numbers.");
        return false;
    }
    if (minSize.empty()) {
        screen.minWidth = ROBO_SCREEN_DEFAULT_MIN_WIDTH;
        screen.minHeight = ROBO_SCREEN_DEFAULT_MIN_HEIGHT;
    }
    if (maxSize.empty()) {
        screen.maxWidth = ROBO_SCREEN_DEFAULT_MAX_WIDTH;
        screen.maxHeight = ROBO_SCREEN_DEFAULT_MAX_HEIGHT;
    }
    if (!RoboScreenReadNumber(node, "minWidth", screen.minWidth, screen.minWidth, diagnostic) ||
        !RoboScreenReadNumber(node, "minHeight", screen.minHeight, screen.minHeight, diagnostic) ||
        !RoboScreenReadNumber(node, "maxWidth", screen.maxWidth, screen.maxWidth, diagnostic) ||
        !RoboScreenReadNumber(node, "maxHeight", screen.maxHeight, screen.maxHeight, diagnostic)) {
        return false;
    }
    if (screen.minWidth > screen.maxWidth || screen.minHeight > screen.maxHeight) {
        diagnostic = RoboScreenFail("R7005", "Screen min is larger than max. Lower the minimum or raise the maximum.");
        return false;
    }
    if (!RoboScreenReadNumber(node, "width", ROBO_SCREEN_DEFAULT_WIDTH, screen.width, diagnostic) ||
        !RoboScreenReadNumber(node, "height", ROBO_SCREEN_DEFAULT_HEIGHT, screen.height, diagnostic) ||
        !RoboScreenReadNumber(node, "x", 0, screen.x, diagnostic) ||
        !RoboScreenReadNumber(node, "y", 0, screen.y, diagnostic)) {
        return false;
    }
    if (screen.width < 0 || screen.height < 0) {
        diagnostic = RoboScreenFail("R7003", "Screen size is negative. Use a positive width and height.");
        return false;
    }
    if (screen.width < screen.minWidth) {
        screen.width = screen.minWidth;
    }
    if (screen.height < screen.minHeight) {
        screen.height = screen.minHeight;
    }
    if (screen.width > screen.maxWidth) {
        screen.width = screen.maxWidth;
    }
    if (screen.height > screen.maxHeight) {
        screen.height = screen.maxHeight;
    }

    const std::string transparent = RoboScreenFirst(node, "transparent", "transparency");
    const std::string transparentFolded = RoboScreenLower(transparent);
    screen.transparent = transparentFolded == "true" || transparentFolded == "1" || transparentFolded == "yes";

    const std::string movable = RoboXMLAttr(node, "movable");
    if (!movable.empty()) {
        const std::string folded = RoboScreenLower(movable);
        screen.movable = !(folded == "false" || folded == "0" || folded == "no" || folded == "off");
    }
    const std::string resizable = RoboXMLAttr(node, "resizable");
    if (!resizable.empty()) {
        const std::string folded = RoboScreenLower(resizable);
        screen.resizable = !(folded == "false" || folded == "0" || folded == "no" || folded == "off");
    }

    const std::string background = RoboXMLAttr(node, "background");
    std::string backgroundValue = background;
    if (backgroundValue.empty()) {
        backgroundValue = RoboXMLAttr(node, "backgroundColor");
    }
    if (backgroundValue.empty()) {
        backgroundValue = RoboXMLAttr(node, "bg");
    }
    if (screen.transparent) {
        screen.color = 0;
    } else if (!backgroundValue.empty()) {
        if (!RoboScreenColor(backgroundValue, screen.color)) {
            diagnostic = RoboScreenFail("R7002", "Screen background '" + backgroundValue + "' is not a color. Use #RRGGBB.");
            return false;
        }
    } else {
        screen.color = RoboFoundationPlatformColorForTheme("bg-primary", screen.theme == "light" ? ROBO_TRUE : ROBO_FALSE);
    }
    return true;
}

void RoboScreenPaint(const RoboScreenRecord& screen, RoboScreenFrame& frame) {
    frame.width = screen.width;
    frame.height = screen.height;
    frame.pixels.assign(static_cast<std::size_t>(screen.width) * static_cast<std::size_t>(screen.height), screen.color);
}

bool RoboScreenPixel(const RoboScreenFrame& frame, int x, int y, RoboU32& color) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return false;
    }
    color = frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)];
    return true;
}
