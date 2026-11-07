#pragma once

#include "../../RoboCompiler.hpp"
#include "../../RoboXML.hpp"

#include <string>

bool RoboCompilerIsScrollbarName(const std::string& name);
std::string RoboCompilerEmitScrollbarCpp(const RoboXMLNode& node, const std::string& parentExpr);
