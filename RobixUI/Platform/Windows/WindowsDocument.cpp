#include "WindowsDocument.hpp"
#include "../ViewZoom.hpp"
#include "WindowsIcon.hpp"

#include "../../Compiler/RoboCompilerShared.hpp"
#include "../../Compiler/RoboCompilerWebShared.hpp"

#include "../../Compiler/Foundation/FoundationPlatform.hpp"
#include "../../Compiler/Compose.hpp"
#include "../../Compiler/RoboXML.hpp"
#include "../../RoboGPU/RoboGPU.hpp"
#include "../../RoboGPU/RoboText.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

void RoboWindowsInstallFontRaster(const RoboU8* pack, RoboSize packBytes);

namespace Robix::UI {
namespace {

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
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void LowerKeys(RoboXMLNode& node) {
    std::vector<std::pair<std::string, std::string>> extra;
    for (const auto& entry : node.attrs) {
        const std::string key = Lower(entry.first);
        if (node.attrs.find(key) == node.attrs.end()) {
            extra.emplace_back(key, entry.second);
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

unsigned gDpi = 96;
std::filesystem::path gAssets;

std::filesystem::path FindAssets(const std::string& document) {
    auto climb = [](std::filesystem::path dir) {
        for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
            std::error_code error;
            if (std::filesystem::exists(dir / "Assets" / "Icons.rpack", error) ||
                std::filesystem::exists(dir / "Assets" / "Icons", error)) {
                return dir / "Assets";
            }
            if (!dir.has_parent_path() || dir.parent_path() == dir) {
                break;
            }
            dir = dir.parent_path();
        }
        return std::filesystem::path{};
    };
    std::filesystem::path found;
    if (!document.empty()) {
        found = climb(std::filesystem::path(document).parent_path());
    }
    if (found.empty()) {
        std::error_code error;
        found = climb(std::filesystem::current_path(error));
    }
    return found;
}

std::string TrimCopy(std::string text) {
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(start, end - start);
}

bool ButtonOn(const RoboXMLNode& node, const char* key);

std::string ButtonSpinner() {
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 32 32\" fill=\"none\">"
           "<circle cx=\"16\" cy=\"16\" r=\"14\" stroke=\"currentColor\" stroke-opacity=\"0.35\" stroke-width=\"2\"/>"
           "<circle cx=\"16\" cy=\"16\" r=\"14\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" "
           "stroke-dasharray=\"24.63 63.33\"/></svg>";
}

std::string ButtonSlot(const RoboXMLNode& node) {
    std::string slot = RoboXMLAttr(node, "slot");
    if (slot.empty()) {
        slot = RoboXMLAttr(node, "side");
    }
    if (slot.empty()) {
        slot = RoboXMLAttr(node, "place");
    }
    return Lower(slot);
}

std::string TextCopy(const RoboXMLNode& node) {
    const std::string body = TrimCopy(node.text);
    if (!body.empty()) {
        return body;
    }
    const char* keys[] = {"value", "content", "text", "label"};
    for (const char* key : keys) {
        const std::string value = TrimCopy(RoboXMLAttr(node, key));
        if (!value.empty()) {
            return value;
        }
    }
    return {};
}

std::string ButtonLabel(const RoboXMLNode& node) {
    if (ButtonOn(node, "loading")) {
        const std::string loading = RoboXMLAttr(node, "loadingtext");
        return loading.empty() ? "Loading..." : loading;
    }
    return TextCopy(node);
}

void ResolveButtonIcons(const RoboXMLNode& node, std::string& leading, std::string& trailing) {
    leading.clear();
    trailing.clear();
    if (ButtonOn(node, "loading")) {
        leading = ButtonSpinner();
        return;
    }
    leading = RoboWebSvg(gAssets, RoboXMLAttr(node, "leading"));
    trailing = RoboWebSvg(gAssets, RoboXMLAttr(node, "trailing"));
    for (const RoboXMLNode& child : node.children) {
        const std::string childName = Lower(child.name);
        if (childName != "icon" && childName != "svg") {
            continue;
        }
        const std::string icon = RoboWebIconMarkup(gAssets, child);
        if (icon.empty()) {
            continue;
        }
        const std::string slot = ButtonSlot(child);
        if (slot == "trailing" || slot == "right" || slot == "end") {
            trailing = icon;
        } else if (slot == "leading" || slot == "left" || slot == "start" || leading.empty()) {
            leading = icon;
        } else {
            trailing = icon;
        }
    }
}

int Px(int value) {
    if (value == 0 || gDpi == 96) {
        return value;
    }
    const int scaled = static_cast<int>((static_cast<long long>(value) * static_cast<long long>(gDpi) + 48) / 96);
    if (scaled == 0) {
        return value < 0 ? -1 : 1;
    }
    return scaled;
}

bool IsDigits(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return true;
}

int LengthOf(const std::string& raw) {
    if (raw.empty()) {
        return 0;
    }
    std::size_t begin = 0;
    int sign = 1;
    if (raw[0] == '+') {
        begin = 1;
    } else if (raw[0] == '-') {
        sign = -1;
        begin = 1;
    }
    const std::string body = raw.substr(begin);
    if (IsDigits(body)) {
        const int magnitude = Px(std::stoi(body));
        return sign < 0 ? -magnitude : magnitude;
    }
    if (sign < 0) {
        return 0;
    }
    const RoboI32 space = RoboFoundationPlatformSpace(raw.c_str());
    if (space > 0) {
        return Px(space);
    }
    const RoboI32 radius = RoboFoundationPlatformRadius(raw.c_str());
    return Px(radius > 0 ? radius : 0);
}

int MeasureLength(const std::string& raw, int basis) {
    if (raw.size() > 1 && raw.back() == '%') {
        const std::string number = raw.substr(0, raw.size() - 1);
        std::size_t begin = 0;
        int sign = 1;
        if (!number.empty() && number[0] == '+') {
            begin = 1;
        } else if (!number.empty() && number[0] == '-') {
            sign = -1;
            begin = 1;
        }
        const std::string digits = number.substr(begin);
        if (basis > 0 && IsDigits(digits)) {
            return sign * basis * std::stoi(digits) / 100;
        }
    }
    return LengthOf(raw);
}

int AttrLength(const RoboXMLNode& node, const char* key, int fallback = 0) {
    const std::string value = RoboXMLAttr(node, key);
    return value.empty() ? fallback : LengthOf(value);
}

bool IsText(const std::string& name) {
    return name == "text" || name == "heading" || name == "subhead" || name == "body" || name == "paragraph" || name == "p" ||
           name == "caption" || name == "eyebrow" || name == "label" || name == "span" || name == "h1" || name == "h2" ||
           name == "h3" || name == "h4" || name == "h5" || name == "h6";
}

bool IsBox(const std::string& name) {
    return name == "screen" || name == "container" || name == "board" || name == "box" || name == "flex" || name == "stack" ||
           name == "canvas";
}

int FaceOf(const std::string& font) {
    const std::string key = Lower(font);
    if (key.find("semibold") != std::string::npos) {
        return 2;
    }
    if (key.find("medium") != std::string::npos) {
        return 1;
    }
    if (key.find("bold") != std::string::npos) {
        return 3;
    }
    if (key.find("italic") != std::string::npos) {
        return 4;
    }
    return 0;
}

struct TypeStyle {
    int size = 17;
    int leading = 24;
    int face = 0;
    const char* color = "text-primary";
};

TypeStyle StyleOf(const RoboXMLNode& node) {
    const std::string name = RoboUIFace(node);
    std::string role = "body-lg";
    const char* color = "text-secondary";
    if (name == "heading" || name == "h1" || name == "h2" || name == "h3" || name == "h4" || name == "h5" || name == "h6") {
        std::string level = Lower(RoboXMLAttr(node, "level", "1"));
        if (name.size() == 2 && name[0] == 'h') {
            level = name.substr(1);
        }
        if (level == "display") {
            role = "display";
        } else if (level == "2") {
            role = "h2";
        } else if (level == "3") {
            role = "h3";
        } else if (level == "4" || level == "5" || level == "6") {
            role = "h1";
        } else {
            role = "h1";
        }
        color = "text-primary";
    } else if (name == "subhead") {
        role = "subhead";
    } else if (name == "caption") {
        role = "caption";
        color = "text-tertiary";
    } else if (name == "eyebrow") {
        role = "eyebrow";
        color = "text-tertiary";
    } else if (name == "label") {
        role = "button";
        color = "text-primary";
    } else if (name == "body") {
        const std::string size = Lower(RoboXMLAttr(node, "size"));
        role = (size == "sm" || size == "small") ? "body" : "body-lg";
    }
    TypeStyle style;
    style.color = color;
    style.face = FaceOf(RoboXMLAttr(node, "font"));
    RoboFoundationTypeMetrics metrics{};
    if (RoboFoundationPlatformType(role.c_str(), &metrics) == ROBO_TRUE) {
        style.size = metrics.sizePx;
        style.leading = std::max(style.size, style.size * metrics.leadingMilli / 1000);
    }
    if (name == "heading" || (name.size() == 2 && name[0] == 'h')) {
        const std::string level = name.size() == 2 ? name.substr(1) : Lower(RoboXMLAttr(node, "level", "1"));
        if (level == "4") {
            style.size = 22;
        } else if (level == "5") {
            style.size = 19;
        } else if (level == "6") {
            style.size = 17;
        }
        style.leading = style.size + style.size / 4;
        if (style.face == 0) {
            style.face = 2;
        }
    }
    style.size = Px(style.size);
    style.leading = Px(style.leading);
    const std::string fontSize = RoboXMLAttr(node, "fontsize");
    int forced = fontSize.empty() ? 0 : LengthOf(fontSize);
    if (forced <= 0) {
        const std::string sizeName = RoboXMLAttr(node, "size");
        if (!sizeName.empty() && sizeName.find_first_not_of("0123456789") == std::string::npos) {
            forced = LengthOf(sizeName);
        }
    }
    if (forced > 0) {
        style.size = forced;
        style.leading = forced + std::max(1, forced / 4);
    }
    return style;
}

RoboFontView FaceFont(int face, int size, RoboFontHeader& header) {
    header = {};
    header.magic = ROBO_FONT_MAGIC;
    header.ascent = static_cast<RoboU16>(size);
    header.glyphCount = 0;
    header.firstCodepoint = static_cast<RoboU16>(face);
    RoboFontView font{};
    font.header = &header;
    return font;
}

int MeasureText(const std::string& text, int size, int face) {
    RoboFontHeader header{};
    const RoboFontView font = FaceFont(face, size, header);
    return RoboTextMeasureUTF8(font, text.c_str(), static_cast<RoboU16>(size));
}

struct PaintOp {
    bool text = false;
    bool stroke = false;
    bool button = false;
    bool focus = false;
    bool drawing = false;
    int borderPx = 0;
    int fade = 255;
    RoboU32 border = 0;
    std::string drawingSvg;
    std::string leadingSvg;
    std::string trailingSvg;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int radius = 0;
    RoboU32 fill = 0;
    RoboU32 color = 0;
    int size = 0;
    int face = 0;
    int align = 0;
    int wrap = 1;
    int maxLines = 0;
    int overflow = 0;
    int whiteSpace = 1;
    bool breakLong = true;
    std::string value;
    int control = -1;
    int kind = 0;
    int preview = 0;
    bool quiet = false;
    bool iconButton = false;
    bool loading = false;
};

const char* ButtonFill(const std::string& variant) {
    if (variant == "secondary" || variant == "ghost" || variant == "action") {
        return nullptr;
    }
    if (variant == "danger" || variant == "destructive") {
        return "danger";
    }
    return "brand-primary";
}

const char* ButtonInk(const std::string& variant) {
    if (variant == "secondary") {
        return "brand-primary";
    }
    if (variant == "ghost") {
        return "text-tertiary";
    }
    if (variant == "action") {
        return "text-primary";
    }
    return "text-on-brand";
}

int ButtonHeight(const RoboXMLNode& node) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    if (size == "sm" || size == "small") {
        return Px(32);
    }
    if (size == "lg" || size == "large") {
        return Px(50);
    }
    return Px(44);
}

int ButtonPad(const RoboXMLNode& node) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    if (size == "sm" || size == "small") {
        return Px(14);
    }
    if (size == "lg" || size == "large") {
        return Px(24);
    }
    return Px(22);
}

int ButtonFontSize(const RoboXMLNode& node) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    return Px(size == "sm" || size == "small" ? 14 : 17);
}

bool ButtonOn(const RoboXMLNode& node, const char* key) {
    const std::string value = Lower(RoboXMLAttr(node, key));
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

void FillRound(RoboGPU* gpu, int x, int y, int width, int height, int radius, RoboU32 color) {
    if (width <= 0 || height <= 0) {
        return;
    }
    radius = std::max(0, std::min(radius, std::min(width, height) / 2));
    if (radius <= 1) {
        RoboGPUFillRect(gpu, x, y, width, height, color);
        return;
    }
    RoboGPUFillRect(gpu, x + radius, y, width - radius * 2, height, color);
    RoboGPUFillRect(gpu, x, y + radius, radius, height - radius * 2, color);
    RoboGPUFillRect(gpu, x + width - radius, y + radius, radius, height - radius * 2, color);
    for (int row = 0; row < radius; ++row) {
        int inside = 0;
        const int dy = radius - row;
        while (inside * inside + dy * dy <= radius * radius) {
            ++inside;
        }
        inside = std::max(0, inside - 1);
        const int cap = radius - inside;
        if (cap > 0) {
            RoboGPUFillRect(gpu, x + cap, y + row, radius - cap, 1, color);
            RoboGPUFillRect(gpu, x + width - radius, y + row, radius - cap, 1, color);
            RoboGPUFillRect(gpu, x + cap, y + height - 1 - row, radius - cap, 1, color);
            RoboGPUFillRect(gpu, x + width - radius, y + height - 1 - row, radius - cap, 1, color);
        }
    }
}

int Padding(const RoboXMLNode& node) {
    if (!RoboXMLAttr(node, "padding").empty()) {
        return AttrLength(node, "padding");
    }
    return AttrLength(node, "paddingx");
}

int Gap(const RoboXMLNode& node) {
    if (!RoboXMLAttr(node, "gap").empty()) {
        return AttrLength(node, "gap");
    }
    return AttrLength(node, "spacing");
}

bool RowDirection(const RoboXMLNode& node) {
    const std::string display = Lower(RoboXMLAttr(node, "display"));
    const std::string direction = Lower(RoboXMLAttr(node, "direction"));
    return display == "horizontal" || direction == "row" || direction == "row-reverse";
}

struct HitState {
    std::vector<WindowControl>* controls = nullptr;
    int hot = -1;
    int pressed = -1;
    int next = 0;
    int pointerX = -1;
    int pointerY = -1;
    bool pointerTaken = false;
};

int PlaceNode(const RoboXMLNode& node, int x, int y, int width, bool light, std::vector<PaintOp>& ops, HitState* hits);

int IconBox(const RoboXMLNode& node) {
    int size = AttrLength(node, "size", 0);
    if (size <= 0) {
        size = AttrLength(node, "width", 0);
    }
    return size <= 0 ? Px(24) : size;
}

std::string IconDrawing(const RoboXMLNode& node) {
    const std::string name = RoboUIFace(node);
    if (name == "svg") {
        std::string svg;
        RoboCompilerSerializeSvgNode(node, svg, true);
        return svg;
    }
    std::string icon = RoboXMLAttr(node, "name");
    if (icon.empty()) {
        icon = RoboXMLAttr(node, "icon");
    }
    if (icon.empty()) {
        icon = RoboXMLAttr(node, "src");
    }
    if (icon.empty() && name != "icon") {
        icon = node.name;
    }
    return RoboWebSvg(gAssets, icon);
}

int IconButtonExtent(const RoboXMLNode& node) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    if (size == "xs" || size == "extra-small") {
        return Px(28);
    }
    if (size == "sm" || size == "small") {
        return Px(32);
    }
    if (size == "lg" || size == "large") {
        return Px(48);
    }
    if (size == "xl" || size == "extra-large") {
        return Px(56);
    }
    if (size == "2xl") {
        return Px(64);
    }
    if (size == "3xl") {
        return Px(72);
    }
    return Px(44);
}

std::string IconButtonMark(bool success) {
    const char* path = success ? "M5.5 12.5L10 17L18.5 7.5" : "M6 6L18 18M18 6L6 18";
    std::string svg = "<svg viewBox=\"0 0 24 24\" fill=\"none\"><path d=\"";
    svg += path;
    svg += "\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\"";
    if (success) {
        svg += " stroke-linejoin=\"round\"";
    }
    svg += "></path></svg>";
    return svg;
}

RoboU32 ThemeColor(const char* name, bool light);

void StyleIconButton(PaintOp& op, bool light, bool hot, bool pressed, bool focused) {
    const bool disabled = op.quiet;
    int look = 0;
    if (!disabled && pressed) {
        look = 3;
    } else if (!disabled && hot) {
        look = 1;
    }
    op.fade = disabled ? 140 : 255;
    op.face = 1;
    op.borderPx = 0;
    op.fill = 0;
    op.border = 0;
    op.focus = focused && !disabled;
    const RoboU32 ink = ThemeColor("text-primary", light);
    const RoboU32 onBrand = ThemeColor("text-on-brand", light);
    if (op.kind == 11) {
        op.fill = ThemeColor(look == 3 ? "bg-active" : look == 1 ? "bg-hover" : "bg-secondary", light);
        op.color = ink;
    } else if (op.kind == 12) {
        op.border = ThemeColor(look == 3 ? "text-primary" : look == 1 ? "border-strong" : "border-default", light);
        op.borderPx = std::max(1, Px(1));
        op.color = ink;
        if (look == 3) {
            op.fill = ThemeColor("bg-active", light);
        } else if (look == 1) {
            op.fill = ThemeColor("bg-hover", light);
        }
    } else if (op.kind == 13) {
        const char* token = look == 3 ? "brand-primary-active" : look == 1 ? "brand-primary-hover" : "brand-primary";
        op.fill = ThemeColor(token, light);
        op.color = onBrand;
    } else if (op.kind == 14) {
        op.fill = ThemeColor(look == 1 ? "danger-hover" : "danger", light);
        op.color = onBrand;
        if (look == 3) {
            const RoboU32 red = ((op.fill >> 16) & 0xFFu) * 78u / 100u;
            const RoboU32 green = ((op.fill >> 8) & 0xFFu) * 78u / 100u;
            const RoboU32 blue = (op.fill & 0xFFu) * 78u / 100u;
            op.fill = 0xFF000000u | (red << 16) | (green << 8) | blue;
        }
    } else {
        op.color = ink;
        if (look == 3) {
            op.fill = ThemeColor("bg-active", light);
        } else if (look == 1 || op.focus) {
            op.fill = ThemeColor("bg-hover", light);
        }
    }
    if (op.kind == 11 && op.focus && look != 3) {
        op.fill = ThemeColor("bg-hover", light);
    } else if (op.kind == 12 && op.focus && look != 3) {
        op.border = ThemeColor("border-focus", light);
        op.borderPx = std::max(1, Px(1));
        if (look != 1) {
            op.fill = 0;
        }
    } else if (op.kind == 13 && op.focus && look != 3) {
        op.fill = ThemeColor("brand-primary-hover", light);
    } else if (op.kind == 14 && op.focus && look != 3) {
        op.fill = ThemeColor("danger-hover", light);
    }
    if (op.preview == 2 && op.kind != 13 && op.kind != 14) {
        op.color = ThemeColor("success", light);
    } else if (op.preview == 3 && op.kind != 13 && op.kind != 14) {
        op.color = ThemeColor("danger", light);
    }
}

void SwitchMetrics(const RoboXMLNode& node, int& trackW, int& trackH, int& thumb) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    if (size == "sm" || size == "small") {
        trackW = Px(44);
        trackH = Px(26);
        thumb = Px(22);
    } else if (size == "lg" || size == "large") {
        trackW = Px(60);
        trackH = Px(36);
        thumb = Px(32);
    } else {
        trackW = Px(51);
        trackH = Px(31);
        thumb = Px(27);
    }
}

