#include "SwitchWeb.hpp"

#include "../../RoboCompilerShared.hpp"

namespace fs = std::filesystem;

namespace {

bool RoboWebFlag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

std::string RoboWebSwitchVariant(const RoboXMLNode& node) {
  const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "primary"));
  if (variant == "subtle" || variant == "outline" || variant == "ghost" || variant == "danger") {
    return variant;
  }
  return "primary";
}

std::string RoboWebSwitchSize(const RoboXMLNode& node) {
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "md"));
  if (size == "small" || size == "sm") {
    return "sm";
  }
  if (size == "large" || size == "lg") {
    return "lg";
  }
  return "md";
}

std::string RoboWebSwitchSpinner(const std::string& size) {
  if (size == "sm") {
    return "<svg class=\"robo-switch-spinner\" width=\"12\" height=\"12\" viewBox=\"0 0 32 32\" fill=\"none\" "
           "aria-hidden=\"true\"><circle class=\"robo-switch-spinner-track\" cx=\"16\" cy=\"16\" r=\"14.35\" "
           "stroke-width=\"1.65\"></circle><circle class=\"robo-switch-spinner-tick\" cx=\"16\" cy=\"16\" r=\"14.35\" "
           "stroke-width=\"1.65\" stroke-linecap=\"round\" stroke-dasharray=\"25.25 64.91\"></circle></svg>";
  }
  return "<svg class=\"robo-switch-spinner\" width=\"14\" height=\"14\" viewBox=\"0 0 32 32\" fill=\"none\" "
         "aria-hidden=\"true\"><circle class=\"robo-switch-spinner-track\" cx=\"16\" cy=\"16\" r=\"14\" "
         "stroke-width=\"2\"></circle><circle class=\"robo-switch-spinner-tick\" cx=\"16\" cy=\"16\" r=\"14\" "
         "stroke-width=\"2\" stroke-linecap=\"round\" stroke-dasharray=\"24.63 63.33\"></circle></svg>";
}

void RoboWebAppendSwitchControl(std::string& html, const std::string& id, const std::string& label,
                                const std::string& variant, const std::string& size, bool checked, bool disabled,
                                bool loading) {
  html += "<button class=\"robo-switch\" type=\"button\" role=\"switch\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\" data-size=\"";
  html += RoboWebEscape(size);
  html += "\" data-checked=\"";
  html += checked ? "true" : "false";
  html += "\" aria-checked=\"";
  html += checked ? "true" : "false";
  html += "\"";
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  if (!label.empty()) {
    html += " aria-label=\"";
    html += RoboWebEscape(label);
    html += "\"";
  }
  if (loading) {
    html += " data-loading=\"true\" aria-busy=\"true\"";
  }
  if (disabled || loading) {
    html += " disabled";
  }
  html += "><span class=\"robo-switch-track\"><span class=\"robo-switch-thumb\">";
  if (loading) {
    html += RoboWebSwitchSpinner(size);
  }
  html += "</span></span></button>";
}

}  // namespace

void RoboWebEmitSwitch(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                       const fs::path& assetsRoot) {
  (void)assetsRoot;
  const bool loading = RoboWebFlag(RoboXMLAttr(node, "loading"));
  RoboWebAppendSwitchControl(html, RoboXMLAttr(node, "id"), RoboXMLAttr(node, "label"), RoboWebSwitchVariant(node),
                             RoboWebSwitchSize(node), RoboWebFlag(RoboXMLAttr(node, "checked")),
                             RoboWebFlag(RoboXMLAttr(node, "disabled")), loading);
  const std::string style = RoboWebStyle(node, library);
  if (!style.empty()) {
    const std::string marker = "class=\"robo-switch\"";
    const auto at = html.rfind(marker);
    if (at != std::string::npos) {
      html.insert(at + marker.size(), " style=\"" + RoboWebEscape(style) + "\"");
    }
  }
}

