#include "ThemeToggleWeb.hpp"

#include "../../RoboCompilerShared.hpp"

namespace fs = std::filesystem;

namespace {

std::string gTheme = "dark";

bool Flag(const std::string& value) {
    const std::string flag = RoboWebLower(value);
    return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

std::string VariantOf(const RoboXMLNode& node) {
    if (Flag(RoboXMLAttr(node, "compact"))) return "compact";
    const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "default"));
    if (variant == "compact" || variant == "labels" || variant == "icons" || variant == "switch") return variant;
    return "default";
}

std::string Glyph(const fs::path& assetsRoot, const char* name, int size) {
    const std::string svg = RoboWebSvg(assetsRoot, name);
    if (svg.empty()) return {};
    return "<span class=\"robo-theme-glyph\" style=\"width:" + std::to_string(size) + "px;height:" + std::to_string(size) +
           "px\">" + svg + "</span>";
}

}  // namespace

void RoboWebThemePrepare(const std::string& theme) {
    gTheme = RoboWebLower(theme) == "light" ? "light" : "dark";
}

void RoboWebEmitThemeToggle(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                            const fs::path& assetsRoot) {
    const std::string id = RoboXMLAttr(node, "id");
    const std::string variant = VariantOf(node);
    const bool disabled = Flag(RoboXMLAttr(node, "disabled"));
    const bool switchOnly = variant == "switch";
    const bool stacked = variant == "default";
    const bool showLabel = variant != "icons" && !switchOnly;
    const bool showIcon = variant != "labels" && !switchOnly;
    const bool compactSwitch = !stacked;
    const int iconSize = stacked ? 18 : 16;
    const bool dark = gTheme != "light";
    const std::string style = RoboWebStyle(node, library);
    const std::string sun = showIcon ? Glyph(assetsRoot, "RadiantSunDaylightAppearance", iconSize) : "";
    const std::string moon = showIcon ? Glyph(assetsRoot, "CrescentMoonNightAppearance", iconSize) : "";

    html += "<div class=\"robo-theme\" data-variant=\"";
    html += variant;
    html += "\"";
    if (!id.empty()) {
        html += " id=\"";
        html += RoboWebEscape(id);
        html += "\"";
    }
    if (disabled) html += " data-disabled=\"true\"";
    if (!switchOnly) html += " aria-label=\"Appearance\"";
    if (!style.empty()) {
        html += " style=\"";
        html += RoboWebEscape(style);
        html += "\"";
    }
    html += ">";

    const auto mode = [&](const char* modeName, const char* label, const std::string& glyph) {
        const bool pressed = std::string(modeName) == gTheme;
        html += "<button class=\"robo-theme-mode robo-control\" type=\"button\" data-mode=\"";
        html += modeName;
        html += "\" aria-pressed=\"";
        html += pressed ? "true" : "false";
        html += "\" aria-label=\"";
        html += label;
        html += " mode\"";
        if (disabled) html += " disabled";
        html += ">";
        html += glyph;
        if (showLabel) {
            html += "<span class=\"robo-type-caption\">";
            html += label;
            html += "</span>";
        }
        html += "</button>";
    };

    if (!switchOnly) mode("light", "Light", sun);
    html += switchOnly ? "<div class=\"robo-theme-switch-wrap\" style=\"display:contents\">" : "<div class=\"robo-theme-switch-wrap\">";
    html += "<button class=\"robo-theme-switch robo-control";
    if (dark) html += " is-on";
    if (compactSwitch) html += " robo-theme-switch-compact";
    html += "\" type=\"button\" role=\"switch\" aria-checked=\"";
    html += dark ? "true" : "false";
    html += "\" aria-label=\"Toggle dark mode\"";
    if (disabled) html += " disabled";
    html += "><span class=\"robo-theme-knob\" aria-hidden=\"true\"></span></button></div>";
    if (!switchOnly) mode("dark", "Dark", moon);
    html += "</div>";
}

