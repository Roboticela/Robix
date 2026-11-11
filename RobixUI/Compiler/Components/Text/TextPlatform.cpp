#include "TextPlatform.hpp"

#include "../../Foundation/FoundationPlatform.hpp"
#include "../../../RoboGPU/RoboText.hpp"
#include "../../../RoboShared/RoboUTF8.hpp"

static RoboU8 RoboUITextHexNibble(char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<RoboU8>(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return static_cast<RoboU8>(10 + c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return static_cast<RoboU8>(10 + c - 'A');
  }
  return 0;
}

static RoboU32 RoboUITextParseColor(const char* text) {
  if (text == ROBO_NULL || text[0] != '#') {
    return 0xFFFFFFFFu;
  }
  RoboSize len = RoboStringLength(text + 1);
  RoboU8 a = 255, r = 0, g = 0, b = 0;
  if (len == 3) {
    r = static_cast<RoboU8>(RoboUITextHexNibble(text[1]) * 17);
    g = static_cast<RoboU8>(RoboUITextHexNibble(text[2]) * 17);
    b = static_cast<RoboU8>(RoboUITextHexNibble(text[3]) * 17);
  } else if (len == 6) {
    r = static_cast<RoboU8>((RoboUITextHexNibble(text[1]) << 4) | RoboUITextHexNibble(text[2]));
    g = static_cast<RoboU8>((RoboUITextHexNibble(text[3]) << 4) | RoboUITextHexNibble(text[4]));
    b = static_cast<RoboU8>((RoboUITextHexNibble(text[5]) << 4) | RoboUITextHexNibble(text[6]));
  } else if (len == 8) {
    a = static_cast<RoboU8>((RoboUITextHexNibble(text[1]) << 4) | RoboUITextHexNibble(text[2]));
    r = static_cast<RoboU8>((RoboUITextHexNibble(text[3]) << 4) | RoboUITextHexNibble(text[4]));
    g = static_cast<RoboU8>((RoboUITextHexNibble(text[5]) << 4) | RoboUITextHexNibble(text[6]));
    b = static_cast<RoboU8>((RoboUITextHexNibble(text[7]) << 4) | RoboUITextHexNibble(text[8]));
  }
  return (static_cast<RoboU32>(a) << 24) | (static_cast<RoboU32>(r) << 16) | (static_cast<RoboU32>(g) << 8) |
         static_cast<RoboU32>(b);
}

static RoboBool RoboUITextEq(const char* a, const char* b) {
  if (a == ROBO_NULL || b == ROBO_NULL) {
    return ROBO_FALSE;
  }
  while (*a != 0 && *b != 0 && *a == *b) {
    ++a;
    ++b;
  }
  return (*a == 0 && *b == 0) ? ROBO_TRUE : ROBO_FALSE;
}

static RoboU16 RoboUITextParseU16(const char* value, RoboU16 fallback) {
  if (value == ROBO_NULL || value[0] == 0) {
    return fallback;
  }
  RoboU32 n = 0;
  for (RoboSize i = 0; value[i] != 0; ++i) {
    if (value[i] < '0' || value[i] > '9') {
      break;
    }
    n = n * 10u + static_cast<RoboU32>(value[i] - '0');
    if (n > 0xFFFFu) {
      return 0xFFFF;
    }
  }
  return static_cast<RoboU16>(n);
}

static RoboI32 RoboUITextParseI32(const char* value, RoboI32 fallback) {
  if (value == ROBO_NULL || value[0] == 0) {
    return fallback;
  }
  RoboBool neg = ROBO_FALSE;
  RoboSize i = 0;
  if (value[0] == '-') {
    neg = ROBO_TRUE;
    i = 1;
  }
  RoboI32 n = 0;
  for (; value[i] != 0; ++i) {
    if (value[i] < '0' || value[i] > '9') {
      break;
    }
    n = n * 10 + (value[i] - '0');
  }
  return neg ? -n : n;
}

static RoboU16 RoboUITextSpanSize(const RoboUIText* text, const RoboTextSpan* span) {
  if (span != ROBO_NULL && span->sizePx != 0) {
    return span->sizePx;
  }
  if (text != ROBO_NULL && text->sizePx != 0) {
    return text->sizePx;
  }
  return text != ROBO_NULL ? RoboFontBaseSize(text->font) : static_cast<RoboU16>(36);
}

static RoboU32 RoboUITextSpanColor(const RoboUIText* text, const RoboTextSpan* span) {
  if (span != ROBO_NULL && span->colorARGB != 0) {
    return span->colorARGB;
  }
  return text != ROBO_NULL ? text->colorARGB : 0xFFFFFFFFu;
}

static const RoboTextSpan* RoboUITextSpanAt(const RoboUIText* text, RoboU32 byteIndex) {
  if (text == ROBO_NULL) {
    return ROBO_NULL;
  }
  for (RoboU16 i = 0; i < text->spanCount; ++i) {
    const RoboTextSpan* span = &text->spans[i];
    if (byteIndex >= span->byteStart && byteIndex < span->byteStart + span->byteLength) {
      return span;
    }
  }
  return ROBO_NULL;
}

static RoboU32 RoboUITextCase(const RoboUIText* text, RoboU32 code) {
  if (text == ROBO_NULL || text->uppercase != ROBO_TRUE) {
    return code;
  }
  if (code >= 'a' && code <= 'z') {
    return code - ('a' - 'A');
  }
  if (code >= 0xE0 && code <= 0xFE && code != 0xF7) {
    return code - 0x20;
  }
  return code;
}

static RoboI32 RoboUITextTrackingPx(const RoboUIText* text, RoboU16 sizePx) {
  if (text == ROBO_NULL || text->trackingMilli == 0 || sizePx == 0) {
    return 0;
  }
  return (static_cast<RoboI32>(sizePx) * static_cast<RoboI32>(text->trackingMilli)) / 1000;
}

static RoboFontView RoboUITextFace(const RoboUIText* text, RoboU16 sizePx) {
  if (text != ROBO_NULL && text->fontPack.header != ROBO_NULL && text->faceName[0] != 0) {
    const RoboFontView strike = RoboFontPackFindSize(text->fontPack, text->faceName, sizePx);
    if (strike.header != ROBO_NULL) {
      return strike;
    }
  }
  if (text != ROBO_NULL) {
    return text->font;
  }
  return RoboFontView{};
}

static RoboI32 RoboUITextAdvance(const RoboUIText* text, RoboU32 code, RoboU16 sizePx) {
  const RoboU32 shown = RoboUITextCase(text, code);
  const RoboFontView face = RoboUITextFace(text, sizePx);
  return RoboTextMeasureCode(face, shown, sizePx) + RoboUITextTrackingPx(text, sizePx);
}

static RoboU32 RoboUITextEncode(RoboU32 code, char* out) {
  if (code < 0x80u) {
    out[0] = static_cast<char>(code);
    out[1] = 0;
    return 1;
  }
  if (code < 0x800u) {
    out[0] = static_cast<char>(0xC0u | (code >> 6));
    out[1] = static_cast<char>(0x80u | (code & 0x3Fu));
    out[2] = 0;
    return 2;
  }
  out[0] = static_cast<char>(0xE0u | (code >> 12));
  out[1] = static_cast<char>(0x80u | ((code >> 6) & 0x3Fu));
  out[2] = static_cast<char>(0x80u | (code & 0x3Fu));
  out[3] = 0;
  return 3;
}

static RoboI32 RoboUITextLineHeight(const RoboUIText* text, RoboU16 sizePx) {
  if (text != ROBO_NULL && text->leadingMilli > 0 && sizePx > 0) {
    return (static_cast<RoboI32>(sizePx) * static_cast<RoboI32>(text->leadingMilli) + 500) / 1000;
  }
  const RoboFontView face = RoboUITextFace(text, sizePx);
  if (text == ROBO_NULL || face.header == ROBO_NULL) {
    return static_cast<RoboI32>(sizePx) + 4;
  }
  const RoboU16 base = RoboFontBaseSize(face);
  const RoboI32 ascent = face.header->ascent;
  const RoboI32 descent = face.header->descent;
  const RoboI32 gap = face.header->lineGap;
  const RoboI32 raw = ascent + descent + gap;
  if (base == 0 || sizePx == base) {
    return raw > 0 ? raw : static_cast<RoboI32>(sizePx) + 4;
  }
  return (raw * static_cast<RoboI32>(sizePx) + static_cast<RoboI32>(base / 2)) / static_cast<RoboI32>(base);
}

struct RoboUITextLayoutLine {
  RoboU32 byteStart;
  RoboU32 byteEnd;
  RoboI32 width;
  RoboU16 maxSizePx;
};

constexpr RoboU32 ROBO_TEXT_MAX_LINES = 96;

static RoboI32 RoboUITextMeasureRange(const RoboUIText* text, RoboU32 byteStart, RoboU32 byteEnd, RoboU16* outMaxSize) {
  RoboI32 width = 0;
  RoboU16 maxSize = 0;
  RoboU32 i = byteStart;
  while (i < byteEnd && i < text->textLength) {
    const char* p = text->text + i;
    const char* start = p;
    const RoboU32 code = RoboUTF8Next(&p);
    if (code == 0) {
      break;
    }
    const RoboU32 consumed = static_cast<RoboU32>(p - start);
    const RoboTextSpan* span = RoboUITextSpanAt(text, i);
    const RoboU16 size = RoboUITextSpanSize(text, span);
    if (size > maxSize) {
      maxSize = size;
    }
    if (code != '\n' && code != '\r') {
      width += RoboUITextAdvance(text, code, size);
    }
    i += consumed == 0 ? 1 : consumed;
  }
  if (outMaxSize != ROBO_NULL) {
    *outMaxSize = maxSize == 0 ? RoboUITextSpanSize(text, ROBO_NULL) : maxSize;
  }
  return width;
}

static void RoboUITextFlushLine(const RoboUIText* text, RoboUITextLayoutLine* lines, RoboU32* lineCount,
                                RoboU32 maxLines, RoboU32* lineStart, RoboU32 end, RoboI32* lineWidth,
                                RoboU16* lineMaxSize, RoboU32* lastBreak) {
  if (*lineCount >= maxLines) {
    return;
  }
  RoboUITextLayoutLine& line = lines[(*lineCount)++];
  line.byteStart = *lineStart;
  line.byteEnd = end;
  RoboU16 maxSize = 0;
  line.width = RoboUITextMeasureRange(text, *lineStart, end, &maxSize);
  line.maxSizePx = maxSize;
  *lineStart = end;
  *lineWidth = 0;
  *lineMaxSize = 0;
  *lastBreak = *lineStart;
}

static RoboU32 RoboUITextLayout(const RoboUIText* text, RoboUITextLayoutLine* lines, RoboU32 maxLines) {
  if (text == ROBO_NULL || lines == ROBO_NULL || maxLines == 0 || text->textLength == 0) {
    return 0;
  }
  const RoboI32 boxW = text->maxWidth > 0 ? text->maxWidth : (text->base.width > 0 ? text->base.width : 0);
  RoboU32 lineCount = 0;
  RoboU32 lineStart = 0;
  RoboU32 i = 0;
  RoboI32 lineWidth = 0;
  RoboU16 lineMaxSize = 0;
  RoboU32 lastBreak = 0;

  while (i < text->textLength && lineCount < maxLines) {
    const char* p = text->text + i;
    const char* start = p;
    const RoboU32 code = RoboUTF8Next(&p);
    if (code == 0) {
      break;
    }
    const RoboU32 consumed = static_cast<RoboU32>(p - start);
    if (consumed == 0) {
      break;
    }
    if (code == '\n') {
      RoboUITextFlushLine(text, lines, &lineCount, maxLines, &lineStart, i, &lineWidth, &lineMaxSize, &lastBreak);
      i += consumed;
      lineStart = i;
      continue;
    }
    const RoboTextSpan* span = RoboUITextSpanAt(text, i);
    const RoboU16 size = RoboUITextSpanSize(text, span);
    const RoboI32 adv = RoboUITextAdvance(text, code, size);
    const RoboBool isSpace = RoboUTF8IsSpace(code);

    if (text->wrap != RoboTextWrap::None && boxW > 0 && lineWidth + adv > boxW && i > lineStart) {
      if (text->wrap == RoboTextWrap::Word && lastBreak > lineStart) {
        RoboUITextFlushLine(text, lines, &lineCount, maxLines, &lineStart, lastBreak, &lineWidth, &lineMaxSize,
                            &lastBreak);
        i = lineStart;
        while (i < text->textLength) {
          const char* sp = text->text + i;
          const char* ss = sp;
          const RoboU32 sc = RoboUTF8Next(&sp);
          if (sc == 0 || !RoboUTF8IsSpace(sc)) {
            break;
          }
          i += static_cast<RoboU32>(sp - ss);
        }
        lineStart = i;
        continue;
      }
      RoboUITextFlushLine(text, lines, &lineCount, maxLines, &lineStart, i, &lineWidth, &lineMaxSize, &lastBreak);
      continue;
    }

    lineWidth += adv;
    if (size > lineMaxSize) {
      lineMaxSize = size;
    }
    if (isSpace || text->wrap == RoboTextWrap::Char) {
      lastBreak = i + consumed;
    }
    i += consumed;
  }
  if (lineStart < text->textLength && lineCount < maxLines) {
    RoboUITextFlushLine(text, lines, &lineCount, maxLines, &lineStart, text->textLength, &lineWidth, &lineMaxSize,
                        &lastBreak);
  }
  return lineCount;
}

static void RoboUITextDrawRange(RoboGPU* gpu, const RoboUIText* text, RoboU32 byteStart, RoboU32 byteEnd, RoboI32 x,
                                RoboI32 baselineY) {
  RoboU32 i = byteStart;
  RoboI32 cursorX = x;
  while (i < byteEnd && i < text->textLength) {
    const char* p = text->text + i;
    const char* start = p;
    const RoboU32 code = RoboUTF8Next(&p);
    if (code == 0) {
      break;
    }
    const RoboU32 consumed = static_cast<RoboU32>(p - start);
    if (consumed == 0) {
      break;
    }
    const RoboTextSpan* span = RoboUITextSpanAt(text, i);
    const RoboU16 size = RoboUITextSpanSize(text, span);
    const RoboU32 color = RoboUITextSpanColor(text, span);
    if (code != '\n' && code != '\r') {
      char one[8];
      const RoboU32 shown = RoboUITextCase(text, code);
      RoboUITextEncode(shown, one);
      RoboTextDrawUTF8Baseline(gpu, RoboUITextFace(text, size), cursorX, baselineY, one, color, size);
      cursorX += RoboUITextAdvance(text, code, size);
    }
    i += consumed;
  }
}

static RoboI32 RoboUITextScaledMetric(const RoboUIText* text, RoboU16 sizePx, RoboI32 metric) {
  const RoboFontView face = RoboUITextFace(text, sizePx);
  if (text == ROBO_NULL || face.header == ROBO_NULL) {
    return metric > 0 ? metric : static_cast<RoboI32>(sizePx);
  }
  const RoboU16 base = RoboFontBaseSize(face);
  if (base == 0 || sizePx == base) {
    return metric;
  }
  return (metric * static_cast<RoboI32>(sizePx) + static_cast<RoboI32>(base / 2)) / static_cast<RoboI32>(base);
}

static RoboI32 RoboUITextScaledAscent(const RoboUIText* text, RoboU16 sizePx) {
  const RoboFontView face = RoboUITextFace(text, sizePx);
  if (text == ROBO_NULL || face.header == ROBO_NULL) {
    return static_cast<RoboI32>(sizePx);
  }
  return RoboUITextScaledMetric(text, sizePx, static_cast<RoboI32>(face.header->ascent));
}

static RoboI32 RoboUITextScaledDescent(const RoboUIText* text, RoboU16 sizePx) {
  const RoboFontView face = RoboUITextFace(text, sizePx);
  if (text == ROBO_NULL || face.header == ROBO_NULL) {
    return 0;
  }
  return RoboUITextScaledMetric(text, sizePx, static_cast<RoboI32>(face.header->descent));
}

static void RoboUITextPaint(RoboUIComponent* self, RoboGPU* gpu, RoboI32 absX, RoboI32 absY) {
  auto* text = reinterpret_cast<RoboUIText*>(self);
  if (text == ROBO_NULL || gpu == ROBO_NULL || text->font.header == ROBO_NULL || text->textLength == 0) {
    return;
  }
  RoboUITextLayoutLine lines[ROBO_TEXT_MAX_LINES];
  const RoboU32 lineCount = RoboUITextLayout(text, lines, ROBO_TEXT_MAX_LINES);
  const RoboI32 boxW = text->maxWidth > 0 ? text->maxWidth : (text->base.width > 0 ? text->base.width : 0);
  RoboI32 penY = absY;
  for (RoboU32 li = 0; li < lineCount; ++li) {
    const RoboUITextLayoutLine& line = lines[li];
    const RoboI32 lineH = RoboUITextLineHeight(text, line.maxSizePx);
    const RoboI32 ascent = RoboUITextScaledAscent(text, line.maxSizePx);
    const RoboI32 content = ascent + RoboUITextScaledDescent(text, line.maxSizePx);
    const RoboI32 halfLead = lineH > content ? (lineH - content) / 2 : 0;
    const RoboI32 baseline = penY + halfLead + ascent;
    RoboI32 x = absX;
    if (text->align == RoboTextAlign::Center && boxW > 0) {
      x = absX + (boxW - line.width) / 2;
    } else if (text->align == RoboTextAlign::Right && boxW > 0) {
      x = absX + (boxW - line.width);
    }
    RoboUITextDrawRange(gpu, text, line.byteStart, line.byteEnd, x, baseline);
    penY += lineH;
  }
}

void RoboUITextMeasure(RoboUIText* text, RoboI32* outWidth, RoboI32* outHeight) {
  if (outWidth != ROBO_NULL) {
    *outWidth = 0;
  }
  if (outHeight != ROBO_NULL) {
    *outHeight = 0;
  }
  if (text == ROBO_NULL || text->textLength == 0) {
    return;
  }
  RoboUITextLayoutLine lines[ROBO_TEXT_MAX_LINES];
  const RoboU32 lineCount = RoboUITextLayout(text, lines, ROBO_TEXT_MAX_LINES);
  RoboI32 maxW = 0;
  RoboI32 totalH = 0;
  for (RoboU32 li = 0; li < lineCount; ++li) {
    if (lines[li].width > maxW) {
      maxW = lines[li].width;
    }
    totalH += RoboUITextLineHeight(text, lines[li].maxSizePx);
  }
  if (text->maxWidth > 0 && maxW > text->maxWidth) {
    maxW = text->maxWidth;
  }
  if (outWidth != ROBO_NULL) {
    *outWidth = maxW;
  }
  if (outHeight != ROBO_NULL) {
    *outHeight = totalH;
  }
}

static RoboBool RoboUITextSetProperty(RoboUIComponent* self, const char* key, const char* value) {
  auto* text = reinterpret_cast<RoboUIText*>(self);
  if (text == ROBO_NULL || key == ROBO_NULL) {
    return ROBO_FALSE;
  }
  /* Text align and wrap stay on the glyphs. Flex uses alignItems and flexWrap. */
  if (RoboUITextEq(key, "align") &&
      (RoboUITextEq(value, "left") || RoboUITextEq(value, "center") || RoboUITextEq(value, "right") ||
       RoboUITextEq(value, "start") || RoboUITextEq(value, "end"))) {
    if (RoboUITextEq(value, "left") || RoboUITextEq(value, "start")) {
      text->align = RoboTextAlign::Left;
    } else if (RoboUITextEq(value, "center")) {
      text->align = RoboTextAlign::Center;
    } else {
      text->align = RoboTextAlign::Right;
    }
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "size") &&
      (RoboUITextEq(value, "sm") || RoboUITextEq(value, "small") || RoboUITextEq(value, "md") ||
       RoboUITextEq(value, "medium"))) {
    if (RoboUITextEq(value, "sm") || RoboUITextEq(value, "small")) {
      text->sizePx = 17;
      text->leadingMilli = 1470;
      text->role = RoboUITextRole::BodySmall;
    } else {
      text->sizePx = 19;
      text->leadingMilli = 1420;
      text->role = RoboUITextRole::Body;
    }
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "wrap") &&
      (RoboUITextEq(value, "none") || RoboUITextEq(value, "char") || RoboUITextEq(value, "character") ||
       RoboUITextEq(value, "word") || RoboUITextEq(value, "nowrap"))) {
    if (RoboUITextEq(value, "none") || RoboUITextEq(value, "nowrap")) {
      text->wrap = RoboTextWrap::None;
    } else if (RoboUITextEq(value, "char") || RoboUITextEq(value, "character")) {
      text->wrap = RoboTextWrap::Char;
    } else {
      text->wrap = RoboTextWrap::Word;
    }
    return ROBO_TRUE;
  }
  if (RoboUILayoutSetProperty(&text->base.layout, key, value)) {
    if (RoboUITextEq(key, "width")) {
      if (text->base.layout.preferWidth > 0) {
        text->base.width = text->base.layout.preferWidth;
        text->maxWidth = text->base.layout.preferWidth;
      }
    }
    if (RoboUITextEq(key, "height") && text->base.layout.preferHeight > 0) {
      text->base.height = text->base.layout.preferHeight;
    }
    if (RoboUITextEq(key, "maxWidth") && text->base.layout.maxWidth > 0) {
      text->maxWidth = text->base.layout.maxWidth;
    }
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "align")) {
    if (RoboUITextEq(value, "left")) {
      text->align = RoboTextAlign::Left;
    } else if (RoboUITextEq(value, "center")) {
      text->align = RoboTextAlign::Center;
    } else if (RoboUITextEq(value, "right")) {
      text->align = RoboTextAlign::Right;
    } else {
      return ROBO_FALSE;
    }
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "wrap")) {
    if (RoboUITextEq(value, "none") || RoboUITextEq(value, "nowrap")) {
      text->wrap = RoboTextWrap::None;
    } else if (RoboUITextEq(value, "char") || RoboUITextEq(value, "character")) {
      text->wrap = RoboTextWrap::Char;
    } else if (RoboUITextEq(value, "word")) {
      text->wrap = RoboTextWrap::Word;
    } else {
      return ROBO_FALSE;
    }
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "size") || RoboUITextEq(key, "fontSize")) {
    text->sizePx = RoboUITextParseU16(value, text->sizePx);
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "color")) {
    text->colorARGB = RoboUITextParseColor(value);
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "tone")) {
    if (RoboUITextEq(value, "secondary")) {
      text->colorARGB = 0xFFA1A1A6u;
    } else if (RoboUITextEq(value, "tertiary")) {
      text->colorARGB = 0xFF98989Du;
    } else if (RoboUITextEq(value, "disabled")) {
      text->colorARGB = 0xFF48484Au;
    } else if (RoboUITextEq(value, "link")) {
      text->colorARGB = 0xFF3B9EFFu;
    } else if (RoboUITextEq(value, "danger")) {
      text->colorARGB = 0xFFFF453Au;
    } else if (RoboUITextEq(value, "success")) {
      text->colorARGB = 0xFF30D158u;
    } else if (RoboUITextEq(value, "warning")) {
      text->colorARGB = 0xFFFF9F0Au;
    } else if (RoboUITextEq(value, "on-brand") || RoboUITextEq(value, "onbrand")) {
      text->colorARGB = 0xFFFFFFFFu;
    } else if (RoboUITextEq(value, "primary")) {
      text->colorARGB = 0xFFFFFFFFu;
    } else {
      return ROBO_FALSE;
    }
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "variant") || RoboUITextEq(key, "role")) {
    if (RoboUITextEq(value, "display")) {
      RoboUITextApplyRole(text, RoboUITextRole::Display);
    } else if (RoboUITextEq(value, "h1") || RoboUITextEq(value, "heading")) {
      RoboUITextApplyRole(text, RoboUITextRole::H1);
    } else if (RoboUITextEq(value, "h2")) {
      RoboUITextApplyRole(text, RoboUITextRole::H2);
    } else if (RoboUITextEq(value, "h3")) {
      RoboUITextApplyRole(text, RoboUITextRole::H3);
    } else if (RoboUITextEq(value, "h4")) {
      RoboUITextApplyRole(text, RoboUITextRole::H4);
    } else if (RoboUITextEq(value, "h5")) {
      RoboUITextApplyRole(text, RoboUITextRole::H5);
    } else if (RoboUITextEq(value, "h6")) {
      RoboUITextApplyRole(text, RoboUITextRole::H6);
    } else if (RoboUITextEq(value, "subhead")) {
      RoboUITextApplyRole(text, RoboUITextRole::Subhead);
    } else if (RoboUITextEq(value, "content") || RoboUITextEq(value, "body")) {
      RoboUITextApplyRole(text, RoboUITextRole::Body);
    } else if (RoboUITextEq(value, "content-sm") || RoboUITextEq(value, "body-sm")) {
      RoboUITextApplyRole(text, RoboUITextRole::BodySmall);
    } else if (RoboUITextEq(value, "caption")) {
      RoboUITextApplyRole(text, RoboUITextRole::Caption);
    } else if (RoboUITextEq(value, "eyebrow")) {
      RoboUITextApplyRole(text, RoboUITextRole::Eyebrow);
    } else if (RoboUITextEq(value, "label")) {
      RoboUITextApplyRole(text, RoboUITextRole::Label);
    } else {
      return ROBO_FALSE;
    }
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "tracking")) {
    const RoboI32 milli = RoboUITextParseI32(value, text->trackingMilli);
    text->trackingMilli = static_cast<RoboI16>(milli);
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "leading")) {
    const RoboI32 milli = RoboUITextParseI32(value, text->leadingMilli);
    text->leadingMilli = milli > 0 ? static_cast<RoboU16>(milli) : 0;
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "uppercase")) {
    text->uppercase = (RoboUITextEq(value, "true") || RoboUITextEq(value, "1") || RoboUITextEq(value, "yes"))
                          ? ROBO_TRUE
                          : ROBO_FALSE;
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "font") || RoboUITextEq(key, "face") || RoboUITextEq(key, "fontFamily")) {
    /* Resolved at BuildUI via RoboUITextSetFontName — store name not needed in component. */
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "maxWidth") || RoboUITextEq(key, "width")) {
    const RoboI32 w = RoboUITextParseI32(value, text->maxWidth);
    text->maxWidth = w;
    if (RoboUITextEq(key, "width")) {
      text->base.width = w;
    }
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "x")) {
    text->base.x = RoboUITextParseI32(value, text->base.x);
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "y")) {
    text->base.y = RoboUITextParseI32(value, text->base.y);
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "height")) {
    text->base.height = RoboUITextParseI32(value, text->base.height);
    return ROBO_TRUE;
  }
  if (RoboUITextEq(key, "content") || RoboUITextEq(key, "text")) {
    return RoboUITextSetContent(text, value);
  }
  return ROBO_FALSE;
}

