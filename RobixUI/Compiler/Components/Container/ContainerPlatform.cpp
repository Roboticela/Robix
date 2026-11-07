#include "ContainerPlatform.hpp"

#include "../../../RoboGPU/RoboGPU.hpp"

static RoboBool RoboContainerEq(const char* a, const char* b) {
  if (a == ROBO_NULL || b == ROBO_NULL) {
    return ROBO_FALSE;
  }
  while (*a != 0 && *b != 0 && *a == *b) {
    ++a;
    ++b;
  }
  return (*a == 0 && *b == 0) ? ROBO_TRUE : ROBO_FALSE;
}

static RoboI32 RoboContainerParseI32(const char* value, RoboI32 fallback) {
  if (value == ROBO_NULL || value[0] == 0) {
    return fallback;
  }
  RoboBool neg = ROBO_FALSE;
  RoboSize i = 0;
  if (value[0] == '-') {
    neg = ROBO_TRUE;
    i = 1;
  }
  RoboI32 n = 0;
  for (; value[i] != 0; ++i) {
    if (value[i] < '0' || value[i] > '9') {
      break;
    }
    n = n * 10 + (value[i] - '0');
  }
  return neg ? -n : n;
}

static RoboU8 RoboContainerHexNibble(char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<RoboU8>(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return static_cast<RoboU8>(10 + c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return static_cast<RoboU8>(10 + c - 'A');
  }
  return 0;
}

static RoboU32 RoboContainerParseColor(const char* text) {
  if (text == ROBO_NULL || text[0] != '#') {
    return 0;
  }
  RoboSize len = RoboStringLength(text + 1);
  RoboU8 a = 255, r = 0, g = 0, b = 0;
  if (len == 3) {
    r = static_cast<RoboU8>(RoboContainerHexNibble(text[1]) * 17);
    g = static_cast<RoboU8>(RoboContainerHexNibble(text[2]) * 17);
    b = static_cast<RoboU8>(RoboContainerHexNibble(text[3]) * 17);
  } else if (len == 6) {
    r = static_cast<RoboU8>((RoboContainerHexNibble(text[1]) << 4) | RoboContainerHexNibble(text[2]));
    g = static_cast<RoboU8>((RoboContainerHexNibble(text[3]) << 4) | RoboContainerHexNibble(text[4]));
    b = static_cast<RoboU8>((RoboContainerHexNibble(text[5]) << 4) | RoboContainerHexNibble(text[6]));
  } else if (len == 8) {
    a = static_cast<RoboU8>((RoboContainerHexNibble(text[1]) << 4) | RoboContainerHexNibble(text[2]));
    r = static_cast<RoboU8>((RoboContainerHexNibble(text[3]) << 4) | RoboContainerHexNibble(text[4]));
    g = static_cast<RoboU8>((RoboContainerHexNibble(text[5]) << 4) | RoboContainerHexNibble(text[6]));
    b = static_cast<RoboU8>((RoboContainerHexNibble(text[7]) << 4) | RoboContainerHexNibble(text[8]));
  }
  return (static_cast<RoboU32>(a) << 24) | (static_cast<RoboU32>(r) << 16) | (static_cast<RoboU32>(g) << 8) |
         static_cast<RoboU32>(b);
}

static void RoboContainerPaint(RoboUIComponent* self, RoboGPU* gpu, RoboI32 absX, RoboI32 absY) {
  auto* box = reinterpret_cast<RoboContainer*>(self);
  if (box == ROBO_NULL || gpu == ROBO_NULL || box->backgroundARGB == 0) {
    return;
  }
  const RoboI32 w = box->base.width;
  const RoboI32 h = box->base.height;
  if (w <= 0 || h <= 0) {
    return;
  }
  const RoboU32 c = box->backgroundARGB;
  const RoboU32 packed = RoboGPUPackColor(gpu, static_cast<RoboU8>((c >> 16) & 0xFF),
                                          static_cast<RoboU8>((c >> 8) & 0xFF), static_cast<RoboU8>(c & 0xFF),
                                          static_cast<RoboU8>((c >> 24) & 0xFF));
  RoboGPUFillRect(gpu, absX, absY, w, h, packed);
}

static void RoboContainerPullStyle(RoboContainer* box) {
  if (box == ROBO_NULL) {
    return;
  }
  const RoboUILayout* layout = &box->base.layout;
  if (layout->boxDisplay != 0) {
    box->display = static_cast<RoboContainerDisplay>(layout->boxDisplay);
  }
  if (layout->boxDirection != 0) {
    box->direction = static_cast<RoboFlexDirection>(layout->boxDirection);
  }
  if (layout->boxJustify != 0) {
    box->justify = static_cast<RoboJustifyContent>(layout->boxJustify);
  }
  if (layout->boxAlign != 0) {
    box->alignItems = static_cast<RoboAlignItems>(layout->boxAlign);
  }
  if (layout->boxAlignContent != 0) {
    box->alignContent = static_cast<RoboJustifyContent>(layout->boxAlignContent);
  }
  if (layout->boxJustifyItems != 0) {
    box->justifyItems = static_cast<RoboAlignItems>(layout->boxJustifyItems);
  }
  if (layout->boxWrap != 0) {
    box->wrap = static_cast<RoboFlexWrap>(layout->boxWrap);
  }
  if ((layout->gapSet & 1u) != 0) {
    box->rowGap = layout->boxRowGap;
    box->gap = layout->boxRowGap;
  }
  if ((layout->gapSet & 2u) != 0) {
    box->columnGap = layout->boxColumnGap;
    box->gap = layout->boxColumnGap;
  }
  if (layout->columns > 0) {
    box->columns = layout->columns;
  }
  if (layout->rows > 0) {
    box->rows = layout->rows;
  }
}

RoboBool RoboContainerSetProperty(RoboUIComponent* self, const char* key, const char* value) {
  auto* box = reinterpret_cast<RoboContainer*>(self);
  if (box == ROBO_NULL || key == ROBO_NULL) {
    return ROBO_FALSE;
  }
  if (RoboUILayoutSetProperty(&box->base.layout, key, value)) {
    RoboContainerPullStyle(box);
    if (RoboContainerEq(key, "width") && box->base.layout.preferWidth > 0) {
      box->base.width = box->base.layout.preferWidth;
    }
    if (RoboContainerEq(key, "height") && box->base.layout.preferHeight > 0) {
      box->base.height = box->base.layout.preferHeight;
    }
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "x")) {
    box->base.x = RoboContainerParseI32(value, box->base.x);
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "y")) {
    box->base.y = RoboContainerParseI32(value, box->base.y);
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "display")) {
    if (RoboContainerEq(value, "flex")) {
      box->display = RoboContainerDisplay::Flex;
    } else if (RoboContainerEq(value, "grid")) {
      box->display = RoboContainerDisplay::Grid;
    } else if (RoboContainerEq(value, "stack") || RoboContainerEq(value, "canvas")) {
      box->display = RoboContainerDisplay::Stack;
    } else {
      return ROBO_FALSE;
    }
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "direction") || RoboContainerEq(key, "flexDirection")) {
    if (RoboContainerEq(value, "row")) {
      box->direction = RoboFlexDirection::Row;
    } else if (RoboContainerEq(value, "column") || RoboContainerEq(value, "col")) {
      box->direction = RoboFlexDirection::Column;
    } else if (RoboContainerEq(value, "row-reverse") || RoboContainerEq(value, "rowreverse")) {
      box->direction = RoboFlexDirection::RowReverse;
    } else if (RoboContainerEq(value, "column-reverse") || RoboContainerEq(value, "columnreverse")) {
      box->direction = RoboFlexDirection::ColumnReverse;
    } else {
      return ROBO_FALSE;
    }
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "justify") || RoboContainerEq(key, "justifyContent")) {
    if (RoboContainerEq(value, "start") || RoboContainerEq(value, "flex-start") || RoboContainerEq(value, "left") ||
        RoboContainerEq(value, "top")) {
      box->justify = RoboJustifyContent::Start;
    } else if (RoboContainerEq(value, "center")) {
      box->justify = RoboJustifyContent::Center;
    } else if (RoboContainerEq(value, "end") || RoboContainerEq(value, "flex-end") || RoboContainerEq(value, "right") ||
               RoboContainerEq(value, "bottom")) {
      box->justify = RoboJustifyContent::End;
    } else if (RoboContainerEq(value, "space-between") || RoboContainerEq(value, "spacebetween")) {
      box->justify = RoboJustifyContent::SpaceBetween;
    } else if (RoboContainerEq(value, "space-around") || RoboContainerEq(value, "spacearound")) {
      box->justify = RoboJustifyContent::SpaceAround;
    } else if (RoboContainerEq(value, "space-evenly") || RoboContainerEq(value, "spaceevenly")) {
      box->justify = RoboJustifyContent::SpaceEvenly;
    } else {
      return ROBO_FALSE;
    }
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "align") || RoboContainerEq(key, "alignItems")) {
    if (RoboContainerEq(value, "start") || RoboContainerEq(value, "flex-start") || RoboContainerEq(value, "left") ||
        RoboContainerEq(value, "top")) {
      box->alignItems = RoboAlignItems::Start;
    } else if (RoboContainerEq(value, "center")) {
      box->alignItems = RoboAlignItems::Center;
    } else if (RoboContainerEq(value, "end") || RoboContainerEq(value, "flex-end") || RoboContainerEq(value, "right") ||
               RoboContainerEq(value, "bottom")) {
      box->alignItems = RoboAlignItems::End;
    } else if (RoboContainerEq(value, "stretch")) {
      box->alignItems = RoboAlignItems::Stretch;
    } else {
      return ROBO_FALSE;
    }
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "wrap") || RoboContainerEq(key, "flexWrap")) {
    if (RoboContainerEq(value, "wrap")) {
      box->wrap = RoboFlexWrap::Wrap;
    } else if (RoboContainerEq(value, "nowrap") || RoboContainerEq(value, "none")) {
      box->wrap = RoboFlexWrap::NoWrap;
    } else {
      return ROBO_FALSE;
    }
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "gap")) {
    box->gap = RoboContainerParseI32(value, box->gap);
    box->rowGap = box->gap;
    box->columnGap = box->gap;
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "rowGap")) {
    box->rowGap = RoboContainerParseI32(value, box->rowGap);
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "columnGap") || RoboContainerEq(key, "colGap")) {
    box->columnGap = RoboContainerParseI32(value, box->columnGap);
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "padding")) {
    RoboContainerSetPadding(box, RoboContainerParseI32(value, 0));
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "paddingTop")) {
    box->paddingT = RoboContainerParseI32(value, box->paddingT);
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "paddingRight")) {
    box->paddingR = RoboContainerParseI32(value, box->paddingR);
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "paddingBottom")) {
    box->paddingB = RoboContainerParseI32(value, box->paddingB);
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "paddingLeft")) {
    box->paddingL = RoboContainerParseI32(value, box->paddingL);
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "background") || RoboContainerEq(key, "backgroundColor") || RoboContainerEq(key, "bg")) {
    box->backgroundARGB = RoboContainerParseColor(value);
    return ROBO_TRUE;
  }
  if (RoboContainerEq(key, "fill") || RoboContainerEq(key, "fillParent")) {
    box->fillParent = (RoboContainerEq(value, "true") || RoboContainerEq(value, "1") || RoboContainerEq(value, "yes"))
                          ? ROBO_TRUE
                          : ROBO_FALSE;
    return ROBO_TRUE;
  }
  return ROBO_FALSE;
}

