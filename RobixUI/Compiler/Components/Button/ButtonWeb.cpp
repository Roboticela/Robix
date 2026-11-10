#include "ButtonWeb.hpp"

#include "Button.hpp"
#include "../../RoboCompilerShared.hpp"
#include "../../Motion.hpp"
#include "../../ScreenSize.hpp"

#include <cstdlib>

namespace fs = std::filesystem;

namespace {

bool RoboWebFlag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes";
}

bool RoboWebFullWidth(const RoboXMLNode& node) {
  const std::string value = RoboWebLower(RoboXMLAttr(node, "fullWidth", "true"));
  return value != "false" && value != "0" && value != "no" && value != "off";
}

std::string RoboWebButtonRoute(const RoboXMLNode& node) {
  std::string route = RoboXMLAttr(node, "path");
  if (route.empty()) {
    route = RoboXMLAttr(node, "route");
  }
  if (route.empty()) {
    route = RoboXMLAttr(node, "href");
  }
  if (route.empty()) {
    route = RoboXMLAttr(node, "screen");
  }
  return route;
}

bool RoboWebButtonExternal(const std::string& route) {
  const std::string lower = RoboWebLower(route);
  return lower.rfind("http://", 0) == 0 || lower.rfind("https://", 0) == 0 || lower.rfind("mailto:", 0) == 0 ||
         lower.rfind("tel:", 0) == 0 || lower.rfind("//", 0) == 0;
}

std::string RoboWebButtonVariant(const RoboXMLNode& node) {
  std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "primary"));
  if (variant == "destructive") {
    variant = "danger";
  }
  if (variant != "primary" && variant != "secondary" && variant != "action" && variant != "ghost" &&
      variant != "danger") {
    return "primary";
  }
  return variant;
}

std::string RoboWebButtonSize(const RoboXMLNode& node) {
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "md"));
  if (size == "xs" || size == "extra-small") return "xs";
  if (size == "small" || size == "sm") return "sm";
  if (size == "large" || size == "lg") return "lg";
  if (size == "xl" || size == "extra-large") return "xl";
  if (size == "2xl") return "2xl";
  if (size == "3xl") return "3xl";
  return "md";
}

std::string RoboWebButtonIconWrap(const std::string& svg) {
  if (svg.empty()) {
    return {};
  }
  return "<span class=\"robo-button-icon\">" + svg + "</span>";
}

int RoboWebButtonIconSide(const RoboXMLNode& node) {
  const std::string side =
      RoboWebLower(RoboXMLAttr(node, "slot", RoboXMLAttr(node, "side", RoboXMLAttr(node, "place").c_str()).c_str()));
  if (side == "trailing" || side == "right" || side == "end") {
    return 1;
  }
  if (side == "leading" || side == "left" || side == "start") {
    return 0;
  }
  return -1;
}

std::string RoboWebButtonSpinner() {
  return "<svg class=\"robo-button-spinner\" width=\"16\" height=\"16\" viewBox=\"0 0 32 32\" fill=\"none\" "
         "aria-hidden=\"true\"><circle class=\"robo-button-spinner-track\" cx=\"16\" cy=\"16\" r=\"14\" "
         "stroke-width=\"2\"></circle><circle class=\"robo-button-spinner-tick\" cx=\"16\" cy=\"16\" r=\"14\" "
         "stroke-width=\"2\" stroke-linecap=\"round\" stroke-dasharray=\"24.63 63.33\"></circle></svg>";
}

}  // namespace

void RoboWebEmitButton(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                       const fs::path& assetsRoot) {
  const std::string id = RoboXMLAttr(node, "id");
  const std::string variant = RoboWebButtonVariant(node);
  const std::string size = RoboWebButtonSize(node);
  const std::string type = RoboWebLower(RoboXMLAttr(node, "type", "button"));
  const std::string loadingText = RoboXMLAttr(node, "loadingText");
  const bool loading = RoboWebFlag(RoboXMLAttr(node, "loading"));
  const bool disabled = RoboWebFlag(RoboXMLAttr(node, "disabled")) || loading;
  const bool fullWidth = RoboWebFullWidth(node);
  std::string leading = RoboWebButtonIconWrap(RoboWebSvg(assetsRoot, RoboXMLAttr(node, "leading")));
  std::string trailing = RoboWebButtonIconWrap(RoboWebSvg(assetsRoot, RoboXMLAttr(node, "trailing")));
  for (const RoboXMLNode& child : node.children) {
    const std::string childName = RoboWebLower(child.name);
    if (childName != "icon" && childName != "svg") {
      continue;
    }
    const std::string icon = RoboWebButtonIconWrap(RoboWebIconMarkup(assetsRoot, child));
    if (icon.empty()) {
      continue;
    }
    const int side = RoboWebButtonIconSide(child);
    if (side == 1) {
      trailing = icon;
    } else if (side == 0 || leading.empty()) {
      leading = icon;
    } else {
      trailing = icon;
    }
  }
  std::string label = RoboXMLAttr(node, "content");
  if (label.empty()) {
    label = RoboXMLAttr(node, "text");
  }
  if (label.empty()) {
    label = RoboXMLAttr(node, "value");
  }
  if (label.empty()) {
    label = RoboCompilerTrim(node.text);
  }
  std::string style = RoboWebStyle(node, library);
  RoboWebAppendControlStyle(style, node, library);
  const std::string route = RoboWebButtonRoute(node);
  const bool link = !route.empty();
  std::string href = route;
  std::string dataRoute;
  if (link && !RoboWebButtonExternal(route)) {
    std::string normalized = route;
    if (!normalized.empty() && normalized[0] == '#') {
      normalized = normalized.substr(1);
    }
    if (normalized.empty() || normalized[0] != '/') {
      normalized = "/" + normalized;
    }
    href = "#" + normalized;
    dataRoute = normalized;
  }
  html += link ? "<a class=\"robo-button robo-control\" data-variant=\"" : "<button class=\"robo-button robo-control\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\" data-size=\"";
  html += RoboWebEscape(size);
  html += "\" data-full=\"";
  html += fullWidth ? "true" : "false";
  html += "\"";
  if (link) {
    html += " href=\"";
    html += RoboWebEscape(href);
    html += "\"";
    if (!dataRoute.empty()) {
      html += " data-route=\"";
      html += RoboWebEscape(dataRoute);
      html += "\"";
    }
  } else {
    html += " type=\"";
    html += RoboWebEscape(type == "submit" || type == "reset" ? type : std::string("button"));
    html += "\"";
  }
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  RoboWebControlAttrs(html, node);
  if (loading) {
    html += " data-loading=\"true\" aria-busy=\"true\"";
  }
  if (disabled) {
    html += link ? " aria-disabled=\"true\" tabindex=\"-1\"" : " disabled";
  }
  RoboWebAppendMotion(style, node);
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += RoboWebScreenAttrs(node, library, "inline-flex");
  html += RoboWebMotionAttrs(node);
  html += "><span class=\"robo-button-label\">";
  if (loading) {
    html += RoboWebButtonSpinner();
  }
  html += "<span class=\"robo-button-content\">";
  if (!loading && !leading.empty()) {
    html += leading;
  }
  html += RoboWebEscape(loading && !loadingText.empty() ? loadingText : label);
  if (!loading && !trailing.empty()) {
    html += trailing;
  }
  html += "</span></span>";
  html += link ? "</a>" : "</button>";
}

