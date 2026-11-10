#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <filesystem>
#include <string>

void RoboWebEmitEmailField(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                           const std::filesystem::path& assetsRoot);
const char* RoboWebEmailFieldClass();
