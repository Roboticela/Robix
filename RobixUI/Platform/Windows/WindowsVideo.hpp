#pragma once

#include "../../RoboShared/RoboTypes.hpp"

#include <filesystem>
#include <vector>

namespace Robix::UI {

/* The RGB frame that should be on screen at timeMs. Later calls on the same file read forward instead of seeking. sampleMs is the frame's own clock. */
bool WindowsVideoFrame(const std::filesystem::path& file, int timeMs, std::vector<RoboU32>& pixels, int& width, int& height, int& sampleMs);

}
