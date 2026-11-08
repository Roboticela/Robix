#pragma once

#include "../../RoboCompiler.hpp"
#include "../../RoboXML.hpp"

#include <string>

bool RoboCompilerCheckButton(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
bool RoboCompilerIsButtonStyleKey(const std::string& key);
bool RoboCompilerIsButtonPreset(const std::string& name);
std::string RoboCompilerEmitButtonCpp(const RoboXMLNode& node, const std::string& parentExpr);
