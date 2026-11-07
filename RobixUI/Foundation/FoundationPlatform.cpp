#include "FoundationPlatform.hpp"

#include "../../RoboUI/RoboUIText.hpp"

struct RoboFoundationColorToken {
  const char* name;
  const char* dark;
  const char* light;
};

struct RoboFoundationTypeToken {
  const char* name;
  const char* size;
  const char* mobileSize;
  const char* weight;
  const char* tracking;
  const char* leading;
};

struct RoboFoundationLayoutToken {
  const char* name;
  const char* minWidth;
  const char* maxWidth;
  const char* padding;
  const char* columns;
  const char* gutter;
};

struct RoboFoundationNumberToken {
  const char* name;
  const char* value;
};

struct RoboFoundationFontToken {
  const char* name;
  const char* face;
};

/* Values are the sealed RoboFoundation.xml set. */
static const RoboFoundationColorToken kRoboFoundationColors[] = {
    {"brand-primary", "#3B9EFF", "#0E7AFF"},
    {"brand-primary-hover", "#5CADFF", "#0A6AE8"},
    {"brand-primary-active", "#2E8CE6", "#0757C7"},
    {"brand-primary-disabled", "#1E3A5C", "#B8D9FF"},
    {"brand-primary-subtle", "rgba(59, 158, 255, 0.15)", "rgba(14, 122, 255, 0.1)"},
    {"brand-primary-range", "rgba(59, 158, 255, 0.38)", "rgba(14, 122, 255, 0.22)"},
    {"brand-primary-focus-ring", "rgba(59, 158, 255, 0.3)", "rgba(14, 122, 255, 0.25)"},
    {"accent-teal", "#2DD4C4", "#14B8A6"},
    {"accent-teal-subtle", "rgba(45, 212, 196, 0.15)", "rgba(20, 184, 166, 0.1)"},
    {"success", "#30D158", "#30D158"},
    {"success-subtle", "rgba(48, 209, 88, 0.15)", "rgba(48, 209, 88, 0.1)"},
    {"success-focus-ring", "rgba(48, 209, 88, 0.3)", "rgba(48, 209, 88, 0.25)"},
    {"warning", "#FF9F0A", "#FF9F0A"},
    {"warning-subtle", "rgba(255, 159, 10, 0.15)", "rgba(255, 159, 10, 0.1)"},
    {"danger", "#FF453A", "#FF3B30"},
    {"danger-hover", "#FF5A50", "#E0342A"},
    {"danger-subtle", "rgba(255, 69, 58, 0.1)", "rgba(255, 59, 48, 0.1)"},
    {"danger-focus-ring", "rgba(255, 69, 58, 0.4)", "rgba(255, 59, 48, 0.25)"},
    {"text-primary", "#FFFFFF", "#1D1D1F"},
    {"text-secondary", "#A1A1A6", "#6E6E73"},
    {"text-tertiary", "#98989D", "#86868B"},
    {"text-disabled", "#48484A", "#C7C7CC"},
    {"text-on-brand", "#FFFFFF", "#FFFFFF"},
    {"text-link", "#3B9EFF", "#0E7AFF"},
    {"bg-primary", "#000000", "#FFFFFF"},
    {"bg-secondary", "#1C1C1E", "#F5F5F7"},
    {"bg-tertiary", "#2C2C2E", "#EDEDEF"},
    {"bg-elevated", "#2C2C2E", "#FFFFFF"},
    {"bg-hover", "#2C2C2E", "#F5F5F7"},
    {"bg-active", "#3A3A3C", "#EDEDEF"},
    {"bg-overlay", "rgba(0, 0, 0, 0.7)", "rgba(0, 0, 0, 0.5)"},
    {"border-default", "#38383A", "#D2D2D7"},
    {"border-subtle", "#2C2C2E", "#E5E5E7"},
    {"border-strong", "#48484A", "#B8B8BD"},
    {"border-focus", "#3B9EFF", "#0E7AFF"},
    {"border-danger", "rgba(255, 69, 58, 0.3)", "rgba(255, 59, 48, 0.3)"},
};

