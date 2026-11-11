#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboTypes.hpp"

#include <string>
#include <vector>

/* Stage C6. Svg is vector paths rasterized into the element box. */

struct RoboSvgPath {
    std::string data;
    std::string fill;
    std::string stroke;
    float strokeWidth = 0.0f;
    bool hasFill = false;
    bool hasStroke = false;
    bool hasStrokeWidth = false;
};

struct RoboSvgNode {
    std::string fill;
    std::string stroke;
    float strokeWidth = 1.0f;
    bool hasStrokeWidth = false;
    float viewX = 0.0f;
    float viewY = 0.0f;
    float viewWidth = 24.0f;
    float viewHeight = 24.0f;
    int width = 24;
    int height = 24;
    std::vector<RoboSvgPath> paths;
};

struct RoboSvgFrame {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
};

/* Accepts paths, fill, stroke, and view box. An unknown attribute or a bad value writes a diagnostic and returns false. */
bool RoboSvgRead(const RoboXMLNode& node, RoboSvgNode& svg, std::string& diagnostic);

/* Rasterizes the paths into the element box. */
void RoboSvgPaint(const RoboSvgNode& svg, RoboSvgFrame& frame);

/* Returns false when x or y is outside the frame. */
bool RoboSvgPixel(const RoboSvgFrame& frame, int x, int y, RoboU32& color);
