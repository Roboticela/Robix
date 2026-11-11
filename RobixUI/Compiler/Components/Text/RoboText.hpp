#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboFontPack.hpp"

#include <string>
#include <vector>

/* Stage C3. Text is the glyph run. Span is a colored part of one Text. */

struct RoboTextRun {
    std::string value;
    std::string weight;
    int size = 0;
    bool hasSize = false;
    RoboU32 color = 0xFFFFFFFFu;
};

struct RoboTextNode {
    std::string value;
    std::string family;
    std::string weight;
    std::string face;
    std::string wrap;
    std::string align;
    int size = 16;
    int width = 0;
    bool hasWidth = false;
    RoboU32 color = 0xFFFFFFFFu;
    int measuredWidth = 0;
    int measuredHeight = 0;
    std::vector<RoboTextRun> runs;
};

struct RoboTextFrame {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
};

/* Accepts the Text schema. An unknown attribute or a bad value writes a diagnostic and returns false. */
bool RoboTextRead(const RoboXMLNode& node, RoboTextNode& text, std::string& diagnostic);

/* Measures width and height from the cooked font pack. */
void RoboTextMeasure(RoboTextNode& text, const RoboFontPackView& pack);

/* Paints the glyphs into a frame. The ink pixel is the glyph coverage in the run color. */
void RoboTextPaint(const RoboTextNode& text, const RoboFontPackView& pack, RoboTextFrame& frame);

/* Returns false when x or y is outside the frame. */
bool RoboTextPixel(const RoboTextFrame& frame, int x, int y, RoboU32& color);
