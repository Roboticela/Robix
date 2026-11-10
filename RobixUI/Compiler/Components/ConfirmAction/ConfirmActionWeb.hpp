#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebEmitConfirmAction(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                              const std::filesystem::path& assetsRoot);
void RoboWebAppendConfirmActionCss(std::ostream& css);
std::string RoboWebConfirmActionScript();
const char* RoboWebConfirmActionClass();