void RoboWebAppendButtonCss(std::ostream& css) {
  css << ".robo-button{box-sizing:border-box;display:inline-flex;align-items:center;justify-content:center;"
         "width:100%;margin:0;padding:0 22px;height:44px;border:0;border-radius:980px;background:var(--brand-primary);"
         "color:var(--text-on-brand);font:500 17px/1.35 var(--font-sans,system-ui,sans-serif);white-space:nowrap;"
         "cursor:pointer;text-decoration:none;outline:2px solid transparent;outline-offset:2px;box-shadow:none;"
         "transition:background-color 150ms linear,border-color 150ms linear,color 150ms linear,"
         "outline-color 150ms linear,box-shadow 150ms linear}\n";
  css << ".robo-button[data-full=\"false\"]{width:auto}\n";
  css << ".robo-button[data-size=\"sm\"]{height:32px;padding:0 14px;font-size:14px}\n";
  css << ".robo-button[data-size=\"lg\"]{height:50px;padding:0 24px}\n";
  css << ".robo-button-label,.robo-button-content{display:inline-flex;align-items:center;justify-content:center;"
         "gap:8px;white-space:nowrap}\n";
  css << ".robo-button-icon{display:inline-flex;width:16px;height:16px;flex:0 0 16px}\n";
  css << ".robo-button-icon svg{width:16px;height:16px;display:block}\n";
  css << ".robo-button-spinner{display:block;flex:0 0 16px;transform-origin:center;"
         "animation:robo-button-spin 1s linear infinite}\n";
  css << "@keyframes robo-button-spin{to{transform:rotate(360deg)}}\n";
  css << "@media (prefers-reduced-motion:reduce){.robo-button-spinner{animation:none}}\n";
  css << ".robo-button-spinner-track{stroke:color-mix(in srgb,var(--text-tertiary) 35%,transparent)}\n";
  css << ".robo-button-spinner-tick{stroke:var(--brand-primary)}\n";
  css << ".robo-button[data-variant=\"primary\"] .robo-button-spinner-track,"
         ".robo-button[data-variant=\"danger\"] .robo-button-spinner-track"
         "{stroke:color-mix(in srgb,var(--text-on-brand) 35%,transparent)}\n";
  css << ".robo-button[data-variant=\"primary\"] .robo-button-spinner-tick,"
         ".robo-button[data-variant=\"danger\"] .robo-button-spinner-tick{stroke:var(--text-on-brand)}\n";
  css << ".robo-button[data-variant=\"secondary\"]{border:1px solid var(--brand-primary);background:transparent;"
         "color:var(--brand-primary)}\n";
  css << ".robo-button[data-variant=\"action\"]{gap:8px;border:1px solid var(--border-default);"
         "background:var(--bg-elevated);color:var(--text-primary)}\n";
  css << "html[data-theme=\"dark\"] .robo-button[data-variant=\"action\"]{border-color:var(--border-strong);"
         "background:var(--bg-secondary)}\n";
  css << ".robo-button[data-variant=\"ghost\"]{border:0;background:transparent;color:var(--text-tertiary);"
         "font-weight:400}\n";
  css << ".robo-button[data-variant=\"danger\"]{border:0;background:var(--danger);color:var(--text-on-brand)}\n";
  css << ".robo-button[data-variant=\"primary\"]:hover:not(:disabled),"
         ".robo-button[data-variant=\"primary\"][data-preview=\"hover\"]{background:var(--brand-primary-hover)}\n";
  css << ".robo-button[data-variant=\"primary\"]:active:not(:disabled),"
         ".robo-button[data-variant=\"primary\"][data-preview=\"active\"]{background:var(--brand-primary-active)}\n";
  css << ".robo-button[data-variant=\"secondary\"]:hover:not(:disabled),"
         ".robo-button[data-variant=\"secondary\"][data-preview=\"hover\"]{border-color:var(--brand-primary);"
         "background:var(--brand-primary);color:var(--text-on-brand)}\n";
  css << ".robo-button[data-variant=\"secondary\"]:active:not(:disabled),"
         ".robo-button[data-variant=\"secondary\"][data-preview=\"active\"]{border-color:var(--brand-primary-active);"
         "background:var(--brand-primary-active);color:var(--text-on-brand)}\n";
  css << ".robo-button[data-variant=\"action\"]:hover:not(:disabled),"
         ".robo-button[data-variant=\"action\"][data-preview=\"hover\"]{border-color:var(--text-tertiary);"
         "background:var(--bg-hover)}\n";
  css << ".robo-button[data-variant=\"action\"]:active:not(:disabled),"
         ".robo-button[data-variant=\"action\"][data-preview=\"active\"]{border-color:var(--text-primary);"
         "background:var(--bg-active)}\n";
  css << ".robo-button[data-variant=\"ghost\"]:hover:not(:disabled),"
         ".robo-button[data-variant=\"ghost\"][data-preview=\"hover\"]{color:var(--brand-primary);"
         "background:transparent}\n";
  css << ".robo-button[data-variant=\"ghost\"]:active:not(:disabled),"
         ".robo-button[data-variant=\"ghost\"][data-preview=\"active\"]{color:var(--brand-primary-active);"
         "background:var(--brand-primary-subtle)}\n";
  css << ".robo-button[data-variant=\"danger\"]:hover:not(:disabled),"
         ".robo-button[data-variant=\"danger\"][data-preview=\"hover\"]{background:var(--danger-hover)}\n";
  css << ".robo-button[data-variant=\"danger\"]:active:not(:disabled),"
         ".robo-button[data-variant=\"danger\"][data-preview=\"active\"]"
         "{background:color-mix(in srgb,var(--danger) 78%,black)}\n";
  css << ".robo-button:focus-visible,.robo-button[data-preview=\"focus\"]{border-color:var(--brand-primary);"
         "outline-color:color-mix(in srgb,var(--brand-primary) 48%,transparent);"
         "box-shadow:0 0 5px 0 color-mix(in srgb,var(--brand-primary) 36%,transparent)}\n";
  css << ".robo-button:disabled,.robo-button[aria-disabled=\"true\"]{opacity:0.55;cursor:not-allowed}\n";
  css << ".robo-button[aria-disabled=\"true\"]{pointer-events:none}\n";
}

