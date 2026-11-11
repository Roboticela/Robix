#include "TextFieldWeb.hpp"

#include "../PasswordReveal/PasswordRevealWeb.hpp"

#include <cstdlib>

namespace fs = std::filesystem;

namespace {

bool RoboWebFieldFlag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes";
}

std::string RoboWebFieldLength(const RoboFoundationLibrary& library, const std::string& raw) {
  return RoboWebCssValue(library, raw, false);
}

std::string RoboWebFieldColor(const RoboFoundationLibrary& library, const std::string& raw) {
  return RoboWebCssValue(library, raw, true);
}

std::string RoboWebFieldBox(const RoboFoundationLibrary& library, const std::string& raw) {
  std::string out;
  std::string token;
  const auto flush = [&]() {
    if (token.empty()) return;
    if (!out.empty()) out += " ";
    out += RoboWebFieldLength(library, token);
    token.clear();
  };
  for (char ch : raw) {
    if (ch == ' ' || ch == '\t') flush();
    else token.push_back(ch);
  }
  flush();
  return out;
}

std::string RoboWebFieldRadius(const RoboFoundationLibrary& library, const std::string& raw) {
  const std::string key = RoboWebLower(raw);
  if (key == "none" || key == "square") return "0";
  if (key == "xs") return "4px";
  if (key == "sm") return "8px";
  if (key == "md") return "12px";
  if (key == "lg") return "16px";
  if (key == "xl" || key == "full" || key == "pill") return "999px";
  return RoboWebFieldLength(library, raw);
}

std::string RoboWebFieldTime(const std::string& raw) {
  if (raw.empty() || !RoboWebIsNumber(raw)) return raw;
  const double value = std::atof(raw.c_str());
  if (value > 0 && value <= 10) return raw + "s";
  return raw + "ms";
}

std::string RoboWebFieldLeading(const RoboFoundationLibrary& library, const std::string& raw) {
  if (raw.empty()) return {};
  if (RoboWebIsNumber(raw)) return std::atof(raw.c_str()) > 8 ? raw + "px" : raw;
  return RoboWebFieldLength(library, raw);
}

