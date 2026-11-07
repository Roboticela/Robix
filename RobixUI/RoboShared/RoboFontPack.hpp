#pragma once

#include "RoboFont.hpp"

/*
 * Multi-font pack — every TTF/OTF under Assets/Fonts cooked at build into one blob.
 * Extension: .robofonts (ESP 8.3: FONTS.RPK or INTER.RFT pack payload).
 * Default face: Inter-Regular (Assets/Fonts/Inter/Inter-Regular.ttf).
 */
#pragma pack(push, 1)

constexpr RoboU32 ROBO_FONT_PACK_MAGIC = 0x544E4652u; /* 'RFNT' */
constexpr RoboU32 ROBO_FONT_PACK_VERSION = 1;
constexpr RoboU32 ROBO_FONT_PACK_NAME_MAX = 48;
constexpr RoboU32 ROBO_FONT_PACK_MAX_FONTS = 4096;
/* Native strikes. Draw picks the exact size so the type scale is not scaled. */
constexpr RoboU16 ROBO_FONT_STRIKES[] = {12, 13, 17, 19, 22, 24, 28, 40, 56, 80};
constexpr RoboU32 ROBO_FONT_STRIKE_COUNT = sizeof(ROBO_FONT_STRIKES) / sizeof(ROBO_FONT_STRIKES[0]);
constexpr RoboU16 ROBO_FONT_MASTER_PX = 80;

struct RoboFontPackEntry {
  char name[ROBO_FONT_PACK_NAME_MAX]; /* e.g. "Inter-Regular", "MyFont" */
  RoboU32 offset;                     /* from start of pack → RoboFontHeader */
  RoboU32 size;
  RoboU32 reserved0;
};

struct RoboFontPackHeader {
  RoboU32 magic;
  RoboU32 version;
  RoboU32 fontCount;
  RoboU32 defaultIndex; /* Inter-Regular when present */
  RoboU32 entryOffset;  /* array of RoboFontPackEntry */
  RoboU32 reserved0;
};

#pragma pack(pop)

struct RoboFontPackView {
  const RoboFontPackHeader* header;
  const RoboFontPackEntry* entries;
  const RoboU8* base;
  RoboSize size;
};

inline RoboBool RoboFontPackValidate(const void* data, RoboSize size) {
  if (data == ROBO_NULL || size < sizeof(RoboFontPackHeader)) {
    return ROBO_FALSE;
  }
  const auto* header = static_cast<const RoboFontPackHeader*>(data);
  if (header->magic != ROBO_FONT_PACK_MAGIC || header->version != ROBO_FONT_PACK_VERSION) {
    return ROBO_FALSE;
  }
  if (header->fontCount == 0 || header->fontCount > ROBO_FONT_PACK_MAX_FONTS) {
    return ROBO_FALSE;
  }
  if (header->defaultIndex >= header->fontCount) {
    return ROBO_FALSE;
  }
  const RoboU64 entriesEnd =
      static_cast<RoboU64>(header->entryOffset) +
      static_cast<RoboU64>(header->fontCount) * sizeof(RoboFontPackEntry);
  return entriesEnd <= size ? ROBO_TRUE : ROBO_FALSE;
}

inline RoboFontPackView RoboFontPackMakeView(const void* data, RoboSize size) {
  RoboFontPackView view{};
  view.header = static_cast<const RoboFontPackHeader*>(data);
  view.entries = reinterpret_cast<const RoboFontPackEntry*>(static_cast<const RoboU8*>(data) +
                                                            view.header->entryOffset);
  view.base = static_cast<const RoboU8*>(data);
  view.size = size;
  return view;
}

inline RoboBool RoboFontPackNameEq(const char* a, const char* b) {
  if (a == ROBO_NULL || b == ROBO_NULL) {
    return ROBO_FALSE;
  }
  while (*a != 0 && *b != 0) {
    char ca = *a;
    char cb = *b;
    if (ca >= 'A' && ca <= 'Z') {
      ca = static_cast<char>(ca - 'A' + 'a');
    }
    if (cb >= 'A' && cb <= 'Z') {
      cb = static_cast<char>(cb - 'A' + 'a');
    }
    if (ca != cb) {
      return ROBO_FALSE;
    }
    ++a;
    ++b;
  }
  return (*a == 0 && *b == 0) ? ROBO_TRUE : ROBO_FALSE;
}

inline RoboFontView RoboFontPackFontAt(const RoboFontPackView& pack, RoboU32 index) {
  RoboFontView empty{};
  if (pack.header == ROBO_NULL || pack.entries == ROBO_NULL || index >= pack.header->fontCount) {
    return empty;
  }
  const RoboFontPackEntry& entry = pack.entries[index];
  if (entry.offset == 0 || static_cast<RoboU64>(entry.offset) + entry.size > pack.size) {
    return empty;
  }
  const void* fontData = pack.base + entry.offset;
  if (!RoboFontValidate(fontData, entry.size)) {
    return empty;
  }
  return RoboFontMakeView(fontData);
}

inline RoboFontView RoboFontPackFind(const RoboFontPackView& pack, const char* name);

inline RoboFontView RoboFontPackDefault(const RoboFontPackView& pack) {
  if (pack.header == ROBO_NULL) {
    return RoboFontView{};
  }
  return RoboFontPackFontAt(pack, pack.header->defaultIndex);
}

