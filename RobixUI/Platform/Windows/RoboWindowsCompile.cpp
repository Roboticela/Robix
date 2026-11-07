#include "RoboWindowsCompile.hpp"

#include "../../Foundation/FoundationPlatform.hpp"

#include "../../Components/Board/Board.hpp"
#include "../../Components/Container/Container.hpp"
#include "../../Components/ScrollView/ScrollView.hpp"
#include "../../Components/Text/Text.hpp"
#include "../../RoboCompilerShared.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

int RoboWindowsParsePx(const std::string& value, int fallback) {
  if (value.empty()) {
    return fallback;
  }
  int number = 0;
  bool any = false;
  for (char character : value) {
    if (character < '0' || character > '9') {
      break;
    }
    any = true;
    number = number * 10 + (character - '0');
  }
  return any ? number : fallback;
}

std::string RoboWindowsFontFace(const RoboXMLNode& node, RoboCompilerTextKind kind, RoboU32 level) {
  std::string font = RoboXMLAttr(node, "font");
  if (font.empty()) {
    font = RoboXMLAttr(node, "face");
  }
  if (font.empty()) {
    font = RoboXMLAttr(node, "fontFamily");
  }
  const std::string lower = RoboCompilerLower(font);
  const char* token = nullptr;
  if (lower == "sans" || lower == "inter" || lower == "inter-regular") {
    token = "sans";
  } else if (lower == "sans-medium" || lower == "inter-medium") {
    token = "sans-medium";
  } else if (lower == "sans-semibold" || lower == "inter-semibold") {
    token = "sans-semibold";
  } else if (lower == "sans-bold" || lower == "inter-bold") {
    token = "sans-bold";
  } else if (lower == "sans-italic" || lower == "inter-italic") {
    token = "sans-italic";
  }
  if (token != nullptr) {
    return RoboFoundationPlatformFont(token);
  }
  if (!font.empty()) {
    return font;
  }
  return RoboCompilerTextFont(kind, level);
}

std::string RoboWindowsContent(const RoboXMLNode& node) {
  std::string spans;
  bool hasSpan = false;
  for (const RoboXMLNode& child : node.children) {
    if (RoboCompilerLower(child.name) != "span") {
      continue;
    }
    hasSpan = true;
    const std::string spanText = child.text.empty() ? RoboXMLAttr(child, "content") : child.text;
    spans += spanText;
  }
  if (hasSpan) {
    return spans;
  }
  const std::string content = RoboXMLAttr(node, "content");
  if (!content.empty()) {
    return content;
  }
  return RoboCompilerTrim(node.text);
}

bool RoboWindowsDescend(const RoboXMLNode& node) {
  const std::string name = RoboCompilerLower(node.name);
  return name == "screen" || RoboCompilerIsContainerName(node.name) || RoboCompilerIsScrollViewName(node.name) ||
         RoboCompilerIsBoardName(node.name);
}

bool RoboWindowsIsContainerTag(const std::string& name) {
  const std::string lower = RoboCompilerLower(name);
  return lower == "container" || lower == "box" || lower == "flex" || lower == "stack" || lower == "canvas" ||
         lower == "screen";
}

struct RoboWindowsNode {
  bool container = false;
  RoboCompilerTextKind kind = RoboCompilerTextKind::Text;
  RoboU32 level = 1;
  bool small = false;
  std::string font;
  std::string content;
  std::string display = "flex";
  std::string direction = "column";
  std::string justify = "start";
  std::string align = "stretch";
  std::string wrap = "nowrap";
  int gap = 0;
  int rowGap = -1;
  int columnGap = -1;
  int padTop = 0;
  int padRight = 0;
  int padBottom = 0;
  int padLeft = 0;
  int maxWidth = 0;
  std::string background;
  bool icon = false;
  std::string iconName;
  int iconSize = 24;
  std::string fill;
  std::string stroke;
  std::string accent;
  bool button = false;
  std::string variant = "primary";
  std::string buttonSize = "md";
  std::string preview;
  bool fullWidth = true;
  bool loading = false;
  bool disabled = false;
  std::string loadingText;
  std::string leading;
  std::string trailing;
  bool iconButton = false;
  std::string badge;
  std::string status;
  std::string action;
  int successMs = 1600;
  bool textField = false;
  std::string placeholder;
  std::string labelText;
  std::string hint;
  std::string fieldType = "text";
  bool invalid = false;
  bool valid = false;
  bool readOnly = false;
  bool required = false;
  bool optional = false;
  int maxLength = 0;
  bool switchControl = false;
  bool switchItem = false;
  bool switchGroup = false;
  bool checked = false;
  bool divider = false;
  std::vector<int> children;
};

