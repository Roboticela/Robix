#pragma once

#include "../RoboShared/RoboHandoff.hpp"
#include "../RoboShared/RoboImage.hpp"
#include "RoboDamage.hpp"
#include "RoboFrameBuffer.hpp"

/*
 * RoboGPU — default soft GPU (any hardware via GOP front + cached back).
 * Draw on back (sys_fillrect / sys_copyarea / imageblit); Present = drm_fb_memcpy
 * of the damage list onto uncached front. X64 and ARM64 share this path.
 */
constexpr RoboU32 ROBO_GPU_SCISSOR_MAX = 8;

struct RoboGPUScissor {
  RoboI32 x;
  RoboI32 y;
  RoboI32 width;
  RoboI32 height;
};

struct RoboGPU;

using RoboCursorBlendRowFn = void (*)(const void* cursorState, const RoboFrameBufferInfo* fbInfo,
                                      const RoboU32* srcDeskRow, RoboU32* dstFrontRow,
                                      RoboI32 y, RoboI32 x0, RoboI32 x1);
using RoboGPUSpanWorker = void (*)(void* context, RoboU32 workerIndex, RoboU32 workerCount);
using RoboGPUParallelHook = void (*)(RoboGPUSpanWorker worker, void* context);
using RoboGPUScanoutHook = RoboBool (*)(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height);

void RoboGPUSetParallel(RoboGPUParallelHook hook);
void RoboGPUSetHardwareScanout(RoboGPUScanoutHook hook);

struct RoboGPU {
  RoboFrameBufferInfo info;
  RoboU8* front; /* scanout (GOP) — uncached */
  RoboU8* back;  /* shadow / desktop — cached */
  RoboU64 backSize;
  RoboDamageList damage;
  RoboBool ownsBack;
  /* GRUB grub_gui_set_viewport nest — clip draws for ScrollView. */
  RoboGPUScissor scissorStack[ROBO_GPU_SCISSOR_MAX];
  RoboU32 scissorDepth;
  const void* cursor;                  /* active soft cursor state */
  RoboCursorBlendRowFn cursorBlendRow; /* cursor row composition callback */
  /* Channel shifts cached once (fb_imageblit). 0 masks mean XRGB8888. */
  RoboU32 redShift;
  RoboU32 greenShift;
  RoboU32 blueShift;
  RoboBool xrgb;
};

/* Uncached scanout store — x86 movnt, same role as memcpy_toio / drm_fb_memcpy. */
void RoboScanoutCopy(void* destination, const void* source, RoboSize bytes);

void RoboGPUInitFromHandoff(RoboGPU* gpu, const RoboHandoff* handoff, void* backBuffer, RoboU64 backSize);
void RoboGPUSetCursor(RoboGPU* gpu, const void* cursor, RoboCursorBlendRowFn blendFn);
RoboU32 RoboGPUPitch(const RoboGPU* gpu);
RoboU32 RoboGPUPackColor(const RoboGPU* gpu, RoboU8 r, RoboU8 g, RoboU8 b, RoboU8 a);

/* sys_fillrect — opaque fill on back + damage */
void RoboGPUClear(RoboGPU* gpu, RoboU32 colorARGB);
void RoboGPUFillRect(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height, RoboU32 colorARGB);
void RoboGPUBlendPixel(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboU32 colorARGB, RoboU8 coverage);

/* fb_copyarea — overlap-safe scroll/move on back + damage */
void RoboGPUCopyArea(RoboGPU* gpu, RoboI32 dx, RoboI32 dy, RoboI32 sx, RoboI32 sy, RoboI32 width, RoboI32 height);

void RoboGPUBlitRGBA(RoboGPU* gpu, RoboI32 dstX, RoboI32 dstY, const RoboU8* src, RoboU32 srcWidth, RoboU32 srcHeight,
                     RoboU32 srcStride);
/* opacity01 in [0,1] — multiplies source alpha (logo fade-in). */
void RoboGPUBlitRGBAOpacity(RoboGPU* gpu, RoboI32 dstX, RoboI32 dstY, const RoboU8* src, RoboU32 srcWidth,
                            RoboU32 srcHeight, RoboU32 srcStride, float opacity01);
/*
 * HD glyph blit — 8-bit coverage atlas → src-over blend (GRUB font alpha path).
 * 1:1 when dst matches src; fixed-point bilinear when scaling (Retina-style downsample).
 */
void RoboGPUBlitCoverageScaled(RoboGPU* gpu, RoboI32 dstX, RoboI32 dstY, RoboI32 dstW, RoboI32 dstH,
                               const RoboU8* coverage, RoboU32 srcW, RoboU32 srcH, RoboU8 r, RoboU8 g, RoboU8 b);
/*
 * GRUB bitmap_scale nearest + drm_fb_memcpy style — opaque cover/stretch into back.
 * Integer fixed-point only (no float). scaleMode: 0=stretch, 1=cover, 2=fit.
 */
void RoboGPUBlitRGBAScaled(RoboGPU* gpu, RoboI32 dstX, RoboI32 dstY, RoboI32 dstW, RoboI32 dstH, const RoboU8* src,
                           RoboU32 srcWidth, RoboU32 srcHeight, RoboU32 srcStride, RoboU32 scaleMode);
void RoboGPUBlitImageProportional(RoboGPU* gpu, const RoboImageView& image, RoboI32 boxX, RoboI32 boxY, RoboI32 boxW,
                                  RoboI32 boxH);

/* drm_fb_memcpy Present — flush damage rects back→front, then clear damage. */
void RoboGPUPresent(RoboGPU* gpu);
void RoboGPUPresentFull(RoboGPU* gpu);
void RoboGPUCopyBackToFront(RoboGPU* gpu);

void RoboGPUMarkDamage(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height);
void RoboGPUMarkFullDamage(RoboGPU* gpu);

/* Nested viewport clip (intersect with current). Pop restores prior. */
void RoboGPUPushScissor(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height);
void RoboGPUPopScissor(RoboGPU* gpu);

/* Optional tap used while the platform paints a document for the OS. */
struct RoboGPUPaintSink {
  void (*fill)(void* context, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height, RoboU32 colorARGB);
  void (*blend)(void* context, RoboI32 x, RoboI32 y, RoboU32 colorARGB, RoboU8 coverage);
  void* context;
};
void RoboGPUSetPaintSink(const RoboGPUPaintSink* sink);
void RoboGPUApplyScissor(const RoboGPU* gpu, RoboI32* x, RoboI32* y, RoboI32* width, RoboI32* height);
