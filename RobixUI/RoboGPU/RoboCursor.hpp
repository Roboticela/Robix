#pragma once

#include "../RoboShared/RoboCursorPack.hpp"
#include "../RoboShared/RoboHandoff.hpp"
#include "../RoboShared/RoboTheme.hpp"
#include "RoboGPU.hpp"

/*
 * Soft mouse cursor — DRM cursor2 hotspot + fbdev soft_cursor imageblit.
 * Draw new sprite first, then restore old-only underlay so the tip never blanks.
 */
struct RoboCursorState {
  const void* pack;
  RoboCursorTheme theme;
  RoboCursorKind kind;
  RoboCursorImageView image;
  RoboI32 tipX;
  RoboI32 tipY;
  RoboI32 lastLeft;
  RoboI32 lastTop;
  RoboI32 lastWidth;
  RoboI32 lastHeight;
  RoboI32 displaySize;
  RoboBool enabled;
  RoboBool hasLast;
  RoboBool imageChanged;
};

void RoboCursorInit(RoboCursorState* state, const void* pack, const RoboHandoff* handoff);
void RoboCursorSetTheme(RoboCursorState* state, RoboCursorTheme theme);
void RoboCursorSetKind(RoboCursorState* state, RoboCursorKind kind);
void RoboCursorMoveTip(RoboCursorState* state, RoboI32 tipX, RoboI32 tipY, RoboI32 screenW, RoboI32 screenH);
/* Damage the previous and current sprite. Present blends the cursor (drm soft cursor). */
void RoboCursorMarkDamage(RoboCursorState* state, RoboGPU* gpu);
/* Returns ROBO_TRUE if the framebuffer was updated. */
RoboBool RoboCursorCompose(RoboCursorState* state, const RoboHandoff* handoff, const RoboU8* desktopBuffer);
/* Blends cursor sprite on the fly during present flushes — atomic, zero-flicker scanout. */
void RoboCursorBlendRow(const void* cursorVoid, const RoboFrameBufferInfo* fbInfo,
                        const RoboU32* srcDeskRow, RoboU32* dstFrontRow,
                        RoboI32 y, RoboI32 x0, RoboI32 x1);
