#include "RoboCursor.hpp"

/* Soft cursor: stage a scanline then imageblit (RefCode fbdev softcursor.c). */
constexpr RoboI32 ROBO_CURSOR_ROW_STAGE_MAX = 512;
static RoboU32 gRoboCursorRowStage[ROBO_CURSOR_ROW_STAGE_MAX];

static void RoboCursorReloadImage(RoboCursorState* state) {
  if (state == ROBO_NULL || state->pack == ROBO_NULL) {
    return;
  }
  const RoboCursorEntry* entry = RoboCursorPackFind(state->pack, state->theme, state->kind);
  if (entry == ROBO_NULL) {
    entry = RoboCursorPackFind(state->pack, state->theme, RoboCursorKind::Default);
  }
  if (entry == ROBO_NULL) {
    entry = RoboCursorPackFind(state->pack, ROBO_DEFAULT_CURSOR_THEME, RoboCursorKind::Default);
  }
  if (entry == ROBO_NULL) {
    state->image = RoboCursorImageView{};
    return;
  }
  state->image = RoboCursorPackMakeView(state->pack, entry);
}

static RoboU32 RoboCursorShiftFromMask(RoboU32 mask) {
  if (mask == 0) {
    return 0;
  }
  RoboU32 shift = 0;
  while ((mask & 1u) == 0) {
    mask >>= 1;
    ++shift;
  }
  return shift;
}

static RoboU32 RoboCursorPackPixel(const RoboFrameBufferInfo* fbInfo, RoboU8 r, RoboU8 g, RoboU8 b) {
  RoboU32 redMask = fbInfo != ROBO_NULL ? fbInfo->redMask : 0;
  RoboU32 greenMask = fbInfo != ROBO_NULL ? fbInfo->greenMask : 0;
  RoboU32 blueMask = fbInfo != ROBO_NULL ? fbInfo->blueMask : 0;
  if (redMask == 0 && greenMask == 0 && blueMask == 0) {
    redMask = 0x00FF0000;
    greenMask = 0x0000FF00;
    blueMask = 0x000000FF;
  }
  const RoboU32 rs = RoboCursorShiftFromMask(redMask);
  const RoboU32 gs = RoboCursorShiftFromMask(greenMask);
  const RoboU32 bs = RoboCursorShiftFromMask(blueMask);
  return ((static_cast<RoboU32>(r) << rs) & redMask) | ((static_cast<RoboU32>(g) << gs) & greenMask) |
         ((static_cast<RoboU32>(b) << bs) & blueMask);
}

static void RoboCursorUnpackPixel(const RoboFrameBufferInfo* fbInfo, RoboU32 pixel, RoboU8* r, RoboU8* g, RoboU8* b) {
  RoboU32 redMask = fbInfo != ROBO_NULL ? fbInfo->redMask : 0;
  RoboU32 greenMask = fbInfo != ROBO_NULL ? fbInfo->greenMask : 0;
  RoboU32 blueMask = fbInfo != ROBO_NULL ? fbInfo->blueMask : 0;
  if (redMask == 0 && greenMask == 0 && blueMask == 0) {
    redMask = 0x00FF0000;
    greenMask = 0x0000FF00;
    blueMask = 0x000000FF;
  }
  const RoboU32 rs = RoboCursorShiftFromMask(redMask);
  const RoboU32 gs = RoboCursorShiftFromMask(greenMask);
  const RoboU32 bs = RoboCursorShiftFromMask(blueMask);
  *r = static_cast<RoboU8>((pixel & redMask) >> rs);
  *g = static_cast<RoboU8>((pixel & greenMask) >> gs);
  *b = static_cast<RoboU8>((pixel & blueMask) >> bs);
}

static RoboCursorTheme RoboCursorClampTheme(RoboU32 raw) {
  if (raw == static_cast<RoboU32>(RoboCursorTheme::Light)) {
    return RoboCursorTheme::Light;
  }
  return RoboCursorTheme::Dark;
}

