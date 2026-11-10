#include "PasswordRevealWeb.hpp"

#include "../Button/ButtonWeb.hpp"
#include "../../RoboCompilerShared.hpp"

namespace fs = std::filesystem;

namespace {

bool RoboWebFlag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes";
}

std::string RoboWebRevealVariant(const RoboXMLNode& node) {
  const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "ghost"));
  if (variant == "subtle" || variant == "outline" || variant == "text" || variant == "pill") {
    return variant;
  }
  return "ghost";
}

std::string RoboWebRevealSize(const RoboXMLNode& node) {
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "md"));
  if (size == "sm" || size == "small") {
    return "sm";
  }
  if (size == "lg" || size == "large") {
    return "lg";
  }
  return "md";
}

int RoboWebRevealIcon(const std::string& size) {
  if (size == "sm") {
    return 16;
  }
  if (size == "lg") {
    return 20;
  }
  return 18;
}

std::string RoboWebRevealGlyph(const fs::path& assetsRoot, const char* name, int size, const char* eye) {
  const std::string svg = RoboWebSvg(assetsRoot, name);
  if (svg.empty()) {
    return {};
  }
  return "<span class=\"robo-reveal-glyph\" data-eye=\"" + std::string(eye) + "\" style=\"width:" +
         std::to_string(size) + "px;height:" + std::to_string(size) + "px\">" + svg + "</span>";
}

}  // namespace

void RoboWebEmitPasswordReveal(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                               const fs::path& assetsRoot, bool fieldDisabled, bool placedInField) {
  const std::string id = RoboXMLAttr(node, "id");
  const std::string variant = RoboWebRevealVariant(node);
  const std::string size = RoboWebRevealSize(node);
  const bool visible = RoboWebFlag(RoboXMLAttr(node, "visible"));
  const bool disabled = fieldDisabled || RoboWebFlag(RoboXMLAttr(node, "disabled"));
  bool inField = placedInField;
  if (!placedInField && !RoboXMLAttr(node, "inField").empty()) {
    inField = RoboWebFlag(RoboXMLAttr(node, "inField"));
  }
  const std::string showLabel = RoboXMLAttr(node, "showLabel", "Show");
  const std::string hideLabel = RoboXMLAttr(node, "hideLabel", "Hide");
  const std::string word = visible ? hideLabel : showLabel;
  std::string style = RoboWebStyle(node, library);
  RoboWebAppendControlStyle(style, node, library);
  const bool labeled = variant == "text" || variant == "pill";
  const int icon = RoboWebRevealIcon(size);

  html += "<button class=\"robo-reveal robo-control\" type=\"button\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\" data-size=\"";
  html += RoboWebEscape(size);
  html += "\"";
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  if (inField) {
    html += " data-in-field=\"true\"";
  }
  html += " aria-pressed=\"";
  html += visible ? "true" : "false";
  html += "\" aria-label=\"";
  html += visible ? "Hide password" : "Show password";
  html += "\"";
  if (disabled) {
    html += " disabled";
  }
  RoboWebAppendPreviewAttr(html, node);
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += ">";
  if (variant != "text") {
    html += RoboWebRevealGlyph(assetsRoot, "VisibilityEyeShown", icon, "open");
    html += RoboWebRevealGlyph(assetsRoot, "VisibilityEyeHidden", icon, "closed");
  }
  if (labeled || variant == "text") {
    html += "<span class=\"robo-reveal-label\" data-show=\"";
    html += RoboWebEscape(showLabel);
    html += "\" data-hide=\"";
    html += RoboWebEscape(hideLabel);
    html += "\">";
    html += RoboWebEscape(word);
    html += "</span>";
  }
  html += "</button>";
}

