#include "RoboEvent.hpp"

#include <cctype>
#include <utility>

namespace {

const char* const ROBO_EVENT_TARGET_ATTRIBUTES[] = {
    "id", "name", "x", "y", "width", "height", "visible",
};

const char* const ROBO_EVENT_STEP_ATTRIBUTES[] = {
    "x", "y", "id", "key", "value", "time", "width", "height", "visible", "offset", "scale", "angle", "direction",
};

struct RoboEventAlias {
    const char* name;
    const char* tag;
};

const RoboEventAlias ROBO_EVENT_TAGS[] = {
    {"screen", "Screen"}, {"window", "Screen"}, {"page", "Screen"},
    {"view", "View"},     {"container", "View"}, {"box", "View"},
    {"text", "Text"},     {"image", "Image"},    {"video", "Video"},
    {"audio", "Audio"},   {"icon", "Icon"},      {"svg", "Svg"},
    {"canvas", "Canvas"}, {"view3d", "View3D"},  {"scene3d", "View3D"},
    {"input", "Input"},   {"textfield", "Input"}, {"scrollview", "ScrollView"},
    {"list", "List"},     {"span", "Span"},      {"at", "At"},
};

std::string RoboEventLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string RoboEventFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

std::string RoboEventAttr(const RoboXMLNode& node, const char* key) {
    return RoboXMLAttr(node, key, "");
}

bool RoboEventListed(const char* const* names, int count, const std::string& key) {
    for (int index = 0; index < count; ++index) {
        if (key == names[index]) {
            return true;
        }
    }
    return false;
}

const char* RoboEventTag(const std::string& name) {
    const std::string key = RoboEventLower(name);
    for (const RoboEventAlias& alias : ROBO_EVENT_TAGS) {
        if (key == alias.name) {
            return alias.tag;
        }
    }
    return nullptr;
}

const char* RoboEventCanonical(const std::string& name) {
    const std::string key = RoboEventLower(name);
    for (const char* event : ROBO_EVENT_NAMES) {
        if (key == RoboEventLower(event)) {
            return event;
        }
    }
    return nullptr;
}

bool RoboEventInteger(const std::string& text, int& value) {
    if (text.empty()) {
        return false;
    }
    std::size_t index = 0;
    int sign = 1;
    if (text[0] == '+' || text[0] == '-') {
        if (text.size() == 1) {
            return false;
        }
        if (text[0] == '-') {
            sign = -1;
        }
        index = 1;
    }
    int number = 0;
    for (; index < text.size(); ++index) {
        const unsigned char ch = static_cast<unsigned char>(text[index]);
        if (std::isdigit(ch) == 0) {
            return false;
        }
        number = number * 10 + (text[index] - '0');
    }
    value = number * sign;
    return true;
}

bool RoboEventVisible(const std::string& text, bool& visible) {
    const std::string key = RoboEventLower(text);
    if (key == "true" || key == "yes" || key == "1") {
        visible = true;
        return true;
    }
    if (key == "false" || key == "no" || key == "0") {
        visible = false;
        return true;
    }
    return false;
}

bool RoboEventUnique(const RoboEventNode& node, std::vector<std::string>& ids, std::string& diagnostic) {
    if (!node.id.empty()) {
        for (const std::string& id : ids) {
            if (id == node.id) {
                diagnostic = RoboEventFail("R8302", "a repeated id is rejected");
                return false;
            }
        }
        ids.push_back(node.id);
    }
    for (const RoboEventNode& child : node.children) {
        if (!RoboEventUnique(child, ids, diagnostic)) {
            return false;
        }
    }
    return true;
}

bool RoboEventReadNode(const RoboXMLNode& node, RoboEventNode& target, std::string& diagnostic) {
    const char* tag = RoboEventTag(node.name);
    if (tag == nullptr) {
        diagnostic = RoboEventFail("R8304", "the wrong tag is rejected");
        return false;
    }
    target.tag = tag;
    target.id = RoboEventAttr(node, "id");
    if (target.id.empty()) {
        target.id = RoboEventAttr(node, "name");
    }
    for (const auto& entry : node.attrs) {
        const std::string key = RoboEventLower(entry.first);
        if (RoboEventListed(ROBO_EVENT_TARGET_ATTRIBUTES,
                            static_cast<int>(sizeof(ROBO_EVENT_TARGET_ATTRIBUTES) / sizeof(ROBO_EVENT_TARGET_ATTRIBUTES[0])),
                            key)) {
            continue;
        }
        const char* event = RoboEventCanonical(entry.first);
        if (event == nullptr) {
            diagnostic = RoboEventFail("R8301", "unknown attribute " + entry.first);
            return false;
        }
        if (entry.second.empty()) {
            diagnostic = RoboEventFail("R8302", "an empty handler is rejected");
            return false;
        }
        for (const auto& handler : target.handlers) {
            if (handler.first == event) {
                diagnostic = RoboEventFail("R8302", "a repeated handler is rejected");
                return false;
            }
        }
        target.handlers.emplace_back(event, entry.second);
    }
    const std::string xText = RoboEventAttr(node, "x");
    const std::string yText = RoboEventAttr(node, "y");
    const std::string widthText = RoboEventAttr(node, "width");
    const std::string heightText = RoboEventAttr(node, "height");
    if (!xText.empty() && (!RoboEventInteger(xText, target.x) || target.x < 0)) {
        diagnostic = RoboEventFail("R8302", "a bad origin is rejected");
        return false;
    }
    if (!yText.empty() && (!RoboEventInteger(yText, target.y) || target.y < 0)) {
        diagnostic = RoboEventFail("R8302", "a bad origin is rejected");
        return false;
    }
    if (!widthText.empty() && (!RoboEventInteger(widthText, target.width) || target.width < 0)) {
        diagnostic = RoboEventFail("R8302", "a bad size is rejected");
        return false;
    }
    if (!heightText.empty() && (!RoboEventInteger(heightText, target.height) || target.height < 0)) {
        diagnostic = RoboEventFail("R8302", "a bad size is rejected");
        return false;
    }
    const std::string visibleText = RoboEventAttr(node, "visible");
    if (!visibleText.empty() && !RoboEventVisible(visibleText, target.visible)) {
        diagnostic = RoboEventFail("R8302", "a bad visible value is rejected");
        return false;
    }
    for (const RoboXMLNode& child : node.children) {
        RoboEventNode next;
        if (!RoboEventReadNode(child, next, diagnostic)) {
            return false;
        }
        target.children.push_back(std::move(next));
    }
    return true;
}

bool RoboEventReadStep(const RoboXMLNode& node, RoboEventStep& step, std::string& diagnostic) {
    if (!node.children.empty()) {
        diagnostic = RoboEventFail("R8304", "a script step has no children");
        return false;
    }
    step.kind = RoboEventLower(node.name);
    static const char* kinds[] = {
        "click", "doubleclick", "hover", "mousedown", "mouseup", "contextmenu", "tap", "longpress", "swipe",
        "drag", "drop", "pinch", "rotate", "keydown", "keyup", "focus", "blur", "submit", "change", "input",
        "valuechange", "selectionchange", "mount", "unmount", "update", "resize", "layout", "visibilitychange",
        "scroll", "scrollstart", "scrollend", "load", "error", "play", "pause", "end", "timeupdate",
    };
    if (!RoboEventListed(kinds, static_cast<int>(sizeof(kinds) / sizeof(kinds[0])), step.kind)) {
        diagnostic = RoboEventFail("R8302", "an unknown script step is rejected");
        return false;
    }
    for (const auto& entry : node.attrs) {
        const std::string key = RoboEventLower(entry.first);
        if (!RoboEventListed(ROBO_EVENT_STEP_ATTRIBUTES,
                             static_cast<int>(sizeof(ROBO_EVENT_STEP_ATTRIBUTES) / sizeof(ROBO_EVENT_STEP_ATTRIBUTES[0])),
                             key)) {
            diagnostic = RoboEventFail("R8301", "unknown attribute " + entry.first);
            return false;
        }
    }
    const std::string xText = RoboEventAttr(node, "x");
    const std::string yText = RoboEventAttr(node, "y");
    if (!xText.empty() || !yText.empty()) {
        if (!RoboEventInteger(xText, step.x) || !RoboEventInteger(yText, step.y) || step.x < 0 || step.y < 0) {
            diagnostic = RoboEventFail("R8302", "a bad point is rejected");
            return false;
        }
        step.hasPoint = true;
    }
    step.id = RoboEventAttr(node, "id");
    step.key = RoboEventAttr(node, "key");
    step.value = RoboEventAttr(node, "value");
    if (step.value.empty()) {
        step.value = RoboEventAttr(node, "offset");
    }
    if (step.value.empty()) {
        step.value = RoboEventAttr(node, "scale");
    }
    if (step.value.empty()) {
        step.value = RoboEventAttr(node, "angle");
    }
    if (step.value.empty()) {
        step.value = RoboEventAttr(node, "direction");
    }
    if (step.value.empty()) {
        step.value = RoboEventAttr(node, "time");
        if (!step.value.empty() && step.kind != "timeupdate") {
            step.value.clear();
        }
    }
    const std::string timeText = RoboEventAttr(node, "time");
    if (!timeText.empty() && step.kind != "timeupdate") {
        if (!RoboEventInteger(timeText, step.time) || step.time < 0) {
            diagnostic = RoboEventFail("R8302", "a bad time is rejected");
            return false;
        }
    }
    const std::string widthText = RoboEventAttr(node, "width");
    const std::string heightText = RoboEventAttr(node, "height");
    if (!widthText.empty() || !heightText.empty()) {
        if (!RoboEventInteger(widthText, step.width) || !RoboEventInteger(heightText, step.height) || step.width < 0 ||
            step.height < 0) {
            diagnostic = RoboEventFail("R8302", "a bad size is rejected");
            return false;
        }
        step.hasSize = true;
    }
    const std::string visibleText = RoboEventAttr(node, "visible");
    if (!visibleText.empty()) {
        bool visible = true;
        if (!RoboEventVisible(visibleText, visible)) {
            diagnostic = RoboEventFail("R8302", "a bad visible value is rejected");
            return false;
        }
        step.visible = visible ? 1 : 0;
    }
    const bool pointer = step.kind == "click" || step.kind == "doubleclick" || step.kind == "hover" ||
                         step.kind == "mousedown" || step.kind == "mouseup" || step.kind == "contextmenu" ||
                         step.kind == "tap" || step.kind == "longpress" || step.kind == "swipe" || step.kind == "drag" ||
                         step.kind == "drop" || step.kind == "pinch" || step.kind == "rotate";
    if (pointer && !step.hasPoint) {
        diagnostic = RoboEventFail("R8302", "a pointer step needs a point");
        return false;
    }
    if ((step.kind == "pinch" || step.kind == "rotate" || step.kind == "swipe") && step.value.empty()) {
        diagnostic = RoboEventFail("R8302", "a gesture step needs its value");
        return false;
    }
    if (step.kind == "pinch" || step.kind == "rotate") {
        int number = 0;
        if (!RoboEventInteger(step.value, number)) {
            diagnostic = RoboEventFail("R8302", "a bad gesture value is rejected");
            return false;
        }
    }
    if (step.kind == "swipe") {
        const std::string direction = RoboEventLower(step.value);
        if (direction != "left" && direction != "right" && direction != "up" && direction != "down") {
            diagnostic = RoboEventFail("R8302", "a bad swipe direction is rejected");
            return false;
        }
        step.value = direction;
    }
    if (step.kind == "resize" && (!step.hasSize || step.id.empty())) {
        diagnostic = RoboEventFail("R8302", "a resize needs an id and a size");
        return false;
    }
    if (step.kind == "visibilitychange" && (step.visible < 0 || step.id.empty())) {
        diagnostic = RoboEventFail("R8302", "a visibility change needs an id and a value");
        return false;
    }
    const bool needsId = step.kind == "unmount" || step.kind == "change" || step.kind == "input" ||
                         step.kind == "valuechange" || step.kind == "selectionchange" || step.kind == "scroll" ||
                         step.kind == "scrollstart" || step.kind == "scrollend" || step.kind == "load" ||
                         step.kind == "error" || step.kind == "play" || step.kind == "pause" || step.kind == "end" ||
                         step.kind == "timeupdate";
    if (needsId && step.id.empty()) {
        diagnostic = RoboEventFail("R8302", "this step needs an id");
        return false;
    }
    return true;
}

bool RoboEventReadRoots(const RoboXMLNode& node, RoboEventDocument& document, std::string& diagnostic) {
    int scripts = 0;
    for (const RoboXMLNode& child : node.children) {
        if (RoboEventLower(child.name) == "script") {
            ++scripts;
            if (scripts > 1) {
                diagnostic = RoboEventFail("R8302", "the script cannot be set twice");
                return false;
            }
            for (const auto& entry : child.attrs) {
                diagnostic = RoboEventFail("R8301", "unknown attribute " + entry.first);
                return false;
            }
            for (const RoboXMLNode& stepNode : child.children) {
                RoboEventStep step;
                if (!RoboEventReadStep(stepNode, step, diagnostic)) {
                    return false;
                }
                document.steps.push_back(std::move(step));
            }
            continue;
        }
        RoboEventNode target;
        if (!RoboEventReadNode(child, target, diagnostic)) {
            return false;
        }
        document.roots.push_back(std::move(target));
    }
    std::vector<std::string> ids;
    for (const RoboEventNode& root : document.roots) {
        if (!RoboEventUnique(root, ids, diagnostic)) {
            return false;
        }
    }
    return true;
}

std::string RoboEventHandler(const RoboEventNode& node, const char* name) {
    for (const auto& handler : node.handlers) {
        if (handler.first == name) {
            return handler.second;
        }
    }
    return {};
}

void RoboEventDeliver(const std::vector<RoboEventNode*>& path, const char* name, int x, int y, const std::string& key,
                      const std::string& value, std::vector<RoboEventDelivery>& deliveries) {
    if (path.empty()) {
        return;
    }
    const std::string hitId = path.back()->id;
    for (std::size_t index = path.size(); index > 0; --index) {
        const RoboEventNode* node = path[index - 1];
        RoboEventDelivery delivery;
        delivery.name = name;
        delivery.hitId = hitId;
        delivery.targetId = node->id;
        delivery.handler = RoboEventHandler(*node, name);
        delivery.key = key;
        delivery.value = value;
        delivery.x = x;
        delivery.y = y;
        deliveries.push_back(std::move(delivery));
    }
}

bool RoboEventTakeChild(RoboEventNode& node, int x, int y, std::vector<RoboEventNode*>& path) {
    for (std::size_t index = node.children.size(); index > 0; --index) {
        RoboEventNode& child = node.children[index - 1];
        if (!child.visible) {
            continue;
        }
        std::vector<RoboEventNode*> nested;
        if (RoboEventTakeChild(child, x, y, nested)) {
            path.push_back(&node);
            path.insert(path.end(), nested.begin(), nested.end());
            return true;
        }
        if (child.width > 0 && child.height > 0 && x >= child.x && y >= child.y && x < child.x + child.width &&
            y < child.y + child.height) {
            path.push_back(&node);
            path.push_back(&child);
            return true;
        }
    }
    return false;
}

bool RoboEventTake(std::vector<RoboEventNode>& roots, int x, int y, std::vector<RoboEventNode*>& path) {
    for (std::size_t index = roots.size(); index > 0; --index) {
        RoboEventNode& root = roots[index - 1];
        if (!root.visible) {
            continue;
        }
        std::vector<RoboEventNode*> nested;
        if (RoboEventTakeChild(root, x, y, nested)) {
            path = std::move(nested);
            return true;
        }
        if (root.width > 0 && root.height > 0 && x >= root.x && y >= root.y && x < root.x + root.width &&
            y < root.y + root.height) {
            path = {&root};
            return true;
        }
    }
    return false;
}

bool RoboEventFindPath(std::vector<RoboEventNode>& nodes, const std::string& id, std::vector<RoboEventNode*>& path) {
    for (RoboEventNode& node : nodes) {
        path.push_back(&node);
        if (node.id == id) {
            return true;
        }
        if (RoboEventFindPath(node.children, id, path)) {
            return true;
        }
        path.pop_back();
    }
    return false;
}

bool RoboEventErase(std::vector<RoboEventNode>& nodes, const std::string& id) {
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        if (nodes[index].id == id) {
            nodes.erase(nodes.begin() + static_cast<std::ptrdiff_t>(index));
            return true;
        }
        if (RoboEventErase(nodes[index].children, id)) {
            return true;
        }
    }
    return false;
}

