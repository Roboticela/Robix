#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <filesystem>
#include <ostream>
#include <string>

void RoboWebEmitNameField(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                          const std::filesystem::path& assetsRoot);
void RoboWebAppendNameFieldCss(std::ostream& css);
const char* RoboWebNameFieldClass();