void PaintSwitchControl(std::vector<PaintOp>& ops, const RoboXMLNode& node, int x, int y, bool light) {
    int trackW = 0;
    int trackH = 0;
    int thumb = 0;
    SwitchMetrics(node, trackW, trackH, thumb);
    const std::string variant = Lower(RoboXMLAttr(node, "variant", "primary"));
    const bool checked = ButtonOn(node, "checked");
    const bool loading = ButtonOn(node, "loading");
    const bool disabled = ButtonOn(node, "disabled") || loading;
    const int fade = disabled && !loading ? 140 : 255;
    PaintOp track;
    track.button = true;
    track.x = x;
    track.y = y;
    track.width = trackW;
    track.height = trackH;
    track.fade = fade;
    if (checked && variant == "danger") {
        track.fill = ThemeColor("danger", light);
    } else if (checked) {
        track.fill = ThemeColor("brand-primary", light);
        if (variant == "outline") {
            track.border = track.fill;
            track.borderPx = std::max(1, Px(1));
        }
    } else if (variant == "subtle") {
        track.fill = ThemeColor("bg-tertiary", light);
    } else if (variant == "outline") {
        track.border = ThemeColor("border-default", light);
        track.borderPx = std::max(1, Px(1));
    } else if (variant == "ghost") {
        track.fill = 0;
    } else if (light) {
        track.fill = ThemeColor("border-default", light);
    } else {
        track.fill = ThemeColor("bg-active", light);
    }
    ops.push_back(track);
    const int inset = std::max(1, Px(2));
    const int travel = std::max(0, trackW - inset * 2 - thumb);
    PaintOp knob;
    knob.button = true;
    knob.x = x + inset + (checked ? travel : 0);
    knob.y = y + inset;
    knob.width = thumb;
    knob.height = thumb;
    knob.fade = fade;
    if (!checked && variant == "ghost") {
        knob.fill = ThemeColor("bg-elevated", light);
        knob.border = ThemeColor("border-default", light);
        knob.borderPx = std::max(1, Px(1));
    } else {
        knob.fill = 0xFFFFFFFFu;
    }
    if (loading) {
        knob.loading = true;
        knob.color = ThemeColor("brand-primary", light);
    }
    ops.push_back(knob);
}

struct SegmentFace {
    std::string label;
    std::string icon;
    std::string badge;
    bool loading = false;
    bool disabled = false;
};

void SegmentFaces(const RoboXMLNode& node, std::vector<SegmentFace>& faces, int& selected) {
    const std::string wanted = RoboXMLAttr(node, "value");
    selected = 0;
    int index = 0;
    for (const RoboXMLNode& child : node.children) {
        if (Lower(child.name) != "segment") {
            continue;
        }
        SegmentFace face;
        face.label = RoboXMLAttr(child, "label");
        if (face.label.empty()) {
            face.label = TextCopy(child);
        }
        face.icon = RoboWebSvg(gAssets, RoboXMLAttr(child, "icon"));
        face.badge = RoboXMLAttr(child, "badge");
        const std::string status = Lower(RoboXMLAttr(child, "status"));
        face.loading = ButtonOn(child, "loading") || status == "loading";
        face.disabled = ButtonOn(child, "disabled");
        if (!wanted.empty() && RoboXMLAttr(child, "value") == wanted) {
            selected = index;
        }
        faces.push_back(std::move(face));
        ++index;
    }
}

int SegmentTrackHeight(const std::string& size) {
    if (size == "sm" || size == "small") {
        return Px(36);
    }
    if (size == "lg" || size == "large") {
        return Px(50);
    }
    return Px(44);
}

int SegmentFontSize(const std::string& size) {
    return std::max(1, size == "sm" || size == "small" ? Px(15) : Px(17));
}

int SegmentCellWidth(const SegmentFace& face, const std::string& content, const std::string& size) {
    const bool icon = content == "icon";
    int minimum = Px(72);
    int pad = Px(16);
    if (icon) {
        minimum = size == "lg" || size == "large" ? Px(50) : size == "sm" || size == "small" ? Px(36) : Px(44);
        pad = size == "lg" || size == "large" ? Px(14) : size == "sm" || size == "small" ? Px(10) : Px(12);
    } else if (size == "sm" || size == "small") {
        pad = Px(12);
    } else if (size == "lg" || size == "large") {
        pad = Px(20);
    }
    int width = pad * 2;
    const int font = SegmentFontSize(size);
    if (icon || content == "both") {
        if (!face.icon.empty()) {
            width += font;
        }
    }
    if (!icon || face.icon.empty()) {
        width += MeasureText(face.label, font, 1);
        if (!face.badge.empty()) {
            width += Px(8) + MeasureText(face.badge, std::max(1, Px(12)), 1);
        }
    }
    if (content == "both" && !face.icon.empty()) {
        width += Px(8);
    }
    return std::max(minimum, width);
}

RoboU32 MixColor(RoboU32 first, RoboU32 second, int firstPercent) {
    auto channel = [&](int shift) {
        const int a = static_cast<int>((first >> shift) & 255u);
        const int b = static_cast<int>((second >> shift) & 255u);
        return (a * firstPercent + b * (100 - firstPercent)) / 100;
    };
    return 0xFF000000u | (static_cast<RoboU32>(channel(16)) << 16) | (static_cast<RoboU32>(channel(8)) << 8) |
           static_cast<RoboU32>(channel(0));
}

int SegmentNaturalWidth(const RoboXMLNode& node) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    const std::string content = Lower(RoboXMLAttr(node, "content", "label"));
    const bool grouped = Lower(RoboXMLAttr(node, "mode", "inset")) == "grouped";
    std::vector<SegmentFace> faces;
    int selected = 0;
    SegmentFaces(node, faces, selected);
    int width = grouped ? 0 : Px(4);
    for (const SegmentFace& face : faces) {
        width += SegmentCellWidth(face, content == "icon" || content == "both" ? content : "label", size);
    }
    const std::string label = RoboXMLAttr(node, "label");
    if (!label.empty()) {
        width = std::max(width, MeasureText(label, std::max(1, Px(12)), 0));
    }
    return std::max(1, width);
}

int PaintSegmented(std::vector<PaintOp>& ops, const RoboXMLNode& node, int x, int y, int width, bool light) {
    const std::string size = Lower(RoboXMLAttr(node, "size", "md"));
    std::string content = Lower(RoboXMLAttr(node, "content", "label"));
    if (content != "icon" && content != "both") {
        content = "label";
    }
    std::string variant = Lower(RoboXMLAttr(node, "variant", "subtle"));
    if (variant != "outline" && variant != "ghost" && variant != "primary" && variant != "pill") {
        variant = "subtle";
    }
    const bool grouped = Lower(RoboXMLAttr(node, "mode", "inset")) == "grouped";
    const bool disabled = ButtonOn(node, "disabled");
    std::vector<SegmentFace> faces;
    int selected = 0;
    SegmentFaces(node, faces, selected);
    const int count = static_cast<int>(faces.size());
    if (count == 0) {
        return 0;
    }
    if (selected < 0 || selected >= count) {
        selected = 0;
    }
    int top = y;
    const std::string label = RoboXMLAttr(node, "label");
    if (!label.empty()) {
        PaintOp caption;
        caption.text = true;
        caption.value = label;
        caption.x = x;
        caption.y = top;
        caption.width = width;
        caption.height = Px(12);
        caption.size = std::max(1, Px(12));
        caption.color = ThemeColor("text-tertiary", light);
        ops.push_back(caption);
        top += Px(12) + Px(8);
    }
    const int trackH = SegmentTrackHeight(size);
    const int inset = grouped ? 0 : std::max(1, Px(2));
    const int radius = variant == "pill" ? trackH / 2 : (size == "sm" || size == "small" ? Px(10) : Px(12));
    PaintOp track;
    track.x = x;
    track.y = top;
    track.width = width;
    track.height = trackH;
    track.radius = radius;
    if (variant == "outline") {
        track.fill = ThemeColor("border-default", light);
        ops.push_back(track);
        PaintOp hole;
        hole.x = x + std::max(1, Px(1));
        hole.y = top + std::max(1, Px(1));
        hole.width = std::max(1, width - std::max(1, Px(1)) * 2);
        hole.height = std::max(1, trackH - std::max(1, Px(1)) * 2);
        hole.radius = std::max(0, radius - std::max(1, Px(1)));
        hole.fill = ThemeColor("bg-primary", light);
        ops.push_back(hole);
    } else if (variant != "ghost") {
        track.fill = ThemeColor("bg-tertiary", light);
        ops.push_back(track);
    }
    const int inner = std::max(1, width - inset * 2);
    const int cell = std::max(1, inner / count);
    RoboU32 thumb = ThemeColor("bg-elevated", light);
    if (variant == "primary") {
        thumb = ThemeColor("brand-primary", light);
    } else if (!light) {
        thumb = MixColor(ThemeColor("bg-tertiary", light), 0xFFFFFFFFu, 72);
    } else if (variant == "outline") {
        thumb = ThemeColor("bg-secondary", light);
    } else if (variant == "ghost") {
        thumb = ThemeColor("bg-tertiary", light);
    }
    if (!grouped) {
        PaintOp slider;
        slider.x = x + inset + cell * selected;
        slider.y = top + inset;
        slider.width = selected == count - 1 ? inner - cell * selected : cell;
        slider.height = std::max(1, trackH - inset * 2);
        slider.radius = variant == "pill" ? slider.height / 2 : std::max(0, radius - inset);
        slider.fill = thumb;
        ops.push_back(slider);
    }
    const RoboU32 rule = ThemeColor(light ? "border-default" : "border-strong", light);
    if (variant != "ghost") {
        for (int index = 1; index < count; ++index) {
            if (index == selected || index == selected + 1) {
                continue;
            }
            PaintOp line;
            line.x = x + inset + cell * index;
            line.y = top + trackH / 4;
            line.width = std::max(1, Px(1));
            line.height = trackH / 2;
            line.fill = rule;
            ops.push_back(line);
        }
    }
    const int font = SegmentFontSize(size);
    for (int index = 0; index < count; ++index) {
        const SegmentFace& face = faces[static_cast<std::size_t>(index)];
        const int cellX = x + inset + cell * index;
        const int cellW = index == count - 1 ? inner - cell * index : cell;
        const bool on = index == selected;
        if (grouped && on) {
            PaintOp chosen;
            chosen.x = cellX;
            chosen.y = top;
            chosen.width = cellW;
            chosen.height = trackH;
            chosen.radius = index == 0 || index == count - 1 ? radius : 0;
            chosen.fill = thumb;
            ops.push_back(chosen);
        }
        RoboU32 ink = on ? ThemeColor(variant == "primary" ? "text-on-brand" : "text-primary", light)
                         : ThemeColor("text-secondary", light);
        if (face.disabled || disabled) {
            ink = ThemeColor("text-tertiary", light);
        }
        if (face.loading) {
            PaintOp spin;
            spin.drawing = true;
            spin.loading = true;
            spin.color = ink;
            const int icon = std::max(1, Px(16));
            spin.x = cellX + std::max(0, (cellW - icon) / 2);
            spin.y = top + std::max(0, (trackH - icon) / 2);
            spin.width = icon;
            spin.height = icon;
            ops.push_back(spin);
            continue;
        }
        const bool showIcon = (content == "icon" || content == "both") && !face.icon.empty();
        const bool showLabel = content != "icon" || face.icon.empty();
        int contentW = 0;
        if (showIcon) {
            contentW += font;
        }
        int textW = 0;
        if (showLabel) {
            textW = MeasureText(face.label, font, on ? 1 : 0);
            contentW += textW;
            if (showIcon) {
                contentW += Px(8);
            }
            if (!face.badge.empty()) {
                contentW += Px(8) + MeasureText(face.badge, std::max(1, Px(12)), 1);
            }
        }
        int cursor = cellX + std::max(0, (cellW - contentW) / 2);
        const int mid = top + trackH / 2;
        if (showIcon) {
            PaintOp glyph;
            glyph.drawing = true;
            glyph.drawingSvg = face.icon;
            glyph.color = ink;
            glyph.x = cursor;
            glyph.y = mid - font / 2;
            glyph.width = font;
            glyph.height = font;
            ops.push_back(glyph);
            cursor += font + (showLabel ? Px(8) : 0);
        }
        if (showLabel) {
            PaintOp text;
            text.text = true;
            text.value = face.label;
            text.x = cursor;
            text.y = mid - font / 2;
            text.width = textW;
            text.height = font;
            text.size = font;
            text.face = on ? 1 : 0;
            text.color = ink;
            ops.push_back(text);
            cursor += textW;
            if (!face.badge.empty()) {
                PaintOp badge;
                badge.text = true;
                badge.value = face.badge;
                badge.x = cursor + Px(8);
                badge.y = mid - Px(12) / 2;
                badge.width = MeasureText(face.badge, std::max(1, Px(12)), 1);
                badge.height = Px(12);
                badge.size = std::max(1, Px(12));
                badge.face = 1;
                badge.color = ink;
                ops.push_back(badge);
            }
        }
    }
    return (top - y) + trackH;
}

