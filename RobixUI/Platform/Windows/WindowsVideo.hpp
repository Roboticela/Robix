#pragma once

#include "../../RoboShared/RoboTypes.hpp"

#include <filesystem>
#include <vector>

namespace Robix::UI {

/*
 * The RGB frame that should be on screen at timeMs.
 * Decoding runs off the UI thread. destWidth and destHeight are the box the frame must fill;
 * the returned pixels are the whole picture scaled to that box. sampleMs is the frame's own clock.
 * An empty pixel buffer means the frame already on screen is still current.
 */
bool WindowsVideoFrame(const std::filesystem::path& file, int timeMs, std::vector<RoboU32>& pixels, int& width, int& height, int& sampleMs,
                       int destWidth = 0, int destHeight = 0, int durationMs = 0);

}
