#include "Button.hpp"

#include "../../RoboCompilerShared.hpp"

#include <sstream>
#include <string>

bool RoboCompilerIsButtonPreset(const std::string& name) {
  return RoboCompilerIsOneOf(name, {"glass",        "frosted",  "neon",       "soft",        "tinted",     "elevated",
                                    "flat",         "outline",  "link",       "gradient",    "floating",   "circular",
                                    "success",      "warning",  "info",       "information", "brutalist",  "neumorphic",
                                    "material",     "fluent",   "macos",      "windows",     "classic",    "acrylic",
                                    "mica",         "liquid",   "cyberpunk",  "luxury",      "metallic",   "chrome",
                                    "holographic",  "retro",    "editorial",  "minimal",     "corporate",  "monochrome",
                                    "pastel",       "industrial","transparent","framed",     "skeuomorphic"});
}

bool RoboCompilerIsButtonStyleKey(const std::string& key) {
  static const char* keys[] = {
      "preset", "shape", "widthmode", "heightmode", "paddingx", "paddingy", "gap", "icongap", "marginx", "marginy",
      "gradient", "gradientfrom", "gradientto", "gradientangle", "backgroundimage", "overlay", "glass", "backdropblur",
      "texture", "textcolor", "iconcolor", "iconsize", "iconplace", "borderstyle", "bordertopwidth", "borderrightwidth",
      "borderbottomwidth", "borderleftwidth", "bordertopcolor", "borderrightcolor", "borderbottomcolor",
      "borderleftcolor", "radius", "radiustopleft", "radiustopright", "radiusbottomleft", "radiusbottomright",
      "shadow", "shadowx", "shadowy", "shadowblur", "shadowspread", "shadowcolor", "shadowinset", "shadows", "glow",
      "depth", "font", "fontsize", "fontweight", "fontstyle", "letterspacing", "texttransform", "decoration",
      "hoverbackground", "hovercolor", "hovericon", "hoverborder", "hoverradius", "hovershadow", "hoverglow",
      "hoverscale", "hoverx", "hovery", "activebackground", "activecolor", "activescale", "activeshadow",
      "focuscolor", "focusbackground", "focusring", "focusoffset", "focusglow", "disabledcolor", "disabledbg",
      "selected", "selectedbackground", "selectedcolor", "enter", "motion", "duration", "delay", "easing", "density",
      "theme", "chrome", "accent", "badgecolor", "badgetext", "group", "blend"};
  for (const char* item : keys) {
    if (key == item) {
      return true;
    }
  }
  return false;
}