void RoboWindowsReadBox(const RoboXMLNode& node, RoboWindowsNode& item) {
  const std::string name = RoboCompilerLower(node.name);
  item.container = true;
  item.display = RoboXMLAttr(node, "display");
  if (item.display.empty()) {
    item.display = (name == "stack" || name == "canvas") ? name : "flex";
  }
  item.direction = RoboXMLAttr(node, "direction");
  if (item.direction.empty()) {
    item.direction = RoboXMLAttr(node, "flexDirection", "column");
  }
  item.justify = RoboXMLAttr(node, "justify");
  if (item.justify.empty()) {
    item.justify = RoboXMLAttr(node, "justifyContent", "start");
  }
  item.align = RoboXMLAttr(node, "align");
  if (item.align.empty()) {
    item.align = RoboXMLAttr(node, "alignItems", "stretch");
  }
  item.wrap = RoboXMLAttr(node, "wrap");
  if (item.wrap.empty()) {
    item.wrap = RoboXMLAttr(node, "flexWrap", "nowrap");
  }
  const bool root = name == "screen";
  const int gap = RoboWindowsParsePx(RoboXMLAttr(node, "gap"), root ? 24 : 0);
  item.gap = gap;
  item.rowGap = RoboXMLAttr(node, "rowGap").empty() ? -1 : RoboWindowsParsePx(RoboXMLAttr(node, "rowGap"), gap);
  const std::string columnGap = RoboXMLAttr(node, "columnGap", RoboXMLAttr(node, "colGap").c_str());
  item.columnGap = columnGap.empty() ? -1 : RoboWindowsParsePx(columnGap, gap);
  const int padding = RoboWindowsParsePx(RoboXMLAttr(node, "padding"), root ? 16 : 0);
  item.padTop = RoboXMLAttr(node, "paddingTop").empty() ? padding : RoboWindowsParsePx(RoboXMLAttr(node, "paddingTop"), padding);
  item.padRight =
      RoboXMLAttr(node, "paddingRight").empty() ? padding : RoboWindowsParsePx(RoboXMLAttr(node, "paddingRight"), padding);
  item.padBottom = RoboXMLAttr(node, "paddingBottom").empty()
                       ? padding
                       : RoboWindowsParsePx(RoboXMLAttr(node, "paddingBottom"), padding);
  item.padLeft =
      RoboXMLAttr(node, "paddingLeft").empty() ? padding : RoboWindowsParsePx(RoboXMLAttr(node, "paddingLeft"), padding);
  item.maxWidth = RoboWindowsParsePx(RoboXMLAttr(node, "maxWidth"), 0);
  item.background = RoboXMLAttr(node, "background");
  if (item.background.empty()) {
    item.background = RoboXMLAttr(node, "backgroundColor");
  }
  if (item.background.empty()) {
    item.background = RoboXMLAttr(node, "bg");
  }
}

int RoboWindowsAdd(std::vector<RoboWindowsNode>& nodes, RoboWindowsNode item) {
  const int index = static_cast<int>(nodes.size());
  nodes.push_back(std::move(item));
  return index;
}

bool RoboWindowsFlag(const std::string& value) {
  const std::string flag = RoboCompilerLower(value);
  return flag == "true" || flag == "1" || flag == "yes" || flag == "on";
}

void RoboWindowsReadSwitch(const RoboXMLNode& node, RoboWindowsNode& item, bool row) {
  item.switchControl = true;
  item.switchItem = row;
  item.content = row ? RoboXMLAttr(node, "title") : RoboXMLAttr(node, "label");
  item.hint = row ? RoboXMLAttr(node, "description") : "";
  item.variant = RoboXMLAttr(node, "variant", "primary");
  item.buttonSize = RoboXMLAttr(node, "size", "md");
  item.checked = RoboWindowsFlag(RoboXMLAttr(node, "checked"));
  item.loading = RoboWindowsFlag(RoboXMLAttr(node, "loading"));
  item.disabled = RoboWindowsFlag(RoboXMLAttr(node, "disabled"));
}

