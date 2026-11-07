#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/* Minimal XML subset for UI screens (elements + attributes + nested children). */
struct RoboXMLNode {
  std::string name;
  std::unordered_map<std::string, std::string> attrs;
  std::vector<RoboXMLNode> children;
  std::string text;
};

struct RoboXMLDocument {
  RoboXMLNode root;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;
};

bool RoboXMLParse(const std::string& source, RoboXMLDocument& out);
std::string RoboXMLAttr(const RoboXMLNode& node, const char* key, const char* fallback = "");
