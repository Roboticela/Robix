#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboIconPack.hpp"

#include <string>
#include <vector>

/* Stage C5. Icon is one glyph from the cooked icon pack. */

struct RoboIconNode {
    std::string name;
    int size = 24;
    RoboU32 color = 0xFFFFFFFFu;
};

struct RoboIconBox {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct RoboIconFrame {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
};

/* Accepts name, size, and color. An unknown attribute or a bad value writes a diagnostic and returns false. */
bool RoboIconRead(const RoboXMLNode& node, RoboIconNode& icon, std::string& diagnostic);

/* Reads one named glyph and reports the tight box of its coverage at the icon size. */
bool RoboIconGlyph(const RoboIconNode& icon, const RoboIconPackView& pack, RoboIconBox& box, std::string& diagnostic);

/* Paints the glyph coverage in the icon color. */
void RoboIconPaint(const RoboIconNode& icon, const RoboIconPackView& pack, RoboIconFrame& frame);

/* Returns false when x or y is outside the frame. */
bool RoboIconPixel(const RoboIconFrame& frame, int x, int y, RoboU32& color);
