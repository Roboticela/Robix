#include "Foundation.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Robix::UI {
namespace {

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

std::filesystem::path FoundationRoot() {
    namespace fs = std::filesystem;
    fs::path cursor = fs::current_path();
    for (int depth = 0; depth < 8 && !cursor.empty(); ++depth) {
        if (fs::is_regular_file(cursor / "RobixUI" / "Components" / "Foundation.rbxu")) return cursor;
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor) break;
        cursor = cursor.parent_path();
    }
    return {};
}

std::string Attribute(const std::string& line, const std::string& name) {
    const std::string key = name + "=\"";
    const auto at = line.find(key);
    if (at == std::string::npos) return {};
    const auto start = at + key.size();
    const auto end = line.find('"', start);
    if (end == std::string::npos) return {};
    return line.substr(start, end - start);
}

std::uint32_t HexColor(const std::string& text, std::uint32_t fallback) {
    if (text.size() == 7 && text[0] == '#') {
        try {
            return static_cast<std::uint32_t>(std::stoul(text.substr(1), nullptr, 16));
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

int Number(const std::string& text, int fallback) {
    if (text.empty()) return fallback;
    try {
        return std::stoi(text);
    } catch (...) {
        return fallback;
    }
}

std::string HexText(std::uint32_t color) {
    std::ostringstream out;
    out << std::hex;
    out.width(6);
    out.fill('0');
    out << (color & 0xFFFFFFu);
    return out.str();
}

} // namespace

bool LoadFoundation(FoundationPaint& paint, bool dark, std::string& error) {
    const auto root = FoundationRoot();
    if (root.empty()) {
        error = "RobixUI: Foundation.rbxu was not found\n";
        return false;
    }
    const std::string text = ReadText(root / "RobixUI" / "Components" / "Foundation.rbxu");
    if (text.find("<Foundation") == std::string::npos) {
        error = "RobixUI: Foundation.rbxu has no Foundation element\n";
        return false;
    }
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        const std::string name = Attribute(line, "Name");
        if (line.find("<Color ") != std::string::npos) {
            const std::uint32_t value = HexColor(dark ? Attribute(line, "Dark") : Attribute(line, "Light"), 0);
            if (name == "bg-primary") paint.primary = value;
            else if (name == "bg-secondary") paint.window = value;
            else if (name == "text-primary") paint.text = value;
            else if (name == "brand-primary") paint.button = value;
            else if (name == "text-on-brand") paint.buttonText = value;
        } else if (line.find("<Space ") != std::string::npos && name == "space-4") {
            paint.space = Number(Attribute(line, "Value"), paint.space);
        } else if (line.find("<Type ") != std::string::npos && name == "body") {
            paint.bodySize = Number(Attribute(line, "Size"), paint.bodySize);
        } else if (line.find("<Type ") != std::string::npos && name == "window-title") {
            paint.titleSize = Number(Attribute(line, "Size"), paint.titleSize);
        } else if (line.find("<Radius ") != std::string::npos && name == "control") {
            paint.radius = Number(Attribute(line, "Value"), paint.radius);
        } else if (line.find("<Radius ") != std::string::npos && name == "radius-lg") {
            paint.radiusLg = Number(Attribute(line, "Value"), paint.radiusLg);
        }
    }
    return true;
}

int FontWeight(const std::string& name) {
    const std::string key = [&] {
        std::string lower = name;
        for (char& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return lower;
    }();
    if (key.find("semibold") != std::string::npos) return 600;
    if (key.find("medium") != std::string::npos) return 500;
    if (key.find("bold") != std::string::npos) return 700;
    return 400;
}

bool FoundationFont(const std::string& name, std::string& src, std::string& token) {
    const auto root = FoundationRoot();
    if (root.empty() || name.empty()) return false;
    std::string key = name;
    for (char& ch : key) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (key == "inter" || key == "inter-regular") key = "sans";
    if (key == "inter-medium" || key == "medium") key = "sans-medium";
    if (key == "inter-semibold" || key == "semibold" || key == "semi-bold") key = "sans-semibold";
    if (key == "inter-bold" || key == "bold") key = "sans-bold";
    if (key == "inter-italic" || key == "italic") key = "sans-italic";
    std::istringstream lines(ReadText(root / "RobixUI" / "Components" / "Foundation.rbxu"));
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find("<Font ") == std::string::npos) continue;
        std::string found = Attribute(line, "Name");
        for (char& ch : found) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (found == key) {
            src = Attribute(line, "Src");
            token = found;
            return !src.empty();
        }
    }
    return false;
}

