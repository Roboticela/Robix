#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebEmitSwitch(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                       const std::filesystem::path& assetsRoot);
void RoboWebEmitSwitchGroup(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                            const std::string& source, const std::filesystem::path& assetsRoot, bool& warnedVideo,
                            RoboWebPackage* package, int depth);
void RoboWebEmitSwitchItem(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                           const std::filesystem::path& assetsRoot);
void RoboWebAppendSwitchCss(std::ostream& css);
std::string RoboWebSwitchScript();
const char* RoboWebSwitchClass();
