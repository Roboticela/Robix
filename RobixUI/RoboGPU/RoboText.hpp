#pragma once

#include "../RoboShared/RoboFont.hpp"
#include "RoboGPU.hpp"

/* UTF-8 text draw / measure — atlas glyphs scaled to sizePx (0 = font base size). */
/* (x, y) specifies the top-left coordinate of the text line. */
void RoboTextDrawUTF8(RoboGPU* gpu, const RoboFontView& font, RoboI32 x, RoboI32 y, const char* text,
                      RoboU32 colorARGB, RoboU16 sizePx);

/* Draw text with an explicit typographic baseline Y coordinate. */
void RoboTextDrawUTF8Baseline(RoboGPU* gpu, const RoboFontView& font, RoboI32 x, RoboI32 baselineY, const char* text,
                              RoboU32 colorARGB, RoboU16 sizePx);

RoboI32 RoboTextMeasureUTF8(const RoboFontView& font, const char* text, RoboU16 sizePx);
RoboI32 RoboTextMeasureCode(const RoboFontView& font, RoboU32 codepoint, RoboU16 sizePx);

/* Windows host can draw the outline at the exact pixel size. The OS leaves this unset. */
struct RoboTextHostRaster {
  RoboI32 (*Measure)(const RoboFontView* font, RoboU32 codepoint, RoboU16 sizePx);
  void (*Draw)(RoboGPU* gpu, const RoboFontView* font, RoboI32 x, RoboI32 baselineY, RoboU32 codepoint,
               RoboU32 colorARGB, RoboU16 sizePx);
};

void RoboTextSetHostRaster(const RoboTextHostRaster* raster);

/* Legacy ASCII helpers (byte = Latin-1) — prefer UTF-8 APIs. */
void RoboTextDrawString(RoboGPU* gpu, const RoboFontView& font, RoboI32 x, RoboI32 y, const char* text,
                        RoboU32 colorARGB);
RoboI32 RoboTextMeasureWidth(const RoboFontView& font, const char* text);
