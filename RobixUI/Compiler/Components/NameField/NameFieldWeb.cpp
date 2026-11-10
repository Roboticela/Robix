#include "NameFieldWeb.hpp"

#include "../TextField/TextFieldWeb.hpp"

#include "../../RoboCompilerShared.hpp"

namespace fs = std::filesystem;

namespace {

std::string RoboWebNameSize(const RoboXMLNode& node) {
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "md"));
  if (size == "sm" || size == "small") {
    return "sm";
  }
  if (size == "lg" || size == "large") {
    return "lg";
  }
  return "md";
}

std::string RoboWebNameVariant(const RoboXMLNode& node) {
  const std::string variant = RoboWebLower(RoboXMLAttr(node, "variant", "full"));
  if (variant == "firstlast" || variant == "first-last") {
    return "firstLast";
  }
  if (variant == "firstmiddlelast" || variant == "first-middle-last" || variant == "complete") {
    return "firstMiddleLast";
  }
  return "full";
}

std::string RoboWebNameAppearance(const RoboXMLNode& node) {
  const std::string appearance = RoboWebLower(RoboXMLAttr(node, "appearance", "outline"));
  if (appearance == "subtle" || appearance == "ghost" || appearance == "underline" || appearance == "pill") {
    return appearance;
  }
  return "outline";
}

std::string RoboWebNameDirection(const RoboXMLNode& node) {
  const std::string direction = RoboWebLower(RoboXMLAttr(node, "direction", "column"));
  if (direction == "row" || direction == "row-reverse") {
    return "row";
  }
  return "column";
}

std::string RoboWebNameAlign(const RoboXMLNode& node) {
  const std::string align = RoboWebLower(RoboXMLAttr(node, "align", "stretch"));
  if (align == "start" || align == "center" || align == "end") {
    return align;
  }
  return "stretch";
}

void RoboWebNamePart(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                     const fs::path& assetsRoot, const std::string& id, const std::string& label,
                     const std::string& value, const std::string& placeholder, const std::string& complete) {
  RoboXMLNode field;
  field.name = "TextField";
  field.attrs["variant"] = RoboWebNameAppearance(node);
  field.attrs["size"] = RoboWebNameSize(node);
  field.attrs["sizedIcon"] = "true";
  field.attrs["type"] = "text";
  field.attrs["label"] = label;
  field.attrs["placeholder"] = placeholder;
  field.attrs["autoComplete"] = complete;
  field.attrs["autoCapitalize"] = "words";
  field.attrs["autoCorrect"] = "off";
  field.attrs["spellCheck"] = "false";
  const std::string icon = RoboXMLAttr(node, "icon");
  field.attrs["icon"] = icon.empty() ? "PersonUserProfile" : icon;
  if (!id.empty()) {
    field.attrs["id"] = id;
  }
  if (!value.empty()) {
    field.attrs["value"] = value;
  }
  const char* copies[] = {"preview", "invalid", "valid", "disabled", "readOnly", "required", "optional"};
  for (const char* key : copies) {
    const std::string copy = RoboXMLAttr(node, key);
    if (!copy.empty()) {
      field.attrs[key] = copy;
    }
  }
  const char* skip[] = {"variant", "appearance", "direction", "align", "gap", "justify", "label", "firstlabel",
                        "lastlabel", "middlelabel", "fulllabel", "first", "last", "middle", "value", "content",
                        "placeholder", "id", "icon", "size", "name"};
  for (const auto& attr : node.attrs) {
    const std::string key = RoboWebLower(attr.first);
    bool skipped = false;
    for (const char* name : skip) {
      if (key == name) skipped = true;
    }
    if (skipped || !RoboXMLAttr(field, attr.first.c_str()).empty()) continue;
    field.attrs[attr.first] = attr.second;
  }
  RoboWebEmitTextField(html, field, library, assetsRoot);
}

}  // namespace