static void RoboContainerDestroy(RoboUIComponent* self) {
  (void)self;
}

static void RoboContainerTick(RoboUIComponent* self, RoboU32 deltaMs) {
  (void)self;
  (void)deltaMs;
}

static const RoboUIVtable gRoboContainerVtable = {
    RoboContainerDestroy,
    RoboContainerPaint,
    RoboContainerSetProperty,
    RoboContainerTick,
};

void RoboContainerInit(RoboContainer* container, const char* id) {
  if (container == ROBO_NULL) {
    return;
  }
  RoboMemorySet(container, 0, sizeof(RoboContainer));
  RoboUIComponentInitBase(&container->base, &gRoboContainerVtable, RoboUINodeKind::Container, id);
  container->display = RoboContainerDisplay::Flex;
  container->direction = RoboFlexDirection::Column;
  container->justify = RoboJustifyContent::Start;
  container->alignItems = RoboAlignItems::Stretch;
  container->wrap = RoboFlexWrap::NoWrap;
  container->alignContent = RoboJustifyContent::Start;
  container->justifyItems = RoboAlignItems::Stretch;
  container->columns = 0;
  container->rows = 0;
  container->gap = 0;
  container->rowGap = 0;
  container->columnGap = 0;
  container->backgroundARGB = 0;
  container->fillParent = ROBO_FALSE;
}

