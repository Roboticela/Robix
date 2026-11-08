#include "IconButtonPlatform.hpp"

#include "../../Foundation/FoundationPlatform.hpp"

#include "../../../RoboGPU/RoboGPU.hpp"
#include "../../../RoboUI/RoboButton.hpp"

static RoboBool RoboIconButtonEq(const char* a, const char* b) {
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

static void RoboIconButtonCopy(char* dst, RoboSize dstSize, const char* src) {
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

static RoboI32 RoboIconButtonScale(RoboI32 design, RoboU32 dpi) {
  const RoboU32 used = dpi == 0 ? 96u : dpi;
  if (design <= 0) {
    return 0;
  }
  const RoboI32 scaled = static_cast<RoboI32>(static_cast<RoboU32>(design) * used / 96u);
  return scaled < 1 ? 1 : scaled;
}

static RoboU32 RoboIconButtonFade(RoboU32 color, RoboU8 alpha) {
  if (alpha >= 255) {
    return color | 0xFF000000u;
  }
  const RoboU32 red = ((color >> 16) & 0xFF) * alpha / 255u;
  const RoboU32 green = ((color >> 8) & 0xFF) * alpha / 255u;
  const RoboU32 blue = (color & 0xFF) * alpha / 255u;
  return 0xFF000000u | (red << 16) | (green << 8) | blue;
}

static RoboU8 RoboIconButtonCircleCoverage(RoboI32 localX, RoboI32 localY, RoboI32 diameter) {
  if (localX < 0 || localY < 0 || localX >= diameter || localY >= diameter || diameter < 2) {
    return 0;
  }
  const RoboI32 radius = diameter * 2;
  const RoboI32 cx = localX * 4 + 2 - radius;
  const RoboI32 cy = localY * 4 + 2 - radius;
  const RoboI32 limit = radius * radius;
  RoboU32 hits = 0;
  for (RoboI32 sampleY = 0; sampleY < 4; ++sampleY) {
    for (RoboI32 sampleX = 0; sampleX < 4; ++sampleX) {
      const RoboI32 dx = cx + sampleX - 2;
      const RoboI32 dy = cy + sampleY - 2;
      if (dx * dx + dy * dy <= limit) {
        ++hits;
      }
    }
  }
  return static_cast<RoboU8>((hits * 255u) / 16u);
}

static void RoboIconButtonFillCircle(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 diameter, RoboU32 color,
                                     RoboU8 coverage) {
  if (gpu == ROBO_NULL || diameter <= 0 || coverage == 0) {
    return;
  }
  for (RoboI32 row = 0; row < diameter; ++row) {
    for (RoboI32 col = 0; col < diameter; ++col) {
      const RoboU8 edge = RoboIconButtonCircleCoverage(col, row, diameter);
      if (edge == 0) {
        continue;
      }
      RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>((edge * coverage) / 255u));
    }
  }
  RoboDamageAdd(&gpu->damage, x, y, diameter, diameter);
}

static void RoboIconButtonStrokeCircle(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 diameter, RoboU32 color,
                                       RoboU8 coverage) {
  if (gpu == ROBO_NULL || diameter < 3 || coverage == 0) {
    return;
  }
  const RoboI32 radius = diameter * 2;
  const RoboI32 outer2 = radius * radius;
  const RoboI32 inner = radius - 4;
  const RoboI32 inner2 = inner * inner;
  for (RoboI32 row = 0; row < diameter; ++row) {
    for (RoboI32 col = 0; col < diameter; ++col) {
      RoboU32 hits = 0;
      for (RoboI32 sampleY = 0; sampleY < 4; ++sampleY) {
        for (RoboI32 sampleX = 0; sampleX < 4; ++sampleX) {
          const RoboI32 dx = col * 4 + sampleX - radius;
          const RoboI32 dy = row * 4 + sampleY - radius;
          const RoboI32 distance = dx * dx + dy * dy;
          if (distance <= outer2 && distance >= inner2) {
            ++hits;
          }
        }
      }
      if (hits != 0) {
        RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>(coverage * hits / 16u));
      }
    }
  }
  RoboDamageAdd(&gpu->damage, x, y, diameter, diameter);
}

static RoboI32 RoboIconButtonMap(RoboI32 designQ, RoboI32 mark) {
  return designQ * mark / 96;
}

