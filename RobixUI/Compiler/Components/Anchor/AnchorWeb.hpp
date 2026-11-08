#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <filesystem>
#include <ostream>
#include <string>

void RoboWebEmitAnchor(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                       const std::filesystem::path& assetsRoot);
void RoboWebAppendAnchorCss(std::ostream& css);
