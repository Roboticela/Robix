#include "Container.hpp"

#include "../../RoboCompilerShared.hpp"

#include <sstream>
#include <string>

bool RoboCompilerIsContainerStyleKey(const std::string& key) {
  static const char* keys[] = {
      "widthmode", "widthbehavior", "heightmode", "heightbehavior", "aspectratio", "sizing", "marginx", "marginy",
      "paddingx", "paddingy", "paddinghorizontal", "paddingvertical", "spacing", "childgap", "place", "layout",
      "reverse", "gradient", "gradientfrom", "gradientto", "gradientangle", "gradientstops", "backgroundimage",
      "backgroundsize", "backgroundposition", "backgroundrepeat", "backgroundopacity", "backgroundbrightness",
      "backgroundcontrast", "backgroundsaturation", "backgroundblur", "backgroundvideo", "backgroundoverlay",
      "overlay", "overlaycolor", "glass", "frost", "blur", "backdropblur", "foregroundblur", "pattern", "noise",
      "grain", "mesh", "texture", "borderstyle", "bordertopwidth", "borderrightwidth", "borderbottomwidth",
      "borderleftwidth", "bordertopcolor", "borderrightcolor", "borderbottomcolor", "borderleftcolor",
      "bordergradient", "radius", "radiustopleft", "radiustopright", "radiusbottomleft", "radiusbottomright",
      "shape", "clippath", "shadow", "shadowcolor", "shadowopacity", "shadowx", "shadowy", "shadowblur",
      "shadowspread", "shadowinset", "shadows", "depth", "brightness", "contrast", "saturation", "grayscale",
      "sepia", "hue", "invert", "blend", "blendmode", "glow", "innerglow", "mask", "fade", "accent", "accentcolor",
      "decor", "textcolor", "headingcolor", "paragraphcolor", "linkcolor", "fontsize", "fontweight", "lineheight",
      "letterspacing", "textalign", "textdecoration", "texttransform", "textwrap", "textoverflow", "selectioncolor",
      "placeholdercolor", "hoverbackground", "hovergradient", "hoverbordercolor", "hoverborderwidth", "hoverradius",
      "hovershadow", "hoverglow", "hoveropacity", "hoverbrightness", "hoverblur", "hoverscale", "hoverrotate",
      "hoverx", "hovery", "hovercolor", "hoveroverlay", "hovermotion", "focusbordercolor", "focusborderwidth",
      "focusoutline", "focusoffset", "focusglow", "focusshadow", "focusbackground", "focusopacity",
      "activebackground", "activeborder", "activeshadow", "activescale", "activeopacity", "enter", "motion",
      "duration", "delay", "easing", "iteration", "speed", "preset", "variant", "theme", "density", "chrome",
      "titlebar", "titlebarcolor", "scrollbarwidth", "scrollbarcolor", "scrollbartrack", "scrollbarthumb",
      "scrollbarhover", "disabled", "selected", "filter", "transform", "transition", "cursor"};
  for (const char* item : keys) {
    if (key == item) {
      return true;
    }
  }
  return false;
}

bool RoboCompilerIsContainerName(const std::string& name) {
  const std::string n = RoboCompilerLower(name);
  return n == "container" || n == "box" || n == "flex" || n == "stack" || n == "canvas" ||
         RoboCompilerIsPaneFamily(name);
}

