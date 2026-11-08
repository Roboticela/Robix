#include "Svg.hpp"

#include "../../RoboCompilerShared.hpp"

#include <sstream>
#include <string>

bool RoboCompilerHexDigit(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

char RoboCompilerHexUpper(char ch) {
  if (ch >= 'a' && ch <= 'f') {
    return static_cast<char>(ch - 'a' + 'A');
  }
  return ch;
}

std::string RoboCompilerBakeSvgColors(const std::string& svg) {
  std::string out;
  out.reserve(svg.size() + 32);
  for (std::size_t i = 0; i < svg.size();) {
    if (svg[i] != '#') {
      out.push_back(svg[i]);
      ++i;
      continue;
    }
    if (i + 5 < svg.size() && (svg.compare(i, 6, "#ROBO_") == 0 || svg.compare(i, 6, "#robo_") == 0)) {
      out.push_back(svg[i]);
      ++i;
      continue;
    }
    std::size_t j = i + 1;
    while (j < svg.size() && RoboCompilerHexDigit(svg[j]) && (j - i) <= 8) {
      ++j;
    }
    const std::size_t digits = j - (i + 1);
    if (digits != 3 && digits != 6 && digits != 8) {
      out.push_back(svg[i]);
      ++i;
      continue;
    }
    std::string hex;
    if (digits == 3) {
      hex.push_back(RoboCompilerHexUpper(svg[i + 1]));
      hex.push_back(RoboCompilerHexUpper(svg[i + 1]));
      hex.push_back(RoboCompilerHexUpper(svg[i + 2]));
      hex.push_back(RoboCompilerHexUpper(svg[i + 2]));
      hex.push_back(RoboCompilerHexUpper(svg[i + 3]));
      hex.push_back(RoboCompilerHexUpper(svg[i + 3]));
    } else {
      for (std::size_t k = i + 1; k < j; ++k) {
        hex.push_back(RoboCompilerHexUpper(svg[k]));
      }
    }
    out += "#ROBO_C";
    out += hex;
    i = j;
  }
  return out;
}

void RoboCompilerAppendSvgAttr(std::string& out, const std::string& key, const std::string& value) {
  out += ' ';
  out += key;
  out += "=\"";
  for (char ch : value) {
    if (ch == '&') {
      out += "&amp;";
    } else if (ch == '"') {
      out += "&quot;";
    } else if (ch == '<') {
      out += "&lt;";
    } else {
      out.push_back(ch);
    }
  }
  out += '"';
}

bool RoboCompilerSvgAttrIsComponent(const std::string& key) {
  return key == "id" || key == "name" || key == "size" || key == "x" || key == "y" || key == "opacity" ||
         key == "position" || key == "left" || key == "top" || key == "right" || key == "bottom" || key == "margin" ||
         key == "margintop" || key == "marginright" || key == "marginbottom" || key == "marginleft" || key == "flex" ||
         key == "flexgrow" || key == "grow" || key == "alignself" || key == "foundation" || key == "fill" ||
         key == "stroke" || key == "accent" || key == "color" || key == "slot" || key == "side" || key == "place";
}

void RoboCompilerSerializeSvgNode(const RoboXMLNode& node, std::string& out, bool root) {
  out += '<';
  out += root ? "svg" : node.name;
  if (root) {
    bool hasXmlns = false;
    bool hasViewBox = false;
    for (const auto& kv : node.attrs) {
      const std::string key = RoboCompilerLower(kv.first);
      if (key == "xmlns") {
        hasXmlns = true;
      }
      if (key == "viewbox") {
        hasViewBox = true;
      }
    }
    if (!hasXmlns) {
      RoboCompilerAppendSvgAttr(out, "xmlns", "http://www.w3.org/2000/svg");
    }
    if (!hasViewBox) {
      const std::string width = RoboXMLAttr(node, "width", "24");
      const std::string height = RoboXMLAttr(node, "height", width.c_str());
      RoboCompilerAppendSvgAttr(out, "viewBox", "0 0 " + width + " " + height);
    }
  }
  for (const auto& kv : node.attrs) {
    if (root && RoboCompilerSvgAttrIsComponent(RoboCompilerLower(kv.first))) {
      continue;
    }
    if (kv.second.empty()) {
      continue;
    }
    RoboCompilerAppendSvgAttr(out, kv.first, kv.second);
  }
  if (node.children.empty() && node.text.empty()) {
    out += "/>";
    return;
  }
  out += '>';
  out += node.text;
  for (const auto& child : node.children) {
    RoboCompilerSerializeSvgNode(child, out, false);
  }
  out += "</";
  out += root ? "svg" : node.name;
  out += '>';
}

bool RoboCompilerCheckSvg(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
  std::string iconName = RoboXMLAttr(node, "id");
  if (iconName.empty()) {
    iconName = RoboXMLAttr(node, "name");
  }
  if (iconName.empty()) {
    iconName = "svg" + std::to_string(RoboCompilerTakeInlineSerial());
  }
  RoboCompilerBindSvgName(&node, iconName);
  if (RoboCompilerIconInCatalog(iconName.c_str())) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "inline svg id '" + iconName + "' matches an Assets icon. Use another id.");
    return false;
  }
  if (RoboCompilerInlineSvgTaken(iconName)) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "duplicate inline svg '" + iconName + "'");
    return false;
  }
  std::string svg;
  RoboCompilerSerializeSvgNode(node, svg, true);
  RoboCompilerPutInlineSvg(iconName, RoboCompilerBakeSvgColors(svg));
  RoboCompilerNoteUsedIcon(iconName);
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (!RoboCompilerIsLayoutKey(key) && key != "id" && key != "name" && key != "size" && key != "width" &&
        key != "height" && key != "x" && key != "y" && key != "opacity" && key != "fill" && key != "stroke" &&
        key != "accent" && key != "color" && key != "viewbox" && key != "xmlns" && key != "preserveaspectratio" &&
        key != "strokewidth" && key != "stroke-width" && key != "fillrule" && key != "fill-rule" &&
        key != "slot" && key != "side" && key != "place") {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "unknown svg attribute '" + kv.first + "'");
    }
  }
  if (node.children.empty()) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path, "<svg> has no shapes");
  }
  return true;
}