void RoboWebAppendPasswordRevealCss(std::ostream& css) {
  css << ".robo-reveal{box-sizing:border-box;display:inline-flex;flex:0 0 auto;align-items:center;justify-content:center;"
         "margin:0;padding:0;border:0;background:transparent;color:var(--text-tertiary);cursor:pointer;"
         "font-family:var(--font-sans,system-ui,sans-serif);font-weight:500;line-height:1.35;white-space:nowrap;outline:none;"
         "transition:color 150ms cubic-bezier(0.28,0.11,0.32,1),background-color 150ms cubic-bezier(0.28,0.11,0.32,1),"
         "border-color 150ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << ".robo-reveal:focus{outline:none}\n";
  css << ".robo-reveal:focus-visible{outline:2px solid var(--brand-primary);outline-offset:2px}\n";
  css << ".robo-reveal:disabled{opacity:0.55;pointer-events:none}\n";
  css << ".robo-reveal-glyph{display:inline-flex;align-items:center;justify-content:center}\n";
  css << ".robo-reveal-glyph svg{width:100%;height:100%;display:block}\n";
  css << ".robo-reveal[aria-pressed=\"false\"] .robo-reveal-glyph[data-eye=\"closed\"],"
         ".robo-reveal[aria-pressed=\"true\"] .robo-reveal-glyph[data-eye=\"open\"],"
         ".robo-reveal[data-variant=\"text\"] .robo-reveal-glyph{display:none}\n";
  css << ".robo-reveal:not([data-variant=\"text\"]):not([data-variant=\"pill\"]) .robo-reveal-label{display:none}\n";
  css << ".robo-reveal[data-size=\"sm\"]:not([data-variant=\"text\"]):not([data-variant=\"pill\"]){width:28px;height:28px}\n";
  css << ".robo-reveal[data-size=\"md\"]:not([data-variant=\"text\"]):not([data-variant=\"pill\"]){width:32px;height:32px}\n";
  css << ".robo-reveal[data-size=\"lg\"]:not([data-variant=\"text\"]):not([data-variant=\"pill\"]){width:36px;height:36px}\n";
  css << ".robo-reveal[data-variant=\"ghost\"],.robo-reveal[data-variant=\"text\"]{border-radius:8px}\n";
  css << ".robo-reveal[data-variant=\"subtle\"],.robo-reveal[data-variant=\"outline\"],.robo-reveal[data-variant=\"pill\"]"
         "{border-radius:980px}\n";
  css << ".robo-reveal[data-variant=\"subtle\"]{background:var(--bg-tertiary);color:var(--text-secondary)}\n";
  css << ".robo-reveal[data-variant=\"outline\"]{border:1px solid var(--border-default);color:var(--text-secondary)}\n";
  css << ".robo-reveal[data-variant=\"text\"]{color:var(--brand-primary)}\n";
  css << ".robo-reveal[data-variant=\"pill\"]{background:var(--bg-tertiary);color:var(--text-secondary)}\n";
  css << ".robo-reveal[data-variant=\"text\"][data-size=\"sm\"]{height:auto;padding:0 2px;font-size:13px}\n";
  css << ".robo-reveal[data-variant=\"text\"][data-size=\"md\"]{height:auto;padding:0 2px;font-size:15px}\n";
  css << ".robo-reveal[data-variant=\"text\"][data-size=\"lg\"]{height:auto;padding:0 2px;font-size:17px}\n";
  css << ".robo-reveal[data-variant=\"pill\"][data-size=\"sm\"]{height:28px;gap:4px;padding:0 8px;font-size:12px}\n";
  css << ".robo-reveal[data-variant=\"pill\"][data-size=\"md\"]{height:28px;gap:6px;padding:0 10px;font-size:13px}\n";
  css << ".robo-reveal[data-variant=\"pill\"][data-size=\"lg\"]{height:32px;gap:6px;padding:0 12px;font-size:15px}\n";
  css << ".robo-reveal[data-variant=\"ghost\"]:hover,.robo-reveal[data-variant=\"ghost\"]:focus-visible"
         "{color:var(--text-primary)}\n";
  css << ".robo-reveal[data-variant=\"subtle\"]:hover,.robo-reveal[data-variant=\"outline\"]:hover,"
         ".robo-reveal[data-variant=\"pill\"]:hover{background:var(--bg-hover);color:var(--text-primary)}\n";
  css << ".robo-reveal[data-variant=\"text\"]:hover{color:var(--brand-primary-hover)}\n";
  css << ".robo-reveal[data-variant=\"ghost\"][aria-pressed=\"true\"]{color:var(--text-primary)}\n";
  css << ".robo-reveal[data-variant=\"subtle\"][aria-pressed=\"true\"],.robo-reveal[data-variant=\"subtle\"]"
         "[aria-pressed=\"true\"]:hover{background:var(--bg-hover);color:var(--text-primary)}\n";
  css << ".robo-reveal[data-variant=\"outline\"][aria-pressed=\"true\"],.robo-reveal[data-variant=\"outline\"]"
         "[aria-pressed=\"true\"]:hover{border-color:var(--brand-primary);color:var(--brand-primary)}\n";
  css << ".robo-reveal[data-variant=\"outline\"][aria-pressed=\"true\"]:hover{background:var(--bg-hover)}\n";
  css << ".robo-reveal[data-variant=\"text\"][aria-pressed=\"true\"]{color:var(--brand-primary)}\n";
  css << ".robo-reveal[data-variant=\"pill\"][aria-pressed=\"true\"],.robo-reveal[data-variant=\"pill\"]"
         "[aria-pressed=\"true\"]:hover{background:var(--brand-primary-subtle);color:var(--brand-primary)}\n";
  css << ".robo-reveal[data-in-field=\"true\"]{position:absolute;top:50%;right:6px;z-index:2;max-width:calc(100% - 28px);"
         "transform:translateY(-50%)}\n";
  css << ".robo-reveal[data-in-field=\"true\"][data-size=\"sm\"]:not([data-variant=\"text\"]):not([data-variant=\"pill\"])"
         "{right:4px}\n";
  css << ".robo-reveal[data-in-field=\"true\"][data-size=\"md\"]:not([data-variant=\"text\"]):not([data-variant=\"pill\"])"
         "{right:6px}\n";
  css << ".robo-reveal[data-in-field=\"true\"][data-size=\"lg\"]:not([data-variant=\"text\"]):not([data-variant=\"pill\"])"
         "{right:8px}\n";
  css << ".robo-reveal[data-in-field=\"true\"][data-variant=\"text\"][data-size=\"sm\"],"
         ".robo-reveal[data-in-field=\"true\"][data-variant=\"pill\"][data-size=\"sm\"]{right:10px}\n";
  css << ".robo-reveal[data-in-field=\"true\"][data-variant=\"text\"][data-size=\"md\"],"
         ".robo-reveal[data-in-field=\"true\"][data-variant=\"pill\"][data-size=\"md\"]{right:12px}\n";
  css << ".robo-reveal[data-in-field=\"true\"][data-variant=\"text\"][data-size=\"lg\"],"
         ".robo-reveal[data-in-field=\"true\"][data-variant=\"pill\"][data-size=\"lg\"]{right:14px}\n";
  css << ".robo-field:has(.robo-reveal) .robo-text-field{padding-right:44px}\n";
  css << ".robo-form-field[data-size=\"sm\"] .robo-field:has(.robo-reveal) .robo-text-field{padding-right:36px}\n";
  css << ".robo-form-field[data-size=\"lg\"] .robo-field:has(.robo-reveal) .robo-text-field{padding-right:48px}\n";
  css << ".robo-field:has(.robo-reveal[data-variant=\"text\"][data-size=\"sm\"]) .robo-text-field{padding-right:56px}\n";
  css << ".robo-field:has(.robo-reveal[data-variant=\"text\"][data-size=\"md\"]) .robo-text-field{padding-right:68px}\n";
  css << ".robo-field:has(.robo-reveal[data-variant=\"text\"][data-size=\"lg\"]) .robo-text-field{padding-right:76px}\n";
  css << ".robo-field:has(.robo-reveal[data-variant=\"pill\"][data-size=\"sm\"]) .robo-text-field{padding-right:76px}\n";
  css << ".robo-field:has(.robo-reveal[data-variant=\"pill\"][data-size=\"md\"]) .robo-text-field{padding-right:88px}\n";
  css << ".robo-field:has(.robo-reveal[data-variant=\"pill\"][data-size=\"lg\"]) .robo-text-field{padding-right:96px}\n";
}

