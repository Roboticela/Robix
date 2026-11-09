#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebAppendContainerCss(std::ostream& css);
const char* RoboWebContainerClass();
bool RoboWebIsContainerBox(const std::string& name);
void RoboWebContainerAttrs(std::string& html, const RoboXMLNode& node);
std::string RoboWebReelScript();
void RoboWebAppendContainerStyle(std::string& style, const RoboXMLNode& node, const RoboFoundationLibrary& library);
std::string RoboWebContainerVideo(const RoboXMLNode& node);
