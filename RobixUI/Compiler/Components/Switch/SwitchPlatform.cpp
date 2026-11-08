#include "SwitchPlatform.hpp"

#include "../../Foundation/FoundationPlatform.hpp"
#include "../Button/ButtonPlatform.hpp"

static RoboBool RoboSwitchEq(const char* a, const char* b) {
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

static void RoboSwitchCopy(char* dst, RoboSize dstSize, const char* src) {
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

static RoboI32 RoboSwitchScale(RoboI32 designPx, RoboU32 dpi) {
  const RoboU32 used = dpi == 0 ? 96u : dpi;
  return static_cast<RoboI32>(static_cast<RoboU32>(designPx) * used / 96u);
}

static RoboI32 RoboSwitchIsqrt(RoboI32 value) {
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

static RoboBool RoboSwitchInPill(RoboI32 localX, RoboI32 localY, RoboI32 width, RoboI32 height) {
  if (localX < 0 || localY < 0 || localX >= width || localY >= height || height < 2) {
    return ROBO_FALSE;
  }
  const RoboI32 radius = height / 2;
  const RoboI32 centerY = localY - radius;
  const RoboI32 distanceY = centerY < 0 ? -centerY : centerY;
  if (distanceY > radius) {
    return ROBO_FALSE;
  }
  const RoboI32 span = RoboSwitchIsqrt(radius * radius - distanceY * distanceY);
  return (localX >= radius - span && localX < width - (radius - span)) ? ROBO_TRUE : ROBO_FALSE;
}

static RoboU8 RoboSwitchPillCoverage(RoboI32 localX, RoboI32 localY, RoboI32 width, RoboI32 height) {
  if (width <= 0 || height <= 0) {
    return 0;
  }
  const RoboI32 width4 = width * 4;
  const RoboI32 x0 = localX * 4;
  RoboU32 hits = 0;
  for (RoboI32 sampleY = 0; sampleY < 4; ++sampleY) {
    for (RoboI32 sampleX = 0; sampleX < 4; ++sampleX) {
      if (RoboSwitchInPill(x0 + sampleX, localY * 4 + sampleY, width4, height * 4)) {
        ++hits;
      }
    }
  }
  return static_cast<RoboU8>(hits * 255u / 16u);
}

static void RoboSwitchFillPill(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height, RoboU32 color,
                               RoboU8 coverage) {
  if (gpu == ROBO_NULL || width <= 0 || height <= 0 || coverage == 0) {
    return;
  }
  for (RoboI32 row = 0; row < height; ++row) {
    for (RoboI32 col = 0; col < width; ++col) {
      const RoboU8 edge = RoboSwitchPillCoverage(col, row, width, height);
      if (edge == 0) {
        continue;
      }
      RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>(edge * coverage / 255));
    }
  }
  RoboDamageAdd(&gpu->damage, x, y, width, height);
}

static void RoboSwitchStrokePill(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height, RoboI32 thickness,
                                 RoboU32 color, RoboU8 coverage) {
  if (gpu == ROBO_NULL || thickness <= 0 || coverage == 0 || width <= 0 || height <= 0) {
    return;
  }
  const RoboI32 outerW = width + thickness * 2;
  const RoboI32 outerH = height + thickness * 2;
  for (RoboI32 row = 0; row < outerH; ++row) {
    for (RoboI32 col = 0; col < outerW; ++col) {
      const RoboU32 outer = RoboSwitchPillCoverage(col, row, outerW, outerH);
      const RoboU32 inner = RoboSwitchPillCoverage(col - thickness, row - thickness, width, height);
      const RoboU32 edge = outer > inner ? outer - inner : 0;
      if (edge != 0) {
        RoboGPUBlendPixel(gpu, x - thickness + col, y - thickness + row, color,
                          static_cast<RoboU8>(edge * coverage / 255u));
      }
    }
  }
  RoboDamageAdd(&gpu->damage, x - thickness, y - thickness, outerW, outerH);
}

static int RoboSwitchEase(int from, int to, RoboU32 elapsed) {
  if (elapsed >= 200u) {
    return to;
  }
  const int t = static_cast<int>(elapsed * 1000u / 200u);
  const int inv = 1000 - t;
  const int ease = 1000 - (inv * inv * inv) / 1000000;
  return from + (to - from) * ease / 1000;
}

static void RoboSwitchTrackBox(const RoboSwitch* control, RoboI32 absX, RoboI32 absY, RoboI32* outX, RoboI32* outY) {
  RoboI32 x = absX;
  RoboI32 y = absY;
  if (control->item) {
    x = absX + control->base.width - control->trackW;
    y = absY + (control->base.height - control->trackH) / 2;
  }
  *outX = x;
  *outY = y;
}

static void RoboSwitchThumbBox(const RoboSwitch* control, RoboI32 trackX, RoboI32 trackY, RoboI32* outX, RoboI32* outY,
                               RoboI32* outW) {
  const RoboI32 inset = control->trackH > 4 ? 2 : 1;
  const RoboI32 travel = control->thumb * control->travelPx / 1000;
  const RoboI32 drawn = control->thumbPx + control->thumbPx * 18 / 100 * control->pressAmount / 1000;
  RoboI32 left = trackX + inset + travel;
  if (control->checked) {
    left = trackX + inset + travel + control->thumbPx - drawn;
  }
  *outX = left;
  *outY = trackY + inset;
  *outW = drawn;
}

static void RoboSwitchPaint(RoboUIComponent* self, RoboGPU* gpu, RoboI32 absX, RoboI32 absY) {
  auto* control = reinterpret_cast<RoboSwitch*>(self);
  if (control == ROBO_NULL || gpu == ROBO_NULL || control->base.width <= 0 || control->base.height <= 0) {
    return;
  }
  if (control->item && control->divider && control->base.width > 0) {
    const RoboU32 line = RoboFoundationPlatformColor("border-default");
    RoboGPUFillRect(gpu, absX, absY, control->base.width, 1,
                    RoboGPUPackColor(gpu, static_cast<RoboU8>((line >> 16) & 0xFF),
                                     static_cast<RoboU8>((line >> 8) & 0xFF), static_cast<RoboU8>(line & 0xFF), 255));
  }
  if (control->item) {
    RoboI32 titleW = 0;
    RoboI32 titleH = 0;
    RoboI32 descW = 0;
    RoboI32 descH = 0;
    RoboUITextMeasure(&control->title, &titleW, &titleH);
    if (control->descriptionText[0] != 0) {
      RoboUITextMeasure(&control->description, &descW, &descH);
    }
    const RoboI32 gap = control->title.sizePx > 8 ? control->title.sizePx / 8 : 2;
    const RoboI32 block = titleH + (descH > 0 ? gap + descH : 0);
    RoboI32 textY = absY + (control->base.height - block) / 2;
    if (control->title.base.vtable != ROBO_NULL && control->title.base.vtable->Paint != ROBO_NULL && titleH > 0) {
      control->title.base.vtable->Paint(&control->title.base, gpu, absX, textY);
      textY += titleH + (descH > 0 ? gap : 0);
    }
    if (descH > 0 && control->description.base.vtable != ROBO_NULL &&
        control->description.base.vtable->Paint != ROBO_NULL) {
      control->description.base.vtable->Paint(&control->description.base, gpu, absX, textY);
    }
  }
  RoboI32 trackX = absX;
  RoboI32 trackY = absY;
  RoboSwitchTrackBox(control, absX, absY, &trackX, &trackY);
  const RoboU8 fade = control->disabled ? static_cast<RoboU8>(140) : static_cast<RoboU8>(255);
  const RoboBool on = control->checked;
  RoboU32 fill = 0;
  RoboU32 border = 0;
  RoboI32 borderPx = 0;
  if (on && control->variant == 4) {
    fill = RoboFoundationPlatformColor("danger");
  } else if (on) {
    fill = RoboFoundationPlatformColor("brand-primary");
    if (control->variant == 2) {
      border = fill;
      borderPx = 1;
    }
  } else if (control->variant == 1) {
    fill = RoboFoundationPlatformColor("bg-tertiary");
  } else if (control->variant == 2) {
    border = RoboFoundationPlatformColor("border-default");
    borderPx = 1;
  } else if (control->variant == 3) {
    fill = 0;
  } else {
    fill = RoboFoundationPlatformColor("bg-active");
  }
  if (fill != 0) {
    RoboSwitchFillPill(gpu, trackX, trackY, control->trackW, control->trackH, fill, fade);
  }
  if (borderPx > 0 && border != 0) {
    RoboSwitchStrokePill(gpu, trackX, trackY, control->trackW, control->trackH, borderPx, border, fade);
  }
  RoboI32 thumbX = 0;
  RoboI32 thumbY = 0;
  RoboI32 thumbW = 0;
  RoboSwitchThumbBox(control, trackX, trackY, &thumbX, &thumbY, &thumbW);
  const RoboU32 thumb = (!on && control->variant == 3) ? RoboFoundationPlatformColor("bg-elevated")
                                                       : RoboFoundationPlatformColor("text-on-brand");
  RoboSwitchFillPill(gpu, thumbX, thumbY, thumbW, control->thumbPx, thumb, fade);
  if (!on && control->variant == 3) {
    RoboSwitchStrokePill(gpu, thumbX, thumbY, thumbW, control->thumbPx, 1, RoboFoundationPlatformColor("border-default"),
                         fade);
  }
  if (control->focused) {
    const RoboI32 gap = control->trackH > 8 ? 2 : 1;
    RoboSwitchStrokePill(gpu, trackX - gap, trackY - gap, control->trackW + gap * 2, control->trackH + gap * 2, 2,
                         RoboFoundationPlatformColor("brand-primary"), 255);
  }
}

static void RoboSwitchDestroy(RoboUIComponent* self) { (void)self; }
static void RoboSwitchIdle(RoboUIComponent* self, RoboU32 deltaMs) {
  (void)self;
  (void)deltaMs;
}

static const RoboUIVtable gRoboSwitchVtable = {RoboSwitchDestroy, RoboSwitchPaint, ROBO_NULL, RoboSwitchIdle};

void RoboSwitchInit(RoboSwitch* control, const char* id) {
  if (control == ROBO_NULL) {
    return;
  }
  RoboMemorySet(control, 0, sizeof(RoboSwitch));
  RoboUIComponentInitBase(&control->base, &gRoboSwitchVtable, RoboUINodeKind::Switch, id);
  RoboUITextInit(&control->title, "switch-title");
  RoboUITextInit(&control->description, "switch-description");
  control->size = 1;
  control->trackW = 51;
  control->trackH = 31;
  control->thumbPx = 27;
  control->travelPx = 20;
  control->base.width = 51;
  control->base.height = 31;
  control->base.layout.preferWidth = 51;
  control->base.layout.preferHeight = 31;
}

RoboUIComponent* RoboSwitchAsComponent(RoboSwitch* control) {
  return control == ROBO_NULL ? ROBO_NULL : &control->base;
}

RoboSwitch* RoboSwitchFromComponent(RoboUIComponent* component) {
  if (component == ROBO_NULL || component->kind != RoboUINodeKind::Switch) {
    return ROBO_NULL;
  }
  return reinterpret_cast<RoboSwitch*>(component);
}

void RoboSwitchSetVariant(RoboSwitch* control, const char* variant) {
  if (control == ROBO_NULL) {
    return;
  }
  if (RoboSwitchEq(variant, "subtle")) {
    control->variant = 1;
  } else if (RoboSwitchEq(variant, "outline")) {
    control->variant = 2;
  } else if (RoboSwitchEq(variant, "ghost")) {
    control->variant = 3;
  } else if (RoboSwitchEq(variant, "danger")) {
    control->variant = 4;
  } else {
    control->variant = 0;
  }
}

void RoboSwitchSetSize(RoboSwitch* control, const char* size) {
  if (control == ROBO_NULL) {
    return;
  }
  if (RoboSwitchEq(size, "sm") || RoboSwitchEq(size, "small")) {
    control->size = 0;
  } else if (RoboSwitchEq(size, "lg") || RoboSwitchEq(size, "large")) {
    control->size = 2;
  } else {
    control->size = 1;
  }
}

void RoboSwitchSetChecked(RoboSwitch* control, RoboBool checked) {
  if (control == ROBO_NULL) {
    return;
  }
  control->checked = checked;
  control->thumb = checked ? 1000 : 0;
  control->thumbFrom = control->thumb;
  control->thumbTo = control->thumb;
  control->animating = ROBO_FALSE;
}

void RoboSwitchSetCopy(RoboSwitch* control, const char* title, const char* description) {
  if (control == ROBO_NULL) {
    return;
  }
  RoboSwitchCopy(control->titleText, sizeof(control->titleText), title);
  RoboSwitchCopy(control->descriptionText, sizeof(control->descriptionText), description);
}

void RoboSwitchSetFlags(RoboSwitch* control, RoboBool disabled, RoboBool loading, RoboBool item, RoboBool divider) {
  if (control == ROBO_NULL) {
    return;
  }
  control->disabled = disabled;
  control->loading = loading;
  control->item = item;
  control->divider = divider;
}

void RoboSwitchSync(RoboSwitch* control, RoboUIContext* ctx, RoboU32 dpi) {
  if (control == ROBO_NULL) {
    return;
  }
  const RoboI32 trackW = RoboSwitchScale(control->size == 0 ? 44 : (control->size == 2 ? 60 : 51), dpi);
  const RoboI32 trackH = RoboSwitchScale(control->size == 0 ? 26 : (control->size == 2 ? 36 : 31), dpi);
  const RoboI32 thumb = RoboSwitchScale(control->size == 0 ? 22 : (control->size == 2 ? 32 : 27), dpi);
  const RoboI32 travel = RoboSwitchScale(control->size == 0 ? 18 : (control->size == 2 ? 24 : 20), dpi);
  control->trackW = trackW > 0 ? trackW : 1;
  control->trackH = trackH > 0 ? trackH : 1;
  control->thumbPx = thumb > 0 ? thumb : 1;
  control->travelPx = travel > 0 ? travel : 1;
  if (!control->item) {
    control->base.width = control->trackW;
    control->base.height = control->trackH;
    control->base.layout.preferWidth = control->trackW;
    control->base.layout.preferHeight = control->trackH;
    control->base.layout.widthPercent = 0;
    return;
  }
  const RoboI32 titleSize = RoboSwitchScale(17, dpi);
  const RoboI32 descSize = RoboSwitchScale(12, dpi);
  const RoboI32 pad = RoboSwitchScale(12, dpi);
  const RoboI32 copyGap = RoboSwitchScale(2, dpi);
  const RoboI32 side = RoboSwitchScale(16, dpi);
  const RoboI32 minH = RoboSwitchScale(56, dpi);
  control->title.sizePx = static_cast<RoboU16>(titleSize > 0 ? titleSize : 17);
  control->title.leadingMilli = 1350;
  control->title.wrap = RoboTextWrap::None;
  control->title.maxWidth = 4096;
  RoboUITextSetColor(&control->title, RoboFoundationPlatformColor("text-primary"));
  control->description.sizePx = static_cast<RoboU16>(descSize > 0 ? descSize : 12);
  control->description.leadingMilli = 1330;
  control->description.wrap = RoboTextWrap::None;
  control->description.maxWidth = 4096;
  RoboUITextSetColor(&control->description, RoboFoundationPlatformColor("text-tertiary"));
  if (ctx != ROBO_NULL) {
    RoboUITextSetFontName(&control->title, ctx, "Inter-Regular");
    RoboUITextSetFontName(&control->description, ctx, "Inter-Regular");
  }
  RoboUITextSetContent(&control->title, control->titleText);
  RoboUITextSetContent(&control->description, control->descriptionText);
  RoboI32 titleW = 0;
  RoboI32 titleH = 0;
  RoboI32 descW = 0;
  RoboI32 descH = 0;
  RoboUITextMeasure(&control->title, &titleW, &titleH);
  if (control->descriptionText[0] != 0) {
    RoboUITextMeasure(&control->description, &descW, &descH);
  }
  const RoboI32 copyW = titleW > descW ? titleW : descW;
  const RoboI32 block = titleH + (descH > 0 ? copyGap + descH : 0);
  RoboI32 height = pad * 2 + block;
  if (height < minH) {
    height = minH;
  }
  if (height < control->trackH) {
    height = control->trackH;
  }
  control->base.height = height;
  control->base.layout.preferHeight = height;
  control->base.layout.preferWidth = copyW + side + control->trackW;
  control->base.layout.widthPercent = 100;
  control->base.width = control->base.layout.preferWidth;
}

void RoboSwitchSetPressed(RoboSwitch* control, RoboBool pressed, RoboU32 nowMs) {
  if (control == ROBO_NULL) {
    return;
  }
  const RoboI32 target = pressed ? 1000 : 0;
  if (control->pressed == pressed && control->pressTo == target && !control->pressing) {
    return;
  }
  control->pressed = pressed;
  control->pressFrom = control->pressAmount;
  control->pressTo = target;
  control->pressStart = nowMs;
  control->pressing = control->pressFrom != control->pressTo ? ROBO_TRUE : ROBO_FALSE;
  if (!control->pressing) {
    control->pressAmount = target;
  }
}

void RoboSwitchSetFocused(RoboSwitch* control, RoboBool focused) {
  if (control != ROBO_NULL) {
    control->focused = focused;
  }
}

void RoboSwitchToggle(RoboSwitch* control, RoboU32 nowMs) {
  if (control == ROBO_NULL || control->disabled || control->loading) {
    return;
  }
  control->checked = control->checked ? ROBO_FALSE : ROBO_TRUE;
  control->thumbFrom = control->thumb;
  control->thumbTo = control->checked ? 1000 : 0;
  control->animStart = nowMs;
  control->animating = control->thumbFrom != control->thumbTo ? ROBO_TRUE : ROBO_FALSE;
  if (!control->animating) {
    control->thumb = control->thumbTo;
  }
}

RoboBool RoboSwitchTick(RoboSwitch* control, RoboU32 nowMs) {
  if (control == ROBO_NULL) {
    return ROBO_FALSE;
  }
  RoboBool changed = ROBO_FALSE;
  if (control->animating) {
    const RoboU32 elapsed = nowMs - control->animStart;
    const RoboI32 next = RoboSwitchEase(control->thumbFrom, control->thumbTo, elapsed);
    if (next != control->thumb) {
      changed = ROBO_TRUE;
    }
    control->thumb = next;
    if (elapsed >= 200u) {
      control->thumb = control->thumbTo;
      control->animating = ROBO_FALSE;
    }
    changed = ROBO_TRUE;
  }
  if (control->pressing) {
    const RoboU32 elapsed = nowMs - control->pressStart;
    const RoboI32 next = RoboSwitchEase(control->pressFrom, control->pressTo, elapsed);
    if (next != control->pressAmount) {
      changed = ROBO_TRUE;
    }
    control->pressAmount = next;
    if (elapsed >= 200u) {
      control->pressAmount = control->pressTo;
      control->pressing = ROBO_FALSE;
    }
    changed = ROBO_TRUE;
  }
  return changed;
}

void RoboSwitchPaintSpin(RoboSwitch* control, RoboGPU* gpu, RoboI32 absX, RoboI32 absY) {
  if (control == ROBO_NULL || gpu == ROBO_NULL || !control->loading || control->trackW <= 0) {
    return;
  }
  RoboI32 trackX = absX;
  RoboI32 trackY = absY;
  RoboSwitchTrackBox(control, absX, absY, &trackX, &trackY);
  RoboI32 thumbX = 0;
  RoboI32 thumbY = 0;
  RoboI32 thumbW = 0;
  RoboSwitchThumbBox(control, trackX, trackY, &thumbX, &thumbY, &thumbW);
  const RoboI32 size = control->size == 0 ? control->thumbPx * 12 / 22 : control->thumbPx * 14 / 27;
  if (size < 4) {
    return;
  }
  const RoboI32 centerX = thumbX + thumbW / 2;
  const RoboI32 centerY = thumbY + control->thumbPx / 2;
  RoboButtonDrawSpinner(gpu, centerX, centerY, RoboFoundationPlatformColor("text-tertiary"), 89,
                        RoboFoundationPlatformColor("brand-primary"), 255, size);
}
