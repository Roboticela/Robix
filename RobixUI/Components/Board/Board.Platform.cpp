#include "Board.Platform.hpp"

#include "../../RoboCompilerShared.hpp"

#include <sstream>
#include <string>

bool RoboCompilerIsBoardName(const std::string& name) {
  return RoboCompilerLower(name) == "board";
}

std::string RoboCompilerEmitBoardCpp(const RoboXMLNode& node, const std::string& parentExpr) {
  const std::string id = RoboXMLAttr(node, "id", "board");
  const std::string display = RoboCompilerLower(RoboXMLAttr(node, "display", "canvas"));
  const std::string direction =
      RoboCompilerLower(RoboXMLAttr(node, "direction", RoboXMLAttr(node, "flexDirection", "column").c_str()));
  const std::string justify =
      RoboCompilerLower(RoboXMLAttr(node, "justify", RoboXMLAttr(node, "justifyContent", "start").c_str()));
  const std::string align =
      RoboCompilerLower(RoboXMLAttr(node, "align", RoboXMLAttr(node, "alignItems", "start").c_str()));
  const std::string wrap =
      RoboCompilerLower(RoboXMLAttr(node, "wrap", RoboXMLAttr(node, "flexWrap", "nowrap").c_str()));
  const std::string screen = RoboXMLAttr(node, "screen", RoboXMLAttr(node, "screenId", RoboXMLAttr(node, "path").c_str()).c_str());
  const std::string columns = RoboXMLAttr(node, "columns", "1");
  const std::string gap = RoboXMLAttr(node, "gap", "0");
  const std::string padding = RoboXMLAttr(node, "padding", "0");
  const std::string x = RoboXMLAttr(node, "x", "0");
  const std::string y = RoboXMLAttr(node, "y", "0");
  const std::string bg =
      RoboXMLAttr(node, "background", RoboXMLAttr(node, "backgroundColor", RoboXMLAttr(node, "bg").c_str()).c_str());

  std::ostringstream ss;
  ss << "  static RoboUIBoard " << id << "Storage;\n";
  ss << "  RoboUIBoardInit(&" << id << "Storage, \"" << RoboCompilerCppEscape(id) << "\");\n";
  if (!screen.empty() && parentExpr.empty()) {
    ss << "  RoboUIBoardSetScreen(&" << id << "Storage, \"" << RoboCompilerCppEscape(screen) << "\");\n";
  }
  const std::string expr = "RoboUIBoardAsComponent(&" + id + "Storage)";
  ss << "  RoboUISetProperty(" << expr << ", \"direction\", \"" << direction << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"justify\", \"" << justify << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"align\", \"" << align << "\");\n";
  if (display != "grid") {
    ss << "  RoboUISetProperty(" << expr << ", \"wrap\", \"" << wrap << "\");\n";
  }
  ss << "  RoboUIBoardSetDisplay(&" << id << "Storage, \"" << RoboCompilerCppEscape(display) << "\");\n";
  if (display == "grid") {
    ss << "  RoboUIBoardSetColumns(&" << id << "Storage, " << columns << ");\n";
  }
  if (!gap.empty() && gap != "0") {
    ss << "  RoboUISetProperty(" << expr << ", \"gap\", \"" << gap << "\");\n";
  }
  if (!padding.empty() && padding != "0") {
    ss << "  RoboUISetProperty(" << expr << ", \"padding\", \"" << padding << "\");\n";
  }
  ss << "  RoboUISetProperty(" << expr << ", \"x\", \"" << x << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"y\", \"" << y << "\");\n";
  if (!RoboXMLAttr(node, "width").empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"width\", \"" << RoboCompilerCppEscape(RoboXMLAttr(node, "width"))
       << "\");\n";
  }
  if (!RoboXMLAttr(node, "height").empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"height\", \"" << RoboCompilerCppEscape(RoboXMLAttr(node, "height"))
       << "\");\n";
  }
  if (!bg.empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"background\", \"" << RoboCompilerCppEscape(bg) << "\");\n";
  }
  const std::string fill = RoboXMLAttr(node, "fill", RoboXMLAttr(node, "fillParent").c_str());
  if (!fill.empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"fill\", \"" << RoboCompilerCppEscape(fill) << "\");\n";
  }
  ss << RoboCompilerEmitLayoutProps(node, expr);
  ss << "  RoboUIBoardRegister(&" << id << "Storage);\n";
  if (!parentExpr.empty()) {
    ss << "  RoboUIAddChild(" << parentExpr << ", " << expr << ");\n";
  }
  for (const auto& child : node.children) {
    ss << RoboCompilerEmitChildCpp(child, expr);
  }
  return ss.str();
}