static const RoboFoundationTypeToken kRoboFoundationTypes[] = {
    {"display", "80", "40", "600", "-0.015", "1.05"},
    {"h1", "56", "32", "600", "-0.015", "1.07"},
    {"h2", "40", "28", "600", "-0.01", "1.1"},
    {"h3", "28", "21", "600", "-0.005", "1.14"},
    {"subhead", "24", "19", "400", "0", "1.33"},
    {"body-lg", "19", "17", "400", "0", "1.42"},
    {"body", "17", "", "400", "0", "1.47"},
    {"caption", "12", "", "400", "0", "1.33"},
    {"eyebrow", "12", "", "400", "0.05", "1.33"},
    {"button", "17", "", "500", "0", "1"},
    {"menubar", "13", "", "400", "0", "1"},
    {"window-title", "13", "", "500", "0", "1"},
    {"tooltip", "13", "", "500", "0", "1"},
};

static const RoboFoundationNumberToken kRoboFoundationSpaces[] = {
    {"space-1", "4"},  {"space-2", "8"},  {"space-3", "12"}, {"space-4", "16"}, {"space-5", "24"},
    {"space-6", "32"}, {"space-7", "48"}, {"space-8", "64"}, {"space-9", "96"}, {"space-10", "128"},
};

static const RoboFoundationNumberToken kRoboFoundationRadii[] = {
    {"radius-sm", "6"}, {"radius-md", "8"},  {"radius-lg", "12"}, {"radius-xl", "16"},
    {"radius-2xl", "20"}, {"radius-pill", "980"}, {"control", "8"}, {"card", "12"},
};

static const RoboFoundationFontToken kRoboFoundationFonts[] = {
    {"sans", "Inter-Regular"},
    {"sans-medium", "Inter-Medium"},
    {"sans-semibold", "Inter-SemiBold"},
    {"sans-bold", "Inter-Bold"},
    {"sans-italic", "Inter-Italic"},
};

static const RoboFoundationLayoutToken kRoboFoundationLayouts[] = {
    {"layout-mobile", "0", "734", "22", "1", "16"},
    {"layout-tablet", "735", "1068", "40", "12", "24"},
    {"layout-desktop", "1069", "0", "80", "12", "24"},
};

static const RoboFoundationNumberToken kRoboFoundationWidths[] = {
    {"layout-copy", "692"},       {"layout-docs", "760"}, {"layout-docs-wide", "900"},
    {"layout-marketing", "980"}, {"layout-wide", "1024"},
};

static RoboBool RoboFoundationNameEq(const char* a, const char* b) {
  if (a == ROBO_NULL || b == ROBO_NULL) {
    return ROBO_FALSE;
  }
  while (*a != 0 && *b != 0 && *a == *b) {
    ++a;
    ++b;
  }
  return (*a == 0 && *b == 0) ? ROBO_TRUE : ROBO_FALSE;
}

static RoboU8 RoboFoundationHex(char character) {
  if (character >= '0' && character <= '9') {
    return static_cast<RoboU8>(character - '0');
  }
  if (character >= 'a' && character <= 'f') {
    return static_cast<RoboU8>(10 + character - 'a');
  }
  if (character >= 'A' && character <= 'F') {
    return static_cast<RoboU8>(10 + character - 'A');
  }
  return 0;
}

static RoboI32 RoboFoundationMilli(const char* text) {
  if (text == ROBO_NULL || text[0] == 0) {
    return 0;
  }
  RoboBool negative = ROBO_FALSE;
  RoboSize index = 0;
  if (text[0] == '-') {
    negative = ROBO_TRUE;
    index = 1;
  }
  RoboI32 whole = 0;
  while (text[index] >= '0' && text[index] <= '9') {
    whole = whole * 10 + (text[index] - '0');
    ++index;
  }
  RoboI32 fraction = 0;
  RoboI32 scale = 1000;
  if (text[index] == '.') {
    ++index;
    while (text[index] >= '0' && text[index] <= '9' && scale > 1) {
      scale /= 10;
      fraction += (text[index] - '0') * scale;
      ++index;
    }
  }
  const RoboI32 milli = whole * 1000 + fraction;
  return negative ? -milli : milli;
}

