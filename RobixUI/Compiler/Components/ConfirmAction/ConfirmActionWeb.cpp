#include "ConfirmActionWeb.hpp"

#include "../Button/ButtonWeb.hpp"
#include "../../RoboCompilerShared.hpp"

namespace fs = std::filesystem;

namespace {

bool RoboWebFlag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

std::string RoboWebConfirmVariant(const RoboXMLNode& node) {
  const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "text"));
  if (variant == "ghost" || variant == "subtle" || variant == "outline" || variant == "solid") {
    return variant;
  }
  return "text";
}

std::string RoboWebConfirmTone(const RoboXMLNode& node) {
  const std::string tone = RoboWebLower(RoboXMLAttr(node, "tone", "danger"));
  if (tone == "brand" || tone == "neutral") {
    return tone;
  }
  return "danger";
}

std::string RoboWebConfirmSize(const RoboXMLNode& node) {
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "sm"));
  if (size == "medium" || size == "md") {
    return "md";
  }
  if (size == "large" || size == "lg") {
    return "lg";
  }
  return "sm";
}

std::string RoboWebConfirmSpinner() {
  return "<svg class=\"robo-confirm-spinner\" width=\"16\" height=\"16\" viewBox=\"0 0 32 32\" fill=\"none\" "
         "aria-hidden=\"true\"><circle class=\"robo-confirm-spinner-track\" cx=\"16\" cy=\"16\" r=\"14\" "
         "stroke-width=\"2\"></circle><circle class=\"robo-confirm-spinner-tick\" cx=\"16\" cy=\"16\" r=\"14\" "
         "stroke-width=\"2\" stroke-linecap=\"round\" stroke-dasharray=\"24.63 63.33\"></circle></svg>";
}

}  // namespace

void RoboWebEmitConfirmAction(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                              const fs::path& assetsRoot) {
  (void)assetsRoot;
  const std::string variant = RoboWebConfirmVariant(node);
  const std::string tone = RoboWebConfirmTone(node);
  const std::string size = RoboWebConfirmSize(node);
  const bool loading = RoboWebFlag(RoboXMLAttr(node, "loading"));
  const bool disabled = RoboWebFlag(RoboXMLAttr(node, "disabled")) || loading;
  std::string label = RoboXMLAttr(node, "label");
  if (label.empty()) {
    label = RoboCompilerTrim(node.text);
  }
  if (label.empty()) {
    label = "Remove";
  }
  std::string confirmLabel = RoboXMLAttr(node, "confirmLabel");
  if (confirmLabel.empty()) {
    confirmLabel = "Confirm?";
  }
  std::string loadingText = RoboXMLAttr(node, "loadingText");
  if (loadingText.empty()) {
    loadingText = label == "Sign Out" ? "Signing out\u2026" : "Removing\u2026";
  }
  const std::string resetMs = RoboXMLAttr(node, "resetMs", "3000");
  const std::string id = RoboXMLAttr(node, "id");
  std::string style = RoboWebStyle(node, library);
  RoboWebAppendControlStyle(style, node, library);
  html += "<button class=\"robo-confirm robo-control\" type=\"button\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\" data-tone=\"";
  html += RoboWebEscape(tone);
  html += "\" data-size=\"";
  html += RoboWebEscape(size);
  html += "\" data-label=\"";
  html += RoboWebEscape(label);
  html += "\" data-confirm=\"";
  html += RoboWebEscape(confirmLabel);
  html += "\" data-reset-ms=\"";
  html += RoboWebEscape(resetMs);
  html += "\" aria-pressed=\"false\"";
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  if (loading) {
    html += " data-loading=\"true\" aria-busy=\"true\"";
  }
  if (disabled) {
    html += " disabled";
  }
  RoboWebControlAttrs(html, node);
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += "><span class=\"robo-confirm-label\">";
  if (loading) {
    html += RoboWebConfirmSpinner();
  }
  html += "<span class=\"robo-confirm-text\">";
  html += RoboWebEscape(loading ? loadingText : label);
  html += "</span></span></button>";
}