const char* RoboWebButtonClass() { return "robo-button"; }

namespace {

std::string RoboWebControlLength(const RoboFoundationLibrary& library, const std::string& raw) {
  return RoboWebCssValue(library, raw, false);
}

std::string RoboWebControlBox(const RoboFoundationLibrary& library, const std::string& raw) {
  std::string out;
  std::string token;
  const auto flush = [&]() {
    if (token.empty()) return;
    if (!out.empty()) out += " ";
    out += RoboWebControlLength(library, token);
    token.clear();
  };
  for (char ch : raw) {
    if (ch == ' ' || ch == '\t') flush();
    else token.push_back(ch);
  }
  flush();
  return out;
}

std::string RoboWebControlColor(const RoboFoundationLibrary& library, const std::string& raw) {
  return RoboWebCssValue(library, raw, true);
}

std::string RoboWebControlShadow(const std::string& name) {
  const std::string key = RoboWebLower(name);
  if (key == "none" || key == "flat") return "none";
  if (key == "subtle") return "0 1px 2px rgba(0,0,0,.28)";
  if (key == "soft") return "0 4px 14px rgba(0,0,0,.26)";
  if (key == "medium" || key == "elevated") return "0 8px 22px rgba(0,0,0,.32)";
  if (key == "large" || key == "floating") return "0 16px 36px rgba(0,0,0,.38)";
  if (key == "inner" || key == "recessed") return "inset 0 2px 8px rgba(0,0,0,.45)";
  if (key == "glow" || key == "neon") return "0 0 18px rgba(59,158,255,.55)";
  if (key == "3d") return "0 4px 0 rgba(0,0,0,.45)";
  return name;
}

void RoboWebControlPaintState(std::string& style, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                              const char* prefix) {
  const auto raw = [&](const char* suffix) {
    return RoboXMLAttr(node, (std::string(prefix) + suffix).c_str());
  };
  const std::string stem = std::string("--btn-") + prefix;
  std::string background = raw("Background");
  std::string ink = raw("Color");
  if (std::string(prefix) == "disabled") {
    if (background.empty()) background = RoboXMLAttr(node, "disabledBg");
    if (ink.empty()) ink = RoboXMLAttr(node, "disabledColor");
  }
  RoboWebAddDecl(style, (stem + "-bg").c_str(), RoboWebControlColor(library, background));
  RoboWebAddDecl(style, (stem + "-color").c_str(), RoboWebControlColor(library, ink));
  RoboWebAddDecl(style, (stem + "-icon").c_str(), RoboWebControlColor(library, raw("Icon")));
  RoboWebAddDecl(style, (stem + "-border").c_str(), RoboWebControlColor(library, raw("Border")));
  RoboWebAddDecl(style, (stem + "-border-width").c_str(), RoboWebControlLength(library, raw("BorderWidth")));
  const std::string borderStyle = RoboWebLower(raw("BorderStyle"));
  if (!borderStyle.empty()) RoboWebAddDecl(style, (stem + "-border-style").c_str(), borderStyle);
  RoboWebAddDecl(style, (stem + "-border-top").c_str(), RoboWebControlColor(library, raw("BorderTop")));
  RoboWebAddDecl(style, (stem + "-border-right").c_str(), RoboWebControlColor(library, raw("BorderRight")));
  RoboWebAddDecl(style, (stem + "-border-bottom").c_str(), RoboWebControlColor(library, raw("BorderBottom")));
  RoboWebAddDecl(style, (stem + "-border-left").c_str(), RoboWebControlColor(library, raw("BorderLeft")));
  RoboWebAddDecl(style, (stem + "-width-top").c_str(), RoboWebControlLength(library, raw("BorderTopWidth")));
  RoboWebAddDecl(style, (stem + "-width-right").c_str(), RoboWebControlLength(library, raw("BorderRightWidth")));
  RoboWebAddDecl(style, (stem + "-width-bottom").c_str(), RoboWebControlLength(library, raw("BorderBottomWidth")));
  RoboWebAddDecl(style, (stem + "-width-left").c_str(), RoboWebControlLength(library, raw("BorderLeftWidth")));
  RoboWebAddDecl(style, (stem + "-radius").c_str(), RoboWebControlLength(library, raw("Radius")));
  RoboWebAddDecl(style, (stem + "-pad").c_str(), RoboWebControlBox(library, raw("Padding")));
  if (!raw("Shadow").empty() || !raw("Glow").empty()) {
    const std::string glow = RoboWebControlColor(library, raw("Glow"));
    RoboWebAddDecl(style, (stem + "-shadow").c_str(),
                   glow.empty() ? RoboWebControlShadow(raw("Shadow")) : "0 0 18px " + glow);
  }
  std::string motion;
  if (!raw("Scale").empty()) motion += "scale(" + raw("Scale") + ")";
  if (!raw("X").empty() || !raw("Y").empty()) {
    if (!motion.empty()) motion += " ";
    motion += "translate(" + RoboWebControlLength(library, raw("X").empty() ? "0" : raw("X")) + "," +
              RoboWebControlLength(library, raw("Y").empty() ? "0" : raw("Y")) + ")";
  }
  RoboWebAddDecl(style, (stem + "-transform").c_str(), motion);
}

}  // namespace

void RoboWebAppendPreviewAttr(std::string& html, const RoboXMLNode& node) {
  const std::string preview = RoboWebLower(RoboXMLAttr(node, "preview"));
  if (preview.empty()) return;
  html += " data-preview=\"";
  html += RoboWebEscape(preview);
  html += "\"";
}