static void RoboUITextDestroy(RoboUIComponent* self) {
  (void)self;
}

static void RoboUITextTick(RoboUIComponent* self, RoboU32 deltaMs) {
  (void)self;
  (void)deltaMs;
}

static const RoboUIVtable gRoboUITextVtable = {
    RoboUITextDestroy,
    RoboUITextPaint,
    RoboUITextSetProperty,
    RoboUITextTick,
};

void RoboUITextInit(RoboUIText* text, const char* id) {
  if (text == ROBO_NULL) {
    return;
  }
  RoboMemorySet(text, 0, sizeof(RoboUIText));
  RoboUIComponentInitBase(&text->base, &gRoboUITextVtable, RoboUINodeKind::Text, id);
  text->align = RoboTextAlign::Left;
  text->wrap = RoboTextWrap::Word;
  text->colorARGB = 0xFFA1A1A6u;
  text->sizePx = 19;
  text->maxWidth = 0;
  text->trackingMilli = 0;
  text->leadingMilli = 1420;
  text->uppercase = ROBO_FALSE;
  text->role = RoboUITextRole::Body;
}

const char* RoboUITextRoleFont(RoboUITextRole role) {
  if (role == RoboUITextRole::Label) {
    return "Inter-Medium";
  }
  if (role == RoboUITextRole::Display || role == RoboUITextRole::H1 || role == RoboUITextRole::H2 ||
      role == RoboUITextRole::H3 || role == RoboUITextRole::H4 || role == RoboUITextRole::H5 ||
      role == RoboUITextRole::H6) {
    return "Inter-SemiBold";
  }
  return "Inter-Regular";
}