void RoboWindowsReadButton(const RoboXMLNode& node, RoboWindowsNode& item) {
  item.button = true;
  item.content = RoboWindowsContent(node);
  item.variant = RoboXMLAttr(node, "variant", "primary");
  item.buttonSize = RoboXMLAttr(node, "size", "md");
  item.preview = RoboXMLAttr(node, "preview");
  const std::string full = RoboCompilerLower(RoboXMLAttr(node, "fullWidth", "true"));
  item.fullWidth = full != "false" && full != "0" && full != "no" && full != "off";
  item.loading = RoboWindowsFlag(RoboXMLAttr(node, "loading"));
  item.disabled = RoboWindowsFlag(RoboXMLAttr(node, "disabled")) || item.loading;
  item.loadingText = RoboXMLAttr(node, "loadingText");
  item.leading = RoboXMLAttr(node, "leading");
  item.trailing = RoboXMLAttr(node, "trailing");
  for (const RoboXMLNode& child : node.children) {
    const std::string name = RoboCompilerLower(child.name);
    if (name != "icon" && name != "svg" && !RoboCompilerIsIconNode(child)) {
      continue;
    }
    const std::string iconName = name == "svg" ? RoboCompilerSvgIconName(child) : RoboCompilerIconNameFromNode(child);
    const std::string side = RoboCompilerLower(
        RoboXMLAttr(child, "slot", RoboXMLAttr(child, "side", RoboXMLAttr(child, "place").c_str()).c_str()));
    if (side == "trailing" || side == "right" || side == "end") {
      item.trailing = iconName;
    } else if (item.leading.empty()) {
      item.leading = iconName;
    } else {
      item.trailing = iconName;
    }
  }
}

bool RoboWindowsIsIcon(const RoboXMLNode& node) {
  const std::string name = RoboCompilerLower(node.name);
  return name == "icon" || name == "svg" || RoboCompilerIsIconNode(node);
}

void RoboWindowsReadIcon(const RoboXMLNode& node, RoboWindowsNode& item) {
  item.icon = true;
  const bool svg = RoboCompilerLower(node.name) == "svg";
  item.iconName = svg ? RoboCompilerSvgIconName(node) : RoboCompilerIconNameFromNode(node);
  const std::string size = RoboXMLAttr(node, "size");
  if (!size.empty()) {
    item.iconSize = RoboWindowsParsePx(size, 24);
  } else {
    item.iconSize = RoboWindowsParsePx(RoboXMLAttr(node, "width", RoboXMLAttr(node, "height", "24").c_str()), 24);
  }
  if (item.iconSize < 1) {
    item.iconSize = 24;
  }
  item.fill = RoboXMLAttr(node, "fill");
  if (item.fill.empty()) {
    item.fill = RoboXMLAttr(node, "color");
  }
  item.stroke = RoboXMLAttr(node, "stroke");
  item.accent = RoboXMLAttr(node, "accent");
}

void RoboWindowsReadIconButton(const RoboXMLNode& node, RoboWindowsNode& item) {
  item.iconButton = true;
  item.content = RoboXMLAttr(node, "label");
  item.variant = RoboXMLAttr(node, "variant", "ghost");
  item.buttonSize = RoboXMLAttr(node, "size", "md");
  item.iconName = RoboXMLAttr(node, "icon");
  if (item.iconName.empty()) {
    item.iconName = RoboXMLAttr(node, "name");
  }
  item.badge = RoboXMLAttr(node, "badge");
  item.status = RoboCompilerLower(RoboXMLAttr(node, "status"));
  if (item.status.empty() && RoboWindowsFlag(RoboXMLAttr(node, "loading"))) {
    item.status = "loading";
  }
  item.action = RoboXMLAttr(node, "action");
  item.successMs = RoboWindowsParsePx(RoboXMLAttr(node, "successMs", "1600"), 1600);
  if (item.successMs <= 0) {
    item.successMs = 1600;
  }
  item.disabled = RoboWindowsFlag(RoboXMLAttr(node, "disabled")) || item.status == "loading";
  item.iconSize = RoboCompilerLower(item.buttonSize) == "sm" || RoboCompilerLower(item.buttonSize) == "small" ? 16 : 18;
  for (const RoboXMLNode& child : node.children) {
    const std::string name = RoboCompilerLower(child.name);
    if (name != "icon" && name != "svg" && !RoboCompilerIsIconNode(child)) {
      continue;
    }
    const std::string iconName = name == "svg" ? RoboCompilerSvgIconName(child) : RoboCompilerIconNameFromNode(child);
    if (!iconName.empty()) {
      item.iconName = iconName;
      break;
    }
  }
}

