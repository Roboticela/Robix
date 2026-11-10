#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebEmitSelect(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                       const std::filesystem::path& assetsRoot);
void RoboWebAppendSelectCss(std::ostream& css);
const char* RoboWebSelectClass();
const char* RoboWebSelectScript();