bool IsIcon(const RoboXMLNode& node) {
    const std::string name = RoboUIFace(node);
    return name == "icon" || name == "svg" || RoboCompilerIsIconNode(node);
}

int AskedLength(const RoboXMLNode& node, const char* key, int basis) {
    const std::string raw = RoboXMLAttr(node, key);
    if (raw.empty() || Lower(raw) == "auto") {
        return -1;
    }
    return std::max(0, MeasureLength(raw, basis));
}

int ContentWidth(const RoboXMLNode& node, int maxWidth) {
    const int cap = AttrLength(node, "maxwidth");
    if (cap > 0 && cap < maxWidth) {
        maxWidth = cap;
    }
    const int asked = AskedLength(node, "width", maxWidth);
    if (asked >= 0) {
        return std::min(maxWidth, std::max(1, asked));
    }
    const std::string name = Lower(node.name);
    if (name == "image") {
        return 0;
    }
    if (name == "anchor") {
        std::string label = TextCopy(node);
        const std::string sizeName = Lower(RoboXMLAttr(node, "size", "inherit"));
        int size = 14;
        if (sizeName == "sm" || sizeName == "small") size = 12;
        else if (sizeName == "md" || sizeName == "medium" || sizeName == "lg" || sizeName == "large") size = 17;
        const std::string variant = Lower(RoboXMLAttr(node, "variant", "inline"));
        const int face = (variant == "arrow" || variant == "standalone" || variant == "more") ? 1 : 0;
        return std::min(maxWidth, std::max(1, MeasureText(label, size, face) + Px(28)));
    }
    if (IsText(name)) {
        const TypeStyle style = StyleOf(node);
        return std::min(maxWidth, std::max(1, MeasureText(TextCopy(node), style.size, style.face)));
    }
    if (IsIcon(node)) {
        return std::min(maxWidth, IconBox(node));
    }
    if (name == "iconbutton") {
        return std::min(maxWidth, IconButtonExtent(node));
    }
    if (name == "switch") {
        int trackW = 0;
        int trackH = 0;
        int thumb = 0;
        SwitchMetrics(node, trackW, trackH, thumb);
        return std::min(maxWidth, trackW);
    }
    if (name == "switchitem" || name == "switchgroup") {
        return maxWidth;
    }
    if (name == "segmentedcontrol") {
        if (ButtonOn(node, "fullWidth")) {
            return maxWidth;
        }
        return std::min(maxWidth, SegmentNaturalWidth(node));
    }
    if (name == "segmentedtoolbar") {
        return maxWidth;
    }
    if (name == "button") {
        const std::string variant = Lower(RoboXMLAttr(node, "variant", "primary"));
        const int face = variant == "ghost" ? 0 : 1;
        const std::string label = ButtonLabel(node);
        int width = MeasureText(label, ButtonFontSize(node), face) + ButtonPad(node) * 2;
        const int icon = Px(16);
        const int gap = Px(8);
        std::string leading;
        std::string trailing;
        ResolveButtonIcons(node, leading, trailing);
        if (!leading.empty()) {
            width += icon + gap;
        }
        if (!trailing.empty()) {
            width += icon + gap;
        }
        const std::string full = Lower(RoboXMLAttr(node, "fullwidth", "true"));
        if (full != "false" && full != "0" && full != "no" && full != "off") {
            return std::max(width, maxWidth);
        }
        return std::min(maxWidth, width);
    }
    if (!IsBox(name)) {
        int width = 0;
        int count = 0;
        for (const RoboXMLNode& child : node.children) {
            width += ContentWidth(child, maxWidth);
            ++count;
        }
        if (count > 1) {
            width += Gap(node) * (count - 1);
        }
        return std::min(maxWidth, std::max(width, 0));
    }
    int used = 0;
    const int inner = std::max(0, maxWidth - Padding(node) * 2);
    const int gap = Gap(node);
    if (RowDirection(node)) {
        int count = 0;
        for (const RoboXMLNode& child : node.children) {
            used += ContentWidth(child, inner);
            ++count;
        }
        if (count > 1) {
            used += gap * (count - 1);
        }
    } else {
        for (const RoboXMLNode& child : node.children) {
            used = std::max(used, ContentWidth(child, inner));
        }
    }
    return std::min(maxWidth, used + Padding(node) * 2);
}

RoboU32 ThemeColor(const char* name, bool light) {
    return RoboFoundationPlatformColorForTheme(name, light ? ROBO_TRUE : ROBO_FALSE);
}

void RecolorButton(PaintOp& op, bool light, bool hot, bool pressed, bool focused) {
    const bool disabled = op.quiet;
    const bool showActive = op.preview == 3 || (pressed && !disabled);
    const bool showHover = !showActive && (op.preview == 1 || (hot && !disabled));
    int look = 0;
    if (showActive) {
        look = 3;
    } else if (showHover) {
        look = 1;
    }
    op.focus = (op.preview == 2 || focused) && !disabled;
    op.fade = disabled && !op.loading ? 140 : 255;
    op.borderPx = 0;
    op.fill = 0;
    op.border = 0;
    const RoboU32 brand = ThemeColor("brand-primary", light);
    const RoboU32 onBrand = ThemeColor("text-on-brand", light);
    if (op.kind == 1) {
        op.border = brand;
        op.borderPx = std::max(1, Px(1));
        op.color = brand;
        if (look == 3) {
            op.fill = ThemeColor("brand-primary-active", light);
            op.border = op.fill;
            op.color = onBrand;
        } else if (look == 1) {
            op.fill = brand;
            op.color = onBrand;
        }
    } else if (op.kind == 2) {
        op.fill = ThemeColor(light ? "bg-elevated" : "bg-secondary", light);
        op.border = ThemeColor(light ? "border-default" : "border-strong", light);
        op.borderPx = std::max(1, Px(1));
        op.color = ThemeColor("text-primary", light);
        if (look == 3) {
            op.fill = ThemeColor("bg-active", light);
            op.border = ThemeColor("text-primary", light);
        } else if (look == 1) {
            op.fill = ThemeColor("bg-hover", light);
            op.border = ThemeColor("text-tertiary", light);
        }
    } else if (op.kind == 3) {
        op.color = ThemeColor("text-tertiary", light);
        if (look == 3) {
            op.fill = ThemeColor("brand-primary-subtle", light);
            op.color = ThemeColor("brand-primary-active", light);
        } else if (look == 1) {
            op.color = brand;
        }
    } else if (op.kind == 4) {
        op.fill = ThemeColor(look == 1 ? "danger-hover" : "danger", light);
        op.color = onBrand;
        if (look == 3) {
            const RoboU32 red = ((op.fill >> 16) & 0xFFu) * 78u / 100u;
            const RoboU32 green = ((op.fill >> 8) & 0xFFu) * 78u / 100u;
            const RoboU32 blue = (op.fill & 0xFFu) * 78u / 100u;
            op.fill = 0xFF000000u | (red << 16) | (green << 8) | blue;
        }
    } else {
        const char* token = "brand-primary";
        if (look == 1) {
            token = "brand-primary-hover";
        } else if (look == 3) {
            token = "brand-primary-active";
        }
        op.fill = ThemeColor(token, light);
        op.color = onBrand;
    }
    if (op.focus && op.borderPx > 0) {
        op.border = brand;
    }
}

void StyleButton(const RoboXMLNode& node, bool light, bool hot, bool pressed, PaintOp& op) {
    std::string variant = Lower(RoboXMLAttr(node, "variant", "primary"));
    if (variant == "destructive") {
        variant = "danger";
    }
    const std::string preview = Lower(RoboXMLAttr(node, "preview"));
    op.kind = variant == "secondary" ? 1 : variant == "action" ? 2 : variant == "ghost" ? 3 : variant == "danger" ? 4 : 0;
    op.preview = preview == "hover" ? 1 : preview == "focus" ? 2 : preview == "active" ? 3 : 0;
    op.quiet = ButtonOn(node, "disabled") || ButtonOn(node, "loading");
    op.face = op.kind == 3 ? 0 : 1;
    op.size = ButtonFontSize(node);
    RecolorButton(op, light, hot, pressed, false);
}

struct TextLine {
    std::string text;
    int width = 0;
};

struct TextFlow {
    int mode = 1;
    int maxLines = 0;
    int overflow = 0;
    int whiteSpace = 1;
    bool breakLong = true;
};

int Utf8Span(const std::string& text, std::size_t index) {
    if (index >= text.size()) {
        return 1;
    }
    const auto lead = static_cast<unsigned char>(text[index]);
    int span = 1;
    if ((lead & 0xE0u) == 0xC0u) {
        span = 2;
    } else if ((lead & 0xF0u) == 0xE0u) {
        span = 3;
    } else if ((lead & 0xF8u) == 0xF0u) {
        span = 4;
    }
    if (index + static_cast<std::size_t>(span) > text.size()) {
        return 1;
    }
    return span;
}

void PopUtf8(std::string& text) {
    if (text.empty()) {
        return;
    }
    std::size_t index = text.size() - 1;
    while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0u) == 0x80u) {
        --index;
    }
    text.erase(index);
}

void FitLine(std::string& line, int width, int size, int face, bool dots) {
    const std::string mark = dots ? "\xE2\x80\xA6" : "";
    if (width <= 0) {
        return;
    }
    if (MeasureText(line + mark, size, face) <= width) {
        if (dots && mark.size() > 0 && line.find('\xE2') == std::string::npos) {
            line += mark;
        }
        return;
    }
    while (!line.empty() && MeasureText(line + mark, size, face) > width) {
        PopUtf8(line);
    }
    line += mark;
    if (MeasureText(line, size, face) > width) {
        line = mark;
    }
}

TextFlow FlowOf(const RoboXMLNode& node) {
    TextFlow flow;
    std::string wrap = Lower(RoboXMLAttr(node, "textwrap"));
    if (wrap.empty()) {
        wrap = Lower(RoboXMLAttr(node, "wrap"));
    }
    if (wrap.empty()) {
        wrap = "word";
    }
    if (wrap == "none" || wrap == "nowrap" || wrap == "false") {
        flow.mode = 0;
    } else if (wrap == "char" || wrap == "character") {
        flow.mode = 2;
    } else if (wrap == "balance") {
        flow.mode = 3;
    } else if (wrap == "pretty") {
        flow.mode = 4;
    } else {
        flow.mode = 1;
    }
    std::string space = Lower(RoboXMLAttr(node, "whitespace"));
    if (space.empty()) {
        space = Lower(RoboXMLAttr(node, "white-space"));
    }
    if (space == "nowrap") {
        flow.mode = 0;
        flow.whiteSpace = 0;
    } else if (space == "pre") {
        flow.whiteSpace = 2;
    } else if (space == "pre-line" || space == "preline") {
        flow.whiteSpace = 3;
    } else if (space == "normal") {
        flow.whiteSpace = 0;
    } else if (space == "pre-wrap" || space == "prewrap") {
        flow.whiteSpace = 1;
    } else {
        flow.whiteSpace = flow.mode == 0 ? 0 : 1;
    }
    std::string lines = RoboXMLAttr(node, "lines");
    if (lines.empty()) {
        lines = RoboXMLAttr(node, "maxlines");
    }
    if (IsDigits(lines)) {
        flow.maxLines = std::max(1, std::stoi(lines));
    }
    std::string overflow = Lower(RoboXMLAttr(node, "textoverflow"));
    if (overflow.empty()) {
        overflow = Lower(RoboXMLAttr(node, "overflow"));
    }
    if (overflow == "ellipsis") {
        flow.overflow = 1;
    } else if (overflow == "clip" || overflow == "hidden") {
        flow.overflow = 2;
    } else if (flow.maxLines > 0 || flow.mode == 0) {
        flow.overflow = flow.mode == 0 ? 0 : 1;
    }
    const std::string longBreak = Lower(RoboXMLAttr(node, "overflowwrap"));
    if (longBreak == "normal") {
        flow.breakLong = false;
    } else if (longBreak == "anywhere" || longBreak == "break-word" || longBreak == "breakword") {
        flow.breakLong = true;
    } else {
        flow.breakLong = flow.mode != 0;
    }
    return flow;
}

