#include "RoboGPU.hpp"

#include <emmintrin.h>

using RoboGPUSpanWorker = void (*)(void* context, RoboU32 workerIndex, RoboU32 workerCount);
using RoboGPUParallelHook = void (*)(RoboGPUSpanWorker worker, void* context);
using RoboGPUScanoutHook = RoboBool (*)(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height);

static RoboGPUParallelHook gRoboGPUParallel = ROBO_NULL;
static RoboGPUScanoutHook gRoboGPUScanout = ROBO_NULL;
constexpr RoboI32 ROBO_GPU_SCALE_MAX = 8192;
static RoboU16 gRoboGPUScaleCol[ROBO_GPU_SCALE_MAX];

void RoboGPUSetParallel(RoboGPUParallelHook hook) {
  gRoboGPUParallel = hook;
}

void RoboGPUSetHardwareScanout(RoboGPUScanoutHook hook) {
  gRoboGPUScanout = hook;
}

static void RoboGPUForSpans(RoboU32 count, RoboGPUSpanWorker worker, void* context) {
  if (worker == ROBO_NULL || count == 0) {
    return;
  }
  if (gRoboGPUParallel == ROBO_NULL || count < 64u) {
    worker(context, 0, 1);
    return;
  }
  gRoboGPUParallel(worker, context);
}

