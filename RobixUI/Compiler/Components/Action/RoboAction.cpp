#include "RoboAction.hpp"

#include <cctype>

namespace {

const char* const ROBO_ACTION_KINDS[] = {
    "SetState", "Conditional", "Show", "Hide", "Enable", "Disable", "Toggle", "Navigate", "Focus", "Blur",
    "ScrollTo", "Validate", "Submit", "Reset", "Execute", "Delay", "Sequence", "Parallel",
};

const char* const ROBO_ACTION_EVENT_STEPS[] = {
    "click", "doubleclick", "hover", "mousedown", "mouseup", "contextmenu", "tap", "longpress", "swipe", "drag", "drop",
    "pinch", "rotate", "keydown", "keyup", "change", "input", "valuechange", "selectionchange", "mount", "unmount",
    "update", "resize", "layout", "visibilitychange", "scroll", "scrollstart", "scrollend", "load", "error", "play",
    "pause", "end", "timeupdate",
};

const char* const ROBO_ACTION_STRIPPED[] = {
    "enabled", "disabled", "value", "path", "required", "offset", "scroll",
};

std::string RoboActionLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string RoboActionFail(const char* code, const std::string& message) {
    return std::string(code) + ": " + message;
}

std::string RoboActionAttr(const RoboXMLNode& node, const char* key) {
    return RoboXMLAttr(node, key, "");
}

bool RoboActionListed(const char* const* names, int count, const std::string& key) {
    for (int index = 0; index < count; ++index) {
        if (key == names[index]) {
            return true;
        }
    }
    return false;
}

const char* RoboActionKind(const std::string& name) {
    const std::string key = RoboActionLower(name);
    if (key == "run") {
        return "Execute";
    }
    for (const char* kind : ROBO_ACTION_KINDS) {
        if (key == RoboActionLower(kind)) {
            return kind;
        }
    }
    return nullptr;
}

bool RoboActionInteger(const std::string& text, int& value) {
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
        if (std::isdigit(static_cast<unsigned char>(text[index])) == 0) {
            return false;
        }
        number = number * 10 + (text[index] - '0');
    }
    value = number * sign;
    return true;
}

bool RoboActionBool(const std::string& text, bool& value) {
    const std::string key = RoboActionLower(text);
    if (key == "true" || key == "yes" || key == "1") {
        value = true;
        return true;
    }
    if (key == "false" || key == "no" || key == "0") {
        value = false;
        return true;
    }
    return false;
}

bool RoboActionTruthy(const std::string& value) {
    const std::string key = RoboActionLower(value);
    return !key.empty() && key != "0" && key != "false" && key != "no" && key != "off";
}

bool RoboActionKnownAttr(const std::string& key, const char* const* names, int count) {
    return RoboActionListed(names, count, key);
}

void RoboActionKeepAttr(const RoboXMLNode& node, const char* const* names, int count, std::string& diagnostic, bool& ok) {
    if (!ok) {
        return;
    }
    for (const auto& entry : node.attrs) {
        if (!RoboActionKnownAttr(RoboActionLower(entry.first), names, count)) {
            diagnostic = RoboActionFail("R8401", "unknown attribute " + entry.first);
            ok = false;
            return;
        }
    }
}

RoboXMLNode RoboActionStrip(const RoboXMLNode& node) {
    RoboXMLNode copy = node;
    for (auto it = copy.attrs.begin(); it != copy.attrs.end();) {
        if (RoboActionListed(ROBO_ACTION_STRIPPED,
                             static_cast<int>(sizeof(ROBO_ACTION_STRIPPED) / sizeof(ROBO_ACTION_STRIPPED[0])),
                             RoboActionLower(it->first))) {
            it = copy.attrs.erase(it);
        } else {
            ++it;
        }
    }
    for (RoboXMLNode& child : copy.children) {
        child = RoboActionStrip(child);
    }
    return copy;
}

RoboActionTarget* RoboActionTargetById(RoboActionDocument& document, const std::string& id) {
    for (RoboActionTarget& target : document.targets) {
        if (target.id == id) {
            return &target;
        }
    }
    return nullptr;
}

RoboEventNode* RoboActionEventById(std::vector<RoboEventNode>& nodes, const std::string& id) {
    for (RoboEventNode& node : nodes) {
        if (node.id == id) {
            return &node;
        }
        if (RoboEventNode* child = RoboActionEventById(node.children, id)) {
            return child;
        }
    }
    return nullptr;
}

