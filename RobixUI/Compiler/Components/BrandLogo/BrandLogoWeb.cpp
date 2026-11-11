#include "BrandLogoWeb.hpp"

#include "../Text/TextWeb.hpp"

namespace {

std::string Trim(std::string text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\n' || text.front() == '\r' || text.front() == '\t')) {
    text.erase(text.begin());
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\n' || text.back() == '\r' || text.back() == '\t')) {
    text.pop_back();
  }
  return text;
}

std::string VariantOf(const RoboXMLNode& node) {
  const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "mark"));
  if (variant == "icon" || variant == "text" || variant == "lockup") return variant;
  return "mark";
}

bool HasAttr(const RoboXMLNode& node, const char* key) {
  const std::string wanted = RoboWebLower(key);
  for (const auto& entry : node.attrs) {
    if (RoboWebLower(entry.first) == wanted) return true;
  }
  return false;
}

std::string Paint(const RoboFoundationLibrary& library, const std::string& raw, const char* fallback) {
  if (raw.empty()) return fallback;
  const std::string color = RoboWebCssValue(library, raw, true);
  return color.empty() ? fallback : color;
}

}  // namespace

void RoboWebEmitBrandLogo(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library) {
  const std::string variant = VariantOf(node);
  const bool lockup = variant == "lockup";
  const bool words = variant == "text" || lockup;
  const std::string id = RoboXMLAttr(node, "id");
  std::string title = RoboXMLAttr(node, "primary");
  if (title.empty()) title = RoboXMLAttr(node, "title");
  if (title.empty()) title = Trim(node.text);
  if (title.empty()) title = lockup ? "Roboticela" : "DevKit";
  std::string subtitle = RoboXMLAttr(node, "secondary");
  if (subtitle.empty()) subtitle = "DevKit";
  const bool decorative = words || (HasAttr(node, "alt") && RoboXMLAttr(node, "alt").empty());
  std::string label = RoboXMLAttr(node, "alt");
  if (label.empty()) {
    label = lockup ? title + " " + subtitle : title;
    if (!words && !HasAttr(node, "primary") && !HasAttr(node, "title") && Trim(node.text).empty()) label = "DevKit";
  }
  const std::string size = RoboWebCssValue(library, RoboXMLAttr(node, "size", "20"), false);
  const std::string mark = Paint(library, RoboXMLAttr(node, "markColor", RoboXMLAttr(node, "frameColor").c_str()), "#8b5cf6");
  const std::string accent = Paint(library, RoboXMLAttr(node, "accentColor", RoboXMLAttr(node, "chevronColor").c_str()), "#10b981");

  RoboXMLNode typed = node;
  typed.attrs.erase("size");
  typed.attrs.erase("Size");
  std::string style = RoboWebStyle(typed, library);
  RoboWebAppendTextStyle(style, typed, library);
  RoboWebAddDecl(style, "--brand-title", RoboWebCssValue(library, RoboXMLAttr(node, "titleColor"), true));
  RoboWebAddDecl(style, "--brand-subtitle", RoboWebCssValue(library, RoboXMLAttr(node, "subtitleColor"), true));
  if (!RoboXMLAttr(node, "color").empty() || !RoboXMLAttr(node, "textColor").empty()) {
    RoboWebAddDecl(style, "--brand-ink", "1");
  }

  html += "<span class=\"robo-brand robo-type\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\"";
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  if (decorative) {
    html += " aria-hidden=\"true\"";
  } else if (!label.empty()) {
    html += " role=\"img\" aria-label=\"";
    html += RoboWebEscape(label);
    html += "\"";
  }
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += "><svg class=\"robo-brand-mark\" width=\"";
  html += RoboWebEscape(size.empty() ? std::string("20px") : size);
  html += "\" height=\"";
  html += RoboWebEscape(size.empty() ? std::string("20px") : size);
  html += "\" viewBox=\"0 0 24 24\" fill=\"none\" aria-hidden=\"true\"><rect x=\"3\" y=\"3\" width=\"18\" height=\"18\" rx=\"3\" stroke=\"";
  html += RoboWebEscape(mark);
  html += "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"></rect><path d=\"M10 8L6 12L10 16\" stroke=\"";
  html += RoboWebEscape(accent);
  html += "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"></path><path d=\"M14 8L18 12L14 16\" stroke=\"";
  html += RoboWebEscape(accent);
  html += "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"></path></svg>";
  if (words) {
    html += "<span class=\"robo-brand-label\"><span class=\"robo-brand-title\">";
    html += RoboWebEscape(title);
    html += "</span>";
    if (lockup) {
      html += " <span class=\"robo-brand-subtitle\">";
      html += RoboWebEscape(subtitle);
      html += "</span>";
    }
    html += "</span>";
  }
  html += "</span>";
}

void RoboWebAppendBrandLogoCss(std::ostream& css) {
  css << ".robo-brand{box-sizing:border-box;display:inline-flex;align-items:center;gap:8px;max-width:100%;"
         "color:var(--text-primary);line-height:1;vertical-align:middle}\n";
  css << ".robo-brand[data-variant=\"mark\"],.robo-brand[data-variant=\"icon\"]{gap:0}\n";
  css << ".robo-brand-mark{display:block;flex:0 0 auto}\n";
  css << ".robo-brand-label{white-space:nowrap;font-size:13px;font-weight:600;letter-spacing:-0.01em;line-height:1}\n";
  css << ".robo-brand-subtitle{font-weight:500;color:var(--text-secondary)}\n";
  css << ".robo-brand[style*=\"font-size\"] .robo-brand-label{font-size:inherit}\n";
  css << ".robo-brand[style*=\"font-weight\"] .robo-brand-label,.robo-brand[style*=\"font-weight\"] .robo-brand-subtitle{font-weight:inherit}\n";
  css << ".robo-brand[style*=\"font-family\"] .robo-brand-label{font-family:inherit}\n";
  css << ".robo-brand[style*=\"font-style\"] .robo-brand-label{font-style:inherit}\n";
  css << ".robo-brand[style*=\"letter-spacing\"] .robo-brand-label{letter-spacing:inherit}\n";
  css << ".robo-brand[style*=\"line-height\"] .robo-brand-label{line-height:inherit}\n";
  css << ".robo-brand[style*=\"text-transform\"] .robo-brand-label{text-transform:inherit}\n";
  css << ".robo-brand[style*=\"--brand-ink\"] .robo-brand-subtitle{color:inherit}\n";
  css << ".robo-brand[style*=\"--brand-title\"] .robo-brand-title{color:var(--brand-title)}\n";
  css << ".robo-brand[style*=\"--brand-subtitle\"] .robo-brand-subtitle{color:var(--brand-subtitle)}\n";
}

const char* RoboWebBrandLogoClass() { return "robo-brand"; }