void RoboWindowsReadTextField(const RoboXMLNode& node, RoboWindowsNode& item) {
  item.textField = true;
  item.content = RoboXMLAttr(node, "value", RoboXMLAttr(node, "content").c_str());
  item.placeholder = RoboXMLAttr(node, "placeholder");
  item.labelText = RoboXMLAttr(node, "label");
  item.hint = RoboXMLAttr(node, "hint");
  item.trailing = RoboXMLAttr(node, "trailing");
  item.variant = RoboXMLAttr(node, "variant", "outline");
  item.buttonSize = RoboXMLAttr(node, "size", "md");
  item.preview = RoboXMLAttr(node, "preview");
  item.fieldType = RoboXMLAttr(node, "type", "text");
  item.iconName = RoboXMLAttr(node, "icon");
  if (item.iconName.empty()) {
    item.iconName = RoboXMLAttr(node, "name");
  }
  item.invalid = RoboWindowsFlag(RoboXMLAttr(node, "invalid"));
  item.valid = RoboWindowsFlag(RoboXMLAttr(node, "valid"));
  item.disabled = RoboWindowsFlag(RoboXMLAttr(node, "disabled"));
  item.readOnly = RoboWindowsFlag(RoboXMLAttr(node, "readOnly")) || RoboWindowsFlag(RoboXMLAttr(node, "readonly"));
  item.required = RoboWindowsFlag(RoboXMLAttr(node, "required"));
  item.optional = RoboWindowsFlag(RoboXMLAttr(node, "optional"));
  item.maxLength = RoboWindowsParsePx(RoboXMLAttr(node, "maxLength"), 0);
  item.maxWidth = RoboWindowsParsePx(RoboXMLAttr(node, "maxWidth"), 0);
  for (const RoboXMLNode& child : node.children) {
    const std::string name = RoboCompilerLower(child.name);
    if (name != "icon" && name != "svg" && !RoboCompilerIsIconNode(child)) {
      continue;
    }
    const std::string iconName = name == "svg" ? RoboCompilerSvgIconName(child) : RoboCompilerIconNameFromNode(child);
    if (!iconName.empty()) {
      item.iconName = iconName;
      break;
    }
  }
}

