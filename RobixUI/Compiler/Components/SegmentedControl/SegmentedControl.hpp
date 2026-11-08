#pragma once

#include "../../RoboCompiler.hpp"
#include "../../RoboXML.hpp"

#include <string>

bool RoboCompilerCheckSegmentedControl(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
bool RoboCompilerCheckSegment(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
bool RoboCompilerCheckSegmentedToolbar(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
