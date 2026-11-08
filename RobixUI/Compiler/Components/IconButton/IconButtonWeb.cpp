#include "IconButtonWeb.hpp"

#include "../Button/ButtonWeb.hpp"
#include "../../RoboCompilerShared.hpp"
#include "../../Motion.hpp"
#include "../../ScreenSize.hpp"

namespace fs = std::filesystem;

namespace {

bool RoboWebFlag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes";
}

std::string RoboWebIconButtonSize(const RoboXMLNode& node) {
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "md"));
  if (size == "xs" || size == "extra-small") return "xs";
  if (size == "sm" || size == "small") return "sm";
  if (size == "lg" || size == "large") return "lg";
  if (size == "xl" || size == "extra-large") return "xl";
  if (size == "2xl") return "2xl";
  if (size == "3xl") return "3xl";
  return "md";
}

std::string RoboWebIconButtonVariant(const RoboXMLNode& node) {
  const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "ghost"));
  if (variant == "subtle" || variant == "outline" || variant == "primary" || variant == "danger") {
    return variant;
  }
  return "ghost";
}

std::string RoboWebIconButtonStatus(const RoboXMLNode& node) {
  std::string status = RoboWebLower(RoboXMLAttr(node, "status"));
  if (status.empty() && RoboWebFlag(RoboXMLAttr(node, "loading"))) {
    status = "loading";
  }
  if (status != "loading" && status != "success" && status != "error") {
    return {};
  }
  return status;
}

std::string RoboWebIconButtonBadge(const std::string& badge) {
  if (badge.empty() || badge == "0") {
    return {};
  }
  bool digits = !badge.empty();
  for (char ch : badge) {
    if (ch < '0' || ch > '9') {
      digits = false;
      break;
    }
  }
  if (!digits) {
    return badge;
  }
  int value = 0;
  for (char ch : badge) {
    value = value * 10 + (ch - '0');
  }
  if (value == 0) {
    return {};
  }
  if (value > 9) {
    return "9+";
  }
  return std::to_string(value);
}

std::string RoboWebIconButtonGlyph(const std::string& svg, int size) {
  if (svg.empty()) {
    return {};
  }
  return "<span class=\"robo-icon-button-glyph\" style=\"width:" + std::to_string(size) + "px;height:" +
         std::to_string(size) + "px\">" + svg + "</span>";
}

std::string RoboWebIconButtonSpinner() {
  return "<svg class=\"robo-icon-button-spinner\" viewBox=\"0 0 32 32\" fill=\"none\" aria-hidden=\"true\">"
         "<circle class=\"robo-icon-button-spinner-track\" cx=\"16\" cy=\"16\" r=\"14\" stroke-width=\"2\"></circle>"
         "<circle class=\"robo-icon-button-spinner-tick\" cx=\"16\" cy=\"16\" r=\"14\" stroke-width=\"2\" "
         "stroke-linecap=\"round\" stroke-dasharray=\"24.63 63.33\"></circle></svg>";
}

std::string RoboWebIconButtonMark(const char* kind) {
  const char* path = kind[0] == 's' ? "M5.5 12.5L10 17L18.5 7.5" : "M6 6L18 18M18 6L6 18";
  std::string svg = "<svg class=\"robo-icon-button-mark\" viewBox=\"0 0 24 24\" fill=\"none\" aria-hidden=\"true\">";
  svg += "<path d=\"";
  svg += path;
  svg += "\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\"";
  if (kind[0] == 's') {
    svg += " stroke-linejoin=\"round\"";
  }
  svg += "></path></svg>";
  return svg;
}

}  // namespace