void RoboWebAppendThemeToggleCss(std::ostream& css) {
    css << ".robo-theme{box-sizing:border-box;align-items:center;font-family:var(--font-sans,system-ui,sans-serif)}\n";
    css << ".robo-theme[data-variant=\"default\"]{display:grid;width:100%;grid-template-columns:48px minmax(0,1fr) 48px;"
           "gap:10px;padding:4px 8px 8px}\n";
    css << ".robo-theme[data-variant=\"compact\"],.robo-theme[data-variant=\"labels\"],.robo-theme[data-variant=\"icons\"]"
           "{display:grid;width:auto;grid-template-columns:auto minmax(52px,64px) auto;gap:8px}\n";
    css << ".robo-theme[data-variant=\"switch\"]{display:inline-flex}\n";
    css << ".robo-theme-mode{display:flex;align-items:center;justify-content:center;margin:0;border:0;padding:0;"
           "background:transparent;color:var(--text-tertiary);font-family:inherit;cursor:pointer;"
           "transition:color 150ms cubic-bezier(0.28,0.11,0.32,1)}\n";
    css << ".robo-theme[data-variant=\"default\"] .robo-theme-mode{min-width:0;flex-direction:column;gap:4px}\n";
    css << ".robo-theme[data-variant=\"compact\"] .robo-theme-mode,.robo-theme[data-variant=\"labels\"] .robo-theme-mode,"
           ".robo-theme[data-variant=\"icons\"] .robo-theme-mode{height:44px;flex:0 0 auto;flex-direction:row;gap:4px;"
           "white-space:nowrap}\n";
    css << ".robo-theme-mode .robo-type-caption{color:inherit;font-size:12px;font-weight:400;line-height:1.35}\n";
    css << ".robo-theme[data-variant=\"compact\"] .robo-theme-mode .robo-type-caption,"
           ".robo-theme[data-variant=\"labels\"] .robo-theme-mode .robo-type-caption{line-height:1}\n";
    css << ".robo-theme-mode[aria-pressed=\"true\"],html[data-theme=\"light\"] .robo-theme-mode[data-mode=\"light\"],"
           "html[data-theme=\"dark\"] .robo-theme-mode[data-mode=\"dark\"]{color:var(--text-primary)}\n";
    css << ".robo-theme-mode:focus-visible{outline:2px solid var(--brand-primary);outline-offset:2px}\n";
    css << ".robo-theme-glyph{display:inline-flex;flex:0 0 auto}\n";
    css << ".robo-theme-glyph svg{width:100%;height:100%;display:block}\n";
    css << ".robo-theme-switch-wrap{display:flex;flex:0 0 auto;justify-content:center}\n";
    css << ".robo-theme[data-variant=\"default\"] .robo-theme-switch-wrap{min-width:0;width:100%}\n";
    css << ".robo-theme-switch{box-sizing:border-box;appearance:none;position:relative;width:72px;height:34px;"
           "min-width:72px;margin:0;padding:0;border:0;border-radius:980px;background:var(--border-default);cursor:pointer;"
           "outline:2px solid transparent;outline-offset:2px;"
           "transition:background-color 200ms cubic-bezier(0.28,0.11,0.32,1),outline-color 150ms linear}\n";
    css << ".robo-theme[data-variant=\"default\"] .robo-theme-switch{width:100%}\n";
    css << ".robo-theme-switch-compact{width:64px;height:28px;min-width:52px}\n";
    css << "html[data-theme=\"dark\"] .robo-theme-switch{background:var(--bg-tertiary)}\n";
    css << ".robo-theme-switch.is-on,html[data-theme=\"dark\"] .robo-theme-switch{background:var(--brand-primary)}\n";
    css << "html[data-theme=\"light\"] .robo-theme-switch{background:var(--border-default)}\n";
    css << ".robo-theme-switch:focus-visible{outline-color:var(--brand-primary)}\n";
    css << ".robo-theme-knob{position:absolute;top:4px;left:4px;width:26px;height:26px;border-radius:50%;"
           "background:var(--text-on-brand);transition:left 200ms cubic-bezier(0.28,0.11,0.32,1)}\n";
    css << ".robo-theme-switch-compact .robo-theme-knob{top:3px;left:3px;width:22px;height:22px}\n";
    css << ".robo-theme-knob::after{content:\"\";position:absolute;top:50%;left:50%;width:6px;height:6px;border-radius:50%;"
           "background:var(--text-disabled);transform:translate(-50%,-50%)}\n";
    css << ".robo-theme-switch-compact .robo-theme-knob::after{width:5px;height:5px}\n";
    css << ".robo-theme-switch.is-on .robo-theme-knob,html[data-theme=\"dark\"] .robo-theme-switch .robo-theme-knob"
           "{left:calc(100% - 30px)}\n";
    css << ".robo-theme-switch-compact.is-on .robo-theme-knob,html[data-theme=\"dark\"] .robo-theme-switch-compact "
           ".robo-theme-knob{left:calc(100% - 25px)}\n";
    css << "html[data-theme=\"light\"] .robo-theme-switch .robo-theme-knob{left:4px}\n";
    css << "html[data-theme=\"light\"] .robo-theme-switch-compact .robo-theme-knob{left:3px}\n";
    css << ".robo-theme[data-disabled=\"true\"] .robo-theme-mode,.robo-theme-switch:disabled,.robo-theme-mode:disabled"
           "{cursor:not-allowed;opacity:0.55}\n";
    css << "@media (prefers-reduced-motion:reduce){.robo-theme-switch,.robo-theme-knob,.robo-theme-mode{transition:none}}\n";
}