void RoboUITextApplyRole(RoboUIText* text, RoboUITextRole role);

static const char* RoboHeadingFoundationType(RoboUITextRole role) {
  switch (role) {
    case RoboUITextRole::Display:
      return "display";
    case RoboUITextRole::H2:
      return "h2";
    case RoboUITextRole::H3:
      return "h3";
    case RoboUITextRole::H1:
    default:
      return "h1";
  }
}

static void RoboHeadingPlatformApply(RoboUIText* text, RoboUITextRole role) {
  if (text == ROBO_NULL) {
    return;
  }
  text->base.kind = RoboUINodeKind::Heading;
  text->uppercase = ROBO_FALSE;
  text->align = RoboTextAlign::Left;
  text->wrap = RoboTextWrap::Word;
  if (role == RoboUITextRole::H4 || role == RoboUITextRole::H5 || role == RoboUITextRole::H6) {
    text->colorARGB = RoboFoundationPlatformColor("text-primary");
    text->trackingMilli = 0;
    if (role == RoboUITextRole::H4) {
      text->sizePx = 22;
      text->leadingMilli = 1220;
    } else if (role == RoboUITextRole::H5) {
      text->sizePx = 19;
      text->leadingMilli = 1260;
    } else {
      text->sizePx = 17;
      text->leadingMilli = 1290;
    }
    return;
  }
  RoboFoundationPlatformApplyType(text, RoboHeadingFoundationType(role), "text-primary");
}

