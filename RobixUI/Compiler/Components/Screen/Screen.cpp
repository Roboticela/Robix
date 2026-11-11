#include "Screen.hpp"

#include "../../RoboCompilerShared.hpp"

#include <sstream>
#include <string>

std::string RoboCompilerEmitScreenCPP(const RoboXMLNode& node) {
  const std::string id = RoboXMLAttr(node, "id", "screen");
  const std::string route = RoboXMLAttr(node, "path", RoboXMLAttr(node, "route", "/").c_str());
  const std::string title = RoboXMLAttr(node, "name", RoboXMLAttr(node, "title", "").c_str());
  const std::string theme = RoboXMLAttr(node, "theme", RoboXMLAttr(node, "appearance", "dark").c_str());
  const std::string transparent = RoboXMLAttr(node, "transparent", RoboXMLAttr(node, "transparency", "false").c_str());
  const std::string place = RoboXMLAttr(node, "place", "center");
  const std::string width = RoboXMLAttr(node, "width", "640");
  const std::string height = RoboXMLAttr(node, "height", "400");
  const std::string x = RoboXMLAttr(node, "x", "0");
  const std::string y = RoboXMLAttr(node, "y", "0");
  std::ostringstream ss;
  ss << "  static RoboUIScreen " << id << "Storage;\n";
  ss << "  RoboUIScreenInit(&" << id << "Storage, \"" << RoboCompilerCPPEscape(id) << "\");\n";
  const std::string expr = "RoboUIScreenAsComponent(&" + id + "Storage)";
  ss << "  RoboUISetProperty(" << expr << ", \"path\", \"" << RoboCompilerCPPEscape(route) << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"name\", \"" << RoboCompilerCPPEscape(title) << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"theme\", \"" << RoboCompilerCPPEscape(theme) << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"transparent\", \"" << RoboCompilerCPPEscape(transparent) << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"place\", \"" << RoboCompilerCPPEscape(place) << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"x\", \"" << RoboCompilerCPPEscape(x) << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"y\", \"" << RoboCompilerCPPEscape(y) << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"width\", \"" << RoboCompilerCPPEscape(width) << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"height\", \"" << RoboCompilerCPPEscape(height) << "\");\n";
  const char* keys[] = {"minWidth",     "minHeight", "maxWidth",    "maxHeight",   "radius",       "border",
                        "display",      "direction", "flexDirection","justify",    "justifyContent","align",
                        "alignItems",   "alignContent","justifyItems","wrap",      "flexWrap",     "gap",
                        "rowGap",       "columnGap", "padding",     "columns",     "rows",         "grow",
                        "flex",         "flexGrow",  "flexShrink",  "alignSelf",   "order",        "position",
                        "inset",        "left",      "top",         "right",       "bottom",       "margin",
                        "gridColumn",   "gridRow",   "state",       "resizable",    "resize",
                        "noResize",     "resizeLeft","resizeRight", "resizeTop",    "resizeBottom", "resizeTopLeft",
                        "resizeTopRight","resizeBottomLeft","resizeBottomRight","minSize","maxSize","movable","topmost",
                        "fullscreen",   "zoomControls","zoom-controls","background","borderColor", "board"};
  for (const char* key : keys) {
    const std::string value = RoboXMLAttr(node, key);
    if (!value.empty()) {
      ss << "  RoboUISetProperty(" << expr << ", \"" << key << "\", \"" << RoboCompilerCPPEscape(value) << "\");\n";
    }
  }
  if (RoboXMLAttr(node, "maxWidth").empty() && !RoboXMLAttr(node, "contentWidth").empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"maxWidth\", \""
       << RoboCompilerCPPEscape(RoboXMLAttr(node, "contentWidth")) << "\");\n";
  }
  if (RoboXMLAttr(node, "gap").empty() && !RoboXMLAttr(node, "gutter").empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"gap\", \"" << RoboCompilerCPPEscape(RoboXMLAttr(node, "gutter"))
       << "\");\n";
  }
  ss << "  RoboUIScreenRegister(&" << id << "Storage);\n";
  const std::string content = "RoboUIScreenContent(&" + id + "Storage)";
  for (const auto& child : node.children) {
    ss << RoboCompilerEmitChildCPP(child, content);
  }
  return ss.str();
}