static RoboI32 RoboCursorMin(RoboI32 a, RoboI32 b) {
  return a < b ? a : b;
}

static RoboI32 RoboCursorMax(RoboI32 a, RoboI32 b) {
  return a > b ? a : b;
}

static void RoboCursorClipRect(RoboI32* left, RoboI32* top, RoboI32* right, RoboI32* bottom, RoboI32 screenW,
                               RoboI32 screenH) {
  if (*left < 0) {
    *left = 0;
  }
  if (*top < 0) {
    *top = 0;
  }
  if (*right > screenW) {
    *right = screenW;
  }
  if (*bottom > screenH) {
    *bottom = screenH;
  }
}

static RoboU32 RoboCursorBlendOver(const RoboFrameBufferInfo* fbInfo, RoboU32 deskPixel, RoboU8 r, RoboU8 g, RoboU8 b,
                                   RoboU8 a) {
  if (a == 255) {
    return RoboCursorPackPixel(fbInfo, r, g, b);
  }
  if (a == 0) {
    return deskPixel;
  }
  RoboU8 dr = 0, dg = 0, db = 0;
  RoboCursorUnpackPixel(fbInfo, deskPixel, &dr, &dg, &db);
  return RoboCursorPackPixel(fbInfo, static_cast<RoboU8>((r * a + dr * (255 - a)) / 255),
                             static_cast<RoboU8>((g * a + dg * (255 - a)) / 255),
                             static_cast<RoboU8>((b * a + db * (255 - a)) / 255));
}

/*
 * Blit span:
 * withCursor: desktop + sprite blend into VRAM front buffer.
 * withoutCursor: direct fast memcpy from desktopBuffer to front buffer.
 */
static void RoboCursorBlitSpan(const RoboHandoff* handoff, const RoboU8* desktopBuffer, RoboI32 y, RoboI32 x0,
                               RoboI32 x1, RoboI32 spriteLeft, RoboI32 spriteTop, RoboI32 spriteW, RoboI32 spriteH,
                               const RoboU8* cursorPx, RoboU32 cursorStride, RoboBool withCursor) {
  if (x1 <= x0) {
    return;
  }
  const RoboI32 span = x1 - x0;
  auto* front = reinterpret_cast<RoboU8*>(handoff->frameBuffer.base);
  const RoboU32 pitch = handoff->frameBuffer.pixelsPerScanLine * 4;
  const RoboU32* deskRow =
      reinterpret_cast<const RoboU32*>(desktopBuffer + static_cast<RoboU64>(y) * pitch);
  RoboU8* frontDest = front + static_cast<RoboU64>(y) * pitch + static_cast<RoboU64>(x0) * 4ull;

  if (!withCursor) {
    /* Pure desktop restoration — fast copy without staging */
    RoboMemoryCopy(frontDest, deskRow + x0, static_cast<RoboSize>(span) * 4u);
    return;
  }

  if (span > ROBO_CURSOR_ROW_STAGE_MAX) {
    return;
  }

  const RoboI32 cy = y - spriteTop;
  const RoboBool rowInSprite = (cy >= 0 && cy < spriteH) ? ROBO_TRUE : ROBO_FALSE;
  const RoboU8* cursorRow =
      rowInSprite ? (cursorPx + static_cast<RoboU64>(cy) * cursorStride) : ROBO_NULL;

  for (RoboI32 x = x0; x < x1; ++x) {
    RoboU32 pixel = deskRow[x];
    if (rowInSprite) {
      const RoboI32 cx = x - spriteLeft;
      if (cx >= 0 && cx < spriteW) {
        const RoboU8 r = cursorRow[static_cast<RoboU32>(cx) * 4u + 0];
        const RoboU8 g = cursorRow[static_cast<RoboU32>(cx) * 4u + 1];
        const RoboU8 b = cursorRow[static_cast<RoboU32>(cx) * 4u + 2];
        const RoboU8 a = cursorRow[static_cast<RoboU32>(cx) * 4u + 3];
        pixel = RoboCursorBlendOver(&handoff->frameBuffer, pixel, r, g, b, a);
      }
    }
    gRoboCursorRowStage[x - x0] = pixel;
  }

  RoboMemoryCopy(frontDest, gRoboCursorRowStage, static_cast<RoboSize>(span) * 4u);
}

