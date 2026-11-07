#include "RoboFoundation.hpp"

#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

static std::string RoboFoundationLower(std::string value) {
  for (char& ch : value) {
    if (ch >= 'A' && ch <= 'Z') {
      ch = static_cast<char>(ch - 'A' + 'a');
    }
  }
  return value;
}

static std::string RoboFoundationTrim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
    value.erase(value.begin());
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }
  return value;
}

static std::string RoboFoundationHexByte(int value) {
  static const char* digits = "0123456789ABCDEF";
  if (value < 0) {
    value = 0;
  }
  if (value > 255) {
    value = 255;
  }
  std::string out;
  out.push_back(digits[(value >> 4) & 0xF]);
  out.push_back(digits[value & 0xF]);
  return out;
}

static std::string RoboFoundationColorHex(const std::string& text) {
  const std::string raw = RoboFoundationTrim(text);
  if (raw.empty()) {
    return {};
  }
  if (raw[0] == '#') {
    if (raw.size() == 4) {
      return std::string("#") + raw[1] + raw[1] + raw[2] + raw[2] + raw[3] + raw[3];
    }
    std::string out = "#";
    for (std::size_t i = 1; i < raw.size(); ++i) {
      const char ch = raw[i];
      if (ch >= 'a' && ch <= 'f') {
        out.push_back(static_cast<char>(ch - 'a' + 'A'));
      } else {
        out.push_back(ch);
      }
    }
    return out;
  }
  const std::string lower = RoboFoundationLower(raw);
  if (lower.rfind("rgba", 0) != 0 && lower.rfind("rgb", 0) != 0) {
    return raw;
  }
  const auto open = raw.find('(');
  const auto close = raw.find(')');
  if (open == std::string::npos || close == std::string::npos || close < open) {
    return raw;
  }
  std::string body = raw.substr(open + 1, close - open - 1);
  for (char& ch : body) {
    if (ch == ',') {
      ch = ' ';
    }
  }
  std::istringstream in(body);
  double r = 0;
  double g = 0;
  double b = 0;
  double a = 1;
  if (!(in >> r >> g >> b)) {
    return raw;
  }
  if (!(in >> a)) {
    a = 1;
  }
  const int ri = static_cast<int>(std::lround(r));
  const int gi = static_cast<int>(std::lround(g));
  const int bi = static_cast<int>(std::lround(b));
  const int ai = static_cast<int>(std::lround(a * 255.0));
  if (ai >= 255) {
    return "#" + RoboFoundationHexByte(ri) + RoboFoundationHexByte(gi) + RoboFoundationHexByte(bi);
  }
  return "#" + RoboFoundationHexByte(ai) + RoboFoundationHexByte(ri) + RoboFoundationHexByte(gi) +
         RoboFoundationHexByte(bi);
}

static RoboFoundationSet* RoboFoundationFind(RoboFoundationLibrary& library, const std::string& name) {
  const std::string key = RoboFoundationLower(name);
  for (auto& set : library.sets) {
    if (RoboFoundationLower(set.name) == key) {
      return &set;
    }
  }
  return nullptr;
}

static const RoboFoundationSet* RoboFoundationFindConst(const RoboFoundationLibrary& library, const std::string& name) {
  const std::string key = RoboFoundationLower(name);
  for (const auto& set : library.sets) {
    if (RoboFoundationLower(set.name) == key) {
      return &set;
    }
  }
  return nullptr;
}

static void RoboFoundationPut(RoboFoundationSet& set, RoboFoundationToken token) {
  token.name = RoboFoundationLower(token.name);
  if (token.name.empty()) {
    return;
  }
  set.tokens[token.name] = std::move(token);
}

static bool RoboFoundationReadFile(const std::string& path, std::string& text) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  text = ss.str();
  return true;
}