void RoboWebPaintFieldState(std::string& style, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                            const char* prefix) {
  const auto raw = [&](const char* suffix) { return RoboXMLAttr(node, (std::string(prefix) + suffix).c_str()); };
  const bool rest = prefix[0] == '\0';
  const std::string stem = rest ? std::string("--field") : std::string("--field-") + prefix;
  std::string background = raw("Background");
  if (background.empty()) background = raw("Bg");
  if (rest && background.empty()) background = RoboXMLAttr(node, "backgroundColor");
  std::string ink = raw("Color");
  if (rest && ink.empty()) ink = RoboXMLAttr(node, "textColor");
  if (!rest && std::string(prefix) == "disabled" && ink.empty()) ink = RoboXMLAttr(node, "disabledColor");
  std::string border = raw("BorderColor");
  if (border.empty()) {
    const std::string alt = raw("Border");
    if (!alt.empty() && !RoboWebIsNumber(alt)) border = alt;
  }
  std::string caret = raw("CaretColor");
  if (caret.empty()) caret = raw("Caret");
  if (caret.empty()) caret = raw("CursorColor");
  std::string caretWidth = raw("CaretWidth");
  if (caretWidth.empty()) caretWidth = raw("CursorWidth");
  std::string blink = raw("CaretBlink");
  if (blink.empty()) blink = raw("CursorBlink");
  if (blink.empty()) blink = raw("CaretRate");
  std::string selection = raw("SelectionColor");
  if (selection.empty()) selection = raw("Selection");
  std::string ring = raw("Ring");
  if (ring.empty()) ring = raw("OutlineColor");
  if (rest && ring.empty()) ring = RoboXMLAttr(node, "outline");
  RoboWebAddDecl(style, (stem + "-bg").c_str(), RoboWebFieldColor(library, background));
  RoboWebAddDecl(style, (stem + "-color").c_str(), RoboWebFieldColor(library, ink));
  std::string icon = raw("IconColor");
  if (icon.empty()) {
    const std::string alt = raw("Icon");
    if (!alt.empty() && (alt[0] == '#' || alt.find("rgb") != std::string::npos || RoboWebLower(alt) == alt)) icon = alt;
  }
  RoboWebAddDecl(style, (stem + "-icon").c_str(), RoboWebFieldColor(library, icon));
  RoboWebAddDecl(style, (stem + "-icon-size").c_str(), RoboWebFieldLength(library, raw("IconSize")));
  RoboWebAddDecl(style, (stem + "-border").c_str(), RoboWebFieldColor(library, border));
  RoboWebAddDecl(style, (stem + "-border-width").c_str(),
                 RoboWebFieldLength(library, raw("BorderWidth").empty() && rest && RoboWebIsNumber(raw("Border")) ? raw("Border") : raw("BorderWidth")));
  const std::string borderStyle = RoboWebLower(raw("BorderStyle"));
  if (!borderStyle.empty()) RoboWebAddDecl(style, (stem + "-border-style").c_str(), borderStyle);
  RoboWebAddDecl(style, (stem + "-border-top").c_str(), RoboWebFieldColor(library, raw("BorderTopColor").empty() ? raw("BorderTop") : raw("BorderTopColor")));
  RoboWebAddDecl(style, (stem + "-border-right").c_str(), RoboWebFieldColor(library, raw("BorderRightColor").empty() ? raw("BorderRight") : raw("BorderRightColor")));
  RoboWebAddDecl(style, (stem + "-border-bottom").c_str(), RoboWebFieldColor(library, raw("BorderBottomColor").empty() ? raw("BorderBottom") : raw("BorderBottomColor")));
  RoboWebAddDecl(style, (stem + "-border-left").c_str(), RoboWebFieldColor(library, raw("BorderLeftColor").empty() ? raw("BorderLeft") : raw("BorderLeftColor")));
  RoboWebAddDecl(style, (stem + "-width-top").c_str(), RoboWebFieldLength(library, raw("BorderTopWidth")));
  RoboWebAddDecl(style, (stem + "-width-right").c_str(), RoboWebFieldLength(library, raw("BorderRightWidth")));
  RoboWebAddDecl(style, (stem + "-width-bottom").c_str(), RoboWebFieldLength(library, raw("BorderBottomWidth")));
  RoboWebAddDecl(style, (stem + "-width-left").c_str(), RoboWebFieldLength(library, raw("BorderLeftWidth")));
  RoboWebAddDecl(style, (stem + "-radius").c_str(), RoboWebFieldRadius(library, raw("Radius").empty() && rest ? RoboXMLAttr(node, "borderRadius") : raw("Radius")));
  RoboWebAddDecl(style, (stem + "-radius-tl").c_str(), RoboWebFieldRadius(library, raw("RadiusTopLeft")));
  RoboWebAddDecl(style, (stem + "-radius-tr").c_str(), RoboWebFieldRadius(library, raw("RadiusTopRight")));
  RoboWebAddDecl(style, (stem + "-radius-br").c_str(), RoboWebFieldRadius(library, raw("RadiusBottomRight")));
  RoboWebAddDecl(style, (stem + "-radius-bl").c_str(), RoboWebFieldRadius(library, raw("RadiusBottomLeft")));
  RoboWebAddDecl(style, (stem + "-pad").c_str(), RoboWebFieldBox(library, raw("Padding")));
  if (!raw("Shadow").empty() || !raw("Glow").empty()) {
    const std::string glow = RoboWebFieldColor(library, raw("Glow"));
    RoboWebAddDecl(style, (stem + "-shadow").c_str(), glow.empty() ? raw("Shadow") : "0 0 18px " + glow);
  }
  RoboWebAddDecl(style, (stem + "-placeholder").c_str(), RoboWebFieldColor(library, raw("PlaceholderColor")));
  RoboWebAddDecl(style, (stem + "-caret").c_str(), RoboWebFieldColor(library, caret));
  RoboWebAddDecl(style, (stem + "-caret-width").c_str(), RoboWebFieldLength(library, caretWidth));
  RoboWebAddDecl(style, (stem + "-caret-blink").c_str(), RoboWebFieldTime(blink));
  RoboWebAddDecl(style, (stem + "-selection").c_str(), RoboWebFieldColor(library, selection));
  RoboWebAddDecl(style, (stem + "-selection-text").c_str(), RoboWebFieldColor(library, raw("SelectionText")));
  RoboWebAddDecl(style, (stem + "-font-size").c_str(), RoboWebFieldLength(library, raw("FontSize")));
  const std::string weight = raw("FontWeight");
  if (!weight.empty()) RoboWebAddDecl(style, (stem + "-font-weight").c_str(), weight);
  const std::string family = raw("FontFamily");
  if (!family.empty()) RoboWebAddDecl(style, (stem + "-font-family").c_str(), family);
  const std::string fontStyle = RoboWebLower(raw("FontStyle"));
  if (!fontStyle.empty()) RoboWebAddDecl(style, (stem + "-font-style").c_str(), fontStyle);
  RoboWebAddDecl(style, (stem + "-letter").c_str(), RoboWebFieldLength(library, raw("LetterSpacing").empty() ? raw("CharacterSpacing") : raw("LetterSpacing")));
  RoboWebAddDecl(style, (stem + "-leading").c_str(), RoboWebFieldLeading(library, raw("LineHeight").empty() ? raw("Leading") : raw("LineHeight")));
  RoboWebAddDecl(style, (stem + "-height").c_str(), RoboWebFieldLength(library, raw("Height")));
  RoboWebAddDecl(style, (stem + "-min-height").c_str(), RoboWebFieldLength(library, raw("MinHeight")));
  RoboWebAddDecl(style, (stem + "-max-height").c_str(), RoboWebFieldLength(library, raw("MaxHeight")));
  if (!ring.empty() && !RoboWebIsNumber(ring)) {
    std::string width = raw("OutlineWidth");
    width = width.empty() ? "2px" : RoboWebFieldLength(library, width);
    RoboWebAddDecl(style, (stem + "-outline").c_str(), width + " solid " + RoboWebFieldColor(library, ring));
  }
  const std::string offset = raw("Offset").empty() ? raw("OutlineOffset") : raw("Offset");
  RoboWebAddDecl(style, (stem + "-outline-offset").c_str(), RoboWebFieldLength(library, offset));
}

}  // namespace