void RoboCursorInit(RoboCursorState* state, const void* pack, const RoboHandoff* handoff) {
  if (state == ROBO_NULL) {
    return;
  }
  RoboMemorySet(state, 0, sizeof(RoboCursorState));
  state->pack = pack;
  state->theme = ROBO_DEFAULT_CURSOR_THEME;
  state->kind = RoboCursorKind::Default;
  state->displaySize = ROBO_CURSOR_DISPLAY_SIZE;
  state->enabled = (pack != ROBO_NULL && RoboCursorPackValidate(pack, sizeof(RoboCursorPackHeader))) ? ROBO_TRUE
                                                                                                      : ROBO_FALSE;
  if (handoff != ROBO_NULL) {
    state->tipX = static_cast<RoboI32>(handoff->frameBuffer.width / 2);
    state->tipY = static_cast<RoboI32>(handoff->frameBuffer.height / 2);
    state->theme = RoboCursorClampTheme(handoff->cursorTheme);
  }
  RoboCursorReloadImage(state);
}

void RoboCursorSetTheme(RoboCursorState* state, RoboCursorTheme theme) {
  if (state == ROBO_NULL) {
    return;
  }
  state->theme = theme;
  RoboCursorReloadImage(state);
  state->imageChanged = ROBO_TRUE;
}

void RoboCursorSetKind(RoboCursorState* state, RoboCursorKind kind) {
  if (state == ROBO_NULL || state->kind == kind) {
    return;
  }
  state->kind = kind;
  RoboCursorReloadImage(state);
  state->imageChanged = ROBO_TRUE;
}

void RoboCursorMoveTip(RoboCursorState* state, RoboI32 tipX, RoboI32 tipY, RoboI32 screenW, RoboI32 screenH) {
  if (state == ROBO_NULL) {
    return;
  }
  if (tipX < 0) {
    tipX = 0;
  }
  if (tipY < 0) {
    tipY = 0;
  }
  if (screenW > 0 && tipX >= screenW) {
    tipX = screenW - 1;
  }
  if (screenH > 0 && tipY >= screenH) {
    tipY = screenH - 1;
  }
  state->tipX = tipX;
  state->tipY = tipY;
}

/*
 * Atomic soft cursor row blend for Present flushes (drm_fb_memcpy path).
 * When RoboGPUFlushRect copies pixels from desktop backbuffer to front scanout,
 * any row intersecting the active cursor will have the cursor sprite blended
 * directly into the scanline before writing to VRAM frontbuffer.
 * This guarantees the cursor is NEVER overwritten or erased by screen updates.
 */
