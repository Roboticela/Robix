#include "RoboUIText.hpp"

#include <cstring>

namespace {

RoboI32 ParseI32(const char* value, RoboI32 fallback) {
  if (value == ROBO_NULL || value[0] == 0) {
    return fallback;
  }
  RoboBool negative = ROBO_FALSE;
  RoboSize index = 0;
  if (value[0] == '-') {
    negative = ROBO_TRUE;
    index = 1;
  }
  RoboI32 number = 0;
  for (; value[index] != 0; ++index) {
    if (value[index] < '0' || value[index] > '9') {
      break;
    }
    number = number * 10 + (value[index] - '0');
  }
  return negative ? -number : number;
}

}  // namespace

void RoboUIComponentInitBase(RoboUIComponent* component, const RoboUIVtable* vtable, RoboUINodeKind kind, const char* id) {
  if (component == ROBO_NULL) {
    return;
  }
  component->vtable = vtable;
  component->kind = kind;
  component->layout = {};
  component->x = 0;
  component->y = 0;
  component->width = 0;
  component->height = 0;
  component->id[0] = 0;
  if (id == ROBO_NULL) {
    return;
  }
  RoboU32 index = 0;
  while (id[index] != 0 && index + 1 < sizeof(component->id)) {
    component->id[index] = id[index];
    ++index;
  }
  component->id[index] = 0;
}

bool RoboUILayoutSetProperty(RoboUILayout* layout, const char* key, const char* value) {
  if (layout == ROBO_NULL || key == ROBO_NULL) {
    return false;
  }
  if (std::strcmp(key, "width") == 0) {
    layout->preferWidth = ParseI32(value, layout->preferWidth);
    return true;
  }
  if (std::strcmp(key, "height") == 0) {
    layout->preferHeight = ParseI32(value, layout->preferHeight);
    return true;
  }
  if (std::strcmp(key, "maxWidth") == 0) {
    layout->maxWidth = ParseI32(value, layout->maxWidth);
    return true;
  }
  return false;
}

bool RoboUISetProperty(RoboUIComponent* component, const char* key, const char* value) {
  if (component == ROBO_NULL || component->vtable == ROBO_NULL || component->vtable->setProperty == ROBO_NULL) {
    return false;
  }
  return component->vtable->setProperty(component, key, value) == ROBO_TRUE;
}
