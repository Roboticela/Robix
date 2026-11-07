#pragma once

#include <cstddef>
#include <cstdint>

/* Types the sealed foundation table uses. Dark is the default theme. */

using RoboU8 = std::uint8_t;
using RoboU16 = std::uint16_t;
using RoboU32 = std::uint32_t;
using RoboI16 = std::int16_t;
using RoboI32 = std::int32_t;
using RoboSize = std::size_t;
using RoboBool = std::uint8_t;

constexpr RoboBool ROBO_TRUE = 1;
constexpr RoboBool ROBO_FALSE = 0;
#define ROBO_NULL nullptr

struct RoboFoundationTypeMetrics {
    RoboU16 sizePx;
    RoboU16 mobileSizePx;
    RoboI16 trackingMilli;
    RoboU16 leadingMilli;
    RoboU16 weight;
};

struct RoboFoundationLayoutMetrics {
    RoboI32 minWidth;
    RoboI32 maxWidth;
    RoboI32 padding;
    RoboI32 columns;
    RoboI32 gutter;
};

RoboU32 RoboFoundationPlatformColor(const char* name);
RoboU32 RoboFoundationPlatformColorForTheme(const char* name, RoboBool light);
RoboBool RoboFoundationPlatformType(const char* name, RoboFoundationTypeMetrics* outMetrics);
RoboI32 RoboFoundationPlatformSpace(const char* name);
RoboI32 RoboFoundationPlatformRadius(const char* name);
const char* RoboFoundationPlatformFont(const char* name);
RoboBool RoboFoundationPlatformLayout(const char* name, RoboFoundationLayoutMetrics* outMetrics);
RoboI32 RoboFoundationPlatformWidth(const char* name);

struct RoboUIText;
void RoboFoundationPlatformApplyType(RoboUIText* text, const char* typeName, const char* colorName);