void RoboWebControlAttrs(std::string& html, const RoboXMLNode& node) {
  const auto data = [&](const char* key, std::string value) {
    if (value.empty()) return;
    html += " data-";
    html += key;
    html += "=\"";
    html += RoboWebEscape(value);
    html += "\"";
  };
  std::string preset = RoboWebLower(RoboXMLAttr(node, "preset"));
  const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant"));
  if (preset.empty() && RoboCompilerIsButtonPreset(variant)) preset = variant;
  if (preset == "frosted" || preset == "liquid" || preset == "acrylic" || preset == "mica") preset = "glass";
  if (preset == "minimal" || preset == "minimalist") preset = "flat";
  if (preset == "windows") preset = "fluent";
  data("preset", preset);
  data("shape", RoboWebLower(RoboXMLAttr(node, "shape")));
  data("icon", RoboWebLower(RoboXMLAttr(node, "iconPlace")));
  data("density", RoboWebLower(RoboXMLAttr(node, "density")));
  data("theme", RoboWebLower(RoboXMLAttr(node, "theme")));
  data("chrome", RoboWebLower(RoboXMLAttr(node, "chrome")));
  data("enter", RoboWebLower(RoboXMLAttr(node, "enter")));
  data("motion", RoboWebLower(RoboXMLAttr(node, "motion")));
  data("texture", RoboWebLower(RoboXMLAttr(node, "texture")));
  data("accent", RoboWebLower(RoboXMLAttr(node, "accent")));
  data("group", RoboWebLower(RoboXMLAttr(node, "group")));
  data("click", RoboXMLAttr(node, "onclick", RoboXMLAttr(node, "onClick").c_str()));
  if (RoboWebLower(RoboXMLAttr(node, "selected")) == "true") data("selected", "true");
  RoboWebAppendPreviewAttr(html, node);
}

