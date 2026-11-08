#include "RoboCompilerWebShared.hpp"

#include "RoboCompilerShared.hpp"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

std::string RoboWebLower(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::string RoboWebEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        if (ch == '&') {
            out += "&amp;";
        } else if (ch == '<') {
            out += "&lt;";
        } else if (ch == '>') {
            out += "&gt;";
        } else if (ch == '"') {
            out += "&quot;";
        } else {
            out += ch;
        }
    }
    return out;
}

const RoboFoundationToken* RoboWebToken(const RoboFoundationLibrary& library, const std::string& name) {
    const std::string key = RoboWebLower(name);
    for (const RoboFoundationSet& set : library.sets) {
        const auto it = set.tokens.find(key);
        if (it != set.tokens.end()) {
            return &it->second;
        }
    }
    return nullptr;
}

int RoboWebHexByte(char high, char low) {
    const auto nibble = [](char ch) {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return ch - 'a' + 10;
        }
        if (ch >= 'A' && ch <= 'F') {
            return ch - 'A' + 10;
        }
        return 0;
    };
    return nibble(high) * 16 + nibble(low);
}

std::string RoboWebColor(const std::string& value) {
    if (value.size() == 9 && value[0] == '#') {
        const int alpha = RoboWebHexByte(value[1], value[2]);
        const int red = RoboWebHexByte(value[3], value[4]);
        const int green = RoboWebHexByte(value[5], value[6]);
        const int blue = RoboWebHexByte(value[7], value[8]);
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "rgba(%d,%d,%d,%.3f)", red, green, blue, alpha / 255.0);
        return buffer;
    }
    return value;
}

bool RoboWebIsNumber(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    std::size_t index = (text[0] == '-' || text[0] == '+') ? 1 : 0;
    bool sawDigit = false;
    bool sawDot = false;
    for (; index < text.size(); ++index) {
        const char ch = text[index];
        if (ch >= '0' && ch <= '9') {
            sawDigit = true;
            continue;
        }
        if (ch == '.' && !sawDot) {
            sawDot = true;
            continue;
        }
        return false;
    }
    return sawDigit;
}

std::string RoboWebCssValue(const RoboFoundationLibrary& library, const std::string& raw, bool color) {
    if (raw.empty()) {
        return {};
    }
    if (const RoboFoundationToken* token = RoboWebToken(library, raw)) {
        if (token->kind == RoboFoundationKind::Color || color) {
            return "var(--" + token->name + ")";
        }
        if (token->kind == RoboFoundationKind::Space || token->kind == RoboFoundationKind::Radius) {
            return "var(--" + token->name + ")";
        }
        if (token->kind == RoboFoundationKind::Type) {
            return "var(--" + token->name + "-size)";
        }
    }
    if (color || raw[0] == '#') {
        return RoboWebColor(raw);
    }
    if (RoboWebIsNumber(raw)) {
        return raw + "px";
    }
    return raw;
}

std::string RoboWebAlign(const std::string& value) {
    const std::string key = RoboWebLower(value);
    if (key == "start") {
        return "flex-start";
    }
    if (key == "end") {
        return "flex-end";
    }
    return value;
}

void RoboWebAddDecl(std::string& style, const char* property, const std::string& value) {
    if (value.empty()) {
        return;
    }
    if (!style.empty()) {
        style += ";";
    }
    style += property;
    style += ":";
    style += value;
}

namespace fs = std::filesystem;

