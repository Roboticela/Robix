#pragma once

#include "../../RoboXML.hpp"

#include <string>
#include <vector>

/* Stage C14. One event record reaches the topmost primitive under the point, then its parents. */

inline constexpr const char* ROBO_EVENT_NAMES[] = {
    "OnClick",       "OnDoubleClick", "OnHover",        "OnMouseDown",     "OnMouseUp",         "OnContextMenu",
    "OnTap",         "OnLongPress",   "OnSwipe",        "OnDrag",          "OnDrop",            "OnPinch",
    "OnRotate",      "OnKeyDown",     "OnKeyUp",        "OnFocus",         "OnBlur",            "OnSubmit",
    "OnChange",      "OnInput",       "OnValueChange",  "OnSelectionChange","OnMount",          "OnUnmount",
    "OnUpdate",      "OnResize",      "OnLayout",       "OnVisibilityChange","OnScroll",        "OnScrollStart",
    "OnScrollEnd",   "OnLoad",        "OnError",        "OnPlay",          "OnPause",           "OnEnd",
    "OnTimeUpdate",
};

constexpr int ROBO_EVENT_NAME_COUNT = static_cast<int>(sizeof(ROBO_EVENT_NAMES) / sizeof(ROBO_EVENT_NAMES[0]));
constexpr int ROBO_EVENT_DOUBLE_MS = 300;

struct RoboEventNode {
    std::string id;
    std::string tag;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool visible = true;
    std::vector<std::pair<std::string, std::string>> handlers;
    std::vector<RoboEventNode> children;
};

struct RoboEventStep {
    std::string kind;
    int x = 0;
    int y = 0;
    bool hasPoint = false;
    std::string id;
    std::string key;
    std::string value;
    int time = -1;
    int width = 0;
    int height = 0;
    bool hasSize = false;
    int visible = -1;
};

struct RoboEventDocument {
    std::vector<RoboEventNode> roots;
    std::vector<RoboEventStep> steps;
    std::string focusId;
    std::string dragId;
    std::string pointerId;
    bool pointerDown = false;
    int lastClickTime = -1000;
    std::string lastClickId;
    int clock = 0;
};

struct RoboEventDelivery {
    std::string name;
    std::string hitId;
    std::string targetId;
    std::string handler;
    std::string key;
    std::string value;
    int x = 0;
    int y = 0;
};

/* Reads an Events document: primitive boxes plus one Script. */
bool RoboEventRead(const RoboXMLNode& node, RoboEventDocument& document, std::string& diagnostic);

/* Plays the script. Each record names the hit, then each parent that receives it. */
bool RoboEventPlay(RoboEventDocument& document, std::vector<RoboEventDelivery>& deliveries, std::string& diagnostic);

/* The topmost visible primitive whose box contains the point. */
bool RoboEventHit(const RoboEventDocument& document, int x, int y, std::string& id);

/* The first delivery of that event, or nullptr. */
const RoboEventDelivery* RoboEventFind(const std::vector<RoboEventDelivery>& deliveries, const char* name);