inline void RoboFontPackStrikeName(char* out, RoboU32 cap, const char* face, RoboU16 sizePx) {
  RoboU32 i = 0;
  if (face != ROBO_NULL) {
    while (face[i] != 0 && i + 1 < cap) {
      out[i] = face[i];
      ++i;
    }
  }
  if (i + 1 < cap) {
    out[i++] = '@';
  }
  char digits[8];
  RoboU32 count = 0;
  RoboU16 value = sizePx;
  if (value == 0) {
    digits[count++] = '0';
  }
  while (value > 0 && count < 8) {
    digits[count++] = static_cast<char>('0' + (value % 10));
    value = static_cast<RoboU16>(value / 10);
  }
  while (count > 0 && i + 1 < cap) {
    out[i++] = digits[--count];
  }
  if (cap > 0) {
    out[i < cap ? i : cap - 1] = 0;
  }
}

inline RoboBool RoboFontPackParseStrike(const char* entry, const char* face, RoboU16* outSize) {
  if (entry == ROBO_NULL || face == ROBO_NULL || face[0] == 0) {
    return ROBO_FALSE;
  }
  const char* a = entry;
  const char* b = face;
  while (*b != 0) {
    char ca = *a;
    char cb = *b;
    if (ca >= 'A' && ca <= 'Z') {
      ca = static_cast<char>(ca - 'A' + 'a');
    }
    if (cb >= 'A' && cb <= 'Z') {
      cb = static_cast<char>(cb - 'A' + 'a');
    }
    if (ca == 0 || ca != cb) {
      return ROBO_FALSE;
    }
    ++a;
    ++b;
  }
  if (*a != '@') {
    return ROBO_FALSE;
  }
  ++a;
  if (*a < '0' || *a > '9') {
    return ROBO_FALSE;
  }
  RoboU32 size = 0;
  while (*a >= '0' && *a <= '9') {
    size = size * 10u + static_cast<RoboU32>(*a - '0');
    if (size > 65535u) {
      return ROBO_FALSE;
    }
    ++a;
  }
  if (*a != 0) {
    return ROBO_FALSE;
  }
  if (outSize != ROBO_NULL) {
    *outSize = static_cast<RoboU16>(size);
  }
  return ROBO_TRUE;
}

inline RoboFontView RoboFontPackFindSize(const RoboFontPackView& pack, const char* name, RoboU16 sizePx) {
  if (pack.header == ROBO_NULL || name == ROBO_NULL || name[0] == 0) {
    return RoboFontPackDefault(pack);
  }
  char exact[ROBO_FONT_PACK_NAME_MAX];
  RoboFontPackStrikeName(exact, ROBO_FONT_PACK_NAME_MAX, name, sizePx);
  for (RoboU32 i = 0; i < pack.header->fontCount; ++i) {
    if (RoboFontPackNameEq(pack.entries[i].name, exact)) {
      return RoboFontPackFontAt(pack, i);
    }
  }
  RoboI32 best = -1;
  RoboI32 bestDist = 0;
  RoboBool bestAbove = ROBO_FALSE;
  for (RoboU32 i = 0; i < pack.header->fontCount; ++i) {
    RoboU16 strike = 0;
    if (!RoboFontPackParseStrike(pack.entries[i].name, name, &strike)) {
      continue;
    }
    const RoboBool above = strike >= sizePx ? ROBO_TRUE : ROBO_FALSE;
    const RoboI32 dist = strike >= sizePx ? static_cast<RoboI32>(strike - sizePx) : static_cast<RoboI32>(sizePx - strike);
    if (best < 0 || (above == ROBO_TRUE && bestAbove == ROBO_FALSE) ||
        (above == bestAbove && dist < bestDist)) {
      best = static_cast<RoboI32>(i);
      bestDist = dist;
      bestAbove = above;
    }
  }
  if (best >= 0) {
    return RoboFontPackFontAt(pack, static_cast<RoboU32>(best));
  }
  return RoboFontPackFind(pack, name);
}

inline RoboFontView RoboFontPackFind(const RoboFontPackView& pack, const char* name) {
  if (pack.header == ROBO_NULL || pack.entries == ROBO_NULL || name == ROBO_NULL || name[0] == 0) {
    return RoboFontPackDefault(pack);
  }
  for (RoboU32 i = 0; i < pack.header->fontCount; ++i) {
    if (RoboFontPackNameEq(pack.entries[i].name, name)) {
      return RoboFontPackFontAt(pack, i);
    }
  }
  /* Allow "Inter" as alias for Inter-Regular */
  if (RoboFontPackNameEq(name, "Inter") || RoboFontPackNameEq(name, "default")) {
    return RoboFontPackDefault(pack);
  }
  return RoboFontView{};
}

/* Boot may still ship a lone .robofont — accept either pack or single atlas. */
inline RoboBool RoboFontBlobIsPack(const void* data, RoboSize size) {
  if (data == ROBO_NULL || size < 4) {
    return ROBO_FALSE;
  }
  return *static_cast<const RoboU32*>(data) == ROBO_FONT_PACK_MAGIC ? ROBO_TRUE : ROBO_FALSE;
}
