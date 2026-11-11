#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboTypes.hpp"

#include <string>
#include <vector>

/* Stage C4. Image is a decoded picture in a box. */

struct RoboImageRecord {
    std::string source;
    std::string alt;
    std::string fit;
    std::string onLoad;
    std::string onError;
    int width = 0;
    int height = 0;
    bool hasWidth = false;
    bool hasHeight = false;
    int sourceWidth = 0;
    int sourceHeight = 0;
    int frame = 0;
    int frameCount = 1;
    std::string format;
    std::string vector;
    std::vector<int> delays;
    std::vector<RoboU32> frames;
    std::vector<RoboU32> pixels;
    std::vector<std::string> fired;
};

struct RoboImageFrame {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
};

/* Accepts source, fit, and alt. An unknown attribute or a bad value writes a diagnostic and returns false. */
bool RoboImageRead(const RoboXMLNode& node, RoboImageRecord& image, std::string& diagnostic);

/* Decodes JPEG, PNG, GIF, SVG, WebP, TIFF, and HEIC, including GIF and WebP animation. OnLoad fires on success. OnError fires on failure. */
bool RoboImageDecode(RoboImageRecord& image, const void* bytes, std::size_t size);

/* Shows one animation frame. The first frame is already current after decode. */
bool RoboImageSelectFrame(RoboImageRecord& image, int index);

/* Delay of one animation frame, in milliseconds. A still picture is 0. */
int RoboImageFrameDelay(const RoboImageRecord& image, int index);

/* Paints the decoded picture into the frame using fit. SVG is drawn at the frame size. */
void RoboImagePaint(const RoboImageRecord& image, RoboImageFrame& frame);

/* Returns false when x or y is outside the frame. */
bool RoboImagePixel(const RoboImageFrame& frame, int x, int y, RoboU32& color);
