#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

void RoboWebAppendScrollCss(std::ostream& css);
std::string RoboWebScrollScript();
const char* RoboWebScrollViewClass();