RoboUIComponent* RoboContainerAsComponent(RoboContainer* container) {
  return container == ROBO_NULL ? ROBO_NULL : &container->base;
}

RoboContainer* RoboContainerFromComponent(RoboUIComponent* component) {
  if (component == ROBO_NULL) {
    return ROBO_NULL;
  }
  const RoboUINodeKind kind = component->kind;
  if (kind != RoboUINodeKind::Container && kind != RoboUINodeKind::Dock && kind != RoboUINodeKind::DockItem &&
      kind != RoboUINodeKind::DockSeparator && kind != RoboUINodeKind::DockTooltip) {
    return ROBO_NULL;
  }
  return reinterpret_cast<RoboContainer*>(component);
}

void RoboContainerSetDisplay(RoboContainer* container, RoboContainerDisplay display) {
  if (container != ROBO_NULL) {
    container->display = display;
  }
}

void RoboContainerSetDirection(RoboContainer* container, RoboFlexDirection direction) {
  if (container != ROBO_NULL) {
    container->direction = direction;
  }
}

void RoboContainerSetJustify(RoboContainer* container, RoboJustifyContent justify) {
  if (container != ROBO_NULL) {
    container->justify = justify;
  }
}

void RoboContainerSetAlignItems(RoboContainer* container, RoboAlignItems align) {
  if (container != ROBO_NULL) {
    container->alignItems = align;
  }
}

void RoboContainerSetWrap(RoboContainer* container, RoboFlexWrap wrap) {
  if (container != ROBO_NULL) {
    container->wrap = wrap;
  }
}

void RoboContainerSetGap(RoboContainer* container, RoboI32 gap) {
  if (container != ROBO_NULL) {
    container->gap = gap;
    container->rowGap = gap;
    container->columnGap = gap;
  }
}

void RoboContainerSetPadding(RoboContainer* container, RoboI32 all) {
  if (container != ROBO_NULL) {
    container->paddingT = container->paddingR = container->paddingB = container->paddingL = all;
  }
}

void RoboContainerSetPaddingTRBL(RoboContainer* container, RoboI32 t, RoboI32 r, RoboI32 b, RoboI32 l) {
  if (container != ROBO_NULL) {
    container->paddingT = t;
    container->paddingR = r;
    container->paddingB = b;
    container->paddingL = l;
  }
}

void RoboContainerSetBackground(RoboContainer* container, RoboU32 colorARGB) {
  if (container != ROBO_NULL) {
    container->backgroundARGB = colorARGB;
  }
}

static RoboAlignItems RoboContainerResolveAlign(const RoboContainer* box, const RoboUIComponent* child) {
  if (child != ROBO_NULL) {
    if (child->layout.alignSelf == RoboAlignSelf::Start) {
      return RoboAlignItems::Start;
    }
    if (child->layout.alignSelf == RoboAlignSelf::Center) {
      return RoboAlignItems::Center;
    }
    if (child->layout.alignSelf == RoboAlignSelf::End) {
      return RoboAlignItems::End;
    }
    if (child->layout.alignSelf == RoboAlignSelf::Stretch) {
      return RoboAlignItems::Stretch;
    }
  }
  return box != ROBO_NULL ? box->alignItems : RoboAlignItems::Start;
}