void RoboActionSync(RoboActionDocument& document, const RoboActionTarget& target) {
    if (target.id.empty()) {
        return;
    }
    if (RoboEventNode* node = RoboActionEventById(document.events.roots, target.id)) {
        node->visible = target.visible && target.enabled;
    }
}

bool RoboActionPut(RoboActionDocument& document, const std::string& name, const std::string& value) {
    for (auto& entry : document.state) {
        if (entry.first == name) {
            entry.second = value;
            return true;
        }
    }
    return false;
}

bool RoboActionZip(RoboEventNode& event, const RoboXMLNode& xml, RoboActionDocument& document, std::string& diagnostic) {
    if (event.children.size() != xml.children.size()) {
        diagnostic = RoboActionFail("R8404", "the target tree does not match");
        return false;
    }
    RoboActionTarget target;
    target.id = event.id;
    target.tag = event.tag;
    target.visible = event.visible;
    target.initialVisible = event.visible;
    const std::string enabledText = RoboActionAttr(xml, "enabled");
    const std::string disabledText = RoboActionAttr(xml, "disabled");
    if (!enabledText.empty() && !disabledText.empty()) {
        diagnostic = RoboActionFail("R8402", "enabled and disabled are both set");
        return false;
    }
    if (!enabledText.empty() && !RoboActionBool(enabledText, target.enabled)) {
        diagnostic = RoboActionFail("R8402", "a bad enabled value is rejected");
        return false;
    }
    if (!disabledText.empty()) {
        bool disabled = false;
        if (!RoboActionBool(disabledText, disabled)) {
            diagnostic = RoboActionFail("R8402", "a bad disabled value is rejected");
            return false;
        }
        target.enabled = !disabled;
    }
    target.initialEnabled = target.enabled;
    const std::string requiredText = RoboActionAttr(xml, "required");
    if (!requiredText.empty() && !RoboActionBool(requiredText, target.required)) {
        diagnostic = RoboActionFail("R8402", "a bad required value is rejected");
        return false;
    }
    target.value = RoboActionAttr(xml, "value");
    target.initialValue = target.value;
    target.path = RoboActionAttr(xml, "path");
    const std::string offsetText = RoboActionAttr(xml, "offset");
    const std::string scrollText = RoboActionAttr(xml, "scroll");
    if (!offsetText.empty() && !scrollText.empty()) {
        diagnostic = RoboActionFail("R8402", "offset and scroll are both set");
        return false;
    }
    const std::string scrollValue = !offsetText.empty() ? offsetText : scrollText;
    if (!scrollValue.empty() && (!RoboActionInteger(scrollValue, target.scroll) || target.scroll < 0)) {
        diagnostic = RoboActionFail("R8402", "a bad offset is rejected");
        return false;
    }
    target.initialScroll = target.scroll;
    if (!target.path.empty()) {
        for (const RoboActionTarget& other : document.targets) {
            if (other.path == target.path) {
                diagnostic = RoboActionFail("R8402", "a repeated path is rejected");
                return false;
            }
        }
        if (document.path.empty()) {
            document.path = target.path;
        }
    }
    if (!target.id.empty() || !target.path.empty()) {
        document.targets.push_back(target);
        event.visible = target.visible && target.enabled;
    }
    for (std::size_t index = 0; index < xml.children.size(); ++index) {
        if (!RoboActionZip(event.children[index], xml.children[index], document, diagnostic)) {
            return false;
        }
    }
    return true;
}

