#include "ButtonPlatform.hpp"

#include "../../Foundation/FoundationPlatform.hpp"

#include "../../../RoboGPU/RoboGPU.hpp"

static RoboI32 gRoboButtonSpinX = 1024;
static RoboI32 gRoboButtonSpinY = 0;
static RoboI32 gRoboButtonSpinEndX = -193;
static RoboI32 gRoboButtonSpinEndY = 1006;

static RoboBool RoboButtonEq(const char* a, const char* b) {
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

static void RoboButtonCopy(char* dst, RoboSize dstSize, const char* src) {
  if (dst == ROBO_NULL || dstSize == 0) {
    return;
  }
  RoboSize index = 0;
  if (src != ROBO_NULL) {
    while (src[index] != 0 && index + 1 < dstSize) {
      dst[index] = src[index];
      ++index;
    }
  }
  dst[index] = 0;
}

static RoboI32 RoboButtonScale(RoboI32 design, RoboU32 dpi) {
  const RoboU32 used = dpi == 0 ? 96u : dpi;
  if (design <= 0) {
    return 0;
  }
  const RoboI32 scaled = static_cast<RoboI32>(static_cast<RoboU32>(design) * used / 96u);
  return scaled < 1 ? 1 : scaled;
}

static RoboU32 RoboButtonFade(RoboU32 color, RoboU8 alpha) {
  if (alpha >= 255) {
    return color | 0xFF000000u;
  }
  const RoboU32 red = ((color >> 16) & 0xFF) * alpha / 255u;
  const RoboU32 green = ((color >> 8) & 0xFF) * alpha / 255u;
  const RoboU32 blue = (color & 0xFF) * alpha / 255u;
  return 0xFF000000u | (red << 16) | (green << 8) | blue;
}

static RoboI32 RoboButtonIsqrt(RoboI32 value) {
  if (value <= 0) {
    return 0;
  }
  RoboI32 root = value;
  RoboI32 next = (root + 1) / 2;
  while (next < root) {
    root = next;
    next = (root + value / root) / 2;
  }
  return root;
}

static RoboBool RoboButtonInPill(RoboI32 localX, RoboI32 localY, RoboI32 width, RoboI32 height) {
  if (localX < 0 || localY < 0 || localX >= width || localY >= height || height < 2) {
    return ROBO_FALSE;
  }
  const RoboI32 radius = height / 2;
  const RoboI32 centerY = localY - radius;
  const RoboI32 distanceY = centerY < 0 ? -centerY : centerY;
  if (distanceY > radius) {
    return ROBO_FALSE;
  }
  const RoboI32 span = RoboButtonIsqrt(radius * radius - distanceY * distanceY);
  const RoboI32 inset = radius - span;
  return (localX >= inset && localX < width - inset) ? ROBO_TRUE : ROBO_FALSE;
}

static RoboU8 RoboButtonPillCoverage(RoboI32 localX, RoboI32 localY, RoboI32 width, RoboI32 height) {
  if (localX < 0 || localY < 0 || localX >= width || localY >= height || height < 2) {
    return 0;
  }
  const RoboI32 width4 = width * 4;
  const RoboI32 radius = height * 2;
  RoboI32 maxInset = 0;
  RoboI32 minInset = radius;
  RoboU32 insideRows = 0;
  for (RoboI32 sampleY = 0; sampleY < 4; ++sampleY) {
    const RoboI32 y = localY * 4 + sampleY;
    const RoboI32 dy = y - radius;
    const RoboI32 ady = dy < 0 ? -dy : dy;
    if (ady > radius) {
      continue;
    }
    const RoboI32 span = RoboButtonIsqrt(radius * radius - ady * ady);
    const RoboI32 inset = radius - span;
    if (inset > maxInset) {
      maxInset = inset;
    }
    if (inset < minInset) {
      minInset = inset;
    }
    ++insideRows;
  }
  if (insideRows == 0) {
    return 0;
  }
  const RoboI32 x0 = localX * 4;
  if (insideRows == 4 && x0 >= maxInset && x0 + 3 < width4 - maxInset) {
    return 255;
  }
  if (x0 + 3 < minInset || x0 >= width4 - minInset) {
    return 0;
  }
  RoboU32 hits = 0;
  for (RoboI32 sampleY = 0; sampleY < 4; ++sampleY) {
    for (RoboI32 sampleX = 0; sampleX < 4; ++sampleX) {
      if (RoboButtonInPill(x0 + sampleX, localY * 4 + sampleY, width4, height * 4)) {
        ++hits;
      }
    }
  }
  return static_cast<RoboU8>((hits * 255u) / 16u);
}

static void RoboButtonFillPill(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height, RoboU32 color,
                               RoboU8 coverage) {
  if (gpu == ROBO_NULL || width <= 0 || height <= 0 || coverage == 0) {
    return;
  }
  for (RoboI32 row = 0; row < height; ++row) {
    for (RoboI32 col = 0; col < width; ++col) {
      const RoboU32 edge = RoboButtonPillCoverage(col, row, width, height);
      if (edge == 0) {
        continue;
      }
      const RoboU8 pixel = static_cast<RoboU8>((edge * coverage) / 255u);
      RoboGPUBlendPixel(gpu, x + col, y + row, color, pixel);
    }
  }
  RoboDamageAdd(&gpu->damage, x, y, width, height);
}

static void RoboButtonStrokePill(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height, RoboI32 thickness,
                                 RoboU32 color, RoboU8 coverage) {
  if (gpu == ROBO_NULL || thickness <= 0 || coverage == 0) {
    return;
  }
  const RoboI32 grow = thickness;
  const RoboI32 outerW = width + grow * 2;
  const RoboI32 outerH = height + grow * 2;
  for (RoboI32 row = 0; row < outerH; ++row) {
    for (RoboI32 col = 0; col < outerW; ++col) {
      const RoboU32 outer = RoboButtonPillCoverage(col, row, outerW, outerH);
      const RoboU32 inner = RoboButtonPillCoverage(col - grow, row - grow, width, height);
      const RoboU32 edge = outer > inner ? outer - inner : 0;
      if (edge != 0) {
        const RoboU8 pixel = static_cast<RoboU8>((edge * coverage) / 255u);
        RoboGPUBlendPixel(gpu, x - grow + col, y - grow + row, color, pixel);
      }
    }
  }
  RoboDamageAdd(&gpu->damage, x - grow, y - grow, outerW, outerH);
}

static void RoboButtonGlow(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height, RoboU8 fade) {
  if (gpu == ROBO_NULL || width <= 0 || height <= 0 || fade == 0) {
    return;
  }
  const RoboI32 reach = 5;
  const RoboI32 gapQ = 4;
  const RoboI32 reachQ = reach * 4;
  const RoboI32 span = reachQ - gapQ;
  const RoboU32 color = RoboFoundationPlatformColor("brand-primary");
  const RoboI32 widthQ = width * 4;
  const RoboI32 heightQ = height * 4;
  const RoboI32 radius = heightQ / 2;
  const RoboI32 straight = widthQ - heightQ;
  for (RoboI32 row = -reach; row < height + reach; ++row) {
    for (RoboI32 col = -reach; col < width + reach; ++col) {
      RoboU32 sum = 0;
      for (RoboI32 sampleY = 0; sampleY < 4; ++sampleY) {
        for (RoboI32 sampleX = 0; sampleX < 4; ++sampleX) {
          RoboI32 localX = col * 4 + sampleX - radius;
          const RoboI32 localY = row * 4 + sampleY - radius;
          if (localX < 0) {
          } else if (straight > 0 && localX > straight) {
            localX -= straight;
          } else {
            localX = 0;
          }
          const RoboI32 outside = RoboButtonIsqrt(localX * localX + localY * localY) - radius;
          if (outside < gapQ || outside >= reachQ) {
            continue;
          }
          const RoboI32 remain = reachQ - outside;
          sum += static_cast<RoboU32>(remain * remain * 128 / (span * span));
        }
      }
      if (sum == 0) {
        continue;
      }
      RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>(sum * fade / (16u * 255u)));
    }
  }
  RoboDamageAdd(&gpu->damage, x - reach, y - reach, width + reach * 2, height + reach * 2);
}