static void RoboBodyPlatformApply(RoboUIText* text, RoboUITextRole role) {
  if (text == ROBO_NULL) {
    return;
  }
  text->base.kind = RoboUINodeKind::Paragraph;
  text->uppercase = ROBO_FALSE;
  text->align = RoboTextAlign::Left;
  text->wrap = RoboTextWrap::Word;
  if (role == RoboUITextRole::BodySmall) {
    text->role = RoboUITextRole::BodySmall;
    RoboFoundationPlatformApplyType(text, "body", "text-secondary");
    return;
  }
  text->role = role == RoboUITextRole::Rich ? RoboUITextRole::Rich : RoboUITextRole::Body;
  RoboFoundationPlatformApplyType(text, "body-lg", "text-secondary");
}

static void RoboSubheadPlatformApply(RoboUIText* text) {
  if (text == ROBO_NULL) {
    return;
  }
  text->role = RoboUITextRole::Subhead;
  text->base.kind = RoboUINodeKind::Text;
  text->uppercase = ROBO_FALSE;
  text->align = RoboTextAlign::Left;
  text->wrap = RoboTextWrap::Word;
  RoboFoundationPlatformApplyType(text, "subhead", "text-secondary");
}

static void RoboCaptionPlatformApply(RoboUIText* text) {
  if (text == ROBO_NULL) {
    return;
  }
  text->role = RoboUITextRole::Caption;
  text->base.kind = RoboUINodeKind::Text;
  text->uppercase = ROBO_FALSE;
  text->align = RoboTextAlign::Left;
  text->wrap = RoboTextWrap::Word;
  RoboFoundationPlatformApplyType(text, "caption", "text-tertiary");
}