bool RoboActionReadNode(const RoboXMLNode& node, RoboActionNode& action, std::string& diagnostic) {
    const char* kind = RoboActionKind(node.name);
    if (kind == nullptr) {
        diagnostic = RoboActionFail("R8402", "an unknown action is rejected");
        return false;
    }
    action.kind = kind;
    bool ok = true;
    if (action.kind == "SetState") {
        static const char* names[] = {"name", "value"};
        RoboActionKeepAttr(node, names, 2, diagnostic, ok);
        action.name = RoboActionAttr(node, "name");
        action.value = RoboActionAttr(node, "value");
        if (ok && action.name.empty()) {
            diagnostic = RoboActionFail("R8402", "SetState needs a name");
            ok = false;
        }
    } else if (action.kind == "Conditional") {
        static const char* names[] = {"name", "equals", "test"};
        RoboActionKeepAttr(node, names, 3, diagnostic, ok);
        action.name = RoboActionAttr(node, "name");
        action.equals = RoboActionAttr(node, "equals");
        action.value = RoboActionAttr(node, "test");
        if (ok && action.name.empty() && action.value.empty()) {
            diagnostic = RoboActionFail("R8402", "Conditional needs a name or a test");
            ok = false;
        }
        if (ok && !action.name.empty() && !action.value.empty()) {
            diagnostic = RoboActionFail("R8402", "Conditional takes name or test");
            ok = false;
        }
        if (ok && !action.name.empty() && RoboActionAttr(node, "equals").empty() && node.attrs.count("Equals") == 0 &&
            node.attrs.count("equals") == 0) {
            diagnostic = RoboActionFail("R8402", "Conditional needs equals");
            ok = false;
        }
    } else if (action.kind == "Show" || action.kind == "Hide" || action.kind == "Enable" || action.kind == "Disable" ||
               action.kind == "Validate" || action.kind == "Submit") {
        static const char* names[] = {"id"};
        RoboActionKeepAttr(node, names, 1, diagnostic, ok);
        action.id = RoboActionAttr(node, "id");
        if (ok && action.id.empty()) {
            diagnostic = RoboActionFail("R8402", action.kind + " needs an id");
            ok = false;
        }
    } else if (action.kind == "Toggle") {
        static const char* names[] = {"id", "name"};
        RoboActionKeepAttr(node, names, 2, diagnostic, ok);
        action.id = RoboActionAttr(node, "id");
        action.name = RoboActionAttr(node, "name");
        if (action.name.empty()) {
            action.name = "visible";
        }
        action.name = RoboActionLower(action.name);
        if (ok && action.id.empty()) {
            diagnostic = RoboActionFail("R8402", "Toggle needs an id");
            ok = false;
        }
        if (ok && action.name != "visible" && action.name != "enabled") {
            diagnostic = RoboActionFail("R8402", "Toggle needs visible or enabled");
            ok = false;
        }
    } else if (action.kind == "Navigate") {
        static const char* names[] = {"path"};
        RoboActionKeepAttr(node, names, 1, diagnostic, ok);
        action.value = RoboActionAttr(node, "path");
        if (ok && action.value.empty()) {
            diagnostic = RoboActionFail("R8402", "Navigate needs a path");
            ok = false;
        }
    } else if (action.kind == "Focus") {
        static const char* names[] = {"id"};
        RoboActionKeepAttr(node, names, 1, diagnostic, ok);
        action.id = RoboActionAttr(node, "id");
        if (ok && action.id.empty()) {
            diagnostic = RoboActionFail("R8402", "Focus needs an id");
            ok = false;
        }
    } else if (action.kind == "Blur") {
        static const char* names[] = {"id"};
        RoboActionKeepAttr(node, names, 1, diagnostic, ok);
        action.id = RoboActionAttr(node, "id");
    } else if (action.kind == "ScrollTo") {
        static const char* names[] = {"id", "offset"};
        RoboActionKeepAttr(node, names, 2, diagnostic, ok);
        action.id = RoboActionAttr(node, "id");
        action.value = RoboActionAttr(node, "offset");
        int offset = 0;
        if (ok && (action.id.empty() || !RoboActionInteger(action.value, offset) || offset < 0)) {
            diagnostic = RoboActionFail("R8402", "ScrollTo needs an id and an offset");
            ok = false;
        }
        action.delay = offset;
    } else if (action.kind == "Reset") {
        static const char* names[] = {"id", "name"};
        RoboActionKeepAttr(node, names, 2, diagnostic, ok);
        action.id = RoboActionAttr(node, "id");
        action.name = RoboActionAttr(node, "name");
    } else if (action.kind == "Execute") {
        static const char* names[] = {"name"};
        RoboActionKeepAttr(node, names, 1, diagnostic, ok);
        action.name = RoboActionAttr(node, "name");
        if (ok && action.name.empty()) {
            diagnostic = RoboActionFail("R8402", "Execute needs a name");
            ok = false;
        }
    } else if (action.kind == "Delay") {
        static const char* names[] = {"ms"};
        RoboActionKeepAttr(node, names, 1, diagnostic, ok);
        int ms = 0;
        if (ok && (!RoboActionInteger(RoboActionAttr(node, "ms"), ms) || ms < 0)) {
            diagnostic = RoboActionFail("R8402", "Delay needs a time in ms");
            ok = false;
        }
        action.delay = ms;
    } else if (action.kind == "Sequence" || action.kind == "Parallel") {
        if (ok && !node.attrs.empty()) {
            diagnostic = RoboActionFail("R8401", "unknown attribute " + node.attrs.begin()->first);
            ok = false;
        }
    }
    if (!ok) {
        return false;
    }
    if (action.kind != "Conditional" && action.kind != "Sequence" && action.kind != "Parallel" && !node.children.empty()) {
        diagnostic = RoboActionFail("R8404", "this action has no children");
        return false;
    }
    int elses = 0;
    for (const RoboXMLNode& child : node.children) {
        if (action.kind == "Conditional" && RoboActionLower(child.name) == "else") {
            ++elses;
            if (elses > 1) {
                diagnostic = RoboActionFail("R8402", "Conditional has one Else");
                return false;
            }
            if (!child.attrs.empty()) {
                diagnostic = RoboActionFail("R8401", "unknown attribute " + child.attrs.begin()->first);
                return false;
            }
            RoboActionNode branch;
            branch.kind = "Else";
            for (const RoboXMLNode& inner : child.children) {
                RoboActionNode next;
                if (!RoboActionReadNode(inner, next, diagnostic)) {
                    return false;
                }
                branch.children.push_back(std::move(next));
            }
            action.children.push_back(std::move(branch));
            continue;
        }
        if (RoboActionLower(child.name) == "else") {
            diagnostic = RoboActionFail("R8404", "Else belongs to Conditional");
            return false;
        }
        RoboActionNode next;
        if (!RoboActionReadNode(child, next, diagnostic)) {
            return false;
        }
        action.children.push_back(std::move(next));
    }
    return true;
}