void AppendChars(const std::string& paragraph, int width, int size, int face, std::vector<TextLine>& lines) {
    std::string line;
    int lineWidth = 0;
    for (std::size_t index = 0; index < paragraph.size();) {
        const int span = Utf8Span(paragraph, index);
        const std::string piece = paragraph.substr(index, static_cast<std::size_t>(span));
        index += static_cast<std::size_t>(span);
        if (piece == "\n") {
            lines.push_back(TextLine{line, lineWidth});
            line.clear();
            lineWidth = 0;
            continue;
        }
        const int pieceWidth = MeasureText(piece, size, face);
        if (width > 0 && !line.empty() && lineWidth + pieceWidth > width) {
            lines.push_back(TextLine{line, lineWidth});
            line = piece;
            lineWidth = pieceWidth;
            continue;
        }
        line += piece;
        lineWidth += pieceWidth;
    }
    lines.push_back(TextLine{line, lineWidth});
}

void AppendWrapped(const std::string& paragraph, int width, int size, int face, bool breakLong, std::vector<TextLine>& lines) {
    std::string word;
    std::string line;
    int lineWidth = 0;
    const auto flush = [&]() {
        if (line.empty()) {
            return;
        }
        lines.push_back(TextLine{line, lineWidth});
        line.clear();
        lineWidth = 0;
    };
    const auto addWord = [&](const std::string& piece) {
        if (piece.empty()) {
            return;
        }
        const int pieceWidth = MeasureText(piece, size, face);
        if (line.empty()) {
            if (breakLong && width > 0 && pieceWidth > width) {
                AppendChars(piece, width, size, face, lines);
                if (!lines.empty()) {
                    line = lines.back().text;
                    lineWidth = lines.back().width;
                    lines.pop_back();
                }
            } else {
                line = piece;
                lineWidth = pieceWidth;
            }
            return;
        }
        const int gap = MeasureText(" ", size, face);
        if (width > 0 && lineWidth + gap + pieceWidth > width) {
            flush();
            if (breakLong && pieceWidth > width) {
                AppendChars(piece, width, size, face, lines);
                if (!lines.empty()) {
                    line = lines.back().text;
                    lineWidth = lines.back().width;
                    lines.pop_back();
                }
            } else {
                line = piece;
                lineWidth = pieceWidth;
            }
            return;
        }
        line.push_back(' ');
        line += piece;
        lineWidth += gap + pieceWidth;
    };
    for (char ch : paragraph) {
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            addWord(word);
            word.clear();
        } else {
            word.push_back(ch);
        }
    }
    addWord(word);
    flush();
}

std::string CollapseSpaces(const std::string& value, bool keepBreaks) {
    std::string out;
    bool space = false;
    for (std::size_t index = 0; index < value.size();) {
        const int span = Utf8Span(value, index);
        const std::string piece = value.substr(index, static_cast<std::size_t>(span));
        index += static_cast<std::size_t>(span);
        const bool br = piece == "\n";
        const bool blank = piece == " " || piece == "\t" || piece == "\r" || (!keepBreaks && br);
        if (blank) {
            space = !out.empty();
            continue;
        }
        if (br) {
            if (!out.empty() && out.back() == ' ') {
                out.pop_back();
            }
            out.push_back('\n');
            space = false;
            continue;
        }
        if (space) {
            out.push_back(' ');
            space = false;
        }
        out += piece;
    }
    return out;
}

void WrapSource(const std::string& value, int width, int size, int face, int mode, bool breakLong, std::vector<TextLine>& lines) {
    lines.clear();
    if (mode == 0) {
        std::string flat = value;
        for (char& ch : flat) {
            if (ch == '\n' || ch == '\t' || ch == '\r') {
                ch = ' ';
            }
        }
        lines.push_back(TextLine{flat, MeasureText(flat, size, face)});
        return;
    }
    if (mode == 2) {
        AppendChars(value, width, size, face, lines);
    } else {
        std::size_t start = 0;
        while (start <= value.size()) {
            const std::size_t mark = value.find('\n', start);
            const std::string paragraph = value.substr(start, mark == std::string::npos ? std::string::npos : mark - start);
            const std::size_t before = lines.size();
            AppendWrapped(paragraph, width, size, face, breakLong, lines);
            if (lines.size() == before) {
                lines.push_back(TextLine{});
            }
            if (mark == std::string::npos) {
                break;
            }
            start = mark + 1;
        }
    }
    if (lines.empty()) {
        lines.push_back(TextLine{});
    }
}

void ClampLines(std::vector<TextLine>& lines, int width, int size, int face, const TextFlow& flow, bool more) {
    if (flow.mode == 0 && !lines.empty() && width > 0 && lines[0].width > width && flow.overflow != 0) {
        FitLine(lines[0].text, width, size, face, flow.overflow == 1);
        lines[0].width = MeasureText(lines[0].text, size, face);
    }
    if (flow.maxLines <= 0 || static_cast<int>(lines.size()) <= flow.maxLines) {
        return;
    }
    const bool ellipsis = flow.overflow != 2;
    lines.resize(static_cast<std::size_t>(flow.maxLines));
    if (more || ellipsis) {
        FitLine(lines.back().text, width, size, face, ellipsis);
        lines.back().width = MeasureText(lines.back().text, size, face);
    }
}

void BalanceLines(std::vector<TextLine>& lines, int width, int size, int face) {
    if (lines.size() < 2 || width <= 0) {
        return;
    }
    TextLine& previous = lines[lines.size() - 2];
    TextLine& last = lines.back();
    const std::size_t cut = previous.text.rfind(' ');
    if (cut == std::string::npos || last.text.find(' ') != std::string::npos) {
        return;
    }
    const std::string moved = previous.text.substr(cut + 1);
    const std::string stay = previous.text.substr(0, cut);
    const std::string joined = moved + " " + last.text;
    if (stay.empty() || moved.empty() || MeasureText(joined, size, face) > width) {
        return;
    }
    last.text = joined;
    last.width = MeasureText(joined, size, face);
    previous.text = stay;
    previous.width = MeasureText(stay, size, face);
}

void TextLines(const std::string& value, int width, int size, int face, const TextFlow& flow, std::vector<TextLine>& lines) {
    std::string source = value;
    if (flow.whiteSpace == 2) {
        TextFlow hard = flow;
        hard.mode = 2;
        hard.breakLong = false;
        source = value;
        WrapSource(source, 0, size, face, 2, false, lines);
        for (TextLine& line : lines) {
            line.width = MeasureText(line.text, size, face);
        }
        ClampLines(lines, width, size, face, flow, false);
        return;
    }
    if (flow.whiteSpace == 0) {
        source = CollapseSpaces(value, false);
    } else if (flow.whiteSpace == 3) {
        source = CollapseSpaces(value, true);
    }
    const int mode = flow.mode == 3 || flow.mode == 4 ? 1 : flow.mode;
    int useWidth = width;
    if ((flow.mode == 3 || flow.mode == 4) && width > 8) {
        std::vector<TextLine> full;
        WrapSource(source, width, size, face, 1, flow.breakLong, full);
        const int count = std::max(1, static_cast<int>(full.size()));
        int low = 8;
        int high = width;
        int best = width;
        while (low <= high) {
            const int mid = (low + high) / 2;
            std::vector<TextLine> trial;
            WrapSource(source, mid, size, face, 1, flow.breakLong, trial);
            if (static_cast<int>(trial.size()) <= count) {
                best = mid;
                high = mid - 1;
            } else {
                low = mid + 1;
            }
        }
        useWidth = best;
    }
    WrapSource(source, useWidth, size, face, mode, flow.breakLong, lines);
    if (flow.mode == 4) {
        BalanceLines(lines, width, size, face);
    }
    const bool more = flow.maxLines > 0 && static_cast<int>(lines.size()) > flow.maxLines;
    ClampLines(lines, width, size, face, flow, more);
}

int TextBlockHeight(const std::string& value, int width, int size, int face, int leading, const TextFlow& flow) {
    std::vector<TextLine> lines;
    TextLines(value, width, size, face, flow, lines);
    return std::max(leading, size) * static_cast<int>(std::max<std::size_t>(1, lines.size()));
}

struct BoxEdge {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
};

BoxEdge BoxEdges(const RoboXMLNode& node, const char* all, const char* axisX, const char* axisY, const char* top,
                 const char* right, const char* bottom, const char* left, int basis) {
    BoxEdge edge;
    const int uniform = MeasureLength(RoboXMLAttr(node, all), basis);
    const std::string rawX = RoboXMLAttr(node, axisX);
    const std::string rawY = RoboXMLAttr(node, axisY);
    const int xAxis = rawX.empty() ? uniform : MeasureLength(rawX, basis);
    const int yAxis = rawY.empty() ? uniform : MeasureLength(rawY, basis);
    const auto pick = [&](const char* key, int axis) {
        const std::string raw = RoboXMLAttr(node, key);
        return raw.empty() ? axis : MeasureLength(raw, basis);
    };
    edge.top = pick(top, yAxis);
    edge.right = pick(right, xAxis);
    edge.bottom = pick(bottom, yAxis);
    edge.left = pick(left, xAxis);
    return edge;
}

int PackAlign(const std::string& raw) {
    const std::string key = Lower(raw);
    if (key == "center" || key == "middle") {
        return 1;
    }
    if (key == "end" || key == "flex-end" || key == "right" || key == "bottom") {
        return 2;
    }
    if (key == "stretch") {
        return 3;
    }
    if (key == "space-between") {
        return 4;
    }
    if (key == "space-around") {
        return 5;
    }
    if (key == "space-evenly") {
        return 6;
    }
    return 0;
}

void ShiftRange(std::vector<PaintOp>& ops, std::size_t begin, std::size_t end, HitState* hits, int controlBegin,
                int controlEnd, int dx, int dy) {
    for (std::size_t index = begin; index < end && index < ops.size(); ++index) {
        ops[index].x += dx;
        ops[index].y += dy;
    }
    if (hits == nullptr || hits->controls == nullptr || (dx == 0 && dy == 0)) {
        return;
    }
    const int limit = std::min(controlEnd, static_cast<int>(hits->controls->size()));
    for (int index = controlBegin; index < limit; ++index) {
        (*hits->controls)[index].x += dx;
        (*hits->controls)[index].y += dy;
    }
}