std::string RoboWebThemeScript() {
    return R"JS(
  function themeNow() {
    return document.documentElement.getAttribute("data-theme") === "light" ? "light" : "dark";
  }
  function themeApply(next, store) {
    if (next !== "light" && next !== "dark") return;
    document.documentElement.setAttribute("data-theme", next);
    document.documentElement.classList.toggle("dark", next === "dark");
    var pages = document.querySelectorAll(".robo-page,.robo-screen,.robo-window");
    for (var p = 0; p < pages.length; p++) pages[p].setAttribute("data-theme", next);
    var themeMeta = document.querySelector('meta[name="theme-color"]');
    if (themeMeta) {
      var surface = getComputedStyle(document.documentElement).getPropertyValue("--bg-primary");
      if (surface) themeMeta.setAttribute("content", surface.trim());
    }
    if (store) {
      try { localStorage.setItem("theme", next); } catch (e) {}
    }
    var dark = next === "dark";
    var modes = document.querySelectorAll(".robo-theme-mode");
    for (var i = 0; i < modes.length; i++) {
      modes[i].setAttribute("aria-pressed", modes[i].getAttribute("data-mode") === next ? "true" : "false");
    }
    var switches = document.querySelectorAll(".robo-theme-switch");
    for (var s = 0; s < switches.length; s++) {
      switches[s].setAttribute("aria-checked", dark ? "true" : "false");
      switches[s].classList.toggle("is-on", dark);
    }
    var swaps = document.querySelectorAll(".robo-theme-swap");
    for (var k = 0; k < swaps.length; k++) {
      swaps[k].setAttribute("aria-label", dark ? "Switch to light mode" : "Switch to dark mode");
    }
    var tracks = document.querySelectorAll(".robo-theme-segments .robo-segmented-track");
    for (var t = 0; t < tracks.length; t++) {
      var buttons = tracks[t].querySelectorAll(".robo-segment");
      var index = 0;
      for (var b = 0; b < buttons.length; b++) {
        var chosen = buttons[b].getAttribute("data-value") === next;
        buttons[b].setAttribute("aria-checked", chosen ? "true" : "false");
        if (!buttons[b].disabled) buttons[b].tabIndex = chosen ? 0 : -1;
        if (chosen) index = b;
      }
      tracks[t].setAttribute("data-index", String(index));
      tracks[t].style.setProperty("--robo-index", String(index));
    }
  }
  function themeBoot() {
    var stored = "";
    try { stored = localStorage.getItem("theme") || ""; } catch (e) {}
    if (stored === "light" || stored === "dark") themeApply(stored, false);
    else themeApply(themeNow(), false);
  }
  themeBoot();
  document.addEventListener("click", function (ev) {
    var mode = ev.target.closest ? ev.target.closest(".robo-theme-mode") : null;
    if (mode) {
      if (!mode.disabled) themeApply(mode.getAttribute("data-mode"), true);
      return;
    }
    var knob = ev.target.closest ? ev.target.closest(".robo-theme-switch") : null;
    if (knob) {
      if (!knob.disabled) themeApply(themeNow() === "dark" ? "light" : "dark", true);
      return;
    }
    var swap = ev.target.closest ? ev.target.closest(".robo-theme-swap") : null;
    if (swap) {
      if (!swap.disabled) themeApply(themeNow() === "dark" ? "light" : "dark", true);
      return;
    }
    var segment = ev.target.closest ? ev.target.closest(".robo-theme-segments .robo-segment") : null;
    if (segment && !segment.disabled) themeApply(segment.getAttribute("data-value"), true);
  });
  document.addEventListener("keydown", function (ev) {
    var segment = ev.target.closest ? ev.target.closest(".robo-theme-segments .robo-segment") : null;
    if (!segment) return;
    var key = ev.key;
    if (key !== "ArrowRight" && key !== "ArrowLeft" && key !== "ArrowDown" && key !== "ArrowUp" && key !== "Home" && key !== "End" && key !== " " && key !== "Enter") return;
    var track = segment.closest(".robo-segmented-track");
    if (!track) return;
    var chosen = track.querySelector(".robo-segment[aria-checked='true']");
    if (chosen && !chosen.disabled) themeApply(chosen.getAttribute("data-value"), true);
  });
  window.addEventListener("storage", function (ev) {
    if (ev.key !== "theme" && ev.key !== null) return;
    var stored = "";
    try { stored = localStorage.getItem("theme") || ""; } catch (e) {}
    if (stored === "light" || stored === "dark") themeApply(stored, false);
  });
)JS";
}

const char* RoboWebThemeToggleClass() { return "robo-theme"; }
