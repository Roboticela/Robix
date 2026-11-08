#pragma once

#include "RoboFoundation.hpp"
#include "RoboXML.hpp"

#include <string>

/* Screen sizes for a .rbxu document.
   sm, mx, xl, and 2x mean that width and wider.
   sm-mx, mx-xl, and xl-2x are only the band between those two.
   900 and at-900 mean that custom width and wider.
   900-1200 is only the custom band.
   Join sizes with a dot in an attribute (Width.sm.xl) or with a comma in Only and At Size.
   A Screen Sizes value replaces the default widths. */

void RoboWebScreenReset(const std::string& sizes);
std::string RoboWebScreenAttrs(const RoboXMLNode& node, const RoboFoundationLibrary& library, const char* display);
std::string RoboWebScreenCss();
