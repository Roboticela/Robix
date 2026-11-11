#include "Text.hpp"

#include "../../RoboCompilerShared.hpp"

#include <sstream>
#include <string>

bool RoboCompilerIsTextStyleKey(const std::string& key) {
  static const char* keys[] = {
      "family", "fontstyle", "slant", "weight", "fontweight", "fontsize", "fontsizemin", "fontsizemax", "fluid",
      "fontsize-sm", "fontsizesm", "fontsizemd", "fontsizelg", "tracking", "letterspacing", "wordspacing", "leading",
      "lineheight", "paragraphspacing", "textindent", "hanging", "align", "textalign", "verticalalign", "wrap",
      "textwrap", "whitespace", "decoration", "textdecoration", "decorationcolor", "decorationthickness",
      "decorationoffset", "decorationstyle", "transform", "texttransform", "smallcaps", "shadow", "textshadow",
      "shadowcolor", "shadowx", "shadowy", "shadowblur", "shadows", "outline", "outlinecolor", "outlinethickness",
      "stroke", "fill", "gradient", "gradientfrom", "gradientto", "gradientangle", "effect", "mark", "highlight",
      "markcolor", "background", "backgroundcolor", "paddingx", "paddingy", "radius", "overflow", "textoverflow",
      "lines", "maxlines", "fade", "direction", "dir", "writingmode", "orientation", "selectioncolor",
      "selectiontext", "selectable", "select", "selection", "userselect", "caretcolor", "cursorcolor",
      "characterspacing", "charspacing", "hovercolor", "hoverbackground", "hoverweight", "hoverdecoration", "hovershadow", "hoverglow",
      "hoveropacity", "hovertracking", "hovertransform", "focuscolor", "focusbackground", "focusoutline",
      "focusdecoration", "activescale", "activecolor", "activebackground", "disabled", "selected", "enter", "motion",
      "duration", "delay", "easing", "iteration", "speed", "preset", "density", "theme", "linkcolor", "visitedcolor",
      "strongcolor", "mutedcolor", "codecolor", "dropcap", "features", "ligatures", "numerals", "kerning",
      "fontsmooth", "variant", "tone"};
  for (const char* item : keys) {
    if (key == item) {
      return true;
    }
  }
  return false;
}

bool RoboCompilerIsAlign(const std::string& align) {
  const std::string t = RoboCompilerLower(align);
  return t == "left" || t == "center" || t == "right" || t == "start" || t == "end";
}

bool RoboCompilerIsTextName(const std::string& name) {
  const std::string n = RoboCompilerLower(name);
  return n == "text" || n == "content" || n == "heading" || n == "h1" || n == "h2" || n == "h3" || n == "h4" ||
         n == "h5" || n == "h6" || n == "subhead" || n == "body" || n == "paragraph" || n == "p" || n == "caption" ||
         n == "eyebrow" || n == "label";
}

bool RoboCompilerIsWrap(const std::string& wrap) {
  const std::string t = RoboCompilerLower(wrap);
  return t == "none" || t == "nowrap" || t == "false" || t == "char" || t == "character" || t == "word" || t == "wrap" ||
         t == "balance" || t == "pretty";
}

RoboCompilerTextKind RoboCompilerClassifyTextNode(const RoboXMLNode& node, RoboU32* outHeadingLevel) {
  const std::string n = RoboCompilerLower(node.name);
  if (outHeadingLevel != nullptr) {
    *outHeadingLevel = 1;
  }
  if (n == "subhead") {
    return RoboCompilerTextKind::Subhead;
  }
  if (n == "caption") {
    return RoboCompilerTextKind::Caption;
  }
  if (n == "eyebrow") {
    return RoboCompilerTextKind::Eyebrow;
  }
  if (n == "label") {
    return RoboCompilerTextKind::Label;
  }
  if (n == "paragraph" || n == "p" || n == "body" || n == "content") {
    return RoboCompilerTextKind::Body;
  }
  auto setLevel = [&](RoboU32 level) {
    if (outHeadingLevel != nullptr) {
      *outHeadingLevel = level;
    }
    return RoboCompilerTextKind::Heading;
  };
  if (n == "h1") {
    return setLevel(1);
  }
  if (n == "h2") {
    return setLevel(2);
  }
  if (n == "h3") {
    return setLevel(3);
  }
  if (n == "h4") {
    return setLevel(4);
  }
  if (n == "h5") {
    return setLevel(5);
  }
  if (n == "h6") {
    return setLevel(6);
  }
  if (n == "heading") {
    std::string lv = RoboCompilerLower(RoboXMLAttr(node, "level", "1"));
    if (lv == "display") {
      return setLevel(0);
    }
    if (lv.size() >= 2 && lv[0] == 'h') {
      lv = lv.substr(1);
    }
    RoboU32 level = 1;
    if (!lv.empty() && lv[0] >= '1' && lv[0] <= '6') {
      level = static_cast<RoboU32>(lv[0] - '0');
    }
    return setLevel(level);
  }
  return RoboCompilerTextKind::Text;
}

