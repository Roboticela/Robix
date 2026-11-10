#include "EmailFieldWeb.hpp"

#include "../TextField/TextFieldWeb.hpp"

#include "../../RoboCompilerShared.hpp"

namespace fs = std::filesystem;

namespace {

std::string RoboWebEmailSize(const RoboXMLNode& node) {
  const std::string size = RoboWebLower(RoboXMLAttr(node, "size", "md"));
  if (size == "sm" || size == "small") {
    return "sm";
  }
  if (size == "lg" || size == "large") {
    return "lg";
  }
  return "md";
}

void RoboWebEmailDefault(RoboXMLNode& node, const char* key, const std::string& value) {
  if (RoboXMLAttr(node, key).empty()) {
    node.attrs[key] = value;
  }
}

}  // namespace

void RoboWebEmitEmailField(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                           const fs::path& assetsRoot) {
  RoboXMLNode field = node;
  field.name = "TextField";
  field.attrs["size"] = RoboWebEmailSize(node);
  field.attrs["email"] = "true";
  RoboWebEmailDefault(field, "type", "email");
  RoboWebEmailDefault(field, "placeholder", "Email address");
  RoboWebEmailDefault(field, "icon", "EnvelopeMessageMail");
  RoboWebEmailDefault(field, "autoComplete", "email");
  RoboWebEmailDefault(field, "autoCapitalize", "none");
  RoboWebEmailDefault(field, "autoCorrect", "off");
  RoboWebEmailDefault(field, "spellCheck", "false");
  RoboWebEmailDefault(field, "inputMode", "email");
  RoboWebEmitTextField(html, field, library, assetsRoot);
}

const char* RoboWebEmailFieldClass() { return "robo-email-field"; }
