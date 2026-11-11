#pragma once

#include "../../RoboCompiler.hpp"
#include "../../RoboXML.hpp"

#include <string>

bool RoboCompilerCheckIcon(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
std::string RoboCompilerEmitIconCPP(const RoboXMLNode& node, const std::string& parentExpr);
