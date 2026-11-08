#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebEmitSegmentedControl(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                                 const std::filesystem::path& assetsRoot);
void RoboWebEmitSegmentedToolbar(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                                 const std::string& source, const std::filesystem::path& assetsRoot, bool& warnedVideo,
                                 RoboWebPackage* package, int depth);
void RoboWebAppendSegmentedCss(std::ostream& css);
std::string RoboWebSegmentedScript();
const char* RoboWebSegmentedClass();
