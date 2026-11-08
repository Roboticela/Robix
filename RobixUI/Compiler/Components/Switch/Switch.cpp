#include "Switch.hpp"

#include "../../RoboCompilerShared.hpp"

#include <sstream>
#include <string>

namespace {

bool RoboCompilerSwitchVariantOk(const std::string& variant) {
  return RoboCompilerIsOneOf(variant, {"primary", "subtle", "outline", "ghost", "danger"});
}

bool RoboCompilerSwitchSizeOk(const std::string& size) {
  return RoboCompilerIsOneOf(size, {"sm", "md", "lg", "small", "medium", "large"});
}

}  // namespace

bool RoboCompilerCheckSwitch(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
  bool ok = true;
  const std::string variant = RoboCompilerLower(RoboXMLAttr(node, "variant", "primary"));
  if (!RoboCompilerSwitchVariantOk(variant)) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Switch variant must be primary|subtle|outline|ghost|danger");
    ok = false;
  }
  const std::string size = RoboCompilerLower(RoboXMLAttr(node, "size", "md"));
  if (!RoboCompilerSwitchSizeOk(size)) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path, "Switch size must be sm|md|lg");
    ok = false;
  }
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (!RoboCompilerIsLayoutKey(key) && key != "id" && key != "variant" && key != "size" && key != "checked" &&
        key != "disabled" && key != "loading" && key != "label" && key != "name") {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "unknown Switch attribute '" + kv.first + "'");
    }
  }
  return ok;
}

bool RoboCompilerCheckSwitchGroup(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
  bool ok = true;
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (!RoboCompilerIsLayoutKey(key) && key != "id" && key != "label" && key != "name") {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "unknown SwitchGroup attribute '" + kv.first + "'");
    }
  }
  for (const RoboXMLNode& child : node.children) {
    ok = RoboCompilerCheckNode(child, path, result) && ok;
  }
  return ok;
}

bool RoboCompilerCheckSwitchItem(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
  bool ok = true;
  if (RoboXMLAttr(node, "title").empty()) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path, "SwitchItem requires a title");
    ok = false;
  }
  const std::string variant = RoboCompilerLower(RoboXMLAttr(node, "variant", "primary"));
  if (!RoboCompilerSwitchVariantOk(variant)) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "SwitchItem variant must be primary|subtle|outline|ghost|danger");
    ok = false;
  }
  const std::string size = RoboCompilerLower(RoboXMLAttr(node, "size", "md"));
  if (!RoboCompilerSwitchSizeOk(size)) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path, "SwitchItem size must be sm|md|lg");
    ok = false;
  }
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (!RoboCompilerIsLayoutKey(key) && key != "id" && key != "title" && key != "description" && key != "variant" &&
        key != "size" && key != "checked" && key != "disabled" && key != "loading" && key != "label" &&
        key != "name") {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "unknown SwitchItem attribute '" + kv.first + "'");
    }
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

}  // namespace

std::string RoboCompilerEmitSwitchCpp(const RoboXMLNode& node, const std::string& parentExpr, bool item,
                                      bool divider) {
  const std::string xmlId = RoboXMLAttr(node, "id", item ? "switchItem" : "switch");
  const std::string id = RoboCompilerStorageName(xmlId, item ? "switchItem" : "switch");
  const std::string variant = RoboCompilerLower(RoboXMLAttr(node, "variant", "primary"));
  const std::string size = RoboCompilerLower(RoboXMLAttr(node, "size", "md"));
  const bool checked = RoboCompilerEmitFlag(RoboXMLAttr(node, "checked"));
  const bool loading = RoboCompilerEmitFlag(RoboXMLAttr(node, "loading"));
  const bool disabled = RoboCompilerEmitFlag(RoboXMLAttr(node, "disabled")) || loading;
  const std::string title = item ? RoboXMLAttr(node, "title") : RoboXMLAttr(node, "label");
  const std::string description = item ? RoboXMLAttr(node, "description") : std::string();

  std::ostringstream ss;
  ss << "  static RoboSwitch " << id << "Storage;\n";
  ss << "  RoboSwitchInit(&" << id << "Storage, \"" << RoboCompilerCppEscape(xmlId) << "\");\n";
  ss << "  RoboSwitchSetVariant(&" << id << "Storage, \"" << RoboCompilerCppEscape(variant) << "\");\n";
  ss << "  RoboSwitchSetSize(&" << id << "Storage, \"" << RoboCompilerCppEscape(size) << "\");\n";
  ss << "  RoboSwitchSetChecked(&" << id << "Storage, " << (checked ? "ROBO_TRUE" : "ROBO_FALSE") << ");\n";
  ss << "  RoboSwitchSetCopy(&" << id << "Storage, \"" << RoboCompilerCppEscape(title) << "\", \""
     << RoboCompilerCppEscape(description) << "\");\n";
  ss << "  RoboSwitchSetFlags(&" << id << "Storage, " << (disabled ? "ROBO_TRUE" : "ROBO_FALSE") << ", "
     << (loading ? "ROBO_TRUE" : "ROBO_FALSE") << ", " << (item ? "ROBO_TRUE" : "ROBO_FALSE") << ", "
     << (divider ? "ROBO_TRUE" : "ROBO_FALSE") << ");\n";
  ss << "  RoboSwitchSync(&" << id << "Storage, ctx, 96);\n";
  const std::string expr = "RoboSwitchAsComponent(&" + id + "Storage)";
  ss << RoboCompilerEmitLayoutProps(node, expr);
  ss << "  RoboUIAddChild(" << parentExpr << ", " << expr << ");\n";
  return ss.str();
}

std::string RoboCompilerEmitSwitchGroupCpp(const RoboXMLNode& node, const std::string& parentExpr) {
  const std::string xmlId = RoboXMLAttr(node, "id", "switchGroup");
  const std::string id = RoboCompilerStorageName(xmlId, "switchGroup");
  std::ostringstream ss;
  ss << "  static RoboContainer " << id << "Storage;\n";
  ss << "  RoboContainerInit(&" << id << "Storage, \"" << RoboCompilerCppEscape(xmlId) << "\");\n";
  const std::string expr = "RoboContainerAsComponent(&" + id + "Storage)";
  ss << RoboCompilerEmitLayoutProps(node, expr);
  ss << "  RoboUISetProperty(" << expr << ", \"display\", \"flex\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"direction\", \"column\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"align\", \"stretch\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"justify\", \"start\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"width\", \"100%\");\n";
  ss << "  RoboUIAddChild(" << parentExpr << ", " << expr << ");\n";
  bool seenItem = false;
  for (const RoboXMLNode& child : node.children) {
    if (RoboCompilerLower(child.name) == "switchitem") {
      ss << RoboCompilerEmitSwitchCpp(child, expr, true, seenItem);
      seenItem = true;
    } else {
      ss << RoboCompilerEmitChildCpp(child, expr);
    }
  }
  return ss.str();
}