void RoboWebAppendConfirmActionCss(std::ostream& css) {
  css << ".robo-confirm{box-sizing:border-box;display:inline-flex;width:auto;align-items:center;justify-content:center;"
         "margin:0;border:0;border-radius:980px;background:transparent;font:500 14px/1.35 var(--font-sans,system-ui,sans-serif);"
         "white-space:nowrap;cursor:pointer;outline:none;transition:background-color 150ms linear,border-color 150ms linear,"
         "color 150ms linear}\n";
  css << ".robo-confirm[data-size=\"sm\"]{height:32px;padding:0 14px}\n";
  css << ".robo-confirm[data-size=\"md\"]{height:44px;padding:0 22px;font-size:17px}\n";
  css << ".robo-confirm[data-size=\"lg\"]{height:50px;padding:0 24px;font-size:17px}\n";
  css << ".robo-confirm-label,.robo-confirm-text{display:inline-flex;align-items:center;justify-content:center;gap:8px;"
         "white-space:nowrap}\n";
  css << ".robo-confirm-spinner{display:block;flex:0 0 16px;width:16px;height:16px;transform-origin:center;"
         "animation:robo-confirm-spin 1s linear infinite}\n";
  css << "@keyframes robo-confirm-spin{to{transform:rotate(360deg)}}\n";
  css << "@media (prefers-reduced-motion:reduce){.robo-confirm,.robo-confirm-spinner{transition:none;animation:none}}\n";
  css << ".robo-confirm-spinner-track{stroke:color-mix(in srgb,var(--text-tertiary) 35%,transparent)}\n";
  css << ".robo-confirm-spinner-tick{stroke:var(--brand-primary)}\n";
  css << ".robo-confirm[data-tone=\"danger\"][data-variant=\"solid\"] .robo-confirm-spinner-track,"
         ".robo-confirm[data-tone=\"brand\"][data-variant=\"solid\"] .robo-confirm-spinner-track,"
         ".robo-confirm[data-tone=\"danger\"][data-confirming=\"true\"] .robo-confirm-spinner-track,"
         ".robo-confirm[data-tone=\"brand\"][data-confirming=\"true\"] .robo-confirm-spinner-track"
         "{stroke:color-mix(in srgb,var(--text-on-brand) 35%,transparent)}\n";
  css << ".robo-confirm[data-tone=\"danger\"][data-variant=\"solid\"] .robo-confirm-spinner-tick,"
         ".robo-confirm[data-tone=\"brand\"][data-variant=\"solid\"] .robo-confirm-spinner-tick,"
         ".robo-confirm[data-tone=\"danger\"][data-confirming=\"true\"] .robo-confirm-spinner-tick,"
         ".robo-confirm[data-tone=\"brand\"][data-confirming=\"true\"] .robo-confirm-spinner-tick"
         "{stroke:var(--text-on-brand)}\n";
  css << ".robo-confirm[data-variant=\"ghost\"][data-tone=\"danger\"]{color:var(--danger)}\n";
  css << ".robo-confirm[data-variant=\"ghost\"][data-tone=\"danger\"]:hover:not(:disabled){background:var(--danger-subtle)}\n";
  css << ".robo-confirm[data-variant=\"ghost\"][data-tone=\"brand\"]{color:var(--brand-primary)}\n";
  css << ".robo-confirm[data-variant=\"ghost\"][data-tone=\"brand\"]:hover:not(:disabled)"
         "{background:var(--brand-primary-subtle)}\n";
  css << ".robo-confirm[data-variant=\"ghost\"][data-tone=\"neutral\"]{color:var(--text-secondary)}\n";
  css << ".robo-confirm[data-variant=\"ghost\"][data-tone=\"neutral\"]:hover:not(:disabled){background:var(--bg-hover);"
         "color:var(--text-primary)}\n";
  css << ".robo-confirm[data-variant=\"subtle\"][data-tone=\"danger\"]{background:var(--danger-subtle);color:var(--danger)}\n";
  css << ".robo-confirm[data-variant=\"subtle\"][data-tone=\"danger\"]:hover:not(:disabled)"
         "{background:color-mix(in srgb,var(--danger) 15%,transparent)}\n";
  css << ".robo-confirm[data-variant=\"subtle\"][data-tone=\"brand\"]{background:var(--brand-primary-subtle);"
         "color:var(--brand-primary)}\n";
  css << ".robo-confirm[data-variant=\"subtle\"][data-tone=\"brand\"]:hover:not(:disabled)"
         "{background:color-mix(in srgb,var(--brand-primary) 15%,transparent)}\n";
  css << ".robo-confirm[data-variant=\"subtle\"][data-tone=\"neutral\"]{background:var(--bg-tertiary);"
         "color:var(--text-primary)}\n";
  css << ".robo-confirm[data-variant=\"subtle\"][data-tone=\"neutral\"]:hover:not(:disabled){background:var(--bg-hover)}\n";
  css << ".robo-confirm[data-variant=\"outline\"][data-tone=\"danger\"]{border:1px solid "
         "color-mix(in srgb,var(--danger) 35%,transparent);color:var(--danger)}\n";
  css << ".robo-confirm[data-variant=\"outline\"][data-tone=\"danger\"]:hover:not(:disabled)"
         "{background:var(--danger-subtle)}\n";
  css << ".robo-confirm[data-variant=\"outline\"][data-tone=\"brand\"]{border:1px solid "
         "color-mix(in srgb,var(--brand-primary) 35%,transparent);color:var(--brand-primary)}\n";
  css << ".robo-confirm[data-variant=\"outline\"][data-tone=\"brand\"]:hover:not(:disabled)"
         "{background:var(--brand-primary-subtle)}\n";
  css << ".robo-confirm[data-variant=\"outline\"][data-tone=\"neutral\"]{border:1px solid var(--border-default);"
         "color:var(--text-primary)}\n";
  css << ".robo-confirm[data-variant=\"outline\"][data-tone=\"neutral\"]:hover:not(:disabled){background:var(--bg-hover)}\n";
  css << ".robo-confirm[data-variant=\"solid\"][data-tone=\"danger\"],.robo-confirm[data-confirming=\"true\"][data-tone=\"danger\"]"
         "{border:0;background:var(--danger);color:var(--text-on-brand)}\n";
  css << ".robo-confirm[data-variant=\"solid\"][data-tone=\"danger\"]:hover:not(:disabled),"
         ".robo-confirm[data-confirming=\"true\"][data-tone=\"danger\"]:hover:not(:disabled){background:var(--danger-hover)}\n";
  css << ".robo-confirm[data-variant=\"solid\"][data-tone=\"brand\"],.robo-confirm[data-confirming=\"true\"][data-tone=\"brand\"]"
         "{border:0;background:var(--brand-primary);color:var(--text-on-brand)}\n";
  css << ".robo-confirm[data-variant=\"solid\"][data-tone=\"brand\"]:hover:not(:disabled),"
         ".robo-confirm[data-confirming=\"true\"][data-tone=\"brand\"]:hover:not(:disabled)"
         "{background:var(--brand-primary-hover)}\n";
  css << ".robo-confirm[data-variant=\"solid\"][data-tone=\"neutral\"],.robo-confirm[data-confirming=\"true\"][data-tone=\"neutral\"]"
         "{border:1px solid var(--border-default);background:var(--bg-elevated);color:var(--text-primary)}\n";
  css << "html[data-theme=\"dark\"] .robo-confirm[data-variant=\"solid\"][data-tone=\"neutral\"],"
         "html[data-theme=\"dark\"] .robo-confirm[data-confirming=\"true\"][data-tone=\"neutral\"]"
         "{border-color:var(--border-strong);background:var(--bg-secondary)}\n";
  css << ".robo-confirm[data-variant=\"solid\"][data-tone=\"neutral\"]:hover:not(:disabled),"
         ".robo-confirm[data-confirming=\"true\"][data-tone=\"neutral\"]:hover:not(:disabled)"
         "{border-color:var(--text-tertiary)}\n";
  css << ".robo-confirm[data-variant=\"text\"]:not([data-confirming=\"true\"]){height:auto;padding:0;border:0;"
         "border-radius:2px;background:transparent;font-weight:400;text-decoration:underline;"
         "text-decoration-color:transparent;text-decoration-thickness:0.065em;text-underline-offset:0.18em;"
         "transition:color 300ms cubic-bezier(0.28,0.11,0.32,1),text-decoration-color 300ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << ".robo-confirm[data-variant=\"text\"][data-size=\"sm\"]:not([data-confirming=\"true\"]){font-size:12px;line-height:1.33}\n";
  css << ".robo-confirm[data-variant=\"text\"][data-size=\"md\"]:not([data-confirming=\"true\"]){font-size:17px;line-height:1.47}\n";
  css << ".robo-confirm[data-variant=\"text\"][data-size=\"lg\"]:not([data-confirming=\"true\"]){font-size:17px;line-height:1.42}\n";
  css << "@media (min-width:735px){.robo-confirm[data-variant=\"text\"][data-size=\"lg\"]:not([data-confirming=\"true\"])"
         "{font-size:19px}}\n";
  css << ".robo-confirm[data-variant=\"text\"][data-tone=\"danger\"]:not([data-confirming=\"true\"]){color:var(--danger)}\n";
  css << ".robo-confirm[data-variant=\"text\"][data-tone=\"danger\"]:not([data-confirming=\"true\"]):hover:not(:disabled),"
         ".robo-confirm[data-variant=\"text\"][data-tone=\"danger\"]:not([data-confirming=\"true\"]):active:not(:disabled)"
         "{color:var(--danger-hover)}\n";
  css << ".robo-confirm[data-variant=\"text\"][data-tone=\"brand\"]:not([data-confirming=\"true\"]){color:var(--text-link)}\n";
  css << ".robo-confirm[data-variant=\"text\"][data-tone=\"neutral\"]:not([data-confirming=\"true\"]){color:var(--text-primary)}\n";
  css << ".robo-confirm[data-variant=\"text\"][data-tone=\"brand\"]:not([data-confirming=\"true\"]):hover:not(:disabled),"
         ".robo-confirm[data-variant=\"text\"][data-tone=\"neutral\"]:not([data-confirming=\"true\"]):hover:not(:disabled)"
         "{color:var(--brand-primary-hover)}\n";
  css << ".robo-confirm[data-variant=\"text\"][data-tone=\"brand\"]:not([data-confirming=\"true\"]):active:not(:disabled),"
         ".robo-confirm[data-variant=\"text\"][data-tone=\"neutral\"]:not([data-confirming=\"true\"]):active:not(:disabled)"
         "{color:var(--brand-primary-active)}\n";
  css << ".robo-confirm[data-variant=\"text\"]:not([data-confirming=\"true\"]):hover:not(:disabled),"
         ".robo-confirm[data-variant=\"text\"]:not([data-confirming=\"true\"]):focus-visible{text-decoration-color:currentColor}\n";
  css << ".robo-confirm[data-variant=\"text\"]:not([data-confirming=\"true\"]):focus-visible{outline:2px solid "
         "var(--brand-primary);outline-offset:2px}\n";
  css << ".robo-confirm:disabled{cursor:not-allowed;opacity:0.55}\n";
  css << ".robo-confirm[data-variant=\"text\"]:disabled:not([data-confirming=\"true\"]){color:var(--text-disabled);"
         "text-decoration-color:transparent;opacity:1}\n";
}

std::string RoboWebConfirmActionScript() {
  return R"JS(
  var confirmTimers = new WeakMap();
  function confirmLabel(node, text) {
    var slot = node.querySelector(".robo-confirm-text");
    if (slot) slot.textContent = text;
  }
  function confirmReset(node) {
    var timer = confirmTimers.get(node);
    if (timer) window.clearTimeout(timer);
    confirmTimers.delete(node);
    node.removeAttribute("data-confirming");
    node.setAttribute("aria-pressed", "false");
    confirmLabel(node, node.getAttribute("data-label") || "");
  }
  document.addEventListener("click", function (ev) {
    var node = ev.target.closest ? ev.target.closest(".robo-confirm") : null;
    if (!node) return;
    ev.stopPropagation();
    if (node.disabled || node.getAttribute("data-loading") === "true") return;
    if (node.getAttribute("data-confirming") === "true") {
      confirmReset(node);
      node.dispatchEvent(new CustomEvent("robo-confirm", { bubbles: true }));
      return;
    }
    node.setAttribute("data-confirming", "true");
    node.setAttribute("aria-pressed", "true");
    confirmLabel(node, node.getAttribute("data-confirm") || "Confirm?");
    var ms = parseInt(node.getAttribute("data-reset-ms") || "3000", 10);
    if (!(ms > 0)) ms = 3000;
    confirmTimers.set(node, window.setTimeout(function () { confirmReset(node); }, ms));
  });
)JS";
}

const char* RoboWebConfirmActionClass() { return "robo-confirm"; }
