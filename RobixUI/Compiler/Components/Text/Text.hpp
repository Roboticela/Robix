#pragma once

#include "../../RoboCompiler.hpp"
#include "../../RoboXML.hpp"

#include <string>

#include "RoboShared/RoboTypes.hpp"

enum class RoboCompilerTextKind { Text, Heading, Subhead, Body, Caption, Eyebrow, Label };

bool RoboCompilerIsAlign(const std::string& align);
bool RoboCompilerIsTextStyleKey(const std::string& key);
bool RoboCompilerIsTextName(const std::string& name);
bool RoboCompilerIsWrap(const std::string& wrap);
RoboCompilerTextKind RoboCompilerClassifyTextNode(const RoboXMLNode& node, RoboU32* outHeadingLevel);
const char* RoboCompilerTextFont(RoboCompilerTextKind kind, RoboU32 level);
bool RoboCompilerCheckText(const RoboXMLNode& node, const std::string& path, RoboCompilerResult& result);
bool RoboCompilerIsTextLike(const RoboXMLNode& node);
std::string RoboCompilerEmitTextCpp(const RoboXMLNode& node, const std::string& parentExpr);
