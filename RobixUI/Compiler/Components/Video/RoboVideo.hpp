#pragma once

#include "../../RoboXML.hpp"
#include "../../../RoboShared/RoboTypes.hpp"

#include <string>
#include <vector>

/* Stage C9. Video is a decoded frame and its clock. */

struct RoboVideoNode {
    std::string source;
    int time = 0;
    bool playing = false;
    int width = 0;
    int height = 0;
    bool hasWidth = false;
    bool hasHeight = false;
};

struct RoboVideoClip {
    std::string format;
    std::string codec;
    std::string projection;
    int width = 0;
    int height = 0;
    int duration = 0;
    int time = 0;
    bool playing = false;
    bool live = false;
    int frameCount = 0;
    int frameMs = 0;
    std::vector<RoboU32> frames;
};

struct RoboVideoBox {
    int width = 0;
    int height = 0;
    std::vector<RoboU32> pixels;
};

/* Accepts source, time, and playing. An unknown attribute or a bad value writes a diagnostic and returns false. */
bool RoboVideoRead(const RoboXMLNode& node, RoboVideoNode& video, std::string& diagnostic);

/* The format named by a file name or extension. Empty when the name is not a supported video format. */
const char* RoboVideoFormatOf(const std::string& source);

/* Opens every supported format. An uncompressed AVI fills frames. Other formats report the clock from the header. */
bool RoboVideoDecode(RoboVideoClip& clip, const void* bytes, std::size_t size, const std::string& source);

/* Moves the clock. A seek past the end stops on the last millisecond. */
bool RoboVideoSeek(RoboVideoClip& clip, int timeMs);

/* Copies the node's clock onto a decoded clip. */
void RoboVideoBind(const RoboVideoNode& video, RoboVideoClip& clip);

/* Paints the frame at the current time into the box. */
bool RoboVideoPaint(const RoboVideoClip& clip, RoboVideoBox& box);

/* Returns false when x or y is outside the box. */
bool RoboVideoPixel(const RoboVideoBox& box, int x, int y, RoboU32& color);
