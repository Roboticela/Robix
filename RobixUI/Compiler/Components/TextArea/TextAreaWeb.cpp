#include "TextAreaWeb.hpp"

#include "../TextField/TextFieldWeb.hpp"
#include "../../RoboCompilerShared.hpp"

#include <string>

namespace fs = std::filesystem;

namespace {

bool RoboWebAreaFlag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

std::string RoboWebAreaVariant(const RoboXMLNode& node) {
  const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "outline"));
  if (variant == "subtle" || variant == "ghost" || variant == "underline" || variant == "well" ||
      variant == "composer") {
    return variant;
  }
  return "outline";
}

std::string RoboWebAreaSize(const RoboXMLNode& node) {
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "md"));
  if (size == "sm" || size == "small") {
    return "sm";
  }
  if (size == "lg" || size == "large") {
    return "lg";
  }
  return "md";
}

std::string RoboWebAreaResize(const std::string& variant, const std::string& requested) {
  const std::string resize = RoboWebLower(requested);
  if (resize == "none" || resize == "false" || resize == "0") {
    return "none";
  }
  if (resize == "y" || resize == "vertical") {
    return "y";
  }
  return (variant == "well" || variant == "composer") ? "none" : "y";
}

std::string RoboWebAreaTone(std::size_t length, int maxLength) {
  if (maxLength < 0) {
    return "tertiary";
  }
  if (length >= static_cast<std::size_t>(maxLength)) {
    return "danger";
  }
  if (length * 10 >= static_cast<std::size_t>(maxLength) * 8) {
    return "warning";
  }
  return "tertiary";
}

}  // namespace

