#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebThemePrepare(const std::string& theme);
void RoboWebEmitThemeToggle(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                            const std::filesystem::path& assetsRoot);
void RoboWebAppendThemeToggleCss(std::ostream& css);
std::string RoboWebThemeScript();
const char* RoboWebThemeToggleClass();
