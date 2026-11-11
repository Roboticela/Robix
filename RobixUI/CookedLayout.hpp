#pragma once

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace Robix::UI {

struct RoboCookedBox {
    int role = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    std::string text;
};

inline int RoboCookedRole(const std::string& kind) {
    std::string lower;
    lower.reserve(kind.size());
    for (unsigned char ch : kind) {
        lower.push_back(static_cast<char>(std::tolower(ch)));
    }
    if (lower == "bar") return 1;
    if (lower == "icon") return 2;
    if (lower == "window") return 3;
    if (lower == "button") return 4;
    if (lower == "text") return 5;
    if (lower == "screen") return 6;
    return 0;
}

inline bool RoboCookedContains(const RoboCookedBox& box, int x, int y) {
    return x >= box.x && y >= box.y && x < box.x + box.width && y < box.y + box.height;
}

inline int RoboCookedCursor(const RoboCookedBox& box, int x, int y) {
    if (box.role == 5) {
        return 4;
    }
    if (box.role == 2 || box.role == 4) {
        return 1;
    }
    if (box.role != 3) {
        return 0;
    }
    const bool left = x < box.x + 4;
    const bool right = x >= box.x + box.width - 4;
    const bool top = y < box.y + 4;
    const bool bottom = y >= box.y + box.height - 4;
    if ((left || right) && (top || bottom)) {
        if ((left && top) || (right && bottom)) {
            return 9;
        }
        return 10;
    }
    if (left || right) {
        return 6;
    }
    if (top || bottom) {
        return 7;
    }
    if (y < box.y + 16) {
        return 2;
    }
    return 0;
}

inline int RoboCookedHit(const std::vector<RoboCookedBox>& boxes, int x, int y) {
    for (std::size_t step = boxes.size(); step > 0; --step) {
        const RoboCookedBox& box = boxes[step - 1];
        if (box.role == 6 || !RoboCookedContains(box, x, y)) {
            continue;
        }
        return RoboCookedCursor(box, x, y);
    }
    return 0;
}

inline int RoboCookedPlace(std::vector<RoboCookedBox>& boxes, int index, int x, int y, int width, int height) {
    if (index < 0 || static_cast<std::size_t>(index) >= boxes.size()) {
        return 0;
    }
    RoboCookedBox& box = boxes[static_cast<std::size_t>(index)];
    box.x = x;
    box.y = y;
    box.width = width;
    box.height = height;
    if (box.role == 5) {
        return 4;
    }
    if (box.role == 2 || box.role == 4) {
        return 1;
    }
    return 0;
}

inline int RoboCookedDrag(std::vector<RoboCookedBox>& boxes, int index, int dx, int dy) {
    if (index < 0 || static_cast<std::size_t>(index) >= boxes.size()) {
        return 0;
    }
    RoboCookedBox& box = boxes[static_cast<std::size_t>(index)];
    box.x += dx;
    box.y += dy;
    return 2;
}

inline int RoboCookedSnap(std::vector<RoboCookedBox>& boxes, int index, int side) {
    if (index < 0 || static_cast<std::size_t>(index) >= boxes.size()) {
        return 0;
    }
    int screenWidth = 320;
    int screenHeight = 200;
    for (const RoboCookedBox& box : boxes) {
        if (box.role == 6) {
            screenWidth = box.width;
            screenHeight = box.height;
        }
    }
    RoboCookedBox& box = boxes[static_cast<std::size_t>(index)];
    const int half = screenWidth / 2;
    box.y = 0;
    box.height = screenHeight;
    box.width = half;
    box.x = side == 1 ? half : 0;
    return 6;
}

inline int RoboCookedField(const RoboCookedBox& box, int field) {
    if (field == 0) return box.role;
    if (field == 1) return box.x;
    if (field == 2) return box.y;
    if (field == 3) return box.width;
    if (field == 4) return box.height;
    return 0;
}

inline bool RoboCookedParse(const std::string& text, std::vector<RoboCookedBox>& boxes) {
    boxes.clear();
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream row(line);
        std::string kind;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        if (!(row >> kind >> x >> y >> width >> height)) {
            continue;
        }
        if (kind == "RXUL") {
            continue;
        }
        std::string rest;
        std::getline(row, rest);
        std::size_t begin = 0;
        while (begin < rest.size() && rest[begin] == ' ') {
            ++begin;
        }
        RoboCookedBox box;
        box.role = RoboCookedRole(kind);
        box.x = x;
        box.y = y;
        box.width = width;
        box.height = height;
        box.text = rest.substr(begin);
        boxes.push_back(std::move(box));
    }
    return !boxes.empty();
}

inline bool RoboCookedLoad(const std::string& path, std::vector<RoboCookedBox>& boxes) {
    boxes.clear();
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    std::error_code error;
    fs::path cursor = fs::current_path(error);
    for (int step = 0; step < 8 && !cursor.empty(); ++step) {
        roots.push_back(cursor);
        fs::path parent = cursor.parent_path();
        if (parent == cursor) {
            break;
        }
        cursor = parent;
    }
    std::vector<fs::path> candidates;
    candidates.emplace_back(path);
    for (const fs::path& root : roots) {
        candidates.push_back(root / path);
        candidates.push_back(root / "RoboticelaOS" / path);
    }
    for (const fs::path& candidate : candidates) {
        std::ifstream input(candidate, std::ios::binary);
        if (!input) {
            continue;
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();
        if (RoboCookedParse(buffer.str(), boxes)) {
            return true;
        }
    }
    return false;
}

}  // namespace Robix::UI
