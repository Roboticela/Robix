#pragma once

#include "RoboXML.hpp"

#include <string>

/* Motion attributes on any visual .rbxu element.
   Animate, Initial, and Exit take a preset or a property list such as "opacity:0 y:16".
   WhileHover, WhileTap, WhileFocus, and WhileInView take a preset or a property list.
   Transition takes duration, delay, ease, repeat, and direction.
   Drag is true, x, or y. */

void RoboWebMotionReset();
void RoboWebAppendMotion(std::string& style, const RoboXMLNode& node);
std::string RoboWebMotionAttrs(const RoboXMLNode& node);
std::string RoboWebMotionCss();
std::string RoboWebMotionScript();
