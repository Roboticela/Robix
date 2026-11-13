#include "RoboList.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace {

const char* const ROBO_LIST_ATTRIBUTES[] = {
    "id",        "name",     "items",    "key",       "template", "numbered", "width",    "height",
    "itemheight","item-height","gap",    "direction", "align",    "justify",  "padding",  "background",
    "overflow",  "margin",   "minwidth", "maxwidth",  "minheight","maxheight","wrap",
};

const char* const ROBO_LIST_TEMPLATES[] = {
    "view", "text", "image", "icon", "svg", "canvas", "view3d", "input", "scrollview", "audio",
};

const RoboU32 ROBO_LIST_PALETTE[] = {
    0xFF0E7AFFu, 0xFF2F9E44u, 0xFFC4553Au, 0xFFF2C14Eu, 0xFFF4F7FBu,
};

std::string RoboListLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string RoboListFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

std::string RoboListTrim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

bool RoboListKnown(const std::string& key) {
    const std::string folded = RoboListLower(key);
    for (const char* name : ROBO_LIST_ATTRIBUTES) {
        if (folded == name) {
            return true;
        }
    }
    return false;
}

bool RoboListTemplateName(std::string& kind) {
    kind = RoboListLower(kind);
    if (kind == "container" || kind == "box") {
        kind = "view";
    } else if (kind == "textfield") {
        kind = "input";
    } else if (kind == "scene3d") {
        kind = "view3d";
    }
    for (const char* name : ROBO_LIST_TEMPLATES) {
        if (kind == name) {
            return true;
        }
    }
    return false;
}

int RoboListHex(char ch) {
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

bool RoboListColor(const std::string& text, RoboU32& color) {
    if (text.size() != 7 || text[0] != '#') {
        return false;
    }
    RoboU32 value = 0;
    for (int index = 1; index <= 6; ++index) {
        const int digit = RoboListHex(text[static_cast<std::size_t>(index)]);
        if (digit < 0) {
            return false;
        }
        value = (value << 4) | static_cast<RoboU32>(digit);
    }
    color = 0xFF000000u | value;
    return true;
}

bool RoboListInteger(const std::string& text, int& number) {
    if (text.empty()) {
        return false;
    }
    int value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10 + (ch - '0');
        if (value > 4096) {
            return false;
        }
    }
    number = value;
    return value > 0;
}

bool RoboListFlag(const std::string& text, bool& flag) {
    const std::string folded = RoboListLower(text);
    if (folded == "true" || folded == "yes" || folded == "on" || folded == "1") {
        flag = true;
        return true;
    }
    if (folded == "false" || folded == "no" || folded == "off" || folded == "0") {
        flag = false;
        return true;
    }
    return false;
}

RoboU32 RoboListPalette(std::size_t index) {
    return ROBO_LIST_PALETTE[index % (sizeof(ROBO_LIST_PALETTE) / sizeof(ROBO_LIST_PALETTE[0]))];
}

void RoboListPlace(RoboListNode& list) {
    int cursor = 0;
    for (std::size_t index = 0; index < list.children.size(); ++index) {
        RoboListChild& child = list.children[index];
        child.x = 0;
        child.y = cursor;
        child.width = list.width;
        child.height = list.itemHeight;
        cursor += list.itemHeight;
        if (index + 1 < list.children.size()) {
            cursor += list.gap;
        }
    }
    list.contentHeight = cursor;
}

bool RoboListInside(int x, int y, int left, int top, int width, int height) {
    return width > 0 && height > 0 && x >= left && y >= top && x < left + width && y < top + height;
}

const RoboXMLNode* RoboListFindChild(const RoboXMLNode& node, const char* name) {
    const std::string wanted = name;
    for (const RoboXMLNode& child : node.children) {
        if (RoboListLower(child.name) == wanted) {
            return &child;
        }
    }
    return nullptr;
}

int RoboListCountChild(const RoboXMLNode& node, const char* name) {
    const std::string wanted = name;
    int count = 0;
    for (const RoboXMLNode& child : node.children) {
        if (RoboListLower(child.name) == wanted) {
            ++count;
        }
    }
    return count;
}

