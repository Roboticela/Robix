#include "CaptionPlatform.hpp"

#include "../../Foundation/FoundationPlatform.hpp"

void RoboCaptionPlatformApply(RoboUIText* text) {
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
