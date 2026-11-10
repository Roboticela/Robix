#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebEmitTextField(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                          const std::filesystem::path& assetsRoot);
void RoboWebAppendTextFieldCss(std::ostream& css);
const char* RoboWebTextFieldClass();