void RoboEventVisit(RoboEventNode& node, const char* name, std::vector<RoboEventNode*>& path,
                    std::vector<RoboEventDelivery>& deliveries) {
    path.push_back(&node);
    RoboEventDeliver(path, name, node.x, node.y, "", "", deliveries);
    for (RoboEventNode& child : node.children) {
        RoboEventVisit(child, name, path, deliveries);
    }
    path.pop_back();
}

bool RoboEventPoint(RoboEventDocument& document, int x, int y, const char* name, const std::string& value,
                    std::vector<RoboEventDelivery>& deliveries) {
    std::vector<RoboEventNode*> path;
    if (!RoboEventTake(document.roots, x, y, path)) {
        return true;
    }
    RoboEventDeliver(path, name, x, y, "", value, deliveries);
    return true;
}

void RoboEventNoteClick(RoboEventDocument& document, const std::string& hitId, int x, int y,
                        std::vector<RoboEventNode*>& path, std::vector<RoboEventDelivery>& deliveries) {
    if (!document.lastClickId.empty() && document.lastClickId == hitId &&
        document.clock - document.lastClickTime <= ROBO_EVENT_DOUBLE_MS) {
        RoboEventDeliver(path, "OnDoubleClick", x, y, "", "", deliveries);
    }
    document.lastClickId = hitId;
    document.lastClickTime = document.clock;
}