bool RoboCompilerCheckContainer(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
  bool ok = true;
  const std::string display = RoboCompilerLower(RoboXMLAttr(node, "display", "flex"));
  if (!RoboCompilerIsOneOf(display, {"flex", "grid", "stack", "canvas", "block", "inline", "inline-flex",
                                     "inline-block", "horizontal", "vertical", "overlay", "layered", "absolute"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Container display must be flex|grid|stack|canvas|block|inline|inline-flex|horizontal|vertical|overlay|layered|absolute (got '" +
                            display + "')");
    ok = false;
  }
  const std::string direction =
      RoboCompilerLower(RoboXMLAttr(node, "direction", RoboXMLAttr(node, "flexDirection", "column").c_str()));
  if (!RoboCompilerIsOneOf(direction, {"row", "column", "col", "row-reverse", "rowreverse", "column-reverse",
                                       "columnreverse"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Container direction must be row|column|row-reverse|column-reverse");
    ok = false;
  }
  const std::string justify =
      RoboCompilerLower(RoboXMLAttr(node, "justify", RoboXMLAttr(node, "justifyContent", "start").c_str()));
  if (!RoboCompilerIsOneOf(justify, {"start", "flex-start", "left", "top", "center", "end", "flex-end", "right",
                                     "bottom", "space-between", "spacebetween", "space-around", "spacearound",
                                     "space-evenly", "spaceevenly"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Container justify must be start|center|end|space-between|space-around|space-evenly");
    ok = false;
  }
  const std::string align =
      RoboCompilerLower(RoboXMLAttr(node, "align", RoboXMLAttr(node, "alignItems", "stretch").c_str()));
  if (!RoboCompilerIsOneOf(align, {"start", "flex-start", "left", "top", "center", "end", "flex-end", "right",
                                   "bottom", "stretch"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Container align must be start|center|end|stretch");
    ok = false;
  }
  const std::string wrap =
      RoboCompilerLower(RoboXMLAttr(node, "wrap", RoboXMLAttr(node, "flexWrap", "nowrap").c_str()));
  if (!RoboCompilerIsOneOf(wrap, {"wrap", "nowrap", "none", "wrap-reverse", "wrapreverse"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Container wrap must be wrap|nowrap");
    ok = false;
  }
  const std::string position = RoboCompilerLower(RoboXMLAttr(node, "position", "relative"));
  if (!RoboCompilerIsOneOf(position, {"relative", "static", "absolute", "fixed"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "position must be relative|absolute|fixed");
    ok = false;
  }
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (!RoboCompilerIsSharedStyleKey(key) && key != "id" && key != "x" && key != "y" && key != "padding" &&
        key != "paddingtop" && key != "paddingright" && key != "paddingbottom" && key != "paddingleft" &&
        key != "background" && key != "backgroundcolor" && key != "bg" && key != "fill" && key != "fillparent" &&
        key != "face" && key != "alias") {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "unknown Container attribute '" + kv.first + "'");
    }
  }
  for (const auto& child : node.children) {
    ok = RoboCompilerCheckNode(child, path, result) && ok;
  }
  return ok;
}

std::string RoboCompilerEmitContainerCpp(const RoboXMLNode& node, const std::string& parentExpr) {
  const std::string id = RoboXMLAttr(node, "id", "container");
  const std::string display = RoboCompilerLower(RoboXMLAttr(node, "display", "flex"));
  const std::string direction =
      RoboCompilerLower(RoboXMLAttr(node, "direction", RoboXMLAttr(node, "flexDirection", "column").c_str()));
  const std::string justify =
      RoboCompilerLower(RoboXMLAttr(node, "justify", RoboXMLAttr(node, "justifyContent", "start").c_str()));
  const std::string align =
      RoboCompilerLower(RoboXMLAttr(node, "align", RoboXMLAttr(node, "alignItems", "stretch").c_str()));
  const std::string wrap =
      RoboCompilerLower(RoboXMLAttr(node, "wrap", RoboXMLAttr(node, "flexWrap", "nowrap").c_str()));
  const std::string gap = RoboXMLAttr(node, "gap", "0");
  const std::string padding = RoboXMLAttr(node, "padding", "0");
  const std::string x = RoboXMLAttr(node, "x", "0");
  const std::string y = RoboXMLAttr(node, "y", "0");
  const std::string bg =
      RoboXMLAttr(node, "background", RoboXMLAttr(node, "backgroundColor", RoboXMLAttr(node, "bg").c_str()).c_str());
  const std::string fill = RoboXMLAttr(node, "fill", RoboXMLAttr(node, "fillParent").c_str());

  std::ostringstream ss;
  ss << "  static RoboContainer " << id << "Storage;\n";
  ss << "  RoboContainerInit(&" << id << "Storage, \"" << RoboCompilerCppEscape(id) << "\");\n";
  if (id == "dock") {
    ss << "  gRoboDesktopDock = &" << id << "Storage;\n";
  }
  const std::string expr = "RoboContainerAsComponent(&" + id + "Storage)";
  ss << "  RoboUISetProperty(" << expr << ", \"display\", \"" << display << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"direction\", \"" << direction << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"justify\", \"" << justify << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"align\", \"" << align << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"wrap\", \"" << wrap << "\");\n";
  if (!gap.empty() && gap != "0") {
    ss << "  RoboUISetProperty(" << expr << ", \"gap\", \"" << gap << "\");\n";
  }
  if (!RoboXMLAttr(node, "rowGap").empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"rowGap\", \"" << RoboXMLAttr(node, "rowGap") << "\");\n";
  }
  if (!RoboXMLAttr(node, "columnGap").empty() || !RoboXMLAttr(node, "colGap").empty()) {
    const std::string colGap = RoboXMLAttr(node, "columnGap", RoboXMLAttr(node, "colGap").c_str());
    ss << "  RoboUISetProperty(" << expr << ", \"columnGap\", \"" << colGap << "\");\n";
  }
  if (!padding.empty() && padding != "0") {
    ss << "  RoboUISetProperty(" << expr << ", \"padding\", \"" << padding << "\");\n";
  }
  if (!RoboXMLAttr(node, "paddingTop").empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"paddingTop\", \"" << RoboXMLAttr(node, "paddingTop") << "\");\n";
  }
  if (!RoboXMLAttr(node, "paddingRight").empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"paddingRight\", \"" << RoboXMLAttr(node, "paddingRight")
       << "\");\n";
  }
  if (!RoboXMLAttr(node, "paddingBottom").empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"paddingBottom\", \"" << RoboXMLAttr(node, "paddingBottom")
       << "\");\n";
  }
  if (!RoboXMLAttr(node, "paddingLeft").empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"paddingLeft\", \"" << RoboXMLAttr(node, "paddingLeft")
       << "\");\n";
  }
  ss << "  RoboUISetProperty(" << expr << ", \"x\", \"" << x << "\");\n";
  ss << "  RoboUISetProperty(" << expr << ", \"y\", \"" << y << "\");\n";
  if (!bg.empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"background\", \"" << RoboCompilerCppEscape(bg) << "\");\n";
  }
  if (!fill.empty()) {
    ss << "  RoboUISetProperty(" << expr << ", \"fill\", \"" << RoboCompilerCppEscape(fill) << "\");\n";
  }
  ss << RoboCompilerEmitLayoutProps(node, expr);
  ss << "  RoboUIAddChild(" << parentExpr << ", " << expr << ");\n";
  for (const auto& child : node.children) {
    ss << RoboCompilerEmitChildCpp(child, expr);
  }
  return ss.str();
}