bool RoboActionHandlers(const RoboEventNode& node, const RoboActionDocument& document, std::string& diagnostic) {
    for (const auto& handler : node.handlers) {
        bool found = false;
        for (const auto& action : document.actions) {
            if (action.first == handler.second) {
                found = true;
                break;
            }
        }
        if (!found) {
            diagnostic = RoboActionFail("R8402", "unknown action " + handler.second);
            return false;
        }
    }
    for (const RoboEventNode& child : node.children) {
        if (!RoboActionHandlers(child, document, diagnostic)) {
            return false;
        }
    }
    return true;
}

void RoboActionRemember(std::vector<RoboActionTrace>& trace, const RoboActionNode& action, const std::string& value) {
    RoboActionTrace record;
    record.kind = action.kind;
    record.name = action.name;
    record.id = action.id;
    record.value = value.empty() ? action.value : value;
    trace.push_back(std::move(record));
}

bool RoboActionRun(RoboActionDocument& document, const RoboActionNode& action, std::vector<RoboActionTrace>& trace,
                   std::string& diagnostic, int depth);

bool RoboActionRunChildren(RoboActionDocument& document, const std::vector<RoboActionNode>& children,
                           std::vector<RoboActionTrace>& trace, std::string& diagnostic, int depth) {
    for (const RoboActionNode& child : children) {
        if (!RoboActionRun(document, child, trace, diagnostic, depth)) {
            return false;
        }
    }
    return true;
}

bool RoboActionRequireTarget(RoboActionDocument& document, const std::string& id, RoboActionTarget*& target,
                             std::string& diagnostic) {
    target = RoboActionTargetById(document, id);
    if (target == nullptr) {
        diagnostic = RoboActionFail("R8402", "the id is missing");
        return false;
    }
    return true;
}

bool RoboActionValidate(RoboActionTarget& target) {
    target.valid = !target.required || !target.value.empty();
    return target.valid;
}

