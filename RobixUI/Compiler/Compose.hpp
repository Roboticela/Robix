#pragma once

#include "RoboXML.hpp"

#include <string>

/* Expand Import, Component, Data, Let, For, If, and {expressions} in a .rbxu document.
   The document path is the folder used to resolve Import Source. */

void RoboUICompose(RoboXMLNode& root, const std::string& documentPath);
