#pragma once

#include "../../RoboCompiler.hpp"
#include "../../RoboXML.hpp"

#include <string>

bool RoboCompilerIsContainerName(const std::string& name);
bool RoboCompilerIsContainerStyleKey(const std::string& key);
bool RoboCompilerCheckContainer(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
std::string RoboCompilerEmitContainerCpp(const RoboXMLNode& node, const std::string& parentExpr);