std::string RoboListReplace(std::string text, const std::unordered_map<std::string, std::string>& fields) {
    std::string out;
    for (std::size_t index = 0; index < text.size();) {
        if (text[index] != '{') {
            out.push_back(text[index]);
            ++index;
            continue;
        }
        const std::size_t end = text.find('}', index + 1);
        if (end == std::string::npos) {
            out.push_back(text[index]);
            ++index;
            continue;
        }
        const std::string name = RoboListLower(text.substr(index + 1, end - index - 1));
        const auto found = fields.find(name);
        if (found == fields.end()) {
            out.append(text, index, end - index + 1);
        } else {
            out += found->second;
        }
        index = end + 1;
    }
    return out;
}

void RoboListStamp(RoboXMLNode& node, const std::unordered_map<std::string, std::string>& fields) {
    node.text = RoboListReplace(node.text, fields);
    for (auto& entry : node.attrs) {
        entry.second = RoboListReplace(entry.second, fields);
    }
    for (RoboXMLNode& child : node.children) {
        RoboListStamp(child, fields);
    }
}

std::string RoboListTag(const std::string& kind) {
    if (kind == "view3d") {
        return "View3D";
    }
    if (kind == "scrollview") {
        return "ScrollView";
    }
    if (kind.empty()) {
        return "View";
    }
    std::string tag = kind;
    tag[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(tag[0])));
    return tag;
}

}  // namespace

bool RoboListApply(RoboListNode& list, const std::vector<RoboListItem>& items, std::string& diagnostic) {
    if (static_cast<int>(items.size()) > ROBO_LIST_MAX_ITEMS) {
        diagnostic = RoboListFail("R8202", "List items stop at 1000.");
        return false;
    }
    std::unordered_map<std::string, int> seen;
    for (const RoboListItem& item : items) {
        if (item.key.empty()) {
            diagnostic = RoboListFail("R8202", "List item is missing its key.");
            return false;
        }
        if (!seen.emplace(item.key, 1).second) {
            diagnostic = RoboListFail("R8202", "List key '" + item.key + "' is repeated.");
            return false;
        }
    }
    std::unordered_map<std::string, RoboListChild> kept;
    for (const RoboListChild& child : list.children) {
        if (seen.find(child.key) != seen.end()) {
            kept.emplace(child.key, child);
        } else {
            list.freeIdentities.push_back(child.identity);
        }
    }
    std::vector<RoboListChild> next;
    next.reserve(items.size());
    for (std::size_t index = 0; index < items.size(); ++index) {
        const RoboListItem& item = items[index];
        RoboListChild child;
        const auto previous = kept.find(item.key);
        if (previous != kept.end()) {
            child = previous->second;
            child.recycled = false;
        } else if (!list.freeIdentities.empty()) {
            child.identity = list.freeIdentities.front();
            list.freeIdentities.erase(list.freeIdentities.begin());
            child.recycled = true;
        } else {
            child.identity = list.nextIdentity;
            ++list.nextIdentity;
            child.recycled = false;
        }
        child.key = item.key;
        child.value = item.value.empty() ? item.key : item.value;
        child.kind = list.templateKind;
        child.color = item.color == 0 ? RoboListPalette(index) : item.color;
        next.push_back(std::move(child));
    }
    list.children = std::move(next);
    RoboListPlace(list);
    diagnostic.clear();
    return true;
}

