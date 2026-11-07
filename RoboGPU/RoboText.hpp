#pragma once

#include "../RobixUI/Foundation/FoundationPlatform.hpp"

struct RoboGPU {
  std::uint8_t* pixels = nullptr;
  RoboI32 width = 0;
  RoboI32 height = 0;
  RoboI32 stride = 0;
};

struct RoboFontHeader {
  RoboI32 ascent = 0;
  RoboI32 descent = 0;
  RoboI32 lineGap = 0;
  RoboU16 units = 0;
  RoboU16 face = 0;
};

struct RoboFontView {
  const RoboFontHeader* header = nullptr;
};

struct RoboFontPackView {
  const RoboFontHeader* header = nullptr;
};

constexpr RoboU16 ROBO_FONT_MASTER_PX = 16;

RoboFontView RoboFontForName(const char* name);
RoboFontView RoboFontPackFindSize(const RoboFontPackView& pack, const char* faceName, RoboU16 sizePx);
RoboU16 RoboFontBaseSize(const RoboFontView& face);
RoboI32 RoboTextMeasureCode(const RoboFontView& face, RoboU32 code, RoboU16 sizePx);
void RoboTextDrawUTF8Baseline(RoboGPU* gpu, const RoboFontView& face, RoboI32 x, RoboI32 baselineY, const char* utf8,
                              RoboU32 colorARGB, RoboU16 sizePx);
