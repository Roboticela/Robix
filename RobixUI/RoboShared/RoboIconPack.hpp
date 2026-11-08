#pragma once

#include "RoboTypes.hpp"

/*
 * Icon packs — SVG sources never ship on ESP.
 * Assets/Icons.rpack  = master catalog (SVG + named color slots) for the toolchain.
 * Icons.pack          = app subset: coverage layers per slot (ROIC), loaded at runtime.
 * ESP 8.3: ICONS.PAK
 */

#pragma pack(push, 1)

constexpr RoboU32 ROBO_ICON_CATALOG_MAGIC = 0x50434952u; /* 'RICP' — Robo Icon Catalog Pack */
constexpr RoboU32 ROBO_ICON_CATALOG_VERSION = 1;
constexpr RoboU32 ROBO_ICON_PACK_MAGIC = 0x43494F52u; /* 'ROIC' — Robo Icon (cooked) Pack */
constexpr RoboU32 ROBO_ICON_PACK_VERSION = 1;
constexpr RoboU32 ROBO_ICON_NAME_MAX = 48;
constexpr RoboU32 ROBO_ICON_SLOT_NAME_MAX = 16;
constexpr RoboU32 ROBO_ICON_MAX_SLOTS = 8;

struct RoboIconCatalogSlot {
  char name[ROBO_ICON_SLOT_NAME_MAX];
};

struct RoboIconCatalogEntry {
  char name[ROBO_ICON_NAME_MAX];
  RoboU32 svgOffset;
  RoboU32 svgSize;
  RoboU32 slotCount;
  RoboU32 slotOffset; /* array of RoboIconCatalogSlot */
};

struct RoboIconCatalogHeader {
  RoboU32 magic;
  RoboU32 version;
  RoboU32 iconCount;
  RoboU32 entryOffset;
  RoboU32 reserved0;
  RoboU32 reserved1;
};

struct RoboIconCookedLayer {
  char slotName[ROBO_ICON_SLOT_NAME_MAX];
  RoboU32 defaultARGB; /* used when app omits the slot color */
  RoboU32 width;
  RoboU32 height;
  RoboU32 stride;      /* bytes per row (width for coverage) */
  RoboU32 pixelOffset; /* 8-bit coverage from start of pack */
};

struct RoboIconCookedEntry {
  char name[ROBO_ICON_NAME_MAX];
  RoboU32 layerCount;
  RoboU32 layerOffset; /* array of RoboIconCookedLayer */
  RoboU16 baseSize;    /* cook atlas edge (square) */
  RoboU16 reserved0;
};

struct RoboIconPackHeader {
  RoboU32 magic;
  RoboU32 version;
  RoboU32 iconCount;
  RoboU32 baseSize;
  RoboU32 entryOffset;
  RoboU32 reserved0;
};

#pragma pack(pop)

struct RoboIconPackView {
  const RoboIconPackHeader* header;
  const RoboIconCookedEntry* entries;
  const RoboU8* bytes;
  RoboSize size;
};

struct RoboIconCatalogView {
  const RoboIconCatalogHeader* header;
  const RoboIconCatalogEntry* entries;
  const RoboU8* bytes;
  RoboSize size;
};

inline RoboBool RoboIconNameEq(const char* a, const char* b) {
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

inline RoboBool RoboIconCatalogValidate(const void* data, RoboSize size) {
  if (data == ROBO_NULL || size < sizeof(RoboIconCatalogHeader)) {
    return ROBO_FALSE;
  }
  const auto* header = static_cast<const RoboIconCatalogHeader*>(data);
  if (header->magic != ROBO_ICON_CATALOG_MAGIC || header->version != ROBO_ICON_CATALOG_VERSION) {
    return ROBO_FALSE;
  }
  if (header->entryOffset + static_cast<RoboU64>(header->iconCount) * sizeof(RoboIconCatalogEntry) > size) {
    return ROBO_FALSE;
  }
  return ROBO_TRUE;
}

inline RoboIconCatalogView RoboIconCatalogMakeView(const void* data, RoboSize size) {
  RoboIconCatalogView view{};
  view.header = static_cast<const RoboIconCatalogHeader*>(data);
  view.bytes = static_cast<const RoboU8*>(data);
  view.size = size;
  view.entries = reinterpret_cast<const RoboIconCatalogEntry*>(view.bytes + view.header->entryOffset);
  return view;
}

inline const RoboIconCatalogEntry* RoboIconCatalogFind(const RoboIconCatalogView& catalog, const char* name) {
  if (catalog.header == ROBO_NULL || name == ROBO_NULL) {
    return ROBO_NULL;
  }
  for (RoboU32 i = 0; i < catalog.header->iconCount; ++i) {
    if (RoboIconNameEq(catalog.entries[i].name, name)) {
      return &catalog.entries[i];
    }
  }
  return ROBO_NULL;
}

inline RoboBool RoboIconPackValidate(const void* data, RoboSize size) {
  if (data == ROBO_NULL || size < sizeof(RoboIconPackHeader)) {
    return ROBO_FALSE;
  }
  const auto* header = static_cast<const RoboIconPackHeader*>(data);
  if (header->magic != ROBO_ICON_PACK_MAGIC || header->version != ROBO_ICON_PACK_VERSION) {
    return ROBO_FALSE;
  }
  if (header->entryOffset + static_cast<RoboU64>(header->iconCount) * sizeof(RoboIconCookedEntry) > size) {
    return ROBO_FALSE;
  }
  return ROBO_TRUE;
}

inline RoboIconPackView RoboIconPackMakeView(const void* data, RoboSize size) {
  RoboIconPackView view{};
  view.header = static_cast<const RoboIconPackHeader*>(data);
  view.bytes = static_cast<const RoboU8*>(data);
  view.size = size;
  view.entries = reinterpret_cast<const RoboIconCookedEntry*>(view.bytes + view.header->entryOffset);
  return view;
}

inline const RoboIconCookedEntry* RoboIconPackFind(const RoboIconPackView& pack, const char* name) {
  if (pack.header == ROBO_NULL || name == ROBO_NULL) {
    return ROBO_NULL;
  }
  for (RoboU32 i = 0; i < pack.header->iconCount; ++i) {
    if (RoboIconNameEq(pack.entries[i].name, name)) {
      return &pack.entries[i];
    }
  }
  return ROBO_NULL;
}

inline const RoboIconCookedLayer* RoboIconPackLayers(const RoboIconPackView& pack, const RoboIconCookedEntry* entry) {
  if (pack.bytes == ROBO_NULL || entry == ROBO_NULL) {
    return ROBO_NULL;
  }
  return reinterpret_cast<const RoboIconCookedLayer*>(pack.bytes + entry->layerOffset);
}