static void RoboContainerResolveChildSize(RoboUIComponent* child, RoboI32 contentW, RoboI32 contentH, RoboBool horizontal,
                                          RoboAlignItems align, RoboI32* outMain, RoboI32* outCross) {
  const RoboU8 mode = child->layout.sizeMode;
  RoboI32 w = 0;
  RoboI32 h = 0;
  if (mode == 0) {
    w = child->width > 0 ? child->width : child->layout.preferWidth;
    h = child->height > 0 ? child->height : child->layout.preferHeight;
  } else if (mode == 5) {
    w = RoboUILayoutMinAxis(&child->layout, ROBO_TRUE, contentW);
    h = RoboUILayoutMinAxis(&child->layout, ROBO_FALSE, contentH);
    if (w <= 0) {
      w = child->layout.preferWidth;
    }
    if (h <= 0) {
      h = child->layout.preferHeight;
    }
  } else if (mode == 6) {
    w = RoboUILayoutMaxAxis(&child->layout, ROBO_TRUE, contentW);
    h = RoboUILayoutMaxAxis(&child->layout, ROBO_FALSE, contentH);
    if (w <= 0) {
      w = child->layout.preferWidth;
    }
    if (h <= 0) {
      h = child->layout.preferHeight;
    }
  } else {
    w = child->layout.preferWidth;
    h = child->layout.preferHeight;
  }
  if (child->layout.widthPercent > 0 && child->layout.widthPercent <= 100) {
    w = (contentW * static_cast<RoboI32>(child->layout.widthPercent)) / 100;
  }
  if (child->layout.heightPercent > 0 && child->layout.heightPercent <= 100) {
    h = (contentH * static_cast<RoboI32>(child->layout.heightPercent)) / 100;
  }
  if (align == RoboAlignItems::Stretch && mode != 1 && mode != 5 && mode != 6) {
    if (horizontal && h <= 0) {
      h = contentH - child->layout.marginT - child->layout.marginB;
    }
    if (!horizontal && w <= 0) {
      w = contentW - child->layout.marginL - child->layout.marginR;
    }
  }
  RoboUILayoutClampPair(&child->layout, contentW, contentH, &w, &h);
  child->width = w;
  child->height = h;
  if (outMain != ROBO_NULL) {
    *outMain = horizontal ? w : h;
  }
  if (outCross != ROBO_NULL) {
    *outCross = horizontal ? h : w;
  }
}

static void RoboContainerPlaceAbsolute(RoboUIComponent* child, RoboI32 contentW, RoboI32 contentH) {
  if (child == ROBO_NULL) {
    return;
  }
  RoboI32 w = 0;
  RoboI32 h = 0;
  RoboContainerResolveChildSize(child, contentW, contentH, ROBO_TRUE, RoboAlignItems::Start, &w, &h);
  (void)w;
  (void)h;

  RoboI32 x = child->x;
  RoboI32 y = child->y;
  if (child->layout.left != ROBO_LAYOUT_AUTO) {
    x = child->layout.left;
  } else if (child->layout.right != ROBO_LAYOUT_AUTO) {
    x = contentW - child->width - child->layout.right;
  }
  if (child->layout.top != ROBO_LAYOUT_AUTO) {
    y = child->layout.top;
  } else if (child->layout.bottom != ROBO_LAYOUT_AUTO) {
    y = contentH - child->height - child->layout.bottom;
  }
  child->x = x + child->layout.marginL;
  child->y = y + child->layout.marginT;
}