void RoboWebEmitNameField(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                          const fs::path& assetsRoot) {
  const std::string variant = RoboWebNameVariant(node);
  const std::string direction = RoboWebNameDirection(node);
  const std::string align = RoboWebNameAlign(node);
  const std::string id = RoboXMLAttr(node, "id");
  const std::string group = RoboXMLAttr(node, "label");
  RoboXMLNode chrome = node;
  chrome.attrs.erase("direction");
  chrome.attrs.erase("align");
  chrome.attrs.erase("gap");
  chrome.attrs.erase("justify");
  RoboWebStripFieldChrome(chrome);
  const std::string style = RoboWebStyle(chrome, library);
  const std::string gap = RoboWebCssValue(library, RoboXMLAttr(node, "gap"), false);

  html += "<div class=\"robo-name\" data-variant=\"";
  html += RoboWebEscape(variant);
  html += "\" data-direction=\"";
  html += RoboWebEscape(direction);
  html += "\" data-align=\"";
  html += RoboWebEscape(align);
  html += "\"";
  if (!id.empty() && variant != "full") {
    html += " id=\"";
    html += RoboWebEscape(id);
    html += "\"";
  }
  if (!style.empty()) {
    html += " style=\"";
    html += RoboWebEscape(style);
    html += "\"";
  }
  html += ">";
  if (variant != "full" && !group.empty()) {
    html += "<span class=\"robo-name-label\">";
    html += RoboWebEscape(group);
    html += "</span>";
  }
  html += "<div class=\"robo-name-parts\"";
  if (!gap.empty()) {
    html += " style=\"gap:";
    html += RoboWebEscape(gap);
    html += "\"";
  }
  html += ">";
  if (variant == "full") {
    const std::string fullLabel = RoboXMLAttr(node, "fullLabel");
    RoboWebNamePart(html, node, library, assetsRoot, id, fullLabel.empty() ? group : fullLabel,
                    RoboXMLAttr(node, "value", RoboXMLAttr(node, "content").c_str()),
                    RoboXMLAttr(node, "placeholder", "Full name"), "name");
  } else {
    const std::string firstLabel = RoboXMLAttr(node, "firstLabel", "First name");
    const std::string lastLabel = RoboXMLAttr(node, "lastLabel", "Last name");
    RoboWebNamePart(html, node, library, assetsRoot, id.empty() ? "" : id + "First", firstLabel,
                    RoboXMLAttr(node, "first"), firstLabel, "given-name");
    if (variant == "firstMiddleLast") {
      const std::string middleLabel = RoboXMLAttr(node, "middleLabel", "Middle name");
      RoboWebNamePart(html, node, library, assetsRoot, id.empty() ? "" : id + "Middle", middleLabel,
                      RoboXMLAttr(node, "middle"), middleLabel, "additional-name");
    }
    RoboWebNamePart(html, node, library, assetsRoot, id.empty() ? "" : id + "Last", lastLabel,
                    RoboXMLAttr(node, "last"), lastLabel, "family-name");
  }
  html += "</div></div>";
}

void RoboWebAppendNameFieldCss(std::ostream& css) {
  css << ".robo-name{display:flex;flex-direction:column;width:100%;min-width:0;box-sizing:border-box;gap:8px}\n";
  css << ".robo-name-label{color:var(--text-secondary);font-size:17px;font-weight:500;line-height:1.35}\n";
  css << ".robo-name-parts{display:flex;flex-direction:column;align-items:stretch;gap:12px;width:100%;min-width:0}\n";
  css << ".robo-name[data-direction=\"row\"] .robo-name-parts{flex-direction:row;flex-wrap:wrap;align-items:flex-start}\n";
  css << ".robo-name[data-direction=\"row\"] .robo-name-parts>.robo-form-field{flex:1 1 160px;min-width:160px}\n";
  css << ".robo-name[data-align=\"start\"] .robo-name-parts{align-items:flex-start}\n";
  css << ".robo-name[data-align=\"center\"] .robo-name-parts{align-items:center}\n";
  css << ".robo-name[data-align=\"end\"] .robo-name-parts{align-items:flex-end}\n";
  css << ".robo-name[data-align=\"stretch\"] .robo-name-parts{align-items:stretch}\n";
  css << ".robo-name[data-direction=\"row\"][data-align=\"stretch\"] .robo-name-parts>.robo-form-field{align-self:stretch}\n";
  css << "@media (max-width:734px){.robo-name[data-direction=\"row\"] .robo-name-parts{flex-direction:column}"
         ".robo-name[data-direction=\"row\"] .robo-name-parts>.robo-form-field{flex-basis:auto;min-width:0;width:100%}}\n";
}

const char* RoboWebNameFieldClass() { return "robo-name"; }
