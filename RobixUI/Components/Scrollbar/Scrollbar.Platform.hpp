#pragma once

#include "../../Platform/Framebuffer.hpp"

#include <string>

namespace Robix::UI {

struct ScrollbarPaint {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int thickness = 6;
    bool vertical = true;
    std::string thumbColor;
    std::string trackColor;
};

void RoboScrollbarPlatformPaint(const ScrollbarPaint& bar, Robo::FrameBuffer& buffer, bool light);

}  // namespace Robix::UI
