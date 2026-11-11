#pragma once

#include "../../RoboCompilerWebShared.hpp"

#include <ostream>
#include <string>

/* Emits a draw-list Canvas. Returns false when the node is a layout box that happens to be named Canvas. */
bool RoboWebEmitCanvas(std::string& html, const RoboXMLNode& node);
void RoboWebAppendCanvasCss(std::ostream& css);
