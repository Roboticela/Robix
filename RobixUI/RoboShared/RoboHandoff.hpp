#pragma once

#include "RoboTypes.hpp"

constexpr RoboU32 ROBO_HANDOFF_MAGIC = 0x444E4852u; /* 'RHND' */
constexpr RoboU32 ROBO_HANDOFF_VERSION = 4;
constexpr RoboU32 ROBO_HANDOFF_MAX_RANGES = 48;

/* GOP / fbdev scanout description — pitch = pixelsPerScanLine * bytesPerPixel. */
struct RoboFrameBufferInfo {
  RoboU64 base;
  RoboU32 width;
  RoboU32 height;
  RoboU32 pixelsPerScanLine; /* fbdev fix.line_length / bpp */
  RoboU32 bytesPerPixel;     /* typically 4 (XRGB8888) */
  RoboU32 redMask;
  RoboU32 greenMask;
  RoboU32 blueMask;
  RoboU32 reserved0;
};

struct RoboMemoryRange {
  RoboU64 physicalStart;
  RoboU64 pageCount;
  RoboU32 type;
  RoboU32 reserved0;
};

constexpr RoboU32 ROBO_MEMORY_TYPE_FREE = 1;
constexpr RoboU32 ROBO_MEMORY_TYPE_BOOT = 2;
constexpr RoboU32 ROBO_MEMORY_TYPE_RUNTIME = 3;
constexpr RoboU32 ROBO_MEMORY_TYPE_ACPI = 4;

struct RoboHandoff {
  RoboU32 magic;
  RoboU32 version;
  RoboFrameBufferInfo frameBuffer;
  RoboU64 kernelPhysical;
  RoboU64 kernelSize;
  RoboU64 runtimePhysical;
  RoboU64 runtimeSize;
  RoboU64 logoPhysical;
  RoboU64 logoSize;
  RoboU64 fontPhysical;
  RoboU64 fontSize;
  RoboU64 cursorPackPhysical;
  RoboU64 cursorPackSize;
  RoboU64 desktopBufferPhysical;
  RoboU64 desktopBufferSize;
  RoboU64 backgroundVideoPhysical; /* cooked .robovid wallpaper */
  RoboU64 backgroundVideoSize;
  RoboU64 iconPackPhysical; /* app Icons.pack (ROIC) */
  RoboU64 iconPackSize;
  RoboU32 bootProgressPercent;
  RoboU32 inputFlags;
  RoboU32 cursorTheme; /* RoboCursorTheme */
  RoboU64 tscFrequency; /* calibrated ticks per second */
  RoboU64 rsdpPhysical;
  char bootName[32];
  RoboU32 memoryRangeCount;
  RoboU32 memoryRangeReserved;
  RoboMemoryRange memoryRanges[ROBO_HANDOFF_MAX_RANGES];
  /* Splash bar kept moving until the runtime presents the desktop. Milli-percent is 0..100000. */
  RoboU32 splashProgressMilli;
  RoboU32 splashCeilingMilli;
  RoboU32 splashOriginMilli;
  RoboU32 splashActive;
  RoboI32 splashBarX;
  RoboI32 splashBarY;
  RoboI32 splashBarW;
  RoboI32 splashBarH;
  RoboU64 splashEpochTsc;
  RoboU64 splashLastTsc;
};

constexpr RoboU32 ROBO_INPUT_FLAG_KEYBOARD = 1u << 0;
constexpr RoboU32 ROBO_INPUT_FLAG_MOUSE = 1u << 1;
constexpr RoboU32 ROBO_INPUT_FLAG_TOUCHPAD = 1u << 2;
constexpr RoboU32 ROBO_INPUT_FLAG_TOUCH = 1u << 3;