static void RoboFoundationIngest(RoboFoundationSet& set, const RoboXMLNode& node) {
  const std::string kind = RoboFoundationLower(node.name);
  if ((kind == "colors" || kind == "typography" || kind == "spacing" || kind == "radius" || kind == "layout" ||
       kind == "fonts") &&
      RoboXMLAttr(node, "name").empty()) {
    for (const auto& child : node.children) {
      RoboFoundationIngest(set, child);
    }
    return;
  }
  if (kind == "color") {
    RoboFoundationToken token;
    token.name = RoboXMLAttr(node, "name");
    token.kind = RoboFoundationKind::Color;
    token.dark = RoboXMLAttr(node, "dark", RoboXMLAttr(node, "value").c_str());
    token.light = RoboXMLAttr(node, "light", token.dark.c_str());
    RoboFoundationPut(set, token);
    return;
  }
  if (kind == "type") {
    RoboFoundationToken token;
    token.name = RoboXMLAttr(node, "name");
    token.kind = RoboFoundationKind::Type;
    token.size = RoboXMLAttr(node, "size");
    token.mobileSize = RoboXMLAttr(node, "mobileSize", token.size.c_str());
    token.weight = RoboXMLAttr(node, "weight");
    token.tracking = RoboXMLAttr(node, "tracking");
    token.leading = RoboXMLAttr(node, "leading");
    RoboFoundationPut(set, token);
    return;
  }
  if (kind == "space") {
    RoboFoundationToken token;
    token.name = RoboXMLAttr(node, "name");
    token.kind = RoboFoundationKind::Space;
    token.value = RoboXMLAttr(node, "value");
    RoboFoundationPut(set, token);
    return;
  }
  if (kind == "radius") {
    RoboFoundationToken token;
    token.name = RoboXMLAttr(node, "name");
    token.kind = RoboFoundationKind::Radius;
    token.value = RoboXMLAttr(node, "value");
    RoboFoundationPut(set, token);
    return;
  }
  if (kind == "font") {
    RoboFoundationToken token;
    token.name = RoboXMLAttr(node, "name");
    token.kind = RoboFoundationKind::Font;
    token.value = RoboXMLAttr(node, "src", RoboXMLAttr(node, "file").c_str());
    token.face = RoboXMLAttr(node, "face");
    if (token.face.empty()) {
      token.face = token.name;
    }
    token.face = RoboFoundationLower(token.face);
    RoboFoundationPut(set, token);
    return;
  }
  if (kind == "breakpoint" || kind == "width") {
    RoboFoundationToken token;
    token.name = RoboXMLAttr(node, "name");
    token.kind = RoboFoundationKind::Layout;
    token.value = RoboXMLAttr(node, "value");
    token.padding = RoboXMLAttr(node, "padding");
    token.columns = RoboXMLAttr(node, "columns");
    token.gutter = RoboXMLAttr(node, "gutter");
    RoboFoundationPut(set, token);
  }
}

static bool RoboFoundationApplyCopy(RoboFoundationSet& dest, const RoboFoundationLibrary& library, const RoboXMLNode& node,
                                    std::string& error) {
  const std::string from = RoboXMLAttr(node, "from");
  const RoboFoundationSet* source = RoboFoundationFindConst(library, from);
  if (source == nullptr) {
    error = "Copy source '" + from + "' was not found";
    return false;
  }
  const std::string tokenName = RoboFoundationLower(RoboXMLAttr(node, "token", RoboXMLAttr(node, "color").c_str()));
  const std::string group = RoboFoundationLower(RoboXMLAttr(node, "group"));
  if (!tokenName.empty()) {
    const auto it = source->tokens.find(tokenName);
    if (it == source->tokens.end()) {
      error = "token '" + tokenName + "' is not in " + source->name;
      return false;
    }
    dest.tokens[tokenName] = it->second;
    return true;
  }
  RoboFoundationKind only = RoboFoundationKind::Color;
  bool filter = false;
  if (group == "color" || group == "colors") {
    only = RoboFoundationKind::Color;
    filter = true;
  } else if (group == "type" || group == "typography") {
    only = RoboFoundationKind::Type;
    filter = true;
  } else if (group == "space" || group == "spacing") {
    only = RoboFoundationKind::Space;
    filter = true;
  } else if (group == "radius") {
    only = RoboFoundationKind::Radius;
    filter = true;
  } else if (group == "layout") {
    only = RoboFoundationKind::Layout;
    filter = true;
  } else if (group == "font" || group == "fonts") {
    only = RoboFoundationKind::Font;
    filter = true;
  }
  for (const auto& item : source->tokens) {
    if (filter && item.second.kind != only) {
      continue;
    }
    dest.tokens[item.first] = item.second;
  }
  return true;
}