const char* RoboCompilerTextFont(RoboCompilerTextKind kind, RoboU32 level) {
  if (kind == RoboCompilerTextKind::Label) {
    return "Inter-Medium";
  }
  if (kind == RoboCompilerTextKind::Heading) {
    return "Inter-SemiBold";
  }
  (void)level;
  return "Inter-Regular";
}

bool RoboCompilerCheckText(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
  bool ok = true;
  RoboU32 headingLevel = 1;
  const RoboCompilerTextKind kind = RoboCompilerClassifyTextNode(node, &headingLevel);
  if (kind == RoboCompilerTextKind::Heading && headingLevel > 6) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Heading level must be display or 1..6");
    ok = false;
  }
  const std::string align = RoboXMLAttr(node, "align", "start");
  if (!RoboCompilerIsAlign(align) &&
      !RoboCompilerIsOneOf(align, {"stretch", "flex-start", "flex-end", "justify", "match", "top", "middle",
                                   "bottom"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "align must be start|center|end (got '" + align + "')");
    ok = false;
  }
  const std::string wrap = RoboXMLAttr(node, "wrap", "word");
  if (!RoboCompilerIsWrap(wrap) && !RoboCompilerIsOneOf(wrap, {"wrap", "balance", "pretty", "wrap-reverse"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "wrap must be none|char|word|balance|pretty (got '" + wrap + "')");
    ok = false;
  }
  bool hasSpan = false;
  for (const auto& child : node.children) {
    if (child.name == "Span" || child.name == "span") {
      hasSpan = true;
      for (const auto& kv : child.attrs) {
        const std::string key = RoboCompilerLower(kv.first);
        if (key != "size" && key != "color" && key != "fontsize" && !RoboCompilerIsTextStyleKey(key)) {
          RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                              "unknown Span attribute '" + kv.first + "'");
        }
      }
    } else {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                          "Text only allows <Span> children (got <" + child.name + ">)");
      ok = false;
    }
  }
  const std::string content = RoboXMLAttr(node, "content");
  if (!hasSpan && content.empty() && RoboCompilerTrim(node.text).empty()) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                        "element has no content or Span children");
  }
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (!RoboCompilerIsSharedStyleKey(key) && key != "id" && key != "x" && key != "y" && key != "width" &&
        key != "height" && key != "maxwidth" && key != "size" && key != "fontsize" && key != "color" &&
        key != "content" && key != "text" && key != "font" && key != "face" && key != "fontfamily" &&
        key != "level" && key != "variant" && key != "tone" && key != "tracking" && key != "leading" &&
        key != "uppercase" && key != "screen" && key != "route" && key != "href" && key != "alias" && key != "as" &&
        key != "placeholder" && key != "type" && key != "value" && key != "editable") {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "unknown attribute '" + kv.first + "'");
    }
  }
  return ok;
}

bool RoboCompilerIsTextLike(const RoboXMLNode& node) {
  RoboU32 unused = 0;
  (void)unused;
  const std::string n = RoboCompilerLower(node.name);
  return RoboCompilerIsTextName(n);
}