bool RoboActionRun(RoboActionDocument& document, const RoboActionNode& action, std::vector<RoboActionTrace>& trace,
                   std::string& diagnostic, int depth) {
    if (depth > 32) {
        diagnostic = RoboActionFail("R8402", "an action called itself too many times");
        return false;
    }
    if (action.kind == "SetState") {
        if (!RoboActionPut(document, action.name, action.value)) {
            diagnostic = RoboActionFail("R8402", "unknown state " + action.name);
            return false;
        }
        RoboActionRemember(trace, action, action.value);
        return true;
    }
    if (action.kind == "Conditional") {
        std::string current;
        bool taken = false;
        if (!action.name.empty()) {
            if (!RoboActionGet(document, action.name, current)) {
                diagnostic = RoboActionFail("R8402", "unknown state " + action.name);
                return false;
            }
            taken = current == action.equals;
        } else {
            if (!RoboActionGet(document, action.value, current)) {
                diagnostic = RoboActionFail("R8402", "unknown state " + action.value);
                return false;
            }
            taken = RoboActionTruthy(current);
        }
        RoboActionRemember(trace, action, taken ? "true" : "false");
        for (const RoboActionNode& child : action.children) {
            if (taken && child.kind != "Else" && !RoboActionRun(document, child, trace, diagnostic, depth + 1)) {
                return false;
            }
            if (!taken && child.kind == "Else" &&
                !RoboActionRunChildren(document, child.children, trace, diagnostic, depth + 1)) {
                return false;
            }
        }
        return true;
    }
    if (action.kind == "Show" || action.kind == "Hide" || action.kind == "Enable" || action.kind == "Disable") {
        RoboActionTarget* target = nullptr;
        if (!RoboActionRequireTarget(document, action.id, target, diagnostic)) {
            return false;
        }
        if (action.kind == "Show") {
            target->visible = true;
        } else if (action.kind == "Hide") {
            target->visible = false;
        } else if (action.kind == "Enable") {
            target->enabled = true;
        } else {
            target->enabled = false;
        }
        RoboActionSync(document, *target);
        RoboActionRemember(trace, action, action.kind == "Show" || action.kind == "Enable" ? "true" : "false");
        return true;
    }
    if (action.kind == "Toggle") {
        RoboActionTarget* target = nullptr;
        if (!RoboActionRequireTarget(document, action.id, target, diagnostic)) {
            return false;
        }
        if (action.name == "enabled") {
            target->enabled = !target->enabled;
            RoboActionRemember(trace, action, target->enabled ? "true" : "false");
        } else {
            target->visible = !target->visible;
            RoboActionRemember(trace, action, target->visible ? "true" : "false");
        }
        RoboActionSync(document, *target);
        return true;
    }
    if (action.kind == "Navigate") {
        bool found = false;
        for (const RoboActionTarget& target : document.targets) {
            if (target.tag == "Screen" && target.path == action.value) {
                found = true;
                break;
            }
        }
        if (!found) {
            diagnostic = RoboActionFail("R8402", "unknown path " + action.value);
            return false;
        }
        for (RoboActionTarget& target : document.targets) {
            if (target.tag != "Screen" || target.path.empty()) {
                continue;
            }
            target.visible = target.path == action.value;
            RoboActionSync(document, target);
        }
        document.path = action.value;
        RoboActionRemember(trace, action, action.value);
        return true;
    }
    if (action.kind == "Focus") {
        RoboActionTarget* target = nullptr;
        if (!RoboActionRequireTarget(document, action.id, target, diagnostic)) {
            return false;
        }
        document.events.focusId = action.id;
        RoboActionRemember(trace, action, action.id);
        return true;
    }
    if (action.kind == "Blur") {
        if (!action.id.empty() && document.events.focusId != action.id) {
            RoboActionRemember(trace, action, document.events.focusId);
            return true;
        }
        const std::string previous = document.events.focusId;
        document.events.focusId.clear();
        RoboActionNode record = action;
        record.id = previous;
        RoboActionRemember(trace, record, previous);
        return true;
    }
    if (action.kind == "ScrollTo") {
        RoboActionTarget* target = nullptr;
        if (!RoboActionRequireTarget(document, action.id, target, diagnostic)) {
            return false;
        }
        target->scroll = action.delay;
        RoboActionRemember(trace, action, action.value);
        return true;
    }
    if (action.kind == "Validate") {
        RoboActionTarget* target = nullptr;
        if (!RoboActionRequireTarget(document, action.id, target, diagnostic)) {
            return false;
        }
        const bool valid = RoboActionValidate(*target);
        RoboActionRemember(trace, action, valid ? "true" : "false");
        return true;
    }
    if (action.kind == "Submit") {
        RoboActionTarget* target = nullptr;
        if (!RoboActionRequireTarget(document, action.id, target, diagnostic)) {
            return false;
        }
        if (!RoboActionValidate(*target)) {
            RoboActionRemember(trace, action, "false");
            return true;
        }
        target->submitted = true;
        RoboActionRemember(trace, action, target->value);
        return true;
    }
    if (action.kind == "Reset") {
        if (action.id.empty() && action.name.empty()) {
            document.state = document.initialState;
            for (RoboActionTarget& target : document.targets) {
                target.visible = target.initialVisible;
                target.enabled = target.initialEnabled;
                target.scroll = target.initialScroll;
                target.value = target.initialValue;
                target.valid = true;
                target.submitted = false;
                RoboActionSync(document, target);
            }
            document.events.focusId.clear();
            RoboActionRemember(trace, action, "");
            return true;
        }
        if (!action.name.empty() && !RoboActionPut(document, action.name, "")) {
            diagnostic = RoboActionFail("R8402", "unknown state " + action.name);
            return false;
        }
        if (!action.name.empty()) {
            for (const auto& entry : document.initialState) {
                if (entry.first == action.name) {
                    RoboActionPut(document, action.name, entry.second);
                    break;
                }
            }
        }
        if (!action.id.empty()) {
            RoboActionTarget* target = nullptr;
            if (!RoboActionRequireTarget(document, action.id, target, diagnostic)) {
                return false;
            }
            target->visible = target->initialVisible;
            target->enabled = target->initialEnabled;
            target->scroll = target->initialScroll;
            target->value = target->initialValue;
            target->valid = true;
            target->submitted = false;
            RoboActionSync(document, *target);
        }
        RoboActionRemember(trace, action, action.name.empty() ? action.id : action.name);
        return true;
    }
    if (action.kind == "Execute") {
        const RoboActionNode* body = nullptr;
        for (const auto& entry : document.actions) {
            if (entry.first == action.name) {
                body = &entry.second;
                break;
            }
        }
        if (body == nullptr) {
            diagnostic = RoboActionFail("R8402", "unknown action " + action.name);
            return false;
        }
        RoboActionRemember(trace, action, action.name);
        return RoboActionRunChildren(document, body->children, trace, diagnostic, depth + 1);
    }
    if (action.kind == "Delay") {
        document.clock += action.delay;
        RoboActionRemember(trace, action, std::to_string(action.delay));
        return true;
    }
    if (action.kind == "Sequence" || action.kind == "Parallel") {
        RoboActionRemember(trace, action, action.kind == "Sequence" ? "order" : "together");
        return RoboActionRunChildren(document, action.children, trace, diagnostic, depth + 1);
    }
    diagnostic = RoboActionFail("R8402", "an unknown action is rejected");
    return false;
}

