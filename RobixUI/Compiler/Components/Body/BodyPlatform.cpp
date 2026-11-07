#include "BodyPlatform.hpp"

#include "../../Foundation/FoundationPlatform.hpp"

void RoboBodyPlatformApply(RoboUIText* text, RoboUITextRole role) {
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
