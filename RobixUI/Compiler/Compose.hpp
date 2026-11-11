#pragma once

#include "RoboXML.hpp"

#include <string>

/* Expand Import, Component, Data, Let, For, If, and {expressions} in a .rxu document.
   The document path is the folder used to resolve Import Source. */

void RoboUICompose(RoboXMLNode& root, const std::string& documentPath);

/* The painted tag. A preset keeps this in Alias after it expands to its base. */
std::string RoboUIFace(const RoboXMLNode& node);

/* When node is a preset such as EmailField, replace it with its compiler base.
   The painted control stays the base component. Returns false when node is already a base. */
bool RoboUIRewritePreset(RoboXMLNode& node);