bool RoboWebFieldWantsCaret(const RoboXMLNode& node) {
  const char* keys[] = {"caretWidth", "cursorWidth", "caretBlink", "cursorBlink", "caretRate", "caretShape", "cursorShape"};
  for (const char* key : keys) {
    if (!RoboXMLAttr(node, key).empty()) return true;
  }
  return false;
}

void RoboWebStripFieldChrome(RoboXMLNode& node) {
  const auto chrome = [](const std::string& key) {
    if (key == "cursor") return false;
    const char* prefixes[] = {"hover", "focus", "disabled", "invalid", "valid", "readonly", "caret", "cursor", "selection", "placeholder"};
    for (const char* prefix : prefixes) {
      const std::string name = prefix;
      if (key.size() > name.size() && key.compare(0, name.size(), name) == 0) return true;
    }
    const char* exact[] = {"padding", "paddingx", "paddingy", "paddinghorizontal", "paddingvertical", "paddingtop",
                           "paddingright", "paddingbottom", "paddingleft", "border", "borderwidth", "borderstyle",
                           "bordercolor", "bordertop", "borderright", "borderbottom", "borderleft", "bordertopwidth",
                           "borderrightwidth", "borderbottomwidth", "borderleftwidth", "bordertopcolor",
                           "borderrightcolor", "borderbottomcolor", "borderleftcolor", "radius", "borderradius",
                           "radiustopleft", "radiustopright", "radiusbottomright", "radiusbottomleft", "background",
                           "backgroundcolor", "color", "textcolor", "font", "fontfamily", "fontsize", "fontweight",
                           "fontstyle", "letterspacing", "characterspacing", "charspacing", "tracking", "lineheight",
                           "leading", "shadow", "boxshadow", "glow", "height", "minheight", "maxheight", "iconcolor",
                           "iconsize", "outline", "outlinecolor", "outlinewidth", "outlineoffset"};
    for (const char* name : exact) {
      if (key == name) return true;
    }
    return false;
  };
  for (auto it = node.attrs.begin(); it != node.attrs.end();) {
    if (chrome(RoboWebLower(it->first))) it = node.attrs.erase(it);
    else ++it;
  }
}

void RoboWebAppendFieldStyle(std::string& style, const RoboXMLNode& node, const RoboFoundationLibrary& library) {
  RoboWebPaintFieldState(style, node, library, "");
  const std::string font = RoboXMLAttr(node, "font");
  if (!font.empty() && !RoboWebIsNumber(font)) RoboWebAddDecl(style, "--field-font-family", font);
  RoboWebPaintFieldState(style, node, library, "hover");
  RoboWebPaintFieldState(style, node, library, "focus");
  RoboWebPaintFieldState(style, node, library, "disabled");
  RoboWebPaintFieldState(style, node, library, "invalid");
  RoboWebPaintFieldState(style, node, library, "valid");
}

const char* RoboWebFieldCaretScript() {
  return R"JS(
(function () {
  function fieldCaretBox(input) {
    var cs = getComputedStyle(input);
    var canvas = fieldCaretBox.canvas || (fieldCaretBox.canvas = document.createElement("canvas"));
    var ctx = canvas.getContext("2d");
    ctx.font = cs.font;
    var index = input.selectionStart || 0;
    var before = (input.value || "").slice(0, index);
    var line = 0;
    var last = before;
    var nl = before.lastIndexOf("\n");
    if (nl >= 0) {
      last = before.slice(nl + 1);
      line = before.split("\n").length - 1;
    }
    var width = ctx.measureText(last).width;
    var leading = parseFloat(cs.lineHeight);
    if (!leading || leading < 8) leading = (parseFloat(cs.fontSize) || 16) * 1.4;
    return {
      x: (parseFloat(cs.paddingLeft) || 0) + width - input.scrollLeft,
      y: (parseFloat(cs.paddingTop) || 0) + line * leading - input.scrollTop,
      h: leading
    };
  }
  function placeFieldCarets() {
    var carets = document.querySelectorAll(".robo-field-caret");
    for (var i = 0; i < carets.length; i++) {
      var caret = carets[i];
      var field = caret.parentElement;
      var input = field && field.querySelector(".robo-field-control");
      if (!input) continue;
      var box = fieldCaretBox(input);
      var shape = field.getAttribute("data-caret");
      caret.style.left = box.x + "px";
      if (shape === "underscore") {
        caret.style.top = (box.y + box.h - 2) + "px";
        caret.style.height = "2px";
      } else {
        caret.style.top = box.y + "px";
        caret.style.height = box.h + "px";
      }
    }
  }
  document.addEventListener("input", placeFieldCarets, true);
  document.addEventListener("keyup", placeFieldCarets, true);
  document.addEventListener("click", placeFieldCarets, true);
  document.addEventListener("focusin", placeFieldCarets, true);
  document.addEventListener("scroll", placeFieldCarets, true);
  document.addEventListener("selectionchange", placeFieldCarets);
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", placeFieldCarets);
  else placeFieldCarets();
})();
)JS";
}

