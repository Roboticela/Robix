#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <filesystem>
#include <ostream>
#include <string>

void RoboWebEmitPasswordField(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                              const std::filesystem::path& assetsRoot);
void RoboWebAppendPasswordFieldCss(std::ostream& css);
const char* RoboWebPasswordFieldClass();
