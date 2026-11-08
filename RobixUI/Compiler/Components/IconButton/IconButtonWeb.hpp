#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebEmitIconButton(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                           const std::filesystem::path& assetsRoot);
void RoboWebAppendIconButtonCss(std::ostream& css);
std::string RoboWebIconButtonScript();
const char* RoboWebIconButtonClass();
