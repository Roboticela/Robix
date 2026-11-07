#pragma once

#include "RoboTypes.hpp"

/* Roboticela font atlas (PFF2-inspired). Extension: .robofont (ESP 8.3: .RFT) */
constexpr RoboU32 ROBO_FONT_MAGIC = 0x54464F52u; /* 'ROFT' little-endian */

struct RoboFontHeader {
  RoboU32 magic;
  RoboU16 ascent;
  RoboU16 descent;
  RoboU16 lineGap;
  RoboU16 glyphCount;
  RoboU16 firstCodepoint;
  RoboU16 lastCodepoint;
  RoboU16 unitsPerEm;
  RoboU16 reserved0;
};

struct RoboFontGlyph {
  RoboU16 codepoint;
  RoboI16 advance;
  RoboI16 bearingX;
  RoboI16 bearingY;
  RoboU16 width;
  RoboU16 height;
  RoboU32 bitmapOffset;
};

struct RoboFontView {
  const RoboFontHeader* header;
  const RoboFontGlyph* glyphs;
  const RoboU8* bitmaps;
};

inline RoboBool RoboFontValidate(const void* data, RoboSize size) {
  if (data == ROBO_NULL || size < sizeof(RoboFontHeader)) {
    return ROBO_FALSE;
  }
  const auto* header = static_cast<const RoboFontHeader*>(data);
  if (header->magic != ROBO_FONT_MAGIC || header->glyphCount == 0) {
    return ROBO_FALSE;
  }
  const RoboU64 tableBytes =
      static_cast<RoboU64>(sizeof(RoboFontHeader)) +
      static_cast<RoboU64>(header->glyphCount) * sizeof(RoboFontGlyph);
  return tableBytes <= size ? ROBO_TRUE : ROBO_FALSE;
}

inline RoboFontView RoboFontMakeView(const void* data) {
  RoboFontView view{};
  view.header = static_cast<const RoboFontHeader*>(data);
  view.glyphs = reinterpret_cast<const RoboFontGlyph*>(
      static_cast<const RoboU8*>(data) + sizeof(RoboFontHeader));
  view.bitmaps = static_cast<const RoboU8*>(data);
  return view;
}

/* Glyphs are cooked sorted by codepoint — binary search (GRUB font lookup style). */
inline const RoboFontGlyph* RoboFontFindGlyph(const RoboFontView& view, RoboU32 codepoint) {
  if (view.header == ROBO_NULL || view.glyphs == ROBO_NULL || codepoint > 0xFFFFu) {
    return ROBO_NULL;
  }
  const RoboU16 key = static_cast<RoboU16>(codepoint);
  RoboI32 lo = 0;
  RoboI32 hi = static_cast<RoboI32>(view.header->glyphCount) - 1;
  while (lo <= hi) {
    const RoboI32 mid = lo + ((hi - lo) / 2);
    const RoboU16 midCp = view.glyphs[mid].codepoint;
    if (midCp == key) {
      return &view.glyphs[mid];
    }
    if (midCp < key) {
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return ROBO_NULL;
}

inline RoboU16 RoboFontBaseSize(const RoboFontView& view) {
  if (view.header == ROBO_NULL) {
    return 72;
  }
  if (view.header->reserved0 != 0) {
    return view.header->reserved0;
  }
  const RoboU32 line = static_cast<RoboU32>(view.header->ascent) + static_cast<RoboU32>(view.header->descent);
  return line > 0 ? static_cast<RoboU16>(line) : static_cast<RoboU16>(96);
}