static void RoboButtonRotateSpin(RoboI32* x, RoboI32* y) {
  const RoboI32 cosine = 974;
  const RoboI32 sine = 316;
  const RoboI32 nextX = (*x * cosine - *y * sine) / 1024;
  const RoboI32 nextY = (*x * sine + *y * cosine) / 1024;
  const RoboI32 length = RoboButtonIsqrt(nextX * nextX + nextY * nextY);
  if (length <= 0) {
    *x = 1024;
    *y = 0;
    return;
  }
  *x = nextX * 1024 / length;
  *y = nextY * 1024 / length;
}

static RoboBool RoboButtonOnSpinArc(RoboI32 quarterX, RoboI32 quarterY) {
  const RoboI32 fromStart = gRoboButtonSpinX * quarterY - gRoboButtonSpinY * quarterX;
  const RoboI32 toEnd = quarterX * gRoboButtonSpinEndY - quarterY * gRoboButtonSpinEndX;
  return (fromStart >= 0 && toEnd >= 0) ? ROBO_TRUE : ROBO_FALSE;
}

void RoboButtonDrawSpinner(RoboGPU* gpu, RoboI32 centerX, RoboI32 centerY, RoboU32 track, RoboU8 trackCoverage,
                           RoboU32 tick, RoboU8 tickCoverage, RoboI32 sizePx) {
  const RoboI32 size = sizePx > 0 ? sizePx : 16;
  const RoboI32 radiusQ = 7 * size / 4;
  RoboI32 halfQ = size / 8;
  if (halfQ < 1) {
    halfQ = 1;
  }
  const RoboI32 outer = radiusQ + halfQ;
  const RoboI32 inner = radiusQ > halfQ ? radiusQ - halfQ : 1;
  const RoboI32 outer2 = outer * outer;
  const RoboI32 inner2 = inner * inner;
  const RoboI32 capR2 = halfQ * halfQ;
  const RoboI32 capX = gRoboButtonSpinX * radiusQ / 1024;
  const RoboI32 capY = gRoboButtonSpinY * radiusQ / 1024;
  const RoboI32 capEndX = gRoboButtonSpinEndX * radiusQ / 1024;
  const RoboI32 capEndY = gRoboButtonSpinEndY * radiusQ / 1024;
  const RoboI32 extent = size / 2 + 1;
  for (RoboI32 row = -extent; row <= extent; ++row) {
    for (RoboI32 col = -extent; col <= extent; ++col) {
      RoboU32 trackHits = 0;
      RoboU32 tickHits = 0;
      for (RoboI32 sampleY = 0; sampleY < 4; ++sampleY) {
        for (RoboI32 sampleX = 0; sampleX < 4; ++sampleX) {
          const RoboI32 quarterX = col * 4 + sampleX - 2;
          const RoboI32 quarterY = row * 4 + sampleY - 2;
          const RoboI32 distance = quarterX * quarterX + quarterY * quarterY;
          const RoboBool ring = (distance >= inner2 && distance <= outer2) ? ROBO_TRUE : ROBO_FALSE;
          const RoboI32 startCap = (quarterX - capX) * (quarterX - capX) + (quarterY - capY) * (quarterY - capY);
          const RoboI32 endCap =
              (quarterX - capEndX) * (quarterX - capEndX) + (quarterY - capEndY) * (quarterY - capEndY);
          const RoboBool cap = (startCap <= capR2 || endCap <= capR2) ? ROBO_TRUE : ROBO_FALSE;
          if (!ring && !cap) {
            continue;
          }
          if ((ring && RoboButtonOnSpinArc(quarterX, quarterY)) || cap) {
            ++tickHits;
          } else if (ring) {
            ++trackHits;
          }
        }
      }
      if (trackHits != 0) {
        RoboGPUBlendPixel(gpu, centerX + col, centerY + row, track,
                          static_cast<RoboU8>(trackCoverage * trackHits / 16u));
      }
      if (tickHits != 0) {
        RoboGPUBlendPixel(gpu, centerX + col, centerY + row, tick,
                          static_cast<RoboU8>(tickCoverage * tickHits / 16u));
      }
    }
  }
}

