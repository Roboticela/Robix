#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboTypes.hpp"

#include <string>
#include <vector>

/* Stage C12. ScrollView moves its child inside a clipped viewport. The track and the thumb are parts. */

constexpr RoboU32 ROBO_SCROLL_PLATE = 0xFFFFFFFFu;
constexpr RoboU32 ROBO_SCROLL_CHILD = 0xFFFF0000u;
constexpr RoboU32 ROBO_SCROLL_MARKER = 0xFF00FF00u;
constexpr RoboU32 ROBO_SCROLL_TRACK = 0xFF2A3142u;
constexpr RoboU32 ROBO_SCROLL_THUMB = 0xFF8B93A7u;
constexpr int ROBO_SCROLL_BAR = 6;
constexpr int ROBO_SCROLL_MARKER_SIZE = 4;

struct RoboScrollViewNode {
    std::string axis = "vertical";
    int offsetX = 0;
    int offsetY = 0;
    int width = 0;
    int height = 0;
    int contentWidth = 0;
    int contentHeight = 0;
    RoboU32 childColor = ROBO_SCROLL_CHILD;
    int childOriginX = 0;
    int childOriginY = 0;
    int trackX = 0;
    int trackY = 0;
    int trackWidth = 0;
    int trackHeight = 0;
    int thumbX = 0;
    int thumbY = 0;
    int thumbWidth = 0;
    int thumbHeight = 0;
    bool verticalBar = false;
    bool horizontalBar = false;
};

struct RoboScrollViewFrame {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
};

/* Accepts axis, offset, and the viewport and content sizes. */
bool RoboScrollViewRead(const RoboXMLNode& node, RoboScrollViewNode& view, std::string& diagnostic);

/* Wheel deltas move the offset on the axes the view allows. A delta that cannot move returns false. */
bool RoboScrollViewWheel(RoboScrollViewNode& view, int deltaX, int deltaY);

/* A drag that starts on the thumb moves that thumb and updates the offset. */
bool RoboScrollViewDrag(RoboScrollViewNode& view, int fromX, int fromY, int toX, int toY);

/* Paints the child clipped to the viewport, then the track and the thumb. */
void RoboScrollViewPaint(const RoboScrollViewNode& view, RoboScrollViewFrame& frame);

/* Returns false when x or y is outside the frame. */
bool RoboScrollViewPixel(const RoboScrollViewFrame& frame, int x, int y, RoboU32& color);
