#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Robix::UI {

struct RoboPlatformPaint {
    int width = 0;
    int height = 0;
    int content = 0;
    int contentWidth = 0;
    int columnX = 0;
    int columnW = 0;
    std::vector<std::uint32_t> column;
    std::vector<std::uint32_t> words;
    int motionX = 0;
    int motionY = 0;
    int motionW = 0;
    int motionH = 0;
    int motionFrames = 0;
    int motionDelay = 0;
    int probeX = 0;
    int probeY = 0;
    std::vector<std::uint32_t> motion;
};

/* Lay out and paint a document with the same platform path the Windows app uses. */
bool RoboPlatformCapture(const std::string& path, int width, int height, RoboPlatformPaint& paint, std::string& error);

/* Replay those commands into a tightly packed little-endian framebuffer. */
void RoboPlatformReplay(std::uint8_t* pixels, int width, int height, const RoboPlatformPaint& paint, int scroll);

/* Draw the current GIF frame. Returns 1 when a frame was stored. */
int RoboPlatformBlitMotion(std::uint8_t* pixels, int width, int height, const RoboPlatformPaint& paint, int scroll, int clock);

/* Blit the frame for clock and return the probe pixel. */
int RoboPlatformSample(std::uint8_t* pixels, int width, int height, const RoboPlatformPaint& paint, int clock);

int RoboPlatformSpot(const RoboPlatformPaint& paint, int kind);

}  // namespace Robix::UI