static void RoboIconButtonSegment(RoboGPU* gpu, RoboI32 x0, RoboI32 y0, RoboI32 x1, RoboI32 y1, RoboI32 strokePx,
                                  RoboU32 color) {
  if (gpu == ROBO_NULL || strokePx <= 0) {
    return;
  }
  const RoboI32 dx = (x1 - x0) * 4;
  const RoboI32 dy = (y1 - y0) * 4;
  const RoboI32 len2 = dx * dx + dy * dy;
  const RoboI32 pad = strokePx + 2;
  const RoboI32 minX = (x0 < x1 ? x0 : x1) - pad;
  const RoboI32 maxX = (x0 > x1 ? x0 : x1) + pad;
  const RoboI32 minY = (y0 < y1 ? y0 : y1) - pad;
  const RoboI32 maxY = (y0 > y1 ? y0 : y1) + pad;
  const RoboI32 radius = strokePx * 2;
  const RoboI32 limit = radius * radius;
  for (RoboI32 y = minY; y <= maxY; ++y) {
    for (RoboI32 x = minX; x <= maxX; ++x) {
      RoboU32 hits = 0;
      for (RoboI32 sampleY = 0; sampleY < 4; ++sampleY) {
        for (RoboI32 sampleX = 0; sampleX < 4; ++sampleX) {
          const RoboI32 px = (x * 4 + sampleX) - (x0 * 4);
          const RoboI32 py = (y * 4 + sampleY) - (y0 * 4);
          RoboI32 qx = px;
          RoboI32 qy = py;
          if (len2 > 0) {
            RoboI32 dot = px * dx + py * dy;
            if (dot < 0) {
              dot = 0;
            } else if (dot > len2) {
              dot = len2;
            }
            qx = px - dx * dot / len2;
            qy = py - dy * dot / len2;
          }
          if (qx * qx + qy * qy <= limit) {
            ++hits;
          }
        }
      }
      if (hits != 0) {
        RoboGPUBlendPixel(gpu, x, y, color, static_cast<RoboU8>(hits * 255u / 16u));
      }
    }
  }
  RoboDamageAdd(&gpu->damage, minX, minY, maxX - minX + 1, maxY - minY + 1);
}

static void RoboIconButtonMark(RoboGPU* gpu, RoboI32 left, RoboI32 top, RoboI32 mark, RoboBool check, RoboU32 color) {
  RoboI32 stroke = mark * 2 / 24;
  if (stroke < 2) {
    stroke = 2;
  }
  if (check) {
    const RoboI32 x0 = left + RoboIconButtonMap(22, mark);
    const RoboI32 y0 = top + RoboIconButtonMap(50, mark);
    const RoboI32 x1 = left + RoboIconButtonMap(40, mark);
    const RoboI32 y1 = top + RoboIconButtonMap(68, mark);
    const RoboI32 x2 = left + RoboIconButtonMap(74, mark);
    const RoboI32 y2 = top + RoboIconButtonMap(30, mark);
    RoboIconButtonSegment(gpu, x0, y0, x1, y1, stroke, color);
    RoboIconButtonSegment(gpu, x1, y1, x2, y2, stroke, color);
    return;
  }
  const RoboI32 a = left + RoboIconButtonMap(24, mark);
  const RoboI32 b = top + RoboIconButtonMap(24, mark);
  const RoboI32 c = left + RoboIconButtonMap(72, mark);
  const RoboI32 d = top + RoboIconButtonMap(72, mark);
  RoboIconButtonSegment(gpu, a, b, c, d, stroke, color);
  RoboIconButtonSegment(gpu, c, b, a, d, stroke, color);
}