static void RoboEyebrowPlatformApply(RoboUIText* text) {
  if (text == ROBO_NULL) {
    return;
  }
  text->role = RoboUITextRole::Eyebrow;
  text->base.kind = RoboUINodeKind::Text;
  text->align = RoboTextAlign::Left;
  text->wrap = RoboTextWrap::Word;
  RoboFoundationPlatformApplyType(text, "eyebrow", "text-tertiary");
  text->uppercase = ROBO_TRUE;
}

static void RoboLabelPlatformApply(RoboUIText* text) {
  if (text == ROBO_NULL) {
    return;
  }
  text->role = RoboUITextRole::Label;
  text->base.kind = RoboUINodeKind::Text;
  text->uppercase = ROBO_FALSE;
  text->align = RoboTextAlign::Left;
  text->wrap = RoboTextWrap::Word;
  RoboFoundationPlatformApplyType(text, "button", "text-primary");
}

static void RoboParagraphPlatformApply(RoboUIText* text, RoboUITextRole role) {
  RoboBodyPlatformApply(text, role);
}

static void RoboUITextApplyHeadingDefaults(RoboUIText* text, RoboU32 level) {
  if (text == ROBO_NULL) {
    return;
  }
  if (level == 0) {
    RoboUITextApplyRole(text, RoboUITextRole::Display);
    return;
  }
  if (level > 6) {
    level = 6;
  }
  RoboUITextApplyRole(text, static_cast<RoboUITextRole>(static_cast<RoboU32>(RoboUITextRole::H1) + (level - 1)));
}

