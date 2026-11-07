#pragma once

/* A screen compiled from the same XML as the web DevKit. Implemented components only. */
struct RoboWindowsCompiledNode {
  int container;
  int childBegin;
  int childCount;
  int textKind;
  unsigned level;
  int bodySmall;
  const char* font;
  const char* content;
  const char* display;
  const char* direction;
  const char* justify;
  const char* align;
  const char* wrap;
  int gap;
  int rowGap;
  int columnGap;
  int padTop;
  int padRight;
  int padBottom;
  int padLeft;
  int maxWidth;
  const char* background;
  int icon;
  const char* iconName;
  int iconSize;
  const char* fill;
  const char* stroke;
  const char* accent;
  int button;
  const char* variant;
  const char* buttonSize;
  const char* preview;
  int fullWidth;
  int loading;
  int disabled;
  const char* loadingText;
  const char* leading;
  const char* trailing;
  int iconButton;
  const char* badge;
  const char* status;
  const char* action;
  int successMs;
  int textField;
  const char* placeholder;
  const char* labelText;
  const char* hint;
  const char* fieldType;
  int invalid;
  int valid;
  int readOnly;
  int required;
  int optional;
  int maxLength;
  int switchControl;
  int switchItem;
  int switchGroup;
  int checked;
  int divider;
};

struct RoboWindowsCompiledScreen {
  const RoboWindowsCompiledNode* nodes;
  int count;
  const int* childIds;
  int childCount;
};

void RoboWindowsCompiledScreenGet(RoboWindowsCompiledScreen* screen);
