#include "IconPlatform.hpp"

#include "../../../RoboGPU/RoboGPU.hpp"

static RoboBool RoboIconEq(const char* a, const char* b) {
  return RoboIconNameEq(a, b);
}

static void RoboIconCopyNameLocal(char* dst, RoboSize dstSize, const char* src) {
  if (dst == ROBO_NULL || dstSize == 0) {
    return;
  }
  RoboSize i = 0;
  if (src != ROBO_NULL) {
    while (src[i] != 0 && i + 1 < dstSize) {
      dst[i] = src[i];
      ++i;
    }
  }
  dst[i] = 0;
}

static RoboI32 RoboIconParseI32(const char* value, RoboI32 fallback) {
  if (value == ROBO_NULL || value[0] == 0) {
    return fallback;
  }
  RoboBool neg = ROBO_FALSE;
  RoboSize i = 0;
  if (value[0] == '-') {
    neg = ROBO_TRUE;
    i = 1;
  }
  RoboI32 n = 0;
  for (; value[i] != 0; ++i) {
    if (value[i] < '0' || value[i] > '9') {
      break;
    }
    n = n * 10 + (value[i] - '0');
  }
  return neg ? -n : n;
}

static RoboU8 RoboIconHexNibble(char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<RoboU8>(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return static_cast<RoboU8>(10 + c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return static_cast<RoboU8>(10 + c - 'A');
  }
  return 0;
}

static RoboU32 RoboIconParseColor(const char* text) {
  if (text == ROBO_NULL || text[0] != '#') {
    return 0xFFFFFFFFu;
  }
  RoboSize len = RoboStringLength(text + 1);
  RoboU8 a = 255, r = 0, g = 0, b = 0;
  if (len == 3) {
    r = static_cast<RoboU8>(RoboIconHexNibble(text[1]) * 17);
    g = static_cast<RoboU8>(RoboIconHexNibble(text[2]) * 17);
    b = static_cast<RoboU8>(RoboIconHexNibble(text[3]) * 17);
  } else if (len == 6) {
    r = static_cast<RoboU8>((RoboIconHexNibble(text[1]) << 4) | RoboIconHexNibble(text[2]));
    g = static_cast<RoboU8>((RoboIconHexNibble(text[3]) << 4) | RoboIconHexNibble(text[4]));
    b = static_cast<RoboU8>((RoboIconHexNibble(text[5]) << 4) | RoboIconHexNibble(text[6]));
  } else if (len == 8) {
    a = static_cast<RoboU8>((RoboIconHexNibble(text[1]) << 4) | RoboIconHexNibble(text[2]));
    r = static_cast<RoboU8>((RoboIconHexNibble(text[3]) << 4) | RoboIconHexNibble(text[4]));
    g = static_cast<RoboU8>((RoboIconHexNibble(text[5]) << 4) | RoboIconHexNibble(text[6]));
    b = static_cast<RoboU8>((RoboIconHexNibble(text[7]) << 4) | RoboIconHexNibble(text[8]));
  }
  return (static_cast<RoboU32>(a) << 24) | (static_cast<RoboU32>(r) << 16) | (static_cast<RoboU32>(g) << 8) |
         static_cast<RoboU32>(b);
}

static RoboU32 RoboIconResolveSlotColor(const RoboIcon* icon, const char* slotName, RoboU32 defaultARGB) {
  if (icon == ROBO_NULL || slotName == ROBO_NULL) {
    return defaultARGB;
  }
  for (RoboU32 i = 0; i < icon->slotCount; ++i) {
    if (icon->slots[i].set && RoboIconEq(icon->slots[i].name, slotName)) {
      return icon->slots[i].colorARGB;
    }
  }
  /* Aliases: color → fill */
  if (RoboIconEq(slotName, "fill")) {
    for (RoboU32 i = 0; i < icon->slotCount; ++i) {
      if (icon->slots[i].set && RoboIconEq(icon->slots[i].name, "color")) {
        return icon->slots[i].colorARGB;
      }
    }
  }
  return defaultARGB;
}

static void RoboIconPaint(RoboUIComponent* self, RoboGPU* gpu, RoboI32 absX, RoboI32 absY) {
  auto* icon = reinterpret_cast<RoboIcon*>(self);
  if (icon == ROBO_NULL || gpu == ROBO_NULL || icon->cooked == ROBO_NULL) {
    return;
  }
  /* Pack bytes live in icon->base.userData when resolved via context. */
  const auto* packBytes = static_cast<const RoboU8*>(icon->base.userData);
  if (packBytes == ROBO_NULL) {
    return;
  }
  const RoboIconCookedEntry* entry = icon->cooked;
  const auto* layers = reinterpret_cast<const RoboIconCookedLayer*>(packBytes + entry->layerOffset);
  const RoboI32 dst = icon->sizePx > 0 ? icon->sizePx : static_cast<RoboI32>(entry->baseSize);
  if (dst <= 0) {
    return;
  }

  for (RoboU32 i = 0; i < entry->layerCount; ++i) {
    const RoboIconCookedLayer& layer = layers[i];
    const RoboU32 color = RoboIconResolveSlotColor(icon, layer.slotName, layer.defaultARGB);
    if (((color >> 24) & 0xFF) == 0 || icon->opacity == 0) {
      continue;
    }
    const RoboU8 r = static_cast<RoboU8>((color >> 16) & 0xFF);
    const RoboU8 g = static_cast<RoboU8>((color >> 8) & 0xFF);
    const RoboU8 b = static_cast<RoboU8>(color & 0xFF);
    const RoboU8* coverage = packBytes + layer.pixelOffset;
    if (icon->opacity < 255) {
      /* Scale coverage by opacity (stack-limited; cook base ≤ 128). */
      constexpr RoboU32 ROBO_ICON_COV_MAX = 128u * 128u;
      RoboU8 scaled[ROBO_ICON_COV_MAX];
      const RoboU32 count = layer.width * layer.height;
      if (count > 0 && count <= ROBO_ICON_COV_MAX) {
        for (RoboU32 p = 0; p < count; ++p) {
          scaled[p] = static_cast<RoboU8>((static_cast<RoboU32>(coverage[p]) * icon->opacity) / 255u);
        }
        RoboGPUBlitCoverageScaled(gpu, absX, absY, dst, dst, scaled, layer.width, layer.height, r, g, b);
        continue;
      }
    }
    RoboGPUBlitCoverageScaled(gpu, absX, absY, dst, dst, coverage, layer.width, layer.height, r, g, b);
  }
}

static RoboBool RoboIconSetProperty(RoboUIComponent* self, const char* key, const char* value) {
  auto* icon = reinterpret_cast<RoboIcon*>(self);
  if (icon == ROBO_NULL || key == ROBO_NULL) {
    return ROBO_FALSE;
  }
  if (RoboUILayoutSetProperty(&icon->base.layout, key, value)) {
    return ROBO_TRUE;
  }
  if (RoboIconEq(key, "name") || RoboIconEq(key, "icon") || RoboIconEq(key, "src")) {
    RoboIconSetName(icon, value);
    return ROBO_TRUE;
  }
  if (RoboIconEq(key, "size") || RoboIconEq(key, "width") || RoboIconEq(key, "height")) {
    /* size sets square; width/height percent handled by layout. */
    if (value != ROBO_NULL && RoboStringLength(value) > 0 && value[RoboStringLength(value) - 1] == '%') {
      return RoboUILayoutSetProperty(&icon->base.layout, key, value);
    }
    const RoboI32 s = RoboIconParseI32(value, icon->sizePx);
    RoboIconSetSize(icon, s);
    return ROBO_TRUE;
  }
  if (RoboIconEq(key, "opacity")) {
    const RoboI32 pct = RoboIconParseI32(value, 100);
    if (pct <= 0) {
      icon->opacity = 0;
    } else if (pct >= 100) {
      icon->opacity = 255;
    } else {
      icon->opacity = static_cast<RoboU8>((pct * 255) / 100);
    }
    return ROBO_TRUE;
  }
  if (RoboIconEq(key, "x")) {
    icon->base.x = RoboIconParseI32(value, icon->base.x);
    return ROBO_TRUE;
  }
  if (RoboIconEq(key, "y")) {
    icon->base.y = RoboIconParseI32(value, icon->base.y);
    return ROBO_TRUE;
  }
  if (RoboIconEq(key, "strokeWidth") || RoboIconEq(key, "strokewidth")) {
    /* Cooked into Icons.pack; accept for API compatibility. */
    return ROBO_TRUE;
  }
  /* Named slot colors: fill, stroke, accent, color, or any slot name. */
  if (value != ROBO_NULL && value[0] == '#') {
    RoboIconSetSlotColor(icon, key, RoboIconParseColor(value));
    return ROBO_TRUE;
  }
  return ROBO_FALSE;
}

static void RoboIconDestroy(RoboUIComponent* self) {
  (void)self;
}

static void RoboIconTick(RoboUIComponent* self, RoboU32 deltaMs) {
  (void)self;
  (void)deltaMs;
}

static const RoboUIVtable gRoboIconVtable = {
    RoboIconDestroy,
    RoboIconPaint,
    RoboIconSetProperty,
    RoboIconTick,
};

void RoboIconInit(RoboIcon* icon, const char* id, const char* iconName) {
  if (icon == ROBO_NULL) {
    return;
  }
  RoboMemorySet(icon, 0, sizeof(RoboIcon));
  RoboUIComponentInitBase(&icon->base, &gRoboIconVtable, RoboUINodeKind::Icon, id);
  RoboIconCopyNameLocal(icon->iconName, sizeof(icon->iconName), iconName != ROBO_NULL ? iconName : id);
  icon->sizePx = 24;
  icon->opacity = 255;
  icon->base.width = 24;
  icon->base.height = 24;
  icon->base.layout.preferWidth = 24;
  icon->base.layout.preferHeight = 24;
}

RoboUIComponent* RoboIconAsComponent(RoboIcon* icon) {
  return icon == ROBO_NULL ? ROBO_NULL : &icon->base;
}

RoboIcon* RoboIconFromComponent(RoboUIComponent* component) {
  if (component == ROBO_NULL || component->kind != RoboUINodeKind::Icon) {
    return ROBO_NULL;
  }
  return reinterpret_cast<RoboIcon*>(component);
}

void RoboIconSetName(RoboIcon* icon, const char* iconName) {
  if (icon == ROBO_NULL) {
    return;
  }
  RoboIconCopyNameLocal(icon->iconName, sizeof(icon->iconName), iconName);
  icon->cooked = ROBO_NULL;
}

void RoboIconSetSize(RoboIcon* icon, RoboI32 sizePx) {
  if (icon == ROBO_NULL) {
    return;
  }
  if (sizePx < 1) {
    sizePx = 1;
  }
  icon->sizePx = sizePx;
  icon->base.width = sizePx;
  icon->base.height = sizePx;
  icon->base.layout.preferWidth = sizePx;
  icon->base.layout.preferHeight = sizePx;
}

void RoboIconSetSlotColor(RoboIcon* icon, const char* slotName, RoboU32 colorARGB) {
  if (icon == ROBO_NULL || slotName == ROBO_NULL) {
    return;
  }
  for (RoboU32 i = 0; i < icon->slotCount; ++i) {
    if (RoboIconEq(icon->slots[i].name, slotName)) {
      icon->slots[i].colorARGB = colorARGB;
      icon->slots[i].set = ROBO_TRUE;
      return;
    }
  }
  if (icon->slotCount >= ROBO_ICON_RUNTIME_SLOTS) {
    return;
  }
  RoboIconSlotColor& slot = icon->slots[icon->slotCount++];
  RoboIconCopyNameLocal(slot.name, sizeof(slot.name), slotName);
  slot.colorARGB = colorARGB;
  slot.set = ROBO_TRUE;
}

void RoboIconResolve(RoboIcon* icon, RoboUIContext* ctx) {
  if (icon == ROBO_NULL || ctx == ROBO_NULL) {
    return;
  }
  if (!RoboIconPackValidate(ctx->iconPack.bytes, ctx->iconPack.size) && ctx->iconPack.header != ROBO_NULL) {
    /* view may already be valid */
  }
  if (ctx->iconPack.header == ROBO_NULL) {
    icon->cooked = ROBO_NULL;
    return;
  }
  icon->cooked = RoboIconPackFind(ctx->iconPack, icon->iconName);
  icon->base.userData = const_cast<RoboU8*>(ctx->iconPack.bytes);
}