static RoboU32 RoboGPUShiftFromMask(RoboU32 mask) {
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

RoboU32 RoboGPUPitch(const RoboGPU* gpu) {
  if (gpu == ROBO_NULL) {
    return 0;
  }
  return RoboFrameBufferPitch(&gpu->info);
}

RoboU32 RoboGPUPackColor(const RoboGPU* gpu, RoboU8 r, RoboU8 g, RoboU8 b, RoboU8 a) {
  if (gpu == ROBO_NULL) {
    return 0;
  }
  if (gpu->info.redMask == 0 && gpu->info.greenMask == 0 && gpu->info.blueMask == 0) {
    return (static_cast<RoboU32>(a) << 24) | (static_cast<RoboU32>(r) << 16) | (static_cast<RoboU32>(g) << 8) |
           static_cast<RoboU32>(b);
  }
  if (gpu->xrgb) {
    return (static_cast<RoboU32>(a) << 24) | (static_cast<RoboU32>(r) << 16) | (static_cast<RoboU32>(g) << 8) |
           static_cast<RoboU32>(b);
  }
  return ((static_cast<RoboU32>(r) << gpu->redShift) & gpu->info.redMask) |
         ((static_cast<RoboU32>(g) << gpu->greenShift) & gpu->info.greenMask) |
         ((static_cast<RoboU32>(b) << gpu->blueShift) & gpu->info.blueMask) | (static_cast<RoboU32>(a) << 24);
}

void RoboGPUInitFromHandoff(RoboGPU* gpu, const RoboHandoff* handoff, void* backBuffer, RoboU64 backSize) {
  if (gpu == ROBO_NULL || handoff == ROBO_NULL) {
    return;
  }
  RoboMemorySet(gpu, 0, sizeof(RoboGPU));
  gpu->info = handoff->frameBuffer;
  if (gpu->info.bytesPerPixel == 0) {
    gpu->info.bytesPerPixel = 4;
  }
  gpu->front = reinterpret_cast<RoboU8*>(handoff->frameBuffer.base);
  gpu->back = static_cast<RoboU8*>(backBuffer);
  gpu->backSize = backSize;
  gpu->ownsBack = ROBO_FALSE;
  gpu->redShift = RoboGPUShiftFromMask(gpu->info.redMask);
  gpu->greenShift = RoboGPUShiftFromMask(gpu->info.greenMask);
  gpu->blueShift = RoboGPUShiftFromMask(gpu->info.blueMask);
  gpu->xrgb = (gpu->info.redMask == 0x00FF0000u && gpu->info.greenMask == 0x0000FF00u &&
               gpu->info.blueMask == 0x000000FFu)
                  ? ROBO_TRUE
                  : ((gpu->info.redMask == 0 && gpu->info.greenMask == 0 && gpu->info.blueMask == 0) ? ROBO_TRUE
                                                                                                    : ROBO_FALSE);
  if (gpu->xrgb && gpu->info.redMask == 0) {
    gpu->redShift = 16;
    gpu->greenShift = 8;
    gpu->blueShift = 0;
  }
  RoboDamageClear(&gpu->damage);
}

void RoboScanoutCopy(void* destination, const void* source, RoboSize bytes) {
  auto* dst = static_cast<RoboU8*>(destination);
  const auto* src = static_cast<const RoboU8*>(source);
  if (dst == ROBO_NULL || src == ROBO_NULL || bytes == 0) {
    return;
  }
  while (bytes > 0 && ((reinterpret_cast<RoboU64>(dst) & 15u) != 0 || (reinterpret_cast<RoboU64>(src) & 15u) != 0)) {
    *dst++ = *src++;
    --bytes;
  }
  while (bytes >= 64) {
    const __m128i a = _mm_load_si128(reinterpret_cast<const __m128i*>(src));
    const __m128i b = _mm_load_si128(reinterpret_cast<const __m128i*>(src + 16));
    const __m128i c = _mm_load_si128(reinterpret_cast<const __m128i*>(src + 32));
    const __m128i d = _mm_load_si128(reinterpret_cast<const __m128i*>(src + 48));
    _mm_stream_si128(reinterpret_cast<__m128i*>(dst), a);
    _mm_stream_si128(reinterpret_cast<__m128i*>(dst + 16), b);
    _mm_stream_si128(reinterpret_cast<__m128i*>(dst + 32), c);
    _mm_stream_si128(reinterpret_cast<__m128i*>(dst + 48), d);
    dst += 64;
    src += 64;
    bytes -= 64;
  }
  while (bytes > 0) {
    *dst++ = *src++;
    --bytes;
  }
  _mm_sfence();
}

void RoboGPUMarkDamage(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height) {
  if (gpu == ROBO_NULL) {
    return;
  }
  RoboFrameBufferClipRect(&x, &y, &width, &height, static_cast<RoboI32>(gpu->info.width),
                          static_cast<RoboI32>(gpu->info.height));
  if (width <= 0 || height <= 0) {
    return;
  }
  RoboDamageAdd(&gpu->damage, x, y, width, height);
}

void RoboGPUMarkFullDamage(RoboGPU* gpu) {
  if (gpu == ROBO_NULL) {
    return;
  }
  RoboDamageMarkFull(&gpu->damage);
}

void RoboGPUApplyScissor(const RoboGPU* gpu, RoboI32* x, RoboI32* y, RoboI32* width, RoboI32* height) {
  if (gpu == ROBO_NULL || gpu->scissorDepth == 0 || x == ROBO_NULL || y == ROBO_NULL || width == ROBO_NULL ||
      height == ROBO_NULL) {
    return;
  }
  const RoboGPUScissor& s = gpu->scissorStack[gpu->scissorDepth - 1];
  RoboI32 x0 = *x;
  RoboI32 y0 = *y;
  RoboI32 x1 = *x + *width;
  RoboI32 y1 = *y + *height;
  if (x0 < s.x) {
    x0 = s.x;
  }
  if (y0 < s.y) {
    y0 = s.y;
  }
  if (x1 > s.x + s.width) {
    x1 = s.x + s.width;
  }
  if (y1 > s.y + s.height) {
    y1 = s.y + s.height;
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

void RoboGPUPushScissor(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height) {
  if (gpu == ROBO_NULL || gpu->scissorDepth >= ROBO_GPU_SCISSOR_MAX) {
    return;
  }
  RoboFrameBufferClipRect(&x, &y, &width, &height, static_cast<RoboI32>(gpu->info.width),
                          static_cast<RoboI32>(gpu->info.height));
  if (gpu->scissorDepth > 0) {
    RoboGPUApplyScissor(gpu, &x, &y, &width, &height);
  }
  RoboGPUScissor& slot = gpu->scissorStack[gpu->scissorDepth++];
  slot.x = x;
  slot.y = y;
  slot.width = width;
  slot.height = height;
}

void RoboGPUPopScissor(RoboGPU* gpu) {
  if (gpu == ROBO_NULL || gpu->scissorDepth == 0) {
    return;
  }
  --gpu->scissorDepth;
}

void RoboGPUResetScissor(RoboGPU* gpu) {
  if (gpu == ROBO_NULL) {
    return;
  }
  gpu->scissorDepth = 0;
}

/* fb_fillrect / sys_fillrect — word stores + 8-pixel unroll on cached back. */
static void RoboGPUFillSpan32(RoboU32* pixels, RoboI32 count, RoboU32 color) {
  while (count >= 8) {
    pixels[0] = color;
    pixels[1] = color;
    pixels[2] = color;
    pixels[3] = color;
    pixels[4] = color;
    pixels[5] = color;
    pixels[6] = color;
    pixels[7] = color;
    pixels += 8;
    count -= 8;
  }
  while (count > 0) {
    *pixels++ = color;
    --count;
  }
}

static const RoboGPUPaintSink* gRoboPaintSink = ROBO_NULL;

void RoboGPUSetPaintSink(const RoboGPUPaintSink* sink) { gRoboPaintSink = sink; }

void RoboGPUClear(RoboGPU* gpu, RoboU32 colorARGB) {
  if (gpu == ROBO_NULL || gpu->back == ROBO_NULL) {
    return;
  }
  const RoboU32 width = gpu->info.width;
  const RoboU32 height = gpu->info.height;
  const RoboU32 pitch = RoboGPUPitch(gpu);
  for (RoboU32 y = 0; y < height; ++y) {
    auto* row = reinterpret_cast<RoboU32*>(RoboFrameBufferRow(gpu->back, pitch, static_cast<RoboI32>(y)));
    RoboGPUFillSpan32(row, static_cast<RoboI32>(width), colorARGB);
  }
  RoboGPUMarkFullDamage(gpu);
  if (gRoboPaintSink != ROBO_NULL && gRoboPaintSink->fill != ROBO_NULL) {
    gRoboPaintSink->fill(gRoboPaintSink->context, 0, 0, static_cast<RoboI32>(width), static_cast<RoboI32>(height),
                         colorARGB);
  }
}

void RoboGPUFillRect(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height, RoboU32 colorARGB) {
  if (gpu == ROBO_NULL || gpu->back == ROBO_NULL || width <= 0 || height <= 0) {
    return;
  }
  RoboFrameBufferClipRect(&x, &y, &width, &height, static_cast<RoboI32>(gpu->info.width),
                          static_cast<RoboI32>(gpu->info.height));
  RoboGPUApplyScissor(gpu, &x, &y, &width, &height);
  if (width <= 0 || height <= 0) {
    return;
  }
  const RoboU32 pitch = RoboGPUPitch(gpu);
  for (RoboI32 row = y; row < y + height; ++row) {
    auto* pixels = reinterpret_cast<RoboU32*>(RoboFrameBufferRow(gpu->back, pitch, row));
    RoboGPUFillSpan32(pixels + x, width, colorARGB);
  }
  RoboDamageAdd(&gpu->damage, x, y, width, height);
  if (gRoboPaintSink != ROBO_NULL && gRoboPaintSink->fill != ROBO_NULL) {
    gRoboPaintSink->fill(gRoboPaintSink->context, x, y, width, height, colorARGB);
  }
}

/*
 * fb_copyarea — same-buffer scroll with reverse scan when dy>sy or same-row dx>sx.
 */
void RoboGPUCopyArea(RoboGPU* gpu, RoboI32 dx, RoboI32 dy, RoboI32 sx, RoboI32 sy, RoboI32 width, RoboI32 height) {
  if (gpu == ROBO_NULL || gpu->back == ROBO_NULL || width <= 0 || height <= 0) {
    return;
  }
  const RoboI32 screenW = static_cast<RoboI32>(gpu->info.width);
  const RoboI32 screenH = static_cast<RoboI32>(gpu->info.height);

  /* Clip source into screen, adjust dest by the same trim. */
  RoboI32 srcX = sx;
  RoboI32 srcY = sy;
  RoboI32 dstX = dx;
  RoboI32 dstY = dy;
  RoboI32 w = width;
  RoboI32 h = height;
  if (srcX < 0) {
    dstX -= srcX;
    w += srcX;
    srcX = 0;
  }
  if (srcY < 0) {
    dstY -= srcY;
    h += srcY;
    srcY = 0;
  }
  if (srcX + w > screenW) {
    w = screenW - srcX;
  }
  if (srcY + h > screenH) {
    h = screenH - srcY;
  }
  if (dstX < 0) {
    srcX -= dstX;
    w += dstX;
    dstX = 0;
  }
  if (dstY < 0) {
    srcY -= dstY;
    h += dstY;
    dstY = 0;
  }
  if (dstX + w > screenW) {
    w = screenW - dstX;
  }
  if (dstY + h > screenH) {
    h = screenH - dstY;
  }
  if (w <= 0 || h <= 0) {
    return;
  }

  const RoboU32 pitch = RoboGPUPitch(gpu);
  const RoboU32 cpp = RoboFrameBufferCPP(&gpu->info);
  const RoboSize rowBytes = static_cast<RoboSize>(w) * cpp;
  const RoboBool revCopy = (dstY > srcY) || (dstY == srcY && dstX > srcX) ? ROBO_TRUE : ROBO_FALSE;

  if (revCopy) {
    for (RoboI32 row = h - 1; row >= 0; --row) {
      RoboU8* dst = RoboFrameBufferRow(gpu->back, pitch, dstY + row) + static_cast<RoboU64>(dstX) * cpp;
      const RoboU8* src = RoboFrameBufferRowConst(gpu->back, pitch, srcY + row) + static_cast<RoboU64>(srcX) * cpp;
      RoboMemoryMove(dst, src, rowBytes);
    }
  } else {
    for (RoboI32 row = 0; row < h; ++row) {
      RoboU8* dst = RoboFrameBufferRow(gpu->back, pitch, dstY + row) + static_cast<RoboU64>(dstX) * cpp;
      const RoboU8* src = RoboFrameBufferRowConst(gpu->back, pitch, srcY + row) + static_cast<RoboU64>(srcX) * cpp;
      RoboMemoryMove(dst, src, rowBytes);
    }
  }
  RoboDamageAdd(&gpu->damage, dstX, dstY, w, h);
}

/* Bilinear sample — used when upscaling or 1:1. */
static void RoboGPUSampleRGBABilinear(const RoboU8* src, RoboU32 srcWidth, RoboU32 srcHeight, RoboU32 srcStride,
                                      float u, float v, RoboU8* r, RoboU8* g, RoboU8* b, RoboU8* a) {
  if (srcWidth == 0 || srcHeight == 0) {
    *r = *g = *b = *a = 0;
    return;
  }
  float x = u * static_cast<float>(srcWidth - 1);
  float y = v * static_cast<float>(srcHeight - 1);
  RoboU32 x0 = static_cast<RoboU32>(x);
  RoboU32 y0 = static_cast<RoboU32>(y);
  RoboU32 x1 = x0 + 1 < srcWidth ? x0 + 1 : x0;
  RoboU32 y1 = y0 + 1 < srcHeight ? y0 + 1 : y0;
  const float fx = x - static_cast<float>(x0);
  const float fy = y - static_cast<float>(y0);
  const RoboU8* p00 = src + static_cast<RoboU64>(y0) * srcStride + static_cast<RoboU64>(x0) * 4ull;
  const RoboU8* p10 = src + static_cast<RoboU64>(y0) * srcStride + static_cast<RoboU64>(x1) * 4ull;
  const RoboU8* p01 = src + static_cast<RoboU64>(y1) * srcStride + static_cast<RoboU64>(x0) * 4ull;
  const RoboU8* p11 = src + static_cast<RoboU64>(y1) * srcStride + static_cast<RoboU64>(x1) * 4ull;
  auto lerpChan = [fx, fy](RoboU8 c00, RoboU8 c10, RoboU8 c01, RoboU8 c11) -> RoboU8 {
    const float top = static_cast<float>(c00) * (1.0f - fx) + static_cast<float>(c10) * fx;
    const float bot = static_cast<float>(c01) * (1.0f - fx) + static_cast<float>(c11) * fx;
    return static_cast<RoboU8>(top * (1.0f - fy) + bot * fy + 0.5f);
  };
  *r = lerpChan(p00[0], p10[0], p01[0], p11[0]);
  *g = lerpChan(p00[1], p10[1], p01[1], p11[1]);
  *b = lerpChan(p00[2], p10[2], p01[2], p11[2]);
  *a = lerpChan(p00[3], p10[3], p01[3], p11[3]);
}

void RoboGPUBlitRGBAOpacity(RoboGPU* gpu, RoboI32 dstX, RoboI32 dstY, const RoboU8* src, RoboU32 srcWidth,
                            RoboU32 srcHeight, RoboU32 srcStride, float opacity01) {
  if (gpu == ROBO_NULL || gpu->back == ROBO_NULL || src == ROBO_NULL || opacity01 <= 0.001f) {
    return;
  }
  if (opacity01 > 1.0f) {
    opacity01 = 1.0f;
  }
  const RoboU32 pitch = RoboGPUPitch(gpu);
  const RoboU32 opacityU8 = static_cast<RoboU32>(opacity01 * 255.0f + 0.5f);
  if (opacityU8 == 255 && dstX >= 0 && dstY >= 0 &&
      dstX + static_cast<RoboI32>(srcWidth) <= static_cast<RoboI32>(gpu->info.width) &&
      dstY + static_cast<RoboI32>(srcHeight) <= static_cast<RoboI32>(gpu->info.height)) {
    for (RoboU32 y = 0; y < srcHeight; ++y) {
      auto* dstRow = reinterpret_cast<RoboU32*>(RoboFrameBufferRow(gpu->back, pitch, dstY + static_cast<RoboI32>(y)));
      const auto* srcRow = reinterpret_cast<const RoboU32*>(src + static_cast<RoboU64>(y) * srcStride);
      if (gpu->xrgb == ROBO_TRUE) {
        /* Source is RGBA in memory. Firmware BGR keeps red in bits 16..23. */
        for (RoboU32 x = 0; x < srcWidth; ++x) {
          const RoboU32 p = srcRow[x];
          dstRow[dstX + static_cast<RoboI32>(x)] =
              (p & 0xFF00FF00u) | ((p & 0xFFu) << 16) | ((p >> 16) & 0xFFu);
        }
      } else {
        RoboMemoryCopy(dstRow + dstX, srcRow, static_cast<RoboSize>(srcWidth) * 4u);
      }
    }
    RoboGPUMarkDamage(gpu, dstX, dstY, static_cast<RoboI32>(srcWidth), static_cast<RoboI32>(srcHeight));
    return;
  }
  RoboI32 dirtyX = dstX;
  RoboI32 dirtyY = dstY;
  RoboI32 dirtyW = static_cast<RoboI32>(srcWidth);
  RoboI32 dirtyH = static_cast<RoboI32>(srcHeight);

  for (RoboU32 y = 0; y < srcHeight; ++y) {
    const RoboI32 outY = dstY + static_cast<RoboI32>(y);
    if (outY < 0 || outY >= static_cast<RoboI32>(gpu->info.height)) {
      continue;
    }
    auto* dstRow = reinterpret_cast<RoboU32*>(RoboFrameBufferRow(gpu->back, pitch, outY));
    const RoboU8* srcRow = src + static_cast<RoboU64>(y) * srcStride;
    for (RoboU32 x = 0; x < srcWidth; ++x) {
      const RoboI32 outX = dstX + static_cast<RoboI32>(x);
      if (outX < 0 || outX >= static_cast<RoboI32>(gpu->info.width)) {
        continue;
      }
      const RoboU8 r = srcRow[x * 4 + 0];
      const RoboU8 g = srcRow[x * 4 + 1];
      const RoboU8 b = srcRow[x * 4 + 2];
      const RoboU8 srcA = srcRow[x * 4 + 3];
      const RoboU8 a = static_cast<RoboU8>((static_cast<RoboU32>(srcA) * opacityU8) / 255u);
      if (a == 0) {
        continue;
      }
      if (a == 255) {
        dstRow[outX] = RoboGPUPackColor(gpu, r, g, b, a);
        continue;
      }
      RoboU32 redMask = gpu->info.redMask;
      RoboU32 greenMask = gpu->info.greenMask;
      RoboU32 blueMask = gpu->info.blueMask;
      if (redMask == 0 && greenMask == 0 && blueMask == 0) {
        redMask = 0x00FF0000;
        greenMask = 0x0000FF00;
        blueMask = 0x000000FF;
      }
      const RoboU32 dst = dstRow[outX];
      const RoboU8 dr = static_cast<RoboU8>((dst & redMask) >> RoboGPUShiftFromMask(redMask));
      const RoboU8 dg = static_cast<RoboU8>((dst & greenMask) >> RoboGPUShiftFromMask(greenMask));
      const RoboU8 db = static_cast<RoboU8>((dst & blueMask) >> RoboGPUShiftFromMask(blueMask));
      const RoboU8 outR = static_cast<RoboU8>((r * a + dr * (255 - a)) / 255);
      const RoboU8 outG = static_cast<RoboU8>((g * a + dg * (255 - a)) / 255);
      const RoboU8 outB = static_cast<RoboU8>((b * a + db * (255 - a)) / 255);
      dstRow[outX] = RoboGPUPackColor(gpu, outR, outG, outB, 255);
    }
  }
  RoboGPUMarkDamage(gpu, dirtyX, dirtyY, dirtyW, dirtyH);
}

void RoboGPUBlitRGBA(RoboGPU* gpu, RoboI32 dstX, RoboI32 dstY, const RoboU8* src, RoboU32 srcWidth, RoboU32 srcHeight,
                     RoboU32 srcStride) {
  RoboGPUBlitRGBAOpacity(gpu, dstX, dstY, src, srcWidth, srcHeight, srcStride, 1.0f);
}

static void RoboGPUUnpackRGB(const RoboGPU* gpu, RoboU32 packed, RoboU8* r, RoboU8* g, RoboU8* b) {
  if (gpu->xrgb) {
    *r = static_cast<RoboU8>((packed >> 16) & 0xFFu);
    *g = static_cast<RoboU8>((packed >> 8) & 0xFFu);
    *b = static_cast<RoboU8>(packed & 0xFFu);
    return;
  }
  *r = static_cast<RoboU8>((packed & gpu->info.redMask) >> gpu->redShift);
  *g = static_cast<RoboU8>((packed & gpu->info.greenMask) >> gpu->greenShift);
  *b = static_cast<RoboU8>((packed & gpu->info.blueMask) >> gpu->blueShift);
}

#include "RoboGPUGamma.inc"

static RoboU8 gRoboGPUFromLinear[65536];
static RoboBool gRoboGPUGammaReady = ROBO_FALSE;

static void RoboGPUInitGamma() {
  if (gRoboGPUGammaReady) {
    return;
  }
  RoboU32 start = 0;
  for (RoboU32 channel = 0; channel < 256u; ++channel) {
    const RoboU32 end = kRoboGPUToLinear[channel];
    for (RoboU32 value = start; value <= end && value < 65536u; ++value) {
      gRoboGPUFromLinear[value] = static_cast<RoboU8>(channel);
    }
    start = end + 1u;
  }
  gRoboGPUGammaReady = ROBO_TRUE;
}

static RoboU8 RoboGPUBlendChannel(RoboU8 source, RoboU8 destination, RoboU32 coverage) {
  RoboGPUInitGamma();
  const RoboU32 inverse = 255u - coverage;
  const RoboU32 linear = (static_cast<RoboU32>(kRoboGPUToLinear[source]) * coverage +
                          static_cast<RoboU32>(kRoboGPUToLinear[destination]) * inverse + 127u) /
                         255u;
  return gRoboGPUFromLinear[linear > 65535u ? 65535u : linear];
}

void RoboGPUBlendPixel(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboU32 colorARGB, RoboU8 coverage) {
  if (gpu == ROBO_NULL || gpu->back == ROBO_NULL || coverage == 0 || x < 0 || y < 0 ||
      x >= static_cast<RoboI32>(gpu->info.width) || y >= static_cast<RoboI32>(gpu->info.height)) {
    return;
  }
  if (gpu->scissorDepth > 0) {
    const RoboGPUScissor& clip = gpu->scissorStack[gpu->scissorDepth - 1];
    if (x < clip.x || y < clip.y || x >= clip.x + clip.width || y >= clip.y + clip.height) {
      return;
    }
  }
  const RoboU32 pitch = RoboGPUPitch(gpu);
  auto* row = reinterpret_cast<RoboU32*>(RoboFrameBufferRow(gpu->back, pitch, y));
  const RoboU8 r = static_cast<RoboU8>((colorARGB >> 16) & 0xFF);
  const RoboU8 g = static_cast<RoboU8>((colorARGB >> 8) & 0xFF);
  const RoboU8 b = static_cast<RoboU8>(colorARGB & 0xFF);
  if (coverage == 255) {
    row[x] = RoboGPUPackColor(gpu, r, g, b, 255);
    if (gRoboPaintSink != ROBO_NULL && gRoboPaintSink->blend != ROBO_NULL) {
      gRoboPaintSink->blend(gRoboPaintSink->context, x, y, colorARGB, coverage);
    }
    return;
  }
  RoboU8 dr = 0;
  RoboU8 dg = 0;
  RoboU8 db = 0;
  RoboGPUUnpackRGB(gpu, row[x], &dr, &dg, &db);
  row[x] = RoboGPUPackColor(gpu, RoboGPUBlendChannel(r, dr, coverage), RoboGPUBlendChannel(g, dg, coverage),
                           RoboGPUBlendChannel(b, db, coverage), 255);
  if (gRoboPaintSink != ROBO_NULL && gRoboPaintSink->blend != ROBO_NULL) {
    gRoboPaintSink->blend(gRoboPaintSink->context, x, y, colorARGB, coverage);
  }
}

static RoboU8 RoboGPUSampleCoverageBilinear(const RoboU8* coverage, RoboU32 srcW, RoboU32 srcH, RoboU32 fx16,
                                            RoboU32 fy16) {
  /* fx16/fy16 = source coord in 16.16 fixed point. */
  if (srcW == 0 || srcH == 0) {
    return 0;
  }
  RoboU32 maxX = (srcW - 1) << 16;
  RoboU32 maxY = (srcH - 1) << 16;
  if (fx16 > maxX) {
    fx16 = maxX;
  }
  if (fy16 > maxY) {
    fy16 = maxY;
  }
  const RoboU32 x0 = fx16 >> 16;
  const RoboU32 y0 = fy16 >> 16;
  const RoboU32 x1 = (x0 + 1 < srcW) ? x0 + 1 : x0;
  const RoboU32 y1 = (y0 + 1 < srcH) ? y0 + 1 : y0;
  const RoboU32 fx = (fx16 & 0xFFFFu) >> 8; /* 0..255 */
  const RoboU32 fy = (fy16 & 0xFFFFu) >> 8;
  const RoboU32 c00 = coverage[y0 * srcW + x0];
  const RoboU32 c10 = coverage[y0 * srcW + x1];
  const RoboU32 c01 = coverage[y1 * srcW + x0];
  const RoboU32 c11 = coverage[y1 * srcW + x1];
  const RoboU32 top = c00 * (256 - fx) + c10 * fx;
  const RoboU32 bot = c01 * (256 - fx) + c11 * fx;
  return static_cast<RoboU8>(((top * (256 - fy) + bot * fy) >> 16));
}

static inline RoboU8 RoboFontSharpCoverage(RoboU8 c) {
  /* Coverage is already a 4x downsample. Keep the gray edge so stems stay smooth. */
  return c;
}

void RoboGPUBlitCoverageScaled(RoboGPU* gpu, RoboI32 dstX, RoboI32 dstY, RoboI32 dstW, RoboI32 dstH,
                               const RoboU8* coverage, RoboU32 srcW, RoboU32 srcH, RoboU8 r, RoboU8 g, RoboU8 b) {
  if (gpu == ROBO_NULL || gpu->back == ROBO_NULL || coverage == ROBO_NULL || dstW <= 0 || dstH <= 0 || srcW == 0 ||
      srcH == 0) {
    return;
  }
  const RoboU32 pitch = RoboGPUPitch(gpu);
  const RoboBool oneToOne =
      (static_cast<RoboU32>(dstW) == srcW && static_cast<RoboU32>(dstH) == srcH) ? ROBO_TRUE : ROBO_FALSE;
  const RoboBool down = (!oneToOne && (static_cast<RoboU32>(dstW) < srcW || static_cast<RoboU32>(dstH) < srcH))
                            ? ROBO_TRUE
                            : ROBO_FALSE;
  const RoboU32 stepX = oneToOne ? 0 : static_cast<RoboU32>((static_cast<RoboU64>(srcW) << 16) / static_cast<RoboU32>(dstW));
  const RoboU32 stepY = oneToOne ? 0 : static_cast<RoboU32>((static_cast<RoboU64>(srcH) << 16) / static_cast<RoboU32>(dstH));
  const RoboU32 solid = RoboGPUPackColor(gpu, r, g, b, 255);

  RoboI32 clipX = 0, clipY = 0, clipW = static_cast<RoboI32>(gpu->info.width),
          clipH = static_cast<RoboI32>(gpu->info.height);
  if (gpu->scissorDepth > 0) {
    const RoboGPUScissor& s = gpu->scissorStack[gpu->scissorDepth - 1];
    clipX = s.x;
    clipY = s.y;
    clipW = s.width;
    clipH = s.height;
  }
  for (RoboI32 row = 0; row < dstH; ++row) {
    const RoboI32 outY = dstY + row;
    if (outY < clipY || outY >= clipY + clipH || outY < 0 || outY >= static_cast<RoboI32>(gpu->info.height)) {
      continue;
    }
    auto* dstRow = reinterpret_cast<RoboU32*>(RoboFrameBufferRow(gpu->back, pitch, outY));
    for (RoboI32 col = 0; col < dstW; ++col) {
      const RoboI32 outX = dstX + col;
      if (outX < clipX || outX >= clipX + clipW || outX < 0 || outX >= static_cast<RoboI32>(gpu->info.width)) {
        continue;
      }
      RoboU8 cov = 0;
      if (oneToOne) {
        cov = RoboFontSharpCoverage(coverage[static_cast<RoboU32>(row) * srcW + static_cast<RoboU32>(col)]);
      } else if (down) {
        /* Area-averaged box downsampling. Steps are fixed-point so the inner loop does not divide. */
        const RoboU32 sx0 = (static_cast<RoboU32>(col) * stepX) >> 16;
        RoboU32 sx1 = (static_cast<RoboU32>(col + 1) * stepX) >> 16;
        const RoboU32 sy0 = (static_cast<RoboU32>(row) * stepY) >> 16;
        RoboU32 sy1 = (static_cast<RoboU32>(row + 1) * stepY) >> 16;
        if (sx1 > srcW) sx1 = srcW;
        if (sy1 > srcH) sy1 = srcH;
        if (sx1 <= sx0) sx1 = sx0 + 1;
        if (sy1 <= sy0) sy1 = sy0 + 1;
        RoboU32 sum = 0;
        RoboU32 count = 0;
        for (RoboU32 sy = sy0; sy < sy1; ++sy) {
          const RoboU32 rowOffset = sy * srcW;
          for (RoboU32 sx = sx0; sx < sx1; ++sx) {
            sum += coverage[rowOffset + sx];
            ++count;
          }
        }
        const RoboU8 rawCov = (count > 0) ? static_cast<RoboU8>((sum + (count >> 1)) / count) : 0;
        cov = RoboFontSharpCoverage(rawCov);
      } else {
        cov = RoboFontSharpCoverage(RoboGPUSampleCoverageBilinear(coverage, srcW, srcH,
                                                                  static_cast<RoboU32>(col) * stepX,
                                                                  static_cast<RoboU32>(row) * stepY));
      }
      if (cov == 0) {
        continue;
      }
      if (cov == 255) {
        dstRow[outX] = solid;
        continue;
      }
      RoboU8 dr = 0, dg = 0, db = 0;
      RoboGPUUnpackRGB(gpu, dstRow[outX], &dr, &dg, &db);
      dstRow[outX] = RoboGPUPackColor(gpu, RoboGPUBlendChannel(r, dr, cov), RoboGPUBlendChannel(g, dg, cov),
                                     RoboGPUBlendChannel(b, db, cov), 255);
    }
  }
  RoboGPUMarkDamage(gpu, dstX, dstY, dstW, dstH);
}

struct RoboGPUCopyJob {
  RoboGPU* gpu;
  const RoboU8* src;
  RoboI32 drawX;
  RoboI32 drawY;
  RoboI32 drawW;
  RoboI32 drawH;
  RoboU32 srcX0;
  RoboU32 srcY0;
  RoboU32 srcStride;
  RoboU32 pitch;
};

static void RoboGPUCopyWorker(void* raw, RoboU32 worker, RoboU32 workers) {
  auto* job = static_cast<RoboGPUCopyJob*>(raw);
  for (RoboI32 y = static_cast<RoboI32>(worker); y < job->drawH; y += static_cast<RoboI32>(workers)) {
    const RoboU8* srcRow = job->src + static_cast<RoboU64>(job->srcY0 + static_cast<RoboU32>(y)) * job->srcStride +
                           static_cast<RoboU64>(job->srcX0) * 4ull;
    auto* dstRow = reinterpret_cast<RoboU32*>(RoboFrameBufferRow(job->gpu->back, job->pitch, job->drawY + y)) + job->drawX;
    RoboI32 x = 0;
    for (; x + 3 < job->drawW; x += 4) {
      const RoboU32 p0 = *reinterpret_cast<const RoboU32*>(srcRow + static_cast<RoboU64>(x) * 4ull);
      const RoboU32 p1 = *reinterpret_cast<const RoboU32*>(srcRow + static_cast<RoboU64>(x + 1) * 4ull);
      const RoboU32 p2 = *reinterpret_cast<const RoboU32*>(srcRow + static_cast<RoboU64>(x + 2) * 4ull);
      const RoboU32 p3 = *reinterpret_cast<const RoboU32*>(srcRow + static_cast<RoboU64>(x + 3) * 4ull);
      dstRow[x] = (p0 & 0xFF00FF00u) | ((p0 & 0xFFu) << 16) | ((p0 >> 16) & 0xFFu);
      dstRow[x + 1] = (p1 & 0xFF00FF00u) | ((p1 & 0xFFu) << 16) | ((p1 >> 16) & 0xFFu);
      dstRow[x + 2] = (p2 & 0xFF00FF00u) | ((p2 & 0xFFu) << 16) | ((p2 >> 16) & 0xFFu);
      dstRow[x + 3] = (p3 & 0xFF00FF00u) | ((p3 & 0xFFu) << 16) | ((p3 >> 16) & 0xFFu);
    }
    for (; x < job->drawW; ++x) {
      const RoboU32 p = *reinterpret_cast<const RoboU32*>(srcRow + static_cast<RoboU64>(x) * 4ull);
      dstRow[x] = (p & 0xFF00FF00u) | ((p & 0xFFu) << 16) | ((p >> 16) & 0xFFu);
    }
  }
}

struct RoboGPUScaleJob {
  RoboGPU* gpu;
  const RoboU8* src;
  RoboI32 drawX;
  RoboI32 drawY;
  RoboI32 drawW;
  RoboI32 drawH;
  RoboI32 screenW;
  RoboI32 screenH;
  RoboU32 srcX0;
  RoboU32 srcY0;
  RoboU32 srcW;
  RoboU32 srcHeight;
  RoboU32 srcStride;
  RoboU32 stepX;
  RoboU32 stepY;
  RoboU32 pitch;
  RoboBool xrgb;
  RoboBool useMap;
};

static void RoboGPUScaleWorker(void* raw, RoboU32 worker, RoboU32 workers) {
  auto* job = static_cast<RoboGPUScaleJob*>(raw);
  for (RoboI32 y = static_cast<RoboI32>(worker); y < job->drawH; y += static_cast<RoboI32>(workers)) {
    const RoboI32 outY = job->drawY + y;
    if (outY < 0 || outY >= job->screenH) {
      continue;
    }
    RoboU32 sy = job->srcY0 + ((static_cast<RoboU32>(y) * job->stepY) >> 16);
    if (sy >= job->srcHeight) {
      sy = job->srcHeight - 1u;
    }
    const RoboU8* srcRow =
        job->src + static_cast<RoboU64>(sy) * job->srcStride + static_cast<RoboU64>(job->srcX0) * 4ull;
    auto* dstRow = reinterpret_cast<RoboU32*>(RoboFrameBufferRow(job->gpu->back, job->pitch, outY));
    for (RoboI32 x = 0; x < job->drawW; ++x) {
      const RoboI32 outX = job->drawX + x;
      if (outX < 0 || outX >= job->screenW) {
        continue;
      }
      const RoboU32 sx = job->useMap ? gRoboGPUScaleCol[x]
                                     : (((static_cast<RoboU32>(x) * job->stepX) >> 16) >= job->srcW
                                            ? job->srcW - 1u
                                            : ((static_cast<RoboU32>(x) * job->stepX) >> 16));
      const RoboU32 p = *reinterpret_cast<const RoboU32*>(srcRow + static_cast<RoboU64>(sx) * 4ull);
      if (job->xrgb == ROBO_TRUE) {
        dstRow[outX] = (p & 0xFF00FF00u) | ((p & 0xFFu) << 16) | ((p >> 16) & 0xFFu);
      } else {
        dstRow[outX] =
            RoboGPUPackColor(job->gpu, static_cast<RoboU8>(p & 0xFFu), static_cast<RoboU8>((p >> 8) & 0xFFu),
                             static_cast<RoboU8>((p >> 16) & 0xFFu), static_cast<RoboU8>((p >> 24) & 0xFFu));
      }
    }
  }
}

void RoboGPUBlitRGBAScaled(RoboGPU* gpu, RoboI32 dstX, RoboI32 dstY, RoboI32 dstW, RoboI32 dstH, const RoboU8* src,
                           RoboU32 srcWidth, RoboU32 srcHeight, RoboU32 srcStride, RoboU32 scaleMode) {
  if (gpu == ROBO_NULL || gpu->back == ROBO_NULL || src == ROBO_NULL || dstW <= 0 || dstH <= 0 || srcWidth == 0 ||
      srcHeight == 0) {
    return;
  }

  RoboI32 drawX = dstX;
  RoboI32 drawY = dstY;
  RoboI32 drawW = dstW;
  RoboI32 drawH = dstH;
  RoboU32 srcX0 = 0;
  RoboU32 srcY0 = 0;
  RoboU32 srcW = srcWidth;
  RoboU32 srcH = srcHeight;

  if (scaleMode == 1) {
    /* cover — crop source to destination aspect (GRUB proportional cover). */
    const RoboU64 dstAspect = static_cast<RoboU64>(dstW) * srcHeight;
    const RoboU64 srcAspect = static_cast<RoboU64>(dstH) * srcWidth;
    if (srcAspect > dstAspect) {
      srcW = static_cast<RoboU32>((static_cast<RoboU64>(dstW) * srcHeight) / static_cast<RoboU64>(dstH));
      if (srcW < 1) {
        srcW = 1;
      }
      if (srcW > srcWidth) {
        srcW = srcWidth;
      }
      srcX0 = (srcWidth - srcW) / 2;
    } else if (dstAspect > srcAspect) {
      srcH = static_cast<RoboU32>((static_cast<RoboU64>(dstH) * srcWidth) / static_cast<RoboU64>(dstW));
      if (srcH < 1) {
        srcH = 1;
      }
      if (srcH > srcHeight) {
        srcH = srcHeight;
      }
      srcY0 = (srcHeight - srcH) / 2;
    }
  } else if (scaleMode == 2) {
    /* fit — letterbox */
    const RoboU64 dstAspect = static_cast<RoboU64>(dstW) * srcHeight;
    const RoboU64 srcAspect = static_cast<RoboU64>(dstH) * srcWidth;
    if (srcAspect > dstAspect) {
      drawH = static_cast<RoboI32>((static_cast<RoboU64>(dstW) * srcHeight) / srcWidth);
      if (drawH < 1) {
        drawH = 1;
      }
      drawY = dstY + (dstH - drawH) / 2;
    } else {
      drawW = static_cast<RoboI32>((static_cast<RoboU64>(dstH) * srcWidth) / srcHeight);
      if (drawW < 1) {
        drawW = 1;
      }
      drawX = dstX + (dstW - drawW) / 2;
    }
    RoboGPUFillRect(gpu, dstX, dstY, dstW, dstH, RoboGPUPackColor(gpu, 0, 0, 0, 255));
  }

  const RoboU32 pitch = RoboGPUPitch(gpu);
  const RoboI32 screenW = static_cast<RoboI32>(gpu->info.width);
  const RoboI32 screenH = static_cast<RoboI32>(gpu->info.height);
  const RoboBool xrgb = (gpu->info.redMask == 0x00FF0000u && gpu->info.greenMask == 0x0000FF00u &&
                         gpu->info.blueMask == 0x000000FFu)
                            ? ROBO_TRUE
                            : ((gpu->info.redMask == 0 && gpu->info.greenMask == 0 && gpu->info.blueMask == 0)
                                   ? ROBO_TRUE
                                   : ROBO_FALSE);

  const RoboU32 stepX = static_cast<RoboU32>((static_cast<RoboU64>(srcW) << 16) / static_cast<RoboU64>(drawW));
  const RoboU32 stepY = static_cast<RoboU32>((static_cast<RoboU64>(srcH) << 16) / static_cast<RoboU64>(drawH));

  /*
   * Native HD: source pixel == screen pixel. Pack RGBA to XRGB and store.
   * No resample, so edges stay as sharp as the cooked frame.
   */
  if (xrgb && stepX == 0x10000u && stepY == 0x10000u && drawX >= 0 && drawY >= 0 &&
      drawX + drawW <= screenW && drawY + drawH <= screenH && drawW == static_cast<RoboI32>(srcW) &&
      drawH == static_cast<RoboI32>(srcH)) {
    RoboGPUCopyJob copyJob{gpu, src, drawX, drawY, drawW, drawH, srcX0, srcY0, srcStride, pitch};
    RoboGPUForSpans(static_cast<RoboU32>(drawH), RoboGPUCopyWorker, &copyJob);
    RoboGPUMarkDamage(gpu, drawX, drawY, drawW, drawH);
    return;
  }

  /*
   * Screens that are not the cooked size: one source pixel per destination pixel.
   * The column map is built once so a wallpaper frame is a straight load and store.
   */
  constexpr RoboI32 ROBO_SCALE_MAX = ROBO_GPU_SCALE_MAX;
  static RoboU32 scaleStepX = 0;
  static RoboU32 scaleSrcW = 0;
  static RoboI32 scaleDrawW = 0;
  const RoboBool useMap = (drawW > 0 && drawW <= ROBO_SCALE_MAX && srcW <= 65535u) ? ROBO_TRUE : ROBO_FALSE;
  if (useMap && (scaleStepX != stepX || scaleSrcW != srcW || scaleDrawW != drawW)) {
    for (RoboI32 x = 0; x < drawW; ++x) {
      RoboU32 sx = (static_cast<RoboU32>(x) * stepX) >> 16;
      if (sx >= srcW) {
        sx = srcW - 1u;
      }
      gRoboGPUScaleCol[x] = static_cast<RoboU16>(sx);
    }
    scaleStepX = stepX;
    scaleSrcW = srcW;
    scaleDrawW = drawW;
  }

  RoboGPUScaleJob job{gpu,           src,       drawX,    drawY,     drawW,     drawH,    screenW,   screenH, srcX0,
                      srcY0,         srcW,      srcHeight, srcStride, stepX,     stepY,    pitch,     xrgb,    useMap};
  RoboGPUForSpans(static_cast<RoboU32>(drawH), RoboGPUScaleWorker, &job);
  RoboGPUMarkDamage(gpu, drawX, drawY, drawW, drawH);
}

void RoboGPUBlitImageProportional(RoboGPU* gpu, const RoboImageView& image, RoboI32 boxX, RoboI32 boxY, RoboI32 boxW,
                                  RoboI32 boxH) {
  if (gpu == ROBO_NULL || image.header == ROBO_NULL || image.pixels == ROBO_NULL || boxW <= 0 || boxH <= 0) {
    return;
  }
  const float srcAspect = static_cast<float>(image.header->width) / static_cast<float>(image.header->height);
  const float boxAspect = static_cast<float>(boxW) / static_cast<float>(boxH);
  RoboI32 drawW = boxW;
  RoboI32 drawH = boxH;
  if (srcAspect > boxAspect) {
    drawH = static_cast<RoboI32>(static_cast<float>(boxW) / srcAspect);
  } else {
    drawW = static_cast<RoboI32>(static_cast<float>(boxH) * srcAspect);
  }
  if (drawW < 1) {
    drawW = 1;
  }
  if (drawH < 1) {
    drawH = 1;
  }
  const RoboI32 drawX = boxX + (boxW - drawW) / 2;
  const RoboI32 drawY = boxY + (boxH - drawH) / 2;
  const RoboU32 pitch = RoboGPUPitch(gpu);

  for (RoboI32 y = 0; y < drawH; ++y) {
    const RoboI32 outY = drawY + y;
    if (outY < 0 || outY >= static_cast<RoboI32>(gpu->info.height)) {
      continue;
    }
    auto* dstRow = reinterpret_cast<RoboU32*>(RoboFrameBufferRow(gpu->back, pitch, outY));
    const float v = (drawH == 1) ? 0.0f : (static_cast<float>(y) + 0.5f) / static_cast<float>(drawH);
    for (RoboI32 x = 0; x < drawW; ++x) {
      const RoboI32 outX = drawX + x;
      if (outX < 0 || outX >= static_cast<RoboI32>(gpu->info.width)) {
        continue;
      }
      const float u = (drawW == 1) ? 0.0f : (static_cast<float>(x) + 0.5f) / static_cast<float>(drawW);
      RoboU8 r = 0, g = 0, b = 0, a = 0;
      RoboGPUSampleRGBABilinear(image.pixels, image.header->width, image.header->height, image.header->stride, u, v, &r,
                                &g, &b, &a);
      if (a == 0) {
        continue;
      }
      dstRow[outX] = RoboGPUPackColor(gpu, r, g, b, a);
    }
  }
  RoboGPUMarkDamage(gpu, drawX, drawY, drawW, drawH);
}

void RoboGPUSetCursor(RoboGPU* gpu, const void* cursor, RoboCursorBlendRowFn blendFn) {
  if (gpu == ROBO_NULL) {
    return;
  }
  gpu->cursor = cursor;
  gpu->cursorBlendRow = blendFn;
}

/* drm_fb_memcpy — row blit one clipped rect from back → front with atomic cursor composition. */
struct RoboGPUFlushJob {
  RoboGPU* gpu;
  RoboI32 x;
  RoboI32 y;
  RoboI32 width;
  RoboI32 height;
  RoboU32 pitch;
  RoboU32 cpp;
  RoboU64 xOff;
};

static void RoboGPUFlushWorker(void* raw, RoboU32 worker, RoboU32 workers) {
  auto* job = static_cast<RoboGPUFlushJob*>(raw);
  const RoboSize rowBytes = static_cast<RoboSize>(job->width) * job->cpp;
  for (RoboI32 row = static_cast<RoboI32>(worker); row < job->height; row += static_cast<RoboI32>(workers)) {
    const RoboI32 curY = job->y + row;
    RoboU8* dst = RoboFrameBufferRow(job->gpu->front, job->pitch, curY) + job->xOff;
    const RoboU8* src = RoboFrameBufferRowConst(job->gpu->back, job->pitch, curY) + job->xOff;
    if (job->gpu->cursor != ROBO_NULL && job->gpu->cursorBlendRow != ROBO_NULL) {
      job->gpu->cursorBlendRow(job->gpu->cursor, &job->gpu->info, reinterpret_cast<const RoboU32*>(src),
                               reinterpret_cast<RoboU32*>(dst), curY, job->x, job->x + job->width);
    } else {
      RoboScanoutCopy(dst, src, rowBytes);
    }
  }
}

static void RoboGPUFlushRect(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height) {
  RoboFrameBufferClipRect(&x, &y, &width, &height, static_cast<RoboI32>(gpu->info.width),
                          static_cast<RoboI32>(gpu->info.height));
  if (width <= 0 || height <= 0 || gpu->front == ROBO_NULL || gpu->back == ROBO_NULL) {
    return;
  }
  RoboGPUFlushJob job{gpu, x, y, width, height, RoboGPUPitch(gpu), RoboFrameBufferCPP(&gpu->info),
                      static_cast<RoboU64>(x) * RoboFrameBufferCPP(&gpu->info)};
  RoboGPUForSpans(static_cast<RoboU32>(height), RoboGPUFlushWorker, &job);
  if (gRoboGPUScanout != ROBO_NULL) {
    gRoboGPUScanout(gpu, x, y, width, height);
  }
}

void RoboGPUCopyBackToFront(RoboGPU* gpu) {
  if (gpu == ROBO_NULL) {
    return;
  }
  RoboGPUFlushRect(gpu, 0, 0, static_cast<RoboI32>(gpu->info.width), static_cast<RoboI32>(gpu->info.height));
}

void RoboGPUPresentFull(RoboGPU* gpu) {
  RoboGPUCopyBackToFront(gpu);
  RoboDamageClear(&gpu->damage);
}

/*
 * Hyper-V / drm_fb_memcpy Present: flush each damage clip (or one merged AABB /
 * full-frame on overflow). Drawing stays on cached back; front only gets row memcpy.
 */
void RoboGPUPresent(RoboGPU* gpu) {
  if (gpu == ROBO_NULL || RoboDamageIsEmpty(&gpu->damage)) {
    return;
  }
  if (gpu->damage.full) {
    RoboGPUPresentFull(gpu);
    return;
  }

  /* Prefer per-rect flush when few clips; merge to AABB when many (less GOP traffic). */
  if (gpu->damage.count > 8) {
    RoboDamageRect merged{};
    if (RoboDamageMerge(&gpu->damage, &merged)) {
      RoboGPUFlushRect(gpu, merged.x, merged.y, merged.width, merged.height);
    }
  } else {
    for (RoboU32 i = 0; i < gpu->damage.count; ++i) {
      const RoboDamageRect& r = gpu->damage.rects[i];
      RoboGPUFlushRect(gpu, r.x, r.y, r.width, r.height);
    }
  }
  RoboDamageClear(&gpu->damage);
}