void RoboWebEmitTextField(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                          const fs::path& assetsRoot) {
  const std::string id = RoboXMLAttr(node, "id");
  const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "outline"));
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "md"));
  std::string type = RoboWebLower(RoboXMLAttr(node, "type", "text"));
  bool revealVisible = false;
  for (const RoboXMLNode& child : node.children) {
    if (RoboWebLower(child.name) == "passwordreveal" && RoboWebFieldFlag(RoboXMLAttr(child, "visible"))) {
      revealVisible = true;
    }
  }
  if (type == "password" && revealVisible) {
    type = "text";
  }
  const std::string value = RoboXMLAttr(node, "value", RoboXMLAttr(node, "content").c_str());
  const std::string placeholder = RoboXMLAttr(node, "placeholder");
  const std::string label = RoboXMLAttr(node, "label");
  const std::string hint = RoboXMLAttr(node, "hint");
  const std::string trailing = RoboXMLAttr(node, "trailing");
  const std::string preview = RoboWebLower(RoboXMLAttr(node, "preview"));
  std::string icon = RoboXMLAttr(node, "icon");
  for (const RoboXMLNode& child : node.children) {
    const std::string childIcon = RoboXMLAttr(child, "name", RoboXMLAttr(child, "icon").c_str());
    if (!childIcon.empty()) {
      icon = childIcon;
    }
  }
  const std::string invalidValue = RoboWebLower(RoboXMLAttr(node, "invalid"));
  const std::string validValue = RoboWebLower(RoboXMLAttr(node, "valid"));
  const std::string disabledValue = RoboWebLower(RoboXMLAttr(node, "disabled"));
  const bool invalid = invalidValue == "true" || invalidValue == "1";
  const bool valid = !invalid && (validValue == "true" || validValue == "1");
  const bool disabled = disabledValue == "true" || disabledValue == "1";
  const bool readOnly = RoboWebLower(RoboXMLAttr(node, "readOnly")) == "true" ||
                         RoboWebLower(RoboXMLAttr(node, "readonly")) == "true";
  const bool required = RoboWebLower(RoboXMLAttr(node, "required")) == "true";
  const bool optional = RoboWebLower(RoboXMLAttr(node, "optional")) == "true";
  RoboXMLNode layout = node;
  RoboWebStripFieldChrome(layout);
  const std::string style = RoboWebStyle(layout, library);
  std::string fieldStyle;
  RoboWebAppendFieldStyle(fieldStyle, node, library);
  const std::string caretShape = RoboWebLower(RoboXMLAttr(node, "caretShape", RoboXMLAttr(node, "cursorShape").c_str()));
  html += "<div class=\"robo-form-field\" data-size=\"";
  html += RoboWebEscape(size);
  html += "\"";
  if (RoboWebFieldFlag(RoboXMLAttr(node, "password"))) {
    html += " data-password=\"true\"";
  }
  if (RoboWebFieldFlag(RoboXMLAttr(node, "email"))) {
    html += " data-email=\"true\"";
  }
  if (RoboWebFieldFlag(RoboXMLAttr(node, "phone"))) {
    html += " data-phone=\"true\"";
  }
  if (RoboWebFieldFlag(RoboXMLAttr(node, "password")) || RoboWebFieldFlag(RoboXMLAttr(node, "email")) ||
      RoboWebFieldFlag(RoboXMLAttr(node, "phone")) || RoboWebFieldFlag(RoboXMLAttr(node, "sizedIcon"))) {
    html += " data-sized-icon=\"true\"";
  }
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += ">";
  if (!label.empty()) {
    html += "<label class=\"robo-field-label\"";
    if (!id.empty()) {
      html += " for=\"";
      html += RoboWebEscape(id);
      html += "\"";
    }
    if (invalid) {
      html += " data-tone=\"danger\"";
    } else if (valid) {
      html += " data-tone=\"success\"";
    } else if (disabled) {
      html += " data-tone=\"disabled\"";
    }
    html += ">";
    html += RoboWebEscape(label);
    if (required && !optional) {
      html += "<span class=\"robo-field-required\" aria-hidden=\"true\">*</span>";
    }
    if (optional) {
      html += "<span class=\"robo-field-optional\">Optional</span>";
    }
    html += "</label>";
  }
  html += "<label class=\"robo-field\"";
  if (caretShape == "bar" || caretShape == "block" || caretShape == "underscore") {
    html += " data-caret=\"";
    html += RoboWebEscape(caretShape);
    html += "\"";
  }
  if (!fieldStyle.empty()) {
    html += " style=\"";
    html += RoboWebEscape(fieldStyle);
    html += "\"";
  }
  html += ">";
  if (!icon.empty()) {
    html += "<span class=\"robo-field-icon\">";
    html += RoboWebSvg(assetsRoot, icon);
    html += "</span>";
  }
  html += "<input class=\"robo-text-field robo-field-control\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\" data-size=\"";
  html += RoboWebEscape(size);
  html += "\"";
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  html += " type=\"";
  html += RoboWebEscape(type.empty() ? std::string("text") : type);
  html += "\"";
  if (!placeholder.empty()) {
    html += " placeholder=\"";
    html += RoboWebEscape(placeholder);
    html += "\"";
  }
  if (!value.empty()) {
    html += " value=\"";
    html += RoboWebEscape(value);
    html += "\"";
  }
  if (!preview.empty()) {
    html += " data-preview=\"";
    html += RoboWebEscape(preview);
    html += "\"";
  }
  if (invalid) {
    html += " aria-invalid=\"true\"";
  }
  if (valid) {
    html += " data-valid=\"true\"";
  }
  if (disabled) {
    html += " disabled";
  }
  if (readOnly) {
    html += " readonly";
  }
  const std::string maxLength = RoboXMLAttr(node, "maxLength");
  if (!maxLength.empty()) {
    html += " maxlength=\"";
    html += RoboWebEscape(maxLength);
    html += "\"";
  }
  const std::string autoComplete = RoboXMLAttr(node, "autoComplete", RoboXMLAttr(node, "autocomplete").c_str());
  if (!autoComplete.empty()) {
    html += " autocomplete=\"";
    html += RoboWebEscape(autoComplete);
    html += "\"";
  }
  const std::string autoCapitalize = RoboXMLAttr(node, "autoCapitalize", RoboXMLAttr(node, "autocapitalize").c_str());
  if (!autoCapitalize.empty()) {
    html += " autocapitalize=\"";
    html += RoboWebEscape(autoCapitalize);
    html += "\"";
  }
  const std::string autoCorrect = RoboXMLAttr(node, "autoCorrect", RoboXMLAttr(node, "autocorrect").c_str());
  if (!autoCorrect.empty()) {
    html += " autocorrect=\"";
    html += RoboWebEscape(autoCorrect);
    html += "\"";
  }
  const std::string spellCheck = RoboWebLower(RoboXMLAttr(node, "spellCheck", RoboXMLAttr(node, "spellcheck").c_str()));
  if (spellCheck == "false" || spellCheck == "0" || spellCheck == "off") {
    html += " spellcheck=\"false\"";
  }
  const std::string inputMode = RoboXMLAttr(node, "inputMode", RoboXMLAttr(node, "inputmode").c_str());
  if (!inputMode.empty()) {
    html += " inputmode=\"";
    html += RoboWebEscape(inputMode);
    html += "\"";
  }
  html += ">";
  if (RoboWebFieldWantsCaret(node)) {
    html += "<span class=\"robo-field-caret\" aria-hidden=\"true\"></span>";
  }
  if (!trailing.empty()) {
    html += "<span class=\"robo-field-trailing\">";
    html += RoboWebEscape(trailing);
    html += "</span>";
  }
  for (const RoboXMLNode& child : node.children) {
    if (RoboWebLower(child.name) != "passwordreveal") {
      continue;
    }
    RoboWebEmitPasswordReveal(html, child, library, assetsRoot, disabled, true);
  }
  html += "</label>";
  if (!hint.empty()) {
    html += "<span class=\"robo-field-hint\"";
    if (invalid) {
      html += " data-tone=\"error\"";
    } else if (valid) {
      html += " data-tone=\"ok\"";
    }
    html += ">";
    html += RoboWebEscape(hint);
    html += "</span>";
  }
  html += "</div>";
}