int PlaceNode(const RoboXMLNode& node, int x, int y, int width, bool light, std::vector<PaintOp>& ops, HitState* hits) {
    const std::string name = Lower(node.name);
    std::string shown = Lower(RoboXMLAttr(node, "visibility"));
    if (shown.empty()) shown = Lower(RoboXMLAttr(node, "visible"));
    if (shown == "false" || shown == "0" || shown == "no" || shown == "off" || shown == "hidden" ||
        (shown.empty() && Lower(RoboXMLAttr(node, "hidden")) == "true")) {
        return 0;
    }
    if (name == "image") {
        return 0;
    }
    if (name == "view3d" || name == "scene3d") {
        PaintOp box;
        box.x = x;
        box.y = y;
        box.width = width;
        box.height = 160;
        box.radius = 12;
        box.fill = ThemeColor("bg-secondary", light);
        ops.push_back(box);
        PaintOp op;
        op.text = true;
        op.x = x + 16;
        op.y = y + 16;
        op.width = std::max(1, width - 32);
        op.height = 28;
        op.size = 16;
        op.face = 1;
        op.color = ThemeColor("text-primary", light);
        const std::string source = RoboXMLAttr(node, "source");
        op.value = source.empty() ? "Robix3D" : source;
        ops.push_back(op);
        return 160;
    }
    if (name == "anchor") {
        std::string label = TextCopy(node);
        const std::string sizeName = Lower(RoboXMLAttr(node, "size", "inherit"));
        int size = 14;
        if (sizeName == "sm" || sizeName == "small") size = 12;
        else if (sizeName == "md" || sizeName == "medium" || sizeName == "lg" || sizeName == "large") size = 17;
        const std::string variant = Lower(RoboXMLAttr(node, "variant", "inline"));
        const int face = (variant == "arrow" || variant == "standalone" || variant == "more" || !RoboXMLAttr(node, "arrow").empty()) ? 1 : 0;
        std::string color = "text-link";
        if (Lower(RoboXMLAttr(node, "disabled")) == "true") color = "text-disabled";
        else if (variant == "subtle" && RoboXMLAttr(node, "arrow").empty()) color = "text-tertiary";
        else if (variant == "plain" && RoboXMLAttr(node, "arrow").empty()) color = "text-primary";
        else if (Lower(RoboXMLAttr(node, "tone")) == "danger") color = "danger";
        const int height = std::max(size + 4, (size * 14) / 10);
        PaintOp op;
        op.text = true;
        op.x = x;
        op.y = y;
        op.width = width;
        op.height = height;
        op.size = size;
        op.face = face;
        op.color = RoboFoundationPlatformColorForTheme(color.c_str(), light ? ROBO_TRUE : ROBO_FALSE);
        op.value = label;
        ops.push_back(op);
        return height;
    }
    if (IsText(name)) {
        const TypeStyle style = StyleOf(node);
        const std::string value = TextCopy(node);
        if (value.empty()) {
            return 0;
        }
        const std::string alignName = Lower(RoboXMLAttr(node, "textalign").empty() ? RoboXMLAttr(node, "align") : RoboXMLAttr(node, "textalign"));
        PaintOp op;
        op.text = true;
        op.x = x;
        op.y = y;
        op.width = width;
        op.size = style.size;
        op.face = style.face;
        op.align = (alignName == "center" || alignName == "middle") ? 1 : (alignName == "end" || alignName == "right") ? 2 : alignName == "justify" ? 3 : 0;
        const TextFlow flow = FlowOf(node);
        op.wrap = flow.mode;
        op.maxLines = flow.maxLines;
        op.overflow = flow.overflow;
        op.whiteSpace = flow.whiteSpace;
        op.breakLong = flow.breakLong;
        const std::string colorName = RoboXMLAttr(node, "color");
        op.color = RoboFoundationPlatformColorForTheme(colorName.empty() ? style.color : colorName.c_str(), light ? ROBO_TRUE : ROBO_FALSE);
        op.value = value;
        op.height = TextBlockHeight(value, width, style.size, style.face, style.leading, flow);
        ops.push_back(op);
        return op.height;
    }
    if (IsIcon(node)) {
        const int box = IconBox(node);
        PaintOp op;
        op.drawing = true;
        op.x = x;
        op.y = y;
        op.width = box;
        op.height = box;
        op.color = RoboFoundationPlatformColorForTheme("text-primary", light ? ROBO_TRUE : ROBO_FALSE);
        op.drawingSvg = IconDrawing(node);
        ops.push_back(op);
        return box;
    }
    if (name == "button") {
        const int height = ButtonHeight(node);
        const int control = hits == nullptr ? -1 : hits->next;
        const bool underPointer = hits != nullptr && hits->pointerX >= x && hits->pointerX < x + width &&
                                  hits->pointerY >= y && hits->pointerY < y + height && !hits->pointerTaken;
        const bool hot = underPointer || (hits != nullptr && hits->pointerX < 0 && control == hits->hot);
        const bool pressed = hits != nullptr && control == hits->pressed;
        if (underPointer && hits != nullptr) {
            hits->pointerTaken = true;
        }
        PaintOp op;
        op.button = true;
        op.control = control;
        op.x = x;
        op.y = y;
        op.width = width;
        op.height = height;
        op.radius = height / 2;
        StyleButton(node, light, hot, pressed, op);
        op.loading = ButtonOn(node, "loading");
        op.value = ButtonLabel(node);
        ResolveButtonIcons(node, op.leadingSvg, op.trailingSvg);
        ops.push_back(op);
        if (hits != nullptr) {
            if (hits->controls != nullptr) {
                const bool enabled = !ButtonOn(node, "disabled") && !ButtonOn(node, "loading");
                hits->controls->push_back(WindowControl{x, y, width, height, enabled});
            }
            ++hits->next;
        }
        return height;
    }
    if (name == "iconbutton") {
        const int box = IconButtonExtent(node);
        const int control = hits == nullptr ? -1 : hits->next;
        const bool underPointer = hits != nullptr && hits->pointerX >= x && hits->pointerX < x + box &&
                                  hits->pointerY >= y && hits->pointerY < y + box && !hits->pointerTaken;
        const bool hot = underPointer || (hits != nullptr && hits->pointerX < 0 && control == hits->hot);
        const bool pressed = hits != nullptr && control == hits->pressed;
        if (underPointer && hits != nullptr) {
            hits->pointerTaken = true;
        }
        std::string status = Lower(RoboXMLAttr(node, "status"));
        if (status.empty() && ButtonOn(node, "loading")) {
            status = "loading";
        }
        std::string iconName = RoboXMLAttr(node, "icon");
        if (iconName.empty()) {
            iconName = RoboXMLAttr(node, "name");
        }
        std::string glyph = RoboWebSvg(gAssets, iconName);
        for (const RoboXMLNode& child : node.children) {
            const std::string childName = Lower(child.name);
            if (childName != "icon" && childName != "svg") {
                continue;
            }
            const std::string markup = RoboWebIconMarkup(gAssets, child);
            if (!markup.empty()) {
                glyph = markup;
                break;
            }
        }
        if (status == "loading") {
            glyph = ButtonSpinner();
        } else if (status == "success") {
            glyph = IconButtonMark(true);
        } else if (status == "error") {
            glyph = IconButtonMark(false);
        }
        PaintOp op;
        op.button = true;
        op.iconButton = true;
        op.control = control;
        op.x = x;
        op.y = y;
        op.width = box;
        op.height = box;
        op.radius = box / 2;
        std::string variant = Lower(RoboXMLAttr(node, "variant", "ghost"));
        op.kind = variant == "subtle" ? 11 : variant == "outline" ? 12 : variant == "primary" ? 13 : variant == "danger" ? 14 : 10;
        op.preview = status == "loading" ? 1 : status == "success" ? 2 : status == "error" ? 3 : 0;
        op.quiet = ButtonOn(node, "disabled") || status == "loading";
        op.loading = status == "loading";
        StyleIconButton(op, light, hot, pressed, false);
        op.leadingSvg = glyph;
        ops.push_back(op);
        const std::string badge = RoboXMLAttr(node, "badge");
        if (!badge.empty() && badge != "0") {
            PaintOp dot;
            dot.x = x + box - Px(16);
            dot.y = y + Px(2);
            dot.width = Px(18);
            dot.height = Px(18);
            dot.radius = Px(9);
            dot.fill = ThemeColor("danger", light);
            ops.push_back(dot);
            PaintOp count;
            count.text = true;
            count.value = badge;
            count.x = dot.x + Px(4);
            count.y = dot.y + Px(4);
            count.width = Px(12);
            count.height = Px(12);
            count.size = std::max(1, Px(10));
            count.face = 1;
            count.color = ThemeColor("text-on-brand", light);
            ops.push_back(count);
        }
        if (hits != nullptr) {
            if (hits->controls != nullptr) {
                const bool enabled = !ButtonOn(node, "disabled") && status != "loading";
                hits->controls->push_back(WindowControl{x, y, box, box, enabled});
            }
            ++hits->next;
        }
        return box;
    }
    if (name == "switch") {
        int trackW = 0;
        int trackH = 0;
        int thumb = 0;
        SwitchMetrics(node, trackW, trackH, thumb);
        PaintSwitchControl(ops, node, x, y, light);
        return trackH;
    }
    if (name == "switchitem") {
        const std::string title = RoboXMLAttr(node, "title");
        const std::string description = RoboXMLAttr(node, "description");
        const int titleSize = std::max(1, Px(17));
        const int descSize = std::max(1, Px(12));
        const int titleH = title.empty() ? 0 : titleSize;
        const int descH = description.empty() ? 0 : descSize;
        const int block = titleH + (descH > 0 ? Px(2) + descH : 0);
        const int height = std::max(Px(56), block + Px(24));
        int trackW = 0;
        int trackH = 0;
        int thumb = 0;
        SwitchMetrics(node, trackW, trackH, thumb);
        if (titleH > 0) {
            PaintOp label;
            label.text = true;
            label.value = title;
            label.x = x;
            label.y = y + (height - block) / 2;
            label.width = std::max(0, width - trackW - Px(16));
            label.height = titleH;
            label.size = titleSize;
            label.face = 0;
            label.color = ThemeColor("text-primary", light);
            ops.push_back(label);
        }
        if (descH > 0) {
            PaintOp copy;
            copy.text = true;
            copy.value = description;
            copy.x = x;
            copy.y = y + (height - block) / 2 + titleH + Px(2);
            copy.width = std::max(0, width - trackW - Px(16));
            copy.height = descH;
            copy.size = descSize;
            copy.face = 0;
            copy.color = ThemeColor("text-tertiary", light);
            ops.push_back(copy);
        }
        PaintSwitchControl(ops, node, x + std::max(0, width - trackW), y + std::max(0, (height - trackH) / 2), light);
        return height;
    }
    if (name == "switchgroup") {
        int cursorY = y;
        bool first = true;
        for (const RoboXMLNode& child : node.children) {
            if (!first) {
                PaintOp line;
                line.x = x;
                line.y = cursorY;
                line.width = width;
                line.height = std::max(1, Px(1));
                line.fill = ThemeColor("border-default", light);
                ops.push_back(line);
            }
            cursorY += PlaceNode(child, x, cursorY, width, light, ops, hits);
            first = false;
        }
        return std::max(0, cursorY - y);
    }
    if (name == "segmentedcontrol") {
        int used = width;
        if (!ButtonOn(node, "fullWidth")) {
            used = std::min(width, SegmentNaturalWidth(node));
        }
        return PaintSegmented(ops, node, x, y, std::max(1, used), light);
    }
    if (name == "segmentedtoolbar") {
        const int gap = Px(8);
        int cursorX = x;
        int lineHeight = 0;
        int cursorY = y;
        int bottom = y;
        for (const RoboXMLNode& child : node.children) {
            const int childWidth = std::min(ContentWidth(child, std::max(1, width - (cursorX - x))), std::max(1, width));
            if (cursorX > x && cursorX + childWidth > x + width) {
                cursorX = x;
                cursorY += lineHeight + gap;
                lineHeight = 0;
            }
            const int childHeight = PlaceNode(child, cursorX, cursorY, std::max(1, childWidth), light, ops, hits);
            lineHeight = std::max(lineHeight, childHeight);
            bottom = std::max(bottom, cursorY + childHeight);
            cursorX += childWidth + gap;
        }
        return std::max(0, bottom - y);
    }
    const int cap = AttrLength(node, "maxwidth");
    if (cap > 0 && cap < width) {
        width = cap;
    }
    BoxEdge pad = BoxEdges(node, "padding", "paddingx", "paddingy", "paddingtop", "paddingright", "paddingbottom", "paddingleft", width);
    if (RoboXMLAttr(node, "paddingx").empty() && !RoboXMLAttr(node, "paddinghorizontal").empty()) {
        const int axis = MeasureLength(RoboXMLAttr(node, "paddinghorizontal"), width);
        if (RoboXMLAttr(node, "paddingleft").empty()) pad.left = axis;
        if (RoboXMLAttr(node, "paddingright").empty()) pad.right = axis;
    }
    if (RoboXMLAttr(node, "paddingy").empty() && !RoboXMLAttr(node, "paddingvertical").empty()) {
        const int axis = MeasureLength(RoboXMLAttr(node, "paddingvertical"), width);
        if (RoboXMLAttr(node, "paddingtop").empty()) pad.top = axis;
        if (RoboXMLAttr(node, "paddingbottom").empty()) pad.bottom = axis;
    }
    const int inner = std::max(0, width - pad.left - pad.right);
    const int originX = x + pad.left;
    const int originY = y + pad.top;
    const std::string display = Lower(RoboXMLAttr(node, "display"));
    const std::string direction = Lower(RoboXMLAttr(node, "direction"));
    const bool grid = name == "grid" || display == "grid" || !RoboXMLAttr(node, "columns").empty();
    const bool row = !grid && (name == "row" || display == "horizontal" || direction == "row" || direction == "row-reverse");
    const bool reverse = direction.find("reverse") != std::string::npos || Lower(RoboXMLAttr(node, "reverse")) == "true";
    const std::string wrapName = Lower(RoboXMLAttr(node, "wrap"));
    const bool wrap = row && (name == "row" || wrapName == "wrap" || wrapName == "true" || wrapName == "wrap-reverse");
    int columns = 1;
    const std::string columnText = RoboXMLAttr(node, "columns");
    if (IsDigits(columnText)) {
        columns = std::max(1, std::stoi(columnText));
    }
    std::string alignRaw = RoboXMLAttr(node, "align");
    if (alignRaw.empty()) {
        alignRaw = RoboXMLAttr(node, "alignitems");
    }
    std::string justifyRaw = RoboXMLAttr(node, "justify");
    if (justifyRaw.empty()) {
        justifyRaw = RoboXMLAttr(node, "justifycontent");
    }
    const std::string place = Lower(RoboXMLAttr(node, "place"));
    if (!place.empty()) {
        const bool top = place.rfind("top", 0) == 0;
        const bool bottom = place.rfind("bottom", 0) == 0;
        const bool left = place.find("left") != std::string::npos || place == "left";
        const bool right = place.find("right") != std::string::npos || place == "right";
        if (alignRaw.empty()) {
            alignRaw = row ? (top ? "start" : bottom ? "end" : "center") : (left ? "start" : right ? "end" : "center");
        }
        if (justifyRaw.empty()) {
            justifyRaw = row ? (left ? "start" : right ? "end" : "center") : (top ? "start" : bottom ? "end" : "center");
        }
    }
    const int align = alignRaw.empty() ? 3 : PackAlign(alignRaw);
    const int justify = PackAlign(justifyRaw);
    std::string itemRaw = RoboXMLAttr(node, "justifyitems");
    const int justifyItems = itemRaw.empty() ? 3 : PackAlign(itemRaw);
    const int rowGap = RoboXMLAttr(node, "rowgap").empty() ? Gap(node) : AttrLength(node, "rowgap");
    const int colGap = !RoboXMLAttr(node, "columngap").empty() ? AttrLength(node, "columngap")
                       : !RoboXMLAttr(node, "colgap").empty()  ? AttrLength(node, "colgap")
                                                               : Gap(node);
    const std::size_t background = ops.size();
    bool hasBackground = false;
    std::string fillName = RoboXMLAttr(node, "background");
    if (fillName.empty()) {
        fillName = RoboXMLAttr(node, "backgroundcolor");
    }
    if (fillName.empty()) {
        fillName = RoboXMLAttr(node, "bg");
    }
    if (!fillName.empty()) {
        PaintOp box;
        box.x = x;
        box.y = y;
        box.width = width;
        box.radius = AttrLength(node, "radius");
        box.fill = RoboFoundationPlatformColorForTheme(fillName.c_str(), light ? ROBO_TRUE : ROBO_FALSE);
        if (box.fill != 0) {
            ops.push_back(box);
            hasBackground = true;
        }
    }
    struct Slot {
        std::size_t opBegin = 0;
        std::size_t opEnd = 0;
        int controlBegin = 0;
        int controlEnd = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int across = 0;
    };
    std::vector<Slot> slots;
    std::vector<const RoboXMLNode*> flow;
    std::vector<const RoboXMLNode*> layered;
    for (const RoboXMLNode& child : node.children) {
        const std::string position = Lower(RoboXMLAttr(child, "position"));
        if (position == "absolute" || position == "fixed" || position == "abs") {
            layered.push_back(&child);
        } else {
            flow.push_back(&child);
        }
    }
    if (reverse) {
        std::reverse(flow.begin(), flow.end());
    }
    const auto controlCount = [&]() { return hits != nullptr && hits->controls != nullptr ? static_cast<int>(hits->controls->size()) : 0; };
    const auto selfAlign = [&](const RoboXMLNode& child) {
        std::string raw = RoboXMLAttr(child, "alignself");
        if (raw.empty()) {
            raw = RoboXMLAttr(child, "self");
        }
        if (raw.empty() || Lower(raw) == "auto") {
            return align;
        }
        return PackAlign(raw);
    };
    const auto columnSpan = [&](const RoboXMLNode& child) {
        std::string raw = RoboXMLAttr(child, "columnspan");
        if (raw.empty()) {
            raw = RoboXMLAttr(child, "colspan");
        }
        if (raw.empty()) {
            const std::string gridColumn = Lower(RoboXMLAttr(child, "gridcolumn"));
            if (gridColumn.rfind("span", 0) == 0) {
                raw.clear();
                for (char ch : gridColumn) {
                    if (ch >= '0' && ch <= '9') {
                        raw.push_back(ch);
                    }
                }
            }
        }
        int span = raw.empty() || !IsDigits(raw) ? 1 : std::stoi(raw);
        if (span < 1) {
            span = 1;
        }
        return std::min(span, columns);
    };
    const auto placeChild = [&](const RoboXMLNode& child, int childX, int childY, int childWidth) {
        const std::size_t begin = ops.size();
        const int controls = controlCount();
        const int childHeight = PlaceNode(child, childX, childY, std::max(1, childWidth), light, ops, hits);
        slots.push_back(Slot{begin, ops.size(), controls, controlCount(), childX, childY, childWidth, childHeight, selfAlign(child)});
        return childHeight;
    };
    int contentBottom = originY;
    if (grid) {
        const int cell = std::max(1, (inner - (columns - 1) * colGap) / std::max(1, columns));
        int column = 0;
        int rowTop = originY;
        int rowHeight = 0;
        std::vector<int> rowSlots;
        const auto finishRow = [&](bool more) {
            for (int index : rowSlots) {
                Slot& slot = slots[static_cast<std::size_t>(index)];
                int dy = 0;
                if (slot.across == 1) {
                    dy = std::max(0, (rowHeight - slot.height) / 2);
                } else if (slot.across == 2) {
                    dy = std::max(0, rowHeight - slot.height);
                }
                if (dy != 0) {
                    ShiftRange(ops, slot.opBegin, slot.opEnd, hits, slot.controlBegin, slot.controlEnd, 0, dy);
                }
            }
            rowSlots.clear();
            rowTop += rowHeight;
            if (more) {
                rowTop += rowGap;
            }
            rowHeight = 0;
            column = 0;
        };
        for (const RoboXMLNode* child : flow) {
            const BoxEdge margin = BoxEdges(*child, "margin", "marginx", "marginy", "margintop", "marginright", "marginbottom", "marginleft", inner);
            int span = columnSpan(*child);
            if (column > 0 && column + span > columns) {
                finishRow(true);
            }
            if (span > columns) {
                span = columns;
            }
            const int cellX = originX + column * (cell + colGap);
            const int cellWidth = cell * span + colGap * std::max(0, span - 1);
            int used = std::max(1, cellWidth - margin.left - margin.right);
            const int asked = AskedLength(*child, "width", used);
            const int across = justifyItems;
            if (asked >= 0) {
                used = std::min(used, asked);
            } else if (across != 3) {
                used = std::min(used, ContentWidth(*child, used));
            }
            int dx = margin.left;
            if (across == 1) {
                dx += std::max(0, (cellWidth - margin.left - margin.right - used) / 2);
            } else if (across == 2) {
                dx += std::max(0, cellWidth - margin.left - margin.right - used);
            }
            const int childHeight = placeChild(*child, cellX + dx, rowTop + margin.top, used);
            rowHeight = std::max(rowHeight, childHeight + margin.top + margin.bottom);
            rowSlots.push_back(static_cast<int>(slots.size()) - 1);
            column += span;
            if (column >= columns) {
                finishRow(true);
            }
        }
        if (!rowSlots.empty()) {
            finishRow(false);
        } else if (!flow.empty()) {
            rowTop -= rowGap;
        }
        contentBottom = rowTop;
    } else if (row) {
        int cursorX = originX;
        int lineTop = originY;
        int lineHeight = 0;
        int lineUsed = 0;
        int lineCount = 0;
        std::vector<int> line;
        const auto finishLine = [&](bool more) {
            int free = inner - lineUsed;
            if (lineCount > 1) {
                free -= colGap * (lineCount - 1);
            }
            if (free < 0) {
                free = 0;
            }
            int lead = 0;
            int extra = 0;
            if (justify == 1) {
                lead = free / 2;
            } else if (justify == 2) {
                lead = free;
            } else if (justify == 4 && lineCount > 1) {
                extra = free / (lineCount - 1);
            } else if (justify == 5 && lineCount > 0) {
                lead = free / (lineCount * 2);
                extra = free / lineCount;
            } else if (justify == 6 && lineCount > 0) {
                lead = free / (lineCount + 1);
                extra = lead;
            }
            int shift = lead;
            for (int index : line) {
                Slot& slot = slots[static_cast<std::size_t>(index)];
                int dy = 0;
                if (slot.across == 1) {
                    dy = std::max(0, (lineHeight - slot.height) / 2);
                } else if (slot.across == 2) {
                    dy = std::max(0, lineHeight - slot.height);
                }
                ShiftRange(ops, slot.opBegin, slot.opEnd, hits, slot.controlBegin, slot.controlEnd, shift, dy);
                shift += extra;
            }
            line.clear();
            lineTop += lineHeight;
            if (more) {
                lineTop += rowGap;
            }
            lineHeight = 0;
            lineUsed = 0;
            lineCount = 0;
            cursorX = originX;
        };
        for (const RoboXMLNode* child : flow) {
            const BoxEdge margin = BoxEdges(*child, "margin", "marginx", "marginy", "margintop", "marginright", "marginbottom", "marginleft", inner);
            int used = ContentWidth(*child, std::max(1, inner - margin.left - margin.right));
            const int asked = AskedLength(*child, "width", inner);
            if (asked >= 0) {
                used = std::min(inner, asked);
            }
            const int outer = used + margin.left + margin.right;
            if (wrap && lineCount > 0 && cursorX + outer > originX + inner) {
                finishLine(true);
            }
            const int childHeight = placeChild(*child, cursorX + margin.left, lineTop + margin.top, std::max(1, used));
            lineHeight = std::max(lineHeight, childHeight + margin.top + margin.bottom);
            lineUsed += outer;
            ++lineCount;
            line.push_back(static_cast<int>(slots.size()) - 1);
            cursorX += outer + colGap;
        }
        if (!line.empty()) {
            finishLine(false);
        }
        contentBottom = lineTop;
    } else {
        int cursorY = originY;
        for (const RoboXMLNode* child : flow) {
            const BoxEdge margin = BoxEdges(*child, "margin", "marginx", "marginy", "margintop", "marginright", "marginbottom", "marginleft", inner);
            int used = std::max(0, inner - margin.left - margin.right);
            const int asked = AskedLength(*child, "width", used);
            const int across = selfAlign(*child);
            if (asked >= 0) {
                used = std::min(used, asked);
            } else if (across != 3) {
                used = std::min(used, ContentWidth(*child, used));
            }
            int dx = margin.left;
            if (across == 1) {
                dx += std::max(0, (inner - margin.left - margin.right - used) / 2);
            } else if (across == 2) {
                dx += std::max(0, inner - margin.left - margin.right - used);
            }
            const int childHeight = placeChild(*child, originX + dx, cursorY + margin.top, std::max(1, used));
            cursorY += childHeight + margin.top + margin.bottom + rowGap;
        }
        if (!flow.empty() && rowGap > 0) {
            cursorY -= rowGap;
        }
        contentBottom = cursorY;
    }
    int total = std::max(0, contentBottom - y + pad.bottom);
    const int askedHeight = AskedLength(node, "height", std::max(total, 1));
    const int minHeight = AttrLength(node, "minheight");
    int boxHeight = total;
    if (askedHeight > boxHeight) {
        boxHeight = askedHeight;
    }
    if (minHeight > boxHeight) {
        boxHeight = minHeight;
    }
    if (!grid && !row && justify != 0 && justify != 3 && boxHeight > total && !slots.empty()) {
        const int free = boxHeight - total;
        const int count = static_cast<int>(slots.size());
        int lead = 0;
        int extra = 0;
        if (justify == 1) {
            lead = free / 2;
        } else if (justify == 2) {
            lead = free;
        } else if (justify == 4 && count > 1) {
            extra = free / (count - 1);
        } else if (justify == 5 && count > 0) {
            lead = free / (count * 2);
            extra = free / count;
        } else if (justify == 6 && count > 0) {
            lead = free / (count + 1);
            extra = lead;
        }
        int shift = lead;
        for (Slot& slot : slots) {
            ShiftRange(ops, slot.opBegin, slot.opEnd, hits, slot.controlBegin, slot.controlEnd, 0, shift);
            shift += extra;
        }
    }
    const int innerHeight = std::max(0, boxHeight - pad.top - pad.bottom);
    for (const RoboXMLNode* child : layered) {
        int used = AskedLength(*child, "width", inner);
        if (used < 0) {
            used = ContentWidth(*child, inner);
        }
        const bool hasLeft = !RoboXMLAttr(*child, "left").empty();
        const bool hasRight = !RoboXMLAttr(*child, "right").empty();
        const bool hasTop = !RoboXMLAttr(*child, "top").empty();
        const bool hasBottom = !RoboXMLAttr(*child, "bottom").empty();
        int left = originX;
        int top = originY;
        if (hasLeft) {
            left = originX + MeasureLength(RoboXMLAttr(*child, "left"), inner);
        } else if (!RoboXMLAttr(*child, "x").empty()) {
            left = originX + MeasureLength(RoboXMLAttr(*child, "x"), inner);
        }
        if (hasRight && !hasLeft) {
            left = originX + inner - MeasureLength(RoboXMLAttr(*child, "right"), inner) - used;
        }
        if (hasTop) {
            top = originY + MeasureLength(RoboXMLAttr(*child, "top"), innerHeight);
        } else if (!RoboXMLAttr(*child, "y").empty()) {
            top = originY + MeasureLength(RoboXMLAttr(*child, "y"), innerHeight);
        }
        const std::size_t begin = ops.size();
        const int controls = controlCount();
        const int childHeight = PlaceNode(*child, left, top, std::max(1, used), light, ops, hits);
        if (hasBottom && !hasTop) {
            const int target = originY + innerHeight - MeasureLength(RoboXMLAttr(*child, "bottom"), innerHeight) - childHeight;
            ShiftRange(ops, begin, ops.size(), hits, controls, controlCount(), 0, target - top);
        }
    }
    if (hasBackground && background < ops.size()) {
        ops[background].height = boxHeight;
    }
    return boxHeight;
}

