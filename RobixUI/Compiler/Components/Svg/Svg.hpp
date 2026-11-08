#pragma once

#include "../../RoboCompiler.hpp"
#include "../../RoboXML.hpp"

#include <string>

bool RoboCompilerHexDigit(char ch);
char RoboCompilerHexUpper(char ch);
std::string RoboCompilerBakeSvgColors(const std::string& svg);
void RoboCompilerAppendSvgAttr(std::string& out, const std::string& key, const std::string& value);
bool RoboCompilerSvgAttrIsComponent(const std::string& key);
void RoboCompilerSerializeSvgNode(const RoboXMLNode& node, std::string& out, bool root);
bool RoboCompilerCheckSvg(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
