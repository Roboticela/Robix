#include "Heading.Platform.hpp"

#include "../../Foundation/FoundationPlatform.hpp"

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

void RoboHeadingPlatformApply(RoboUIText* text, RoboUITextRole role) {
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

RoboU16 RoboUITextHeadingDefaultSize(RoboU32 level) {
  const char* typeName = "h1";
  if (level == 0) {
    typeName = "display";
  } else if (level == 2) {
    typeName = "h2";
  } else if (level == 3) {
    typeName = "h3";
  } else if (level == 4) {
    return 22;
  } else if (level == 5) {
    return 19;
  } else if (level >= 6) {
    return 17;
  }
  RoboFoundationTypeMetrics metrics{};
  if (RoboFoundationPlatformType(typeName, &metrics)) {
    return metrics.sizePx;
  }
  return 56;
}

const char* RoboUITextHeadingDefaultFont(RoboU32 level) {
  return level == 0 ? RoboUITextRoleFont(RoboUITextRole::Display) : RoboUITextRoleFont(RoboUITextRole::H1);
}

void RoboUITextApplyHeadingDefaults(RoboUIText* text, RoboU32 level) {
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