bool RoboFoundationLoadNode(const RoboXMLNode& node, RoboFoundationLibrary& library, std::string& error) {
  const std::string root = RoboFoundationLower(node.name);
  if (root != "foundation" && root != "robofoundation") {
    error = "foundation root must be <Foundation>";
    return false;
  }
  const std::string name = RoboXMLAttr(node, "name");
  if (name.empty()) {
    error = "foundation name is required";
    return false;
  }
  if (RoboFoundationLower(name) == "robofoundation" && RoboXMLAttr(node, "sealed") != "true") {
    error = "RoboFoundation is sealed. Define a new Foundation from it instead of replacing it.";
    return false;
  }
  if (RoboFoundationFind(library, name) != nullptr && RoboFoundationLower(name) != "robofoundation") {
    error = "foundation '" + name + "' is already defined";
    return false;
  }
  RoboFoundationSet set;
  set.name = name;
  set.sealed = RoboXMLAttr(node, "sealed") == "true";
  const std::string from = RoboXMLAttr(node, "from");
  if (!from.empty()) {
    const RoboFoundationSet* source = RoboFoundationFindConst(library, from);
    if (source == nullptr) {
      error = "foundation '" + name + "' cannot extend '" + from + "'";
      return false;
    }
    set.tokens = source->tokens;
  }
  for (const auto& child : node.children) {
    const std::string childName = RoboFoundationLower(child.name);
    if (childName == "copy") {
      RoboXMLNode copy = child;
      if (RoboXMLAttr(copy, "from").empty()) {
        copy.attrs["from"] = from;
      }
      if (RoboXMLAttr(copy, "from").empty()) {
        error = "Copy needs from=\"RoboFoundation\" or a parent from=\"...\"";
        return false;
      }
      if (!RoboFoundationApplyCopy(set, library, copy, error)) {
        return false;
      }
      continue;
    }
    RoboFoundationIngest(set, child);
  }
  if (RoboFoundationSet* existing = RoboFoundationFind(library, name)) {
    *existing = std::move(set);
  } else {
    library.sets.push_back(std::move(set));
  }
  return true;
}

bool RoboFoundationLoadFile(const std::string& path, RoboFoundationLibrary& library, bool sealed, std::string& error) {
  std::string text;
  if (!RoboFoundationReadFile(path, text)) {
    error = "cannot read " + path;
    return false;
  }
  RoboXMLDocument doc;
  if (!RoboXMLParse(text, doc)) {
    error = path + ": " + (doc.errors.empty() ? "invalid XML" : doc.errors.front());
    return false;
  }
  if (sealed) {
    doc.root.attrs["sealed"] = "true";
    if (RoboXMLAttr(doc.root, "name").empty()) {
      doc.root.attrs["name"] = "RoboFoundation";
    }
  }
  return RoboFoundationLoadNode(doc.root, library, error);
}

enum class RoboFoundationSlot {
  None,
  Color,
  Size,
  Weight,
  Tracking,
  Leading,
  Space,
  Radius,
  ContentWidth,
  Gutter,
  Font
};

