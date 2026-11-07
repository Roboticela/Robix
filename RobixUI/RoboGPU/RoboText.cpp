#include "RoboText.hpp"

#include "../RoboShared/RoboUTF8.hpp"

static const RoboTextHostRaster* gRoboTextHostRaster = ROBO_NULL;

void RoboTextSetHostRaster(const RoboTextHostRaster* raster) { gRoboTextHostRaster = raster; }

static RoboI32 RoboTextScaleI(RoboI32 value, RoboU16 sizePx, RoboU16 baseSize) {
  if (baseSize == 0 || sizePx == 0 || sizePx == baseSize) {
    return value;
  }
  /* Round-to-nearest so advances stay tight at HD sizes. */
  if (value >= 0) {
    return (value * static_cast<RoboI32>(sizePx) + static_cast<RoboI32>(baseSize / 2)) /
           static_cast<RoboI32>(baseSize);
  } else {
    return (value * static_cast<RoboI32>(sizePx) - static_cast<RoboI32>(baseSize / 2)) /
           static_cast<RoboI32>(baseSize);
  }
}

RoboI32 RoboTextMeasureCode(const RoboFontView& font, RoboU32 codepoint, RoboU16 sizePx) {
  if (gRoboTextHostRaster != ROBO_NULL && gRoboTextHostRaster->Measure != ROBO_NULL) {
    return gRoboTextHostRaster->Measure(&font, codepoint, sizePx);
  }
  if (font.header == ROBO_NULL) {
    return 0;
  }
  const RoboU16 base = RoboFontBaseSize(font);
  const RoboU16 size = sizePx == 0 ? base : sizePx;
  if (codepoint == '\n' || codepoint == '\r') {
    return 0;
  }
  if (codepoint == '\t') {
    return RoboTextMeasureCode(font, ' ', size) * 4;
  }
  const auto* glyph = RoboFontFindGlyph(font, codepoint);
  if (glyph == ROBO_NULL) {
    glyph = RoboFontFindGlyph(font, ROBO_UNICODE_REPLACEMENT);
  }
  const RoboI32 track = 0;
  if (glyph != ROBO_NULL) {
    return RoboTextScaleI(glyph->advance, size, base) + track;
  }
  return RoboTextScaleI(static_cast<RoboI32>(font.header->ascent / 2), size, base) + track;
}

RoboI32 RoboTextMeasureUTF8(const RoboFontView& font, const char* text, RoboU16 sizePx) {
  if (text == ROBO_NULL || font.header == ROBO_NULL) {
    return 0;
  }
  RoboI32 width = 0;
  const char* p = text;
  for (;;) {
    const RoboU32 code = RoboUTF8Next(&p);
    if (code == 0) {
      break;
    }
    if (code == '\n') {
      break;
    }
    width += RoboTextMeasureCode(font, code, sizePx);
  }
  return width;
}

static void RoboTextBlitGlyph(RoboGPU* gpu, const RoboFontView& font, const RoboFontGlyph* glyph, RoboI32 dstX,
                              RoboI32 dstY, RoboU16 sizePx, RoboU16 baseSize, RoboU8 r, RoboU8 g, RoboU8 b) {
  if (gpu == ROBO_NULL || glyph == ROBO_NULL || glyph->width == 0 || glyph->height == 0) {
    return;
  }
  const RoboU8* bitmap = font.bitmaps + glyph->bitmapOffset;
  const RoboI32 outW = RoboTextScaleI(static_cast<RoboI32>(glyph->width), sizePx, baseSize);
  const RoboI32 outH = RoboTextScaleI(static_cast<RoboI32>(glyph->height), sizePx, baseSize);
  if (outW <= 0 || outH <= 0) {
    return;
  }
  /* Area-averaged box downsampling or bilinear upsampling with src-over blend */
  RoboGPUBlitCoverageScaled(gpu, dstX, dstY, outW, outH, bitmap, glyph->width, glyph->height, r, g, b);
}

void RoboTextDrawUTF8Baseline(RoboGPU* gpu, const RoboFontView& font, RoboI32 x, RoboI32 baselineY, const char* text,
                              RoboU32 colorARGB, RoboU16 sizePx) {
  if (gpu == ROBO_NULL || font.header == ROBO_NULL || text == ROBO_NULL) {
    return;
  }
  const RoboU16 base = RoboFontBaseSize(font);
  const RoboU16 size = sizePx == 0 ? base : sizePx;
  const RoboU8 r = static_cast<RoboU8>((colorARGB >> 16) & 0xFF);
  const RoboU8 g = static_cast<RoboU8>((colorARGB >> 8) & 0xFF);
  const RoboU8 b = static_cast<RoboU8>(colorARGB & 0xFF);
  RoboI32 cursorX = x;
  const char* p = text;
  for (;;) {
    const RoboU32 code = RoboUTF8Next(&p);
    if (code == 0) {
      break;
    }
    if (code == '\n') {
      break;
    }
    if (code == '\t') {
      cursorX += RoboTextMeasureCode(font, ' ', size) * 4;
      continue;
    }
    const auto* glyph = RoboFontFindGlyph(font, code);
    if (glyph == ROBO_NULL) {
      glyph = RoboFontFindGlyph(font, ROBO_UNICODE_REPLACEMENT);
    }
    if (gRoboTextHostRaster != ROBO_NULL && gRoboTextHostRaster->Draw != ROBO_NULL) {
      gRoboTextHostRaster->Draw(gpu, &font, cursorX, baselineY, code, colorARGB, size);
      cursorX += RoboTextMeasureCode(font, code, size);
      continue;
    }
    if (glyph == ROBO_NULL) {
      cursorX += RoboTextMeasureCode(font, code, size);
      continue;
    }
    const RoboI32 bearingX = RoboTextScaleI(glyph->bearingX, size, base);
    const RoboI32 bearingY = RoboTextScaleI(glyph->bearingY, size, base);
    RoboTextBlitGlyph(gpu, font, glyph, cursorX + bearingX, baselineY - bearingY, size, base, r, g, b);
    const RoboI32 track = 0;
    cursorX += RoboTextScaleI(glyph->advance, size, base) + track;
  }
}

void RoboTextDrawUTF8(RoboGPU* gpu, const RoboFontView& font, RoboI32 x, RoboI32 y, const char* text,
                      RoboU32 colorARGB, RoboU16 sizePx) {
  if (gpu == ROBO_NULL || font.header == ROBO_NULL || text == ROBO_NULL) {
    return;
  }
  const RoboU16 base = RoboFontBaseSize(font);
  const RoboU16 size = sizePx == 0 ? base : sizePx;
  const RoboI32 scaledAscent = RoboTextScaleI(static_cast<RoboI32>(font.header->ascent), size, base);
  const RoboI32 baselineY = y + scaledAscent;
  RoboTextDrawUTF8Baseline(gpu, font, x, baselineY, text, colorARGB, sizePx);
}

RoboI32 RoboTextMeasureWidth(const RoboFontView& font, const char* text) {
  return RoboTextMeasureUTF8(font, text, 0);
}

void RoboTextDrawString(RoboGPU* gpu, const RoboFontView& font, RoboI32 x, RoboI32 y, const char* text,
                        RoboU32 colorARGB) {
  RoboTextDrawUTF8(gpu, font, x, y, text, colorARGB, 0);
}
