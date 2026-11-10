#include "TextFieldWeb.hpp"

#include "../PasswordReveal/PasswordRevealWeb.hpp"

namespace fs = std::filesystem;

namespace {

bool RoboWebFieldFlag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes";
}

}  // namespace

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
  const std::string style = RoboWebStyle(node, library);
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
  html += "<label class=\"robo-field\">";
  if (!icon.empty()) {
    html += "<span class=\"robo-field-icon\">";
    html += RoboWebSvg(assetsRoot, icon);
    html += "</span>";
  }
  html += "<input class=\"robo-text-field\" data-variant=\"";
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
}

const char* RoboWebTextFieldClass() { return "robo-textfield"; }