bool RoboListRead(const RoboXMLNode& node, RoboListNode& list, std::string& diagnostic) {
    list = RoboListNode{};
    diagnostic.clear();
    if (RoboListLower(node.name) != "list") {
        diagnostic = RoboListFail("R8204", "'" + node.name + "' is not a List.");
        return false;
    }
    for (const auto& entry : node.attrs) {
        if (!RoboListKnown(entry.first)) {
            diagnostic = RoboListFail("R8201", "List attribute '" + entry.first + "' is unknown. Use items, key, and template.");
            return false;
        }
    }
    const std::string keyField = RoboXMLAttr(node, "key");
    list.keyField = keyField.empty() ? "key" : RoboListLower(keyField);
    const std::string numbered = RoboXMLAttr(node, "numbered");
    if (!numbered.empty() && !RoboListFlag(numbered, list.numbered)) {
        diagnostic = RoboListFail("R8202", "List numbered '" + numbered + "' is unknown. Use true or false.");
        return false;
    }
    const std::string width = RoboXMLAttr(node, "width");
    if (!width.empty() && !RoboListInteger(width, list.width)) {
        diagnostic = RoboListFail("R8203", "List width '" + width + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    const std::string height = RoboXMLAttr(node, "height");
    if (!height.empty() && !RoboListInteger(height, list.height)) {
        diagnostic = RoboListFail("R8203", "List height '" + height + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    const std::string itemHeight = RoboXMLAttr(node, "itemheight");
    const std::string itemHeightAlias = itemHeight.empty() ? RoboXMLAttr(node, "item-height") : itemHeight;
    if (!itemHeightAlias.empty() && !RoboListInteger(itemHeightAlias, list.itemHeight)) {
        diagnostic = RoboListFail("R8203", "List item height '" + itemHeightAlias + "' is not a size. Use a whole number of pixels.");
        return false;
    }
    const std::string gap = RoboXMLAttr(node, "gap");
    if (!gap.empty()) {
        if (!RoboListInteger(gap, list.gap) && gap != "0") {
            diagnostic = RoboListFail("R8203", "List gap '" + gap + "' is not a size. Use a whole number of pixels.");
            return false;
        }
        if (gap == "0") {
            list.gap = 0;
        }
    }
    const RoboXMLNode* templateNode = RoboListFindChild(node, "template");
    if (RoboListCountChild(node, "template") > 1) {
        diagnostic = RoboListFail("R8202", "List has more than one Template.");
        return false;
    }
    const std::string templateName = RoboXMLAttr(node, "template");
    if (!templateName.empty() && templateNode != nullptr) {
        diagnostic = RoboListFail("R8202", "List template is set twice. Use the attribute or the Template child.");
        return false;
    }
    if (!templateName.empty()) {
        list.templateKind = templateName;
        if (!RoboListTemplateName(list.templateKind)) {
            diagnostic = RoboListFail("R8202", "List template '" + templateName + "' is unknown. Use an earlier primitive.");
            return false;
        }
    } else if (templateNode != nullptr) {
        if (templateNode->children.size() != 1) {
            diagnostic = RoboListFail("R8202", "List Template needs one child, and that child is the repeated primitive.");
            return false;
        }
        list.templateKind = templateNode->children[0].name;
        if (!RoboListTemplateName(list.templateKind)) {
            diagnostic = RoboListFail("R8202", "List template '" + templateNode->children[0].name + "' is unknown. Use an earlier primitive.");
            return false;
        }
    }
    const std::string itemsText = RoboXMLAttr(node, "items");
    const int itemNodes = RoboListCountChild(node, "item");
    if (!itemsText.empty() && itemNodes > 0) {
        diagnostic = RoboListFail("R8202", "List items are set twice. Use the attribute or Item children.");
        return false;
    }
    std::vector<RoboListItem> items;
    if (!itemsText.empty()) {
        std::size_t cursor = 0;
        while (cursor <= itemsText.size()) {
            const std::size_t comma = itemsText.find(',', cursor);
            const std::string token = RoboListTrim(itemsText.substr(cursor, comma == std::string::npos ? std::string::npos : comma - cursor));
            if (token.empty()) {
                diagnostic = RoboListFail("R8202", "List items contain an empty entry.");
                return false;
            }
            RoboListItem item;
            item.key = token;
            item.value = token;
            items.push_back(std::move(item));
            if (comma == std::string::npos) {
                break;
            }
            cursor = comma + 1;
        }
    }
    for (const RoboXMLNode& child : node.children) {
        if (RoboListLower(child.name) != "item") {
            continue;
        }
        RoboListItem item;
        item.key = RoboListTrim(RoboXMLAttr(child, list.keyField.c_str()));
        if (item.key.empty()) {
            diagnostic = RoboListFail("R8202", "List item is missing '" + list.keyField + "'.");
            return false;
        }
        item.value = RoboXMLAttr(child, "value");
        if (item.value.empty()) {
            item.value = item.key;
        }
        const std::string color = RoboXMLAttr(child, "color");
        if (!color.empty() && !RoboListColor(color, item.color)) {
            diagnostic = RoboListFail("R8202", "List item color '" + color + "' is not a color. Use #RRGGBB.");
            return false;
        }
        items.push_back(std::move(item));
    }
    return RoboListApply(list, items, diagnostic);
}

bool RoboListHit(const RoboListNode& list, int x, int y, std::string& key) {
    key.clear();
    const int limit = list.height > 0 ? list.height : list.contentHeight;
    if (!RoboListInside(x, y, 0, 0, list.width, limit)) {
        return false;
    }
    for (const RoboListChild& child : list.children) {
        int top = child.y;
        int height = child.height;
        if (top >= limit) {
            continue;
        }
        if (top + height > limit) {
            height = limit - top;
        }
        if (RoboListInside(x, y, child.x, top, child.width, height)) {
            key = child.key;
            return true;
        }
    }
    return false;
}

void RoboListPaint(const RoboListNode& list, RoboListFrame& frame) {
    frame = RoboListFrame{};
    frame.width = list.width;
    frame.height = list.height > 0 ? list.height : list.contentHeight;
    frame.pixels.assign(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height), ROBO_LIST_PLATE);
    for (const RoboListChild& child : list.children) {
        const int top = std::max(0, child.y);
        const int bottom = std::min(frame.height, child.y + child.height);
        for (int row = top; row < bottom; ++row) {
            for (int column = 0; column < frame.width; ++column) {
                frame.pixels[static_cast<std::size_t>(row) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(column)] = child.color;
            }
        }
    }
}

bool RoboListPixel(const RoboListFrame& frame, int x, int y, RoboU32& color) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) {
        return false;
    }
    color = frame.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x)];
    return true;
}