void RoboWebEmitIconButton(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                           const fs::path& assetsRoot) {
  const std::string id = RoboXMLAttr(node, "id");
  const std::string label = RoboXMLAttr(node, "label");
  const std::string title = RoboXMLAttr(node, "title", label.c_str());
  const std::string variant = RoboWebIconButtonVariant(node);
  const std::string size = RoboWebIconButtonSize(node);
  const std::string status = RoboWebIconButtonStatus(node);
  const std::string action = RoboXMLAttr(node, "action");
  const std::string successMs = RoboXMLAttr(node, "successMs", "1600");
  const std::string badge = RoboWebIconButtonBadge(RoboXMLAttr(node, "badge"));
  const bool disabled = RoboWebFlag(RoboXMLAttr(node, "disabled")) || status == "loading";
  const int glyph = size == "sm" ? 16 : 18;
  std::string icon = RoboWebSvg(assetsRoot, RoboXMLAttr(node, "icon", RoboXMLAttr(node, "name").c_str()));
  for (const RoboXMLNode& child : node.children) {
    const std::string childName = RoboWebLower(child.name);
    if (childName != "icon" && childName != "svg") {
      continue;
    }
    const std::string markup = RoboWebIconMarkup(assetsRoot, child);
    if (!markup.empty()) {
      icon = markup;
      break;
    }
  }
  std::string style = RoboWebStyle(node, library);
  RoboWebAppendControlStyle(style, node, library);
  html += "<button class=\"robo-icon-button robo-control\" type=\"button\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\" data-size=\"";
  html += RoboWebEscape(size);
  html += "\"";
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  if (!status.empty()) {
    html += " data-status=\"";
    html += RoboWebEscape(status);
    html += "\"";
  }
  if (!action.empty() && status.empty()) {
    html += " data-action=\"";
    html += RoboWebEscape(action);
    html += "\" data-success-ms=\"";
    html += RoboWebEscape(successMs);
    html += "\"";
  }
  html += " title=\"";
  html += RoboWebEscape(title);
  html += "\" aria-label=\"";
  html += RoboWebEscape(label);
  html += "\"";
  if (status == "loading") {
    html += " aria-busy=\"true\"";
  }
  if (disabled) {
    html += " disabled";
  }
  RoboWebControlAttrs(html, node);
  RoboWebAppendMotion(style, node);
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += RoboWebScreenAttrs(node, library, "inline-flex");
  html += RoboWebMotionAttrs(node);
  html += ">";
  html += RoboWebIconButtonGlyph(icon, glyph);
  html += "<span class=\"robo-icon-button-overlay\" data-state=\"loading\">";
  html += RoboWebIconButtonSpinner();
  html += "</span><span class=\"robo-icon-button-overlay\" data-state=\"success\">";
  html += RoboWebIconButtonMark("success");
  html += "</span><span class=\"robo-icon-button-overlay\" data-state=\"error\">";
  html += RoboWebIconButtonMark("error");
  html += "</span>";
  if (!badge.empty()) {
    html += "<span class=\"robo-icon-button-badge\">";
    html += RoboWebEscape(badge);
    html += "</span>";
  }
  html += "</button>";
}

