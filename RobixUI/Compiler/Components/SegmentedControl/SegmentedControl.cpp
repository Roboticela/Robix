#include "SegmentedControl.hpp"

#include "../Button/Button.hpp"
#include "../../RoboCompilerShared.hpp"

#include <string>

namespace {

bool RoboCompilerSegmentVariantOk(const std::string& variant) {
  return RoboCompilerIsOneOf(variant, {"subtle", "outline", "ghost", "primary", "pill"});
}

bool RoboCompilerSegmentSizeOk(const std::string& size) {
  return RoboCompilerIsOneOf(size, {"sm", "md", "lg", "small", "medium", "large"});
}

bool RoboCompilerSegmentStatusOk(const std::string& status) {
  return status.empty() || RoboCompilerIsOneOf(status, {"idle", "loading", "success", "error"});
}

void RoboCompilerNoteIcon(const std::string& name) {
  if (!name.empty()) {
    RoboCompilerNoteUsedIcon(name);
  }
}

}  // namespace

bool RoboCompilerCheckSegment(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
  bool ok = true;
  if (RoboXMLAttr(node, "value").empty()) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path, "Segment requires a value");
    ok = false;
  }
  const std::string label = RoboXMLAttr(node, "label");
  if (label.empty() && RoboCompilerTrim(node.text).empty()) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path, "Segment requires a label");
    ok = false;
  }
  const std::string status = RoboCompilerLower(RoboXMLAttr(node, "status"));
  if (!RoboCompilerSegmentStatusOk(status)) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Segment status must be idle|loading|success|error");
    ok = false;
  }
  const std::string icon = RoboXMLAttr(node, "icon");
  if (!icon.empty() && !RoboCompilerIconInCatalog(icon.c_str())) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "Segment icon '" + icon + "' was not found in the icon pack");
    ok = false;
  } else {
    RoboCompilerNoteIcon(icon);
  }
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (!RoboCompilerIsLayoutKey(key) && key != "id" && key != "value" && key != "label" && key != "icon" &&
        key != "badge" && key != "disabled" && key != "loading" && key != "status" && key != "action" &&
        key != "name" && !RoboCompilerIsButtonStyleKey(key)) {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "unknown Segment attribute '" + kv.first + "'");
    }
  }
  for (const RoboXMLNode& child : node.children) {
    const std::string childName = RoboCompilerLower(child.name);
    if (childName == "icon" || childName == "svg") {
      ok = RoboCompilerCheckNode(child, path, result) && ok;
    } else if (!childName.empty()) {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "Segment child <" + child.name + "> is ignored");
    }
  }
  return ok;
}

bool RoboCompilerCheckSegmentedControl(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
  bool ok = true;
  const std::string variant = RoboCompilerLower(RoboXMLAttr(node, "variant", "subtle"));
  if (!RoboCompilerSegmentVariantOk(variant)) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "SegmentedControl variant must be subtle|outline|ghost|primary|pill");
    ok = false;
  }
  const std::string size = RoboCompilerLower(RoboXMLAttr(node, "size", "md"));
  if (!RoboCompilerSegmentSizeOk(size)) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "SegmentedControl size must be sm|md|lg");
    ok = false;
  }
  const std::string content = RoboCompilerLower(RoboXMLAttr(node, "content", "label"));
  if (!RoboCompilerIsOneOf(content, {"label", "icon", "both"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "SegmentedControl content must be label|icon|both");
    ok = false;
  }
  const std::string mode = RoboCompilerLower(RoboXMLAttr(node, "mode", "inset"));
  if (!RoboCompilerIsOneOf(mode, {"inset", "grouped"})) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "SegmentedControl mode must be inset|grouped");
    ok = false;
  }
  const std::string leading = RoboXMLAttr(node, "leading");
  const std::string trailing = RoboXMLAttr(node, "trailing");
  if (!leading.empty() && !RoboCompilerIconInCatalog(leading.c_str())) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "SegmentedControl leading icon '" + leading + "' was not found in the icon pack");
    ok = false;
  } else {
    RoboCompilerNoteIcon(leading);
  }
  if (!trailing.empty() && !RoboCompilerIconInCatalog(trailing.c_str())) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "SegmentedControl trailing icon '" + trailing + "' was not found in the icon pack");
    ok = false;
  } else {
    RoboCompilerNoteIcon(trailing);
  }
  int segments = 0;
  for (const RoboXMLNode& child : node.children) {
    const std::string childName = RoboCompilerLower(child.name);
    if (childName == "segment") {
      ok = RoboCompilerCheckSegment(child, path, result) && ok;
      ++segments;
    } else if (childName == "icon" || childName == "svg") {
      ok = RoboCompilerCheckNode(child, path, result) && ok;
    } else if (!childName.empty()) {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "SegmentedControl child <" + child.name + "> is ignored");
    }
  }
  if (segments == 0) {
    RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Error, path,
                        "SegmentedControl requires at least one Segment");
    ok = false;
  }
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (!RoboCompilerIsLayoutKey(key) && key != "id" && key != "label" && key != "value" && key != "variant" &&
        key != "size" && key != "content" && key != "mode" && key != "fullwidth" && key != "disabled" &&
        key != "leading" && key != "trailing" && key != "action" && key != "successms" && key != "name" &&
        !RoboCompilerIsButtonStyleKey(key)) {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "unknown SegmentedControl attribute '" + kv.first + "'");
    }
  }
  return ok;
}

bool RoboCompilerCheckSegmentedToolbar(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result) {
  bool ok = true;
  for (const auto& kv : node.attrs) {
    const std::string key = RoboCompilerLower(kv.first);
    if (!RoboCompilerIsLayoutKey(key) && key != "id" && key != "attached" && key != "name" &&
        !RoboCompilerIsButtonStyleKey(key)) {
      RoboCompilerAddDiag(result, RoboCompilerDiagnostic::Severity::Warning, path,
                          "unknown SegmentedToolbar attribute '" + kv.first + "'");
    }
  }
  for (const RoboXMLNode& child : node.children) {
    ok = RoboCompilerCheckNode(child, path, result) && ok;
  }
  return ok;
}