bool RoboActionParseEvent(const RoboXMLNode& step, RoboEventStep& eventStep, std::string& diagnostic) {
    RoboXMLNode script;
    script.name = "Script";
    script.children.push_back(step);
    RoboXMLNode root;
    root.name = "Events";
    root.children.push_back(std::move(script));
    RoboEventDocument temp;
    if (!RoboEventRead(root, temp, diagnostic)) {
        return false;
    }
    if (temp.steps.size() != 1) {
        diagnostic = RoboActionFail("R8402", "a script step was not read");
        return false;
    }
    eventStep = temp.steps[0];
    return true;
}

}  // namespace

bool RoboActionRead(const RoboXMLNode& node, RoboActionDocument& document, std::string& diagnostic) {
    document = RoboActionDocument();
    if (RoboActionLower(node.name) != "behavior") {
        diagnostic = RoboActionFail("R8404", "the wrong tag is rejected");
        return false;
    }
    if (!node.attrs.empty()) {
        diagnostic = RoboActionFail("R8401", "unknown attribute " + node.attrs.begin()->first);
        return false;
    }
    std::vector<RoboXMLNode> targets;
    int scripts = 0;
    for (const RoboXMLNode& child : node.children) {
        const std::string kind = RoboActionLower(child.name);
        if (kind == "state") {
            static const char* names[] = {"name", "value"};
            bool ok = true;
            RoboActionKeepAttr(child, names, 2, diagnostic, ok);
            if (!ok) {
                return false;
            }
            if (!child.children.empty()) {
                diagnostic = RoboActionFail("R8404", "State has no children");
                return false;
            }
            const std::string name = RoboActionAttr(child, "name");
            if (name.empty()) {
                diagnostic = RoboActionFail("R8402", "State needs a name");
                return false;
            }
            for (const auto& entry : document.state) {
                if (entry.first == name) {
                    diagnostic = RoboActionFail("R8402", "a repeated state is rejected");
                    return false;
                }
            }
            document.state.emplace_back(name, RoboActionAttr(child, "value"));
            continue;
        }
        if (kind == "action") {
            static const char* names[] = {"name"};
            bool ok = true;
            RoboActionKeepAttr(child, names, 1, diagnostic, ok);
            if (!ok) {
                return false;
            }
            const std::string name = RoboActionAttr(child, "name");
            if (name.empty()) {
                diagnostic = RoboActionFail("R8402", "Action needs a name");
                return false;
            }
            for (const auto& entry : document.actions) {
                if (entry.first == name) {
                    diagnostic = RoboActionFail("R8402", "a repeated action is rejected");
                    return false;
                }
            }
            RoboActionNode body;
            body.kind = "Sequence";
            for (const RoboXMLNode& inner : child.children) {
                RoboActionNode next;
                if (!RoboActionReadNode(inner, next, diagnostic)) {
                    return false;
                }
                body.children.push_back(std::move(next));
            }
            document.actions.emplace_back(name, std::move(body));
            continue;
        }
        if (kind == "script") {
            ++scripts;
            if (scripts > 1) {
                diagnostic = RoboActionFail("R8402", "the script cannot be set twice");
                return false;
            }
            if (!child.attrs.empty()) {
                diagnostic = RoboActionFail("R8401", "unknown attribute " + child.attrs.begin()->first);
                return false;
            }
            for (const RoboXMLNode& step : child.children) {
                RoboActionStep item;
                if (RoboActionListed(ROBO_ACTION_EVENT_STEPS,
                                     static_cast<int>(sizeof(ROBO_ACTION_EVENT_STEPS) / sizeof(ROBO_ACTION_EVENT_STEPS[0])),
                                     RoboActionLower(step.name))) {
                    item.event = true;
                    if (!RoboActionParseEvent(step, item.eventStep, diagnostic)) {
                        return false;
                    }
                } else if (!RoboActionReadNode(step, item.action, diagnostic)) {
                    return false;
                }
                document.steps.push_back(std::move(item));
            }
            continue;
        }
        if (RoboActionKind(child.name) != nullptr || kind == "else") {
            diagnostic = RoboActionFail("R8404", "an action is not a target");
            return false;
        }
        targets.push_back(child);
    }
    document.initialState = document.state;
    RoboXMLNode host;
    host.name = "Events";
    for (const RoboXMLNode& target : targets) {
        host.children.push_back(RoboActionStrip(target));
    }
    if (!host.children.empty() && !RoboEventRead(host, document.events, diagnostic)) {
        return false;
    }
    if (document.events.roots.size() != targets.size()) {
        diagnostic = RoboActionFail("R8404", "the target tree does not match");
        return false;
    }
    for (std::size_t index = 0; index < targets.size(); ++index) {
        if (!RoboActionZip(document.events.roots[index], targets[index], document, diagnostic)) {
            return false;
        }
    }
    for (const RoboEventNode& root : document.events.roots) {
        if (!RoboActionHandlers(root, document, diagnostic)) {
            return false;
        }
    }
    return true;
}