bool RoboListMaterialize(RoboXMLNode& node, std::string& diagnostic) {
    const bool hasItems = !RoboXMLAttr(node, "items").empty();
    const bool hasItemNodes = RoboListCountChild(node, "item") > 0;
    if (RoboListLower(node.name) == "list" && !hasItems && !hasItemNodes) {
        diagnostic.clear();
        return true;
    }
    RoboListNode list;
    if (!RoboListRead(node, list, diagnostic)) {
        return false;
    }
    const RoboXMLNode* templateNode = RoboListFindChild(node, "template");
    RoboXMLNode prototype;
    const bool cloned = templateNode != nullptr && !templateNode->children.empty();
    if (cloned) {
        prototype = templateNode->children[0];
    }
    std::vector<RoboXMLNode> built;
    built.reserve(list.children.size());
    std::size_t itemIndex = 0;
    for (const RoboXMLNode& child : node.children) {
        if (RoboListLower(child.name) != "item") {
            continue;
        }
        if (itemIndex >= list.children.size()) {
            break;
        }
        const RoboListChild& row = list.children[itemIndex];
        ++itemIndex;
        std::unordered_map<std::string, std::string> fields;
        fields["key"] = row.key;
        fields["value"] = row.value;
        for (const auto& entry : child.attrs) {
            fields[RoboListLower(entry.first)] = entry.second;
        }
        RoboXMLNode element;
        if (cloned) {
            element = prototype;
            RoboListStamp(element, fields);
        } else {
            element.name = RoboListTag(list.templateKind);
            element.attrs["value"] = row.value;
        }
        element.attrs["key"] = row.key;
        built.push_back(std::move(element));
    }
    if (!hasItemNodes) {
        for (const RoboListChild& row : list.children) {
            std::unordered_map<std::string, std::string> fields;
            fields["key"] = row.key;
            fields["value"] = row.value;
            RoboXMLNode element;
            if (cloned) {
                element = prototype;
                RoboListStamp(element, fields);
            } else {
                element.name = RoboListTag(list.templateKind);
                element.attrs["value"] = row.value;
            }
            element.attrs["key"] = row.key;
            built.push_back(std::move(element));
        }
    }
    node.children = std::move(built);
    diagnostic.clear();
    return true;
}