static RoboFoundationSlot RoboFoundationSlotFor(const std::string& key) {
  if (key == "background" || key == "backgroundcolor" || key == "bg" || key == "bordercolor" || key == "color" ||
      key == "from" || key == "to" || key == "thumbcolor" || key == "trackcolor") {
    return RoboFoundationSlot::Color;
  }
  if (key == "size" || key == "fontsize") {
    return RoboFoundationSlot::Size;
  }
  if (key == "weight" || key == "fontweight") {
    return RoboFoundationSlot::Weight;
  }
  if (key == "tracking" || key == "letterspacing") {
    return RoboFoundationSlot::Tracking;
  }
  if (key == "leading" || key == "lineheight") {
    return RoboFoundationSlot::Leading;
  }
  if (key == "gap" || key == "rowgap" || key == "columngap" || key == "colgap" || key == "padding" ||
      key == "paddingtop" || key == "paddingright" || key == "paddingbottom" || key == "paddingleft" ||
      key == "margin" || key == "margintop" || key == "marginright" || key == "marginbottom" || key == "marginleft") {
    return RoboFoundationSlot::Space;
  }
  if (key == "radius") {
    return RoboFoundationSlot::Radius;
  }
  if (key == "contentwidth") {
    return RoboFoundationSlot::ContentWidth;
  }
  if (key == "gutter") {
    return RoboFoundationSlot::Gutter;
  }
  if (key == "font" || key == "face" || key == "fontfamily") {
    return RoboFoundationSlot::Font;
  }
  return RoboFoundationSlot::None;
}

static bool RoboFoundationIsKeyword(const std::string& value) {
  const std::string key = RoboFoundationLower(value);
  return key == "transparent" || key == "none" || key == "auto" || key == "inherit" || key == "unset";
}

static bool RoboFoundationIsDirect(const std::string& value) {
  if (value.empty()) {
    return false;
  }
  const std::string trimmed = RoboFoundationTrim(value);
  if (trimmed[0] == '#' || RoboFoundationLower(trimmed).rfind("rgb", 0) == 0) {
    return true;
  }
  std::size_t i = 0;
  if (trimmed[i] == '-' || trimmed[i] == '+') {
    ++i;
  }
  bool digit = false;
  bool dot = false;
  for (; i < trimmed.size(); ++i) {
    const char ch = trimmed[i];
    if (ch >= '0' && ch <= '9') {
      digit = true;
      continue;
    }
    if (ch == '.' && !dot) {
      dot = true;
      continue;
    }
    break;
  }
  if (!digit) {
    return false;
  }
  const std::string rest = RoboFoundationLower(trimmed.substr(i));
  return rest.empty() || rest == "px" || rest == "em";
}

static std::string RoboFoundationStripUnit(const std::string& value) {
  std::string trimmed = RoboFoundationTrim(value);
  const std::string lower = RoboFoundationLower(trimmed);
  if (lower.size() > 2 && lower.substr(lower.size() - 2) == "px") {
    trimmed.resize(trimmed.size() - 2);
  } else if (lower.size() > 2 && lower.substr(lower.size() - 2) == "em") {
    trimmed.resize(trimmed.size() - 2);
  }
  return RoboFoundationTrim(trimmed);
}

static const char* RoboFoundationSlotName(RoboFoundationSlot slot) {
  switch (slot) {
    case RoboFoundationSlot::Color:
      return "color";
    case RoboFoundationSlot::Size:
    case RoboFoundationSlot::Weight:
    case RoboFoundationSlot::Tracking:
    case RoboFoundationSlot::Leading:
      return "typography";
    case RoboFoundationSlot::Space:
      return "spacing";
    case RoboFoundationSlot::Radius:
      return "radius";
    case RoboFoundationSlot::ContentWidth:
    case RoboFoundationSlot::Gutter:
      return "layout";
    case RoboFoundationSlot::Font:
      return "font";
    default:
      return "foundation";
  }
}