void RoboCursorBlendRow(const void* cursorVoid, const RoboFrameBufferInfo* fbInfo,
                        const RoboU32* srcDeskRow, RoboU32* dstFrontRow,
                        RoboI32 y, RoboI32 x0, RoboI32 x1) {
  const auto* state = static_cast<const RoboCursorState*>(cursorVoid);
  const RoboI32 width = x1 - x0;
  if (width <= 0) {
    return;
  }
  if (state == ROBO_NULL || !state->enabled || state->image.pixels == ROBO_NULL) {
    RoboScanoutCopy(dstFrontRow, srcDeskRow, static_cast<RoboSize>(width) * 4u);
    return;
  }

  const RoboI32 spriteLeft = state->tipX - state->image.hotX;
  const RoboI32 spriteTop = state->tipY - state->image.hotY;
  const RoboI32 spriteW = static_cast<RoboI32>(state->image.width);
  const RoboI32 spriteH = static_cast<RoboI32>(state->image.height);

  /* Check vertical intersection with cursor */
  if (y < spriteTop || y >= spriteTop + spriteH) {
    RoboScanoutCopy(dstFrontRow, srcDeskRow, static_cast<RoboSize>(width) * 4u);
    return;
  }

  const RoboI32 spriteRight = spriteLeft + spriteW;
  const RoboI32 cx0 = RoboCursorMax(x0, spriteLeft);
  const RoboI32 cx1 = RoboCursorMin(x1, spriteRight);

  /* Check horizontal intersection with cursor */
  if (cx1 <= cx0) {
    RoboScanoutCopy(dstFrontRow, srcDeskRow, static_cast<RoboSize>(width) * 4u);
    return;
  }

  /* 1. Copy part of row before cursor intersection */
  if (cx0 > x0) {
    const RoboSize leading = static_cast<RoboSize>(cx0 - x0);
    RoboScanoutCopy(dstFrontRow, srcDeskRow, leading * 4u);
  }

  /* 2. Blend cursor intersection */
  const RoboI32 cy = y - spriteTop;
  const RoboU8* cursorRow = state->image.pixels + static_cast<RoboU64>(cy) * state->image.stride;
  const RoboI32 cursorSpan = cx1 - cx0;

  RoboU32 stage[128];
  RoboU32* pStage = stage;
  if (cursorSpan > 128) {
    pStage = gRoboCursorRowStage;
  }

  for (RoboI32 x = cx0; x < cx1; ++x) {
    const RoboI32 cx = x - spriteLeft;
    const RoboU8 r = cursorRow[static_cast<RoboU32>(cx) * 4u + 0];
    const RoboU8 g = cursorRow[static_cast<RoboU32>(cx) * 4u + 1];
    const RoboU8 b = cursorRow[static_cast<RoboU32>(cx) * 4u + 2];
    const RoboU8 a = cursorRow[static_cast<RoboU32>(cx) * 4u + 3];
    pStage[x - cx0] = RoboCursorBlendOver(fbInfo, srcDeskRow[x - x0], r, g, b, a);
  }
  RoboScanoutCopy(dstFrontRow + (cx0 - x0), pStage, static_cast<RoboSize>(cursorSpan) * 4u);

  /* 3. Copy part of row after cursor intersection */
  if (x1 > cx1) {
    const RoboSize trailing = static_cast<RoboSize>(x1 - cx1);
    RoboScanoutCopy(dstFrontRow + (cx1 - x0), srcDeskRow + (cx1 - x0), trailing * 4u);
  }
}

void RoboCursorMarkDamage(RoboCursorState* state, RoboGPU* gpu) {
  if (state == ROBO_NULL || gpu == ROBO_NULL || !state->enabled || state->image.pixels == ROBO_NULL) {
    return;
  }
  const RoboI32 drawW = static_cast<RoboI32>(state->image.width);
  const RoboI32 drawH = static_cast<RoboI32>(state->image.height);
  if (drawW <= 0 || drawH <= 0) {
    return;
  }
  const RoboI32 newLeft = state->tipX - state->image.hotX;
  const RoboI32 newTop = state->tipY - state->image.hotY;
  if (state->hasLast) {
    RoboGPUMarkDamage(gpu, state->lastLeft, state->lastTop, state->lastWidth, state->lastHeight);
  }
  RoboGPUMarkDamage(gpu, newLeft, newTop, drawW, drawH);
  state->lastLeft = newLeft;
  state->lastTop = newTop;
  state->lastWidth = drawW;
  state->lastHeight = drawH;
  state->hasLast = ROBO_TRUE;
  state->imageChanged = ROBO_FALSE;
}

/*
 * Soft mouse cursor composition:
 * 1. Draw NEW cursor first so the cursor is ALWAYS visible on screen.
 * 2. Restore OLD cursor pixels that lie outside the new cursor bounds.
 * Overlapping pixels are drawn once and never erased.
 * There is zero intermediate flicker, zero ghosting, and no blank frames.
 */
