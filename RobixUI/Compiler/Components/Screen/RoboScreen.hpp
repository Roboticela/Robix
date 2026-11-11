#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboTypes.hpp"

#include <string>
#include <vector>

/* Stage C1. Screen is the root. Window and Page are the same primitive. */

struct RoboScreenRecord {
    std::string tag;
    std::string title;
    std::string path;
    std::string theme;
    std::string place;
    std::string state;
    int x = 0;
    int y = 0;
    int width = 640;
    int height = 400;
    int minWidth = 320;
    int minHeight = 200;
    int maxWidth = 1920;
    int maxHeight = 1080;
    bool transparent = false;
    bool movable = true;
    bool resizable = true;
    RoboU32 color = 0;
};

struct RoboScreenFrame {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
};

const RoboXMLNode* RoboScreenFind(const RoboXMLNode& node);

/* Accepts the Screen schema. An unknown attribute or a bad value writes a diagnostic and returns false. */
bool RoboScreenRead(const RoboXMLNode& node, RoboScreenRecord& screen, std::string& diagnostic);

/* Fills the frame with the screen color. The origin pixel is that color. */
void RoboScreenPaint(const RoboScreenRecord& screen, RoboScreenFrame& frame);

/* Returns false when x or y is outside the frame. */
bool RoboScreenPixel(const RoboScreenFrame& frame, int x, int y, RoboU32& color);
