#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboTypes.hpp"

#include <string>
#include <vector>

/* Stage C8. View3D hosts one .rx3d scene. Scene3D is the same primitive. */

struct RoboView3DNode {
    std::string source;
    std::string camera;
    int width = 0;
    int height = 0;
};

struct RoboView3DFrame {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
};

/* Accepts source, camera, width, and height. Scene3D is read as View3D. */
bool RoboView3DRead(const RoboXMLNode& node, RoboView3DNode& view, std::string& diagnostic);

/* Paints one frame of the scene into the box. The clear pixels keep the scene background. */
bool RoboView3DPaint(const RoboView3DNode& view, RoboView3DFrame& frame);

/* Returns false when x or y is outside the frame. */
bool RoboView3DPixel(const RoboView3DFrame& frame, int x, int y, RoboU32& color);
