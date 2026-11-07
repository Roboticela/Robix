#pragma once

#include "../../RoboShared/RoboTypes.hpp"

/* The one native Foundation. Web reads RoboFoundation.xml on its own. Dark is the default theme.
   Text objects apply these metrics when a text component is added. */

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
