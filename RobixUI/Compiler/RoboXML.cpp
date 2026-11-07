#include "RoboXML.hpp"

#include <cctype>

static void RoboXMLSkipWs(const std::string& s, size_t& i) {
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
    ++i;
  }
}

static std::string RoboXMLParseName(const std::string& s, size_t& i) {
  const size_t start = i;
  while (i < s.size()) {
    const char c = s[i];
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ':') {
      ++i;
      continue;
    }
    break;
  }
  return s.substr(start, i - start);
}

static std::string RoboXMLParseString(const std::string& s, size_t& i, RoboXMLDocument& doc) {
  if (i >= s.size() || (s[i] != '"' && s[i] != '\'')) {
    doc.errors.push_back("expected quoted attribute value");
    return {};
  }
  const char quote = s[i++];
  const size_t start = i;
  while (i < s.size() && s[i] != quote) {
    ++i;
  }
  if (i >= s.size()) {
    doc.errors.push_back("unterminated attribute string");
    return {};
  }
  std::string value = s.substr(start, i - start);
  ++i;
  return value;
}

static bool RoboXMLParseNode(const std::string& s, size_t& i, RoboXMLNode& node, RoboXMLDocument& doc);

static bool RoboXMLParseElement(const std::string& s, size_t& i, RoboXMLNode& node, RoboXMLDocument& doc) {
  RoboXMLSkipWs(s, i);
  if (i >= s.size() || s[i] != '<') {
    doc.errors.push_back("expected '<'");
    return false;
  }
  ++i;
  if (i < s.size() && s[i] == '/') {
    return false;
  }
  if (i < s.size() && s[i] == '?') {
    while (i + 1 < s.size() && !(s[i] == '?' && s[i + 1] == '>')) {
      ++i;
    }
    if (i + 1 < s.size()) {
      i += 2;
    }
    return RoboXMLParseElement(s, i, node, doc);
  }
  if (i < s.size() && s[i] == '!') {
    while (i < s.size() && s[i] != '>') {
      ++i;
    }
    if (i < s.size()) {
      ++i;
    }
    return RoboXMLParseElement(s, i, node, doc);
  }

  node.name = RoboXMLParseName(s, i);
  if (node.name.empty()) {
    doc.errors.push_back("missing element name");
    return false;
  }

  for (;;) {
    RoboXMLSkipWs(s, i);
    if (i >= s.size()) {
      doc.errors.push_back("unexpected end inside <" + node.name + ">");
      return false;
    }
    if (s[i] == '/') {
      ++i;
      RoboXMLSkipWs(s, i);
      if (i >= s.size() || s[i] != '>') {
        doc.errors.push_back("expected '/>'");
        return false;
      }
      ++i;
      return true;
    }
    if (s[i] == '>') {
      ++i;
      break;
    }
    const std::string key = RoboXMLParseName(s, i);
    RoboXMLSkipWs(s, i);
    if (key.empty() || i >= s.size() || s[i] != '=') {
      doc.errors.push_back("invalid attribute in <" + node.name + ">");
      return false;
    }
    ++i;
    RoboXMLSkipWs(s, i);
    node.attrs[key] = RoboXMLParseString(s, i, doc);
  }

  for (;;) {
    RoboXMLSkipWs(s, i);
    if (i + 1 < s.size() && s[i] == '<' && s[i + 1] == '/') {
      i += 2;
      const std::string endName = RoboXMLParseName(s, i);
      RoboXMLSkipWs(s, i);
      if (endName != node.name) {
        doc.errors.push_back("mismatched close tag </" + endName + "> for <" + node.name + ">");
        return false;
      }
      if (i >= s.size() || s[i] != '>') {
        doc.errors.push_back("expected '>' after </" + endName + ">");
        return false;
      }
      ++i;
      return true;
    }
    if (i < s.size() && s[i] == '<') {
      RoboXMLNode child;
      if (!RoboXMLParseNode(s, i, child, doc)) {
        return false;
      }
      node.children.push_back(std::move(child));
      continue;
    }
    const size_t start = i;
    while (i < s.size() && s[i] != '<') {
      ++i;
    }
    node.text += s.substr(start, i - start);
  }
}

static bool RoboXMLParseNode(const std::string& s, size_t& i, RoboXMLNode& node, RoboXMLDocument& doc) {
  return RoboXMLParseElement(s, i, node, doc);
}

bool RoboXMLParse(const std::string& source, RoboXMLDocument& out) {
  out = RoboXMLDocument{};
  size_t i = 0;
  if (!RoboXMLParseElement(source, i, out.root, out)) {
    if (out.errors.empty()) {
      out.errors.push_back("failed to parse XML root");
    }
    return false;
  }
  RoboXMLSkipWs(source, i);
  if (i < source.size()) {
    out.warnings.push_back("trailing content after root element");
  }
  return out.errors.empty();
}

std::string RoboXMLAttr(const RoboXMLNode& node, const char* key, const char* fallback) {
  const auto it = node.attrs.find(key);
  if (it == node.attrs.end()) {
    return fallback ? fallback : "";
  }
  return it->second;
}