bool RoboEventTargeted(RoboEventDocument& document, const std::string& id, const char* name, const std::string& key,
                       const std::string& value, std::vector<RoboEventDelivery>& deliveries, std::string& diagnostic) {
    std::vector<RoboEventNode*> path;
    if (!RoboEventFindPath(document.roots, id, path)) {
        diagnostic = RoboEventFail("R8302", "the id is missing");
        return false;
    }
    RoboEventDeliver(path, name, path.back()->x, path.back()->y, key, value, deliveries);
    return true;
}

bool RoboEventPlayStep(RoboEventDocument& document, const RoboEventStep& step, std::vector<RoboEventDelivery>& deliveries,
                       std::string& diagnostic) {
    if (step.time >= 0) {
        document.clock = step.time;
    } else {
        document.clock += 1000;
    }
    if (step.kind == "click" || step.kind == "mousedown") {
        if (!RoboEventPoint(document, step.x, step.y, "OnMouseDown", "", deliveries)) {
            return false;
        }
        std::vector<RoboEventNode*> path;
        if (RoboEventTake(document.roots, step.x, step.y, path)) {
            document.pointerDown = true;
            document.pointerId = path.back()->id;
        } else {
            document.pointerDown = false;
            document.pointerId.clear();
        }
        if (step.kind == "mousedown") {
            return true;
        }
    }
    if (step.kind == "click" || step.kind == "mouseup") {
        std::vector<RoboEventNode*> path;
        const bool hit = RoboEventTake(document.roots, step.x, step.y, path);
        if (hit) {
            RoboEventDeliver(path, "OnMouseUp", step.x, step.y, "", "", deliveries);
        }
        if (hit && document.pointerDown && document.pointerId == path.back()->id) {
            RoboEventDeliver(path, "OnClick", step.x, step.y, "", "", deliveries);
            RoboEventNoteClick(document, path.back()->id, step.x, step.y, path, deliveries);
        }
        document.pointerDown = false;
        document.pointerId.clear();
        return true;
    }
    if (step.kind == "doubleclick") {
        return RoboEventPoint(document, step.x, step.y, "OnDoubleClick", "", deliveries);
    }
    if (step.kind == "hover") {
        return RoboEventPoint(document, step.x, step.y, "OnHover", "", deliveries);
    }
    if (step.kind == "contextmenu") {
        return RoboEventPoint(document, step.x, step.y, "OnContextMenu", "", deliveries);
    }
    if (step.kind == "tap") {
        return RoboEventPoint(document, step.x, step.y, "OnTap", "", deliveries);
    }
    if (step.kind == "longpress") {
        return RoboEventPoint(document, step.x, step.y, "OnLongPress", "", deliveries);
    }
    if (step.kind == "swipe") {
        return RoboEventPoint(document, step.x, step.y, "OnSwipe", step.value, deliveries);
    }
    if (step.kind == "drag") {
        std::vector<RoboEventNode*> path;
        if (RoboEventTake(document.roots, step.x, step.y, path)) {
            document.dragId = path.back()->id;
            RoboEventDeliver(path, "OnDrag", step.x, step.y, "", "", deliveries);
        }
        return true;
    }
    if (step.kind == "drop") {
        return RoboEventPoint(document, step.x, step.y, "OnDrop", document.dragId, deliveries);
    }
    if (step.kind == "pinch") {
        return RoboEventPoint(document, step.x, step.y, "OnPinch", step.value, deliveries);
    }
    if (step.kind == "rotate") {
        return RoboEventPoint(document, step.x, step.y, "OnRotate", step.value, deliveries);
    }
    if (step.kind == "focus") {
        const std::string id = step.id.empty() ? document.focusId : step.id;
        if (id.empty()) {
            diagnostic = RoboEventFail("R8302", "focus needs an id");
            return false;
        }
        if (document.focusId == id) {
            return true;
        }
        if (!document.focusId.empty() &&
            !RoboEventTargeted(document, document.focusId, "OnBlur", "", "", deliveries, diagnostic)) {
            return false;
        }
        document.focusId = id;
        return RoboEventTargeted(document, id, "OnFocus", "", "", deliveries, diagnostic);
    }
    if (step.kind == "blur") {
        if (document.focusId.empty()) {
            return true;
        }
        const std::string id = document.focusId;
        document.focusId.clear();
        return RoboEventTargeted(document, id, "OnBlur", "", "", deliveries, diagnostic);
    }
    if (step.kind == "keydown" || step.kind == "keyup" || step.kind == "submit") {
        const std::string id = step.id.empty() ? document.focusId : step.id;
        if (id.empty()) {
            diagnostic = RoboEventFail("R8302", "a key step needs a focused id");
            return false;
        }
        const char* name = "OnKeyDown";
        if (step.kind == "keyup") {
            name = "OnKeyUp";
        } else if (step.kind == "submit") {
            name = "OnSubmit";
        }
        return RoboEventTargeted(document, id, name, step.key, step.value, deliveries, diagnostic);
    }
    if (step.kind == "change" || step.kind == "input" || step.kind == "valuechange" || step.kind == "selectionchange") {
        const char* name = "OnChange";
        if (step.kind == "input") {
            name = "OnInput";
        } else if (step.kind == "valuechange") {
            name = "OnValueChange";
        } else if (step.kind == "selectionchange") {
            name = "OnSelectionChange";
        }
        return RoboEventTargeted(document, step.id, name, "", step.value, deliveries, diagnostic);
    }
    if (step.kind == "mount" || step.kind == "update" || step.kind == "layout") {
        const char* name = "OnMount";
        if (step.kind == "update") {
            name = "OnUpdate";
        } else if (step.kind == "layout") {
            name = "OnLayout";
        }
        if (!step.id.empty()) {
            return RoboEventTargeted(document, step.id, name, "", "", deliveries, diagnostic);
        }
        for (RoboEventNode& root : document.roots) {
            std::vector<RoboEventNode*> path;
            RoboEventVisit(root, name, path, deliveries);
        }
        return true;
    }
    if (step.kind == "unmount") {
        std::vector<RoboEventNode*> path;
        if (!RoboEventFindPath(document.roots, step.id, path)) {
            diagnostic = RoboEventFail("R8302", "the id is missing");
            return false;
        }
        std::vector<RoboEventNode*> walk(path.begin(), path.end() - 1);
        RoboEventVisit(*path.back(), "OnUnmount", walk, deliveries);
        if (document.focusId == step.id) {
            document.focusId.clear();
        }
        if (document.dragId == step.id) {
            document.dragId.clear();
        }
        RoboEventErase(document.roots, step.id);
        return true;
    }
    if (step.kind == "resize") {
        std::vector<RoboEventNode*> path;
        if (!RoboEventFindPath(document.roots, step.id, path)) {
            diagnostic = RoboEventFail("R8302", "the id is missing");
            return false;
        }
        RoboEventNode* node = path.back();
        if (node->width == step.width && node->height == step.height) {
            return true;
        }
        node->width = step.width;
        node->height = step.height;
        RoboEventDeliver(path, "OnResize", node->x, node->y, "", std::to_string(step.width) + "," + std::to_string(step.height),
                         deliveries);
        return true;
    }
    if (step.kind == "visibilitychange") {
        std::vector<RoboEventNode*> path;
        if (!RoboEventFindPath(document.roots, step.id, path)) {
            diagnostic = RoboEventFail("R8302", "the id is missing");
            return false;
        }
        RoboEventNode* node = path.back();
        const bool visible = step.visible == 1;
        if (node->visible == visible) {
            return true;
        }
        node->visible = visible;
        RoboEventDeliver(path, "OnVisibilityChange", node->x, node->y, "", visible ? "true" : "false", deliveries);
        return true;
    }
    if (step.kind == "scroll" || step.kind == "scrollstart" || step.kind == "scrollend") {
        const char* name = "OnScroll";
        if (step.kind == "scrollstart") {
            name = "OnScrollStart";
        } else if (step.kind == "scrollend") {
            name = "OnScrollEnd";
        }
        return RoboEventTargeted(document, step.id, name, "", step.value, deliveries, diagnostic);
    }
    const char* media = "OnLoad";
    if (step.kind == "error") {
        media = "OnError";
    } else if (step.kind == "play") {
        media = "OnPlay";
    } else if (step.kind == "pause") {
        media = "OnPause";
    } else if (step.kind == "end") {
        media = "OnEnd";
    } else if (step.kind == "timeupdate") {
        media = "OnTimeUpdate";
    }
    return RoboEventTargeted(document, step.id, media, "", step.value, deliveries, diagnostic);
}

}  // namespace

