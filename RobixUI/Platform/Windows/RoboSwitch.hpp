#pragma once

#include "../../../RoboUI/RoboUI.hpp"
#include "../../../RoboUI/RoboUIText.hpp"

struct RoboSwitch {
  RoboUIComponent base;
  RoboUIText title;
  RoboUIText description;
  char titleText[192];
  char descriptionText[256];
  RoboU8 variant;
  RoboU8 size;
  RoboBool checked;
  RoboBool disabled;
  RoboBool loading;
  RoboBool item;
  RoboBool divider;
  RoboBool pressed;
  RoboBool focused;
  RoboI32 trackW;
  RoboI32 trackH;
  RoboI32 thumbPx;
  RoboI32 travelPx;
  RoboI32 thumb;
  RoboI32 thumbFrom;
  RoboI32 thumbTo;
  RoboU32 animStart;
  RoboBool animating;
  RoboI32 pressAmount;
  RoboI32 pressFrom;
  RoboI32 pressTo;
  RoboU32 pressStart;
  RoboBool pressing;
};

void RoboSwitchInit(RoboSwitch* control, const char* id);
RoboUIComponent* RoboSwitchAsComponent(RoboSwitch* control);
RoboSwitch* RoboSwitchFromComponent(RoboUIComponent* component);
void RoboSwitchSetVariant(RoboSwitch* control, const char* variant);
void RoboSwitchSetSize(RoboSwitch* control, const char* size);
void RoboSwitchSetChecked(RoboSwitch* control, RoboBool checked);
void RoboSwitchSetCopy(RoboSwitch* control, const char* title, const char* description);
void RoboSwitchSetFlags(RoboSwitch* control, RoboBool disabled, RoboBool loading, RoboBool item, RoboBool divider);
void RoboSwitchSync(RoboSwitch* control, RoboUIContext* ctx, RoboU32 dpi);
void RoboSwitchSetPressed(RoboSwitch* control, RoboBool pressed, RoboU32 nowMs);
void RoboSwitchSetFocused(RoboSwitch* control, RoboBool focused);
void RoboSwitchToggle(RoboSwitch* control, RoboU32 nowMs);
RoboBool RoboSwitchTick(RoboSwitch* control, RoboU32 nowMs);
void RoboSwitchPaintSpin(RoboSwitch* control, RoboGPU* gpu, RoboI32 absX, RoboI32 absY);
