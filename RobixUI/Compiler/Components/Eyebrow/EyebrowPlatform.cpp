#include "EyebrowPlatform.hpp"

#include "../../Foundation/FoundationPlatform.hpp"

void RoboEyebrowPlatformApply(RoboUIText* text) {
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