int Isqrt(int value) {
    if (value <= 0) {
        return 0;
    }
    int root = value;
    int next = (root + 1) / 2;
    while (next < root) {
        root = next;
        next = (root + value / root) / 2;
    }
    return root;
}

bool InPill(int localX, int localY, int width, int height) {
    if (localX < 0 || localY < 0 || localX >= width || localY >= height || height < 2) {
        return false;
    }
    const int radius = height / 2;
    const int centerY = localY - radius;
    const int distanceY = centerY < 0 ? -centerY : centerY;
    if (distanceY > radius) {
        return false;
    }
    const int span = Isqrt(radius * radius - distanceY * distanceY);
    const int inset = radius - span;
    return localX >= inset && localX < width - inset;
}

RoboU8 PillCoverage(int localX, int localY, int width, int height) {
    if (localX < 0 || localY < 0 || localX >= width || localY >= height || height < 2) {
        return 0;
    }
    const int width4 = width * 4;
    const int radius = height * 2;
    int maxInset = 0;
    int minInset = radius;
    unsigned insideRows = 0;
    for (int sampleY = 0; sampleY < 4; ++sampleY) {
        const int y = localY * 4 + sampleY;
        const int dy = y - radius;
        const int ady = dy < 0 ? -dy : dy;
        if (ady > radius) {
            continue;
        }
        const int span = Isqrt(radius * radius - ady * ady);
        const int inset = radius - span;
        if (inset > maxInset) {
            maxInset = inset;
        }
        if (inset < minInset) {
            minInset = inset;
        }
        ++insideRows;
    }
    if (insideRows == 0) {
        return 0;
    }
    const int x0 = localX * 4;
    if (insideRows == 4 && x0 >= maxInset && x0 + 3 < width4 - maxInset) {
        return 255;
    }
    if (x0 + 3 < minInset || x0 >= width4 - minInset) {
        return 0;
    }
    unsigned hits = 0;
    for (int sampleY = 0; sampleY < 4; ++sampleY) {
        for (int sampleX = 0; sampleX < 4; ++sampleX) {
            if (InPill(x0 + sampleX, localY * 4 + sampleY, width4, height * 4)) {
                ++hits;
            }
        }
    }
    return static_cast<RoboU8>((hits * 255u) / 16u);
}

void FillPill(RoboGPU* gpu, int x, int y, int width, int height, RoboU32 color, int coverage) {
    if (gpu == nullptr || width <= 0 || height <= 0 || coverage <= 0) {
        return;
    }
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const unsigned edge = PillCoverage(col, row, width, height);
            if (edge == 0) {
                continue;
            }
            RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>(edge * static_cast<unsigned>(coverage) / 255u));
        }
    }
}

void StrokePill(RoboGPU* gpu, int x, int y, int width, int height, int thickness, RoboU32 color, int coverage) {
    if (gpu == nullptr || thickness <= 0 || coverage <= 0) {
        return;
    }
    const int outerW = width + thickness * 2;
    const int outerH = height + thickness * 2;
    for (int row = 0; row < outerH; ++row) {
        for (int col = 0; col < outerW; ++col) {
            const unsigned outer = PillCoverage(col, row, outerW, outerH);
            const unsigned inner = PillCoverage(col - thickness, row - thickness, width, height);
            const unsigned edge = outer > inner ? outer - inner : 0;
            if (edge == 0) {
                continue;
            }
            RoboGPUBlendPixel(gpu, x - thickness + col, y - thickness + row, color,
                              static_cast<RoboU8>(edge * static_cast<unsigned>(coverage) / 255u));
        }
    }
}

void GlowPill(RoboGPU* gpu, int x, int y, int width, int height, RoboU32 color, int fade) {
    const int reach = std::max(3, Px(5));
    const int gapQ = Px(1) * 4;
    const int reachQ = reach * 4;
    const int span = reachQ - gapQ;
    if (span <= 0) {
        return;
    }
    const int widthQ = width * 4;
    const int heightQ = height * 4;
    const int radius = heightQ / 2;
    const int straight = widthQ - heightQ;
    for (int row = -reach; row < height + reach; ++row) {
        for (int col = -reach; col < width + reach; ++col) {
            unsigned sum = 0;
            for (int sampleY = 0; sampleY < 4; ++sampleY) {
                for (int sampleX = 0; sampleX < 4; ++sampleX) {
                    int localX = col * 4 + sampleX - radius;
                    const int localY = row * 4 + sampleY - radius;
                    if (localX < 0) {
                    } else if (straight > 0 && localX > straight) {
                        localX -= straight;
                    } else {
                        localX = 0;
                    }
                    const int outside = Isqrt(localX * localX + localY * localY) - radius;
                    if (outside < gapQ || outside >= reachQ) {
                        continue;
                    }
                    const int remain = reachQ - outside;
                    sum += static_cast<unsigned>(remain * remain * 128 / (span * span));
                }
            }
            if (sum == 0) {
                continue;
            }
            RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>(sum * static_cast<unsigned>(fade) / (16u * 255u)));
        }
    }
}

int gSpinX = 1024;
int gSpinY = 0;
int gSpinEndX = -193;
int gSpinEndY = 1006;

void RotateSpin(int& x, int& y) {
    const int cosine = 974;
    const int sine = 316;
    const int nextX = (x * cosine - y * sine) / 1024;
    const int nextY = (x * sine + y * cosine) / 1024;
    const int length = Isqrt(nextX * nextX + nextY * nextY);
    if (length <= 0) {
        x = 1024;
        y = 0;
        return;
    }
    x = nextX * 1024 / length;
    y = nextY * 1024 / length;
}

bool SpinArc(int quarterX, int quarterY) {
    const int fromStart = gSpinX * quarterY - gSpinY * quarterX;
    const int toEnd = quarterX * gSpinEndY - quarterY * gSpinEndX;
    return fromStart >= 0 && toEnd >= 0;
}