static void RoboContainerLayoutFlexLine(RoboContainer* box, RoboUIComponent** items, RoboU32 count, RoboBool horizontal,
                                        RoboBool reverse, RoboI32 mainMax, RoboI32 crossMax, RoboI32 crossOffset,
                                        RoboI32 mainGap, RoboBool fillCross) {
  if (box == ROBO_NULL || items == ROBO_NULL || count == 0) {
    return;
  }

  RoboI32 mainSizes[ROBO_UI_MAX_CHILDREN];
  RoboI32 crossSizes[ROBO_UI_MAX_CHILDREN];
  RoboU32 totalGrow = 0;
  RoboI32 totalMain = 0;

  for (RoboU32 i = 0; i < count; ++i) {
    RoboUIComponent* child = items[i];
    const RoboAlignItems align = RoboContainerResolveAlign(box, child);
    RoboI32 main = 0;
    RoboI32 cross = 0;
    RoboContainerResolveChildSize(child, horizontal ? mainMax : crossMax, horizontal ? crossMax : mainMax, horizontal,
                                  align, &main, &cross);
    mainSizes[i] = main;
    crossSizes[i] = cross;
    const RoboI32 marginMain =
        horizontal ? (child->layout.marginL + child->layout.marginR) : (child->layout.marginT + child->layout.marginB);
    totalMain += main + marginMain;
    totalGrow += child->layout.flexGrow;
  }
  if (count > 1) {
    totalMain += mainGap * static_cast<RoboI32>(count - 1);
  }

  RoboI32 freeMain = mainMax - totalMain;
  if (freeMain < 0) {
    freeMain = 0;
  }
  if (totalGrow > 0 && freeMain > 0) {
    RoboI32 distributed = 0;
    for (RoboU32 i = 0; i < count; ++i) {
      if (items[i]->layout.flexGrow > 0) {
        const RoboI32 add =
            (freeMain * static_cast<RoboI32>(items[i]->layout.flexGrow)) / static_cast<RoboI32>(totalGrow);
        mainSizes[i] += add;
        const RoboI32 limit = RoboUILayoutMaxAxis(&items[i]->layout, horizontal, mainMax);
        if (limit > 0 && mainSizes[i] > limit) {
          mainSizes[i] = limit;
        }
        const RoboI32 floor = RoboUILayoutMinAxis(&items[i]->layout, horizontal, mainMax);
        if (floor > 0 && mainSizes[i] < floor) {
          mainSizes[i] = floor;
        }
        distributed += add;
      }
    }
    freeMain -= distributed;
    if (freeMain < 0) {
      freeMain = 0;
    }
  }

  RoboI32 cursor = 0;
  RoboI32 between = mainGap;
  if (box->justify == RoboJustifyContent::Center) {
    cursor = freeMain / 2;
  } else if (box->justify == RoboJustifyContent::End) {
    cursor = freeMain;
  } else if (box->justify == RoboJustifyContent::SpaceBetween && count > 1) {
    between = mainGap + freeMain / static_cast<RoboI32>(count - 1);
    cursor = 0;
  } else if (box->justify == RoboJustifyContent::SpaceAround && count > 0) {
    between = mainGap + freeMain / static_cast<RoboI32>(count);
    cursor = between / 2;
  } else if (box->justify == RoboJustifyContent::SpaceEvenly && count > 0) {
    between = mainGap + freeMain / static_cast<RoboI32>(count + 1);
    cursor = between;
  }

  RoboI32 lineCross = 0;
  for (RoboU32 i = 0; i < count; ++i) {
    RoboUIComponent* child = items[i];
    const RoboI32 crossWithMargin =
        crossSizes[i] +
        (horizontal ? (child->layout.marginT + child->layout.marginB) : (child->layout.marginL + child->layout.marginR));
    if (crossWithMargin > lineCross) {
      lineCross = crossWithMargin;
    }
  }
  if (fillCross && crossMax > lineCross) {
    lineCross = crossMax;
  }

  for (RoboU32 i = 0; i < count; ++i) {
    RoboUIComponent* child = items[i];
    const RoboAlignItems align = RoboContainerResolveAlign(box, child);
    RoboI32 cross = crossSizes[i];
    if (align == RoboAlignItems::Stretch) {
      cross = lineCross -
              (horizontal ? (child->layout.marginT + child->layout.marginB)
                          : (child->layout.marginL + child->layout.marginR));
      if (cross < 0) {
        cross = 0;
      }
      if (horizontal) {
        RoboUILayoutClampPair(&child->layout, mainMax, crossMax, ROBO_NULL, &cross);
      } else {
        RoboUILayoutClampPair(&child->layout, crossMax, mainMax, &cross, ROBO_NULL);
      }
      crossSizes[i] = cross;
    }

    const RoboI32 marginMainStart = horizontal ? child->layout.marginL : child->layout.marginT;
    const RoboI32 marginMainEnd = horizontal ? child->layout.marginR : child->layout.marginB;
    const RoboI32 marginCrossStart = horizontal ? child->layout.marginT : child->layout.marginL;
    const RoboI32 marginCrossEnd = horizontal ? child->layout.marginB : child->layout.marginR;

    RoboI32 crossPos = crossOffset + marginCrossStart;
    if (align == RoboAlignItems::Center) {
      crossPos = crossOffset + (lineCross - cross - marginCrossStart - marginCrossEnd) / 2 + marginCrossStart;
    } else if (align == RoboAlignItems::End) {
      crossPos = crossOffset + lineCross - cross - marginCrossEnd;
    }

    RoboI32 mainStart = cursor + marginMainStart;
    if (reverse) {
      mainStart = mainMax - cursor - marginMainEnd - mainSizes[i];
    }
    if (horizontal) {
      child->width = mainSizes[i];
      child->height = cross;
      child->x = mainStart;
      child->y = crossPos;
    } else {
      child->height = mainSizes[i];
      child->width = cross;
      child->y = mainStart;
      child->x = crossPos;
    }
    cursor += mainSizes[i] + marginMainStart + marginMainEnd;
    if (i + 1 < count) {
      cursor += between;
    }
  }
}

static void RoboContainerSortFlow(RoboUIComponent** flow, RoboU32 count) {
  for (RoboU32 i = 1; i < count; ++i) {
    RoboUIComponent* item = flow[i];
    const RoboI16 order = item->layout.order;
    RoboU32 j = i;
    while (j > 0 && flow[j - 1]->layout.order > order) {
      flow[j] = flow[j - 1];
      --j;
    }
    flow[j] = item;
  }
}

static void RoboContainerShiftCross(RoboUIComponent* child, RoboBool horizontal, RoboI32 shift) {
  if (child == ROBO_NULL || shift == 0) {
    return;
  }
  if (horizontal) {
    child->y += shift;
  } else {
    child->x += shift;
  }
}

static void RoboContainerApplyAlignContent(RoboContainer* box, RoboUIComponent** flow, RoboU32 count,
                                           RoboBool horizontal, RoboI32 crossMax, const RoboI32* lineOrigin,
                                           const RoboI32* lineSize, RoboU32 lineCount) {
  if (box == ROBO_NULL || count == 0 || lineCount == 0 || box->alignContent == RoboJustifyContent::Start) {
    return;
  }
  RoboI32 packed = 0;
  for (RoboU32 line = 0; line < lineCount; ++line) {
    packed += lineSize[line];
  }
  if (lineCount > 1) {
    packed += (horizontal ? box->rowGap : box->columnGap) * static_cast<RoboI32>(lineCount - 1);
  }
  const RoboI32 extra = crossMax - packed;
  if (extra <= 0) {
    return;
  }
  RoboI32 shift = 0;
  RoboI32 step = 0;
  if (box->alignContent == RoboJustifyContent::Center) {
    shift = extra / 2;
  } else if (box->alignContent == RoboJustifyContent::End) {
    shift = extra;
  } else if (box->alignContent == RoboJustifyContent::SpaceBetween && lineCount > 1) {
    step = extra / static_cast<RoboI32>(lineCount - 1);
  } else if (box->alignContent == RoboJustifyContent::SpaceAround) {
    step = extra / static_cast<RoboI32>(lineCount);
    shift = step / 2;
  } else if (box->alignContent == RoboJustifyContent::SpaceEvenly) {
    step = extra / static_cast<RoboI32>(lineCount + 1);
    shift = step;
  } else {
    return;
  }
  for (RoboU32 i = 0; i < count; ++i) {
    RoboUIComponent* child = flow[i];
    const RoboI32 pos = horizontal ? child->y : child->x;
    RoboU32 lineIndex = 0;
    for (RoboU32 line = 0; line < lineCount; ++line) {
      const RoboI32 end = lineOrigin[line] + lineSize[line];
      if (pos >= lineOrigin[line] && pos < end) {
        lineIndex = line;
        break;
      }
    }
    RoboContainerShiftCross(child, horizontal, shift + step * static_cast<RoboI32>(lineIndex));
  }
}

