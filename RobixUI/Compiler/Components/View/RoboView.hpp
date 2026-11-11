#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboTypes.hpp"

#include <string>
#include <vector>

/* Stage C2. View is the box. Container and Box are the same primitive. */

struct RoboViewEdge {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;
};

struct RoboViewNode {
    std::string tag;
    std::string id;
    std::string display;
    std::string direction;
    std::string justify;
    std::string align;
    std::string alignSelf;
    std::string sizeMode;
    std::string position;
    bool absolute = false;
    bool clipX = false;
    bool clipY = false;
    bool hasWidth = false;
    bool hasHeight = false;
    bool widthPercent = false;
    bool heightPercent = false;
    bool hasMinWidth = false;
    bool hasMinHeight = false;
    bool hasMaxWidth = false;
    bool hasMaxHeight = false;
    bool hasAspect = false;
    bool hasColor = false;
    bool fill = false;
    int width = 0;
    int height = 0;
    int minWidth = 0;
    int minHeight = 0;
    int maxWidth = 0;
    int maxHeight = 0;
    int aspectNum = 1;
    int aspectDen = 1;
    int grow = 0;
    int shrink = 0;
    int order = 0;
    int columns = 1;
    int rows = 0;
    int colSpan = 1;
    int rowSpan = 1;
    int colStart = -1;
    int rowStart = -1;
    int placedCol = 0;
    int placedRow = 0;
    int x = 0;
    int y = 0;
    bool hasLeft = false;
    bool hasTop = false;
    bool hasRight = false;
    bool hasBottom = false;
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    int gap = 0;
    int rowGap = 0;
    int columnGap = 0;
    RoboViewEdge padding;
    RoboViewEdge margin;
    RoboU32 color = 0;
    int usedW = 0;
    int usedH = 0;
    int boxX = 0;
    int boxY = 0;
    std::vector<RoboViewNode> children;
};

struct RoboViewFrame {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
};

bool RoboViewRead(const RoboXMLNode& node, RoboViewNode& view, std::string& diagnostic);
void RoboViewLayout(RoboViewNode& view);
void RoboViewPaint(const RoboViewNode& view, RoboViewFrame& frame);
bool RoboViewPixel(const RoboViewFrame& frame, int x, int y, RoboU32& color);
const RoboViewNode* RoboViewHit(const RoboViewNode& view, int x, int y);
const RoboViewNode* RoboViewFind(const RoboViewNode& view, const std::string& id);