void DrawSpin(RoboGPU* gpu, int centerX, int centerY, RoboU32 track, int trackCoverage, RoboU32 tick, int tickCoverage,
              int sizePx) {
    const int size = sizePx > 0 ? sizePx : 16;
    const int radiusQ = 7 * size / 4;
    int halfQ = size / 8;
    if (halfQ < 1) {
        halfQ = 1;
    }
    const int outer = radiusQ + halfQ;
    const int inner = radiusQ > halfQ ? radiusQ - halfQ : 1;
    const int outer2 = outer * outer;
    const int inner2 = inner * inner;
    const int capR2 = halfQ * halfQ;
    const int capX = gSpinX * radiusQ / 1024;
    const int capY = gSpinY * radiusQ / 1024;
    const int capEndX = gSpinEndX * radiusQ / 1024;
    const int capEndY = gSpinEndY * radiusQ / 1024;
    const int extent = size / 2 + 1;
    if (trackCoverage < 0) {
        trackCoverage = 0;
    }
    if (tickCoverage < 0) {
        tickCoverage = 0;
    }
    if (trackCoverage > 255) {
        trackCoverage = 255;
    }
    if (tickCoverage > 255) {
        tickCoverage = 255;
    }
    for (int row = -extent; row <= extent; ++row) {
        for (int col = -extent; col <= extent; ++col) {
            unsigned trackHits = 0;
            unsigned tickHits = 0;
            for (int sampleY = 0; sampleY < 4; ++sampleY) {
                for (int sampleX = 0; sampleX < 4; ++sampleX) {
                    const int quarterX = col * 4 + sampleX - 2;
                    const int quarterY = row * 4 + sampleY - 2;
                    const int distance = quarterX * quarterX + quarterY * quarterY;
                    const bool ring = distance >= inner2 && distance <= outer2;
                    const int startCap = (quarterX - capX) * (quarterX - capX) + (quarterY - capY) * (quarterY - capY);
                    const int endCap = (quarterX - capEndX) * (quarterX - capEndX) + (quarterY - capEndY) * (quarterY - capEndY);
                    const bool cap = startCap <= capR2 || endCap <= capR2;
                    if (!ring && !cap) {
                        continue;
                    }
                    if ((ring && SpinArc(quarterX, quarterY)) || cap) {
                        ++tickHits;
                    } else if (ring) {
                        ++trackHits;
                    }
                }
            }
            if (trackHits != 0) {
                RoboGPUBlendPixel(gpu, centerX + col, centerY + row, track,
                                  static_cast<RoboU8>(trackCoverage * static_cast<int>(trackHits) / 16));
            }
            if (tickHits != 0) {
                RoboGPUBlendPixel(gpu, centerX + col, centerY + row, tick,
                                  static_cast<RoboU8>(tickCoverage * static_cast<int>(tickHits) / 16));
            }
        }
    }
}

int CenteredTextTop(int top, int boxHeight, int fontSize) {
    if (fontSize <= 0) {
        return top;
    }
    const int mid = top + boxHeight / 2;
    const int baseline = mid + fontSize * 745 / 2048;
    return baseline - fontSize;
}

void PaintSpin(RoboGPU* gpu, int centerX, int centerY, int size, RoboU32 ink, int fade, bool light) {
    const RoboU32 tick = ink != 0 ? ink : ThemeColor("brand-primary", light);
    int cover = fade > 0 ? fade : 255;
    DrawSpin(gpu, centerX, centerY, tick, cover * 35 / 100, tick, cover, size);
}

void PaintOne(RoboGPU* gpu, const PaintOp& op, int scrollY, int viewHeight, bool light) {
    const int y = op.y - scrollY;
    if (y + op.height < 0 || y > viewHeight) {
        return;
    }
    if (op.button) {
            if (op.fill != 0) {
                int cover = op.fade;
                const unsigned alpha = (op.fill >> 24) & 255u;
                if (alpha > 0 && alpha < 255) {
                    cover = cover * static_cast<int>(alpha) / 255;
                }
                FillPill(gpu, op.x, y, op.width, op.height, op.fill, cover);
            }
            if (op.borderPx > 0 && op.border != 0) {
                StrokePill(gpu, op.x, y, op.width, op.height, op.borderPx, op.border, op.fade);
            }
        } else if (!op.text && op.fill != 0) {
            FillRound(gpu, op.x, y, op.width, op.height, op.radius, op.fill);
        }
        if (op.drawing && op.loading) {
            const int size = std::max(8, std::min(op.width, op.height));
            PaintSpin(gpu, op.x + op.width / 2, y + op.height / 2, size, op.color, op.fade, light);
            return;
        }
        if (op.drawing && !op.drawingSvg.empty()) {
            PaintWindowsSvg(gpu, op.x, y, op.width, op.drawingSvg, op.color);
            return;
        }
        if (op.value.empty() && op.leadingSvg.empty() && op.trailingSvg.empty()) {
            return;
        }
        RoboFontHeader header{};
        const RoboFontView font = FaceFont(op.face, op.size, header);
        int textX = op.x;
        int textY = y;
        if (op.button) {
            const int icon = Px(16);
            const int gap = Px(8);
            const int measured = op.value.empty() ? 0 : MeasureText(op.value, op.size, op.face);
            int content = measured;
            if (!op.leadingSvg.empty()) {
                content += icon + gap;
            }
            if (!op.trailingSvg.empty()) {
                content += (content > 0 ? gap : 0) + icon;
            }
            int cursor = op.x + std::max(0, (op.width - content) / 2);
            const int mid = y + op.height / 2;
            if (op.loading && op.leadingSvg.empty() && op.value.empty()) {
                const int spin = std::max(8, std::min(op.width, op.height) - 4);
                PaintSpin(gpu, op.x + op.width / 2, mid, spin, op.color, op.fade, light);
            } else if (!op.leadingSvg.empty() || op.loading) {
                if (op.loading) {
                    PaintSpin(gpu, cursor + icon / 2, mid, icon, op.color, op.fade, light);
                    cursor += icon + gap;
                } else {
                    PaintWindowsSvg(gpu, cursor, mid - icon / 2, icon, op.leadingSvg, op.color);
                    cursor += icon + gap;
                }
            }
            textX = cursor;
            textY = CenteredTextTop(y, op.height, op.size);
            if (!op.trailingSvg.empty()) {
                PaintWindowsSvg(gpu, cursor + measured + gap, mid - icon / 2, icon, op.trailingSvg, op.color);
            }
        } else if (!op.text) {
            const int measured = MeasureText(op.value, op.size, op.face);
            textX = op.x + std::max(0, (op.width - measured) / 2);
            textY = CenteredTextTop(y, op.height, op.size);
        }
        if (!op.value.empty()) {
            if (op.text) {
                std::vector<TextLine> lines;
                TextFlow flow;
                flow.mode = op.wrap;
                flow.maxLines = op.maxLines;
                flow.overflow = op.overflow;
                flow.whiteSpace = op.whiteSpace;
                flow.breakLong = op.breakLong;
                TextLines(op.value, op.width, op.size, op.face, flow, lines);
                const int step = std::max(1, op.height / static_cast<int>(std::max<std::size_t>(1, lines.size())));
                int lineY = y;
                for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
                    const TextLine& line = lines[lineIndex];
                    int lineX = textX;
                    if (op.align == 1) {
                        lineX = op.x + std::max(0, (op.width - line.width) / 2);
                    } else if (op.align == 2) {
                        lineX = op.x + std::max(0, op.width - line.width);
                    }
                    const bool justify = op.align == 3 && lineIndex + 1 < lines.size() && line.width < op.width &&
                                         line.text.find(' ') != std::string::npos;
                    if (!line.text.empty() && justify) {
                        int gaps = 0;
                        for (char ch : line.text) {
                            if (ch == ' ') {
                                ++gaps;
                            }
                        }
                        const int extra = gaps > 0 ? (op.width - line.width) / gaps : 0;
                        int cursor = textX;
                        std::string word;
                        const auto drawWord = [&]() {
                            if (word.empty()) {
                                return;
                            }
                            RoboTextDrawUTF8(gpu, font, cursor, lineY, word.c_str(), op.color, static_cast<RoboU16>(op.size));
                            cursor += MeasureText(word, op.size, op.face);
                            word.clear();
                        };
                        for (char ch : line.text) {
                            if (ch == ' ') {
                                drawWord();
                                cursor += MeasureText(" ", op.size, op.face) + extra;
                            } else {
                                word.push_back(ch);
                            }
                        }
                        drawWord();
                    } else if (!line.text.empty()) {
                        RoboTextDrawUTF8(gpu, font, lineX, lineY, line.text.c_str(), op.color, static_cast<RoboU16>(op.size));
                    }
                    lineY += step;
                }
            } else {
                RoboTextDrawUTF8(gpu, font, textX, textY, op.value.c_str(), op.color, static_cast<RoboU16>(op.size));
            }
        }
}

void PaintOps(RoboGPU* gpu, const std::vector<PaintOp>& ops, int scrollY, int viewHeight, bool light) {
    for (const PaintOp& op : ops) {
        PaintOne(gpu, op, scrollY, viewHeight, light);
    }
}

struct SceneRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct Scene {
    bool valid = false;
    std::string path;
    std::filesystem::file_time_type stamp{};
    std::uintmax_t bytes = 0;
    bool light = false;
    unsigned dpi = 96;
    int viewWidth = 0;
    int viewHeight = 0;
    int scrollY = 0;
    int hot = -1;
    int pressed = -1;
    int focus = -1;
    int contentHeight = 0;
    int zoomAnchor = 0;
    RoboU32 background = 0;
    std::vector<PaintOp> ops;
    std::vector<WindowControl> controls;
};

Scene gScene;

bool SceneStamp(const std::string& path, std::filesystem::file_time_type& stamp, std::uintmax_t& bytes) {
    std::error_code error;
    stamp = std::filesystem::last_write_time(path, error);
    if (error) {
        return false;
    }
    bytes = std::filesystem::file_size(path, error);
    return !error;
}

bool SceneCurrent(const std::string& path, bool light, unsigned dpi, int viewWidth, int viewHeight) {
    if (!gScene.valid || gScene.path != path || gScene.light != light || gScene.dpi != dpi || gScene.viewWidth != viewWidth ||
        gScene.viewHeight != viewHeight) {
        return false;
    }
    std::filesystem::file_time_type stamp{};
    std::uintmax_t bytes = 0;
    return SceneStamp(path, stamp, bytes) && stamp == gScene.stamp && bytes == gScene.bytes;
}

void PublishControls(int* contentHeight, std::vector<WindowControl>* controls, int viewHeight) {
    if (contentHeight != nullptr) {
        *contentHeight = std::max(viewHeight, gScene.contentHeight);
    }
    if (controls != nullptr) {
        *controls = gScene.controls;
    }
}

SceneRect OpBounds(const PaintOp& op, int scrollY) {
    SceneRect bounds{op.x, op.y - scrollY, op.width, op.height};
    if (op.button) {
        const int pad = std::max(op.borderPx, Px(14));
        bounds.x -= pad;
        bounds.y -= pad;
        bounds.width += pad * 2;
        bounds.height += pad * 2;
    }
    return bounds;
}

bool BoundsHit(const SceneRect& bounds, const SceneRect& rect) {
    return bounds.width > 0 && bounds.height > 0 && rect.width > 0 && rect.height > 0 && bounds.x < rect.x + rect.width &&
           bounds.x + bounds.width > rect.x && bounds.y < rect.y + rect.height && bounds.y + bounds.height > rect.y;
}

void ClipRect(SceneRect& rect, int viewWidth, int viewHeight) {
    if (rect.x < 0) {
        rect.width += rect.x;
        rect.x = 0;
    }
    if (rect.y < 0) {
        rect.height += rect.y;
        rect.y = 0;
    }
    if (rect.x + rect.width > viewWidth) {
        rect.width = viewWidth - rect.x;
    }
    if (rect.y + rect.height > viewHeight) {
        rect.height = viewHeight - rect.y;
    }
}

void GrowRect(SceneRect& rect, int scrollY, int viewWidth, int viewHeight) {
    for (int pass = 0; pass < 6; ++pass) {
        bool grew = false;
        for (const PaintOp& op : gScene.ops) {
            const SceneRect bounds = OpBounds(op, scrollY);
            if (!BoundsHit(bounds, rect)) {
                continue;
            }
            const int x1 = std::max(rect.x + rect.width, bounds.x + bounds.width);
            const int y1 = std::max(rect.y + rect.height, bounds.y + bounds.height);
            const int x0 = std::min(rect.x, bounds.x);
            const int y0 = std::min(rect.y, bounds.y);
            if (x0 != rect.x || y0 != rect.y || x1 != rect.x + rect.width || y1 != rect.y + rect.height) {
                rect = SceneRect{x0, y0, x1 - x0, y1 - y0};
                grew = true;
            }
        }
        if (!grew) {
            break;
        }
    }
    ClipRect(rect, viewWidth, viewHeight);
}

void ReplayRect(RoboGPU* gpu, const SceneRect& rect, int scrollY, int viewHeight, bool light) {
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }
    RoboGPUFillRect(gpu, rect.x, rect.y, rect.width, rect.height, gScene.background);
    for (const PaintOp& op : gScene.ops) {
        if (BoundsHit(OpBounds(op, scrollY), rect)) {
            PaintOne(gpu, op, scrollY, viewHeight, light);
        }
    }
}

void ShiftScene(RoboGPU* gpu, int delta, int viewWidth, int viewHeight) {
    if (delta == 0 || viewWidth <= 0 || viewHeight <= 0) {
        return;
    }
    const int distance = delta < 0 ? -delta : delta;
    if (distance >= viewHeight) {
        return;
    }
    if (delta > 0) {
        RoboGPUCopyArea(gpu, 0, 0, 0, delta, viewWidth, viewHeight - delta);
    } else {
        RoboGPUCopyArea(gpu, 0, distance, 0, 0, viewWidth, viewHeight - distance);
    }
    RoboDamageClear(&gpu->damage);
}

bool BuildScene(RoboGPU* gpu, const std::string& path, bool light, int viewWidth, int viewHeight, int scrollY,
                int hotControl, int pressedControl, int pointerX, int pointerY) {
    const std::string source = ReadText(path);
    RoboXMLDocument document;
    if (source.empty() || !RoboXMLParse(source, document)) {
        gScene.valid = false;
        return false;
    }
    RoboUICompose(document.root, path);
    LowerKeys(document.root);
    const RoboXMLNode* screen = FindScreen(document.root);
    if (screen == nullptr) {
        gScene.valid = false;
        return false;
    }
    std::vector<WindowControl> controls;
    HitState hits;
    hits.controls = &controls;
    hits.hot = hotControl;
    hits.pressed = pressedControl;
    hits.pointerX = pointerX;
    hits.pointerY = pointerY;
    std::vector<PaintOp> ops;
    const int height = PlaceNode(*screen, 0, 0, viewWidth, light, ops, &hits);
    gScene.ops = std::move(ops);
    gScene.controls = std::move(controls);
    gScene.contentHeight = height;
    gScene.scrollY = scrollY;
    gScene.hot = hotControl;
    gScene.pressed = pressedControl;
    gScene.focus = -1;
    gScene.path = path;
    gScene.light = light;
    gScene.dpi = gDpi;
    gScene.viewWidth = viewWidth;
    gScene.viewHeight = viewHeight;
    std::string zoomPlace = RoboXMLAttr(*screen, "zoomcontrols");
    if (zoomPlace.empty()) {
        zoomPlace = RoboXMLAttr(*screen, "zoom-controls");
    }
    gScene.zoomAnchor = ViewZoom::Anchor(zoomPlace.c_str());
    gScene.background = RoboFoundationPlatformColorForTheme("bg-primary", light ? ROBO_TRUE : ROBO_FALSE);
    gScene.valid = SceneStamp(path, gScene.stamp, gScene.bytes);
    if (gpu != nullptr) {
        RoboGPUClear(gpu, gScene.background);
        PaintOps(gpu, gScene.ops, scrollY, viewHeight, light);
    }
    return true;
}