void RoboWebAppendTextFieldCss(std::ostream& css) {
    css << ".robo-form-field[data-password=\"true\"][data-size=\"sm\"] .robo-field-icon,"
           ".robo-form-field[data-password=\"true\"][data-size=\"sm\"] .robo-field-icon svg{width:16px;height:16px}\n";
    css << ".robo-form-field[data-password=\"true\"][data-size=\"md\"] .robo-field-icon,"
           ".robo-form-field[data-password=\"true\"][data-size=\"md\"] .robo-field-icon svg{width:18px;height:18px}\n";
    css << ".robo-form-field[data-password=\"true\"][data-size=\"lg\"] .robo-field-icon,"
           ".robo-form-field[data-password=\"true\"][data-size=\"lg\"] .robo-field-icon svg{width:20px;height:20px}\n";
    css << ".robo-form-field{display:flex;flex-direction:column;width:100%;min-width:0;box-sizing:border-box}\n";
    css << ".robo-field-label{display:flex;align-items:baseline;gap:6px;margin:0 0 8px;color:var(--text-secondary);"
           "font-size:17px;font-weight:500;line-height:1.2}\n";
    css << ".robo-form-field[data-size=\"sm\"] .robo-field-label{margin-bottom:6px;font-size:15px}\n";
    css << ".robo-form-field[data-size=\"lg\"] .robo-field-label{margin-bottom:10px;font-size:19px}\n";
    css << ".robo-field-label[data-tone=\"danger\"]{color:var(--danger)}\n";
    css << ".robo-field-label[data-tone=\"success\"]{color:var(--success)}\n";
    css << ".robo-field-label[data-tone=\"disabled\"]{color:var(--text-disabled)}\n";
    css << ".robo-field-required{color:var(--danger)}\n";
    css << ".robo-field-optional{margin-left:auto;color:var(--text-tertiary);font-size:13px;font-weight:400}\n";
    css << ".robo-field{position:relative;display:block;width:100%}\n";
    css << ".robo-field-icon{position:absolute;top:50%;left:16px;z-index:1;display:inline-flex;width:18px;height:18px;"
           "transform:translateY(-50%);color:var(--text-tertiary);pointer-events:none;"
           "transition:color 150ms cubic-bezier(0.28,0.11,0.32,1)}\n";
    css << ".robo-form-field[data-size=\"sm\"] .robo-field-icon{left:12px}\n";
    css << ".robo-field-icon svg{width:18px;height:18px;display:block}\n";
    css << ".robo-field:hover .robo-field-icon{color:var(--text-secondary)}\n";
    css << ".robo-field:focus-within .robo-field-icon{color:var(--brand-primary)}\n";
    css << ".robo-field:has([data-preview=\"hover\"]) .robo-field-icon{color:var(--text-secondary)}\n";
    css << ".robo-field:has([data-preview=\"focus\"]) .robo-field-icon{color:var(--brand-primary)}\n";
    css << ".robo-field:has([aria-invalid=\"true\"]) .robo-field-icon,.robo-field:has([aria-invalid=\"true\"]):focus-within "
           ".robo-field-icon{color:var(--danger)}\n";
    css << ".robo-field:has([data-valid=\"true\"]) .robo-field-icon,.robo-field:has([data-valid=\"true\"]):focus-within "
           ".robo-field-icon{color:var(--success)}\n";
    css << ".robo-field:has(:disabled) .robo-field-icon{color:var(--text-disabled)}\n";
    css << ".robo-field-trailing{position:absolute;top:50%;right:16px;transform:translateY(-50%);color:var(--text-tertiary);"
           "font-size:15px;pointer-events:none}\n";
    css << ".robo-field{min-width:0;max-width:100%}\n";
    css << ".robo-text-field{box-sizing:border-box;display:block;width:100%;min-width:0;max-width:100%;margin:0;padding:0 16px;height:44px;"
           "border:1px solid var(--border-default);border-radius:12px;background:var(--bg-secondary);"
           "color:var(--text-primary);caret-color:var(--text-primary);font:400 17px/42px var(--font-sans,system-ui,sans-serif);"
           "outline:2px solid transparent;outline-offset:1px;"
           "transition:border-color 150ms cubic-bezier(0.28,0.11,0.32,1),background-color 150ms "
           "cubic-bezier(0.28,0.11,0.32,1),outline-color 150ms cubic-bezier(0.28,0.11,0.32,1)}\n";
    css << ".robo-text-field::placeholder{color:var(--text-tertiary)}\n";
    css << ".robo-text-field[data-size=\"sm\"]{height:36px;padding:0 12px;border-radius:10px;font-size:15px;"
           "line-height:34px}\n";
    css << ".robo-text-field[data-size=\"lg\"]{height:50px;line-height:48px}\n";
    css << ".robo-field:has(.robo-field-icon) .robo-text-field{padding-left:44px}\n";
    css << ".robo-form-field[data-size=\"sm\"] .robo-field:has(.robo-field-icon) .robo-text-field{padding-left:36px}\n";
    css << ".robo-form-field[data-size=\"lg\"] .robo-field:has(.robo-field-icon) .robo-text-field{padding-left:48px}\n";
    css << ".robo-field:has(.robo-field-trailing) .robo-text-field{padding-right:44px}\n";
    css << ".robo-form-field[data-size=\"sm\"] .robo-field:has(.robo-field-trailing) .robo-text-field{padding-right:36px}\n";
    css << ".robo-form-field[data-size=\"lg\"] .robo-field:has(.robo-field-trailing) .robo-text-field{padding-right:48px}\n";
    css << ".robo-text-field[data-variant=\"outline\"]:hover,.robo-text-field[data-variant=\"pill\"]:hover,"
           ".robo-text-field[data-variant=\"underline\"]:hover{border-color:var(--border-strong)}\n";
    css << ".robo-text-field[data-variant=\"outline\"]:focus,.robo-text-field[data-variant=\"pill\"]:focus,"
           ".robo-text-field[data-variant=\"outline\"][data-preview=\"focus\"],"
           ".robo-text-field[data-variant=\"pill\"][data-preview=\"focus\"]{border-color:var(--brand-primary);"
           "outline-color:var(--brand-primary-focus-ring)}\n";
    css << ".robo-text-field[data-variant=\"subtle\"]{border-color:transparent;background:var(--bg-tertiary)}\n";
    css << ".robo-text-field[data-variant=\"subtle\"]:hover,.robo-text-field[data-variant=\"ghost\"]:hover,"
           ".robo-text-field[data-variant=\"subtle\"][data-preview=\"hover\"],"
           ".robo-text-field[data-variant=\"ghost\"][data-preview=\"hover\"]{background:var(--bg-hover)}\n";
    css << ".robo-text-field[data-variant=\"subtle\"]:focus,.robo-text-field[data-variant=\"ghost\"]:focus,"
           ".robo-text-field[data-variant=\"subtle\"][data-preview=\"focus\"],"
           ".robo-text-field[data-variant=\"ghost\"][data-preview=\"focus\"]{background:var(--bg-secondary);"
           "border-color:var(--brand-primary);outline-color:var(--brand-primary-focus-ring)}\n";
    css << ".robo-text-field[data-variant=\"ghost\"]{border-color:transparent;background:transparent}\n";
    css << ".robo-text-field[data-variant=\"underline\"]{height:44px;padding:0;border:0;border-bottom:1px solid "
           "var(--border-default);border-radius:0;background:transparent;outline:none}\n";
    css << ".robo-text-field[data-variant=\"underline\"]:focus,.robo-text-field[data-variant=\"underline\"]"
           "[data-preview=\"focus\"]{border-bottom-color:var(--brand-primary);outline:none}\n";
    css << ".robo-text-field[data-variant=\"pill\"]{border-radius:980px}\n";
    css << ".robo-text-field[aria-invalid=\"true\"]{border-color:var(--danger);outline-color:transparent}\n";
    css << ".robo-text-field[aria-invalid=\"true\"]:focus,.robo-text-field[aria-invalid=\"true\"][data-preview=\"focus\"]"
           "{border-color:var(--danger);outline-color:var(--danger-focus-ring)}\n";
    css << ".robo-text-field[data-valid=\"true\"]{border-color:var(--success);outline-color:transparent}\n";
    css << ".robo-text-field[data-valid=\"true\"]:focus,.robo-text-field[data-valid=\"true\"][data-preview=\"focus\"]"
           "{border-color:var(--success);outline-color:var(--success-focus-ring)}\n";
    css << ".robo-text-field:disabled{opacity:0.55;cursor:not-allowed}\n";
    css << ".robo-text-field[readonly]{color:var(--text-secondary)}\n";
    css << ".robo-field-hint{display:block;margin-top:6px;color:var(--text-tertiary);font-size:13px;line-height:1.3}\n";
    css << ".robo-form-field[data-size=\"sm\"] .robo-field-hint{margin-top:4px}\n";
    css << ".robo-form-field[data-size=\"lg\"] .robo-field-hint{margin-top:8px}\n";
    css << ".robo-field-hint[data-tone=\"error\"]{color:var(--danger)}\n";
    css << ".robo-field-hint[data-tone=\"ok\"]{color:var(--success)}\n";
    css << "html[data-theme=\"light\"] .robo-text-field[data-variant=\"outline\"],"
           "html[data-theme=\"light\"] .robo-text-field[data-variant=\"pill\"]{background:var(--bg-elevated)}\n";
    css << ".robo-form-field[data-sized-icon=\"true\"][data-size=\"sm\"] .robo-field-icon,"
           ".robo-form-field[data-sized-icon=\"true\"][data-size=\"sm\"] .robo-field-icon svg{width:16px;height:16px}\n";
    css << ".robo-form-field[data-sized-icon=\"true\"][data-size=\"md\"] .robo-field-icon,"
           ".robo-form-field[data-sized-icon=\"true\"][data-size=\"md\"] .robo-field-icon svg{width:18px;height:18px}\n";
    css << ".robo-form-field[data-sized-icon=\"true\"][data-size=\"lg\"] .robo-field-icon,"
           ".robo-form-field[data-sized-icon=\"true\"][data-size=\"lg\"] .robo-field-icon svg{width:20px;height:20px}\n";
    css << "html[data-theme=\"light\"] .robo-text-field[data-variant=\"subtle\"]:focus,"
           "html[data-theme=\"light\"] .robo-text-field[data-variant=\"ghost\"]:focus,"
           "html[data-theme=\"light\"] .robo-text-field[data-variant=\"subtle\"][data-preview=\"focus\"],"
           "html[data-theme=\"light\"] .robo-text-field[data-variant=\"ghost\"][data-preview=\"focus\"]"
           "{background:var(--bg-elevated)}\n";
    const char* states[][3] = {{"", "", ""},
                               {"-hover", ":hover", "hover"},
                               {"-focus", ":focus-within", "focus"},
                               {"-disabled", ":has(:disabled)", "disabled"},
                               {"-invalid", ":has([aria-invalid=\"true\"])", "invalid"},
                               {"-valid", ":has([data-valid=\"true\"])", "valid"}};
    const auto paint = [&](const char* state, const char* pseudo, const char* preview, const char* suffix,
                           const char* declaration, const char* target) {
        const std::string variable = std::string("--field") + state + suffix;
        std::string selector = ".robo-field[style*=\"" + variable + "\"]" + pseudo + " " + target;
        if (preview[0] != '\0') {
            selector += ",.robo-field[style*=\"" + variable + "\"]:has(.robo-field-control[data-preview=\"" +
                        std::string(preview) + "\"]) " + target;
        }
        css << selector << "{" << declaration << ":var(" << variable << ") !important}\n";
    };
    const char* props[][3] = {{"-bg", "background", ".robo-field-control"},
                              {"-color", "color", ".robo-field-control"},
                              {"-border", "border-color", ".robo-field-control"},
                              {"-border-width", "border-width", ".robo-field-control"},
                              {"-border-style", "border-style", ".robo-field-control"},
                              {"-border-top", "border-top-color", ".robo-field-control"},
                              {"-border-right", "border-right-color", ".robo-field-control"},
                              {"-border-bottom", "border-bottom-color", ".robo-field-control"},
                              {"-border-left", "border-left-color", ".robo-field-control"},
                              {"-width-top", "border-top-width", ".robo-field-control"},
                              {"-width-right", "border-right-width", ".robo-field-control"},
                              {"-width-bottom", "border-bottom-width", ".robo-field-control"},
                              {"-width-left", "border-left-width", ".robo-field-control"},
                              {"-radius", "border-radius", ".robo-field-control"},
                              {"-radius-tl", "border-top-left-radius", ".robo-field-control"},
                              {"-radius-tr", "border-top-right-radius", ".robo-field-control"},
                              {"-radius-br", "border-bottom-right-radius", ".robo-field-control"},
                              {"-radius-bl", "border-bottom-left-radius", ".robo-field-control"},
                              {"-pad", "padding", ".robo-field-control"},
                              {"-shadow", "box-shadow", ".robo-field-control"},
                              {"-font-size", "font-size", ".robo-field-control"},
                              {"-font-weight", "font-weight", ".robo-field-control"},
                              {"-font-family", "font-family", ".robo-field-control"},
                              {"-font-style", "font-style", ".robo-field-control"},
                              {"-letter", "letter-spacing", ".robo-field-control"},
                              {"-leading", "line-height", ".robo-field-control"},
                              {"-height", "height", ".robo-field-control"},
                              {"-min-height", "min-height", ".robo-field-control"},
                              {"-max-height", "max-height", ".robo-field-control"},
                              {"-outline", "outline", ".robo-field-control"},
                              {"-outline-offset", "outline-offset", ".robo-field-control"},
                              {"-caret", "caret-color", ".robo-field-control"},
                              {"-icon", "color", ".robo-field-icon"},
                              {"-icon-size", "width", ".robo-field-icon"},
                              {"-placeholder", "color", ".robo-field-control::placeholder"},
                              {"-caret", "background", ".robo-field-caret"}};
    for (const auto& state : states) {
        for (const auto& prop : props) {
            paint(state[0], state[1], state[2], prop[0], prop[1], prop[2]);
        }
        paint(state[0], state[1], state[2], "-selection", "background", ".robo-field-control::selection");
        paint(state[0], state[1], state[2], "-selection", "background", ".robo-field-control::-moz-selection");
        paint(state[0], state[1], state[2], "-selection-text", "color", ".robo-field-control::selection");
        paint(state[0], state[1], state[2], "-selection-text", "color", ".robo-field-control::-moz-selection");
        paint(state[0], state[1], state[2], "-icon-size", "height", ".robo-field-icon");
        paint(state[0], state[1], state[2], "-icon-size", "width", ".robo-field-icon svg");
        paint(state[0], state[1], state[2], "-icon-size", "height", ".robo-field-icon svg");
    }
    css << ".robo-field[style*=\"--field-icon-size\"] .robo-field-icon,.robo-field[style*=\"--field-icon-size\"] "
           ".robo-field-icon svg{height:var(--field-icon-size)}\n";
    css << ".robo-field-caret{display:none;position:absolute;z-index:2;width:var(--field-caret-width,2px);"
           "background:var(--field-caret,currentColor);pointer-events:none;border-radius:1px}\n";
    css << ".robo-field[data-caret=\"block\"] .robo-field-caret{width:var(--field-caret-width,1ch)}\n";
    css << ".robo-field:focus-within>.robo-field-caret,.robo-field:has(.robo-field-control[data-preview=\"focus\"])>"
           ".robo-field-caret{display:block}\n";
    css << ".robo-field[style*=\"--field-caret-width\"] .robo-field-control,.robo-field[style*=\"--field-caret-blink\"] "
           ".robo-field-control,.robo-field[data-caret] .robo-field-control{caret-color:transparent !important}\n";
    css << ".robo-field[style*=\"--field-caret-blink\"] .robo-field-caret{animation:robo-field-caret "
           "var(--field-caret-blink) steps(1,end) infinite}\n";
    css << "@keyframes robo-field-caret{0%,49%{opacity:1}50%,100%{opacity:0}}\n";
}

const char* RoboWebTextFieldClass() { return "robo-textfield"; }
