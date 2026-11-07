#pragma once

#include <string>
#include <unordered_map>
#include <vector>

enum class RoboFoundationKind { Color, Type, Space, Radius, Layout, Font };

struct RoboFoundationToken {
  std::string name;
  RoboFoundationKind kind = RoboFoundationKind::Color;
  std::string dark;
  std::string light;
  std::string size;
  std::string mobileSize;
  std::string weight;
  std::string tracking;
  std::string leading;
  std::string padding;
  std::string columns;
  std::string gutter;
  std::string value;
  std::string face;
};

struct RoboFoundationSet {
  std::string name;
  bool sealed = false;
  std::unordered_map<std::string, RoboFoundationToken> tokens;
};

struct RoboFoundationLibrary {
  std::vector<RoboFoundationSet> sets;
};