void RoboWebAppendControlStyle(std::string& style, const RoboXMLNode& node, const RoboFoundationLibrary& library) {
  const std::string widthMode = RoboWebLower(RoboXMLAttr(node, "widthMode"));
  if (widthMode == "auto" || widthMode == "fit" || widthMode == "fit-content") RoboWebAddDecl(style, "width", "fit-content");
  else if (widthMode == "full") RoboWebAddDecl(style, "width", "100%");
  else if (widthMode == "min" || widthMode == "min-content") RoboWebAddDecl(style, "width", "min-content");
  else if (widthMode == "max" || widthMode == "max-content") RoboWebAddDecl(style, "width", "max-content");
  const std::string heightMode = RoboWebLower(RoboXMLAttr(node, "heightMode"));
  if (heightMode == "auto" || heightMode == "content" || heightMode == "fit") RoboWebAddDecl(style, "height", "auto");
  else if (heightMode == "full") RoboWebAddDecl(style, "height", "100%");
  RoboWebAddDecl(style, "min-width", RoboWebControlLength(library, RoboXMLAttr(node, "minWidth")));
  RoboWebAddDecl(style, "max-width", RoboWebControlLength(library, RoboXMLAttr(node, "maxWidth")));
  RoboWebAddDecl(style, "min-height", RoboWebControlLength(library, RoboXMLAttr(node, "minHeight")));
  RoboWebAddDecl(style, "max-height", RoboWebControlLength(library, RoboXMLAttr(node, "maxHeight")));
  const std::string padX = RoboWebControlLength(library, RoboXMLAttr(node, "paddingX"));
  const std::string padY = RoboWebControlLength(library, RoboXMLAttr(node, "paddingY"));
  if (!padX.empty() || !padY.empty()) {
    RoboWebAddDecl(style, "padding", (padY.empty() ? "0" : padY) + " " + (padX.empty() ? "0" : padX));
  }
  RoboWebAddDecl(style, "padding-top", RoboWebControlLength(library, RoboXMLAttr(node, "paddingTop")));
  RoboWebAddDecl(style, "padding-right", RoboWebControlLength(library, RoboXMLAttr(node, "paddingRight")));
  RoboWebAddDecl(style, "padding-bottom", RoboWebControlLength(library, RoboXMLAttr(node, "paddingBottom")));
  RoboWebAddDecl(style, "padding-left", RoboWebControlLength(library, RoboXMLAttr(node, "paddingLeft")));
  RoboWebAddDecl(style, "padding", RoboWebControlBox(library, RoboXMLAttr(node, "padding")));
  const std::string marginX = RoboWebControlLength(library, RoboXMLAttr(node, "marginX"));
  const std::string marginY = RoboWebControlLength(library, RoboXMLAttr(node, "marginY"));
  if (!marginX.empty() || !marginY.empty()) {
    RoboWebAddDecl(style, "margin", (marginY.empty() ? "0" : marginY) + " " + (marginX.empty() ? "0" : marginX));
  }
  RoboWebAddDecl(style, "margin", RoboWebControlBox(library, RoboXMLAttr(node, "margin")));
  RoboWebAddDecl(style, "margin-top", RoboWebControlLength(library, RoboXMLAttr(node, "marginTop")));
  RoboWebAddDecl(style, "margin-right", RoboWebControlLength(library, RoboXMLAttr(node, "marginRight")));
  RoboWebAddDecl(style, "margin-bottom", RoboWebControlLength(library, RoboXMLAttr(node, "marginBottom")));
  RoboWebAddDecl(style, "margin-left", RoboWebControlLength(library, RoboXMLAttr(node, "marginLeft")));
  RoboWebAddDecl(style, "--btn-gap", RoboWebControlLength(library, RoboXMLAttr(node, "gap").empty() ? RoboXMLAttr(node, "iconGap") : RoboXMLAttr(node, "gap")));
  const std::string from = RoboWebControlColor(library, RoboXMLAttr(node, "gradientFrom"));
  const std::string to = RoboWebControlColor(library, RoboXMLAttr(node, "gradientTo"));
  std::string gradient = RoboXMLAttr(node, "gradient");
  if (!from.empty() || !to.empty()) {
    const std::string angle = RoboXMLAttr(node, "gradientAngle").empty() ? "160deg" : RoboXMLAttr(node, "gradientAngle");
    const std::string kind = RoboWebLower(gradient);
    const std::string body = from + "," + (to.empty() ? from : to);
    if (kind == "radial") gradient = "radial-gradient(circle," + body + ")";
    else if (kind == "conic") gradient = "conic-gradient(from " + angle + "," + body + ")";
    else gradient = "linear-gradient(" + angle + "," + body + ")";
  }
  if (!gradient.empty() && gradient.find('(') != std::string::npos) RoboWebAddDecl(style, "background", gradient);
  const std::string image = RoboXMLAttr(node, "backgroundImage");
  if (!image.empty()) {
    RoboWebAddDecl(style, "background-image", "url('" + image + "')");
    RoboWebAddDecl(style, "background-size", "cover");
    RoboWebAddDecl(style, "background-position", "center");
  }
  const std::string overlay = RoboWebControlColor(library, RoboXMLAttr(node, "overlay"));
  if (!overlay.empty()) RoboWebAddDecl(style, "box-shadow", "inset 0 0 0 999px " + overlay);
  if (!RoboXMLAttr(node, "backdropBlur").empty() || RoboWebLower(RoboXMLAttr(node, "glass")) == "true") {
    const std::string blur = RoboWebControlLength(library, RoboXMLAttr(node, "backdropBlur"));
    RoboWebAddDecl(style, "backdrop-filter", "blur(" + (blur.empty() ? "16px" : blur) + ")");
  }
  const std::string borderStyle = RoboWebLower(RoboXMLAttr(node, "borderStyle"));
  if (!borderStyle.empty()) RoboWebAddDecl(style, "border-style", borderStyle == "none" ? "none" : borderStyle);
  RoboWebAddDecl(style, "border-width", RoboWebControlLength(library, RoboXMLAttr(node, "borderWidth")));
  RoboWebAddDecl(style, "border-color", RoboWebControlColor(library, RoboXMLAttr(node, "borderColor")));
  RoboWebAddDecl(style, "border-top-width", RoboWebControlLength(library, RoboXMLAttr(node, "borderTopWidth")));
  RoboWebAddDecl(style, "border-right-width", RoboWebControlLength(library, RoboXMLAttr(node, "borderRightWidth")));
  RoboWebAddDecl(style, "border-bottom-width", RoboWebControlLength(library, RoboXMLAttr(node, "borderBottomWidth")));
  RoboWebAddDecl(style, "border-left-width", RoboWebControlLength(library, RoboXMLAttr(node, "borderLeftWidth")));
  RoboWebAddDecl(style, "border-top-color", RoboWebControlColor(library, RoboXMLAttr(node, "borderTopColor")));
  RoboWebAddDecl(style, "border-right-color", RoboWebControlColor(library, RoboXMLAttr(node, "borderRightColor")));
  RoboWebAddDecl(style, "border-bottom-color", RoboWebControlColor(library, RoboXMLAttr(node, "borderBottomColor")));
  RoboWebAddDecl(style, "border-left-color", RoboWebControlColor(library, RoboXMLAttr(node, "borderLeftColor")));
  const std::string radius = RoboWebLower(RoboXMLAttr(node, "radius"));
  if (radius == "none" || radius == "square") RoboWebAddDecl(style, "border-radius", "0");
  else if (radius == "xs") RoboWebAddDecl(style, "border-radius", "4px");
  else if (radius == "sm" || radius == "small") RoboWebAddDecl(style, "border-radius", "8px");
  else if (radius == "md" || radius == "medium") RoboWebAddDecl(style, "border-radius", "12px");
  else if (radius == "lg" || radius == "large") RoboWebAddDecl(style, "border-radius", "16px");
  else if (radius == "xl" || radius == "full" || radius == "pill") RoboWebAddDecl(style, "border-radius", "999px");
  else RoboWebAddDecl(style, "border-radius", RoboWebControlLength(library, RoboXMLAttr(node, "radius")));
  RoboWebAddDecl(style, "border-top-left-radius", RoboWebControlLength(library, RoboXMLAttr(node, "radiusTopLeft")));
  RoboWebAddDecl(style, "border-top-right-radius", RoboWebControlLength(library, RoboXMLAttr(node, "radiusTopRight")));
  RoboWebAddDecl(style, "border-bottom-right-radius", RoboWebControlLength(library, RoboXMLAttr(node, "radiusBottomRight")));
  RoboWebAddDecl(style, "border-bottom-left-radius", RoboWebControlLength(library, RoboXMLAttr(node, "radiusBottomLeft")));
  if (!RoboXMLAttr(node, "shadows").empty()) RoboWebAddDecl(style, "box-shadow", RoboXMLAttr(node, "shadows"));
  else if (!RoboXMLAttr(node, "shadowX").empty() || !RoboXMLAttr(node, "shadowY").empty() || !RoboXMLAttr(node, "shadowBlur").empty()) {
    const std::string color = RoboWebControlColor(library, RoboXMLAttr(node, "shadowColor"));
    RoboWebAddDecl(style, "box-shadow",
                   std::string(RoboWebLower(RoboXMLAttr(node, "shadowInset")) == "true" ? "inset " : "") +
                       RoboWebControlLength(library, RoboXMLAttr(node, "shadowX").empty() ? "0" : RoboXMLAttr(node, "shadowX")) + " " +
                       RoboWebControlLength(library, RoboXMLAttr(node, "shadowY").empty() ? "8" : RoboXMLAttr(node, "shadowY")) + " " +
                       RoboWebControlLength(library, RoboXMLAttr(node, "shadowBlur").empty() ? "20" : RoboXMLAttr(node, "shadowBlur")) + " " +
                       RoboWebControlLength(library, RoboXMLAttr(node, "shadowSpread").empty() ? "0" : RoboXMLAttr(node, "shadowSpread")) + " " +
                       (color.empty() ? "rgba(0,0,0,.35)" : color));
  } else if (!RoboXMLAttr(node, "shadow").empty() || !RoboXMLAttr(node, "depth").empty()) {
    RoboWebAddDecl(style, "box-shadow", RoboWebControlShadow(RoboXMLAttr(node, "shadow").empty() ? RoboXMLAttr(node, "depth") : RoboXMLAttr(node, "shadow")));
  }
  const std::string glow = RoboWebControlColor(library, RoboXMLAttr(node, "glow"));
  if (!glow.empty()) RoboWebAddDecl(style, "box-shadow", "0 0 18px " + glow);
  const std::string text = RoboWebControlColor(library, RoboXMLAttr(node, "textColor"));
  if (!text.empty()) RoboWebAddDecl(style, "color", text);
  RoboWebAddDecl(style, "--btn-icon", RoboWebControlColor(library, RoboXMLAttr(node, "iconColor")));
  RoboWebAddDecl(style, "--btn-icon-size", RoboWebControlLength(library, RoboXMLAttr(node, "iconSize")));
  const std::string family = RoboXMLAttr(node, "font");
  if (!family.empty()) RoboWebAddDecl(style, "font-family", family == "mono" ? "ui-monospace,Consolas,monospace" : family);
  RoboWebAddDecl(style, "font-size", RoboWebControlLength(library, RoboXMLAttr(node, "fontSize")));
  RoboWebAddDecl(style, "font-weight", RoboXMLAttr(node, "fontWeight"));
  const std::string fontStyle = RoboWebLower(RoboXMLAttr(node, "fontStyle"));
  if (fontStyle == "italic" || fontStyle == "normal") RoboWebAddDecl(style, "font-style", fontStyle);
  RoboWebAddDecl(style, "letter-spacing", RoboWebControlLength(library, RoboXMLAttr(node, "letterSpacing").empty() ? RoboXMLAttr(node, "characterSpacing") : RoboXMLAttr(node, "letterSpacing")));
  {
    std::string leading = RoboXMLAttr(node, "lineHeight");
    if (!leading.empty()) {
      if (RoboWebIsNumber(leading)) {
        leading = std::atof(leading.c_str()) > 8 ? leading + "px" : leading;
      } else {
        leading = RoboWebControlLength(library, leading);
      }
      RoboWebAddDecl(style, "line-height", leading);
    }
  }
  const std::string transform = RoboWebLower(RoboXMLAttr(node, "textTransform"));
  if (transform == "uppercase" || transform == "lowercase" || transform == "capitalize" || transform == "none") {
    RoboWebAddDecl(style, "text-transform", transform);
  }
  const std::string decoration = RoboWebLower(RoboXMLAttr(node, "decoration"));
  if (decoration == "underline" || decoration == "none" || decoration == "line-through") {
    RoboWebAddDecl(style, "text-decoration", decoration);
  }
  RoboWebControlPaintState(style, node, library, "hover");
  RoboWebControlPaintState(style, node, library, "active");
  RoboWebControlPaintState(style, node, library, "focus");
  RoboWebControlPaintState(style, node, library, "disabled");
  RoboWebControlPaintState(style, node, library, "selected");
  if (!RoboXMLAttr(node, "focusRing").empty()) {
    RoboWebAddDecl(style, "--btn-focus-ring", "2px solid " + RoboWebControlColor(library, RoboXMLAttr(node, "focusRing")));
  }
  RoboWebAddDecl(style, "--btn-focus-offset", RoboWebControlLength(library, RoboXMLAttr(node, "focusOffset")));
  RoboWebAddDecl(style, "--btn-badge", RoboWebControlColor(library, RoboXMLAttr(node, "badgeColor")));
  RoboWebAddDecl(style, "--btn-badge-text", RoboWebControlColor(library, RoboXMLAttr(node, "badgeText")));
  if (!RoboXMLAttr(node, "duration").empty()) {
    const std::string time = RoboWebIsNumber(RoboXMLAttr(node, "duration")) ? RoboXMLAttr(node, "duration") + "ms" : RoboXMLAttr(node, "duration");
    RoboWebAddDecl(style, "transition-duration", time);
  }
  RoboWebAddDecl(style, "transition-delay", RoboWebIsNumber(RoboXMLAttr(node, "delay")) ? RoboXMLAttr(node, "delay") + "ms" : RoboXMLAttr(node, "delay"));
  RoboWebAddDecl(style, "transition-timing-function", RoboXMLAttr(node, "easing"));
  RoboWebAddDecl(style, "mix-blend-mode", RoboWebLower(RoboXMLAttr(node, "blend")));
}

