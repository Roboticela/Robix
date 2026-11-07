#include "RoboCompilerWebShared.hpp"

#include <cctype>
#include <cstdio>
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
