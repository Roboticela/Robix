#pragma once

#include <string>

/* Compiler components are implemented in C++.
   Parts belong inside one compiler component.
   Presets are .rxu definitions that reuse a compiler component.
   A new tag is a preset unless it needs behavior the compiler components do not have. */

enum class RoboUITier { Unknown, Compiler, Part, Preset };

RoboUITier RoboCompilerComponentTier(const std::string& name);
const char* RoboCompilerComponentBase(const std::string& name);
bool RoboCompilerPresetExpands(const std::string& name);
bool RoboCompilerAllowsComponentName(const std::string& name);
const char* RoboCompilerPresetLibrary();