void RoboWebAppendIconButtonCss(std::ostream& css) {
  css << ".robo-icon-button{position:relative;box-sizing:border-box;display:inline-flex;flex:0 0 auto;"
         "align-items:center;justify-content:center;width:44px;height:44px;margin:0;padding:0;border:0;"
         "border-radius:980px;background:transparent;color:var(--text-primary);cursor:pointer;"
         "outline:2px solid transparent;outline-offset:2px;box-shadow:none;"
         "transition:background-color 150ms cubic-bezier(0.28,0.11,0.32,1),"
         "border-color 150ms cubic-bezier(0.28,0.11,0.32,1),color 150ms cubic-bezier(0.28,0.11,0.32,1),"
         "outline-color 150ms cubic-bezier(0.28,0.11,0.32,1),box-shadow 150ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << ".robo-icon-button[data-size=\"sm\"]{width:32px;height:32px}\n";
  css << ".robo-icon-button-glyph{display:inline-flex;align-items:center;justify-content:center}\n";
  css << ".robo-icon-button-glyph svg{width:100%;height:100%;display:block}\n";
  css << ".robo-icon-button-overlay{position:absolute;inset:0;display:none;align-items:center;"
         "justify-content:center}\n";
  css << ".robo-icon-button[data-status=\"loading\"] .robo-icon-button-overlay[data-state=\"loading\"],"
         ".robo-icon-button:not([data-status])[data-phase=\"loading\"] "
         ".robo-icon-button-overlay[data-state=\"loading\"],"
         ".robo-icon-button[data-status=\"success\"] .robo-icon-button-overlay[data-state=\"success\"],"
         ".robo-icon-button:not([data-status])[data-phase=\"success\"] "
         ".robo-icon-button-overlay[data-state=\"success\"],"
         ".robo-icon-button[data-status=\"error\"] .robo-icon-button-overlay[data-state=\"error\"],"
         ".robo-icon-button:not([data-status])[data-phase=\"error\"] "
         ".robo-icon-button-overlay[data-state=\"error\"]{display:inline-flex}\n";
  css << ".robo-icon-button[data-status=\"loading\"] .robo-icon-button-glyph,"
         ".robo-icon-button[data-status=\"success\"] .robo-icon-button-glyph,"
         ".robo-icon-button[data-status=\"error\"] .robo-icon-button-glyph,"
         ".robo-icon-button:not([data-status])[data-phase=\"loading\"] .robo-icon-button-glyph,"
         ".robo-icon-button:not([data-status])[data-phase=\"success\"] .robo-icon-button-glyph,"
         ".robo-icon-button:not([data-status])[data-phase=\"error\"] .robo-icon-button-glyph{visibility:hidden}\n";
  css << ".robo-icon-button-spinner{display:block;width:16px;height:16px;transform-origin:center;"
         "animation:robo-icon-button-spin 1s linear infinite}\n";
  css << ".robo-icon-button[data-size=\"sm\"] .robo-icon-button-spinner{width:14px;height:14px}\n";
  css << ".robo-icon-button-mark{display:block;width:18px;height:18px}\n";
  css << ".robo-icon-button[data-size=\"sm\"] .robo-icon-button-mark{width:16px;height:16px}\n";
  css << ".robo-icon-button-overlay[data-state=\"success\"]{color:var(--success)}\n";
  css << ".robo-icon-button-overlay[data-state=\"error\"]{color:var(--danger)}\n";
  css << ".robo-icon-button[data-variant=\"primary\"] .robo-icon-button-overlay[data-state=\"success\"],"
         ".robo-icon-button[data-variant=\"danger\"] .robo-icon-button-overlay[data-state=\"success\"],"
         ".robo-icon-button[data-variant=\"primary\"] .robo-icon-button-overlay[data-state=\"error\"],"
         ".robo-icon-button[data-variant=\"danger\"] .robo-icon-button-overlay[data-state=\"error\"]"
         "{color:currentColor}\n";
  css << "@keyframes robo-icon-button-spin{to{transform:rotate(360deg)}}\n";
  css << "@media (prefers-reduced-motion:reduce){.robo-icon-button-spinner{animation:none}}\n";
  css << ".robo-icon-button-spinner-track{stroke:color-mix(in srgb,var(--text-tertiary) 35%,transparent)}\n";
  css << ".robo-icon-button-spinner-tick{stroke:var(--brand-primary)}\n";
  css << ".robo-icon-button[data-variant=\"primary\"] .robo-icon-button-spinner-track,"
         ".robo-icon-button[data-variant=\"danger\"] .robo-icon-button-spinner-track"
         "{stroke:color-mix(in srgb,var(--text-on-brand) 35%,transparent)}\n";
  css << ".robo-icon-button[data-variant=\"primary\"] .robo-icon-button-spinner-tick,"
         ".robo-icon-button[data-variant=\"danger\"] .robo-icon-button-spinner-tick{stroke:var(--text-on-brand)}\n";
  css << ".robo-icon-button[data-variant=\"ghost\"]{border:0;background:transparent;color:var(--text-primary)}\n";
  css << ".robo-icon-button[data-variant=\"subtle\"]{border:0;background:var(--bg-secondary);"
         "color:var(--text-primary)}\n";
  css << ".robo-icon-button[data-variant=\"outline\"]{border:1px solid var(--border-default);background:transparent;"
         "color:var(--text-primary)}\n";
  css << ".robo-icon-button[data-variant=\"primary\"]{border:0;background:var(--brand-primary);"
         "color:var(--text-on-brand)}\n";
  css << ".robo-icon-button[data-variant=\"danger\"]{border:0;background:var(--danger);color:var(--text-on-brand)}\n";
  css << ".robo-icon-button[data-variant=\"ghost\"]:hover,.robo-icon-button[data-variant=\"subtle\"]:hover,"
         ".robo-icon-button[data-variant=\"outline\"]:hover,"
         ".robo-icon-button[data-variant=\"ghost\"]:focus-visible,"
         ".robo-icon-button[data-variant=\"subtle\"]:focus-visible{background:var(--bg-hover)}\n";
  css << ".robo-icon-button[data-variant=\"outline\"]:hover{border-color:var(--border-strong)}\n";
  css << ".robo-icon-button[data-variant=\"outline\"]:focus-visible{border-color:var(--border-focus)}\n";
  css << ".robo-icon-button[data-variant=\"primary\"]:hover,"
         ".robo-icon-button[data-variant=\"primary\"]:focus-visible{background:var(--brand-primary-hover)}\n";
  css << ".robo-icon-button[data-variant=\"danger\"]:hover,"
         ".robo-icon-button[data-variant=\"danger\"]:focus-visible{background:var(--danger-hover)}\n";
  css << ".robo-icon-button[data-variant=\"ghost\"]:active,.robo-icon-button[data-variant=\"subtle\"]:active,"
         ".robo-icon-button[data-variant=\"outline\"]:active{background:var(--bg-active)}\n";
  css << ".robo-icon-button[data-variant=\"outline\"]:active{border-color:var(--text-primary)}\n";
  css << ".robo-icon-button[data-variant=\"primary\"]:active{background:var(--brand-primary-active)}\n";
  css << ".robo-icon-button[data-variant=\"danger\"]:active"
         "{background:color-mix(in srgb,var(--danger) 78%,black)}\n";
  css << ".robo-icon-button:focus-visible{outline-color:color-mix(in srgb,var(--brand-primary) 48%,transparent);"
         "box-shadow:0 0 5px 0 color-mix(in srgb,var(--brand-primary) 36%,transparent)}\n";
  css << ".robo-icon-button:disabled{pointer-events:none;opacity:0.55}\n";
  css << ".robo-icon-button-badge{position:absolute;top:2px;right:2px;display:inline-flex;align-items:center;"
         "justify-content:center;box-sizing:border-box;height:18px;min-width:18px;padding:0 4px;"
         "border:1px solid transparent;border-radius:980px;background:var(--danger);color:var(--text-on-brand);"
         "font:500 10px/1 var(--font-sans,system-ui,sans-serif);font-variant-numeric:tabular-nums;"
         "pointer-events:none}\n";
}

std::string RoboWebIconButtonScript() {
  return R"JS(
  document.addEventListener("click", function (ev) {
    var button = ev.target.closest ? ev.target.closest(".robo-icon-button") : null;
    if (!button || button.disabled || button.getAttribute("data-status")) return;
    var action = button.getAttribute("data-action");
    if (action == null || action === "") return;
    if (ev.defaultPrevented) return;
    var phase = button.getAttribute("data-phase") || "idle";
    if (phase !== "idle") return;
    var wait = parseInt(action, 10);
    if (!(wait > 0)) wait = 0;
    var hold = parseInt(button.getAttribute("data-success-ms") || "1600", 10);
    if (!(hold > 0)) hold = 1600;
    var fail = action === "error";
    button.setAttribute("data-phase", "loading");
    button.disabled = true;
    button.setAttribute("aria-busy", "true");
    window.setTimeout(function () {
      button.removeAttribute("aria-busy");
      button.setAttribute("data-phase", fail ? "error" : "success");
      window.setTimeout(function () {
        button.setAttribute("data-phase", "idle");
        button.disabled = false;
      }, hold);
    }, wait);
  });
)JS";
}

const char* RoboWebIconButtonClass() { return "robo-icon-button"; }
