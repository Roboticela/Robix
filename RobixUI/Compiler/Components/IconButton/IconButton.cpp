#include "IconButton.hpp"

#include "../Button/Button.hpp"
#include "../../RoboCompilerShared.hpp"

#include <sstream>
#include <string>

bool RoboCompilerCheckIconButton(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
  bool ok = true;
  if (RoboXMLAttr(node, "label").empty()) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path, "IconButton requires a label");
    ok = false;
  }
  const std::string variant = RoboCompilerLower(RoboXMLAttr(node, "variant", "ghost"));
  if (!RoboCompilerIsOneOf(variant, {"ghost", "subtle", "outline", "primary", "danger"}) &&
      !RoboCompilerIsButtonPreset(variant)) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "IconButton variant must be ghost|subtle|outline|primary|danger");
    ok = false;
  }
  const std::string size = RoboCompilerLower(RoboXMLAttr(node, "size", "md"));
  if (!RoboCompilerIsOneOf(size, {"xs", "sm", "md", "lg", "xl", "2xl", "3xl", "small", "medium", "large"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path, "IconButton size must be sm|md");
    ok = false;
  }
  const std::string status = RoboCompilerLower(RoboXMLAttr(node, "status"));
  if (!status.empty() && !RoboCompilerIsOneOf(status, {"idle", "loading", "success", "error"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "IconButton status must be idle|loading|success|error");
    ok = false;
  }
  const std::string icon = RoboXMLAttr(node, "icon");
  if (!icon.empty() && !RoboCompilerIconInCatalog(icon.c_str())) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "IconButton icon '" + icon + "' was not found in the icon pack");
    ok = false;
  } else if (!icon.empty()) {
    RoboCompilerNoteUsedIcon(icon);
  }
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (!RoboCompilerIsLayoutKey(key) && key != "id" && key != "label" && key != "badge" && key != "size" &&
        key != "variant" && key != "loading" && key != "status" && key != "action" && key != "successms" &&
        key != "disabled" && key != "title" && key != "icon" && key != "name" &&
        !RoboCompilerIsButtonStyleKey(key)) {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "unknown IconButton attribute '" + kv.first + "'");
    }
  }
  for (const RoboXMLNode& child : node.children) {
    ok = RoboCompilerCheckNode(child, path, result) && ok;
  }
  return ok;
}

namespace {

bool RoboCompilerEmitFlag(const std::string& value) {
  const std::string flag = RoboCompilerLower(value);
  return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

std::string RoboCompilerStorageName(const std::string& id, const char* fallback) {
  return RoboCompilerIsIdent(id) ? id : std::string(fallback);
}

int RoboCompilerEmitInt(const std::string& value, int fallback) {
  if (value.empty()) {
    return fallback;
  }
  int number = 0;
  for (char ch : value) {
    if (ch < '0' || ch > '9') {
      return fallback;
    }
    number = number * 10 + (ch - '0');
  }
  return number > 0 ? number : fallback;
}

}  // namespace

std::string RoboCompilerEmitIconButtonCpp(const RoboXMLNode& node, const std::string& parentExpr) {
  const std::string xmlId = RoboXMLAttr(node, "id", "iconButton");
  const std::string id = RoboCompilerStorageName(xmlId, "iconButton");
  const std::string variant = RoboCompilerLower(RoboXMLAttr(node, "variant", "ghost"));
  const std::string size = RoboCompilerLower(RoboXMLAttr(node, "size", "md"));
  std::string status = RoboCompilerLower(RoboXMLAttr(node, "status"));
  if (status.empty() && RoboCompilerEmitFlag(RoboXMLAttr(node, "loading"))) {
    status = "loading";
  }
  const bool controlled = !status.empty();
  const bool disabled = RoboCompilerEmitFlag(RoboXMLAttr(node, "disabled")) || status == "loading";
  const std::string badge = RoboXMLAttr(node, "badge");
  const std::string action = controlled ? std::string() : RoboXMLAttr(node, "action");
  const int successMs = RoboCompilerEmitInt(RoboXMLAttr(node, "successMs"), 1600);
  std::string icon = RoboXMLAttr(node, "icon");
  if (icon.empty()) {
    icon = RoboXMLAttr(node, "name");
  }
  for (const RoboXMLNode& child : node.children) {
    const std::string name = RoboCompilerLower(child.name);
    if (name != "icon" && name != "svg" && !RoboCompilerIsIconNode(child)) {
      continue;
    }
    const std::string iconName = name == "svg" ? RoboCompilerSvgIconName(child) : RoboCompilerIconNameFromNode(child);
    if (!iconName.empty()) {
      icon = iconName;
      break;
    }
  }

  std::ostringstream ss;
  ss << "  static RoboIconButton " << id << "Storage;\n";
  ss << "  RoboIconButtonInit(&" << id << "Storage, \"" << RoboCompilerCppEscape(xmlId) << "\");\n";
  ss << "  RoboIconButtonSetIcon(&" << id << "Storage, \"" << RoboCompilerCppEscape(icon) << "\");\n";
  ss << "  RoboIconButtonSetVariant(&" << id << "Storage, \"" << RoboCompilerCppEscape(variant) << "\");\n";
  ss << "  RoboIconButtonSetSize(&" << id << "Storage, \"" << RoboCompilerCppEscape(size) << "\");\n";
  ss << "  RoboIconButtonSetStatus(&" << id << "Storage, \"" << RoboCompilerCppEscape(status) << "\", "
     << (controlled ? "ROBO_TRUE" : "ROBO_FALSE") << ");\n";
  ss << "  RoboIconButtonSetBadge(&" << id << "Storage, \"" << RoboCompilerCppEscape(badge) << "\");\n";
  ss << "  RoboIconButtonSetAction(&" << id << "Storage, \"" << RoboCompilerCppEscape(action) << "\", " << successMs
     << ");\n";
  ss << "  RoboIconButtonSetDisabled(&" << id << "Storage, " << (disabled ? "ROBO_TRUE" : "ROBO_FALSE") << ");\n";
  ss << "  RoboIconButtonSync(&" << id << "Storage, ctx, 96);\n";
  const std::string expr = "RoboIconButtonAsComponent(&" + id + "Storage)";
  ss << RoboCompilerEmitLayoutProps(node, expr);
  ss << "  RoboUIAddChild(" << parentExpr << ", " << expr << ");\n";
  return ss.str();
}