static RoboI32 RoboContainerGridSpan(RoboI16 span) {
  return span > 0 ? span : 1;
}

static RoboBool RoboContainerGridFits(const RoboU8 taken[32][16], RoboI32 columns, RoboI32 col, RoboI32 row,
                                      RoboI32 colSpan, RoboI32 rowSpan) {
  if (col < 0 || row < 0 || colSpan < 1 || rowSpan < 1) {
    return ROBO_FALSE;
  }
  if (col + colSpan > columns || row + rowSpan > 32) {
    return ROBO_FALSE;
  }
  for (RoboI32 r = row; r < row + rowSpan; ++r) {
    for (RoboI32 c = col; c < col + colSpan; ++c) {
      if (taken[r][c] != 0) {
        return ROBO_FALSE;
      }
    }
  }
  return ROBO_TRUE;
}

static void RoboContainerGridMark(RoboU8 taken[32][16], RoboI32 col, RoboI32 row, RoboI32 colSpan, RoboI32 rowSpan) {
  for (RoboI32 r = row; r < row + rowSpan; ++r) {
    for (RoboI32 c = col; c < col + colSpan; ++c) {
      taken[r][c] = 1;
    }
  }
}

static void RoboContainerLayoutGrid(RoboContainer* box, RoboUIComponent** flow, RoboU32 flowCount, RoboI32 contentW,
                                    RoboI32 contentH) {
  RoboI32 columns = box->columns > 0 ? box->columns : 1;
  if (columns > 16) {
    columns = 16;
  }
  const RoboI32 colGap = box->columnGap > 0 ? box->columnGap : 0;
  const RoboI32 rowGap = box->rowGap > 0 ? box->rowGap : 0;
  RoboI32 cellW = columns > 1 ? (contentW - colGap * (columns - 1)) / columns : contentW;
  if (cellW < 1) {
    cellW = 1;
  }

  RoboU8 taken[32][16];
  RoboMemorySet(taken, 0, sizeof(taken));
  RoboI32 placeC[ROBO_UI_MAX_CHILDREN];
  RoboI32 placeR[ROBO_UI_MAX_CHILDREN];
  RoboI32 placeCs[ROBO_UI_MAX_CHILDREN];
  RoboI32 placeRs[ROBO_UI_MAX_CHILDREN];
  RoboI32 cursorC = 0;
  RoboI32 cursorR = 0;
  RoboI32 usedRows = 0;

  for (RoboU32 i = 0; i < flowCount; ++i) {
    RoboUIComponent* child = flow[i];
    RoboI32 colSpan = RoboContainerGridSpan(child->layout.gridColSpan);
    RoboI32 rowSpan = RoboContainerGridSpan(child->layout.gridRowSpan);
    if (colSpan > columns) {
      colSpan = columns;
    }
    const RoboI32 colStart = child->layout.gridColStart > 0 ? child->layout.gridColStart - 1 : -1;
    const RoboI32 rowStart = child->layout.gridRowStart > 0 ? child->layout.gridRowStart - 1 : -1;
    RoboI32 col = 0;
    RoboI32 row = 0;
    RoboBool placed = ROBO_FALSE;
    if (colStart >= 0 && rowStart >= 0) {
      col = colStart;
      row = rowStart;
      placed = RoboContainerGridFits(taken, columns, col, row, colSpan, rowSpan);
    } else if (colStart >= 0) {
      col = colStart;
      for (row = 0; row < 32; ++row) {
        if (RoboContainerGridFits(taken, columns, col, row, colSpan, rowSpan)) {
          placed = ROBO_TRUE;
          break;
        }
      }
    } else if (rowStart >= 0) {
      row = rowStart;
      for (col = 0; col <= columns - colSpan; ++col) {
        if (RoboContainerGridFits(taken, columns, col, row, colSpan, rowSpan)) {
          placed = ROBO_TRUE;
          break;
        }
      }
    } else {
      col = cursorC;
      row = cursorR;
      while (row < 32) {
        if (col + colSpan > columns) {
          col = 0;
          ++row;
          continue;
        }
        if (RoboContainerGridFits(taken, columns, col, row, colSpan, rowSpan)) {
          placed = ROBO_TRUE;
          cursorC = col + colSpan;
          cursorR = row;
          if (cursorC >= columns) {
            cursorC = 0;
            cursorR = row + rowSpan;
          }
          break;
        }
        ++col;
      }
    }
    if (!placed) {
      placeC[i] = 0;
      placeR[i] = 0;
      placeCs[i] = 1;
      placeRs[i] = 1;
      continue;
    }
    RoboContainerGridMark(taken, col, row, colSpan, rowSpan);
    placeC[i] = col;
    placeR[i] = row;
    placeCs[i] = colSpan;
    placeRs[i] = rowSpan;
    if (row + rowSpan > usedRows) {
      usedRows = row + rowSpan;
    }
  }
  if (usedRows < 1) {
    usedRows = 1;
  }
  if (usedRows > 32) {
    usedRows = 32;
  }

  RoboI32 rowH[32];
  RoboMemorySet(rowH, 0, sizeof(rowH));
  if (box->rows > 0) {
    RoboI32 rowCount = box->rows > 32 ? 32 : box->rows;
    if (rowCount < usedRows) {
      rowCount = usedRows;
    }
    const RoboI32 gaps = rowGap * (rowCount - 1);
    RoboI32 each = rowCount > 0 ? (contentH - gaps) / rowCount : contentH;
    if (each < 1) {
      each = 1;
    }
    for (RoboI32 r = 0; r < 32; ++r) {
      rowH[r] = each;
    }
    usedRows = rowCount;
  } else {
    for (RoboU32 i = 0; i < flowCount; ++i) {
      RoboUIComponent* child = flow[i];
      const RoboI32 slotW = cellW * placeCs[i] + colGap * (placeCs[i] - 1);
      RoboContainerResolveChildSize(child, slotW, contentH, ROBO_TRUE, RoboAlignItems::Start, ROBO_NULL, ROBO_NULL);
      RoboI32 h = child->height > 0 ? child->height : child->layout.preferHeight;
      if (h < 1) {
        h = 1;
      }
      const RoboI32 each = h / placeRs[i] > 0 ? h / placeRs[i] : h;
      for (RoboI32 k = 0; k < placeRs[i] && placeR[i] + k < 32; ++k) {
        if (rowH[placeR[i] + k] < each) {
          rowH[placeR[i] + k] = each;
        }
      }
    }
  }

  RoboI32 totalH = 0;
  for (RoboI32 r = 0; r < usedRows; ++r) {
    if (rowH[r] < 1) {
      rowH[r] = 1;
    }
    totalH += rowH[r];
  }
  totalH += rowGap * (usedRows - 1);
  RoboI32 yShift = 0;
  const RoboI32 extra = contentH - totalH;
  if (extra > 0) {
    if (box->alignContent == RoboJustifyContent::Center) {
      yShift = extra / 2;
    } else if (box->alignContent == RoboJustifyContent::End) {
      yShift = extra;
    }
  }

  for (RoboU32 i = 0; i < flowCount; ++i) {
    RoboUIComponent* child = flow[i];
    const RoboI32 slotW = cellW * placeCs[i] + colGap * (placeCs[i] - 1);
    RoboI32 slotH = 0;
    for (RoboI32 k = 0; k < placeRs[i] && placeR[i] + k < 32; ++k) {
      slotH += rowH[placeR[i] + k];
    }
    slotH += rowGap * (placeRs[i] - 1);
    if (slotH < 1) {
      slotH = 1;
    }
    const RoboAlignItems align = RoboContainerResolveAlign(box, child);
    const RoboAlignItems justify =
        box->justifyItems != static_cast<RoboAlignItems>(0) ? box->justifyItems : RoboAlignItems::Stretch;
    RoboI32 w = child->layout.preferWidth;
    RoboI32 h = child->layout.preferHeight;
    if (child->layout.widthPercent > 0) {
      w = (slotW * static_cast<RoboI32>(child->layout.widthPercent)) / 100;
    }
    if (child->layout.heightPercent > 0) {
      h = (slotH * static_cast<RoboI32>(child->layout.heightPercent)) / 100;
    }
    if (justify == RoboAlignItems::Stretch && child->layout.preferWidth <= 0 && child->layout.widthPercent == 0) {
      w = slotW - child->layout.marginL - child->layout.marginR;
    }
    if (align == RoboAlignItems::Stretch && child->layout.preferHeight <= 0 && child->layout.heightPercent == 0) {
      h = slotH - child->layout.marginT - child->layout.marginB;
    }
    RoboUILayoutClampPair(&child->layout, slotW, slotH, &w, &h);
    if (w < 1) {
      w = 1;
    }
    if (h < 1) {
      h = 1;
    }
    RoboI32 ox = child->layout.marginL;
    RoboI32 oy = child->layout.marginT;
    if (justify == RoboAlignItems::Center) {
      ox = (slotW - w) / 2;
    } else if (justify == RoboAlignItems::End) {
      ox = slotW - w - child->layout.marginR;
    }
    if (align == RoboAlignItems::Center) {
      oy = (slotH - h) / 2;
    } else if (align == RoboAlignItems::End) {
      oy = slotH - h - child->layout.marginB;
    }
    RoboI32 y = yShift;
    for (RoboI32 r = 0; r < placeR[i]; ++r) {
      y += rowH[r] + rowGap;
    }
    child->x = placeC[i] * (cellW + colGap) + ox;
    child->y = y + oy;
    child->width = w;
    child->height = h;
  }
}

