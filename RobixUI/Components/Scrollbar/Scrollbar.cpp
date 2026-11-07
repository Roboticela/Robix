#include "Scrollbar.hpp"

#include "../../RoboCompilerShared.hpp"

#include <sstream>
#include <string>

bool RoboCompilerIsScrollbarName(const std::string& name) {
  return RoboCompilerLower(name) == "scrollbar";
}

std::string RoboCompilerEmitScrollbarCpp(const RoboXMLNode& node, const std::string& parentExpr) {
  const std::string id = RoboXMLAttr(node, "id", "scrollbar");
  const std::string orientation = RoboCompilerLower(RoboXMLAttr(node, "orientation", "vertical"));
  std::ostringstream ss;
  ss << "  /* Scrollbar " << RoboCompilerCppEscape(id) << " orientation " << RoboCompilerCppEscape(orientation)
     << " parent " << parentExpr << " */\n";
  return ss.str();
}
