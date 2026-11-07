#include "Paragraph.Platform.hpp"

#include "../Body/Body.Platform.hpp"

void RoboParagraphPlatformApply(RoboUIText* text, RoboUITextRole role) {
  RoboBodyPlatformApply(text, role);
}

void RoboUITextApplyParagraphDefaults(RoboUIText* text) {
  RoboUITextApplyRole(text, RoboUITextRole::Body);
}