void RoboWindowsCollectInto(const RoboXMLNode& node, std::vector<RoboWindowsNode>& nodes, std::vector<int>& parent) {
  const std::string tag = RoboCompilerLower(node.name);
  if (tag == "switch") {
    RoboWindowsNode item;
    RoboWindowsReadSwitch(node, item, false);
    parent.push_back(RoboWindowsAdd(nodes, std::move(item)));
    return;
  }
  if (tag == "switchitem") {
    RoboWindowsNode item;
    RoboWindowsReadSwitch(node, item, true);
    parent.push_back(RoboWindowsAdd(nodes, std::move(item)));
    return;
  }
  if (tag == "switchgroup") {
    RoboWindowsNode item;
    RoboWindowsReadBox(node, item);
    item.switchGroup = true;
    item.display = "flex";
    item.direction = "column";
    item.align = "stretch";
    item.justify = "start";
    item.gap = 0;
    item.rowGap = 0;
    item.columnGap = 0;
    const int index = RoboWindowsAdd(nodes, std::move(item));
    std::vector<int> children;
    for (const RoboXMLNode& child : node.children) {
      RoboWindowsCollectInto(child, nodes, children);
    }
    bool seenItem = false;
    for (int child : children) {
      RoboWindowsNode& childNode = nodes[static_cast<size_t>(child)];
      if (!childNode.switchItem) {
        continue;
      }
      childNode.divider = seenItem;
      seenItem = true;
    }
    if (!children.empty()) {
      nodes[static_cast<size_t>(index)].children = std::move(children);
      parent.push_back(index);
    }
    return;
  }
  if (tag == "textfield" || tag == "input") {
    RoboWindowsNode item;
    RoboWindowsReadTextField(node, item);
    parent.push_back(RoboWindowsAdd(nodes, std::move(item)));
    return;
  }
  if (tag == "iconbutton") {
    RoboWindowsNode item;
    RoboWindowsReadIconButton(node, item);
    parent.push_back(RoboWindowsAdd(nodes, std::move(item)));
    return;
  }
  if (RoboCompilerLower(node.name) == "button") {
    RoboWindowsNode item;
    RoboWindowsReadButton(node, item);
    parent.push_back(RoboWindowsAdd(nodes, std::move(item)));
    return;
  }
  if (RoboWindowsIsIcon(node)) {
    RoboWindowsNode item;
    RoboWindowsReadIcon(node, item);
    if (!item.iconName.empty()) {
      parent.push_back(RoboWindowsAdd(nodes, std::move(item)));
    }
    return;
  }
  if (RoboCompilerIsTextLike(node)) {
    const std::string content = RoboWindowsContent(node);
    if (content.empty()) {
      return;
    }
    RoboWindowsNode item;
    item.kind = RoboCompilerClassifyTextNode(node, &item.level);
    const std::string size = RoboCompilerLower(RoboXMLAttr(node, "size"));
    item.small = size == "sm" || size == "small";
    item.font = RoboWindowsFontFace(node, item.kind, item.level);
    item.content = content;
    parent.push_back(RoboWindowsAdd(nodes, std::move(item)));
    return;
  }
  if (RoboWindowsIsContainerTag(node.name)) {
    RoboWindowsNode item;
    RoboWindowsReadBox(node, item);
    const int index = RoboWindowsAdd(nodes, std::move(item));
    std::vector<int> children;
    for (const RoboXMLNode& child : node.children) {
      RoboWindowsCollectInto(child, nodes, children);
    }
    if (children.empty() && RoboCompilerLower(node.name) != "screen") {
      return;
    }
    nodes[static_cast<size_t>(index)].children = std::move(children);
    parent.push_back(index);
    return;
  }
  if (!RoboWindowsDescend(node)) {
    return;
  }
  for (const RoboXMLNode& child : node.children) {
    RoboWindowsCollectInto(child, nodes, parent);
  }
}

}  // namespace

