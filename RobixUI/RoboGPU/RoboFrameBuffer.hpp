#pragma once

#include "../RoboShared/RoboHandoff.hpp"

/*
 * Framebuffer surface math — mirrors linux fbdev fix.line_length + DRM pitch.
 * Front (GOP) is uncached scanout; back is cached shadow (sys_* style).
 * Always address pixels as: base + y * pitch + x * bytesPerPixel.
 */

inline RoboU32 RoboFrameBufferPitch(const RoboFrameBufferInfo* info) {
  if (info == ROBO_NULL || info->bytesPerPixel == 0) {
    return 0;
  }
  return info->pixelsPerScanLine * info->bytesPerPixel;
}

inline RoboU32 RoboFrameBufferCpp(const RoboFrameBufferInfo* info) {
  if (info == ROBO_NULL || info->bytesPerPixel == 0) {
    return 4;
  }
  return info->bytesPerPixel;
}

/* drm_fb_clip_offset — byte offset of (x,y) within a pitched buffer. */
inline RoboU64 RoboFrameBufferClipOffset(RoboU32 pitch, RoboU32 cpp, RoboI32 x, RoboI32 y) {
  return static_cast<RoboU64>(y) * static_cast<RoboU64>(pitch) + static_cast<RoboU64>(x) * static_cast<RoboU64>(cpp);
}

inline RoboU8* RoboFrameBufferRow(RoboU8* base, RoboU32 pitch, RoboI32 y) {
  return base + static_cast<RoboU64>(y) * static_cast<RoboU64>(pitch);
}

inline const RoboU8* RoboFrameBufferRowConst(const RoboU8* base, RoboU32 pitch, RoboI32 y) {
  return base + static_cast<RoboU64>(y) * static_cast<RoboU64>(pitch);
}

inline RoboU64 RoboFrameBufferByteSize(const RoboFrameBufferInfo* info) {
  if (info == ROBO_NULL) {
    return 0;
  }
  return static_cast<RoboU64>(RoboFrameBufferPitch(info)) * static_cast<RoboU64>(info->height);
}

inline void RoboFrameBufferClipRect(RoboI32* x, RoboI32* y, RoboI32* width, RoboI32* height, RoboI32 screenW,
                                    RoboI32 screenH) {
  if (x == ROBO_NULL || y == ROBO_NULL || width == ROBO_NULL || height == ROBO_NULL) {
    return;
  }
  RoboI32 x0 = *x;
  RoboI32 y0 = *y;
  RoboI32 x1 = *x + *width;
  RoboI32 y1 = *y + *height;
  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 > screenW) {
    x1 = screenW;
  }
  if (y1 > screenH) {
    y1 = screenH;
  }
  *x = x0;
  *y = y0;
  *width = x1 - x0;
  *height = y1 - y0;
  if (*width < 0) {
    *width = 0;
  }
  if (*height < 0) {
    *height = 0;
  }
}