static void RoboIconButtonColors(const RoboIconButton* button, RoboU32* fill, RoboU32* border, RoboI32* borderPx,
                                 RoboU32* glyph) {
  const RoboBool enabled = (!button->disabled && button->status != 1) ? ROBO_TRUE : ROBO_FALSE;
  const RoboBool pressed = (enabled && button->pointer == 2) ? ROBO_TRUE : ROBO_FALSE;
  const RoboBool hover = (enabled && !pressed && (button->pointer == 1 || button->focused)) ? ROBO_TRUE : ROBO_FALSE;
  const RoboU32 text = RoboFoundationPlatformColor("text-primary");
  const RoboU32 onBrand = RoboFoundationPlatformColor("text-on-brand");
  const RoboU32 brand = RoboFoundationPlatformColor("brand-primary");
  const RoboU32 hoverFill = RoboFoundationPlatformColor("bg-hover");
  const RoboU32 activeFill = RoboFoundationPlatformColor("bg-active");
  *fill = 0;
  *border = 0;
  *borderPx = 0;
  *glyph = text;
  if (button->variant == 1) {
    *fill = RoboFoundationPlatformColor("bg-secondary");
    if (pressed) {
      *fill = activeFill;
    } else if (hover) {
      *fill = hoverFill;
    }
  } else if (button->variant == 2) {
    *border = RoboFoundationPlatformColor("border-default");
    *borderPx = 1;
    if (pressed) {
      *fill = activeFill;
      *border = RoboFoundationPlatformColor("text-primary");
    } else if (button->pointer == 1 && enabled) {
      *fill = hoverFill;
      *border = RoboFoundationPlatformColor("border-strong");
    } else if (button->focused) {
      *border = RoboFoundationPlatformColor("border-focus");
    }
  } else if (button->variant == 3) {
    *fill = brand;
    *glyph = onBrand;
    if (pressed) {
      *fill = RoboFoundationPlatformColor("brand-primary-active");
    } else if (hover) {
      *fill = RoboFoundationPlatformColor("brand-primary-hover");
    }
  } else if (button->variant == 4) {
    *fill = RoboFoundationPlatformColor("danger");
    *glyph = onBrand;
    if (pressed) {
      const RoboU32 red = ((*fill >> 16) & 0xFFu) * 78u / 100u;
      const RoboU32 green = ((*fill >> 8) & 0xFFu) * 78u / 100u;
      const RoboU32 blue = (*fill & 0xFFu) * 78u / 100u;
      *fill = 0xFF000000u | (red << 16) | (green << 8) | blue;
    } else if (hover) {
      *fill = RoboFoundationPlatformColor("danger-hover");
    }
  } else if (pressed) {
    *fill = activeFill;
  } else if (hover) {
    *fill = hoverFill;
  }
}

