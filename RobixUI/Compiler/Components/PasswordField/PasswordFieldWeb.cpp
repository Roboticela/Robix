#include "PasswordFieldWeb.hpp"

#include "../TextField/TextFieldWeb.hpp"

#include "../../RoboCompilerShared.hpp"

namespace fs = std::filesystem;

namespace {

bool RoboWebPasswordFlag(const std::string& value) {
  const std::string flag = RoboWebLower(value);
  return flag == "true" || flag == "1" || flag == "yes";
}

std::string RoboWebPasswordSize(const RoboXMLNode& node) {
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "md"));
  if (size == "sm" || size == "small") {
    return "sm";
  }
  if (size == "lg" || size == "large") {
    return "lg";
  }
  return "md";
}

std::string RoboWebPasswordReveal(const RoboXMLNode& node) {
  const std::string reveal = RoboWebLower(RoboXMLAttr(node, "reveal", "text"));
  if (reveal == "ghost" || reveal == "subtle" || reveal == "outline" || reveal == "pill") {
    return reveal;
  }
  return "text";
}

void RoboWebPasswordDefault(RoboXMLNode& node, const char* key, const std::string& value) {
  if (RoboXMLAttr(node, key).empty()) {
    node.attrs[key] = value;
  }
}

}  // namespace

void RoboWebEmitPasswordField(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                              const fs::path& assetsRoot) {
  RoboXMLNode field = node;
  field.name = "TextField";
  const std::string size = RoboWebPasswordSize(node);
  const std::string revealName = RoboWebPasswordReveal(node);
  const bool visible = RoboWebPasswordFlag(RoboXMLAttr(node, "visible"));
  const bool disabled = RoboWebPasswordFlag(RoboXMLAttr(node, "disabled"));
  field.attrs["size"] = size;
  field.attrs["password"] = "true";
  if (RoboXMLAttr(field, "type").empty()) {
    field.attrs["type"] = visible ? "text" : "password";
  }
  RoboWebPasswordDefault(field, "placeholder", "Password");
  RoboWebPasswordDefault(field, "icon", "PadlockSecureClosed");
  RoboWebPasswordDefault(field, "autoComplete", "current-password");
  RoboWebPasswordDefault(field, "autoCapitalize", "none");
  RoboWebPasswordDefault(field, "autoCorrect", "off");
  RoboWebPasswordDefault(field, "spellCheck", "false");

  bool hasReveal = false;
  for (const RoboXMLNode& child : field.children) {
    if (RoboWebLower(child.name) == "passwordreveal") {
      hasReveal = true;
    }
  }
  if (!hasReveal) {
    RoboXMLNode reveal;
    reveal.name = "PasswordReveal";
    reveal.attrs["variant"] = revealName;
    reveal.attrs["size"] = size;
    if (visible) {
      reveal.attrs["visible"] = "true";
    }
    if (disabled) {
      reveal.attrs["disabled"] = "true";
    }
    const std::string id = RoboXMLAttr(node, "id");
    if (!id.empty()) {
      reveal.attrs["id"] = id + "Reveal";
    }
    field.children.push_back(std::move(reveal));
  }
  RoboWebEmitTextField(html, field, library, assetsRoot);
}

void RoboWebAppendPasswordFieldCss(std::ostream& css) {
  css << ".robo-form-field[data-password=\"true\"][data-size=\"sm\"] .robo-field-icon,"
         ".robo-form-field[data-password=\"true\"][data-size=\"sm\"] .robo-field-icon svg{width:16px;height:16px}\n";
  css << ".robo-form-field[data-password=\"true\"][data-size=\"md\"] .robo-field-icon,"
         ".robo-form-field[data-password=\"true\"][data-size=\"md\"] .robo-field-icon svg{width:18px;height:18px}\n";
  css << ".robo-form-field[data-password=\"true\"][data-size=\"lg\"] .robo-field-icon,"
         ".robo-form-field[data-password=\"true\"][data-size=\"lg\"] .robo-field-icon svg{width:20px;height:20px}\n";
}

const char* RoboWebPasswordFieldClass() { return "robo-password-field"; }
