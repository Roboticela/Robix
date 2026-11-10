#include "ThemeIconToggleWeb.hpp"

#include "../Button/ButtonWeb.hpp"
#include "../../RoboCompilerShared.hpp"

namespace fs = std::filesystem;

namespace {

bool Flag(const std::string& value) {
    const std::string flag = RoboWebLower(value);
    return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

std::string VariantOf(const RoboXMLNode& node) {
    const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "ghost"));
    if (variant == "subtle" || variant == "outline" || variant == "pair" || variant == "segmented" || variant == "split") {
        return variant;
    }
    return "ghost";
}

std::string SizeOf(const RoboXMLNode& node) {
    const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "md"));
    if (size == "sm" || size == "small") return "sm";
    return "md";
}

std::string SwapGlyph(const fs::path& assetsRoot, const char* name, const char* glyph, int size) {
    const std::string svg = RoboWebSvg(assetsRoot, name);
    if (svg.empty()) return {};
    return "<span class=\"robo-icon-button-glyph\" data-glyph=\"" + std::string(glyph) + "\" style=\"width:" +
           std::to_string(size) + "px;height:" + std::to_string(size) + "px\">" + svg + "</span>";
}

std::string SegmentGlyph(const fs::path& assetsRoot, const char* name, int size) {
    const std::string svg = RoboWebSvg(assetsRoot, name);
    if (svg.empty()) return {};
    return "<span class=\"robo-segment-icon\" aria-hidden=\"true\" style=\"width:" + std::to_string(size) +
           "px;height:" + std::to_string(size) + "px\">" + svg + "</span>";
}

}  // namespace

void RoboWebEmitThemeIconToggle(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                                const fs::path& assetsRoot) {
    const std::string id = RoboXMLAttr(node, "id");
    const std::string variant = VariantOf(node);
    const std::string size = SizeOf(node);
    const bool disabled = Flag(RoboXMLAttr(node, "disabled"));
    const int icon = size == "sm" ? 15 : 16;
    const std::string styleBase = RoboWebStyle(node, library);
    std::string style = styleBase;
    RoboWebAppendControlStyle(style, node, library);
    std::string control;
    RoboWebAppendControlStyle(control, node, library);
    const bool single = variant == "ghost" || variant == "subtle" || variant == "outline";

    if (single) {
        html += "<button class=\"robo-icon-button robo-theme-swap robo-control\" type=\"button\" data-variant=\"";
        html += variant;
        html += "\" data-size=\"sm\" aria-label=\"Switch to light mode\"";
        if (!id.empty()) {
            html += " id=\"";
            html += RoboWebEscape(id);
            html += "\"";
        }
        if (disabled) html += " disabled";
        RoboWebAppendPreviewAttr(html, node);
        if (!style.empty()) {
            html += " style=\"";
            html += RoboWebEscape(style);
            html += "\"";
        }
        html += ">";
        html += SwapGlyph(assetsRoot, "CrescentMoonNightAppearance", "moon", icon);
        html += SwapGlyph(assetsRoot, "RadiantSunDaylightAppearance", "sun", icon);
        html += "</button>";
        return;
    }

    const char* look = "pill";
    if (variant == "segmented") look = "outline";
    else if (variant == "split") look = "ghost";
    const char* trackSize = size == "sm" ? "sm" : "md";
    html += "<div class=\"robo-segmented robo-theme-segments\" data-variant=\"";
    html += variant;
    html += "\"";
    if (!id.empty()) {
        html += " id=\"";
        html += RoboWebEscape(id);
        html += "\"";
    }
    if (!styleBase.empty()) {
        html += " style=\"";
        html += RoboWebEscape(styleBase);
        html += "\"";
    }
    html += "><div class=\"robo-segmented-track\" role=\"radiogroup\" aria-label=\"Appearance\" data-variant=\"";
    html += look;
    html += "\" data-size=\"";
    html += trackSize;
    html += "\" data-content=\"icon\" data-mode=\"inset\" data-count=\"2\" style=\"--robo-count:2\"";
    if (disabled) html += " aria-disabled=\"true\"";
    html += "><span class=\"robo-segmented-thumb\" aria-hidden=\"true\"></span>";

    const auto segment = [&](const char* value, const char* label, const char* iconName) {
        html += "<button class=\"robo-segment robo-control\" type=\"button\" role=\"radio\" data-value=\"";
        html += value;
        html += "\" aria-checked=\"false\" tabindex=\"-1\" aria-label=\"";
        html += label;
        html += "\"";
        if (disabled) html += " disabled aria-disabled=\"true\"";
        RoboWebAppendPreviewAttr(html, node);
        if (!control.empty()) {
            html += " style=\"";
            html += RoboWebEscape(control);
            html += "\"";
        }
        html += "><span class=\"robo-segment-face\">";
        html += SegmentGlyph(assetsRoot, iconName, icon);
        html += "<span class=\"robo-segment-label\"><span class=\"robo-segment-copy\">";
        html += label;
        html += "</span><span class=\"robo-segment-sizer\" aria-hidden=\"true\">";
        html += label;
        html += "</span></span></span></button>";
    };
    segment("light", "Light mode", "RadiantSunDaylightAppearance");
    segment("dark", "Dark mode", "CrescentMoonNightAppearance");
    html += "</div></div>";
}

void RoboWebAppendThemeIconToggleCss(std::ostream& css) {
    css << ".robo-theme-swap{color:var(--text-secondary)}\n";
    css << ".robo-theme-swap:hover,.robo-theme-swap:focus-visible{color:var(--text-primary)}\n";
    css << ".robo-theme-swap [data-glyph]{display:none}\n";
    css << "html[data-theme=\"light\"] .robo-theme-swap [data-glyph=\"moon\"],html:not([data-theme=\"light\"]) "
           ".robo-theme-swap [data-glyph=\"sun\"]{display:inline-flex}\n";
    css << ".robo-theme-segments{width:auto}\n";
    css << ".robo-theme-segments .robo-segment-icon svg{width:100%;height:100%}\n";
    css << "html[data-theme=\"light\"] .robo-theme-segments .robo-segmented-track{--robo-index:0}\n";
    css << "html[data-theme=\"dark\"] .robo-theme-segments .robo-segmented-track{--robo-index:1}\n";
    css << "html[data-theme=\"light\"] .robo-theme-segments .robo-segment[data-value=\"light\"],"
           "html[data-theme=\"dark\"] .robo-theme-segments .robo-segment[data-value=\"dark\"]"
           "{color:var(--text-primary);font-weight:500}\n";
}

const char* RoboWebThemeIconToggleClass() { return "robo-theme-swap"; }
