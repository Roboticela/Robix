#include "Icon.hpp"

#include "../../RoboCompilerShared.hpp"

#include <cctype>
#include <sstream>
#include <string>

bool RoboCompilerCheckIcon(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
  bool ok = true;
  const std::string iconName = RoboCompilerIconNameFromNode(node);
  if (iconName.empty()) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "<Icon> requires name= or use <IconName /> tag");
    return false;
  }
  if (!RoboCompilerIconInCatalog(iconName.c_str())) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "icon '" + iconName + "' not found in Assets/Icons.rpack");
    ok = false;
  } else {
    RoboCompilerNoteUsedIcon(iconName);
  }
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (!RoboCompilerIsLayoutKey(key) && key != "id" && key != "name" && key != "icon" && key != "src" &&
        key != "size" && key != "width" && key != "height" && key != "x" && key != "y" && key != "opacity" &&
        key != "color" && key != "fill" && key != "stroke" && key != "accent" && key != "strokewidth" &&
        key != "slot" && key != "side" && key != "place") {
      /* Allow extra named slot colors without warning if value looks like a color. */
      if (kv.second.empty() || kv.second[0] != '#') {
        RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                            "unknown Icon attribute '" + kv.first + "'");
      }
    }
  }
  if (!node.children.empty()) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path, "Icon elements cannot have children");
    ok = false;
  }
  return ok;
}

std::string RoboCompilerEmitIconCpp(const RoboXMLNode& node, const std::string& parentExpr) {
  const bool inlineSvg = RoboCompilerLower(node.name) == "svg";
  const std::string iconName = inlineSvg ? RoboCompilerSvgIconName(node) : RoboCompilerIconNameFromNode(node);
  RoboCompilerNoteUsedIcon(iconName);
  std::string id = RoboXMLAttr(node, "id", iconName.c_str());
  if (!RoboCompilerIsIdent(id)) {
    std::string safe = "svg";
    for (char ch : id) {
      safe.push_back(std::isalnum(static_cast<unsigned char>(ch)) ? ch : '_');
    }
    id = safe;
  }
  const std::string size = RoboXMLAttr(node, "size", "24");
  const std::string x = RoboXMLAttr(node, "x", "0");
  const std::string y = RoboXMLAttr(node, "y", "0");
  const std::string opacity = RoboXMLAttr(node, "opacity");

  std::ostringstream ss;
  ss << "  static RoboIcon " << id << "Storage;\n";
  ss << "  RoboIconInit(&" << id << "Storage, \"" << RoboCompilerCppEscape(id) << "\", \""
     << RoboCompilerCppEscape(iconName) << "\");\n";
  const std::string expr = "RoboIconAsComponent(&" + id + "Storage)";
  ss << "  RoboUISetProperty(" << expr << ", \"size\", \"" << size << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"x\", \"" << x << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"y\", \"" << y << "\");\n";
  if (!opacity.empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"opacity\", \"" << opacity << "\");\n";
  }
  ss << RoboCompilerEmitLayoutProps(node, expr);
  /* Named slot / color attributes. */
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (key == "id" || key == "name" || key == "icon" || key == "src" || key == "size" || key == "x" || key == "y" ||
        key == "opacity" || key == "strokewidth") {
      continue;
    }
    if (kv.second.empty()) {
      continue;
    }
    if (kv.second[0] == '#' || key == "fill" || key == "stroke" || key == "accent" || key == "color" ||
        key == "width" || key == "height" || key == "position" || key == "left" || key == "top" || key == "right" ||
        key == "bottom" || key == "margin" || key == "flex" || key == "alignself") {
      ss << "  RoboUISetProperty(" << expr << ", \"" << RoboCompilerCppEscape(kv.first) << "\", \""
         << RoboCompilerCppEscape(kv.second) << "\");\n";
    }
  }
  ss << "  RoboIconResolve(&" << id << "Storage, ctx);\n";
  ss << "  RoboUIAddChild(" << parentExpr << ", " << expr << ");\n";
  return ss.str();
}
