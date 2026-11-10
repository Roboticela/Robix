#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <filesystem>
#include <string>

void RoboWebEmitCountrySelect(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                              const std::filesystem::path& assetsRoot);
std::string RoboWebCountrySelectScript(const std::filesystem::path& assetsRoot);