static void RoboUITextApplyParagraphDefaults(RoboUIText* text) {
  RoboUITextApplyRole(text, RoboUITextRole::Body);
}

void RoboUITextApplyRole(RoboUIText* text, RoboUITextRole role) {
  if (text == ROBO_NULL) {
    return;
  }
  text->role = role;
  text->uppercase = ROBO_FALSE;
  text->align = RoboTextAlign::Left;
  text->wrap = RoboTextWrap::Word;
  text->base.kind = RoboUINodeKind::Text;
  switch (role) {
    case RoboUITextRole::Display:
    case RoboUITextRole::H1:
    case RoboUITextRole::H2:
    case RoboUITextRole::H3:
    case RoboUITextRole::H4:
    case RoboUITextRole::H5:
    case RoboUITextRole::H6:
      RoboHeadingPlatformApply(text, role);
      break;
    case RoboUITextRole::Subhead:
      RoboSubheadPlatformApply(text);
      break;
    case RoboUITextRole::BodySmall:
      RoboBodyPlatformApply(text, role);
      break;
    case RoboUITextRole::Caption:
      RoboCaptionPlatformApply(text);
      break;
    case RoboUITextRole::Eyebrow:
      RoboEyebrowPlatformApply(text);
      break;
    case RoboUITextRole::Label:
      RoboLabelPlatformApply(text);
      break;
    case RoboUITextRole::Body:
    case RoboUITextRole::Rich:
    default:
      RoboParagraphPlatformApply(text, role);
      break;
  }
}

