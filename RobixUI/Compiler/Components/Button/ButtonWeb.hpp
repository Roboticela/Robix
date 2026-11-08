#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebEmitButton(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                       const std::filesystem::path& assetsRoot);
void RoboWebAppendButtonCss(std::ostream& css);
void RoboWebAppendControlCss(std::ostream& css);
void RoboWebControlAttrs(std::string& html, const RoboXMLNode& node);
void RoboWebAppendControlStyle(std::string& style, const RoboXMLNode& node, const RoboFoundationLibrary& library);
const char* RoboWebButtonClass();
