#pragma once

#include "../../RoboCompiler.hpp"
#include "../../RoboXML.hpp"

#include <string>

bool RoboCompilerCheckSwitch(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
bool RoboCompilerCheckSwitchGroup(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
bool RoboCompilerCheckSwitchItem(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
std::string RoboCompilerEmitSwitchCpp(const RoboXMLNode& node, const std::string& parentExpr, bool item, bool divider);
std::string RoboCompilerEmitSwitchGroupCpp(const RoboXMLNode& node, const std::string& parentExpr);