bool RoboCompilerCheckButton(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
  bool ok = true;
  const std::string variant = RoboCompilerLower(RoboXMLAttr(node, "variant", "primary"));
  if (!RoboCompilerIsOneOf(variant, {"primary", "secondary", "action", "ghost", "danger", "destructive"}) &&
      !RoboCompilerIsButtonPreset(variant)) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Button variant must be primary|secondary|action|ghost|danger");
    ok = false;
  }
  const std::string size = RoboCompilerLower(RoboXMLAttr(node, "size", "md"));
  if (!RoboCompilerIsOneOf(size, {"xs", "sm", "md", "lg", "xl", "2xl", "3xl", "small", "medium", "large"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path, "Button size must be sm|md|lg");
    ok = false;
  }
  const std::string preview = RoboCompilerLower(RoboXMLAttr(node, "preview"));
  if (!preview.empty() && !RoboCompilerIsOneOf(preview, {"hover", "focus", "active"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Button preview must be hover|focus|active");
    ok = false;
  }
  const std::string type = RoboCompilerLower(RoboXMLAttr(node, "type", "button"));
  if (!RoboCompilerIsOneOf(type, {"button", "submit", "reset"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Button type must be button|submit|reset");
    ok = false;
  }
  const std::string leading = RoboXMLAttr(node, "leading");
  const std::string trailing = RoboXMLAttr(node, "trailing");
  if (!leading.empty() && !RoboCompilerIconInCatalog(leading.c_str())) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Button leading icon '" + leading + "' was not found in the icon pack");
    ok = false;
  } else if (!leading.empty()) {
    RoboCompilerNoteUsedIcon(leading);
  }
  if (!trailing.empty() && !RoboCompilerIconInCatalog(trailing.c_str())) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Button trailing icon '" + trailing + "' was not found in the icon pack");
    ok = false;
  } else if (!trailing.empty()) {
    RoboCompilerNoteUsedIcon(trailing);
  }
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (!RoboCompilerIsSharedStyleKey(key) && key != "id" && key != "variant" && key != "size" && key != "fullwidth" &&
        key != "loading" && key != "loadingtext" && key != "leading" && key != "trailing" && key != "preview" &&
        key != "disabled" && key != "type" && key != "content" && key != "name" && key != "path" &&
        key != "route" && key != "href" && key != "screen" && key != "face" && key != "alias") {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "unknown Button attribute '" + kv.first + "'");
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

}  // namespace

std::string RoboCompilerEmitButtonCpp(const RoboXMLNode& node, const std::string& parentExpr) {
  const std::string xmlId = RoboXMLAttr(node, "id", "button");
  const std::string id = RoboCompilerStorageName(xmlId, "button");
  std::string label = RoboXMLAttr(node, "content");
  if (label.empty()) {
    label = RoboCompilerTrim(node.text);
  }
  std::string variant = RoboCompilerLower(RoboXMLAttr(node, "variant", "primary"));
  if (variant == "destructive") {
    variant = "danger";
  }
  const std::string size = RoboCompilerLower(RoboXMLAttr(node, "size", "md"));
  const std::string preview = RoboCompilerLower(RoboXMLAttr(node, "preview"));
  const std::string full = RoboCompilerLower(RoboXMLAttr(node, "fullWidth", "true"));
  const bool fullWidth = full != "false" && full != "0" && full != "no" && full != "off";
  const bool loading = RoboCompilerEmitFlag(RoboXMLAttr(node, "loading"));
  const bool disabled = RoboCompilerEmitFlag(RoboXMLAttr(node, "disabled")) || loading;
  const std::string loadingText = RoboXMLAttr(node, "loadingText");
  std::string leading = RoboXMLAttr(node, "leading");
  std::string trailing = RoboXMLAttr(node, "trailing");
  for (const RoboXMLNode& child : node.children) {
    const std::string name = RoboCompilerLower(child.name);
    if (name != "icon" && name != "svg" && !RoboCompilerIsIconNode(child)) {
      continue;
    }
    const std::string iconName = name == "svg" ? RoboCompilerSvgIconName(child) : RoboCompilerIconNameFromNode(child);
    const std::string side = RoboCompilerLower(
        RoboXMLAttr(child, "slot", RoboXMLAttr(child, "side", RoboXMLAttr(child, "place").c_str()).c_str()));
    if (side == "trailing" || side == "right" || side == "end") {
      trailing = iconName;
    } else if (leading.empty()) {
      leading = iconName;
    } else {
      trailing = iconName;
    }
  }

  std::ostringstream ss;
  ss << "  static RoboButton " << id << "Storage;\n";
  ss << "  RoboButtonInit(&" << id << "Storage, \"" << RoboCompilerCppEscape(xmlId) << "\");\n";
  ss << "  RoboButtonSetLabel(&" << id << "Storage, \"" << RoboCompilerCppEscape(label) << "\");\n";
  ss << "  RoboButtonSetVariant(&" << id << "Storage, \"" << RoboCompilerCppEscape(variant) << "\");\n";
  ss << "  RoboButtonSetSize(&" << id << "Storage, \"" << RoboCompilerCppEscape(size) << "\");\n";
  ss << "  RoboButtonSetPreview(&" << id << "Storage, \"" << RoboCompilerCppEscape(preview) << "\");\n";
  ss << "  RoboButtonSetIcons(&" << id << "Storage, \"" << RoboCompilerCppEscape(leading) << "\", \""
     << RoboCompilerCppEscape(trailing) << "\");\n";
  ss << "  RoboButtonSetFlags(&" << id << "Storage, " << (fullWidth ? "ROBO_TRUE" : "ROBO_FALSE") << ", "
     << (loading ? "ROBO_TRUE" : "ROBO_FALSE") << ", " << (disabled ? "ROBO_TRUE" : "ROBO_FALSE") << ", \""
     << RoboCompilerCppEscape(loadingText) << "\");\n";
  ss << "  RoboButtonSync(&" << id << "Storage, ctx, 96);\n";
  const std::string expr = "RoboButtonAsComponent(&" + id + "Storage)";
  ss << RoboCompilerEmitLayoutProps(node, expr);
  ss << "  RoboUIAddChild(" << parentExpr << ", " << expr << ");\n";
  return ss.str();
}
