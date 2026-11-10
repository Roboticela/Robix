#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebEmitTextArea(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                         const std::filesystem::path& assetsRoot);
void RoboWebAppendTextAreaCss(std::ostream& css);
const char* RoboWebTextAreaClass();
const char* RoboWebTextAreaScript();