void RoboWebEmitSwitchGroup(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                            const std::string& source, const fs::path& assetsRoot, bool& warnedVideo,
                            RoboWebPackage* package, int depth) {
  const std::string id = RoboXMLAttr(node, "id");
  const std::string label = RoboXMLAttr(node, "label");
  const std::string style = RoboWebStyle(node, library);
  html += "<div class=\"robo-switch-group\" role=\"group\"";
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  if (!label.empty()) {
    html += " aria-label=\"";
    html += RoboWebEscape(label);
    html += "\"";
  }
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += ">";
  for (const RoboXMLNode& child : node.children) {
    RoboWebEmit(html, child, library, source, assetsRoot, false, warnedVideo, package, depth + 1);
  }
  html += "</div>";
}

void RoboWebEmitSwitchItem(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                           const fs::path& assetsRoot) {
  (void)assetsRoot;
  const bool loading = RoboWebFlag(RoboXMLAttr(node, "loading"));
  const bool disabled = RoboWebFlag(RoboXMLAttr(node, "disabled"));
  const std::string title = RoboXMLAttr(node, "title");
  const std::string description = RoboXMLAttr(node, "description");
  const std::string style = RoboWebStyle(node, library);
  html += "<div class=\"robo-switch-item\"";
  if (disabled) {
    html += " data-disabled=\"true\"";
  }
  if (loading) {
    html += " data-loading=\"true\"";
  }
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += "><span class=\"robo-switch-copy\"><span class=\"robo-switch-title\">";
  html += RoboWebEscape(title);
  html += "</span>";
  if (!description.empty()) {
    html += "<span class=\"robo-switch-description\">";
    html += RoboWebEscape(description);
    html += "</span>";
  }
  html += "</span>";
  RoboWebAppendSwitchControl(html, RoboXMLAttr(node, "id"), title, RoboWebSwitchVariant(node), RoboWebSwitchSize(node),
                             RoboWebFlag(RoboXMLAttr(node, "checked")), disabled, loading);
  html += "</div>";
}

