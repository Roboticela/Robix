#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Robix {

/* Perspective frame of one .rx3d scene: one camera, one directional light, flat box meshes. */
bool RoboRasterizeScene(const std::string& path, const std::string& camera, int width, int height,
                        std::vector<std::uint32_t>& pixels);

}  // namespace Robix