RoboBool RoboCursorCompose(RoboCursorState* state, const RoboHandoff* handoff, const RoboU8* desktopBuffer) {
  if (state == ROBO_NULL || handoff == ROBO_NULL || desktopBuffer == ROBO_NULL || !state->enabled ||
      state->image.pixels == ROBO_NULL) {
    return ROBO_FALSE;
  }

  const RoboI32 drawW = static_cast<RoboI32>(state->image.width);
  const RoboI32 drawH = static_cast<RoboI32>(state->image.height);
  if (drawW <= 0 || drawH <= 0) {
    return ROBO_FALSE;
  }

  const RoboI32 hotX = state->image.hotX;
  const RoboI32 hotY = state->image.hotY;
  const RoboI32 newLeft = state->tipX - hotX;
  const RoboI32 newTop = state->tipY - hotY;

  if (state->hasLast && !state->imageChanged && state->lastLeft == newLeft && state->lastTop == newTop &&
      state->lastWidth == drawW && state->lastHeight == drawH) {
    return ROBO_FALSE;
  }

  const RoboU8* cursorPx = state->image.pixels;
  const RoboU32 cursorStride = state->image.stride;
  const RoboI32 screenW = static_cast<RoboI32>(handoff->frameBuffer.width);
  const RoboI32 screenH = static_cast<RoboI32>(handoff->frameBuffer.height);

  /* Step 1: Draw the NEW cursor first so cursor is ALWAYS visible. */
  RoboI32 nx0 = newLeft, ny0 = newTop, nx1 = newLeft + drawW, ny1 = newTop + drawH;
  RoboCursorClipRect(&nx0, &ny0, &nx1, &ny1, screenW, screenH);
  for (RoboI32 y = ny0; y < ny1; ++y) {
    RoboCursorBlitSpan(handoff, desktopBuffer, y, nx0, nx1, newLeft, newTop, drawW, drawH, cursorPx, cursorStride,
                       ROBO_TRUE);
  }

  /* Step 2: If we had a prior cursor location, restore pixels that are outside the new cursor. */
  if (state->hasLast) {
    RoboI32 ox0 = state->lastLeft, oy0 = state->lastTop, ox1 = state->lastLeft + state->lastWidth,
            oy1 = state->lastTop + state->lastHeight;
    RoboCursorClipRect(&ox0, &oy0, &ox1, &oy1, screenW, screenH);

    for (RoboI32 y = oy0; y < oy1; ++y) {
      if (y < ny0 || y >= ny1) {
        /* Entire scanline is outside the new cursor — restore completely */
        RoboCursorBlitSpan(handoff, desktopBuffer, y, ox0, ox1, 0, 0, 0, 0, ROBO_NULL, 0, ROBO_FALSE);
      } else {
        /* Scanline intersects new cursor vertically — restore only parts outside [nx0, nx1) */
        if (ox0 < nx0) {
          const RoboI32 leftEnd = RoboCursorMin(ox1, nx0);
          RoboCursorBlitSpan(handoff, desktopBuffer, y, ox0, leftEnd, 0, 0, 0, 0, ROBO_NULL, 0, ROBO_FALSE);
        }
        if (ox1 > nx1) {
          const RoboI32 rightStart = RoboCursorMax(ox0, nx1);
          RoboCursorBlitSpan(handoff, desktopBuffer, y, rightStart, ox1, 0, 0, 0, 0, ROBO_NULL, 0, ROBO_FALSE);
        }
      }
    }
  }

  state->lastLeft = newLeft;
  state->lastTop = newTop;
  state->lastWidth = drawW;
  state->lastHeight = drawH;
  state->hasLast = ROBO_TRUE;
  state->imageChanged = ROBO_FALSE;
  return ROBO_TRUE;
}
