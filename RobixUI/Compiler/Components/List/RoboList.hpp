#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboTypes.hpp"

#include <string>
#include <vector>

/* Stage C13. List repeats a template over a collection. Each child keeps the key it was built with. */

constexpr RoboU32 ROBO_LIST_PLATE = 0xFF10151Cu;
constexpr int ROBO_LIST_DEFAULT_WIDTH = 48;
constexpr int ROBO_LIST_DEFAULT_ITEM = 12;
constexpr int ROBO_LIST_MAX_ITEMS = 1000;

struct RoboListItem {
    std::string key;
    std::string value;
    RoboU32 color = 0;
};

struct RoboListChild {
    std::string key;
    std::string value;
    std::string kind;
    int identity = 0;
    bool recycled = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    RoboU32 color = 0;
};

struct RoboListNode {
    std::string keyField = "key";
    std::string templateKind = "view";
    bool numbered = false;
    int width = ROBO_LIST_DEFAULT_WIDTH;
    int height = 0;
    int itemHeight = ROBO_LIST_DEFAULT_ITEM;
    int gap = 0;
    int contentHeight = 0;
    int nextIdentity = 1;
    std::vector<RoboListChild> children;
    std::vector<int> freeIdentities;
};

struct RoboListFrame {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
};

/* Accepts items, key, and template. Items may be a comma list or Item children. */
bool RoboListRead(const RoboXMLNode& node, RoboListNode& list, std::string& diagnostic);

/* Rebuilds the children for a new collection. A key that remains keeps its identity.
   A new key reuses a freed identity when one exists. */
bool RoboListApply(RoboListNode& list, const std::vector<RoboListItem>& items, std::string& diagnostic);

/* The key of the child under the point. Returns false when the point misses every child. */
bool RoboListHit(const RoboListNode& list, int x, int y, std::string& key);

/* Paints one row per child and clips a row that falls outside the list box. */
void RoboListPaint(const RoboListNode& list, RoboListFrame& frame);

/* Returns false when x or y is outside the frame. */
bool RoboListPixel(const RoboListFrame& frame, int x, int y, RoboU32& color);

/* Replaces Item and Template children with one element per item. */
bool RoboListMaterialize(RoboXMLNode& node, std::string& diagnostic);
