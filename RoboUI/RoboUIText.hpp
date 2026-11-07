#pragma once

#include "../RoboGPU/RoboText.hpp"

constexpr RoboU32 ROBO_TEXT_MAX_SPANS = 48;
constexpr RoboU32 ROBO_TEXT_BUF = 4096;

enum class RoboUINodeKind : RoboU32 { None = 0, Text, Heading, Paragraph, Board, Screen };

enum class RoboUITextRole : RoboU32 {
  Text = 0,
  Display,
  H1,
  H2,
  H3,
  H4,
  H5,
  H6,
  Subhead,
  Body,
  BodySmall,
  Caption,
  Eyebrow,
  Label,
  Rich
};

enum class RoboTextAlign : RoboU32 { Left = 0, Center, Right };
enum class RoboTextWrap : RoboU32 { Word = 0, None, Char };

struct RoboUILayout {
  RoboI32 preferWidth = 0;
  RoboI32 preferHeight = 0;
  RoboI32 maxWidth = 0;
};

struct RoboUIComponent;

struct RoboUIVtable {
  void (*destroy)(RoboUIComponent*);
  void (*paint)(RoboUIComponent*, RoboGPU*, RoboI32, RoboI32);
  RoboBool (*setProperty)(RoboUIComponent*, const char*, const char*);
  void (*tick)(RoboUIComponent*, RoboU32);
};

struct RoboUIComponent {
  const RoboUIVtable* vtable = nullptr;
  RoboUINodeKind kind = RoboUINodeKind::None;
  RoboUILayout layout{};
  RoboI32 x = 0;
  RoboI32 y = 0;
  RoboI32 width = 0;
  RoboI32 height = 0;
  char id[64]{};
};

struct RoboTextSpan {
  RoboU32 byteStart = 0;
  RoboU32 byteLength = 0;
  RoboU16 sizePx = 0;
  RoboU16 reserved0 = 0;
  RoboU32 colorARGB = 0;
};

struct RoboUIText {
  RoboUIComponent base{};
  RoboTextSpan spans[ROBO_TEXT_MAX_SPANS]{};
  RoboU16 spanCount = 0;
  RoboFontView font{};
  RoboFontPackView fontPack{};
  char faceName[64]{};
  RoboU16 sizePx = 0;
  RoboU32 colorARGB = 0;
  char text[ROBO_TEXT_BUF]{};
  RoboU32 textLength = 0;
  RoboI16 trackingMilli = 0;
  RoboU16 leadingMilli = 0;
  RoboBool uppercase = ROBO_FALSE;
  RoboTextWrap wrap = RoboTextWrap::Word;
  RoboTextAlign align = RoboTextAlign::Left;
  RoboI32 maxWidth = 0;
  RoboUITextRole role = RoboUITextRole::Body;
};

struct RoboUIContext {
  RoboFontPackView fontPack{};
  RoboFontView font{};
};

void RoboUIComponentInitBase(RoboUIComponent* component, const RoboUIVtable* vtable, RoboUINodeKind kind, const char* id);
bool RoboUILayoutSetProperty(RoboUILayout* layout, const char* key, const char* value);
bool RoboUISetProperty(RoboUIComponent* component, const char* key, const char* value);

void RoboUITextInit(RoboUIText* text, const char* id);
const char* RoboUITextRoleFont(RoboUITextRole role);
void RoboUITextApplyRole(RoboUIText* text, RoboUITextRole role);
void RoboUITextInitHeading(RoboUIText* text, const char* id, RoboU32 level);
void RoboUITextInitParagraph(RoboUIText* text, const char* id);
RoboUIComponent* RoboUITextAsComponent(RoboUIText* text);
RoboUIText* RoboUITextFromComponent(RoboUIComponent* component);
void RoboUITextSetFont(RoboUIText* text, const RoboFontView& font);
void RoboUITextSetFontName(RoboUIText* text, RoboUIContext* ctx, const char* fontName);
void RoboUITextClear(RoboUIText* text);
RoboBool RoboUITextSetContent(RoboUIText* text, const char* utf8);
RoboBool RoboUITextAppendSpan(RoboUIText* text, const char* utf8, RoboU16 sizePx, RoboU32 colorARGB);
void RoboUITextSetAlign(RoboUIText* text, RoboTextAlign align);
void RoboUITextSetWrap(RoboUIText* text, RoboTextWrap wrap);
void RoboUITextSetSize(RoboUIText* text, RoboU16 sizePx);
void RoboUITextSetColor(RoboUIText* text, RoboU32 colorARGB);
void RoboUITextSetMaxWidth(RoboUIText* text, RoboI32 maxWidth);
void RoboUITextMeasure(RoboUIText* text, RoboI32* outWidth, RoboI32* outHeight);

void RoboUITextApplyHeadingDefaults(RoboUIText* text, RoboU32 level);
RoboU16 RoboUITextHeadingDefaultSize(RoboU32 level);
const char* RoboUITextHeadingDefaultFont(RoboU32 level);
void RoboUITextApplyParagraphDefaults(RoboUIText* text);
