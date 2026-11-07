#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

bool RoboWebIsTextName(const std::string& name);
std::string RoboWebTypeClass(const RoboXMLNode& node);
void RoboWebAppendTextCss(std::ostream& css);
void RoboWebTextAttrs(std::string& html, const RoboXMLNode& node);
void RoboWebAppendTextStyle(std::string& style, const RoboXMLNode& node, const RoboFoundationLibrary& library);
const char* RoboWebTextTag(const std::string& name);
std::string RoboWebTextClass(const std::string& name);
