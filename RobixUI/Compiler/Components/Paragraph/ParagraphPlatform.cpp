#include "ParagraphPlatform.hpp"

#include "../Body/BodyPlatform.hpp"

void RoboParagraphPlatformApply(RoboUIText* text, RoboUITextRole role) {
  RoboBodyPlatformApply(text, role);
}

void RoboUITextApplyParagraphDefaults(RoboUIText* text) {
  RoboUITextApplyRole(text, RoboUITextRole::Body);
}