static void RoboButtonPaint(RoboUIComponent* self, RoboGPU* gpu, RoboI32 absX, RoboI32 absY) {
  auto* button = reinterpret_cast<RoboButton*>(self);
  if (button == ROBO_NULL || gpu == ROBO_NULL || button->base.width <= 0 || button->base.height <= 0) {
    return;
  }
  RoboU8 shown = button->preview;
  if (!button->disabled && button->pointer == 2) {
    shown = 3;
  } else if (!button->disabled && button->pointer == 1) {
    shown = 1;
  }
  const RoboBool pressed = shown == 3 ? ROBO_TRUE : ROBO_FALSE;
  const RoboBool hover = shown == 1 ? ROBO_TRUE : ROBO_FALSE;
  const RoboBool focus = (button->focused || shown == 2) ? ROBO_TRUE : ROBO_FALSE;
  const RoboU8 fade = button->disabled ? static_cast<RoboU8>(140) : static_cast<RoboU8>(255);

  RoboU32 fill = 0;
  RoboU32 border = 0;
  RoboI32 borderPx = 0;
  RoboU32 text = RoboFoundationPlatformColor("text-on-brand");
  const RoboU32 brand = RoboFoundationPlatformColor("brand-primary");
  const RoboU32 onBrand = RoboFoundationPlatformColor("text-on-brand");
  RoboU8 fillCoverage = 255;
  if (button->variant == 1) {
    fill = 0;
    border = brand;
    borderPx = 1;
    text = brand;
    if (pressed) {
      fill = RoboFoundationPlatformColor("brand-primary-active");
      border = fill;
      text = onBrand;
    } else if (hover) {
      fill = brand;
      text = onBrand;
    }
  } else if (button->variant == 2) {
    fill = RoboFoundationPlatformColor("bg-secondary");
    border = RoboFoundationPlatformColor("border-strong");
    borderPx = 1;
    text = RoboFoundationPlatformColor("text-primary");
    if (pressed) {
      fill = RoboFoundationPlatformColor("bg-active");
      border = RoboFoundationPlatformColor("text-primary");
    } else if (hover) {
      fill = RoboFoundationPlatformColor("bg-hover");
      border = RoboFoundationPlatformColor("text-tertiary");
    }
  } else if (button->variant == 3) {
    fill = 0;
    text = RoboFoundationPlatformColor("text-tertiary");
    if (pressed) {
      fill = RoboFoundationPlatformColor("brand-primary-subtle");
      fillCoverage = static_cast<RoboU8>(((fill >> 24) & 0xFFu) * fade / 255u);
      text = RoboFoundationPlatformColor("brand-primary-active");
    } else if (hover) {
      text = brand;
    }
  } else if (button->variant == 4) {
    fill = RoboFoundationPlatformColor("danger");
    if (pressed) {
      const RoboU32 red = ((fill >> 16) & 0xFFu) * 78u / 100u;
      const RoboU32 green = ((fill >> 8) & 0xFFu) * 78u / 100u;
      const RoboU32 blue = (fill & 0xFFu) * 78u / 100u;
      fill = 0xFF000000u | (red << 16) | (green << 8) | blue;
    } else if (hover) {
      fill = RoboFoundationPlatformColor("danger-hover");
    }
    text = onBrand;
  } else {
    const char* token = "brand-primary";
    if (shown == 1) {
      token = "brand-primary-hover";
    } else if (pressed) {
      token = "brand-primary-active";
    }
    fill = RoboFoundationPlatformColor(token);
    text = onBrand;
  }
  text = RoboButtonFade(text, fade);
  if (fill != 0) {
    const RoboU32 paintFill = fillCoverage == 255 ? RoboButtonFade(fill, fade) : (fill | 0xFF000000u);
    RoboButtonFillPill(gpu, absX, absY, button->base.width, button->base.height, paintFill, fillCoverage);
  }
  if (focus && borderPx > 0) {
    border = brand;
  }
  if (borderPx > 0) {
    RoboButtonStrokePill(gpu, absX, absY, button->base.width, button->base.height, borderPx, RoboButtonFade(border, fade),
                         255);
  }
  if (focus) {
    RoboButtonGlow(gpu, absX, absY, button->base.width, button->base.height, fade);
  }

  const RoboI32 iconSize = button->leadingIcon.sizePx > 0 ? button->leadingIcon.sizePx : 16;
  const RoboI32 gap = iconSize / 2;
  RoboI32 textW = 0;
  RoboI32 textH = 0;
  RoboUITextMeasure(&button->label, &textW, &textH);
  RoboI32 contentW = textW;
  const RoboBool showLeading = button->showLeading && !button->loading ? ROBO_TRUE : ROBO_FALSE;
  const RoboBool showTrailing = button->showTrailing && !button->loading ? ROBO_TRUE : ROBO_FALSE;
  if (showLeading) {
    contentW += iconSize + gap;
  }
  if (showTrailing) {
    contentW += iconSize + gap;
  }
  if (button->loading) {
    contentW += iconSize + gap;
  }
  RoboI32 cursor = absX + (button->base.width - contentW) / 2;
  const RoboI32 midY = absY + button->base.height / 2;
  if (button->loading) {
    RoboU32 track = RoboFoundationPlatformColor("text-tertiary");
    RoboU32 tick = brand;
    if (button->variant == 0 || button->variant == 4) {
      track = onBrand;
      tick = onBrand;
    }
    RoboButtonDrawSpinner(gpu, cursor + iconSize / 2, midY, track, static_cast<RoboU8>(fade * 35 / 100), tick, fade, iconSize);
    cursor += iconSize + gap;
  }
  if (showLeading) {
    RoboIconSetSlotColor(&button->leadingIcon, "fill", text);
    RoboIconSetSlotColor(&button->leadingIcon, "stroke", text);
    if (button->leadingIcon.base.vtable != ROBO_NULL && button->leadingIcon.base.vtable->Paint != ROBO_NULL) {
      button->leadingIcon.base.vtable->Paint(&button->leadingIcon.base, gpu, cursor, midY - iconSize / 2);
    }
    cursor += iconSize + gap;
  }
  button->label.colorARGB = text;
  for (RoboU16 span = 0; span < button->label.spanCount; ++span) {
    button->label.spans[span].colorARGB = text;
  }
  if (button->label.base.vtable != ROBO_NULL && button->label.base.vtable->Paint != ROBO_NULL) {
    button->label.base.vtable->Paint(&button->label.base, gpu, cursor, midY - textH / 2);
  }
  cursor += textW + gap;
  if (showTrailing) {
    RoboIconSetSlotColor(&button->trailingIcon, "fill", text);
    RoboIconSetSlotColor(&button->trailingIcon, "stroke", text);
    if (button->trailingIcon.base.vtable != ROBO_NULL && button->trailingIcon.base.vtable->Paint != ROBO_NULL) {
      button->trailingIcon.base.vtable->Paint(&button->trailingIcon.base, gpu, cursor, midY - iconSize / 2);
    }
  }
}

