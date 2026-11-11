#include "RoboView.hpp"

#include "../../ComponentTier.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>

namespace {

constexpr int ROBO_VIEW_CHILD_MAX = 64;

const char* const ROBO_VIEW_ATTRIBUTES[] = {
    "id",           "display",     "direction",      "flexdirection", "justify",        "justifycontent",
    "align",        "alignitems",  "aligncontent",   "justifyitems",  "wrap",           "flexwrap",
    "columns",      "rows",        "gap",            "spacing",       "rowgap",         "columngap",
    "colgap",       "padding",     "paddingtop",     "paddingright",  "paddingbottom", "paddingleft",
    "width",        "height",      "minwidth",       "minheight",     "maxwidth",      "maxheight",
    "minsize",      "maxsize",     "size",           "x",             "y",             "position",
    "inset",        "left",        "top",            "right",         "bottom",        "margin",
    "margintop",    "marginright", "marginbottom",   "marginleft",    "flex",          "flexgrow",
    "grow",         "flexshrink",  "shrink",         "alignself",     "order",         "gridcolumn",
    "gridrow",      "overflow",    "overflowx",      "overflow-x",    "overflowy",     "overflow-y",
    "clip",         "background",  "backgroundcolor","bg",            "fill",          "fillparent",
    "aspectratio",  "constraints",
};

std::string RoboViewLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string RoboViewFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

bool RoboViewIsViewTag(const std::string& name) {
    const char* base = RoboCompilerComponentBase(name);
    return base != nullptr && std::string(base) == "View";
}

bool RoboViewKnown(const std::string& key) {
    const std::string folded = RoboViewLower(key);
    for (const char* name : ROBO_VIEW_ATTRIBUTES) {
        if (folded == name) {
            return true;
        }
    }
    return false;
}

std::string RoboViewAttr(const RoboXMLNode& node, const char* key, const char* alias = nullptr) {
    const std::string value = RoboXMLAttr(node, key);
    if (!value.empty() || alias == nullptr) {
        return value;
    }
    return RoboXMLAttr(node, alias);
}

bool RoboViewOneOf(const std::string& value, const char* const* choices, int count) {
    const std::string folded = RoboViewLower(value);
    for (int index = 0; index < count; ++index) {
        if (folded == choices[index]) {
            return true;
        }
    }
    return false;
}

bool RoboViewInteger(const std::string& text, int& number) {
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

bool RoboViewPercent(const std::string& text, int& number) {
    if (text.size() < 2 || text.back() != '%') {
        return false;
    }
    return RoboViewInteger(text.substr(0, text.size() - 1), number);
}

int RoboViewHex(char ch) {
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

bool RoboViewColor(const std::string& text, RoboU32& color) {
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
        const int digit = RoboViewHex(ch);
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

bool RoboViewReadNumber(const std::string& text, const char* key, int& number, std::string& diagnostic) {
    if (!RoboViewInteger(text, number)) {
        diagnostic = RoboViewFail("R7103", std::string("View ") + key + " '" + text + "' is not an integer. Use a whole number.");
        return false;
    }
    return true;
}

bool RoboViewEdges(const std::string& text, const char* key, RoboViewEdge& edge, std::string& diagnostic) {
    std::vector<int> numbers;
    std::string token;
    auto flush = [&]() -> bool {
        if (token.empty()) {
            return true;
        }
        int number = 0;
        if (!RoboViewInteger(token, number)) {
            diagnostic = RoboViewFail("R7103", std::string("View ") + key + " '" + text + "' is not a size. Use one to four numbers.");
            return false;
        }
        numbers.push_back(number);
        token.clear();
        return true;
    };
    for (char ch : text) {
        if (ch == ' ' || ch == ',') {
            if (!flush()) {
                return false;
            }
            continue;
        }
        token.push_back(ch);
    }
    if (!flush()) {
        return false;
    }
    if (numbers.empty() || numbers.size() > 4) {
        diagnostic = RoboViewFail("R7103", std::string("View ") + key + " '" + text + "' is not a size. Use one to four numbers.");
        return false;
    }
    if (numbers.size() == 1) {
        edge.top = edge.right = edge.bottom = edge.left = numbers[0];
    } else if (numbers.size() == 2) {
        edge.top = edge.bottom = numbers[0];
        edge.right = edge.left = numbers[1];
    } else if (numbers.size() == 3) {
        edge.top = numbers[0];
        edge.right = edge.left = numbers[1];
        edge.bottom = numbers[2];
    } else {
        edge.top = numbers[0];
        edge.right = numbers[1];
        edge.bottom = numbers[2];
        edge.left = numbers[3];
    }
    return true;
}

bool RoboViewAspect(const std::string& text, int& num, int& den, std::string& diagnostic) {
    std::string left;
    std::string right;
    bool split = false;
    for (char ch : text) {
        if (ch == ':' || ch == '/') {
            split = true;
            continue;
        }
        if (!split) {
            left.push_back(ch);
        } else {
            right.push_back(ch);
        }
    }
    if (!RoboViewInteger(left, num) || num <= 0) {
        diagnostic = RoboViewFail("R7102", "View aspectRatio '" + text + "' is not a ratio. Use 2 or 16:9.");
        return false;
    }
    if (!split) {
        den = 1;
        return true;
    }
    if (!RoboViewInteger(right, den) || den <= 0) {
        diagnostic = RoboViewFail("R7102", "View aspectRatio '" + text + "' is not a ratio. Use 2 or 16:9.");
        return false;
    }
    return true;
}

bool RoboViewSpan(const std::string& text, int& start, int& span, std::string& diagnostic) {
    start = -1;
    span = 1;
    if (text.empty()) {
        return true;
    }
    const std::string folded = RoboViewLower(text);
    if (folded.rfind("span", 0) == 0) {
        std::string count = folded.substr(4);
        if (!count.empty() && count[0] == ' ') {
            count.erase(count.begin());
        }
        if (!RoboViewInteger(count, span) || span < 1) {
            diagnostic = RoboViewFail("R7103", "View grid span '" + text + "' is not a count. Use span 2.");
            return false;
        }
        return true;
    }
    const std::size_t slash = folded.find('/');
    if (slash != std::string::npos) {
        int end = 0;
        std::string left = folded.substr(0, slash);
        std::string right = folded.substr(slash + 1);
        if (!left.empty() && left.back() == ' ') {
            left.pop_back();
        }
        if (!right.empty() && right.front() == ' ') {
            right.erase(right.begin());
        }
        if (!RoboViewInteger(left, start) || !RoboViewInteger(right, end) || start < 1 || end <= start) {
            diagnostic = RoboViewFail("R7103", "View grid line '" + text + "' is not a range. Use 1 / 3.");
            return false;
        }
        span = end - start;
        return true;
    }
    if (!RoboViewInteger(folded, start) || start < 1) {
        diagnostic = RoboViewFail("R7103", "View grid line '" + text + "' is not a column. Use a number or span 2.");
        return false;
    }
    return true;
}

bool RoboViewClips(const std::string& value) {
    const std::string folded = RoboViewLower(value);
    return folded == "hidden" || folded == "clip" || folded == "scroll" || folded == "auto";
}

bool RoboViewRow(const RoboViewNode& node) {
    return node.direction == "row" || node.direction == "row-reverse";
}

bool RoboViewReverse(const RoboViewNode& node) {
    return node.direction == "row-reverse" || node.direction == "column-reverse";
}

int RoboViewMainGap(const RoboViewNode& node) {
    return RoboViewRow(node) ? node.columnGap : node.rowGap;
}

int RoboViewOuterW(const RoboViewNode& node) {
    return node.margin.left + node.usedW + node.margin.right;
}

int RoboViewOuterH(const RoboViewNode& node) {
    return node.margin.top + node.usedH + node.margin.bottom;
}

void RoboViewClamp(RoboViewNode& node) {
    if (node.hasMinWidth && node.usedW < node.minWidth) {
        node.usedW = node.minWidth;
    }
    if (node.hasMaxWidth && node.usedW > node.maxWidth) {
        node.usedW = node.maxWidth;
    }
    if (node.hasMinHeight && node.usedH < node.minHeight) {
        node.usedH = node.minHeight;
    }
    if (node.hasMaxHeight && node.usedH > node.maxHeight) {
        node.usedH = node.maxHeight;
    }
    if (node.usedW < 0) {
        node.usedW = 0;
    }
    if (node.usedH < 0) {
        node.usedH = 0;
    }
}

int RoboViewContentMain(const RoboViewNode& node, bool row) {
    int count = 0;
    int sum = 0;
    for (const RoboViewNode& child : node.children) {
        if (child.absolute) {
            continue;
        }
        sum += row ? RoboViewOuterW(child) : RoboViewOuterH(child);
        ++count;
    }
    if (count > 1) {
        sum += (count - 1) * RoboViewMainGap(node);
    }
    return sum;
}

int RoboViewContentCross(const RoboViewNode& node, bool row) {
    int breadth = 0;
    for (const RoboViewNode& child : node.children) {
        if (child.absolute) {
            continue;
        }
        const int cross = row ? RoboViewOuterH(child) : RoboViewOuterW(child);
        if (cross > breadth) {
            breadth = cross;
        }
    }
    return breadth;
}

void RoboViewResolve(RoboViewNode& node, int slotW, int slotH);

void RoboViewLayoutInside(RoboViewNode& node) {
    const int innerW = std::max(0, node.usedW - node.padding.left - node.padding.right);
    const int innerH = std::max(0, node.usedH - node.padding.top - node.padding.bottom);
    for (RoboViewNode& child : node.children) {
        const int slotChildW = std::max(0, innerW - child.margin.left - child.margin.right);
        const int slotChildH = std::max(0, innerH - child.margin.top - child.margin.bottom);
        RoboViewResolve(child, slotChildW, slotChildH);
    }
    if (node.display == "grid") {
        const int columns = std::max(1, node.columns);
        const int gaps = columns > 1 ? (columns - 1) * node.columnGap : 0;
        const int cellW = columns > 0 ? std::max(0, innerW - gaps) / columns : innerW;
        for (RoboViewNode& child : node.children) {
            if (child.absolute || child.hasWidth) {
                continue;
            }
            const int span = std::max(1, child.colSpan);
            const int spanGaps = span > 1 ? (span - 1) * node.columnGap : 0;
            child.usedW = span * cellW + spanGaps;
            RoboViewClamp(child);
            RoboViewLayoutInside(child);
        }
    }
    const bool row = RoboViewRow(node);
    if (node.display == "flex") {
        for (RoboViewNode& child : node.children) {
            if (child.absolute) {
                continue;
            }
            const std::string align = child.alignSelf == "auto" ? node.align : child.alignSelf;
            if (align != "stretch") {
                continue;
            }
            if (row && !child.hasHeight) {
                child.usedH = std::max(0, innerH - child.margin.top - child.margin.bottom);
                RoboViewClamp(child);
                RoboViewLayoutInside(child);
            }
            if (!row && !child.hasWidth) {
                child.usedW = std::max(0, innerW - child.margin.left - child.margin.right);
                RoboViewClamp(child);
                RoboViewLayoutInside(child);
            }
        }
        int count = 0;
        int base = 0;
        int growSum = 0;
        int shrinkSum = 0;
        for (const RoboViewNode& child : node.children) {
            if (child.absolute) {
                continue;
            }
            base += row ? RoboViewOuterW(child) : RoboViewOuterH(child);
            growSum += child.grow;
            shrinkSum += child.shrink;
            ++count;
        }
        if (count > 1) {
            base += (count - 1) * RoboViewMainGap(node);
        }
        const int inner = row ? innerW : innerH;
        int free = inner - base;
        if (free > 0 && growSum > 0) {
            int left = free;
            int growLeft = growSum;
            for (RoboViewNode& child : node.children) {
                if (child.absolute || child.grow <= 0) {
                    continue;
                }
                const int share = growLeft == child.grow ? left : left * child.grow / growLeft;
                left -= share;
                growLeft -= child.grow;
                if (row) {
                    child.usedW += share;
                } else {
                    child.usedH += share;
                }
                RoboViewClamp(child);
                RoboViewLayoutInside(child);
            }
        } else if (free < 0 && shrinkSum > 0) {
            int left = -free;
            int shrinkLeft = shrinkSum;
            for (RoboViewNode& child : node.children) {
                if (child.absolute || child.shrink <= 0) {
                    continue;
                }
                int share = shrinkLeft == child.shrink ? left : left * child.shrink / shrinkLeft;
                const int limit = row ? child.usedW : child.usedH;
                if (share > limit) {
                    share = limit;
                }
                left -= share;
                shrinkLeft -= child.shrink;
                if (row) {
                    child.usedW -= share;
                } else {
                    child.usedH -= share;
                }
                RoboViewClamp(child);
                RoboViewLayoutInside(child);
            }
        }
    }
}

void RoboViewResolve(RoboViewNode& node, int slotW, int slotH) {
    bool autoW = true;
    bool autoH = true;
    int width = 0;
    int height = 0;
    if (node.fill || node.sizeMode == "fill") {
        width = std::max(0, slotW);
        height = std::max(0, slotH);
        autoW = false;
        autoH = false;
    }
    if (node.sizeMode == "min") {
        width = node.minWidth;
        height = node.minHeight;
        autoW = false;
        autoH = false;
    }
    if (node.sizeMode == "max" && node.hasMaxWidth && node.hasMaxHeight) {
        width = node.maxWidth;
        height = node.maxHeight;
        autoW = false;
        autoH = false;
    }
    if (node.hasWidth && !node.widthPercent) {
        width = node.width;
        autoW = false;
    } else if (node.hasWidth && node.widthPercent && slotW >= 0) {
        width = slotW * node.width / 100;
        autoW = false;
    }
    if (node.hasHeight && !node.heightPercent) {
        height = node.height;
        autoH = false;
    } else if (node.hasHeight && node.heightPercent && slotH >= 0) {
        height = slotH * node.height / 100;
        autoH = false;
    }
    if (node.hasAspect && !autoW && autoH && node.aspectNum > 0) {
        height = (width * node.aspectDen + node.aspectNum / 2) / node.aspectNum;
        autoH = false;
    }
    if (node.hasAspect && autoW && !autoH && node.aspectDen > 0) {
        width = (height * node.aspectNum + node.aspectDen / 2) / node.aspectDen;
        autoW = false;
    }
    if (autoW || autoH) {
        const int probeW = autoW ? -1 : std::max(0, width - node.padding.left - node.padding.right);
        const int probeH = autoH ? -1 : std::max(0, height - node.padding.top - node.padding.bottom);
        for (RoboViewNode& child : node.children) {
            if (child.absolute) {
                RoboViewResolve(child, 0, 0);
                continue;
            }
            const int childW = probeW < 0 ? -1 : std::max(0, probeW - child.margin.left - child.margin.right);
            const int childH = probeH < 0 ? -1 : std::max(0, probeH - child.margin.top - child.margin.bottom);
            RoboViewResolve(child, childW, childH);
        }
        const bool row = node.display == "stack" ? true : RoboViewRow(node);
        if (autoW) {
            if (node.display == "stack") {
                int breadth = 0;
                for (const RoboViewNode& child : node.children) {
                    if (!child.absolute) {
                        breadth = std::max(breadth, RoboViewOuterW(child));
                    }
                }
                width = breadth + node.padding.left + node.padding.right;
            } else if (row && node.display != "grid") {
                width = RoboViewContentMain(node, true) + node.padding.left + node.padding.right;
            } else {
                width = RoboViewContentCross(node, false) + node.padding.left + node.padding.right;
            }
        }
        if (autoH) {
            if (node.display == "stack") {
                int breadth = 0;
                for (const RoboViewNode& child : node.children) {
                    if (!child.absolute) {
                        breadth = std::max(breadth, RoboViewOuterH(child));
                    }
                }
                height = breadth + node.padding.top + node.padding.bottom;
            } else if (row && node.display != "grid") {
                height = RoboViewContentCross(node, true) + node.padding.top + node.padding.bottom;
            } else {
                height = RoboViewContentMain(node, false) + node.padding.top + node.padding.bottom;
            }
        }
        if (node.hasAspect && autoW && !autoH && node.aspectDen > 0) {
            width = (height * node.aspectNum + node.aspectDen / 2) / node.aspectDen;
        }
        if (node.hasAspect && !autoW && autoH && node.aspectNum > 0) {
            height = (width * node.aspectDen + node.aspectNum / 2) / node.aspectNum;
        }
    }
    node.usedW = width;
    node.usedH = height;
    RoboViewClamp(node);
    RoboViewLayoutInside(node);
}

std::vector<int> RoboViewFlow(const RoboViewNode& node) {
    std::vector<int> indexes;
    for (int index = 0; index < static_cast<int>(node.children.size()); ++index) {
        if (!node.children[static_cast<std::size_t>(index)].absolute) {
            indexes.push_back(index);
        }
    }
    std::stable_sort(indexes.begin(), indexes.end(), [&](int left, int right) {
        return node.children[static_cast<std::size_t>(left)].order < node.children[static_cast<std::size_t>(right)].order;
    });
    return indexes;
}

void RoboViewPlace(RoboViewNode& node, int x, int y) {
    node.boxX = x;
    node.boxY = y;
    const int innerX = x + node.padding.left;
    const int innerY = y + node.padding.top;
    const int innerW = std::max(0, node.usedW - node.padding.left - node.padding.right);
    const int innerH = std::max(0, node.usedH - node.padding.top - node.padding.bottom);
    const std::vector<int> flow = RoboViewFlow(node);
    if (node.display == "stack") {
        for (int index : flow) {
            RoboViewNode& child = node.children[static_cast<std::size_t>(index)];
            RoboViewPlace(child, innerX + child.margin.left, innerY + child.margin.top);
        }
    } else if (node.display == "grid") {
        const int columns = std::max(1, node.columns);
        const int cellGaps = columns > 1 ? (columns - 1) * node.columnGap : 0;
        const int cellW = std::max(0, innerW - cellGaps) / columns;
        int column = 0;
        int row = 0;
        for (int index : flow) {
            RoboViewNode& child = node.children[static_cast<std::size_t>(index)];
            int startCol = child.colStart > 0 ? child.colStart - 1 : column;
            int startRow = child.rowStart > 0 ? child.rowStart - 1 : row;
            if (startCol + child.colSpan > columns) {
                startCol = 0;
                startRow += 1;
            }
            child.placedCol = startCol;
            child.placedRow = startRow;
            column = startCol + child.colSpan;
            row = startRow;
            if (column >= columns) {
                column = 0;
                row = startRow + 1;
            }
        }
        int rowCount = 0;
        for (int index : flow) {
            rowCount = std::max(rowCount, node.children[static_cast<std::size_t>(index)].placedRow + 1);
        }
        std::vector<int> rowHeights(static_cast<std::size_t>(std::max(1, rowCount)), 0);
        if (node.rows > 0) {
            const int rowGaps = node.rows > 1 ? (node.rows - 1) * node.rowGap : 0;
            const int cellH = std::max(0, innerH - rowGaps) / node.rows;
            rowHeights.assign(static_cast<std::size_t>(node.rows), cellH);
        } else {
            for (int index : flow) {
                const RoboViewNode& child = node.children[static_cast<std::size_t>(index)];
                int& height = rowHeights[static_cast<std::size_t>(child.placedRow)];
                height = std::max(height, child.usedH);
            }
        }
        for (int index : flow) {
            RoboViewNode& child = node.children[static_cast<std::size_t>(index)];
            int top = 0;
            for (int cursor = 0; cursor < child.placedRow; ++cursor) {
                top += rowHeights[static_cast<std::size_t>(cursor)] + node.rowGap;
            }
            const int left = child.placedCol * (cellW + node.columnGap);
            RoboViewPlace(child, innerX + left + child.margin.left, innerY + top + child.margin.top);
        }
    } else {
        const bool row = RoboViewRow(node);
        const bool reverse = RoboViewReverse(node);
        const int gap = RoboViewMainGap(node);
        const int inner = row ? innerW : innerH;
        int occupied = 0;
        for (int index : flow) {
            const RoboViewNode& child = node.children[static_cast<std::size_t>(index)];
            occupied += row ? RoboViewOuterW(child) : RoboViewOuterH(child);
        }
        if (flow.size() > 1) {
            occupied += (static_cast<int>(flow.size()) - 1) * gap;
        }
        int free = inner - occupied;
        if (free < 0) {
            free = 0;
        }
        int leading = 0;
        int between = gap;
        const int count = static_cast<int>(flow.size());
        if (node.justify == "center") {
            leading = free / 2;
        } else if (node.justify == "end") {
            leading = free;
        } else if (node.justify == "space-between" && count > 1) {
            between = gap + free / (count - 1);
        } else if (node.justify == "space-around" && count > 0) {
            const int extra = free / count;
            leading = extra / 2;
            between = gap + extra;
        } else if (node.justify == "space-evenly" && count > 0) {
            const int slot = free / (count + 1);
            leading = slot;
            between = gap + slot;
        }
        int cursor = reverse ? inner - leading : leading;
        for (int index : flow) {
            RoboViewNode& child = node.children[static_cast<std::size_t>(index)];
            const int outer = row ? RoboViewOuterW(child) : RoboViewOuterH(child);
            const std::string align = child.alignSelf == "auto" ? node.align : child.alignSelf;
            const int crossOuter = row ? RoboViewOuterH(child) : RoboViewOuterW(child);
            const int crossInner = row ? innerH : innerW;
            int cross = 0;
            if (align == "center") {
                cross = (crossInner - crossOuter) / 2;
            } else if (align == "end") {
                cross = crossInner - crossOuter;
            }
            if (cross < 0) {
                cross = 0;
            }
            int main = 0;
            if (row) {
                main = reverse ? cursor - child.margin.right - child.usedW : cursor + child.margin.left;
                RoboViewPlace(child, innerX + main, innerY + cross + child.margin.top);
            } else {
                main = reverse ? cursor - child.margin.bottom - child.usedH : cursor + child.margin.top;
                RoboViewPlace(child, innerX + cross + child.margin.left, innerY + main);
            }
            cursor += reverse ? -(outer + between) : (outer + between);
        }
    }
    for (RoboViewNode& child : node.children) {
        if (!child.absolute) {
            continue;
        }
        int childX = innerX + child.x + child.margin.left;
        int childY = innerY + child.y + child.margin.top;
        if (child.hasLeft) {
            childX = innerX + child.left;
        } else if (child.hasRight) {
            childX = x + node.usedW - node.padding.right - child.right - child.usedW;
        }
        if (child.hasTop) {
            childY = innerY + child.top;
        } else if (child.hasBottom) {
            childY = y + node.usedH - node.padding.bottom - child.bottom - child.usedH;
        }
        RoboViewPlace(child, childX, childY);
    }
}

struct RoboViewClip {
    int x = 0;
    int y = 0;
    int right = 0;
    int bottom = 0;
    bool limitX = false;
    bool limitY = false;
};

void RoboViewFill(RoboViewFrame& frame, int x, int y, int width, int height, RoboU32 color, const RoboViewClip& clip) {
    if (!frame.pixels.empty() && width > 0 && height > 0) {
        int x0 = x;
        int y0 = y;
        int x1 = x + width;
        int y1 = y + height;
        if (clip.limitX) {
            x0 = std::max(x0, clip.x);
            x1 = std::min(x1, clip.right);
        }
        if (clip.limitY) {
            y0 = std::max(y0, clip.y);
            y1 = std::min(y1, clip.bottom);
        }
        x0 = std::max(0, x0);
        y0 = std::max(0, y0);
        x1 = std::min(frame.width, x1);
        y1 = std::min(frame.height, y1);
        for (int row = y0; row < y1; ++row) {
            RoboU32* pixels = frame.pixels.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(frame.width);
            for (int column = x0; column < x1; ++column) {
                pixels[column] = color;
            }
        }
    }
}

void RoboViewPaintNode(const RoboViewNode& node, RoboViewFrame& frame, RoboViewClip clip) {
    if (node.hasColor) {
        RoboViewFill(frame, node.boxX, node.boxY, node.usedW, node.usedH, node.color, clip);
    }
    if (node.clipX) {
        const int left = node.boxX;
        const int right = node.boxX + node.usedW;
        clip.x = clip.limitX ? std::max(clip.x, left) : left;
        clip.right = clip.limitX ? std::min(clip.right, right) : right;
        clip.limitX = true;
    }
    if (node.clipY) {
        const int top = node.boxY;
        const int bottom = node.boxY + node.usedH;
        clip.y = clip.limitY ? std::max(clip.y, top) : top;
        clip.bottom = clip.limitY ? std::min(clip.bottom, bottom) : bottom;
        clip.limitY = true;
    }
    for (const RoboViewNode& child : node.children) {
        RoboViewPaintNode(child, frame, clip);
    }
}

const RoboViewNode* RoboViewHitNode(const RoboViewNode& node, int x, int y, RoboViewClip clip) {
    if (node.clipX) {
        const int left = node.boxX;
        const int right = node.boxX + node.usedW;
        clip.x = clip.limitX ? std::max(clip.x, left) : left;
        clip.right = clip.limitX ? std::min(clip.right, right) : right;
        clip.limitX = true;
    }
    if (node.clipY) {
        const int top = node.boxY;
        const int bottom = node.boxY + node.usedH;
        clip.y = clip.limitY ? std::max(clip.y, top) : top;
        clip.bottom = clip.limitY ? std::min(clip.bottom, bottom) : bottom;
        clip.limitY = true;
    }
    const bool insideX = !clip.limitX || (x >= clip.x && x < clip.right);
    const bool insideY = !clip.limitY || (y >= clip.y && y < clip.bottom);
    const RoboViewNode* found = nullptr;
    if (insideX && insideY && x >= node.boxX && y >= node.boxY && x < node.boxX + node.usedW && y < node.boxY + node.usedH) {
        found = &node;
    }
    if (insideX && insideY) {
        for (const RoboViewNode& child : node.children) {
            const RoboViewNode* hit = RoboViewHitNode(child, x, y, clip);
            if (hit != nullptr) {
                found = hit;
            }
        }
    }
    return found;
}

bool RoboViewReadNode(const RoboXMLNode& node, RoboViewNode& view, std::string& diagnostic) {
    if (!RoboViewIsViewTag(node.name)) {
        diagnostic = RoboViewFail("R7104", "'" + node.name + "' is not a View. Use View or Container.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!RoboViewKnown(entry.first)) {
            diagnostic = RoboViewFail("R7101", "View attribute '" + entry.first + "' is unknown. Remove it or use a View attribute.");
            return false;
        }
    }
    view = RoboViewNode{};
    view.tag = "View";
    view.id = RoboXMLAttr(node, "id");
    view.display = RoboViewLower(RoboXMLAttr(node, "display"));
    if (view.display.empty()) {
        view.display = "flex";
    }
    const char* displays[] = {"flex", "grid", "stack"};
    if (!RoboViewOneOf(view.display, displays, 3)) {
        diagnostic = RoboViewFail("R7102", "View display '" + view.display + "' is unknown. Use flex, grid, or stack.");
        return false;
    }
    view.direction = RoboViewLower(RoboViewAttr(node, "direction", "flexDirection"));
    if (view.direction.empty() || view.direction == "col") {
        view.direction = view.direction == "col" ? "column" : "column";
    }
    if (view.direction == "column") {
        view.direction = "column";
    }
    const char* directions[] = {"row", "column", "row-reverse", "column-reverse"};
    if (view.direction.empty()) {
        view.direction = "column";
    }
    if (!RoboViewOneOf(view.direction, directions, 4)) {
        diagnostic = RoboViewFail("R7102", "View direction '" + view.direction + "' is unknown. Use row, column, row-reverse, or column-reverse.");
        return false;
    }
    view.justify = RoboViewLower(RoboViewAttr(node, "justify", "justifyContent"));
    if (view.justify.empty() || view.justify == "flex-start") {
        view.justify = "start";
    }
    if (view.justify == "flex-end") {
        view.justify = "end";
    }
    const char* justifies[] = {"start", "center", "end", "space-between", "space-around", "space-evenly"};
    if (!RoboViewOneOf(view.justify, justifies, 6)) {
        diagnostic = RoboViewFail("R7102", "View justify '" + view.justify + "' is unknown. Use start, center, end, or a space mode.");
        return false;
    }
    view.align = RoboViewLower(RoboViewAttr(node, "align", "alignItems"));
    if (view.align.empty() || view.align == "flex-start") {
        view.align = view.align.empty() ? "stretch" : "start";
    }
    if (view.align == "flex-end") {
        view.align = "end";
    }
    const char* aligns[] = {"start", "center", "end", "stretch"};
    if (!RoboViewOneOf(view.align, aligns, 4)) {
        diagnostic = RoboViewFail("R7102", "View align '" + view.align + "' is unknown. Use start, center, end, or stretch.");
        return false;
    }
    view.alignSelf = RoboViewLower(RoboXMLAttr(node, "alignSelf"));
    if (view.alignSelf.empty()) {
        view.alignSelf = "auto";
    }
    const char* selves[] = {"auto", "start", "center", "end", "stretch"};
    if (!RoboViewOneOf(view.alignSelf, selves, 5)) {
        diagnostic = RoboViewFail("R7102", "View alignSelf '" + view.alignSelf + "' is unknown. Use auto, start, center, end, or stretch.");
        return false;
    }
    view.position = RoboViewLower(RoboXMLAttr(node, "position"));
    if (view.position.empty()) {
        view.position = "relative";
    }
    const char* positions[] = {"relative", "absolute"};
    if (!RoboViewOneOf(view.position, positions, 2)) {
        diagnostic = RoboViewFail("R7102", "View position '" + view.position + "' is unknown. Use relative or absolute.");
        return false;
    }
    view.absolute = view.position == "absolute";
    view.sizeMode = RoboViewLower(RoboXMLAttr(node, "size"));
    if (!view.sizeMode.empty()) {
        const char* sizes[] = {"exact", "auto", "dynamic", "fill", "content", "min", "max"};
        if (!RoboViewOneOf(view.sizeMode, sizes, 7)) {
            diagnostic = RoboViewFail("R7102", "View size '" + view.sizeMode + "' is unknown. Use exact, content, fill, min, or max.");
            return false;
        }
        if (view.sizeMode == "auto" || view.sizeMode == "dynamic") {
            view.sizeMode = "content";
        }
    }
    std::string overflow = RoboViewLower(RoboXMLAttr(node, "overflow"));
    if (overflow.empty()) {
        overflow = "visible";
    }
    const char* overflows[] = {"visible", "hidden", "clip", "scroll", "auto"};
    if (!RoboViewOneOf(overflow, overflows, 5)) {
        diagnostic = RoboViewFail("R7102", "View overflow '" + overflow + "' is unknown. Use visible, hidden, clip, or scroll.");
        return false;
    }
    std::string overflowX = RoboViewLower(RoboViewAttr(node, "overflowX", "overflow-x"));
    std::string overflowY = RoboViewLower(RoboViewAttr(node, "overflowY", "overflow-y"));
    if (!overflowX.empty() && !RoboViewOneOf(overflowX, overflows, 5)) {
        diagnostic = RoboViewFail("R7102", "View overflowX '" + overflowX + "' is unknown. Use visible, hidden, clip, or scroll.");
        return false;
    }
    if (!overflowY.empty() && !RoboViewOneOf(overflowY, overflows, 5)) {
        diagnostic = RoboViewFail("R7102", "View overflowY '" + overflowY + "' is unknown. Use visible, hidden, clip, or scroll.");
        return false;
    }
    if (overflowX.empty()) {
        overflowX = overflow;
    }
    if (overflowY.empty()) {
        overflowY = overflow;
    }
    const std::string clip = RoboViewLower(RoboXMLAttr(node, "clip"));
    if (clip == "true" || clip == "1" || clip == "yes") {
        overflowX = "clip";
        overflowY = "clip";
    }
    view.clipX = RoboViewClips(overflowX);
    view.clipY = RoboViewClips(overflowY);

    const std::string padding = RoboXMLAttr(node, "padding");
    if (!padding.empty() && !RoboViewEdges(padding, "padding", view.padding, diagnostic)) {
        return false;
    }
    const std::string margin = RoboXMLAttr(node, "margin");
    if (!margin.empty() && !RoboViewEdges(margin, "margin", view.margin, diagnostic)) {
        return false;
    }
    const char* edgeNames[] = {"Top", "Right", "Bottom", "Left"};
    int* paddingEdges[] = {&view.padding.top, &view.padding.right, &view.padding.bottom, &view.padding.left};
    int* marginEdges[] = {&view.margin.top, &view.margin.right, &view.margin.bottom, &view.margin.left};
    for (int index = 0; index < 4; ++index) {
        const std::string paddingKey = std::string("padding") + edgeNames[index];
        const std::string paddingValue = RoboXMLAttr(node, paddingKey.c_str());
        if (!paddingValue.empty() && !RoboViewReadNumber(paddingValue, paddingKey.c_str(), *paddingEdges[index], diagnostic)) {
            return false;
        }
        const std::string marginKey = std::string("margin") + edgeNames[index];
        const std::string marginValue = RoboXMLAttr(node, marginKey.c_str());
        if (!marginValue.empty() && !RoboViewReadNumber(marginValue, marginKey.c_str(), *marginEdges[index], diagnostic)) {
            return false;
        }
    }
    const std::string inset = RoboXMLAttr(node, "inset");
    RoboViewEdge insetEdge;
    if (!inset.empty() && !RoboViewEdges(inset, "inset", insetEdge, diagnostic)) {
        return false;
    }
    if (!inset.empty()) {
        view.left = insetEdge.left;
        view.top = insetEdge.top;
        view.right = insetEdge.right;
        view.bottom = insetEdge.bottom;
        view.hasLeft = view.hasTop = view.hasRight = view.hasBottom = true;
    }
    const char* sideNames[] = {"left", "top", "right", "bottom"};
    int* sideValues[] = {&view.left, &view.top, &view.right, &view.bottom};
    bool* sideFlags[] = {&view.hasLeft, &view.hasTop, &view.hasRight, &view.hasBottom};
    for (int index = 0; index < 4; ++index) {
        const std::string value = RoboXMLAttr(node, sideNames[index]);
        if (!value.empty()) {
            if (!RoboViewReadNumber(value, sideNames[index], *sideValues[index], diagnostic)) {
                return false;
            }
            *sideFlags[index] = true;
        }
    }

    const std::string width = RoboXMLAttr(node, "width");
    if (!width.empty()) {
        view.hasWidth = true;
        if (RoboViewPercent(width, view.width)) {
            view.widthPercent = true;
        } else if (!RoboViewReadNumber(width, "width", view.width, diagnostic)) {
            return false;
        }
    }
    const std::string height = RoboXMLAttr(node, "height");
    if (!height.empty()) {
        view.hasHeight = true;
        if (RoboViewPercent(height, view.height)) {
            view.heightPercent = true;
        } else if (!RoboViewReadNumber(height, "height", view.height, diagnostic)) {
            return false;
        }
    }
    const std::string minWidth = RoboXMLAttr(node, "minWidth");
    if (!minWidth.empty()) {
        view.hasMinWidth = true;
        if (!RoboViewReadNumber(minWidth, "minWidth", view.minWidth, diagnostic)) {
            return false;
        }
    }
    const std::string minHeight = RoboXMLAttr(node, "minHeight");
    if (!minHeight.empty()) {
        view.hasMinHeight = true;
        if (!RoboViewReadNumber(minHeight, "minHeight", view.minHeight, diagnostic)) {
            return false;
        }
    }
    const std::string maxWidth = RoboXMLAttr(node, "maxWidth");
    if (!maxWidth.empty()) {
        view.hasMaxWidth = true;
        if (!RoboViewReadNumber(maxWidth, "maxWidth", view.maxWidth, diagnostic)) {
            return false;
        }
    }
    const std::string maxHeight = RoboXMLAttr(node, "maxHeight");
    if (!maxHeight.empty()) {
        view.hasMaxHeight = true;
        if (!RoboViewReadNumber(maxHeight, "maxHeight", view.maxHeight, diagnostic)) {
            return false;
        }
    }
    const std::string constraints = RoboXMLAttr(node, "constraints");
    if (!constraints.empty()) {
        RoboViewEdge constraint;
        if (!RoboViewEdges(constraints, "constraints", constraint, diagnostic)) {
            return false;
        }
        view.hasMinWidth = true;
        view.hasMinHeight = true;
        view.hasMaxWidth = true;
        view.hasMaxHeight = true;
        view.minWidth = constraint.top;
        view.minHeight = constraint.right;
        view.maxWidth = constraint.bottom;
        view.maxHeight = constraint.left;
    }
    const std::string aspect = RoboXMLAttr(node, "aspectRatio");
    if (!aspect.empty()) {
        view.hasAspect = true;
        if (!RoboViewAspect(aspect, view.aspectNum, view.aspectDen, diagnostic)) {
            return false;
        }
    }
    std::string grow = RoboViewAttr(node, "grow", "flex");
    if (grow.empty()) {
        grow = RoboXMLAttr(node, "flexGrow");
    }
    if (!grow.empty() && !RoboViewReadNumber(grow, "grow", view.grow, diagnostic)) {
        return false;
    }
    std::string shrink = RoboViewAttr(node, "shrink", "flexShrink");
    if (!shrink.empty() && !RoboViewReadNumber(shrink, "shrink", view.shrink, diagnostic)) {
        return false;
    }
    const std::string order = RoboXMLAttr(node, "order");
    if (!order.empty() && !RoboViewReadNumber(order, "order", view.order, diagnostic)) {
        return false;
    }
    const std::string columns = RoboXMLAttr(node, "columns");
    if (!columns.empty() && !RoboViewReadNumber(columns, "columns", view.columns, diagnostic)) {
        return false;
    }
    const std::string rows = RoboXMLAttr(node, "rows");
    if (!rows.empty() && !RoboViewReadNumber(rows, "rows", view.rows, diagnostic)) {
        return false;
    }
    if (!RoboViewSpan(RoboXMLAttr(node, "gridColumn"), view.colStart, view.colSpan, diagnostic)) {
        return false;
    }
    if (!RoboViewSpan(RoboXMLAttr(node, "gridRow"), view.rowStart, view.rowSpan, diagnostic)) {
        return false;
    }
    const std::string gap = RoboViewAttr(node, "gap", "spacing");
    if (!gap.empty() && !RoboViewReadNumber(gap, "gap", view.gap, diagnostic)) {
        return false;
    }
    view.rowGap = view.gap;
    view.columnGap = view.gap;
    const std::string rowGap = RoboXMLAttr(node, "rowGap");
    if (!rowGap.empty() && !RoboViewReadNumber(rowGap, "rowGap", view.rowGap, diagnostic)) {
        return false;
    }
    const std::string columnGap = RoboViewAttr(node, "columnGap", "colGap");
    if (!columnGap.empty() && !RoboViewReadNumber(columnGap, "columnGap", view.columnGap, diagnostic)) {
        return false;
    }
    const std::string x = RoboXMLAttr(node, "x");
    if (!x.empty() && !RoboViewReadNumber(x, "x", view.x, diagnostic)) {
        return false;
    }
    const std::string y = RoboXMLAttr(node, "y");
    if (!y.empty() && !RoboViewReadNumber(y, "y", view.y, diagnostic)) {
        return false;
    }
    const std::string fill = RoboViewLower(RoboViewAttr(node, "fill", "fillParent"));
    view.fill = fill == "true" || fill == "1" || fill == "yes";
    std::string background = RoboXMLAttr(node, "background");
    if (background.empty()) {
        background = RoboXMLAttr(node, "backgroundColor");
    }
    if (background.empty()) {
        background = RoboXMLAttr(node, "bg");
    }
    if (!background.empty()) {
        if (!RoboViewColor(background, view.color)) {
            diagnostic = RoboViewFail("R7102", "View background '" + background + "' is not a color. Use #RRGGBB.");
            return false;
        }
        view.hasColor = true;
    }
    for (const RoboXMLNode& child : node.children) {
        if (RoboViewIsViewTag(child.name)) {
            if (static_cast<int>(view.children.size()) >= ROBO_VIEW_CHILD_MAX) {
                diagnostic = RoboViewFail("R7105", "View has more than 64 children. Remove the extra children.");
                return false;
            }
            RoboViewNode next;
            if (!RoboViewReadNode(child, next, diagnostic)) {
                return false;
            }
            view.children.push_back(std::move(next));
            continue;
        }
        if (RoboCompilerComponentTier(child.name) == RoboUITier::Unknown && !child.name.empty()) {
            diagnostic = RoboViewFail("R7104", "'" + child.name + "' is not a View. Use View or Container.");
            return false;
        }
    }
    return true;
}

}  // namespace

bool RoboViewRead(const RoboXMLNode& node, RoboViewNode& view, std::string& diagnostic) {
    diagnostic.clear();
    return RoboViewReadNode(node, view, diagnostic);
}

void RoboViewLayout(RoboViewNode& view) {
    const int slotW = view.hasWidth && !view.widthPercent ? view.width : -1;
    const int slotH = view.hasHeight && !view.heightPercent ? view.height : -1;
    RoboViewResolve(view, slotW, slotH);
    RoboViewPlace(view, 0, 0);
}

void RoboViewPaint(const RoboViewNode& view, RoboViewFrame& frame) {
    frame.width = view.usedW;
    frame.height = view.usedH;
    frame.pixels.assign(static_cast<std::size_t>(std::max(0, frame.width)) * static_cast<std::size_t>(std::max(0, frame.height)), 0);
    RoboViewClip clip;
    clip.x = 0;
    clip.y = 0;
    clip.right = frame.width;
    clip.bottom = frame.height;
    clip.limitX = true;
    clip.limitY = true;
    RoboViewPaintNode(view, frame, clip);
}

bool RoboViewPixel(const RoboViewFrame& frame, int x, int y, RoboU32& color) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return false;
    }
    color = frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)];
    return true;
}

const RoboViewNode* RoboViewHit(const RoboViewNode& view, int x, int y) {
    RoboViewClip clip;
    return RoboViewHitNode(view, x, y, clip);
}

const RoboViewNode* RoboViewFind(const RoboViewNode& view, const std::string& id) {
    if (view.id == id) {
        return &view;
    }
    for (const RoboViewNode& child : view.children) {
        if (const RoboViewNode* found = RoboViewFind(child, id)) {
            return found;
        }
    }
    return nullptr;
}