std::string RoboWebPasswordRevealScript() {
  return R"JS(
  document.addEventListener("mousedown", function (event) {
    var reveal = event.target.closest && event.target.closest(".robo-reveal");
    if (!reveal || reveal.disabled) return;
    event.preventDefault();
  }, true);
  document.addEventListener("click", function (event) {
    var reveal = event.target.closest && event.target.closest(".robo-reveal");
    if (!reveal || reveal.disabled) return;
    var shown = reveal.getAttribute("aria-pressed") === "true";
    var next = !shown;
    reveal.setAttribute("aria-pressed", next ? "true" : "false");
    reveal.setAttribute("aria-label", next ? "Hide password" : "Show password");
    var label = reveal.querySelector(".robo-reveal-label");
    if (label) label.textContent = next ? (label.getAttribute("data-hide") || "Hide") : (label.getAttribute("data-show") || "Show");
    var field = reveal.closest(".robo-field");
    var input = field ? field.querySelector("input") : null;
    if (!input) return;
    var start = input.selectionStart;
    var end = input.selectionEnd;
    input.type = next ? "text" : "password";
    if (start != null && end != null) {
      try { input.setSelectionRange(start, end); } catch (error) {}
    }
  });
)JS";
}

const char* RoboWebPasswordRevealClass() { return "robo-reveal"; }
