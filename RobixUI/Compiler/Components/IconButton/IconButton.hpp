#pragma once

#include "../../RoboCompiler.hpp"
#include "../../RoboXML.hpp"

#include <string>

bool RoboCompilerCheckIconButton(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
std::string RoboCompilerEmitIconButtonCpp(const RoboXMLNode& node, const std::string& parentExpr);