void RoboButtonPaintSpin(RoboButton* button, RoboGPU* gpu, RoboI32 absX, RoboI32 absY) {
  if (button == ROBO_NULL || gpu == ROBO_NULL || !button->loading || button->base.width <= 0 || button->base.height <= 0) {
    return;
  }
  const RoboU8 fade = button->disabled ? static_cast<RoboU8>(140) : static_cast<RoboU8>(255);
  const RoboU32 brand = RoboFoundationPlatformColor("brand-primary");
  const RoboU32 onBrand = RoboFoundationPlatformColor("text-on-brand");
  RoboU32 fill = 0;
  if (button->variant == 2) {
    fill = RoboFoundationPlatformColor("bg-secondary");
  } else if (button->variant == 4) {
    fill = RoboFoundationPlatformColor("danger");
  } else if (button->variant == 0) {
    fill = RoboFoundationPlatformColor("brand-primary");
  }
  const RoboI32 iconSize = button->leadingIcon.sizePx > 0 ? button->leadingIcon.sizePx : 16;
  const RoboI32 gap = iconSize / 2;
  RoboI32 textW = 0;
  RoboI32 textH = 0;
  RoboUITextMeasure(&button->label, &textW, &textH);
  const RoboI32 contentW = textW + iconSize + gap;
  const RoboI32 cursor = absX + (button->base.width - contentW) / 2;
  const RoboI32 midY = absY + button->base.height / 2;
  const RoboI32 centerX = cursor + iconSize / 2;
  const RoboI32 left = centerX - iconSize / 2 - 1;
  const RoboI32 top = midY - iconSize / 2 - 1;
  const RoboU32 under = fill != 0 ? RoboButtonFade(fill, fade) : RoboFoundationPlatformColor("bg-primary");
  const RoboU32 packed = RoboGPUPackColor(gpu, static_cast<RoboU8>((under >> 16) & 0xFF),
                                          static_cast<RoboU8>((under >> 8) & 0xFF), static_cast<RoboU8>(under & 0xFF),
                                          255);
  RoboGPUFillRect(gpu, left, top, iconSize + 2, iconSize + 2, packed);
  RoboU32 track = RoboFoundationPlatformColor("text-tertiary");
  RoboU32 tick = brand;
  if (button->variant == 0 || button->variant == 4) {
    track = onBrand;
    tick = onBrand;
  }
  RoboButtonDrawSpinner(gpu, centerX, midY, track, static_cast<RoboU8>(fade * 35 / 100), tick, fade, iconSize);
}