bool RoboEventRead(const RoboXMLNode& node, RoboEventDocument& document, std::string& diagnostic) {
    document = RoboEventDocument();
    if (RoboEventLower(node.name) == "events") {
        for (const auto& entry : node.attrs) {
            diagnostic = RoboEventFail("R8301", "unknown attribute " + entry.first);
            return false;
        }
        return RoboEventReadRoots(node, document, diagnostic);
    }
    RoboEventNode target;
    if (!RoboEventReadNode(node, target, diagnostic)) {
        return false;
    }
    document.roots.push_back(std::move(target));
    std::vector<std::string> ids;
    return RoboEventUnique(document.roots[0], ids, diagnostic);
}

bool RoboEventPlay(RoboEventDocument& document, std::vector<RoboEventDelivery>& deliveries, std::string& diagnostic) {
    deliveries.clear();
    for (const RoboEventStep& step : document.steps) {
        if (!RoboEventPlayStep(document, step, deliveries, diagnostic)) {
            return false;
        }
    }
    return true;
}

bool RoboEventHit(const RoboEventDocument& document, int x, int y, std::string& id) {
    RoboEventDocument editable = document;
    std::vector<RoboEventNode*> path;
    if (!RoboEventTake(editable.roots, x, y, path)) {
        id.clear();
        return false;
    }
    id = path.back()->id;
    return true;
}

const RoboEventDelivery* RoboEventFind(const std::vector<RoboEventDelivery>& deliveries, const char* name) {
    for (const RoboEventDelivery& delivery : deliveries) {
        if (delivery.name == name) {
            return &delivery;
        }
    }
    return nullptr;
}