void RoboWebEmitTextArea(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                         const fs::path& assetsRoot) {
  const std::string id = RoboXMLAttr(node, "id");
  const std::string variant = RoboWebAreaVariant(node);
  const std::string size = RoboWebAreaSize(node);
  const std::string resize = RoboWebAreaResize(variant, RoboXMLAttr(node, "resize"));
  const std::string value = RoboXMLAttr(node, "value", RoboXMLAttr(node, "content").c_str());
  const std::string placeholder = RoboXMLAttr(node, "placeholder");
  const std::string label = RoboXMLAttr(node, "label");
  const std::string hint = RoboXMLAttr(node, "hint");
  const std::string preview = RoboWebLower(RoboXMLAttr(node, "preview"));
  const std::string rows = RoboXMLAttr(node, "rows");
  const std::string maxLengthText = RoboXMLAttr(node, "maxLength");
  std::string icon = RoboXMLAttr(node, "icon");
  for (const RoboXMLNode& child : node.children) {
    if (RoboWebLower(child.name) != "icon") {
      continue;
    }
    const std::string childIcon = RoboXMLAttr(child, "name", RoboXMLAttr(child, "icon").c_str());
    if (!childIcon.empty()) {
      icon = childIcon;
    }
  }
  const bool invalid = RoboWebAreaFlag(RoboXMLAttr(node, "invalid"));
  const bool valid = !invalid && RoboWebAreaFlag(RoboXMLAttr(node, "valid"));
  const bool disabled = RoboWebAreaFlag(RoboXMLAttr(node, "disabled"));
  const bool readOnly = RoboWebAreaFlag(RoboXMLAttr(node, "readOnly")) || RoboWebAreaFlag(RoboXMLAttr(node, "readonly"));
  const bool required = RoboWebAreaFlag(RoboXMLAttr(node, "required"));
  const bool optional = RoboWebAreaFlag(RoboXMLAttr(node, "optional"));
  const bool composer = variant == "composer";
  int maxLength = -1;
  if (!maxLengthText.empty()) {
    maxLength = 0;
    for (char digit : maxLengthText) {
      if (digit < '0' || digit > '9') {
        maxLength = -1;
        break;
      }
      maxLength = maxLength * 10 + (digit - '0');
    }
  }
  const bool showCount = RoboWebAreaFlag(RoboXMLAttr(node, "count")) || maxLength >= 0;
  RoboXMLNode styled = node;
  for (auto it = styled.attrs.begin(); it != styled.attrs.end();) {
    if (RoboWebLower(it->first) == "rows") {
      it = styled.attrs.erase(it);
    } else {
      ++it;
    }
  }
  RoboWebStripFieldChrome(styled);
  const std::string style = RoboWebStyle(styled, library);
  std::string fieldStyle;
  RoboWebAppendFieldStyle(fieldStyle, node, library);
  const std::string caretShape = RoboWebLower(RoboXMLAttr(node, "caretShape", RoboXMLAttr(node, "cursorShape").c_str()));

  html += "<div class=\"robo-form-field\" data-size=\"";
  html += RoboWebEscape(size);
  html += "\"";
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

  if (composer) {
    html += "<div class=\"robo-area-composer\" data-variant=\"composer\" data-size=\"";
    html += RoboWebEscape(size);
    html += "\"";
    if (invalid) {
      html += " data-invalid=\"true\"";
    } else if (valid) {
      html += " data-valid=\"true\"";
    }
    if (preview == "hover" || preview == "focus") {
      html += " data-preview=\"";
      html += RoboWebEscape(preview);
      html += "\"";
    }
    html += ">";
  }

  html += "<label class=\"robo-field robo-area\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\" data-size=\"";
  html += RoboWebEscape(size);
  html += "\"";
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
  if (showCount && !composer) {
    html += " data-count=\"true\"";
  }
  html += ">";
  if (!icon.empty()) {
    html += "<span class=\"robo-field-icon\">";
    html += RoboWebSvg(assetsRoot, icon);
    html += "</span>";
  }
  html += "<textarea class=\"robo-text-area robo-field-control\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\" data-size=\"";
  html += RoboWebEscape(size);
  html += "\" data-resize=\"";
  html += resize;
  html += "\"";
  if (!id.empty()) {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  if (!rows.empty()) {
    html += " rows=\"";
    html += RoboWebEscape(rows);
    html += "\"";
  }
  if (!placeholder.empty()) {
    html += " placeholder=\"";
    html += RoboWebEscape(placeholder);
    html += "\"";
  }
  if (maxLength >= 0) {
    html += " maxlength=\"";
    html += RoboWebEscape(maxLengthText);
    html += "\"";
  }
  if (preview == "hover" || preview == "focus") {
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
  if (required && !optional) {
    html += " required";
  }
  html += ">";
  html += RoboWebEscape(value);
  html += "</textarea>";
  if (RoboWebFieldWantsCaret(node)) {
    html += "<span class=\"robo-field-caret\" aria-hidden=\"true\"></span>";
  }
  if (showCount && !composer) {
    html += "<span class=\"robo-area-count\" data-tone=\"";
    html += RoboWebAreaTone(value.size(), maxLength);
    html += "\"";
    if (maxLength >= 0) {
      html += " data-max=\"";
      html += RoboWebEscape(maxLengthText);
      html += "\"";
    }
    html += ">";
    html += std::to_string(value.size());
    if (maxLength >= 0) {
      html += " / ";
      html += RoboWebEscape(maxLengthText);
    }
    html += "</span>";
  }
  html += "</label>";

  if (composer) {
    html += "<span class=\"robo-area-footer\">";
    html += "<span class=\"robo-field-hint\">";
    html += RoboWebEscape(hint);
    html += "</span>";
    if (showCount) {
      html += "<span class=\"robo-area-count\" data-tone=\"";
      html += RoboWebAreaTone(value.size(), maxLength);
      html += "\"";
      if (maxLength >= 0) {
        html += " data-max=\"";
        html += RoboWebEscape(maxLengthText);
        html += "\"";
      }
      html += ">";
      html += std::to_string(value.size());
      if (maxLength >= 0) {
        html += " / ";
        html += RoboWebEscape(maxLengthText);
      }
      html += "</span>";
    }
    html += "</span></div>";
  }
  html += "</div>";
}

void RoboWebAppendTextAreaCss(std::ostream& css) {
  css << ".robo-text-area{box-sizing:border-box;display:block;width:100%;min-width:0;max-width:100%;margin:0;"
         "padding:12px 16px;min-height:120px;border:1px solid var(--border-default);border-radius:12px;"
         "background:var(--bg-secondary);color:var(--text-primary);caret-color:var(--text-primary);"
         "font:400 17px/1.42 var(--font-sans,system-ui,sans-serif);outline:2px solid transparent;outline-offset:1px;"
         "resize:vertical;transition:border-color 150ms cubic-bezier(0.28,0.11,0.32,1),background-color 150ms "
         "cubic-bezier(0.28,0.11,0.32,1),outline-color 150ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << ".robo-text-area::placeholder{color:var(--text-tertiary)}\n";
  css << ".robo-text-area[data-size=\"sm\"]{min-height:88px;padding:10px 12px;font-size:15px;line-height:1.47}\n";
  css << ".robo-text-area[data-size=\"lg\"]{min-height:180px;padding:16px 20px}\n";
  css << ".robo-text-area[data-resize=\"none\"]{resize:none}\n";
  css << ".robo-text-area[data-resize=\"y\"]{resize:vertical}\n";
  css << ".robo-area .robo-field-icon{top:14px;left:16px;width:18px;height:18px;transform:none}\n";
  css << ".robo-area .robo-field-icon svg{width:18px;height:18px;display:block}\n";
  css << ".robo-form-field[data-size=\"sm\"] .robo-area .robo-field-icon{top:10px;left:12px}\n";
  css << ".robo-form-field[data-size=\"lg\"] .robo-area .robo-field-icon{top:16px;left:16px}\n";
  css << ".robo-area:has(.robo-field-icon) .robo-text-area{padding-left:44px}\n";
  css << ".robo-form-field[data-size=\"sm\"] .robo-area:has(.robo-field-icon) .robo-text-area{padding-left:36px}\n";
  css << ".robo-form-field[data-size=\"lg\"] .robo-area:has(.robo-field-icon) .robo-text-area{padding-left:48px}\n";
  css << ".robo-area[data-count=\"true\"] .robo-text-area{padding-bottom:36px}\n";
  css << ".robo-form-field[data-size=\"sm\"] .robo-area[data-count=\"true\"] .robo-text-area{padding-bottom:32px}\n";
  css << ".robo-form-field[data-size=\"lg\"] .robo-area[data-count=\"true\"] .robo-text-area{padding-bottom:40px}\n";
  css << ".robo-area-count{color:var(--text-tertiary);font:400 13px/1.35 var(--font-sans,system-ui,sans-serif);"
         "font-variant-numeric:tabular-nums;pointer-events:none}\n";
  css << ".robo-area .robo-area-count{position:absolute;right:16px;bottom:12px;z-index:1}\n";
  css << ".robo-form-field[data-size=\"sm\"] .robo-area .robo-area-count{right:12px;bottom:8px}\n";
  css << ".robo-area-count[data-tone=\"warning\"]{color:var(--warning)}\n";
  css << ".robo-area-count[data-tone=\"danger\"]{color:var(--danger)}\n";
  css << ".robo-area:has(.robo-text-area:not(:placeholder-shown)) .robo-field-icon{color:var(--text-primary)}\n";
  css << ".robo-area:hover .robo-field-icon{color:var(--text-secondary)}\n";
  css << ".robo-area:focus-within .robo-field-icon{color:var(--brand-primary)}\n";
  css << ".robo-area:has([data-preview=\"hover\"]) .robo-field-icon{color:var(--text-secondary)}\n";
  css << ".robo-area:has([data-preview=\"focus\"]) .robo-field-icon{color:var(--brand-primary)}\n";
  css << ".robo-area:has([data-valid=\"true\"]) .robo-field-icon{color:var(--success)}\n";
  css << ".robo-area:has([aria-invalid=\"true\"]) .robo-field-icon{color:var(--danger)}\n";
  css << ".robo-area:has(:disabled) .robo-field-icon{color:var(--text-disabled)}\n";
  css << "html[data-theme=\"light\"] .robo-text-area[data-variant=\"outline\"]{background:var(--bg-elevated)}\n";
  css << ".robo-text-area[data-variant=\"outline\"]:hover,.robo-text-area[data-variant=\"outline\"][data-preview=\"hover\"]"
         "{border-color:var(--border-strong)}\n";
  css << ".robo-text-area[data-variant=\"outline\"]:focus,.robo-text-area[data-variant=\"outline\"][data-preview=\"focus\"]"
         "{border-color:var(--brand-primary);outline-color:var(--brand-primary-focus-ring)}\n";
  css << ".robo-text-area[data-variant=\"subtle\"]{border-color:transparent;background:var(--bg-tertiary)}\n";
  css << ".robo-text-area[data-variant=\"ghost\"]{border-color:transparent;background:transparent}\n";
  css << ".robo-text-area[data-variant=\"subtle\"]:hover,.robo-text-area[data-variant=\"ghost\"]:hover,"
         ".robo-text-area[data-variant=\"subtle\"][data-preview=\"hover\"],"
         ".robo-text-area[data-variant=\"ghost\"][data-preview=\"hover\"]{background:var(--bg-hover)}\n";
  css << ".robo-text-area[data-variant=\"subtle\"]:focus,.robo-text-area[data-variant=\"ghost\"]:focus,"
         ".robo-text-area[data-variant=\"subtle\"][data-preview=\"focus\"],"
         ".robo-text-area[data-variant=\"ghost\"][data-preview=\"focus\"]{background:var(--bg-secondary);"
         "border-color:var(--brand-primary);outline-color:var(--brand-primary-focus-ring)}\n";
  css << "html[data-theme=\"light\"] .robo-text-area[data-variant=\"subtle\"]:focus,"
         "html[data-theme=\"light\"] .robo-text-area[data-variant=\"ghost\"]:focus,"
         "html[data-theme=\"light\"] .robo-text-area[data-variant=\"subtle\"][data-preview=\"focus\"],"
         "html[data-theme=\"light\"] .robo-text-area[data-variant=\"ghost\"][data-preview=\"focus\"]"
         "{background:var(--bg-elevated)}\n";
  css << ".robo-text-area[data-variant=\"underline\"]{min-height:88px;border:0;border-bottom:1px solid "
         "var(--border-default);border-radius:0;background:transparent;outline:none}\n";
  css << ".robo-text-area[data-variant=\"underline\"][data-size=\"md\"]{min-height:120px}\n";
  css << ".robo-text-area[data-variant=\"underline\"][data-size=\"lg\"]{min-height:180px}\n";
  css << ".robo-text-area[data-variant=\"underline\"]:hover,.robo-text-area[data-variant=\"underline\"]"
         "[data-preview=\"hover\"]{border-bottom-color:var(--border-strong)}\n";
  css << ".robo-text-area[data-variant=\"underline\"]:focus,.robo-text-area[data-variant=\"underline\"]"
         "[data-preview=\"focus\"]{border-bottom-color:var(--brand-primary);outline:none}\n";
  css << ".robo-text-area[data-variant=\"well\"]{border-color:transparent;border-radius:18px;background:var(--bg-tertiary)}\n";
  css << "html[data-theme=\"light\"] .robo-text-area[data-variant=\"well\"]{background:var(--bg-secondary)}\n";
  css << ".robo-text-area[data-variant=\"well\"][data-size=\"sm\"]{padding:12px 16px}\n";
  css << ".robo-text-area[data-variant=\"well\"][data-size=\"md\"],.robo-text-area[data-variant=\"well\"][data-size=\"lg\"]"
         "{padding:20px 24px}\n";
  css << ".robo-text-area[data-variant=\"well\"][data-size=\"lg\"]{padding:24px}\n";
  css << ".robo-form-field[data-size=\"sm\"] .robo-area[data-variant=\"well\"] .robo-field-icon{top:12px;left:16px}\n";
  css << ".robo-form-field[data-size=\"md\"] .robo-area[data-variant=\"well\"] .robo-field-icon{top:20px;left:24px}\n";
  css << ".robo-form-field[data-size=\"lg\"] .robo-area[data-variant=\"well\"] .robo-field-icon{top:24px;left:24px}\n";
  css << ".robo-form-field[data-size=\"sm\"] .robo-area[data-variant=\"well\"]:has(.robo-field-icon) .robo-text-area"
         "{padding-left:40px}\n";
  css << ".robo-form-field[data-size=\"md\"] .robo-area[data-variant=\"well\"]:has(.robo-field-icon) .robo-text-area,"
         ".robo-form-field[data-size=\"lg\"] .robo-area[data-variant=\"well\"]:has(.robo-field-icon) .robo-text-area"
         "{padding-left:56px}\n";
  css << ".robo-area[data-variant=\"well\"] .robo-area-count{right:24px;bottom:20px}\n";
  css << ".robo-text-area[data-variant=\"well\"]:hover{background:var(--bg-hover)}\n";
  css << "html[data-theme=\"light\"] .robo-text-area[data-variant=\"well\"]:hover,"
         "html[data-theme=\"light\"] .robo-text-area[data-variant=\"well\"][data-preview=\"hover\"]"
         "{background:var(--bg-tertiary)}\n";
  css << ".robo-text-area[data-variant=\"well\"][data-preview=\"hover\"]{background:var(--bg-hover)}\n";
  css << "html[data-theme=\"light\"] .robo-text-area[data-variant=\"well\"][data-preview=\"hover\"]{background:var(--bg-tertiary)}\n";
  css << ".robo-text-area[data-variant=\"well\"]:focus,.robo-text-area[data-variant=\"well\"][data-preview=\"focus\"]"
         "{background:var(--bg-secondary);border-color:var(--brand-primary);outline-color:var(--brand-primary-focus-ring)}\n";
  css << "html[data-theme=\"light\"] .robo-text-area[data-variant=\"well\"]:focus,"
         "html[data-theme=\"light\"] .robo-text-area[data-variant=\"well\"][data-preview=\"focus\"]"
         "{background:var(--bg-elevated)}\n";
  css << ".robo-area-composer{display:flex;flex-direction:column;width:100%;min-width:0;overflow:hidden;"
         "border:1px solid var(--border-default);border-radius:18px;background:var(--bg-secondary);"
         "outline:2px solid transparent;outline-offset:1px;"
         "transition:border-color 150ms cubic-bezier(0.28,0.11,0.32,1),background-color 150ms "
         "cubic-bezier(0.28,0.11,0.32,1),outline-color 150ms cubic-bezier(0.28,0.11,0.32,1)}\n";
  css << "html[data-theme=\"light\"] .robo-area-composer{background:var(--bg-elevated)}\n";
  css << ".robo-area-composer .robo-text-area{border:0;border-radius:0;background:transparent;outline:none;box-shadow:none}\n";
  css << ".robo-area-composer:hover,.robo-area-composer[data-preview=\"hover\"]{border-color:var(--border-strong)}\n";
  css << ".robo-area-composer:focus-within,.robo-area-composer[data-preview=\"focus\"]{border-color:var(--brand-primary);"
         "outline-color:var(--brand-primary-focus-ring)}\n";
  css << ".robo-area-footer{display:flex;align-items:center;justify-content:space-between;gap:12px;"
         "border-top:1px solid var(--border-subtle);padding:10px 16px}\n";
  css << ".robo-area-footer .robo-field-hint{margin:0;min-width:0}\n";
  css << ".robo-area-footer .robo-area-count{position:static}\n";
  css << ".robo-text-area[aria-invalid=\"true\"]{border-color:var(--danger);outline-color:transparent}\n";
  css << ".robo-text-area[aria-invalid=\"true\"]:hover{border-color:var(--danger)}\n";
  css << ".robo-text-area[aria-invalid=\"true\"]:focus,.robo-text-area[aria-invalid=\"true\"][data-preview=\"focus\"]"
         "{border-color:var(--danger);outline-color:var(--danger-focus-ring)}\n";
  css << ".robo-text-area[data-valid=\"true\"]{border-color:var(--success);outline-color:transparent}\n";
  css << ".robo-text-area[data-valid=\"true\"]:focus,.robo-text-area[data-valid=\"true\"][data-preview=\"focus\"]"
         "{border-color:var(--success);outline-color:var(--success-focus-ring)}\n";
  css << ".robo-area-composer[data-invalid=\"true\"]{border-color:var(--danger);outline-color:transparent}\n";
  css << ".robo-area-composer[data-invalid=\"true\"]:focus-within,.robo-area-composer[data-invalid=\"true\"]"
         "[data-preview=\"focus\"]{border-color:var(--danger);outline-color:var(--danger-focus-ring)}\n";
  css << ".robo-area-composer[data-valid=\"true\"]{border-color:var(--success);outline-color:transparent}\n";
  css << ".robo-area-composer[data-valid=\"true\"]:focus-within,.robo-area-composer[data-valid=\"true\"]"
         "[data-preview=\"focus\"]{border-color:var(--success);outline-color:var(--success-focus-ring)}\n";
  css << ".robo-area:has(:disabled),.robo-area-composer:has(:disabled){opacity:0.55}\n";
  css << ".robo-area-composer .robo-area:has(:disabled){opacity:1}\n";
  css << ".robo-text-area:disabled{cursor:not-allowed}\n";
  css << ".robo-text-area[readonly]{color:var(--text-secondary)}\n";
}

const char* RoboWebTextAreaClass() { return "robo-textarea"; }

const char* RoboWebTextAreaScript() {
  return R"JS(
  function areaTone(len, max) {
    if (max == null || max < 0) return "tertiary";
    if (len >= max) return "danger";
    if (len >= max * 0.8) return "warning";
    return "tertiary";
  }
  function paintArea(area) {
    var root = area.closest(".robo-form-field");
    if (!root) return;
    var count = root.querySelector(".robo-area-count");
    if (!count) return;
    var maxAttr = count.getAttribute("data-max");
    var max = maxAttr == null || maxAttr === "" ? null : Number(maxAttr);
    var len = area.value.length;
    count.textContent = max == null ? String(len) : (len + " / " + max);
    count.setAttribute("data-tone", areaTone(len, max));
  }
  document.addEventListener("input", function (ev) {
    var area = ev.target;
    if (!area || !area.classList || !area.classList.contains("robo-text-area")) return;
    paintArea(area);
  });
  var areas = document.querySelectorAll(".robo-text-area");
  for (var i = 0; i < areas.length; i++) paintArea(areas[i]);
)JS";
}