void RoboUITextInitHeading(RoboUIText* text, const char* id, RoboU32 level) {
  RoboUITextInit(text, id);
  RoboUITextApplyHeadingDefaults(text, level);
}

void RoboUITextInitParagraph(RoboUIText* text, const char* id) {
  RoboUITextInit(text, id);
  RoboUITextApplyParagraphDefaults(text);
}

RoboUIComponent* RoboUITextAsComponent(RoboUIText* text) {
  return text == ROBO_NULL ? ROBO_NULL : &text->base;
}

RoboUIText* RoboUITextFromComponent(RoboUIComponent* component) {
  if (component == ROBO_NULL) {
    return ROBO_NULL;
  }
  if (component->kind != RoboUINodeKind::Text && component->kind != RoboUINodeKind::Heading &&
      component->kind != RoboUINodeKind::Paragraph) {
    return ROBO_NULL;
  }
  return reinterpret_cast<RoboUIText*>(component);
}

void RoboUITextSetFont(RoboUIText* text, const RoboFontView& font) {
  if (text != ROBO_NULL) {
    text->font = font;
    text->fontPack = RoboFontPackView{};
    text->faceName[0] = 0;
  }
}

void RoboUITextSetFontName(RoboUIText* text, RoboUIContext* ctx, const char* fontName) {
  if (text == ROBO_NULL) {
    return;
  }
  text->faceName[0] = 0;
  text->fontPack = RoboFontPackView{};
  if (fontName != ROBO_NULL) {
    RoboU32 i = 0;
    while (fontName[i] != 0 && i + 1 < sizeof(text->faceName)) {
      text->faceName[i] = fontName[i];
      ++i;
    }
    text->faceName[i] = 0;
  }
  if (ctx != ROBO_NULL && ctx->fontPack.header != ROBO_NULL) {
    text->fontPack = ctx->fontPack;
    const RoboU16 size = text->sizePx != 0 ? text->sizePx : ROBO_FONT_MASTER_PX;
    const RoboFontView face = RoboFontPackFindSize(ctx->fontPack, text->faceName, size);
    if (face.header != ROBO_NULL) {
      text->font = face;
      return;
    }
  }
  if (ctx != ROBO_NULL && ctx->font.header != ROBO_NULL) {
    text->font = ctx->font;
  }
}