static RoboU32 RoboFoundationParseColor(const char* text) {
  if (text == ROBO_NULL) {
    return 0xFFFFFFFFu;
  }
  if (text[0] == '#') {
    RoboSize length = 0;
    while (text[length + 1] != 0) {
      ++length;
    }
    RoboU8 alpha = 255;
    RoboU8 red = 0;
    RoboU8 green = 0;
    RoboU8 blue = 0;
    if (length == 3) {
      red = static_cast<RoboU8>(RoboFoundationHex(text[1]) * 17);
      green = static_cast<RoboU8>(RoboFoundationHex(text[2]) * 17);
      blue = static_cast<RoboU8>(RoboFoundationHex(text[3]) * 17);
    } else if (length == 6) {
      red = static_cast<RoboU8>((RoboFoundationHex(text[1]) << 4) | RoboFoundationHex(text[2]));
      green = static_cast<RoboU8>((RoboFoundationHex(text[3]) << 4) | RoboFoundationHex(text[4]));
      blue = static_cast<RoboU8>((RoboFoundationHex(text[5]) << 4) | RoboFoundationHex(text[6]));
    } else if (length == 8) {
      alpha = static_cast<RoboU8>((RoboFoundationHex(text[1]) << 4) | RoboFoundationHex(text[2]));
      red = static_cast<RoboU8>((RoboFoundationHex(text[3]) << 4) | RoboFoundationHex(text[4]));
      green = static_cast<RoboU8>((RoboFoundationHex(text[5]) << 4) | RoboFoundationHex(text[6]));
      blue = static_cast<RoboU8>((RoboFoundationHex(text[7]) << 4) | RoboFoundationHex(text[8]));
    }
    return (static_cast<RoboU32>(alpha) << 24) | (static_cast<RoboU32>(red) << 16) |
           (static_cast<RoboU32>(green) << 8) | blue;
  }
  if (text[0] == 'r') {
    RoboI32 channels[4] = {0, 0, 0, 1000};
    RoboI32 channel = 0;
    RoboBool inNumber = ROBO_FALSE;
    RoboI32 whole = 0;
    RoboI32 fraction = 0;
    RoboI32 scale = 1;
    for (RoboSize index = 0; text[index] != 0 && channel < 4; ++index) {
      const char character = text[index];
      if (character >= '0' && character <= '9') {
        inNumber = ROBO_TRUE;
        if (scale == 1) {
          whole = whole * 10 + (character - '0');
        } else {
          fraction = fraction * 10 + (character - '0');
          scale *= 10;
        }
      } else if (character == '.' && inNumber && scale == 1) {
        scale = 10;
      } else if (inNumber && (character == ',' || character == ')')) {
        RoboI32 value = whole;
        if (scale > 1) {
          value = static_cast<RoboI32>((whole * 1000) + (fraction * 1000) / scale);
          if (channel == 3) {
            channels[channel] = value;
          } else {
            channels[channel] = whole;
          }
        } else if (channel == 3) {
          channels[channel] = whole * 1000;
        } else {
          channels[channel] = whole;
        }
        ++channel;
        inNumber = ROBO_FALSE;
        whole = 0;
        fraction = 0;
        scale = 1;
      }
    }
    const RoboU8 red = static_cast<RoboU8>(channels[0]);
    const RoboU8 green = static_cast<RoboU8>(channels[1]);
    const RoboU8 blue = static_cast<RoboU8>(channels[2]);
    const RoboU8 alpha = static_cast<RoboU8>((channels[3] * 255 + 500) / 1000);
    return (static_cast<RoboU32>(alpha) << 24) | (static_cast<RoboU32>(red) << 16) |
           (static_cast<RoboU32>(green) << 8) | blue;
  }
  return 0xFFFFFFFFu;
}

static RoboU16 RoboFoundationParseU16(const char* text) {
  RoboU32 number = 0;
  if (text == ROBO_NULL) {
    return 0;
  }
  for (RoboSize index = 0; text[index] >= '0' && text[index] <= '9'; ++index) {
    number = number * 10u + static_cast<RoboU32>(text[index] - '0');
  }
  return static_cast<RoboU16>(number > 65535u ? 65535u : number);
}