static void RoboButtonDestroy(RoboUIComponent* self) { (void)self; }
static void RoboButtonTick(RoboUIComponent* self, RoboU32 deltaMs) {
  (void)self;
  (void)deltaMs;
}

static const RoboUIVtable gRoboButtonVtable = {RoboButtonDestroy, RoboButtonPaint, ROBO_NULL, RoboButtonTick};

void RoboButtonInit(RoboButton* button, const char* id) {
  if (button == ROBO_NULL) {
    return;
  }
  RoboMemorySet(button, 0, sizeof(RoboButton));
  RoboUIComponentInitBase(&button->base, &gRoboButtonVtable, RoboUINodeKind::Button, id);
  RoboUITextInit(&button->label, "button-label");
  button->variant = 0;
  button->size = 1;
  button->fullWidth = ROBO_TRUE;
  button->base.height = 44;
  button->base.layout.preferHeight = 44;
  RoboButtonCopy(button->labelText, sizeof(button->labelText), "");
}

RoboUIComponent* RoboButtonAsComponent(RoboButton* button) {
  return button == ROBO_NULL ? ROBO_NULL : &button->base;
}

RoboButton* RoboButtonFromComponent(RoboUIComponent* component) {
  if (component == ROBO_NULL || component->kind != RoboUINodeKind::Button) {
    return ROBO_NULL;
  }
  return reinterpret_cast<RoboButton*>(component);
}