void RoboContainerLayoutChildren(RoboContainer* box, RoboI32 contentW, RoboI32 contentH) {
  if (box == ROBO_NULL || contentW < 0 || contentH < 0) {
    return;
  }

  RoboUIComponent* flow[ROBO_UI_MAX_CHILDREN];
  RoboU32 flowCount = 0;
  for (RoboU32 i = 0; i < box->base.childCount; ++i) {
    RoboUIComponent* child = box->base.children[i];
    if (child == ROBO_NULL) {
      continue;
    }
    if (child->layout.position == RoboUIPosition::Absolute || child->layout.position == RoboUIPosition::Fixed) {
      /* Fixed is remapped to viewport in RoboUILayoutTree; Absolute uses this content box. */
      if (child->layout.position == RoboUIPosition::Absolute) {
        RoboContainerPlaceAbsolute(child, contentW, contentH);
      }
      continue;
    }
    if (flowCount < ROBO_UI_MAX_CHILDREN) {
      flow[flowCount++] = child;
    }
  }
  RoboContainerSortFlow(flow, flowCount);

  if (box->display == RoboContainerDisplay::Stack) {
    for (RoboU32 i = 0; i < flowCount; ++i) {
      RoboUIComponent* child = flow[i];
      RoboContainerResolveChildSize(child, contentW, contentH, ROBO_TRUE, RoboAlignItems::Start, ROBO_NULL, ROBO_NULL);
    }
    return;
  }

  if (box->display == RoboContainerDisplay::Grid) {
    RoboContainerLayoutGrid(box, flow, flowCount, contentW, contentH);
    return;
  }

  const RoboBool horizontal =
      (box->direction == RoboFlexDirection::Row || box->direction == RoboFlexDirection::RowReverse) ? ROBO_TRUE
                                                                                                    : ROBO_FALSE;
  const RoboBool reverse =
      (box->direction == RoboFlexDirection::RowReverse || box->direction == RoboFlexDirection::ColumnReverse)
          ? ROBO_TRUE
          : ROBO_FALSE;
  const RoboI32 mainGap = horizontal ? box->columnGap : box->rowGap;
  const RoboI32 crossGap = horizontal ? box->rowGap : box->columnGap;
  const RoboI32 mainMax = horizontal ? contentW : contentH;
  const RoboI32 crossMax = horizontal ? contentH : contentW;

  if (box->wrap == RoboFlexWrap::NoWrap || flowCount <= 1) {
    RoboContainerLayoutFlexLine(box, flow, flowCount, horizontal, reverse, mainMax, crossMax, 0, mainGap, ROBO_TRUE);
    RoboI32 origin = 0;
    RoboI32 size = crossMax;
    RoboContainerApplyAlignContent(box, flow, flowCount, horizontal, crossMax, &origin, &size, 1);
    return;
  }

  /* Wrap — pack children into lines (GRUB gui_box + CSS flex-wrap). */
  RoboUIComponent* line[ROBO_UI_MAX_CHILDREN];
  RoboU32 lineCount = 0;
  RoboI32 lineMainUsed = 0;
  RoboI32 crossCursor = 0;
  RoboI32 lineOrigin[ROBO_UI_MAX_CHILDREN];
  RoboI32 lineSize[ROBO_UI_MAX_CHILDREN];
  RoboU32 packedLines = 0;

  for (RoboU32 i = 0; i < flowCount; ++i) {
    RoboUIComponent* child = flow[i];
    const RoboAlignItems align = RoboContainerResolveAlign(box, child);
    RoboI32 main = 0;
    RoboI32 cross = 0;
    RoboContainerResolveChildSize(child, contentW, contentH, horizontal, align, &main, &cross);
    const RoboI32 marginMain =
        horizontal ? (child->layout.marginL + child->layout.marginR) : (child->layout.marginT + child->layout.marginB);
    const RoboI32 need = main + marginMain + (lineCount > 0 ? mainGap : 0);

    if (lineCount > 0 && lineMainUsed + need > mainMax) {
      RoboContainerLayoutFlexLine(box, line, lineCount, horizontal, reverse, mainMax, crossMax, crossCursor, mainGap,
                                 ROBO_FALSE);
      RoboI32 lineCross = 0;
      for (RoboU32 li = 0; li < lineCount; ++li) {
        const RoboI32 c =
            (horizontal ? line[li]->height : line[li]->width) +
            (horizontal ? (line[li]->layout.marginT + line[li]->layout.marginB)
                        : (line[li]->layout.marginL + line[li]->layout.marginR));
        if (c > lineCross) {
          lineCross = c;
        }
      }
      if (packedLines < ROBO_UI_MAX_CHILDREN) {
        lineOrigin[packedLines] = crossCursor;
        lineSize[packedLines] = lineCross;
        ++packedLines;
      }
      crossCursor += lineCross + crossGap;
      lineCount = 0;
      lineMainUsed = 0;
    }

    line[lineCount++] = child;
    lineMainUsed += need;
  }

  if (lineCount > 0) {
    const RoboBool onlyLine = packedLines == 0 ? ROBO_TRUE : ROBO_FALSE;
    RoboContainerLayoutFlexLine(box, line, lineCount, horizontal, reverse, mainMax, crossMax, crossCursor, mainGap,
                               onlyLine);
    RoboI32 lineCross = onlyLine && crossMax > 0 ? crossMax : 0;
    if (!onlyLine) {
      for (RoboU32 li = 0; li < lineCount; ++li) {
        const RoboI32 c =
            (horizontal ? line[li]->height : line[li]->width) +
            (horizontal ? (line[li]->layout.marginT + line[li]->layout.marginB)
                        : (line[li]->layout.marginL + line[li]->layout.marginR));
        if (c > lineCross) {
          lineCross = c;
        }
      }
    }
    if (packedLines < ROBO_UI_MAX_CHILDREN) {
      lineOrigin[packedLines] = crossCursor;
      lineSize[packedLines] = lineCross;
      ++packedLines;
    }
  }
  RoboContainerApplyAlignContent(box, flow, flowCount, horizontal, crossMax, lineOrigin, lineSize, packedLines);
}

void RoboUILayoutPlace(RoboUIComponent* node, RoboI32 contentW, RoboI32 contentH) {
  if (node == ROBO_NULL || node->childCount == 0 || contentW < 0 || contentH < 0) {
    return;
  }
  RoboContainer* box = RoboContainerFromComponent(node);
  if (box != ROBO_NULL) {
    RoboContainerPullStyle(box);
    RoboContainerLayoutChildren(box, contentW, contentH);
    return;
  }
  if (node->layout.boxDisplay == 0) {
    return;
  }
  RoboContainer shell;
  RoboContainerInit(&shell, "place");
  shell.base.layout = node->layout;
  RoboContainerPullStyle(&shell);
  shell.base.childCount = node->childCount;
  for (RoboU32 i = 0; i < node->childCount && i < ROBO_UI_MAX_CHILDREN; ++i) {
    shell.base.children[i] = node->children[i];
  }
  RoboContainerLayoutChildren(&shell, contentW, contentH);
}
