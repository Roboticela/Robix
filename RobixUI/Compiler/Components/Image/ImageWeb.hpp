#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <filesystem>
#include <ostream>
#include <string>

void RoboWebImageReset(const std::filesystem::path& document, const std::filesystem::path& outRoot, bool compile);
void RoboWebEmitImage(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library);
void RoboWebAppendImageStyle(std::string& style, const RoboXMLNode& node, const RoboFoundationLibrary& library);
void RoboWebAppendImageCss(std::ostream& css);
std::string RoboWebImageScript();
const char* RoboWebImageClass();
