#pragma once

#include "RoboXML.hpp"

#include <string>
#include <unordered_map>
#include <vector>

/* RoboFoundation — colors, typography, spacing, radius, and layout.
 * The sealed set matches RefCode/Components (globals.css + lib/tokens.ts +
 * the layout table in FoundationDocs). Apps inherit it. They do not replace it.
 */

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
  std::string face; /* pack name for a Font token */
};

struct RoboFoundationSet {
  std::string name;
  bool sealed = false;
  std::unordered_map<std::string, RoboFoundationToken> tokens;
};

struct RoboFoundationLibrary {
  std::vector<RoboFoundationSet> sets;
};

struct RoboFoundationNote {
  bool isError = false;
  std::string message;
};

bool RoboFoundationLoadFile(const std::string& path, RoboFoundationLibrary& library, bool sealed, std::string& error);
bool RoboFoundationLoadNode(const RoboXMLNode& node, RoboFoundationLibrary& library, std::string& error);
void RoboFoundationBind(RoboXMLNode& node, const RoboFoundationLibrary& library, const std::string& foundation,
                        const std::string& theme, std::vector<RoboFoundationNote>& notes);
