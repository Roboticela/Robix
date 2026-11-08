#pragma once

#include "../../RoboGPU/RoboGPU.hpp"

#include <string>

namespace Robix::UI {

void PaintWindowsSvg(RoboGPU* gpu, int x, int y, int size, const std::string& svg, RoboU32 color);

}  // namespace Robix::UI
