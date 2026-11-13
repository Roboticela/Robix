#pragma once

#include "../Event/RoboEvent.hpp"

#include <string>
#include <utility>
#include <vector>

/* Stage C15. State lives on the document. Actions run from a script or from a click handler. */

struct RoboActionNode {
    std::string kind;
    std::string name;
    std::string id;
    std::string value;
    std::string equals;
    int delay = 0;
    std::vector<RoboActionNode> children;
};

struct RoboActionTarget {
    std::string id;
    std::string tag;
    bool visible = true;
    bool enabled = true;
    bool initialVisible = true;
    bool initialEnabled = true;
    bool required = false;
    bool valid = true;
    bool submitted = false;
    int scroll = 0;
    int initialScroll = 0;
    std::string value;
    std::string initialValue;
    std::string path;
};

struct RoboActionStep {
    bool event = false;
    RoboEventStep eventStep;
    RoboActionNode action;
};

struct RoboActionTrace {
    std::string kind;
    std::string name;
    std::string id;
    std::string value;
};

struct RoboActionDocument {
    RoboEventDocument events;
    std::vector<RoboActionTarget> targets;
    std::vector<std::pair<std::string, std::string>> state;
    std::vector<std::pair<std::string, std::string>> initialState;
    std::vector<std::pair<std::string, RoboActionNode>> actions;
    std::vector<RoboActionStep> steps;
    std::string path;
    int clock = 0;
};

/* Reads State, named actions, the primitive tree, and one Script. */
bool RoboActionRead(const RoboXMLNode& node, RoboActionDocument& document, std::string& diagnostic);

/* Plays the script. A click runs the action named by its handler. */
bool RoboActionPlay(RoboActionDocument& document, std::vector<RoboActionTrace>& trace, std::string& diagnostic);

/* The current value of a named state. */
bool RoboActionGet(const RoboActionDocument& document, const std::string& name, std::string& value);

/* The target with this id, or nullptr. */
const RoboActionTarget* RoboActionFind(const RoboActionDocument& document, const std::string& id);