void RoboWebAppendSwitchCss(std::ostream& css) {
  css << ".robo-switch{position:relative;box-sizing:border-box;display:inline-flex;flex:0 0 auto;align-items:center;"
         "justify-content:center;margin:0;padding:0;border:0;border-radius:980px;background:transparent;"
         "color:var(--text-primary);cursor:pointer;outline:none;appearance:none;-webkit-appearance:none;"
         "touch-action:manipulation;-webkit-tap-highlight-color:transparent}\n";
  css << ".robo-switch[data-size=\"sm\"]{width:44px;height:26px}\n";
  css << ".robo-switch[data-size=\"md\"]{width:51px;height:31px}\n";
  css << ".robo-switch[data-size=\"lg\"]{width:60px;height:36px}\n";
  css << ".robo-switch:focus-visible{outline:2px solid var(--brand-primary);outline-offset:2px}\n";
  css << ".robo-switch:disabled{cursor:not-allowed;opacity:0.55}\n";
  css << ".robo-switch[data-loading=\"true\"]{cursor:wait;opacity:1}\n";
  css << ".robo-switch-track{position:relative;display:block;width:100%;height:100%;overflow:hidden;"
         "border-radius:980px;background:var(--border-default);transition:background-color 200ms "
         "cubic-bezier(0.28,0.11,0.32,1),outline-color 200ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << "html[data-theme=\"dark\"] .robo-switch[data-variant=\"primary\"]:not([data-checked=\"true\"]) .robo-switch-track,"
         "html[data-theme=\"dark\"] .robo-switch[data-variant=\"danger\"]:not([data-checked=\"true\"]) .robo-switch-track"
         "{background:var(--bg-active)}\n";
  css << ".robo-switch[data-variant=\"subtle\"] .robo-switch-track{background:var(--bg-tertiary)}\n";
  css << ".robo-switch[data-variant=\"outline\"] .robo-switch-track{background:transparent;"
         "outline:1px solid var(--border-default);outline-offset:0}\n";
  css << ".robo-switch[data-variant=\"ghost\"] .robo-switch-track{background:transparent}\n";
  css << ".robo-switch[data-checked=\"true\"][data-variant=\"primary\"] .robo-switch-track,"
         ".robo-switch[data-checked=\"true\"][data-variant=\"subtle\"] .robo-switch-track,"
         ".robo-switch[data-checked=\"true\"][data-variant=\"outline\"] .robo-switch-track,"
         ".robo-switch[data-checked=\"true\"][data-variant=\"ghost\"] .robo-switch-track"
         "{background:var(--brand-primary)}\n";
  css << ".robo-switch[data-checked=\"true\"][data-variant=\"outline\"] .robo-switch-track"
         "{outline-color:var(--brand-primary)}\n";
  css << ".robo-switch[data-checked=\"true\"][data-variant=\"danger\"] .robo-switch-track{background:var(--danger)}\n";
  css << ".robo-switch-thumb{position:absolute;top:2px;left:2px;display:flex;align-items:center;justify-content:center;"
         "border-radius:980px;background:#fff;transform:translate3d(0,0,0) scaleX(1);transform-origin:left center;"
         "transition:transform 200ms cubic-bezier(0.28,0.11,0.32,1),background-color 200ms "
         "cubic-bezier(0.28,0.11,0.32,1),outline-color 200ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << ".robo-switch[data-size=\"sm\"] .robo-switch-thumb{width:22px;height:22px}\n";
  css << ".robo-switch[data-size=\"md\"] .robo-switch-thumb{width:27px;height:27px}\n";
  css << ".robo-switch[data-size=\"lg\"] .robo-switch-thumb{width:32px;height:32px}\n";
  css << ".robo-switch[data-checked=\"true\"] .robo-switch-thumb{transform-origin:right center}\n";
  css << ".robo-switch[data-size=\"sm\"][data-checked=\"true\"] .robo-switch-thumb{transform:translate3d(18px,0,0) scaleX(1)}\n";
  css << ".robo-switch[data-size=\"md\"][data-checked=\"true\"] .robo-switch-thumb{transform:translate3d(20px,0,0) scaleX(1)}\n";
  css << ".robo-switch[data-size=\"lg\"][data-checked=\"true\"] .robo-switch-thumb{transform:translate3d(24px,0,0) scaleX(1)}\n";
  css << ".robo-switch[data-pressed=\"true\"] .robo-switch-thumb{transform:translate3d(0,0,0) scaleX(1.18)}\n";
  css << ".robo-switch[data-size=\"sm\"][data-checked=\"true\"][data-pressed=\"true\"] .robo-switch-thumb"
         "{transform:translate3d(18px,0,0) scaleX(1.18)}\n";
  css << ".robo-switch[data-size=\"md\"][data-checked=\"true\"][data-pressed=\"true\"] .robo-switch-thumb"
         "{transform:translate3d(20px,0,0) scaleX(1.18)}\n";
  css << ".robo-switch[data-size=\"lg\"][data-checked=\"true\"][data-pressed=\"true\"] .robo-switch-thumb"
         "{transform:translate3d(24px,0,0) scaleX(1.18)}\n";
  css << ".robo-switch[data-variant=\"ghost\"]:not([data-checked=\"true\"]) .robo-switch-thumb"
         "{background:var(--bg-elevated);outline:1px solid var(--border-default)}\n";
  css << ".robo-switch-spinner{display:block;transform-origin:center;animation:robo-switch-spin 1s linear infinite}\n";
  css << ".robo-switch-spinner-track{stroke:color-mix(in srgb,var(--text-tertiary) 35%,transparent)}\n";
  css << ".robo-switch-spinner-tick{stroke:var(--brand-primary)}\n";
  css << "@keyframes robo-switch-spin{to{transform:rotate(360deg)}}\n";
  css << "@media (prefers-reduced-motion:reduce){.robo-switch-track,.robo-switch-thumb{transition:none}"
         ".robo-switch-spinner{animation:none}}\n";
  css << ".robo-switch-group{display:flex;width:100%;flex-direction:column}\n";
  css << ".robo-switch-group>*+*{border-top:1px solid var(--border-default)}\n";
  css << ".robo-switch-item{display:flex;min-height:56px;align-items:center;justify-content:space-between;gap:16px;"
         "padding:12px 0;cursor:pointer}\n";
  css << ".robo-switch-item[data-disabled=\"true\"],.robo-switch-item[data-loading=\"true\"]{cursor:default}\n";
  css << ".robo-switch-copy{display:flex;min-width:0;flex-direction:column;gap:2px}\n";
  css << ".robo-switch-title{font-size:17px;line-height:1.35;color:var(--text-primary)}\n";
  css << ".robo-switch-description{font-size:12px;line-height:1.33;color:var(--text-tertiary)}\n";
}

std::string RoboWebSwitchScript() {
  return R"JS(
  function switchOn(node) {
    return node.getAttribute("aria-checked") === "true";
  }
  function switchSet(node, on) {
    var value = on ? "true" : "false";
    node.setAttribute("aria-checked", value);
    node.setAttribute("data-checked", value);
  }
  function switchRelease(node) {
    if (node) node.removeAttribute("data-pressed");
  }
  document.addEventListener("keydown", function (ev) {
    if (ev.key !== "Enter" && ev.key !== " ") return;
    var node = ev.target && ev.target.closest ? ev.target.closest(".robo-switch") : null;
    if (!node || node.disabled) return;
    ev.preventDefault();
    if (ev.repeat) return;
    node.setAttribute("data-pressed", "true");
    node.setAttribute("data-skip-click", "1");
    switchSet(node, !switchOn(node));
  });
  document.addEventListener("keyup", function (ev) {
    if (ev.key !== "Enter" && ev.key !== " ") return;
    var node = ev.target && ev.target.closest ? ev.target.closest(".robo-switch") : null;
    if (node) node.removeAttribute("data-pressed");
  });
  document.addEventListener("pointerdown", function (ev) {
    var node = ev.target.closest ? ev.target.closest(".robo-switch") : null;
    if (!node || node.disabled || ev.button !== 0) return;
    node.setAttribute("data-skip-click", "1");
    node.setAttribute("data-pressed", "true");
    switchSet(node, !switchOn(node));
  });
  function switchReleaseAll() {
    var nodes = document.querySelectorAll(".robo-switch[data-pressed]");
    for (var i = 0; i < nodes.length; i++) nodes[i].removeAttribute("data-pressed");
  }
  document.addEventListener("pointerup", switchReleaseAll);
  document.addEventListener("pointercancel", switchReleaseAll);
  document.addEventListener("pointerout", function (ev) {
    var node = ev.target.closest ? ev.target.closest(".robo-switch") : null;
    if (!node) return;
    if (ev.relatedTarget && node.contains(ev.relatedTarget)) return;
    switchRelease(node);
  });
  document.addEventListener("click", function (ev) {
    var node = ev.target.closest ? ev.target.closest(".robo-switch") : null;
    if (node && !node.disabled) {
      if (node.getAttribute("data-skip-click") === "1") node.removeAttribute("data-skip-click");
      else switchSet(node, !switchOn(node));
      return;
    }
    var row = ev.target.closest ? ev.target.closest(".robo-switch-item") : null;
    if (!row || row.getAttribute("data-disabled") === "true" || row.getAttribute("data-loading") === "true") return;
    var control = row.querySelector(".robo-switch");
    if (!control || control.disabled) return;
    control.click();
  });
)JS";
}

const char* RoboWebSwitchClass() { return "robo-switch"; }
