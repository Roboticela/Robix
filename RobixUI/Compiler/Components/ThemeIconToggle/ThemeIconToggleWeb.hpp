#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebEmitThemeIconToggle(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                                const std::filesystem::path& assetsRoot);
void RoboWebAppendThemeIconToggleCss(std::ostream& css);
const char* RoboWebThemeIconToggleClass();