void PaintSceneRect(RoboGPU* gpu, SceneRect rect, int scrollY, int viewHeight, bool light) {
    GrowRect(rect, scrollY, gScene.viewWidth, viewHeight);
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }
    const long long area = static_cast<long long>(rect.width) * rect.height;
    const long long full = static_cast<long long>(gScene.viewWidth) * gScene.viewHeight;
    if (full > 0 && area * 5 > full * 3) {
        RoboGPUClear(gpu, gScene.background);
        PaintOps(gpu, gScene.ops, scrollY, viewHeight, light);
        return;
    }
    ReplayRect(gpu, rect, scrollY, viewHeight, light);
}

}  // namespace

void RestoreWindowsDocumentRect(RoboGPU* gpu, int x, int y, int width, int height) {
    if (gpu == nullptr || !gScene.valid || width <= 0 || height <= 0) {
        return;
    }
    gDpi = gScene.dpi;
    PaintSceneRect(gpu, SceneRect{x, y, width, height}, gScene.scrollY, gScene.viewHeight, gScene.light);
}

void PaintWindowsDocument(RoboGPU* gpu, const std::string& path, bool light, int viewWidth, int viewHeight, int scrollY,
                          int* contentHeight, std::vector<WindowControl>* controls, int hotControl, int pressedControl,
                          unsigned dpi, int pointerX, int pointerY) {
    gDpi = dpi == 0 ? 96u : dpi;
    gAssets = FindAssets(path);
    if (gpu == nullptr) {
        return;
    }
    RoboWindowsInstallFontRaster(nullptr, 0);
    if (path.empty() || viewWidth <= 0 || viewHeight <= 0) {
        RoboGPUClear(gpu, RoboFoundationPlatformColorForTheme("bg-primary", light ? ROBO_TRUE : ROBO_FALSE));
        if (contentHeight != nullptr) {
            *contentHeight = viewHeight;
        }
        return;
    }
    if (!SceneCurrent(path, light, gDpi, viewWidth, viewHeight)) {
        if (!BuildScene(gpu, path, light, viewWidth, viewHeight, scrollY, hotControl, pressedControl, pointerX, pointerY)) {
            RoboGPUClear(gpu, RoboFoundationPlatformColorForTheme("bg-primary", light ? ROBO_TRUE : ROBO_FALSE));
            return;
        }
        PublishControls(contentHeight, controls, viewHeight);
        return;
    }
    const int delta = scrollY - gScene.scrollY;
    if (delta != 0 && (delta >= viewHeight || -delta >= viewHeight)) {
        gScene.scrollY = scrollY;
        RoboGPUClear(gpu, gScene.background);
        PaintOps(gpu, gScene.ops, scrollY, viewHeight, light);
    } else if (delta != 0) {
        ShiftScene(gpu, delta, viewWidth, viewHeight);
        gScene.scrollY = scrollY;
        SceneRect exposed{0, delta > 0 ? viewHeight - delta : 0, viewWidth, delta > 0 ? delta : -delta};
        PaintSceneRect(gpu, exposed, scrollY, viewHeight, light);
    }
    if (gScene.hot != hotControl || gScene.pressed != pressedControl) {
        const int changed[4] = {gScene.hot, hotControl, gScene.pressed, pressedControl};
        for (int index : changed) {
            if (index < 0) {
                continue;
            }
            for (PaintOp& op : gScene.ops) {
                if (!op.button || op.control != index) {
                    continue;
                }
                if (op.iconButton) {
                    StyleIconButton(op, light, index == hotControl, index == pressedControl, false);
                } else {
                    RecolorButton(op, light, index == hotControl, index == pressedControl, false);
                }
                PaintSceneRect(gpu, OpBounds(op, scrollY), scrollY, viewHeight, light);
            }
        }
        gScene.hot = hotControl;
        gScene.pressed = pressedControl;
    }
    PublishControls(contentHeight, controls, viewHeight);
}

int LayoutWindowsDocument(const std::string& path, bool light, int viewWidth, int viewHeight, unsigned dpi, int hotControl,
                          int pressedControl, int pointerX, int pointerY, int* contentHeight,
                          std::vector<WindowControl>* controls) {
    RoboWindowsInstallFontRaster(nullptr, 0);
    gDpi = dpi == 0 ? 96u : dpi;
    gAssets = FindAssets(path);
    if (path.empty() || viewWidth <= 0 || viewHeight <= 0) {
        gScene.valid = false;
        if (contentHeight != nullptr) {
            *contentHeight = viewHeight;
        }
        if (controls != nullptr) {
            controls->clear();
        }
        return -1;
    }
    if (SceneCurrent(path, light, gDpi, viewWidth, viewHeight)) {
        if (contentHeight != nullptr) {
            *contentHeight = std::max(viewHeight, gScene.contentHeight);
        }
        return 0;
    }
    if (!BuildScene(nullptr, path, light, viewWidth, viewHeight, 0, hotControl, pressedControl, pointerX, pointerY)) {
        if (contentHeight != nullptr) {
            *contentHeight = viewHeight;
        }
        if (controls != nullptr) {
            controls->clear();
        }
        return -1;
    }
    if (contentHeight != nullptr) {
        *contentHeight = std::max(viewHeight, gScene.contentHeight);
    }
    if (controls != nullptr) {
        *controls = gScene.controls;
    }
    return 1;
}

void PaintWindowsDocumentView(RoboGPU* gpu, int scrollY, int viewWidth, int viewHeight) {
    RoboWindowsInstallFontRaster(nullptr, 0);
    if (gpu == nullptr || !gScene.valid || viewWidth <= 0 || viewHeight <= 0) {
        return;
    }
    RoboGPUClear(gpu, gScene.background);
    PaintOps(gpu, gScene.ops, scrollY, viewHeight, gScene.light);
    gScene.scrollY = scrollY;
}

void RasterWindowsDocument(RoboGPU* gpu) {
    RoboWindowsInstallFontRaster(nullptr, 0);
    if (gpu == nullptr || !gScene.valid) {
        return;
    }
    const int height = std::max(gScene.contentHeight, gScene.viewHeight);
    RoboGPUClear(gpu, gScene.background);
    PaintOps(gpu, gScene.ops, 0, height, gScene.light);
    RoboDamageClear(&gpu->damage);
}

int FocusReach() {
    return std::max(1, Px(2)) + std::max(1, Px(2)) + std::max(2, Px(5));
}

RoboU32 ReadPixel(const RoboGPU* gpu, int x, int y) {
    if (gpu == nullptr || gpu->back == nullptr || x < 0 || y < 0 || x >= static_cast<int>(gpu->info.width) ||
        y >= static_cast<int>(gpu->info.height)) {
        return gScene.background;
    }
    const auto* row = reinterpret_cast<const RoboU32*>(RoboFrameBufferRow(gpu->back, RoboGPUPitch(gpu), y));
    return row[x];
}

void WritePixel(RoboGPU* gpu, int x, int y, RoboU32 color) {
    if (gpu == nullptr || gpu->back == nullptr || x < 0 || y < 0 || x >= static_cast<int>(gpu->info.width) ||
        y >= static_cast<int>(gpu->info.height)) {
        return;
    }
    auto* row = reinterpret_cast<RoboU32*>(RoboFrameBufferRow(gpu->back, RoboGPUPitch(gpu), y));
    row[x] = color;
}

void ClearButtonShape(RoboGPU* gpu, const PaintOp& op) {
    const int pad = std::max(1, op.borderPx);
    const RoboU32 backdrop = ReadPixel(gpu, op.x, op.y);
    const int outerW = op.width + pad * 2;
    const int outerH = op.height + pad * 2;
    for (int row = 0; row < outerH; ++row) {
        for (int col = 0; col < outerW; ++col) {
            if (PillCoverage(col, row, outerW, outerH) == 0) {
                continue;
            }
            WritePixel(gpu, op.x - pad + col, op.y - pad + row, backdrop);
        }
    }
}

bool OpInside(const PaintOp& op, const SceneRect& rect) {
    return op.width > 0 && op.height > 0 && op.x >= rect.x - 1 && op.y >= rect.y - 1 &&
           op.x + op.width <= rect.x + rect.width + 1 && op.y + op.height <= rect.y + rect.height + 1;
}

bool OpHits(const PaintOp& op, const SceneRect& rect) {
    return op.width > 0 && op.height > 0 && op.x < rect.x + rect.width && op.x + op.width > rect.x &&
           op.y < rect.y + rect.height && op.y + op.height > rect.y;
}

int CollectWindowsDocumentPatches(int hotControl, int pressedControl, int focusControl, WindowPatch* patches, int capacity) {
    if (!gScene.valid || (gScene.hot == hotControl && gScene.pressed == pressedControl && gScene.focus == focusControl)) {
        return 0;
    }
    int count = 0;
    bool overflow = false;
    const int changed[6] = {gScene.hot, hotControl, gScene.pressed, pressedControl, gScene.focus, focusControl};
    const int reach = FocusReach();
    for (int index : changed) {
        if (index < 0) {
            continue;
        }
        for (PaintOp& op : gScene.ops) {
            if (!op.button || op.control != index) {
                continue;
            }
            const bool focused = index == focusControl;
            if (op.iconButton) {
                StyleIconButton(op, gScene.light, index == hotControl, index == pressedControl, focused);
            } else {
                RecolorButton(op, gScene.light, index == hotControl, index == pressedControl, focused);
            }
            if (patches == nullptr || count >= capacity) {
                overflow = true;
                continue;
            }
            patches[count].x = op.x - reach;
            patches[count].y = op.y - reach;
            patches[count].width = op.width + reach * 2;
            patches[count].height = op.height + reach * 2;
            ++count;
        }
    }
    gScene.hot = hotControl;
    gScene.pressed = pressedControl;
    gScene.focus = focusControl;
    return overflow ? -1 : count;
}

void PaintWindowsDocumentRect(RoboGPU* gpu, int x, int y, int width, int height) {
    if (gpu == nullptr || !gScene.valid || width <= 0 || height <= 0) {
        return;
    }
    const int limit = std::max(gScene.contentHeight, gScene.viewHeight);
    SceneRect rect{x, y, width, height};
    ClipRect(rect, gScene.viewWidth, limit);
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }
    for (const PaintOp& op : gScene.ops) {
        if (op.button && OpHits(op, rect)) {
            ClearButtonShape(gpu, op);
        } else if (!op.button && !op.text && OpInside(op, rect)) {
            const RoboU32 backdrop = ReadPixel(gpu, op.x, op.y);
            for (int row = 0; row < op.height; ++row) {
                for (int col = 0; col < op.width; ++col) {
                    WritePixel(gpu, op.x + col, op.y + row, backdrop);
                }
            }
        }
    }
    for (const PaintOp& op : gScene.ops) {
        if ((op.button && OpHits(op, rect)) || (!op.button && OpInside(op, rect))) {
            PaintOne(gpu, op, 0, limit, gScene.light);
        }
    }
    RoboDamageClear(&gpu->damage);
}

void PaintWindowsFocusRing(RoboGPU* gpu, int x, int y, int width, int height) {
    if (gpu == nullptr || width <= 0 || height <= 0) {
        return;
    }
    const int offset = std::max(1, Px(2));
    const int thick = std::max(1, Px(2));
    const int blur = std::max(2, Px(5));
    const int reach = offset + thick + blur;
    const RoboU32 brand = ThemeColor("brand-primary", gScene.light);
    const int widthQ = width * 4;
    const int heightQ = height * 4;
    const int radius = heightQ / 2;
    const int straight = widthQ - heightQ;
    const auto outsideOf = [&](int localX, int localY) {
        if (localX < 0) {
        } else if (straight > 0 && localX > straight) {
            localX -= straight;
        } else {
            localX = 0;
        }
        return Isqrt(localX * localX + localY * localY) - radius;
    };
    const int blurQ = blur * 4;
    const int ring0 = offset * 4;
    const int ring1 = (offset + thick) * 4;
    for (int row = -reach; row < height + reach; ++row) {
        for (int col = -reach; col < width + reach; ++col) {
            unsigned shadow = 0;
            unsigned ring = 0;
            for (int sampleY = 0; sampleY < 4; ++sampleY) {
                for (int sampleX = 0; sampleX < 4; ++sampleX) {
                    const int outside = outsideOf(col * 4 + sampleX - radius, row * 4 + sampleY - radius);
                    if (outside >= 0 && outside < blurQ) {
                        shadow += static_cast<unsigned>(blurQ - outside);
                    }
                    if (outside >= ring0 && outside < ring1) {
                        ++ring;
                    }
                }
            }
            if (shadow != 0) {
                const int cover = static_cast<int>(shadow) * 92 / (16 * blurQ);
                if (cover > 0) {
                    RoboGPUBlendPixel(gpu, x + col, y + row, brand, static_cast<RoboU8>(cover > 92 ? 92 : cover));
                }
            }
            if (ring != 0) {
                const int cover = static_cast<int>(ring) * 122 / 16;
                RoboGPUBlendPixel(gpu, x + col, y + row, brand, static_cast<RoboU8>(cover > 122 ? 122 : cover));
            }
        }
    }
    RoboGPUMarkDamage(gpu, x - reach, y - reach, width + reach * 2, height + reach * 2);
}

bool WindowsDocumentNeedsSpin() {
    if (!gScene.valid) {
        return false;
    }
    for (const PaintOp& op : gScene.ops) {
        if (op.loading) {
            return true;
        }
    }
    return false;
}

void WindowsDocumentAdvanceSpin() {
    RotateSpin(gSpinX, gSpinY);
    RotateSpin(gSpinEndX, gSpinEndY);
}

int WindowsDocumentSpinRects(WindowPatch* patches, int capacity) {
    if (!gScene.valid || patches == nullptr || capacity <= 0) {
        return 0;
    }
    int count = 0;
    for (const PaintOp& op : gScene.ops) {
        if (!op.loading || op.width <= 0 || op.height <= 0) {
            continue;
        }
        if (count >= capacity) {
            break;
        }
        patches[count].x = op.x;
        patches[count].y = op.y;
        patches[count].width = op.width;
        patches[count].height = op.height;
        ++count;
    }
    return count;
}

int WindowsDocumentZoomAnchor() { return gScene.valid ? gScene.zoomAnchor : 0; }

std::string WindowsDocumentIconSvg(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return {};
    }
    return RoboWebSvg(gAssets, name);
}

}  // namespace Robix::UI
