#include "Label.Platform.hpp"

#include "../../Foundation/FoundationPlatform.hpp"

void RoboLabelPlatformApply(RoboUIText* text) {
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