bool RoboActionPlay(RoboActionDocument& document, std::vector<RoboActionTrace>& trace, std::string& diagnostic) {
    trace.clear();
    for (const RoboActionStep& step : document.steps) {
        if (step.event) {
            document.events.steps = {step.eventStep};
            std::vector<RoboEventDelivery> deliveries;
            if (!RoboEventPlay(document.events, deliveries, diagnostic)) {
                return false;
            }
            for (const RoboEventDelivery& delivery : deliveries) {
                if (delivery.handler.empty()) {
                    continue;
                }
                RoboActionNode call;
                call.kind = "Execute";
                call.name = delivery.handler;
                if (!RoboActionRun(document, call, trace, diagnostic, 0)) {
                    return false;
                }
            }
            continue;
        }
        if (!RoboActionRun(document, step.action, trace, diagnostic, 0)) {
            return false;
        }
    }
    return true;
}

bool RoboActionGet(const RoboActionDocument& document, const std::string& name, std::string& value) {
    for (const auto& entry : document.state) {
        if (entry.first == name) {
            value = entry.second;
            return true;
        }
    }
    return false;
}

const RoboActionTarget* RoboActionFind(const RoboActionDocument& document, const std::string& id) {
    for (const RoboActionTarget& target : document.targets) {
        if (target.id == id) {
            return &target;
        }
    }
    return nullptr;
}
