#include "PhoneFieldWeb.hpp"

#include "../TextField/TextFieldWeb.hpp"

#include "../../RoboCompilerShared.hpp"

namespace fs = std::filesystem;

namespace {

std::string RoboWebPhoneSize(const RoboXMLNode& node) {
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "md"));
  if (size == "sm" || size == "small") {
    return "sm";
  }
  if (size == "lg" || size == "large") {
    return "lg";
  }
  return "md";
}

void RoboWebPhoneDefault(RoboXMLNode& node, const char* key, const std::string& value) {
  if (RoboXMLAttr(node, key).empty()) {
    node.attrs[key] = value;
  }
}

}  // namespace

void RoboWebEmitPhoneField(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                           const fs::path& assetsRoot) {
  RoboXMLNode field = node;
  field.name = "TextField";
  field.attrs["size"] = RoboWebPhoneSize(node);
  field.attrs["phone"] = "true";
  RoboWebPhoneDefault(field, "type", "tel");
  RoboWebPhoneDefault(field, "placeholder", "Phone number");
  RoboWebPhoneDefault(field, "icon", "HandheldMobilePhone");
  RoboWebPhoneDefault(field, "autoComplete", "tel");
  RoboWebPhoneDefault(field, "autoCapitalize", "none");
  RoboWebPhoneDefault(field, "autoCorrect", "off");
  RoboWebPhoneDefault(field, "spellCheck", "false");
  RoboWebPhoneDefault(field, "inputMode", "tel");
  RoboWebEmitTextField(html, field, library, assetsRoot);
}

const char* RoboWebPhoneFieldClass() { return "robo-phone-field"; }