void RoboButtonSetLabel(RoboButton* button, const char* text) {
  if (button != ROBO_NULL) {
    RoboButtonCopy(button->labelText, sizeof(button->labelText), text);
  }
}

void RoboButtonSetVariant(RoboButton* button, const char* variant) {
  if (button == ROBO_NULL) {
    return;
  }
  button->variant = 0;
  if (RoboButtonEq(variant, "secondary")) {
    button->variant = 1;
  } else if (RoboButtonEq(variant, "action")) {
    button->variant = 2;
  } else if (RoboButtonEq(variant, "ghost")) {
    button->variant = 3;
  } else if (RoboButtonEq(variant, "danger") || RoboButtonEq(variant, "destructive")) {
    button->variant = 4;
  }
}

void RoboButtonSetSize(RoboButton* button, const char* size) {
  if (button == ROBO_NULL) {
    return;
  }
  button->size = 1;
  if (RoboButtonEq(size, "sm") || RoboButtonEq(size, "small")) {
    button->size = 0;
  } else if (RoboButtonEq(size, "lg") || RoboButtonEq(size, "large")) {
    button->size = 2;
  }
}

void RoboButtonSetPointer(RoboButton* button, RoboU8 pointer) {
  if (button != ROBO_NULL) {
    button->pointer = pointer > 2 ? static_cast<RoboU8>(0) : pointer;
  }
}

void RoboButtonSetFocused(RoboButton* button, RoboBool focused) {
  if (button != ROBO_NULL) {
    button->focused = focused;
  }
}