std::string RoboWebRead(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::string RoboWebStyle(const RoboXMLNode& node, const RoboFoundationLibrary& library) {
    std::string style;
    const auto attr = [&](const char* key) { return RoboXMLAttr(node, key); };
    const auto first = [&](const char* key, const char* alt) {
        const std::string value = attr(key);
        return value.empty() ? attr(alt) : value;
    };
    const auto length = [&](const std::string& value) { return RoboWebCssValue(library, value, false); };
    const std::string displayRaw = RoboWebLower(attr("display"));
    std::string direction = first("direction", "flexDirection");
    const std::string columns = attr("columns");
    const std::string rows = attr("rows");
    const std::string templateColumns = first("gridTemplateColumns", "templateColumns");
    const std::string templateRows = first("gridTemplateRows", "templateRows");
    const bool gridAsked = displayRaw == "grid" || !columns.empty() || !rows.empty() || !templateColumns.empty() ||
                           !templateRows.empty();
    if (displayRaw == "horizontal") {
        RoboWebAddDecl(style, "display", "flex");
        RoboWebAddDecl(style, "flex-direction", "row");
    } else if (displayRaw == "vertical") {
        RoboWebAddDecl(style, "display", "flex");
        RoboWebAddDecl(style, "flex-direction", "column");
    } else if (displayRaw == "overlay" || displayRaw == "layered") {
        RoboWebAddDecl(style, "display", "grid");
    } else if (!displayRaw.empty()) {
        RoboWebAddDecl(style, "display", displayRaw);
    } else if (gridAsked) {
        RoboWebAddDecl(style, "display", "grid");
    } else if (!direction.empty()) {
        RoboWebAddDecl(style, "display", "flex");
    }
    if (!direction.empty() && displayRaw != "horizontal" && displayRaw != "vertical") {
        const std::string flow = RoboWebLower(direction);
        const char* mapped = flow.c_str();
        if (flow == "col" || flow == "column") {
            mapped = "column";
        } else if (flow == "row") {
            mapped = "row";
        } else if (flow == "row-reverse" || flow == "rowreverse") {
            mapped = "row-reverse";
        } else if (flow == "column-reverse" || flow == "col-reverse" || flow == "columnreverse") {
            mapped = "column-reverse";
        }
        RoboWebAddDecl(style, "flex-direction", mapped);
    }
    const std::string flex = attr("flex");
    if (!flex.empty()) {
        RoboWebAddDecl(style, "flex", flex);
    }
    const std::string grow = first("flexGrow", "grow");
    RoboWebAddDecl(style, "flex-grow", grow);
    const std::string shrink = first("flexShrink", "shrink");
    RoboWebAddDecl(style, "flex-shrink", shrink);
    RoboWebAddDecl(style, "flex-basis", length(first("flexBasis", "basis")));
    RoboWebAddDecl(style, "order", attr("order"));
    std::string wrap = attr("wrap");
    if (wrap.empty()) {
        wrap = attr("flexWrap");
    }
    if (!wrap.empty()) {
        const std::string flow = RoboWebLower(wrap);
        const char* value = "wrap";
        if (flow == "nowrap" || flow == "false" || flow == "0" || flow == "no") {
            value = "nowrap";
        } else if (flow == "wrap-reverse" || flow == "wrapreverse") {
            value = "wrap-reverse";
        }
        RoboWebAddDecl(style, "flex-wrap", value);
    }
    const std::string gap = attr("gap").empty() ? first("spacing", "childGap") : attr("gap");
    RoboWebAddDecl(style, "gap", length(gap));
    RoboWebAddDecl(style, "row-gap", length(attr("rowGap")));
    RoboWebAddDecl(style, "column-gap", length(first("columnGap", "colGap")));
    RoboWebAddDecl(style, "align-items", RoboWebAlign(first("align", "alignItems")));
    RoboWebAddDecl(style, "justify-content", RoboWebAlign(first("justify", "justifyContent")));
    RoboWebAddDecl(style, "align-self", RoboWebAlign(first("alignSelf", "self")));
    RoboWebAddDecl(style, "align-content", RoboWebAlign(attr("alignContent")));
    RoboWebAddDecl(style, "justify-items", RoboWebAlign(attr("justifyItems")));
    RoboWebAddDecl(style, "justify-self", RoboWebAlign(attr("justifySelf")));
    RoboWebAddDecl(style, "place-items", attr("placeItems"));
    RoboWebAddDecl(style, "place-content", attr("placeContent"));
    RoboWebAddDecl(style, "place-self", attr("placeSelf"));
    if (RoboWebIsNumber(columns)) {
        RoboWebAddDecl(style, "grid-template-columns", "repeat(" + columns + ",minmax(0,1fr))");
    } else {
        RoboWebAddDecl(style, "grid-template-columns", templateColumns.empty() ? columns : templateColumns);
    }
    if (RoboWebIsNumber(rows)) {
        RoboWebAddDecl(style, "grid-template-rows", "repeat(" + rows + ",minmax(0,1fr))");
    } else {
        RoboWebAddDecl(style, "grid-template-rows", templateRows.empty() ? rows : templateRows);
    }
    const std::string gridColumn = attr("gridColumn");
    const std::string columnSpan = first("columnSpan", "colSpan");
    if (!gridColumn.empty()) {
        RoboWebAddDecl(style, "grid-column", gridColumn);
    } else if (RoboWebIsNumber(columnSpan)) {
        RoboWebAddDecl(style, "grid-column", "span " + columnSpan);
    }
    const std::string gridRow = attr("gridRow");
    const std::string rowSpan = attr("rowSpan");
    if (!gridRow.empty()) {
        RoboWebAddDecl(style, "grid-row", gridRow);
    } else if (RoboWebIsNumber(rowSpan)) {
        RoboWebAddDecl(style, "grid-row", "span " + rowSpan);
    }
    RoboWebAddDecl(style, "grid-area", attr("gridArea"));
    RoboWebAddDecl(style, "grid-auto-flow", RoboWebLower(first("gridAutoFlow", "autoFlow")));
    const std::string position = RoboWebLower(attr("position"));
    if (position == "abs" || position == "absolute") {
        RoboWebAddDecl(style, "position", "absolute");
    } else if (position == "rel" || position == "relative") {
        RoboWebAddDecl(style, "position", "relative");
    } else if (position == "fixed") {
        RoboWebAddDecl(style, "position", "fixed");
    } else if (position == "sticky") {
        RoboWebAddDecl(style, "position", "sticky");
    } else if (position == "static") {
        RoboWebAddDecl(style, "position", "static");
    } else if (!position.empty()) {
        RoboWebAddDecl(style, "position", position);
    }
    RoboWebAddDecl(style, "inset", length(attr("inset")));
    RoboWebAddDecl(style, "top", length(attr("top")));
    RoboWebAddDecl(style, "right", length(attr("right")));
    RoboWebAddDecl(style, "bottom", length(attr("bottom")));
    RoboWebAddDecl(style, "left", length(attr("left")));
    RoboWebAddDecl(style, "width", length(attr("width")));
    RoboWebAddDecl(style, "height", length(attr("height")));
    RoboWebAddDecl(style, "min-width", length(first("minWidth", "minW")));
    RoboWebAddDecl(style, "min-height", length(first("minHeight", "minH")));
    RoboWebAddDecl(style, "max-width", length(first("maxWidth", "maxW")));
    RoboWebAddDecl(style, "max-height", length(first("maxHeight", "maxH")));
    RoboWebAddDecl(style, "aspect-ratio", attr("aspectRatio"));
    RoboWebAddDecl(style, "padding", length(attr("padding")));
    if (attr("paddingLeft").empty() && attr("paddingRight").empty()) {
        const std::string padX = length(first("paddingX", "paddingHorizontal"));
        RoboWebAddDecl(style, "padding-left", padX);
        RoboWebAddDecl(style, "padding-right", padX);
    }
    if (attr("paddingTop").empty() && attr("paddingBottom").empty()) {
        const std::string padY = length(first("paddingY", "paddingVertical"));
        RoboWebAddDecl(style, "padding-top", padY);
        RoboWebAddDecl(style, "padding-bottom", padY);
    }
    RoboWebAddDecl(style, "padding-top", length(attr("paddingTop")));
    RoboWebAddDecl(style, "padding-right", length(attr("paddingRight")));
    RoboWebAddDecl(style, "padding-bottom", length(attr("paddingBottom")));
    RoboWebAddDecl(style, "padding-left", length(attr("paddingLeft")));
    RoboWebAddDecl(style, "margin", length(attr("margin")));
    if (attr("marginLeft").empty() && attr("marginRight").empty()) {
        const std::string marginX = length(first("marginX", "marginHorizontal"));
        RoboWebAddDecl(style, "margin-left", marginX);
        RoboWebAddDecl(style, "margin-right", marginX);
    }
    if (attr("marginTop").empty() && attr("marginBottom").empty()) {
        const std::string marginY = length(first("marginY", "marginVertical"));
        RoboWebAddDecl(style, "margin-top", marginY);
        RoboWebAddDecl(style, "margin-bottom", marginY);
    }
    RoboWebAddDecl(style, "margin-top", length(attr("marginTop")));
    RoboWebAddDecl(style, "margin-right", length(attr("marginRight")));
    RoboWebAddDecl(style, "margin-bottom", length(attr("marginBottom")));
    RoboWebAddDecl(style, "margin-left", length(attr("marginLeft")));
    RoboWebAddDecl(style, "border-radius", length(attr("radius")));
    RoboWebAddDecl(style, "background", RoboWebCssValue(library, attr("background"), true));
    RoboWebAddDecl(style, "color", RoboWebCssValue(library, attr("color"), true));
    const std::string border = attr("border");
    if (!border.empty() && border != "0" && RoboWebLower(border) != "false") {
        RoboWebAddDecl(style, "border-width", length(border));
        RoboWebAddDecl(style, "border-style", "solid");
    }
    const std::string borderStyle = RoboWebLower(attr("borderStyle"));
    if (!borderStyle.empty()) {
        RoboWebAddDecl(style, "border-style", borderStyle);
    }
    RoboWebAddDecl(style, "border-color", RoboWebCssValue(library, attr("borderColor"), true));
    RoboWebAddDecl(style, "opacity", attr("opacity"));
    RoboWebAddDecl(style, "z-index", first("zIndex", "z"));
    std::string overflow = attr("overflow");
    if (overflow.empty()) {
        const std::string clip = RoboWebLower(attr("clip"));
        if (clip == "true" || clip == "1" || clip == "yes" || clip == "on") {
            overflow = "clip";
        }
    }
    RoboWebAddDecl(style, "overflow", overflow);
    const std::string overflowX = attr("overflowX").empty() ? attr("overflow-x") : attr("overflowX");
    const std::string overflowY = attr("overflowY").empty() ? attr("overflow-y") : attr("overflowY");
    RoboWebAddDecl(style, "overflow-x", overflowX);
    RoboWebAddDecl(style, "overflow-y", overflowY);
    RoboWebAddDecl(style, "cursor", RoboWebLower(attr("cursor")));
    RoboWebAddDecl(style, "pointer-events", RoboWebLower(first("pointerEvents", "pointer")));
    RoboWebAddDecl(style, "user-select", RoboWebLower(attr("userSelect")));
    const std::string visibility = RoboWebLower(first("visibility", "visible"));
    const bool hide = visibility == "false" || visibility == "0" || visibility == "no" || visibility == "off" ||
                      visibility == "hidden" || (visibility.empty() && RoboWebLower(attr("hidden")) == "true");
    if (hide) {
        RoboWebAddDecl(style, "display", "none");
    } else if (visibility == "collapse") {
        RoboWebAddDecl(style, "visibility", "collapse");
    }
    std::string transform;
    const auto addTransform = [&](const std::string& piece) {
        if (piece.empty()) {
            return;
        }
        if (!transform.empty()) {
            transform += " ";
        }
        transform += piece;
    };
    if (!attr("translateX").empty() || !attr("translateY").empty()) {
        addTransform("translate(" + length(attr("translateX").empty() ? "0" : attr("translateX")) + "," +
                     length(attr("translateY").empty() ? "0" : attr("translateY")) + ")");
    }
    if (!attr("scale").empty()) {
        addTransform("scale(" + attr("scale") + ")");
    }
    if (!attr("rotate").empty()) {
        const std::string rotate = attr("rotate");
        addTransform("rotate(" + (RoboWebIsNumber(rotate) ? rotate + "deg" : rotate) + ")");
    }
    if (!attr("transform").empty()) {
        addTransform(attr("transform"));
    }
    RoboWebAddDecl(style, "transform", transform);
    RoboWebAddDecl(style, "box-shadow", attr("boxShadow"));
    RoboWebAddDecl(style, "outline", attr("outline"));
    RoboWebAddDecl(style, "outline-offset", length(attr("outlineOffset")));
    RoboWebAddDecl(style, "object-fit", RoboWebLower(attr("objectFit")));
    RoboWebAddDecl(style, "float", RoboWebLower(attr("float")));
    RoboWebAddDecl(style, "clear", RoboWebLower(attr("clear")));
    if (!style.empty()) {
        style = std::string("box-sizing:border-box;") + style;
    }
    return style;
}

void RoboWebPaintSvg(std::string& svg) {
    const auto paint = [](std::string& text, const char* token, const char* color) {
        const std::string from = token;
        for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + 1)) {
            text.replace(at, from.size(), color);
        }
    };
    paint(svg, "#ROBO_FILL", "currentColor");
    paint(svg, "#ROBO_STROKE", "currentColor");
    paint(svg, "#ROBO_ACCENT", "#3B9EFF");
}

