#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <filesystem>
#include <ostream>
#include <string>

void RoboWebEmitPasswordReveal(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                               const std::filesystem::path& assetsRoot, bool fieldDisabled, bool placedInField);
void RoboWebAppendPasswordRevealCss(std::ostream& css);
std::string RoboWebPasswordRevealScript();
const char* RoboWebPasswordRevealClass();