static std::string RoboFoundationConcrete(const RoboFoundationToken& token, RoboFoundationSlot slot, bool light) {
  if (token.kind == RoboFoundationKind::Color) {
    return RoboFoundationColorHex(light ? token.light : token.dark);
  }
  if (token.kind == RoboFoundationKind::Font) {
    return token.face.empty() ? token.name : token.face;
  }
  if (slot == RoboFoundationSlot::Size) {
    return token.size;
  }
  if (slot == RoboFoundationSlot::Weight) {
    return token.weight;
  }
  if (slot == RoboFoundationSlot::Tracking) {
    return token.tracking;
  }
  if (slot == RoboFoundationSlot::Leading) {
    return token.leading;
  }
  if (slot == RoboFoundationSlot::Gutter) {
    if (!token.gutter.empty()) {
      return token.gutter;
    }
    return token.value;
  }
  if (slot == RoboFoundationSlot::Space && !token.padding.empty() && token.value.empty()) {
    return token.padding;
  }
  if (slot == RoboFoundationSlot::ContentWidth) {
    if (!token.value.empty()) {
      return token.value;
    }
    return token.padding;
  }
  if (!token.value.empty()) {
    return token.value;
  }
  return token.size;
}

static std::string RoboFoundationComparable(RoboFoundationSlot slot, const std::string& concrete) {
  if (slot == RoboFoundationSlot::Color) {
    return RoboFoundationColorHex(concrete);
  }
  return RoboFoundationStripUnit(concrete);
}

static std::string RoboFoundationSuggest(const RoboFoundationSet& set, RoboFoundationSlot slot, const std::string& concrete,
                                         bool light) {
  const std::string want = RoboFoundationComparable(slot, concrete);
  for (const auto& item : set.tokens) {
    const std::string have = RoboFoundationComparable(slot, RoboFoundationConcrete(item.second, slot, light));
    if (!have.empty() && have == want) {
      return item.first;
    }
  }
  return {};
}