void RoboWebAppendControlCss(std::ostream& css) {
  css << ".robo-control .robo-button-content,.robo-control .robo-button-label{gap:var(--btn-gap,8px)}\n";
  css << ".robo-control[style*=\"--btn-icon\"] .robo-button-icon,.robo-control[style*=\"--btn-icon\"] .robo-icon-button-glyph{color:var(--btn-icon)}\n";
  css << ".robo-control[style*=\"--btn-icon-size\"] .robo-button-icon,.robo-control[style*=\"--btn-icon-size\"] .robo-icon-button-glyph{width:var(--btn-icon-size);height:var(--btn-icon-size)}\n";
  css << ".robo-control[data-icon=top] .robo-button-content,.robo-control[data-icon=bottom] .robo-button-content{flex-direction:column}\n";
  css << ".robo-control[data-icon=bottom] .robo-button-icon{order:1}\n";
  css << ".robo-button[data-size=xs]{height:28px;padding:0 10px;font-size:12px}\n";
  css << ".robo-button[data-size=xl]{height:56px;padding:0 28px;font-size:18px}\n";
  css << ".robo-button[data-size=2xl]{height:64px;padding:0 32px;font-size:20px}\n";
  css << ".robo-button[data-size=3xl]{height:72px;padding:0 36px;font-size:22px}\n";
  css << ".robo-icon-button[data-size=xs]{width:28px;height:28px}\n";
  css << ".robo-icon-button[data-size=lg]{width:48px;height:48px}\n";
  css << ".robo-icon-button[data-size=xl]{width:56px;height:56px}\n";
  css << ".robo-icon-button[data-size=2xl]{width:64px;height:64px}\n";
  css << ".robo-icon-button[data-size=3xl]{width:72px;height:72px}\n";
  css << ".robo-control[data-shape=square]{border-radius:0}\n";
  css << ".robo-control[data-shape=rounded],.robo-control[data-shape=soft]{border-radius:12px}\n";
  css << ".robo-control[data-shape=pill],.robo-control[data-shape=capsule]{border-radius:999px}\n";
  css << ".robo-control[data-shape=circle]{border-radius:50%;aspect-ratio:1;padding:0}\n";
  css << ".robo-control[data-shape=squircle]{border-radius:28%}\n";
  css << ".robo-control[data-shape=cut]{clip-path:polygon(12px 0,calc(100% - 12px) 0,100% 12px,100% calc(100% - 12px),calc(100% - 12px) 100%,12px 100%,0 calc(100% - 12px),0 12px)}\n";
  css << ".robo-control[data-shape=hex]{clip-path:polygon(25% 0,75% 0,100% 50%,75% 100%,25% 100%,0 50%)}\n";
  css << ".robo-control[data-shape=diamond]{clip-path:polygon(50% 0,100% 50%,50% 100%,0 50%)}\n";
  css << ".robo-control[data-texture=noise]{background-image:radial-gradient(rgba(255,255,255,.35) .6px,transparent .6px);background-size:3px 3px}\n";
  css << ".robo-control[data-accent=top]{box-shadow:inset 0 2px 0 var(--brand-primary)}\n";
  css << ".robo-control[data-accent=bottom]{box-shadow:inset 0 -2px 0 var(--brand-primary)}\n";
  css << ".robo-control[data-accent=left]{box-shadow:inset 3px 0 0 var(--brand-primary)}\n";
  css << ".robo-control[data-preset=glass]{background:rgba(255,255,255,.08);backdrop-filter:blur(16px);border:1px solid rgba(255,255,255,.16);color:var(--text-primary)}\n";
  css << ".robo-control[data-preset=flat]{box-shadow:none;background:var(--bg-tertiary);color:var(--text-primary)}\n";
  css << ".robo-control[data-preset=elevated],.robo-control[data-preset=floating]{box-shadow:0 10px 24px rgba(0,0,0,.32)}\n";
  css << ".robo-control[data-preset=neon]{background:#0c1220;color:#7dd3fc;border:1px solid #38bdf8;box-shadow:0 0 16px rgba(56,189,248,.45)}\n";
  css << ".robo-control[data-preset=soft],.robo-control[data-preset=neumorphic]{background:#2a2a2e;color:var(--text-primary);box-shadow:6px 6px 14px #17171a,-5px -5px 14px #3d3d44}\n";
  css << ".robo-control[data-preset=brutalist]{border-radius:0;border:3px solid var(--text-primary);box-shadow:4px 4px 0 var(--text-primary);background:var(--bg-secondary);color:var(--text-primary)}\n";
  css << ".robo-control[data-preset=gradient]{background:linear-gradient(135deg,#1d4ed8,#7c3aed);color:#fff;border:0}\n";
  css << ".robo-control[data-preset=success]{background:var(--success, #16a34a);color:#fff;border:0}\n";
  css << ".robo-control[data-preset=warning]{background:var(--warning, #d97706);color:#111;border:0}\n";
  css << ".robo-control[data-preset=info],.robo-control[data-preset=information]{background:var(--brand-primary);color:var(--text-on-brand)}\n";
  css << ".robo-control[data-preset=outline],.robo-control[data-preset=framed]{background:transparent;border:1px solid var(--border-strong);color:var(--text-primary)}\n";
  css << ".robo-control[data-preset=link]{background:transparent;border:0;color:var(--text-link);border-radius:0;padding-left:0;padding-right:0;text-decoration:underline}\n";
  css << ".robo-control[data-preset=tinted]{background:var(--brand-primary-subtle);color:var(--brand-primary);border:0}\n";
  css << ".robo-control[data-preset=fluent]{background:rgba(32,32,36,.72);backdrop-filter:blur(20px);border:1px solid rgba(255,255,255,.12);border-radius:4px;color:var(--text-primary)}\n";
  css << ".robo-control[data-preset=macos]{background:rgba(40,40,44,.8);backdrop-filter:blur(18px);border-radius:8px;border:1px solid rgba(255,255,255,.14);color:var(--text-primary)}\n";
  css << ".robo-control[data-preset=material]{box-shadow:0 2px 6px rgba(0,0,0,.3);border-radius:8px}\n";
  css << ".robo-control[data-preset=classic]{background:#d4d0c8;color:#111;border:2px solid;border-color:#fff #404040 #404040 #fff;border-radius:0}\n";
  css << ".robo-control[data-preset=cyberpunk]{background:#140018;color:#f0abfc;border:1px solid #e879f9;box-shadow:0 0 14px rgba(232,121,249,.5);border-radius:0}\n";
  css << ".robo-control[data-preset=luxury],.robo-control[data-preset=metallic]{background:linear-gradient(180deg,#3a342c,#1c1a17);color:#f6ecd8;border:1px solid #c6a15b}\n";
  css << ".robo-control[data-preset=chrome],.robo-control[data-preset=holographic]{background:linear-gradient(110deg,#e5e7eb,#6b7280,#fff,#a78bfa);color:#111}\n";
  css << ".robo-control[data-preset=retro]{background:#1b2838;color:#c7d5e0;border:2px solid #66c0f4;border-radius:0}\n";
  css << ".robo-control[data-preset=monochrome]{background:#111;color:#fff;border:1px solid #fff}\n";
  css << ".robo-control[data-preset=pastel]{background:#2a2633;color:#f3e9ff;border-radius:16px}\n";
  css << ".robo-control[data-preset=corporate],.robo-control[data-preset=editorial]{background:var(--bg-secondary);color:var(--text-primary);border:1px solid var(--border-default);border-radius:8px}\n";
  css << ".robo-control[data-preset=industrial]{background:#1e2124;color:#e5e7eb;border:1px solid #3a3f44;border-radius:2px}\n";
  css << ".robo-control[data-preset=transparent]{background:transparent;color:var(--text-primary);border:0;box-shadow:none}\n";
  css << ".robo-control[data-preset=circular]{border-radius:50%;aspect-ratio:1;padding:0}\n";
  css << ".robo-control[data-density=compact]{height:32px;padding-left:12px;padding-right:12px;font-size:13px}\n";
  css << ".robo-control[data-density=spacious]{height:52px;padding-left:28px;padding-right:28px}\n";
  css << ".robo-control[data-chrome=toolbar],.robo-control[data-chrome=titlebar]{height:32px;border-radius:6px;background:transparent;color:var(--text-primary)}\n";
  css << ".robo-control[data-chrome=sidebar]{justify-content:flex-start;width:100%;border-radius:8px;background:transparent;color:var(--text-primary)}\n";
  css << ".robo-control[data-chrome=fab],.robo-control[data-chrome=dock]{border-radius:50%;aspect-ratio:1;box-shadow:0 10px 24px rgba(0,0,0,.35)}\n";
  css << ".robo-control[data-theme=light]{background:#f4f6fc;color:#141418;border-color:#d2d2d7}\n";
  css << ".robo-control[data-theme=dark]{background:#141418;color:#f4f6fc}\n";
  css << ".robo-control[data-theme=neon]{background:#061018;color:#7dd3fc;box-shadow:0 0 16px rgba(125,211,252,.45)}\n";
  css << ".robo-control[data-theme=high-contrast]{background:#000;color:#fff;border:2px solid #fff}\n";
  css << ".robo-control[data-group=start]{border-top-right-radius:0;border-bottom-right-radius:0}\n";
  css << ".robo-control[data-group=middle]{border-radius:0}\n";
  css << ".robo-control[data-group=end]{border-top-left-radius:0;border-bottom-left-radius:0}\n";
  css << ".robo-control[style*=\"--btn-badge\"] .robo-icon-button-badge{background:var(--btn-badge);color:var(--btn-badge-text,#fff)}\n";
  const auto stateRule = [&](const char* name, const char* when, const char* suffix, const char* declaration) {
    const std::string variable = std::string("--btn-") + name + suffix;
    std::string rest = when;
    std::string selector;
    while (!rest.empty()) {
      const auto comma = rest.find(',');
      const std::string part = rest.substr(0, comma);
      if (!selector.empty()) selector += ",";
      selector += ".robo-control[style*=\"" + variable + "\"]" + part;
      if (comma == std::string::npos) break;
      rest.erase(0, comma + 1);
    }
    css << selector << "{" << declaration << ":var(" << variable << ") !important}\n";
  };
  const auto state = [&](const char* name, const char* when) {
    stateRule(name, when, "-bg", "background");
    stateRule(name, when, "-color", "color");
    stateRule(name, when, "-border", "border-color");
    stateRule(name, when, "-border-width", "border-width");
    stateRule(name, when, "-border-style", "border-style");
    stateRule(name, when, "-border-top", "border-top-color");
    stateRule(name, when, "-border-right", "border-right-color");
    stateRule(name, when, "-border-bottom", "border-bottom-color");
    stateRule(name, when, "-border-left", "border-left-color");
    stateRule(name, when, "-width-top", "border-top-width");
    stateRule(name, when, "-width-right", "border-right-width");
    stateRule(name, when, "-width-bottom", "border-bottom-width");
    stateRule(name, when, "-width-left", "border-left-width");
    stateRule(name, when, "-radius", "border-radius");
    stateRule(name, when, "-pad", "padding");
    stateRule(name, when, "-shadow", "box-shadow");
    stateRule(name, when, "-transform", "transform");
    const std::string iconVar = std::string("--btn-") + name + "-icon";
    std::string rest = when;
    std::string selector;
    while (!rest.empty()) {
      const auto comma = rest.find(',');
      const std::string part = rest.substr(0, comma);
      if (!selector.empty()) selector += ",";
      selector += ".robo-control[style*=\"" + iconVar + "\"]" + part + " .robo-button-icon,";
      selector += ".robo-control[style*=\"" + iconVar + "\"]" + part + " .robo-icon-button-glyph,";
      selector += ".robo-control[style*=\"" + iconVar + "\"]" + part + " .robo-reveal-glyph,";
      selector += ".robo-control[style*=\"" + iconVar + "\"]" + part + " .robo-segment-icon,";
      selector += ".robo-control[style*=\"" + iconVar + "\"]" + part + " .robo-theme-glyph,";
      selector += ".robo-control[style*=\"" + iconVar + "\"]" + part + " .robo-anchor-icon";
      if (comma == std::string::npos) break;
      rest.erase(0, comma + 1);
    }
    css << selector << "{color:var(" << iconVar << ") !important}\n";
  };
  state("hover", ":hover:not(:disabled),[data-preview=\"hover\"]");
  state("active", ":active:not(:disabled),[data-preview=\"active\"]");
  state("focus", ":focus-visible,[data-preview=\"focus\"]");
  state("disabled", ":disabled,[aria-disabled=\"true\"],[data-preview=\"disabled\"]");
  state("selected", "[data-selected=\"true\"],[aria-pressed=\"true\"],[data-preview=\"selected\"]");
  css << ".robo-control[style*=\"--btn-focus-ring\"]:focus-visible,.robo-control[style*=\"--btn-focus-ring\"][data-preview=\"focus\"]{outline:var(--btn-focus-ring) !important}\n";
  css << ".robo-control[style*=\"--btn-focus-offset\"]:focus-visible,.robo-control[style*=\"--btn-focus-offset\"][data-preview=\"focus\"]{outline-offset:var(--btn-focus-offset) !important}\n";
  css << ".robo-control[style*=\"--btn-disabled-bg\"]:disabled,.robo-control[style*=\"--btn-disabled-bg\"][data-preview=\"disabled\"]{opacity:1}\n";
  css << ".robo-control[data-selected=true]{background:var(--btn-selected-bg,var(--brand-primary-subtle));color:var(--btn-selected-color,var(--brand-primary))}\n";
  css << ".robo-control[data-motion=lift]:hover:not(:disabled){transform:translateY(-2px)}\n";
  css << ".robo-control[data-motion=pulse]:hover:not(:disabled){animation:robo-btn-pulse .7s ease infinite alternate}\n";
  css << ".robo-control[data-motion=shake]:hover:not(:disabled){animation:robo-btn-shake .35s ease}\n";
  css << ".robo-control[data-motion=shimmer]{background-size:200% 100%;animation:robo-btn-shift 2.4s linear infinite}\n";
  css << "@keyframes robo-btn-pulse{from{transform:scale(1)}to{transform:scale(1.04)}}\n";
  css << "@keyframes robo-btn-shake{25%{transform:translateX(-2px)}75%{transform:translateX(2px)}}\n";
  css << "@keyframes robo-btn-shift{from{background-position:0% 50%}to{background-position:200% 50%}}\n";
  css << "@keyframes robo-btn-in{from{opacity:0;transform:translateY(8px)}to{opacity:1;transform:none}}\n";
  css << ".robo-control[data-enter=fade],.robo-control[data-enter=slide],.robo-control[data-enter=fade-up]{animation:robo-btn-in var(--btn-in,360ms) ease both}\n";
  css << ".robo-control[data-enter=zoom]{animation:robo-btn-pulse 280ms ease both}\n";
}