RoboU32 RoboFoundationPlatformColorForTheme(const char* name, RoboBool light) {
  for (const RoboFoundationColorToken& token : kRoboFoundationColors) {
    if (RoboFoundationNameEq(token.name, name)) {
      const char* value = light == ROBO_TRUE && token.light != ROBO_NULL ? token.light : token.dark;
      return RoboFoundationParseColor(value);
    }
  }
  return 0xFFFFFFFFu;
}

RoboU32 RoboFoundationPlatformColor(const char* name) {
  return RoboFoundationPlatformColorForTheme(name, ROBO_FALSE);
}

RoboBool RoboFoundationPlatformType(const char* name, RoboFoundationTypeMetrics* outMetrics) {
  if (outMetrics == ROBO_NULL) {
    return ROBO_FALSE;
  }
  for (const RoboFoundationTypeToken& token : kRoboFoundationTypes) {
    if (!RoboFoundationNameEq(token.name, name)) {
      continue;
    }
    outMetrics->sizePx = RoboFoundationParseU16(token.size);
    outMetrics->mobileSizePx =
        token.mobileSize != ROBO_NULL && token.mobileSize[0] != 0 ? RoboFoundationParseU16(token.mobileSize) : outMetrics->sizePx;
    outMetrics->weight = RoboFoundationParseU16(token.weight);
    outMetrics->trackingMilli = static_cast<RoboI16>(RoboFoundationMilli(token.tracking));
    outMetrics->leadingMilli = static_cast<RoboU16>(RoboFoundationMilli(token.leading));
    return ROBO_TRUE;
  }
  return ROBO_FALSE;
}

static RoboI32 RoboFoundationFindNumber(const RoboFoundationNumberToken* tokens, RoboSize count, const char* name) {
  for (RoboSize index = 0; index < count; ++index) {
    if (RoboFoundationNameEq(tokens[index].name, name)) {
      return RoboFoundationParseU16(tokens[index].value);
    }
  }
  return 0;
}

RoboI32 RoboFoundationPlatformSpace(const char* name) {
  return RoboFoundationFindNumber(kRoboFoundationSpaces, sizeof(kRoboFoundationSpaces) / sizeof(kRoboFoundationSpaces[0]),
                                  name);
}

RoboI32 RoboFoundationPlatformRadius(const char* name) {
  return RoboFoundationFindNumber(kRoboFoundationRadii, sizeof(kRoboFoundationRadii) / sizeof(kRoboFoundationRadii[0]),
                                  name);
}

const char* RoboFoundationPlatformFont(const char* name) {
  for (const RoboFoundationFontToken& token : kRoboFoundationFonts) {
    if (RoboFoundationNameEq(token.name, name)) {
      return token.face;
    }
  }
  return "Inter-Regular";
}

RoboBool RoboFoundationPlatformLayout(const char* name, RoboFoundationLayoutMetrics* outMetrics) {
  if (outMetrics == ROBO_NULL) {
    return ROBO_FALSE;
  }
  for (const RoboFoundationLayoutToken& token : kRoboFoundationLayouts) {
    if (!RoboFoundationNameEq(token.name, name)) {
      continue;
    }
    outMetrics->minWidth = RoboFoundationParseU16(token.minWidth);
    outMetrics->maxWidth = RoboFoundationParseU16(token.maxWidth);
    outMetrics->padding = RoboFoundationParseU16(token.padding);
    outMetrics->columns = RoboFoundationParseU16(token.columns);
    outMetrics->gutter = RoboFoundationParseU16(token.gutter);
    return ROBO_TRUE;
  }
  return ROBO_FALSE;
}

RoboI32 RoboFoundationPlatformWidth(const char* name) {
  return RoboFoundationFindNumber(kRoboFoundationWidths, sizeof(kRoboFoundationWidths) / sizeof(kRoboFoundationWidths[0]),
                                  name);
}

void RoboFoundationPlatformApplyType(RoboUIText* text, const char* typeName, const char* colorName) {
  if (text == ROBO_NULL) {
    return;
  }
  RoboFoundationTypeMetrics metrics{};
  if (RoboFoundationPlatformType(typeName, &metrics)) {
    text->sizePx = metrics.sizePx;
    text->trackingMilli = metrics.trackingMilli;
    text->leadingMilli = metrics.leadingMilli;
  }
  if (colorName != ROBO_NULL) {
    text->colorARGB = RoboFoundationPlatformColor(colorName);
  }
}
