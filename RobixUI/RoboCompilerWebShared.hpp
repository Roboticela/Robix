#pragma once

#include "RoboFoundation.hpp"
#include "RoboXML.hpp"

#include <string>

std::string RoboWebLower(std::string text);
std::string RoboWebEscape(const std::string& text);
const RoboFoundationToken* RoboWebToken(const RoboFoundationLibrary& library, const std::string& name);
bool RoboWebIsNumber(const std::string& text);
std::string RoboWebCssValue(const RoboFoundationLibrary& library, const std::string& raw, bool color);
void RoboWebAddDecl(std::string& style, const char* property, const std::string& value);
