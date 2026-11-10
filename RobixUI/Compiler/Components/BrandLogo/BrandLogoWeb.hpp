#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebEmitBrandLogo(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library);
void RoboWebAppendBrandLogoCss(std::ostream& css);
const char* RoboWebBrandLogoClass();