static RoboI32 RoboIconButtonIsqrt(RoboI32 value) {
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

static void RoboIconButtonPaint(RoboUIComponent* self, RoboGPU* gpu, RoboI32 absX, RoboI32 absY) {
  auto* button = reinterpret_cast<RoboIconButton*>(self);
  if (button == ROBO_NULL || gpu == ROBO_NULL || button->base.width <= 0) {
    return;
  }
  const RoboI32 diameter = button->base.width;
  RoboIconButtonFillCircle(gpu, absX, absY, diameter, RoboFoundationPlatformColor("bg-primary"), 255);
  const RoboU8 fade = button->disabled || button->status == 1 ? static_cast<RoboU8>(140) : static_cast<RoboU8>(255);
  RoboU32 fill = 0;
  RoboU32 border = 0;
  RoboI32 borderPx = 0;
  RoboU32 glyph = 0;
  RoboIconButtonColors(button, &fill, &border, &borderPx, &glyph);
  glyph = RoboIconButtonFade(glyph, fade);
  if (fill != 0) {
    RoboIconButtonFillCircle(gpu, absX, absY, diameter, RoboIconButtonFade(fill, fade), 255);
  }
  if (borderPx > 0) {
    RoboIconButtonStrokeCircle(gpu, absX, absY, diameter, RoboIconButtonFade(border, fade), 255);
  }
  if (button->focused) {
    const RoboI32 reach = 5;
    const RoboI32 gapQ = 4;
    const RoboI32 reachQ = reach * 4;
    const RoboI32 span = reachQ - gapQ;
    const RoboU32 color = RoboFoundationPlatformColor("brand-primary");
    const RoboI32 radius = diameter * 2;
    const RoboI32 center = (reach * 4) + radius;
    const RoboI32 box = diameter + reach * 2;
    for (RoboI32 row = 0; row < box; ++row) {
      for (RoboI32 col = 0; col < box; ++col) {
        RoboU32 sum = 0;
        for (RoboI32 sampleY = 0; sampleY < 4; ++sampleY) {
          for (RoboI32 sampleX = 0; sampleX < 4; ++sampleX) {
            const RoboI32 dx = col * 4 + sampleX - center;
            const RoboI32 dy = row * 4 + sampleY - center;
            const RoboI32 outside = RoboIconButtonIsqrt(dx * dx + dy * dy) - radius;
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
        RoboGPUBlendPixel(gpu, absX - reach + col, absY - reach + row, color,
                          static_cast<RoboU8>(sum * fade / (16u * 255u)));
      }
    }
    RoboDamageAdd(&gpu->damage, absX - reach, absY - reach, box, box);
  }
  const RoboI32 centerX = absX + diameter / 2;
  const RoboI32 centerY = absY + diameter / 2;
  if (button->status == 1) {
    const RoboI32 spin = button->spinPx > 0 ? button->spinPx : 16;
    RoboU32 track = RoboFoundationPlatformColor("text-tertiary");
    RoboU32 tick = RoboFoundationPlatformColor("brand-primary");
    if (button->variant == 3 || button->variant == 4) {
      track = RoboFoundationPlatformColor("text-on-brand");
      tick = track;
    }
    RoboButtonDrawSpinner(gpu, centerX, centerY, track, static_cast<RoboU8>(fade * 35 / 100), tick, fade, spin);
  } else if (button->status == 2 || button->status == 3) {
    const RoboI32 mark = button->icon.sizePx > 0 ? button->icon.sizePx : (button->size == 0 ? 16 : 18);
    RoboU32 color = button->status == 2 ? RoboFoundationPlatformColor("success") : RoboFoundationPlatformColor("danger");
    if (button->variant == 3 || button->variant == 4) {
      color = glyph;
    }
    RoboIconButtonMark(gpu, centerX - mark / 2, centerY - mark / 2, mark, button->status == 2 ? ROBO_TRUE : ROBO_FALSE,
                       color);
  } else if (button->icon.base.vtable != ROBO_NULL && button->icon.base.vtable->Paint != ROBO_NULL) {
    const RoboI32 icon = button->icon.sizePx > 0 ? button->icon.sizePx : 18;
    RoboIconSetSlotColor(&button->icon, "fill", glyph);
    RoboIconSetSlotColor(&button->icon, "stroke", glyph);
    button->icon.base.vtable->Paint(&button->icon.base, gpu, centerX - icon / 2, centerY - icon / 2);
  }
  if (button->badge[0] != 0) {
    RoboI32 textW = 0;
    RoboI32 textH = 0;
    RoboUITextMeasure(&button->badgeText, &textW, &textH);
    RoboI32 badgeW = textW + 8;
    if (badgeW < 18) {
      badgeW = 18;
    }
    const RoboI32 badgeH = 18;
    const RoboI32 badgeX = absX + diameter - badgeW - 2;
    const RoboI32 badgeY = absY + 2;
    RoboIconButtonFillCircle(gpu, badgeX, badgeY, badgeH, RoboFoundationPlatformColor("danger"), 255);
    if (badgeW > badgeH) {
      RoboGPUFillRect(gpu, badgeX + badgeH / 2, badgeY, badgeW - badgeH, badgeH,
                      RoboGPUPackColor(gpu, 255, 69, 58, 255));
      RoboIconButtonFillCircle(gpu, badgeX + badgeW - badgeH, badgeY, badgeH, RoboFoundationPlatformColor("danger"),
                               255);
    }
    button->badgeText.colorARGB = RoboFoundationPlatformColor("text-on-brand");
    for (RoboU16 span = 0; span < button->badgeText.spanCount; ++span) {
      button->badgeText.spans[span].colorARGB = button->badgeText.colorARGB;
    }
    if (button->badgeText.base.vtable != ROBO_NULL && button->badgeText.base.vtable->Paint != ROBO_NULL) {
      button->badgeText.base.vtable->Paint(&button->badgeText.base, gpu, badgeX + (badgeW - textW) / 2,
                                           badgeY + (badgeH - textH) / 2);
    }
  }
}

static void RoboIconButtonDestroy(RoboUIComponent* self) { (void)self; }
static void RoboIconButtonTickComponent(RoboUIComponent* self, RoboU32 deltaMs) {
  (void)self;
  (void)deltaMs;
}

static const RoboUIVtable gRoboIconButtonVtable = {RoboIconButtonDestroy, RoboIconButtonPaint, ROBO_NULL,
                                                  RoboIconButtonTickComponent};

void RoboIconButtonInit(RoboIconButton* button, const char* id) {
  if (button == ROBO_NULL) {
    return;
  }
  RoboMemorySet(button, 0, sizeof(RoboIconButton));
  RoboUIComponentInitBase(&button->base, &gRoboIconButtonVtable, RoboUINodeKind::IconButton, id);
  RoboIconInit(&button->icon, "icon-button", "");
  RoboUITextInit(&button->badgeText, "icon-button-badge");
  button->size = 1;
  button->actionMs = -1;
  button->successMs = 1600;
  button->base.width = 44;
  button->base.height = 44;
  button->base.layout.preferWidth = 44;
  button->base.layout.preferHeight = 44;
}

RoboUIComponent* RoboIconButtonAsComponent(RoboIconButton* button) {
  return button == ROBO_NULL ? ROBO_NULL : &button->base;
}

RoboIconButton* RoboIconButtonFromComponent(RoboUIComponent* component) {
  if (component == ROBO_NULL || component->kind != RoboUINodeKind::IconButton) {
    return ROBO_NULL;
  }
  return reinterpret_cast<RoboIconButton*>(component);
}

void RoboIconButtonSetIcon(RoboIconButton* button, const char* iconName) {
  if (button == ROBO_NULL) {
    return;
  }
  RoboIconButtonCopy(button->iconName, sizeof(button->iconName), iconName);
  RoboIconInit(&button->icon, "icon-button", button->iconName);
}

void RoboIconButtonSetVariant(RoboIconButton* button, const char* variant) {
  if (button == ROBO_NULL) {
    return;
  }
  button->variant = 0;
  if (RoboIconButtonEq(variant, "subtle")) {
    button->variant = 1;
  } else if (RoboIconButtonEq(variant, "outline")) {
    button->variant = 2;
  } else if (RoboIconButtonEq(variant, "primary")) {
    button->variant = 3;
  } else if (RoboIconButtonEq(variant, "danger") || RoboIconButtonEq(variant, "destructive")) {
    button->variant = 4;
  }
}

void RoboIconButtonSetSize(RoboIconButton* button, const char* size) {
  if (button == ROBO_NULL) {
    return;
  }
  button->size = (RoboIconButtonEq(size, "sm") || RoboIconButtonEq(size, "small")) ? static_cast<RoboU8>(0)
                                                                                   : static_cast<RoboU8>(1);
}

void RoboIconButtonSetStatus(RoboIconButton* button, const char* status, RoboBool controlled) {
  if (button == ROBO_NULL) {
    return;
  }
  button->controlled = controlled;
  button->status = 0;
  if (RoboIconButtonEq(status, "loading")) {
    button->status = 1;
  } else if (RoboIconButtonEq(status, "success")) {
    button->status = 2;
  } else if (RoboIconButtonEq(status, "error")) {
    button->status = 3;
  }
}

void RoboIconButtonSetBadge(RoboIconButton* button, const char* badge) {
  if (button == ROBO_NULL) {
    return;
  }
  if (badge == ROBO_NULL || badge[0] == 0 || RoboIconButtonEq(badge, "0")) {
    button->badge[0] = 0;
    return;
  }
  RoboBool digits = ROBO_TRUE;
  int value = 0;
  for (const char* cursor = badge; *cursor != 0; ++cursor) {
    if (*cursor < '0' || *cursor > '9') {
      digits = ROBO_FALSE;
      break;
    }
    value = value * 10 + (*cursor - '0');
  }
  if (!digits) {
    RoboIconButtonCopy(button->badge, sizeof(button->badge), badge);
    return;
  }
  if (value <= 0) {
    button->badge[0] = 0;
    return;
  }
  if (value > 9) {
    RoboIconButtonCopy(button->badge, sizeof(button->badge), "9+");
    return;
  }
  button->badge[0] = static_cast<char>('0' + value);
  button->badge[1] = 0;
}

void RoboIconButtonSetAction(RoboIconButton* button, const char* action, int successMs) {
  if (button == ROBO_NULL) {
    return;
  }
  button->successMs = successMs > 0 ? successMs : 1600;
  button->actionFail = ROBO_FALSE;
  button->actionMs = -1;
  if (action == ROBO_NULL || action[0] == 0 || button->controlled) {
    return;
  }
  if (RoboIconButtonEq(action, "error")) {
    button->actionFail = ROBO_TRUE;
    button->actionMs = 0;
    return;
  }
  int wait = 0;
  for (const char* cursor = action; *cursor >= '0' && *cursor <= '9'; ++cursor) {
    wait = wait * 10 + (*cursor - '0');
  }
  button->actionMs = wait;
}

void RoboIconButtonSetDisabled(RoboIconButton* button, RoboBool disabled) {
  if (button != ROBO_NULL) {
    button->disabled = disabled;
  }
}

void RoboIconButtonSetPointer(RoboIconButton* button, RoboU8 pointer) {
  if (button != ROBO_NULL) {
    button->pointer = pointer > 2 ? static_cast<RoboU8>(0) : pointer;
  }
}

void RoboIconButtonSetFocused(RoboIconButton* button, RoboBool focused) {
  if (button != ROBO_NULL) {
    button->focused = focused;
  }
}

void RoboIconButtonSync(RoboIconButton* button, RoboUIContext* ctx, RoboU32 dpi) {
  if (button == ROBO_NULL) {
    return;
  }
  const RoboI32 diameter = RoboIconButtonScale(button->size == 0 ? 32 : 44, dpi);
  const RoboI32 glyph = RoboIconButtonScale(button->size == 0 ? 16 : 18, dpi);
  button->base.width = diameter;
  button->base.height = diameter;
  button->base.layout.preferWidth = diameter;
  button->base.layout.preferHeight = diameter;
  button->spinPx = RoboIconButtonScale(button->size == 0 ? 14 : 16, dpi);
  RoboIconSetSize(&button->icon, glyph);
  if (ctx != ROBO_NULL) {
    RoboIconResolve(&button->icon, ctx);
    RoboUITextSetFontName(&button->badgeText, ctx, "Inter-Medium");
  }
  button->badgeText.sizePx = static_cast<RoboU16>(RoboIconButtonScale(10, dpi));
  button->badgeText.leadingMilli = 1000;
  button->badgeText.trackingMilli = 0;
  button->badgeText.maxWidth = 64;
  button->badgeText.wrap = RoboTextWrap::None;
  RoboUITextSetContent(&button->badgeText, button->badge);
}

void RoboIconButtonPress(RoboIconButton* button, RoboU32 nowMs) {
  if (button == ROBO_NULL || button->controlled || button->disabled || button->status != 0 || button->actionMs < 0) {
    return;
  }
  button->status = 1;
  button->phaseNext = button->actionFail ? static_cast<RoboU8>(3) : static_cast<RoboU8>(2);
  button->phaseUntil = nowMs + static_cast<RoboU32>(button->actionMs);
}

RoboBool RoboIconButtonTick(RoboIconButton* button, RoboU32 nowMs) {
  if (button == ROBO_NULL || button->phaseUntil == 0 || nowMs < button->phaseUntil) {
    return ROBO_FALSE;
  }
  if (button->status == 1) {
    button->status = button->phaseNext;
    button->phaseNext = 0;
    button->phaseUntil = nowMs + static_cast<RoboU32>(button->successMs > 0 ? button->successMs : 1600);
    return ROBO_TRUE;
  }
  button->status = 0;
  button->phaseUntil = 0;
  return ROBO_TRUE;
}

void RoboIconButtonPaintSpin(RoboIconButton* button, RoboGPU* gpu, RoboI32 absX, RoboI32 absY) {
  if (button == ROBO_NULL || gpu == ROBO_NULL || button->status != 1 || button->base.width <= 0) {
    return;
  }
  const RoboI32 diameter = button->base.width;
  const RoboI32 centerX = absX + diameter / 2;
  const RoboI32 centerY = absY + diameter / 2;
  const RoboI32 spin = button->spinPx > 0 ? button->spinPx : 16;
  const RoboU8 fade = button->disabled || button->status == 1 ? static_cast<RoboU8>(140) : static_cast<RoboU8>(255);
  RoboU32 fill = 0;
  RoboU32 border = 0;
  RoboI32 borderPx = 0;
  RoboU32 glyph = 0;
  RoboIconButtonColors(button, &fill, &border, &borderPx, &glyph);
  (void)glyph;
  RoboIconButtonFillCircle(gpu, absX, absY, diameter, RoboFoundationPlatformColor("bg-primary"), 255);
  if (fill != 0) {
    RoboIconButtonFillCircle(gpu, absX, absY, diameter, RoboIconButtonFade(fill, fade), 255);
  }
  if (borderPx > 0) {
    RoboIconButtonStrokeCircle(gpu, absX, absY, diameter, RoboIconButtonFade(border, fade), 255);
  }
  RoboU32 track = RoboFoundationPlatformColor("text-tertiary");
  RoboU32 tick = RoboFoundationPlatformColor("brand-primary");
  if (button->variant == 3 || button->variant == 4) {
    track = RoboFoundationPlatformColor("text-on-brand");
    tick = track;
  }
  const RoboU8 spinFade = button->disabled ? static_cast<RoboU8>(140) : static_cast<RoboU8>(255);
  RoboButtonDrawSpinner(gpu, centerX, centerY, track, static_cast<RoboU8>(spinFade * 35 / 100), tick, spinFade, spin);
}
