#include "ScreenSize.hpp"

#include "Components/Container/ContainerWeb.hpp"
#include "Components/Image/ImageWeb.hpp"
#include "Components/Text/TextWeb.hpp"
#include "RoboCompilerWebShared.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

namespace {

struct Scale {
    int sm = 640;
    int mx = 960;
    int xl = 1280;
    int x2 = 1536;
};

struct Query {
    int min = -1;
    int max = -1;
};

Scale g_scale;
int g_next = 0;
std::ostringstream g_css;

bool Digits(const std::string& text) {
    if (text.empty()) return false;
    for (char ch : text) {
        if (ch < '0' || ch > '9') return false;
    }
    return true;
}

int Number(const std::string& text) {
    int value = 0;
    for (char ch : text) value = value * 10 + (ch - '0');
    return value;
}

std::string Lower(std::string text) {
    for (char& ch : text) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return text;
}

bool NamedMin(const std::string& token, int& min) {
    if (token == "sm") { min = g_scale.sm; return true; }
    if (token == "mx") { min = g_scale.mx; return true; }
    if (token == "xl") { min = g_scale.xl; return true; }
    if (token == "2x" || token == "2xl") { min = g_scale.x2; return true; }
    return false;
}

bool ParseQuery(std::string token, Query& query) {
    if (token.rfind("at-", 0) == 0) token = token.substr(3);
    if (token.rfind("only-", 0) == 0) {
        const std::string inner = token.substr(5);
        if (inner == "sm") { query.min = g_scale.sm; query.max = g_scale.mx - 1; return true; }
        if (inner == "mx") { query.min = g_scale.mx; query.max = g_scale.xl - 1; return true; }
        if (inner == "xl") { query.min = g_scale.xl; query.max = g_scale.x2 - 1; return true; }
        if (inner == "2x" || inner == "2xl") { query.min = g_scale.x2; return true; }
        token = inner;
    }
    if (token == "sm-mx") { query.min = g_scale.sm; query.max = g_scale.mx - 1; return true; }
    if (token == "mx-xl") { query.min = g_scale.mx; query.max = g_scale.xl - 1; return true; }
    if (token == "xl-2x" || token == "xl-2xl") { query.min = g_scale.xl; query.max = g_scale.x2 - 1; return true; }
    int named = 0;
    if (NamedMin(token, named)) { query.min = named; return true; }
    const auto dash = token.find('-');
    if (dash != std::string::npos && dash > 0 && dash + 1 < token.size()) {
        const std::string left = token.substr(0, dash);
        const std::string right = token.substr(dash + 1);
        if (Digits(left) && Digits(right)) {
            query.min = Number(left);
            query.max = Number(right) - 1;
            if (query.max < query.min) query.max = Number(right);
            return true;
        }
    }
    if (Digits(token)) { query.min = Number(token); return true; }
    std::cerr << "warning: screen size '" << token << "' was not recognized and was skipped.\n";
    return false;
}

std::string Media(const Query& query) {
    std::string text = "@media (";
    if (query.min >= 0) text += "min-width:" + std::to_string(query.min) + "px";
    if (query.max >= 0) {
        if (query.min >= 0) text += ") and (";
        text += "max-width:" + std::to_string(query.max) + "px";
    }
    text += ")";
    return text;
}

std::vector<std::string> Tokens(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : text) {
        if (ch == '.' || ch == ',' || ch == ' ' || ch == '\t') {
            if (!current.empty()) tokens.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

bool SplitSized(const std::string& key, std::string& property, std::string& sizes) {
    const auto dot = key.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= key.size()) return false;
    property = key.substr(0, dot);
    sizes = key.substr(dot + 1);
    if (property == "bind" || property == "bindtwoway") return false;
    return !sizes.empty();
}

std::string Declarations(const RoboXMLNode& node, const RoboFoundationLibrary& library) {
    std::string style = RoboWebStyle(node, library);
    RoboWebAppendImageStyle(style, node, library);
    const std::string prefix = "box-sizing:border-box;";
    if (style.rfind(prefix, 0) == 0) style = style.substr(prefix.size());
    std::string out;
    std::string piece;
    for (std::size_t i = 0; i <= style.size(); ++i) {
        if (i == style.size() || style[i] == ';') {
            if (!piece.empty() && piece.find("box-sizing") == std::string::npos) {
                out += piece;
                out += " !important;";
            }
            piece.clear();
        } else {
            piece.push_back(style[i]);
        }
    }
    return out;
}

std::string DisplayOf(const RoboXMLNode& node, const RoboFoundationLibrary& library, const char* fallback) {
    const std::string style = RoboWebStyle(node, library);
    const auto at = style.find("display:");
    if (at == std::string::npos) return fallback != nullptr ? fallback : "block";
    const auto end = style.find(';', at);
    return style.substr(at + 8, (end == std::string::npos ? style.size() : end) - (at + 8));
}

}  // namespace

void RoboWebScreenReset(const std::string& sizes) {
    g_scale = Scale{};
    g_next = 0;
    g_css.str("");
    g_css.clear();
    std::string token;
    std::string name;
    for (std::size_t i = 0; i <= sizes.size(); ++i) {
        const char ch = i == sizes.size() ? ' ' : sizes[i];
        if (ch == ' ' || ch == '\t' || ch == ',' || ch == ';') {
            if (!token.empty()) {
                const auto mark = token.find(':');
                const auto eq = token.find('=');
                const auto cut = mark != std::string::npos ? mark : eq;
                if (cut != std::string::npos && cut + 1 < token.size() && Digits(token.substr(cut + 1))) {
                    const std::string key = Lower(token.substr(0, cut));
                    const int value = Number(token.substr(cut + 1));
                    if (key == "sm") g_scale.sm = value;
                    else if (key == "mx") g_scale.mx = value;
                    else if (key == "xl") g_scale.xl = value;
                    else if (key == "2x" || key == "2xl") g_scale.x2 = value;
                }
            }
            token.clear();
        } else {
            token.push_back(ch);
        }
    }
    (void)name;
}

std::string RoboWebScreenAttrs(const RoboXMLNode& node, const RoboFoundationLibrary& library, const char* display) {
    std::map<std::string, RoboXMLNode> slices;
    const auto add = [&](const std::string& sizes, const std::string& property, const std::string& value) {
        for (const std::string& token : Tokens(Lower(sizes))) {
            slices[token].attrs[property] = value;
        }
    };
    const auto cased = [](const RoboXMLNode& owner, const std::string& key) {
        const auto dot = key.find('.');
        for (const auto& other : owner.attrs) {
            if (other.first != key && Lower(other.first) == key) return other.first.substr(0, other.first.find('.'));
        }
        return key.substr(0, dot);
    };
    for (const auto& entry : node.attrs) {
        const std::string key = Lower(entry.first);
        if (key != entry.first) continue;
        std::string property;
        std::string sizes;
        if (!SplitSized(key, property, sizes)) continue;
        std::string styled = cased(node, key);
        if (!styled.empty() && styled[0] >= 'A' && styled[0] <= 'Z') styled[0] = static_cast<char>(styled[0] - 'A' + 'a');
        add(sizes, styled, entry.second);
    }
    for (const RoboXMLNode& child : node.children) {
        if (Lower(child.name) != "at") continue;
        const std::string sizes = RoboXMLAttr(child, "size", RoboXMLAttr(child, "sizes").c_str());
        if (sizes.empty()) continue;
        for (const auto& entry : child.attrs) {
            const std::string key = Lower(entry.first);
            if (key != entry.first || key == "size" || key == "sizes") continue;
            std::string styled = cased(child, key);
            if (!styled.empty() && styled[0] >= 'A' && styled[0] <= 'Z') styled[0] = static_cast<char>(styled[0] - 'A' + 'a');
            add(sizes, styled, entry.second);
        }
    }
    const std::string only = Lower(RoboXMLAttr(node, "only"));
    if (slices.empty() && only.empty()) return {};

    const int id = ++g_next;
    const std::string selector = "[data-screen=\"" + std::to_string(id) + "\"]";
    struct Rule {
        int min = -1;
        int max = -1;
        std::string media;
        std::string body;
    };
    std::vector<Rule> rules;
    const auto append = [&](const Query& query, const std::string& body) {
        const std::string media = Media(query);
        for (Rule& rule : rules) {
            if (rule.media == media) {
                rule.body += body;
                return;
            }
        }
        rules.push_back(Rule{query.min, query.max, media, body});
    };
    const auto flag = [](const RoboXMLNode& owner) {
        std::string value = Lower(RoboXMLAttr(owner, "visibility"));
        if (value.empty()) value = Lower(RoboXMLAttr(owner, "visible"));
        return value;
    };
    const auto shows = [](const std::string& value) {
        return value == "true" || value == "1" || value == "yes" || value == "on" || value == "visible";
    };
    const std::string shown = DisplayOf(node, library, display);
    for (const auto& slice : slices) {
        Query query;
        if (!ParseQuery(slice.first, query)) continue;
        std::string body = Declarations(slice.second, library);
        if (shows(flag(slice.second))) body += "display:" + shown + " !important;";
        append(query, body);
    }
    if (!only.empty()) {
        g_css << selector << "[data-only]{display:none !important}\n";
        const std::string shown = DisplayOf(node, library, display);
        for (const std::string& token : Tokens(only)) {
            Query query;
            if (!ParseQuery(token, query)) continue;
            append(query, "display:" + shown + " !important;");
        }
    }
    std::sort(rules.begin(), rules.end(), [](const Rule& left, const Rule& right) {
        if (left.min != right.min) return left.min < right.min;
        const bool leftBand = left.max >= 0;
        const bool rightBand = right.max >= 0;
        if (leftBand != rightBand) return !leftBand;
        return left.max < right.max;
    });
    for (const Rule& rule : rules) {
        if (rule.body.empty()) continue;
        g_css << rule.media << "{" << selector << "{" << rule.body << "}}\n";
    }
    std::string attrs = " data-screen=\"" + std::to_string(id) + "\"";
    if (!only.empty()) attrs += " data-only=\"1\"";
    return attrs;
}

std::string RoboWebScreenCss() {
    if (g_css.tellp() == 0) return {};
    std::ostringstream out;
    out << "/* screen sm:" << g_scale.sm << " mx:" << g_scale.mx << " xl:" << g_scale.xl << " 2x:" << g_scale.x2 << " */\n";
    out << g_css.str();
    return out.str();
}
