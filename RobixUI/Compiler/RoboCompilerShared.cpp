#include "RoboCompilerShared.hpp"

#include "Components/Screen/Screen.hpp"

#include <cstdio>
#include <sstream>

std::string RoboCompilerLower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

std::string RoboCompilerCppEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() * 2);
    for (unsigned char c : value) {
        if (c == '\\' || c == '"') {
            out.push_back('\\');
            out.push_back(static_cast<char>(c));
        } else if (c < 0x20 || c >= 0x7F) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\x%02X\"\"", c);
            out += buf;
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

std::string RoboCompilerEmitChildCpp(const RoboXMLNode& node, const std::string& parentExpr) {
    (void)parentExpr;
    if (RoboCompilerLower(node.name) == "screen" &&
        (!RoboXMLAttr(node, "path").empty() || !RoboXMLAttr(node, "route").empty() || !RoboXMLAttr(node, "id").empty())) {
        return RoboCompilerEmitScreenCpp(node);
    }
    return {};
}
