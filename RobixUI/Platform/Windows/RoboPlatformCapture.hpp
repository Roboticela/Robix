#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Robix::UI {

struct RoboPlatformPaint {
    int width = 0;
    int height = 0;
    int content = 0;
    std::vector<std::uint32_t> words;
};

/* Lay out and paint a document with the same platform path the Windows app uses. */
bool RoboPlatformCapture(const std::string& path, int width, int height, RoboPlatformPaint& paint, std::string& error);

/* Replay those commands into a tightly packed little-endian framebuffer. */
void RoboPlatformReplay(std::uint8_t* pixels, int width, int height, const std::vector<std::uint32_t>& words, int scroll);

}  // namespace Robix::UI
