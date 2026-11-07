#include "Subhead.Platform.hpp"

#include "../../Foundation/FoundationPlatform.hpp"

void RoboSubheadPlatformApply(RoboUIText* text) {
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
