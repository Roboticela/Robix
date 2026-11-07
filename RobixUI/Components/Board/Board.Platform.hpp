#pragma once

#include "../../RoboCompiler.hpp"
#include "../../RoboXML.hpp"

#include <string>

bool RoboCompilerIsBoardName(const std::string& name);
std::string RoboCompilerEmitBoardCpp(const RoboXMLNode& node, const std::string& parentExpr);