void RoboButtonSetPreview(RoboButton* button, const char* preview) {
  if (button == ROBO_NULL) {
    return;
  }
  button->preview = 0;
  if (RoboButtonEq(preview, "hover")) {
    button->preview = 1;
  } else if (RoboButtonEq(preview, "focus")) {
    button->preview = 2;
  } else if (RoboButtonEq(preview, "active")) {
    button->preview = 3;
  }
}

void RoboButtonSetIcons(RoboButton* button, const char* leading, const char* trailing) {
  if (button == ROBO_NULL) {
    return;
  }
  RoboButtonCopy(button->leadingName, sizeof(button->leadingName), leading);
  RoboButtonCopy(button->trailingName, sizeof(button->trailingName), trailing);
  button->showLeading = button->leadingName[0] != 0 ? ROBO_TRUE : ROBO_FALSE;
  button->showTrailing = button->trailingName[0] != 0 ? ROBO_TRUE : ROBO_FALSE;
}

void RoboButtonSetFlags(RoboButton* button, RoboBool fullWidth, RoboBool loading, RoboBool disabled,
                        const char* loadingText) {
  if (button == ROBO_NULL) {
    return;
  }
  button->fullWidth = fullWidth;
  button->loading = loading;
  button->disabled = (disabled || loading) ? ROBO_TRUE : ROBO_FALSE;
  RoboButtonCopy(button->loadingText, sizeof(button->loadingText), loadingText);
}

void RoboButtonSync(RoboButton* button, RoboUIContext* ctx, RoboU32 dpi) {
  if (button == ROBO_NULL) {
    return;
  }
  const RoboI32 height = RoboButtonScale(button->size == 0 ? 32 : (button->size == 2 ? 50 : 44), dpi);
  const RoboI32 pad = RoboButtonScale(button->size == 0 ? 14 : (button->size == 2 ? 24 : 22), dpi);
  const RoboI32 type = RoboButtonScale(button->size == 0 ? 14 : 17, dpi);
  const RoboI32 icon = RoboButtonScale(16, dpi);
  const RoboI32 gap = RoboButtonScale(8, dpi);
  const char* face = button->variant == 3 ? "Inter-Regular" : "Inter-Medium";
  const char* text = button->loading && button->loadingText[0] != 0 ? button->loadingText : button->labelText;
  button->label.sizePx = static_cast<RoboU16>(type);
  button->label.leadingMilli = 1350;
  button->label.trackingMilli = 0;
  button->label.maxWidth = 4096;
  button->label.wrap = RoboTextWrap::None;
  if (ctx != ROBO_NULL) {
    RoboUITextSetFontName(&button->label, ctx, face);
  }
  RoboUITextSetContent(&button->label, text != ROBO_NULL ? text : "");
  RoboI32 textW = 0;
  RoboI32 textH = 0;
  RoboUITextMeasure(&button->label, &textW, &textH);
  RoboI32 width = pad * 2 + textW;
  if (button->loading) {
    width += icon + gap;
  } else {
    if (button->showLeading) {
      width += icon + gap;
    }
    if (button->showTrailing) {
      width += icon + gap;
    }
  }
  button->base.height = height;
  button->base.layout.preferHeight = height;
  button->base.layout.preferWidth = width;
  if (button->fullWidth) {
    button->base.width = 0;
    button->base.layout.widthPercent = 100;
  } else {
    button->base.width = width;
    button->base.layout.widthPercent = 0;
  }
  if (ctx != ROBO_NULL && button->showLeading) {
    RoboIconInit(&button->leadingIcon, "leading", button->leadingName);
    RoboIconSetSize(&button->leadingIcon, icon);
    RoboIconResolve(&button->leadingIcon, ctx);
  }
  if (ctx != ROBO_NULL && button->showTrailing) {
    RoboIconInit(&button->trailingIcon, "trailing", button->trailingName);
    RoboIconSetSize(&button->trailingIcon, icon);
    RoboIconResolve(&button->trailingIcon, ctx);
  }
}

void RoboButtonAdvanceSpin() {
  RoboButtonRotateSpin(&gRoboButtonSpinX, &gRoboButtonSpinY);
  RoboButtonRotateSpin(&gRoboButtonSpinEndX, &gRoboButtonSpinEndY);
}
