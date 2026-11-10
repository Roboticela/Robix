#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebEmitTextField(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                          const std::filesystem::path& assetsRoot);
void RoboWebAppendTextFieldCss(std::ostream& css);
void RoboWebAppendFieldStyle(std::string& style, const RoboXMLNode& node, const RoboFoundationLibrary& library);
void RoboWebStripFieldChrome(RoboXMLNode& node);
bool RoboWebFieldWantsCaret(const RoboXMLNode& node);
const char* RoboWebFieldCaretScript();
const char* RoboWebTextFieldClass();
