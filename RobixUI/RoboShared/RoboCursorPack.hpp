#pragma once

#include "RoboTypes.hpp"

/*
 * Packed cursor theme blob — DRM ARGB8888 + hotspot (drm_mode_cursor2).
 * Extension: .robocur (ESP 8.3: .RCR). Cooked from Assets/Cursors Light and Dark SVG sources.
 * Layout is packed so host cook and UEFI runtime share identical offsets.
 */
#pragma pack(push, 1)

constexpr RoboU32 ROBO_CURSOR_PACK_MAGIC = 0x52554352u; /* 'RCUR' */
constexpr RoboU32 ROBO_CURSOR_PACK_VERSION = 2;
constexpr RoboU32 ROBO_CURSOR_THEME_COUNT = 2;
constexpr RoboU32 ROBO_CURSOR_KIND_COUNT = 11;

/* Dark=0 so zero-initialized handoff/state defaults to dark (OS default). */
enum class RoboCursorTheme : RoboU32 {
  Dark = 0,
  Light = 1
};

enum class RoboCursorKind : RoboU32 {
  Default = 0,
  Pointer = 1,
  Grab = 2,
  Link = 3,
  Text = 4,
  VerticalText = 5,
  ColResize = 6,
  RowResize = 7,
  AllScroll = 8,
  NwseResize = 9,
  NeswResize = 10
};

struct RoboCursorEntry {
  RoboU32 kind;
  RoboI32 hotX;
  RoboI32 hotY;
  RoboU32 width;
  RoboU32 height;
  RoboU32 stride;
  RoboU32 pixelOffset; /* from start of pack */
};

struct RoboCursorThemeTable {
  RoboU32 theme;
  RoboU32 entryCount;
  RoboU32 entryOffset; /* from start of pack, array of RoboCursorEntry */
};

struct RoboCursorPackHeader {
  RoboU32 magic;
  RoboU32 version;
  RoboU32 themeCount;
  RoboU32 kindCount;
  RoboU32 themeTableOffset;
  RoboU32 reserved0;
};

#pragma pack(pop)

struct RoboCursorImageView {
  const RoboU8* pixels;
  RoboU32 width;
  RoboU32 height;
  RoboU32 stride;
  RoboI32 hotX;
  RoboI32 hotY;
};

inline RoboBool RoboCursorPackValidate(const void* data, RoboSize size) {
  if (data == ROBO_NULL || size < sizeof(RoboCursorPackHeader)) {
    return ROBO_FALSE;
  }
  const auto* header = static_cast<const RoboCursorPackHeader*>(data);
  if (header->magic != ROBO_CURSOR_PACK_MAGIC) {
    return ROBO_FALSE;
  }
  if (header->version != ROBO_CURSOR_PACK_VERSION && header->version != 1) {
    return ROBO_FALSE;
  }
  if (header->themeCount != ROBO_CURSOR_THEME_COUNT || header->kindCount != ROBO_CURSOR_KIND_COUNT) {
    return ROBO_FALSE;
  }
  return ROBO_TRUE;
}

inline const RoboCursorEntry* RoboCursorPackFind(const void* data, RoboCursorTheme theme, RoboCursorKind kind) {
  if (!RoboCursorPackValidate(data, sizeof(RoboCursorPackHeader))) {
    return ROBO_NULL;
  }
  const auto* bytes = static_cast<const RoboU8*>(data);
  const auto* header = reinterpret_cast<const RoboCursorPackHeader*>(bytes);
  const auto* themes = reinterpret_cast<const RoboCursorThemeTable*>(bytes + header->themeTableOffset);
  for (RoboU32 t = 0; t < header->themeCount; ++t) {
    if (themes[t].theme != static_cast<RoboU32>(theme)) {
      continue;
    }
    const auto* entries = reinterpret_cast<const RoboCursorEntry*>(bytes + themes[t].entryOffset);
    for (RoboU32 e = 0; e < themes[t].entryCount; ++e) {
      if (entries[e].kind == static_cast<RoboU32>(kind)) {
        return &entries[e];
      }
    }
  }
  return ROBO_NULL;
}

inline RoboCursorImageView RoboCursorPackMakeView(const void* data, const RoboCursorEntry* entry) {
  RoboCursorImageView view{};
  if (data == ROBO_NULL || entry == ROBO_NULL) {
    return view;
  }
  view.pixels = static_cast<const RoboU8*>(data) + entry->pixelOffset;
  view.width = entry->width;
  view.height = entry->height;
  view.stride = entry->stride;
  view.hotX = entry->hotX;
  view.hotY = entry->hotY;
  return view;
}