std::string RoboCompilerEmitTextCPP(const RoboXMLNode& node, const std::string& parentExpr) {
  RoboU32 headingLevel = 1;
  const RoboCompilerTextKind kind = RoboCompilerClassifyTextNode(node, &headingLevel);

  std::string defaultId = "text";
  const char* roleExpr = "RoboUITextRole::Rich";
  if (kind == RoboCompilerTextKind::Heading) {
    defaultId = "heading";
    if (headingLevel == 0) {
      roleExpr = "RoboUITextRole::Display";
    } else if (headingLevel == 2) {
      roleExpr = "RoboUITextRole::H2";
    } else if (headingLevel == 3) {
      roleExpr = "RoboUITextRole::H3";
    } else if (headingLevel == 4) {
      roleExpr = "RoboUITextRole::H4";
    } else if (headingLevel == 5) {
      roleExpr = "RoboUITextRole::H5";
    } else if (headingLevel == 6) {
      roleExpr = "RoboUITextRole::H6";
    } else {
      roleExpr = "RoboUITextRole::H1";
    }
  } else if (kind == RoboCompilerTextKind::Subhead) {
    defaultId = "subhead";
    roleExpr = "RoboUITextRole::Subhead";
  } else if (kind == RoboCompilerTextKind::Caption) {
    defaultId = "caption";
    roleExpr = "RoboUITextRole::Caption";
  } else if (kind == RoboCompilerTextKind::Eyebrow) {
    defaultId = "eyebrow";
    roleExpr = "RoboUITextRole::Eyebrow";
  } else if (kind == RoboCompilerTextKind::Label) {
    defaultId = "label";
    roleExpr = "RoboUITextRole::Label";
  } else if (kind == RoboCompilerTextKind::Body) {
    defaultId = "body";
    const std::string bodySize = RoboCompilerLower(RoboXMLAttr(node, "size"));
    roleExpr = (bodySize == "sm" || bodySize == "small") ? "RoboUITextRole::BodySmall" : "RoboUITextRole::Body";
  }

  const std::string id = RoboXMLAttr(node, "id", defaultId.c_str());
  const std::string align = RoboCompilerLower(RoboXMLAttr(node, "align", "start"));
  const std::string wrap = RoboCompilerLower(RoboXMLAttr(node, "wrap", "word"));
  const std::string sizeAttr = RoboXMLAttr(node, "size", RoboXMLAttr(node, "fontSize").c_str());
  const bool sizeIsRole = sizeAttr == "sm" || sizeAttr == "small" || sizeAttr == "md" || sizeAttr == "medium" ||
                          sizeAttr == "exact" || sizeAttr == "auto" || sizeAttr == "dynamic" || sizeAttr == "fill" ||
                          sizeAttr == "min" || sizeAttr == "max";
  const bool sizeOverride = !sizeAttr.empty() && !sizeIsRole;
  const bool colorOverride = !RoboXMLAttr(node, "color").empty();
  const std::string color = RoboXMLAttr(node, "color");
  const std::string tone = RoboXMLAttr(node, "tone");
  const std::string x = RoboXMLAttr(node, "x", "0");
  const std::string y = RoboXMLAttr(node, "y", "0");
  std::string maxWidth = RoboXMLAttr(node, "maxWidth");
  if (maxWidth.empty()) {
    const std::string widthAttr = RoboXMLAttr(node, "width", "0");
    /* Percent widths go through layout props — maxWidth is px only. */
    if (!widthAttr.empty() && widthAttr.find('%') == std::string::npos) {
      maxWidth = widthAttr;
    }
  }
  const std::string height = RoboXMLAttr(node, "height", "0");
  const std::string contentAttr = RoboXMLAttr(node, "content");
  std::string fontName = RoboXMLAttr(node, "font");
  if (fontName.empty()) {
    fontName = RoboXMLAttr(node, "face");
  }
  if (fontName.empty()) {
    fontName = RoboXMLAttr(node, "fontFamily", RoboCompilerTextFont(kind, headingLevel));
  }

  std::ostringstream ss;
  ss << "  static RoboUIText " << id << "Storage;\n";
  ss << "  RoboUITextInit(&" << id << "Storage, \"" << RoboCompilerCPPEscape(id) << "\");\n";
  ss << "  RoboUITextApplyRole(&" << id << "Storage, " << roleExpr << ");\n";
  ss << "  RoboUITextSetFontName(&" << id << "Storage, ctx, \"" << RoboCompilerCPPEscape(fontName) << "\");\n";
  ss << "  RoboUISetProperty(RoboUITextAsComponent(&" << id << "Storage), \"align\", \"" << align << "\");\n";
  ss << "  RoboUISetProperty(RoboUITextAsComponent(&" << id << "Storage), \"wrap\", \"" << wrap << "\");\n";
  const std::string variant = RoboXMLAttr(node, "variant");
  if (!variant.empty()) {
    ss << "  RoboUISetProperty(RoboUITextAsComponent(&" << id << "Storage), \"variant\", \""
       << RoboCompilerCPPEscape(variant) << "\");\n";
  }
  if (sizeOverride || sizeAttr == "sm" || sizeAttr == "small" || sizeAttr == "md" || sizeAttr == "medium") {
    ss << "  RoboUISetProperty(RoboUITextAsComponent(&" << id << "Storage), \"size\", \""
       << RoboCompilerCPPEscape(sizeAttr) << "\");\n";
  }
  if (!tone.empty()) {
    ss << "  RoboUISetProperty(RoboUITextAsComponent(&" << id << "Storage), \"tone\", \""
       << RoboCompilerCPPEscape(tone) << "\");\n";
  }
  if (colorOverride) {
    ss << "  RoboUISetProperty(RoboUITextAsComponent(&" << id << "Storage), \"color\", \""
       << RoboCompilerCPPEscape(color) << "\");\n";
  }
  ss << "  RoboUISetProperty(RoboUITextAsComponent(&" << id << "Storage), \"x\", \"" << x << "\");\n";
  ss << "  RoboUISetProperty(RoboUITextAsComponent(&" << id << "Storage), \"y\", \"" << y << "\");\n";
  if (!maxWidth.empty() && maxWidth != "0") {
    ss << "  RoboUISetProperty(RoboUITextAsComponent(&" << id << "Storage), \"maxWidth\", \"" << maxWidth
       << "\");\n";
  }
  if (!height.empty() && height != "0") {
    ss << "  RoboUISetProperty(RoboUITextAsComponent(&" << id << "Storage), \"height\", \"" << height << "\");\n";
  }
  ss << RoboCompilerEmitLayoutProps(node, "RoboUITextAsComponent(&" + id + "Storage)");
  ss << "  RoboUITextClear(&" << id << "Storage);\n";

  bool hasSpan = false;
  for (const auto& child : node.children) {
    if (child.name != "Span" && child.name != "span") {
      continue;
    }
    hasSpan = true;
    const std::string spanSize = RoboXMLAttr(child, "size");
    const std::string spanColor = RoboXMLAttr(child, "color");
    const std::string spanText = child.text.empty() ? RoboXMLAttr(child, "content") : child.text;
    ss << "  {\n";
    if (!spanColor.empty()) {
      ss << "    const RoboU32 spanColorARGB = RoboUITextParseColorGen(\"" << RoboCompilerCPPEscape(spanColor)
         << "\");\n";
    } else if (colorOverride) {
      ss << "    const RoboU32 spanColorARGB = RoboUITextParseColorGen(\"" << RoboCompilerCPPEscape(color)
         << "\");\n";
    } else {
      ss << "    const RoboU32 spanColorARGB = 0u;\n";
    }
    ss << "    RoboUITextAppendSpan(&" << id << "Storage, \"" << RoboCompilerCPPEscape(spanText) << "\", "
       << (spanSize.empty() ? "0" : spanSize) << "u, spanColorARGB);\n";
    ss << "  }\n";
  }
  if (!hasSpan) {
    const std::string body = !contentAttr.empty() ? contentAttr : RoboCompilerTrim(node.text);
    if (!body.empty()) {
      ss << "  {\n";
      if (colorOverride) {
        ss << "    const RoboU32 spanColorARGB = RoboUITextParseColorGen(\"" << RoboCompilerCPPEscape(color)
           << "\");\n";
      } else {
        ss << "    const RoboU32 spanColorARGB = 0u;\n";
      }
      ss << "    RoboUITextAppendSpan(&" << id << "Storage, \"" << RoboCompilerCPPEscape(body) << "\", "
         << (sizeOverride ? sizeAttr : "0") << "u, spanColorARGB);\n";
      ss << "  }\n";
    }
  }

  ss << "  RoboUIAddChild(" << parentExpr << ", RoboUITextAsComponent(&" << id << "Storage));\n";
  return ss.str();
}
