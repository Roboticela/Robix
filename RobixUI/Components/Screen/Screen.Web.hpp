#pragma once

#include <ostream>
#include <string>

void RoboWebAppendScreenCss(std::ostream& css);
const char* RoboWebScreenClass(bool root);
const char* RoboWebScreenTag(bool root);