std::string RoboWebPrepareSvg(std::string svg) {
    std::string::size_type start = svg.find("<svg");
    if (start == std::string::npos) {
        start = svg.find("<SVG");
    }
    if (start == std::string::npos) {
        return {};
    }
    svg = svg.substr(start);
    RoboWebPaintSvg(svg);
    return svg;
}

std::string RoboWebIconLeaf(std::string name) {
    const auto slash = name.find_last_of("/\\");
    if (slash != std::string::npos) {
        name = name.substr(slash + 1);
    }
    if (name.size() > 4 && RoboWebLower(name.substr(name.size() - 4)) == ".svg") {
        name.resize(name.size() - 4);
    }
    return name;
}

std::string RoboWebSvg(const fs::path& assetsRoot, const std::string& name) {
    if (name.empty()) {
        return {};
    }
    const std::string leaf = RoboWebIconLeaf(name);
    std::string svg = RoboWebPrepareSvg(RoboCompilerCatalogSvg(leaf.c_str()));
    if (!svg.empty()) {
        return svg;
    }
    svg = RoboWebPrepareSvg(RoboWebRead(fs::path(name)));
    if (!svg.empty()) {
        return svg;
    }
    svg = RoboWebPrepareSvg(RoboWebRead(fs::path(assetsRoot) / "Icons" / (leaf + ".svg")));
    if (!svg.empty()) {
        return svg;
    }
    return RoboWebPrepareSvg(RoboWebRead(fs::path(assetsRoot) / "Icons" / "Flags" / (leaf + ".svg")));
}

std::string RoboWebIconMarkup(const fs::path& assetsRoot, const RoboXMLNode& node) {
    const std::string kind = RoboWebLower(node.name);
    if (kind == "svg") {
        std::string svg;
        RoboCompilerSerializeSvgNode(node, svg, true);
        return RoboWebPrepareSvg(svg);
    }
    if (kind == "icon") {
        std::string name = RoboXMLAttr(node, "name", RoboXMLAttr(node, "icon").c_str());
        if (name.empty()) {
            name = RoboXMLAttr(node, "src");
        }
        return RoboWebSvg(assetsRoot, name);
    }
    return {};
}
