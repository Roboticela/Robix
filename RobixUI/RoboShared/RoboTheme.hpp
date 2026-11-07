#pragma once

#include "RoboCursorPack.hpp"
#include "RoboTypes.hpp"

/* OS + Compiler apps share this default appearance. Dark=0 matches zero-init. */
enum class RoboUiTheme : RoboU32 {
  Dark = 0,
  Light = 1
};

constexpr RoboUiTheme ROBO_DEFAULT_UI_THEME = RoboUiTheme::Dark;
constexpr RoboCursorTheme ROBO_DEFAULT_CURSOR_THEME = RoboCursorTheme::Dark;

/*
 * Cooked cursors are already the macOS pointer size. Blit them 1:1.
 */
constexpr RoboI32 ROBO_CURSOR_DISPLAY_SIZE = 0;
