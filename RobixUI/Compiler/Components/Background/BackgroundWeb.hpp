#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebEmitBackground(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                           const std::string& source, const std::filesystem::path& assetsRoot, RoboWebPackage* package, int depth);
void RoboWebAppendBackgroundCss(std::ostream& css);
std::string RoboWebSlideshowScript();
const char* RoboWebBackgroundClass();