static void RoboFoundationBindNode(RoboXMLNode& node, const RoboFoundationLibrary& library, std::string foundation,
                                   std::string theme, std::vector<RoboFoundationNote>& notes) {
  const std::string nodeFoundation = RoboXMLAttr(node, "foundation");
  if (!nodeFoundation.empty()) {
    foundation = nodeFoundation;
  }
  const std::string nodeTheme = RoboFoundationLower(RoboXMLAttr(node, "theme", RoboXMLAttr(node, "appearance").c_str()));
  if (nodeTheme == "light" || nodeTheme == "dark") {
    theme = nodeTheme;
  }
  const bool light = theme == "light";
  const RoboFoundationSet* set = RoboFoundationFindConst(library, foundation);
  if (set == nullptr) {
    RoboFoundationNote note;
    note.isError = true;
    note.message = "foundation '" + foundation + "' was not found";
    notes.push_back(note);
    set = RoboFoundationFindConst(library, "RoboFoundation");
  }
  const std::string nodeKind = RoboFoundationLower(node.name);
  const bool typeNode = nodeKind == "text" || nodeKind == "heading" || nodeKind == "paragraph" || nodeKind == "p" ||
                        nodeKind == "span" || nodeKind == "h1" || nodeKind == "h2" || nodeKind == "h3" ||
                        nodeKind == "h4" || nodeKind == "h5" || nodeKind == "h6" || nodeKind == "subhead" ||
                        nodeKind == "body" || nodeKind == "content" || nodeKind == "caption" ||
                        nodeKind == "eyebrow" || nodeKind == "label";
  if (RoboFoundationLower(node.name) != "foundation" && set != nullptr) {
    for (auto& attr : node.attrs) {
      const RoboFoundationSlot slot = RoboFoundationSlotFor(RoboFoundationLower(attr.first));
      if (slot == RoboFoundationSlot::None || attr.second.empty() || RoboFoundationIsKeyword(attr.second)) {
        continue;
      }
      if (typeNode && slot == RoboFoundationSlot::Size) {
        const std::string sizeWord = RoboFoundationLower(attr.second);
        if (sizeWord == "xs" || sizeWord == "sm" || sizeWord == "small" || sizeWord == "md" || sizeWord == "medium" ||
            sizeWord == "lg" || sizeWord == "large" || sizeWord == "xl" || sizeWord == "2xl" || sizeWord == "3xl" ||
            sizeWord == "4xl" || sizeWord == "5xl" || sizeWord == "6xl" || sizeWord == "7xl" || sizeWord == "fluid") {
          continue;
        }
      }
      if (typeNode && (slot == RoboFoundationSlot::Font || slot == RoboFoundationSlot::Weight ||
                       slot == RoboFoundationSlot::Tracking || slot == RoboFoundationSlot::Leading)) {
        const std::string word = RoboFoundationLower(attr.second);
        if (word == "sans" || word == "serif" || word == "mono" || word == "monospace" || word == "system" ||
            word == "display" || word == "handwriting" || word == "cursive" || word == "decorative" ||
            word == "rounded" || word == "geometric" || word == "humanist" || word == "condensed" ||
            word == "expanded" || word == "thin" || word == "extralight" || word == "extra-light" || word == "light" ||
            word == "regular" || word == "medium" || word == "semibold" || word == "semi-bold" || word == "bold" ||
            word == "extrabold" || word == "extra-bold" || word == "black" || word == "normal" || word == "italic" ||
            word == "tighter" || word == "tight" || word == "wide" || word == "wider" || word == "widest" ||
            word == "compact" || word == "relaxed" || word == "loose") {
          continue;
        }
      }
      if (!typeNode && (slot == RoboFoundationSlot::Size || slot == RoboFoundationSlot::Weight ||
                        slot == RoboFoundationSlot::Tracking || slot == RoboFoundationSlot::Leading)) {
        continue;
      }
      const std::string lookup = RoboFoundationLower(attr.second);
      const auto token = set->tokens.find(lookup);
      if (token != set->tokens.end()) {
        const std::string concrete = RoboFoundationConcrete(token->second, slot, light);
        if (!concrete.empty()) {
          attr.second = concrete;
        }
        continue;
      }
      if (!RoboFoundationIsDirect(attr.second)) {
        RoboFoundationNote note;
        if (slot == RoboFoundationSlot::Font) {
          note.message = attr.first + " '" + attr.second +
                         "' is not a font token. Add <Font name=\"" + attr.second +
                         "\" src=\"file.ttf\" /> to the Foundation.";
        } else {
          note.message = attr.first + " '" + attr.second + "' is not a " + RoboFoundationSlotName(slot) +
                         " token in " + set->name;
        }
        notes.push_back(note);
        continue;
      }
      const std::string suggestion = RoboFoundationSuggest(*set, slot, attr.second, light);
      RoboFoundationNote note;
      note.message = attr.first + " '" + attr.second + "' is a direct " + RoboFoundationSlotName(slot) +
                     " value. Use a token from " + set->name;
      if (!suggestion.empty()) {
        note.message += " (" + suggestion + ")";
      }
      notes.push_back(note);
      if (slot == RoboFoundationSlot::Color) {
        attr.second = RoboFoundationColorHex(attr.second);
      } else {
        attr.second = RoboFoundationStripUnit(attr.second);
      }
    }
  }
  /* Graphic markup inside <svg> is packed art, not a design-token attribute. */
  if (RoboFoundationLower(node.name) == "svg") {
    return;
  }
  for (auto& child : node.children) {
    RoboFoundationBindNode(child, library, foundation, theme, notes);
  }
}

void RoboFoundationBind(RoboXMLNode& node, const RoboFoundationLibrary& library, const std::string& foundation,
                        const std::string& theme, std::vector<RoboFoundationNote>& notes) {
  RoboFoundationBindNode(node, library, foundation, theme, notes);
}