bool RoboCompilerWriteWindows(const RoboXMLNode& screen, const RoboCompilerOptions& options, RoboCompilerResult& result) {
  std::vector<RoboWindowsNode> nodes;
  std::vector<int> roots;
  RoboWindowsCollectInto(screen, nodes, roots);

  std::vector<int> childIds;
  for (RoboWindowsNode& node : nodes) {
    const int begin = static_cast<int>(childIds.size());
    for (int child : node.children) {
      childIds.push_back(child);
    }
    node.children.clear();
    node.children.push_back(begin);
    node.children.push_back(static_cast<int>(childIds.size()) - begin);
  }

  std::ostringstream cpp;
  cpp << "/* Auto-generated by RoboCompiler from the app XML. Implemented components only. */\n";
  cpp << "#include \"RoboWindowsCompiled.hpp\"\n\n";
  cpp << "static const int kRoboWindowsChildIds[] = {\n";
  if (childIds.empty()) {
    cpp << "  0\n";
  }
  for (int child : childIds) {
    cpp << "  " << child << ",\n";
  }
  cpp << "};\n\n";
  cpp << "static const RoboWindowsCompiledNode kRoboWindowsNodes[] = {\n";
  if (nodes.empty()) {
    cpp << "  {1, 0, 0, 0, 1u, 0, \"\", \"\", \"flex\", \"column\", \"start\", \"stretch\", \"nowrap\", 24, -1, -1, "
           "16, 16, 16, 16, 0, \"\", 0, \"\", 0, \"\", \"\", \"\", 0, \"primary\", \"md\", \"\", 1, 0, 0, \"\", \"\", "
           "\"\", 0, \"\", \"\", \"\", 1600, 0, \"\", \"\", \"\", \"text\", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}\n";
  }
  for (size_t index = 0; index < nodes.size(); ++index) {
    const RoboWindowsNode& node = nodes[index];
    const int childBegin = node.children.size() == 2 ? node.children[0] : 0;
    const int childCount = node.children.size() == 2 ? node.children[1] : 0;
    cpp << "  {" << (node.container ? 1 : 0) << ", " << childBegin << ", " << childCount << ", "
        << static_cast<int>(node.kind) << ", " << node.level << "u, " << (node.small ? 1 : 0) << ", \""
        << RoboCompilerCppEscape(node.font) << "\", \"" << RoboCompilerCppEscape(node.content) << "\", \""
        << RoboCompilerCppEscape(node.display) << "\", \"" << RoboCompilerCppEscape(node.direction) << "\", \""
        << RoboCompilerCppEscape(node.justify) << "\", \"" << RoboCompilerCppEscape(node.align) << "\", \""
        << RoboCompilerCppEscape(node.wrap) << "\", " << node.gap << ", " << node.rowGap << ", " << node.columnGap
        << ", " << node.padTop << ", " << node.padRight << ", " << node.padBottom << ", " << node.padLeft << ", "
        << node.maxWidth << ", \"" << RoboCompilerCppEscape(node.background) << "\", " << (node.icon ? 1 : 0) << ", \""
        << RoboCompilerCppEscape(node.iconName) << "\", " << node.iconSize << ", \"" << RoboCompilerCppEscape(node.fill)
        << "\", \"" << RoboCompilerCppEscape(node.stroke) << "\", \"" << RoboCompilerCppEscape(node.accent) << "\", "
        << (node.button ? 1 : 0) << ", \"" << RoboCompilerCppEscape(node.variant) << "\", \""
        << RoboCompilerCppEscape(node.buttonSize) << "\", \"" << RoboCompilerCppEscape(node.preview) << "\", "
        << (node.fullWidth ? 1 : 0) << ", " << (node.loading ? 1 : 0) << ", " << (node.disabled ? 1 : 0) << ", \""
        << RoboCompilerCppEscape(node.loadingText) << "\", \"" << RoboCompilerCppEscape(node.leading) << "\", \""
        << RoboCompilerCppEscape(node.trailing) << "\", " << (node.iconButton ? 1 : 0) << ", \""
        << RoboCompilerCppEscape(node.badge) << "\", \"" << RoboCompilerCppEscape(node.status) << "\", \""
        << RoboCompilerCppEscape(node.action) << "\", " << node.successMs << ", " << (node.textField ? 1 : 0) << ", \""
        << RoboCompilerCppEscape(node.placeholder) << "\", \"" << RoboCompilerCppEscape(node.labelText) << "\", \""
        << RoboCompilerCppEscape(node.hint) << "\", \"" << RoboCompilerCppEscape(node.fieldType) << "\", "
        << (node.invalid ? 1 : 0) << ", " << (node.valid ? 1 : 0) << ", " << (node.readOnly ? 1 : 0) << ", "
        << (node.required ? 1 : 0) << ", " << (node.optional ? 1 : 0) << ", " << node.maxLength << ", "
        << (node.switchControl ? 1 : 0) << ", " << (node.switchItem ? 1 : 0) << ", " << (node.switchGroup ? 1 : 0)
        << ", " << (node.checked ? 1 : 0) << ", " << (node.divider ? 1 : 0) << "},\n";
    (void)index;
  }
  cpp << "};\n\n";
  cpp << "void RoboWindowsCompiledScreenGet(RoboWindowsCompiledScreen* screen) {\n";
  cpp << "  if (screen == nullptr) {\n";
  cpp << "    return;\n";
  cpp << "  }\n";
  cpp << "  screen->nodes = kRoboWindowsNodes;\n";
  cpp << "  screen->count = " << (nodes.empty() ? 1 : static_cast<int>(nodes.size())) << ";\n";
  cpp << "  screen->childIds = kRoboWindowsChildIds;\n";
  cpp << "  screen->childCount = " << childIds.size() << ";\n";
  cpp << "}\n";

  const std::filesystem::path outRoot(options.outputRoot);
  const std::filesystem::path generated = outRoot / "generated" / "DevKitScreen.gen.cpp";
  std::error_code error;
  std::filesystem::create_directories(generated.parent_path(), error);
  std::ofstream file(generated, std::ios::binary | std::ios::trunc);
  if (!file) {
    result.ok = false;
    return false;
  }
  const std::string text = cpp.str();
  file.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (!file) {
    result.ok = false;
    return false;
  }
  result.generatedCppPath = generated.string();
  result.ok = true;
  return true;
}