std::string FoundationCss(const std::string& fontPrefix) {
    FoundationPaint light;
    FoundationPaint dark;
    std::string error;
    if (!LoadFoundation(light, false, error) || !LoadFoundation(dark, true, error)) return {};
    std::ostringstream css;
    css << ":root{color-scheme:light dark;"
        << "--bg-secondary:#" << HexText(light.window) << ";"
        << "--text-primary:#" << HexText(light.text) << ";"
        << "--brand-primary:#" << HexText(light.button) << ";"
        << "--text-on-brand:#" << HexText(light.buttonText) << ";"
        << "--space-4:" << light.space << "px;"
        << "--body-size:" << light.bodySize << "px;"
        << "--title-size:" << light.titleSize << "px;"
        << "--control:" << light.radius << "px;}\n"
        << "@media (prefers-color-scheme: dark){:root{"
        << "--bg-secondary:#" << HexText(dark.window) << ";"
        << "--text-primary:#" << HexText(dark.text) << ";"
        << "--brand-primary:#" << HexText(dark.button) << ";"
        << "--text-on-brand:#" << HexText(dark.buttonText) << ";}}\n"
        << "html[data-theme=\"dark\"]{"
        << "--bg-secondary:#" << HexText(dark.window) << ";"
        << "--text-primary:#" << HexText(dark.text) << ";"
        << "--brand-primary:#" << HexText(dark.button) << ";"
        << "--text-on-brand:#" << HexText(dark.buttonText) << ";}\n"
        << "html[data-theme=\"light\"]{"
        << "--bg-secondary:#" << HexText(light.window) << ";"
        << "--text-primary:#" << HexText(light.text) << ";"
        << "--brand-primary:#" << HexText(light.button) << ";"
        << "--text-on-brand:#" << HexText(light.buttonText) << ";}\n";
    const auto root = FoundationRoot();
    if (!root.empty()) {
        std::istringstream lines(ReadText(root / "RobixUI" / "Components" / "Foundation.rbxu"));
        std::string line;
        while (std::getline(lines, line)) {
            if (line.find("<Font ") == std::string::npos) continue;
            const std::string name = Attribute(line, "Name");
            const std::string src = Attribute(line, "Src");
            if (name.empty() || src.empty()) continue;
            const bool italic = FontWeight(name) == 400 && name.find("italic") != std::string::npos;
            const std::string family = src.find("Inter/") != std::string::npos ? "Inter" : name;
            css << "@font-face{font-family:\"" << family << "\";src:url(\"" << fontPrefix << "/" << src
                << "\") format(\"truetype\");font-weight:" << FontWeight(name) << ";font-style:"
                << (italic || name.find("italic") != std::string::npos ? "italic" : "normal") << ";font-display:swap}\n";
            css << ":root{--font-" << name << ":\"" << family << "\",system-ui,sans-serif}\n";
        }
    }
    css << ":root{--font-sans:\"Inter\",system-ui,sans-serif}\n";
    return css.str();
}

std::string FoundationLine() {
    FoundationPaint light;
    FoundationPaint dark;
    std::string error;
    if (!LoadFoundation(light, false, error) || !LoadFoundation(dark, true, error)) return error;
    auto hex = [](std::uint32_t color) {
        std::ostringstream out;
        out << std::hex << std::uppercase;
        out.width(6);
        out.fill('0');
        out << (color & 0xFFFFFFu);
        return out.str();
    };
    return "foundation bg-secondary light " + hex(light.window) + " dark " + hex(dark.window);
}

bool SameFoundation(std::string& error) {
    const auto root = FoundationRoot();
    if (root.empty()) {
        error = "RobixUI: Foundation.rbxu was not found\n";
        return false;
    }
    const std::string shared = ReadText(root / "RobixUI" / "Components" / "Foundation.rbxu");
    const std::string web = ReadText(root / "RobixUI" / "Web" / "Foundation.rbxu");
    const std::string platform = ReadText(root / "RobixUI" / "Platform" / "Windows" / "Foundation.rbxu");
    if (shared.find("<Foundation") == std::string::npos) {
        error = "RobixUI: the shared foundation is missing\n";
        return false;
    }
    if (shared != web || shared != platform) {
        error = "RobixUI: web and Windows Foundation.rbxu are not the same foundation\n";
        return false;
    }
    return true;
}

} // namespace Robix::UI