void RoboUITextClear(RoboUIText* text) {
  if (text == ROBO_NULL) {
    return;
  }
  text->textLength = 0;
  text->text[0] = 0;
  text->spanCount = 0;
}

RoboBool RoboUITextSetContent(RoboUIText* text, const char* utf8) {
  if (text == ROBO_NULL) {
    return ROBO_FALSE;
  }
  RoboUITextClear(text);
  if (utf8 == ROBO_NULL) {
    return ROBO_TRUE;
  }
  return RoboUITextAppendSpan(text, utf8, text->sizePx, text->colorARGB);
}

RoboBool RoboUITextAppendSpan(RoboUIText* text, const char* utf8, RoboU16 sizePx, RoboU32 colorARGB) {
  if (text == ROBO_NULL || utf8 == ROBO_NULL || text->spanCount >= ROBO_TEXT_MAX_SPANS) {
    return ROBO_FALSE;
  }
  const RoboSize len = RoboStringLength(utf8);
  if (text->textLength + len + 1 > ROBO_TEXT_BUF) {
    return ROBO_FALSE;
  }
  RoboTextSpan& span = text->spans[text->spanCount++];
  span.byteStart = text->textLength;
  span.byteLength = static_cast<RoboU32>(len);
  span.sizePx = sizePx == 0 ? text->sizePx : sizePx;
  span.colorARGB = colorARGB == 0 ? text->colorARGB : colorARGB;
  span.reserved0 = 0;
  for (RoboSize i = 0; i < len; ++i) {
    text->text[text->textLength++] = utf8[i];
  }
  text->text[text->textLength] = 0;
  return ROBO_TRUE;
}

void RoboUITextSetAlign(RoboUIText* text, RoboTextAlign align) {
  if (text != ROBO_NULL) {
    text->align = align;
  }
}

void RoboUITextSetWrap(RoboUIText* text, RoboTextWrap wrap) {
  if (text != ROBO_NULL) {
    text->wrap = wrap;
  }
}

void RoboUITextSetSize(RoboUIText* text, RoboU16 sizePx) {
  if (text != ROBO_NULL) {
    text->sizePx = sizePx;
  }
}

void RoboUITextSetColor(RoboUIText* text, RoboU32 colorARGB) {
  if (text != ROBO_NULL) {
    text->colorARGB = colorARGB;
  }
}

void RoboUITextSetMaxWidth(RoboUIText* text, RoboI32 maxWidth) {
  if (text != ROBO_NULL) {
    text->maxWidth = maxWidth;
  }
}
