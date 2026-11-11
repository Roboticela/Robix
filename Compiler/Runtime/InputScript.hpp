#pragma once

#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace Robix {

struct RoboPlayedEvent {
    int type = 0;
    int code = 0;
    int value = 0;
    int x = 0;
    int y = 0;
};

inline void RoboPlayedMouse(const std::vector<RoboPlayedEvent>& events, int& x, int& y) {
    x = 0;
    y = 0;
    for (const RoboPlayedEvent& event : events) {
        if (event.type == 2) {
            x = event.x;
            y = event.y;
        }
    }
}

inline bool RoboParseInputScript(const std::string& text, std::vector<RoboPlayedEvent>& events) {
    events.clear();
    std::string normalized = text;
    for (char& ch : normalized) {
        if (ch == ';') {
            ch = '\n';
        }
    }
    std::istringstream input(normalized);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::size_t begin = 0;
        while (begin < line.size() && std::isspace(static_cast<unsigned char>(line[begin]))) {
            ++begin;
        }
        if (begin >= line.size() || line[begin] == '#') {
            continue;
        }
        std::istringstream row(line.substr(begin));
        std::string kind;
        if (!(row >> kind)) {
            continue;
        }
        RoboPlayedEvent event;
        if (kind == "move") {
            if (!(row >> event.x >> event.y)) {
                return false;
            }
            event.type = 2;
        } else if (kind == "click") {
            if (!(row >> event.x >> event.y >> event.value)) {
                return false;
            }
            event.type = 2;
        } else if (kind == "key") {
            if (!(row >> event.code >> event.value)) {
                return false;
            }
            event.type = 1;
        } else {
            return false;
        }
        if (events.size() >= 128) {
            return false;
        }
        events.push_back(event);
    }
    return !events.empty();
}

}  // namespace Robix
