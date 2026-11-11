#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboFontPack.hpp"

#include <string>
#include <vector>

/* Stage C7. Canvas is a draw list inside one box. */

enum class RoboCanvasKind {
    Rect,
    Disc,
    Line,
    Text,
};

struct RoboCanvasCommand {
    RoboCanvasKind kind = RoboCanvasKind::Rect;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int radius = 0;
    int x2 = 0;
    int y2 = 0;
    int size = 16;
    std::string value;
    std::string family;
    RoboU32 color = 0xFF000000u;
};

struct RoboCanvasNode {
    int width = 0;
    int height = 0;
    std::vector<RoboCanvasCommand> commands;
};

struct RoboCanvasFrame {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
};

/* Accepts width, height, and a draw list. An unknown attribute or a bad value writes a diagnostic and returns false. */
bool RoboCanvasRead(const RoboXMLNode& node, RoboCanvasNode& canvas, std::string& diagnostic);

/* Draws each rect, disc, line, and text command into the box. Text uses the cooked font pack. */
void RoboCanvasPaint(const RoboCanvasNode& canvas, const RoboFontPackView& pack, RoboCanvasFrame& frame);

/* Returns false when x or y is outside the frame. */
bool RoboCanvasPixel(const RoboCanvasFrame& frame, int x, int y, RoboU32& color);
