#include "RoboWindows.hpp"
#include "RoboWindowsCompiled.hpp"

#include "../../Components/Button/ButtonPlatform.hpp"
#include "../../Components/IconButton/IconButtonPlatform.hpp"
#include "../../Components/TextField/TextFieldPlatform.hpp"
#include "../../Components/Switch/SwitchPlatform.hpp"
#include "../../Components/Icon/IconPlatform.hpp"
#include "../../Foundation/FoundationPlatform.hpp"
#include "../../Components/Heading/HeadingPlatform.hpp"
#include "../../Components/Paragraph/ParagraphPlatform.hpp"
#include "../../Components/Text/Text.hpp"
#include "../../Components/Text/TextPlatform.hpp"

#include "../../../RoboGPU/RoboGPU.hpp"
#include "../../../RoboShared/RoboFontPack.hpp"
#include "../../../RoboUI/RoboContainer.hpp"
#include "../../../RoboUI/RoboDock.hpp"
#include "../../../RoboUI/RoboScrollView.hpp"
#include "../../../RoboUI/RoboTextField.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

constexpr RoboU32 ROBO_WINDOWS_DESIGN_WIDTH = 960;
constexpr RoboU32 ROBO_WINDOWS_DESIGN_HEIGHT = 640;

struct RoboWindowsView {
  HWND window;
  ID3D11Device* device;
  ID3D11DeviceContext* context;
  IDXGISwapChain1* swap;
  ID3D11Texture2D* upload;
  ID3D11Query* uploadQuery;
  RoboBool uploadBusy;
  RoboBool flip;
  RoboGPU gpu;
  RoboU8* front;
  RoboU8* back;
  RoboU8* fontBytes;
  RoboSize fontBytesCount;
  RoboFontPackView fontPack;
  RoboUIText* items;
  RoboU16* designSize;
  int itemCount;
  RoboContainer* boxes;
  int boxCount;
  RoboIcon* icons;
  RoboU16* iconDesignSize;
  int iconCount;
  RoboButton* buttons;
  int buttonCount;
  RoboIconButton* iconButtons;
  int iconButtonCount;
  RoboTextField* fields;
  int fieldCount;
  RoboSwitch* switches;
  int switchCount;
  int caretTick;
  int clickCount;
  int clickField;
  DWORD clickTick;
  wchar_t pendingChar;
  RoboBool spinButtons;
  int hoverIndex;
  int pressIndex;
  int focusIndex;
  RoboBool trackLeave;
  RoboU8* iconBytes;
  RoboSize iconBytesCount;
  RoboIconPackView iconPack;
  RoboUIComponent** components;
  int nodeCount;
  RoboUIComponent* root;
  int paddingDesign;
  int gapDesign;
  int scrollY;
  int scrollTarget;
  int contentHeight;
  RoboU8* sheet;
  RoboU32 sheetWidth;
  RoboU32 sheetHeight;
  RoboBool sheetBound;
  int shownScroll;
  int gpuScroll;
  RoboBool gpuFull;
  RoboBool bufferReady[2];
  int presentIndex;
  RECT recent[2][16];
  int recentCount[2];
  int presentDelta;
  RECT notes[24];
  int noteCount;
  RoboBool noteFull;
  DWORD pace;
  int barOpacity;
  int barShown;
  int barShownPart;
  int barShownHold;
  RoboBool barShownDrag;
  RoboBool barHot;
  int barPart;
  int barHold;
  int barGrab;
  DWORD barIdle;
  DWORD barRepeat;
  RoboBool barDrag;
  UINT dpi;
  RoboU32 pixelWidth;
  RoboU32 pixelHeight;
  RoboU32 layoutWidth;
  RoboU32 layoutHeight;
  RoboBool fullPaint;
  RoboBool frameReady;
  int dirtyButtons[128];
  int dirtyCount;
  RoboBool spinFrame;
  RoboI32 margin;
  RoboI32 gap;
  RoboBool ready;
  RoboBool inResize;
};

/* The window does not paint docks or fields. Those symbols stay in the OS build. */
RoboBool RoboDockShouldDeferPaint(const RoboUIComponent*) {
  return ROBO_FALSE;
}
RoboDockTooltipView* RoboDockTooltipFromComponent(RoboUIComponent*) {
  return ROBO_NULL;
}
RoboScrollView* RoboScrollViewFromComponent(RoboUIComponent*) {
  return ROBO_NULL;
}
RoboBool RoboScrollViewHandleInput(RoboScrollView*, const RoboInputEvent*, RoboI32, RoboI32, RoboBool*) {
  return ROBO_FALSE;
}

static void RoboWindowsRelease(RoboWindowsView* view) {
  if (view == nullptr) {
    return;
  }
  if (view->upload != nullptr) {
    view->upload->Release();
    view->upload = nullptr;
  }
  if (view->uploadQuery != nullptr) {
    view->uploadQuery->Release();
    view->uploadQuery = nullptr;
  }
  view->uploadBusy = ROBO_FALSE;
  if (view->swap != nullptr) {
    view->swap->Release();
    view->swap = nullptr;
  }
  if (view->context != nullptr) {
    view->context->Release();
    view->context = nullptr;
  }
  if (view->device != nullptr) {
    view->device->Release();
    view->device = nullptr;
  }
  _aligned_free(view->front);
  _aligned_free(view->back);
  _aligned_free(view->sheet);
  std::free(view->fontBytes);
  std::free(view->iconBytes);
  delete[] view->items;
  delete[] view->designSize;
  delete[] view->boxes;
  delete[] view->icons;
  delete[] view->iconDesignSize;
  delete[] view->buttons;
  delete[] view->iconButtons;
  delete[] view->fields;
  delete[] view->switches;
  delete[] view->components;
  view->front = nullptr;
  view->back = nullptr;
  view->sheet = nullptr;
  view->fontBytes = nullptr;
  view->items = nullptr;
  view->designSize = nullptr;
  view->boxes = nullptr;
  view->icons = nullptr;
  view->iconDesignSize = nullptr;
  view->buttons = nullptr;
  view->iconButtons = nullptr;
  view->fields = nullptr;
  view->switches = nullptr;
  view->iconBytes = nullptr;
  view->components = nullptr;
  view->root = nullptr;
}

void RoboWindowsInstallFontRaster(const RoboU8* pack, RoboSize packBytes);

static RoboBool RoboWindowsLoadFont(RoboWindowsView* view, const char* fontPath) {
  FILE* file = nullptr;
  if (fontPath == nullptr || fontPath[0] == 0 || fopen_s(&file, fontPath, "rb") != 0 || file == nullptr) {
    return ROBO_FALSE;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return ROBO_FALSE;
  }
  const long length = ftell(file);
  if (length <= 0) {
    fclose(file);
    return ROBO_FALSE;
  }
  rewind(file);
  auto* bytes = static_cast<RoboU8*>(std::malloc(static_cast<size_t>(length)));
  if (bytes == nullptr) {
    fclose(file);
    return ROBO_FALSE;
  }
  const size_t read = fread(bytes, 1, static_cast<size_t>(length), file);
  fclose(file);
  if (read != static_cast<size_t>(length) || !RoboFontPackValidate(bytes, read)) {
    std::free(bytes);
    return ROBO_FALSE;
  }
  view->fontBytes = bytes;
  view->fontBytesCount = read;
  view->fontPack = RoboFontPackMakeView(bytes, read);
  RoboWindowsInstallFontRaster(bytes, read);
  return ROBO_TRUE;
}

static RoboBool RoboWindowsLoadIcons(RoboWindowsView* view, const char* iconPath) {
  FILE* file = nullptr;
  if (view == nullptr || iconPath == nullptr || iconPath[0] == 0 || fopen_s(&file, iconPath, "rb") != 0 ||
      file == nullptr) {
    return ROBO_FALSE;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return ROBO_FALSE;
  }
  const long length = ftell(file);
  if (length <= 0) {
    fclose(file);
    return ROBO_FALSE;
  }
  rewind(file);
  auto* bytes = static_cast<RoboU8*>(std::malloc(static_cast<size_t>(length)));
  if (bytes == nullptr) {
    fclose(file);
    return ROBO_FALSE;
  }
  const size_t read = fread(bytes, 1, static_cast<size_t>(length), file);
  fclose(file);
  if (read != static_cast<size_t>(length) || !RoboIconPackValidate(bytes, read)) {
    std::free(bytes);
    return ROBO_FALSE;
  }
  view->iconBytes = bytes;
  view->iconBytesCount = read;
  view->iconPack = RoboIconPackMakeView(bytes, read);
  return ROBO_TRUE;
}

static RoboU16 RoboWindowsTypeSize(RoboU16 designPx, UINT dpi) {
  const UINT used = dpi == 0 ? 96u : dpi;
  const RoboU32 scaled = static_cast<RoboU32>(designPx) * used / 96u;
  return static_cast<RoboU16>(scaled < 1u ? 1u : scaled);
}

static RoboI32 RoboWindowsScale(RoboI32 designPx, UINT dpi) {
  const UINT used = dpi == 0 ? 96u : dpi;
  RoboI32 scaled = static_cast<RoboI32>(static_cast<RoboU32>(designPx) * used / 96u);
  return scaled < 1 ? 1 : scaled;
}

static void RoboWindowsRetargetSpans(RoboUIText* text) {
  if (text == nullptr) {
    return;
  }
  for (RoboU16 index = 0; index < text->spanCount; ++index) {
    text->spans[index].sizePx = text->sizePx;
  }
}

static int RoboWindowsScaleBox(int designPx, UINT dpi) {
  if (designPx <= 0) {
    return 0;
  }
  const UINT used = dpi == 0 ? 96u : dpi;
  return static_cast<int>(static_cast<unsigned>(designPx) * used / 96u);
}

static void RoboWindowsSetPx(RoboUIComponent* component, const char* key, int designPx, UINT dpi) {
  if (component == nullptr || key == nullptr || designPx < 0) {
    return;
  }
  char text[16];
  std::snprintf(text, sizeof(text), "%d", RoboWindowsScaleBox(designPx, dpi));
  RoboUISetProperty(component, key, text);
}

static void RoboWindowsApplyType(RoboWindowsView* view) {
  if (view == nullptr) {
    return;
  }
  RoboUIContext context{};
  context.gpu = &view->gpu;
  context.fontPack = view->fontPack;
  context.iconPack = view->iconPack;
  const int padding = view->paddingDesign > 0 ? view->paddingDesign : RoboFoundationPlatformSpace("space-4");
  const int gap = view->gapDesign > 0 ? view->gapDesign : RoboFoundationPlatformSpace("space-5");
  view->margin = RoboWindowsScale(padding, view->dpi);
  view->gap = RoboWindowsScale(gap, view->dpi);
  RoboWindowsCompiledScreen compiled{};
  RoboWindowsCompiledScreenGet(&compiled);
  int textIndex = 0;
  int boxIndex = 0;
  int iconIndex = 0;
  int buttonIndex = 0;
  int iconButtonIndex = 0;
  int fieldIndex = 0;
  int switchIndex = 0;
  for (int index = 0; index < compiled.count && view->components != nullptr; ++index) {
    const RoboWindowsCompiledNode& node = compiled.nodes[index];
    RoboUIComponent* component = view->components[index];
    if (node.container) {
      if (boxIndex < view->boxCount && component != nullptr) {
        if (node.display != nullptr && node.display[0] != 0) {
          RoboUISetProperty(component, "display", node.display);
        }
        if (node.direction != nullptr && node.direction[0] != 0) {
          RoboUISetProperty(component, "direction", node.direction);
        }
        if (node.justify != nullptr && node.justify[0] != 0) {
          RoboUISetProperty(component, "justify", node.justify);
        }
        if (node.align != nullptr && node.align[0] != 0) {
          RoboUISetProperty(component, "align", node.align);
        }
        if (node.wrap != nullptr && node.wrap[0] != 0) {
          RoboUISetProperty(component, "wrap", node.wrap);
        }
        RoboWindowsSetPx(component, "gap", node.gap, view->dpi);
        if (node.rowGap >= 0) {
          RoboWindowsSetPx(component, "rowGap", node.rowGap, view->dpi);
        }
        if (node.columnGap >= 0) {
          RoboWindowsSetPx(component, "columnGap", node.columnGap, view->dpi);
        }
        RoboWindowsSetPx(component, "paddingTop", node.padTop, view->dpi);
        RoboWindowsSetPx(component, "paddingRight", node.padRight, view->dpi);
        RoboWindowsSetPx(component, "paddingBottom", node.padBottom, view->dpi);
        RoboWindowsSetPx(component, "paddingLeft", node.padLeft, view->dpi);
        if (node.maxWidth > 0) {
          RoboWindowsSetPx(component, "maxWidth", node.maxWidth, view->dpi);
        }
        if (node.background != nullptr && node.background[0] != 0) {
          RoboUISetProperty(component, "background", node.background);
        }
        if (node.switchGroup) {
          component->layout.widthPercent = 100;
        }
      }
      ++boxIndex;
      continue;
    }
    if (node.icon) {
      RoboIcon* icon = RoboIconFromComponent(component);
      if (icon != nullptr && iconIndex < view->iconCount && view->iconDesignSize != nullptr) {
        RoboIconSetSize(icon, RoboWindowsTypeSize(view->iconDesignSize[iconIndex], view->dpi));
        if (node.fill != nullptr && node.fill[0] == '#') {
          RoboUISetProperty(component, "fill", node.fill);
        }
        if (node.stroke != nullptr && node.stroke[0] == '#') {
          RoboUISetProperty(component, "stroke", node.stroke);
        }
        if (node.accent != nullptr && node.accent[0] == '#') {
          RoboUISetProperty(component, "accent", node.accent);
        }
      }
      ++iconIndex;
      continue;
    }
    if (node.button) {
      if (buttonIndex < view->buttonCount && view->buttons != nullptr) {
        RoboButtonSync(&view->buttons[buttonIndex], &context, view->dpi);
      }
      ++buttonIndex;
      continue;
    }
    if (node.iconButton) {
      if (iconButtonIndex < view->iconButtonCount && view->iconButtons != nullptr) {
        RoboIconButtonSync(&view->iconButtons[iconButtonIndex], &context, view->dpi);
      }
      ++iconButtonIndex;
      continue;
    }
    if (node.textField) {
      if (fieldIndex < view->fieldCount && view->fields != nullptr) {
        RoboTextFieldSync(&view->fields[fieldIndex], &context, view->dpi);
      }
      ++fieldIndex;
      continue;
    }
    if (node.switchControl) {
      if (switchIndex < view->switchCount && view->switches != nullptr) {
        RoboSwitchSync(&view->switches[switchIndex], &context, view->dpi);
      }
      ++switchIndex;
      continue;
    }
    if (textIndex < view->itemCount && view->items != nullptr) {
      view->items[textIndex].sizePx = RoboWindowsTypeSize(view->designSize[textIndex], view->dpi);
      RoboWindowsRetargetSpans(&view->items[textIndex]);
      RoboUITextSetFontName(&view->items[textIndex], &context, view->items[textIndex].faceName);
      ++textIndex;
    }
  }
  view->layoutWidth = 0;
  view->layoutHeight = 0;
}

static void RoboWindowsBuildCompiled(RoboWindowsView* view) {
  RoboWindowsCompiledScreen compiled{};
  RoboWindowsCompiledScreenGet(&compiled);
  view->nodeCount = compiled.count;
  view->paddingDesign = RoboFoundationPlatformSpace("space-4");
  view->gapDesign = RoboFoundationPlatformSpace("space-5");
  if (compiled.count <= 0 || compiled.nodes == nullptr) {
    view->nodeCount = 0;
    return;
  }
  if (compiled.nodes[0].container) {
    view->paddingDesign = compiled.nodes[0].padTop;
    view->gapDesign = compiled.nodes[0].gap;
  }
  int textCount = 0;
  int boxCount = 0;
  int iconCount = 0;
  int buttonCount = 0;
  int iconButtonCount = 0;
  int fieldCount = 0;
  int switchCount = 0;
  for (int index = 0; index < compiled.count; ++index) {
    if (compiled.nodes[index].container) {
      ++boxCount;
    } else if (compiled.nodes[index].icon) {
      ++iconCount;
    } else if (compiled.nodes[index].button) {
      ++buttonCount;
    } else if (compiled.nodes[index].iconButton) {
      ++iconButtonCount;
    } else if (compiled.nodes[index].textField) {
      ++fieldCount;
    } else if (compiled.nodes[index].switchControl) {
      ++switchCount;
    } else {
      ++textCount;
    }
  }
  view->itemCount = textCount;
  view->boxCount = boxCount;
  view->iconCount = iconCount;
  view->buttonCount = buttonCount;
  view->iconButtonCount = iconButtonCount;
  view->fieldCount = fieldCount;
  view->switchCount = switchCount;
  view->items = textCount > 0 ? new RoboUIText[static_cast<size_t>(textCount)] : nullptr;
  view->designSize = textCount > 0 ? new RoboU16[static_cast<size_t>(textCount)] : nullptr;
  view->boxes = boxCount > 0 ? new RoboContainer[static_cast<size_t>(boxCount)] : nullptr;
  view->icons = iconCount > 0 ? new RoboIcon[static_cast<size_t>(iconCount)] : nullptr;
  view->iconDesignSize = iconCount > 0 ? new RoboU16[static_cast<size_t>(iconCount)] : nullptr;
  view->buttons = buttonCount > 0 ? new RoboButton[static_cast<size_t>(buttonCount)] : nullptr;
  view->iconButtons = iconButtonCount > 0 ? new RoboIconButton[static_cast<size_t>(iconButtonCount)] : nullptr;
  view->fields = fieldCount > 0 ? new RoboTextField[static_cast<size_t>(fieldCount)] : nullptr;
  view->switches = switchCount > 0 ? new RoboSwitch[static_cast<size_t>(switchCount)] : nullptr;
  view->components = new RoboUIComponent*[static_cast<size_t>(compiled.count)];
  RoboUIContext context{};
  context.fontPack = view->fontPack;
  context.iconPack = view->iconPack;
  int textIndex = 0;
  int boxIndex = 0;
  int iconIndex = 0;
  int buttonIndex = 0;
  int iconButtonIndex = 0;
  int fieldIndex = 0;
  int switchIndex = 0;
  for (int index = 0; index < compiled.count; ++index) {
    const RoboWindowsCompiledNode& node = compiled.nodes[index];
    if (node.container) {
      RoboContainer* box = &view->boxes[boxIndex++];
      RoboContainerInit(box, "compiled");
      view->components[index] = &box->base;
      continue;
    }
    if (node.icon) {
      RoboIcon* icon = &view->icons[iconIndex];
      const char* name = node.iconName != nullptr ? node.iconName : "";
      RoboIconInit(icon, "compiled", name);
      view->iconDesignSize[iconIndex] = static_cast<RoboU16>(node.iconSize > 0 ? node.iconSize : 24);
      RoboIconResolve(icon, &context);
      view->components[index] = &icon->base;
      ++iconIndex;
      continue;
    }
    if (node.button) {
      RoboButton* button = &view->buttons[buttonIndex];
      RoboButtonInit(button, "compiled");
      RoboButtonSetLabel(button, node.content);
      RoboButtonSetVariant(button, node.variant);
      RoboButtonSetSize(button, node.buttonSize);
      RoboButtonSetPreview(button, node.preview);
      RoboButtonSetIcons(button, node.leading, node.trailing);
      RoboButtonSetFlags(button, node.fullWidth ? ROBO_TRUE : ROBO_FALSE, node.loading ? ROBO_TRUE : ROBO_FALSE,
                         node.disabled ? ROBO_TRUE : ROBO_FALSE, node.loadingText);
      if (node.loading) {
        view->spinButtons = ROBO_TRUE;
      }
      view->components[index] = &button->base;
      ++buttonIndex;
      continue;
    }
    if (node.iconButton) {
      RoboIconButton* iconButton = &view->iconButtons[iconButtonIndex];
      RoboIconButtonInit(iconButton, "compiled");
      RoboIconButtonSetIcon(iconButton, node.iconName);
      RoboIconButtonSetVariant(iconButton, node.variant);
      RoboIconButtonSetSize(iconButton, node.buttonSize);
      const RoboBool controlled = node.status != nullptr && node.status[0] != 0 ? ROBO_TRUE : ROBO_FALSE;
      RoboIconButtonSetStatus(iconButton, node.status, controlled);
      RoboIconButtonSetBadge(iconButton, node.badge);
      RoboIconButtonSetAction(iconButton, controlled ? "" : node.action, node.successMs);
      RoboIconButtonSetDisabled(iconButton, node.disabled ? ROBO_TRUE : ROBO_FALSE);
      if ((node.status != nullptr && std::strcmp(node.status, "loading") == 0) ||
          (node.action != nullptr && node.action[0] != 0 && !controlled)) {
        view->spinButtons = ROBO_TRUE;
      }
      view->components[index] = &iconButton->base;
      ++iconButtonIndex;
      continue;
    }
    if (node.textField) {
      RoboTextField* field = &view->fields[fieldIndex];
      RoboTextFieldInit(field, "compiled");
      RoboUISetProperty(&field->base, "value", node.content != nullptr ? node.content : "");
      RoboUISetProperty(&field->base, "placeholder", node.placeholder != nullptr ? node.placeholder : "");
      RoboUISetProperty(&field->base, "label", node.labelText != nullptr ? node.labelText : "");
      RoboUISetProperty(&field->base, "hint", node.hint != nullptr ? node.hint : "");
      RoboUISetProperty(&field->base, "trailing", node.trailing != nullptr ? node.trailing : "");
      RoboUISetProperty(&field->base, "variant", node.variant != nullptr ? node.variant : "outline");
      RoboUISetProperty(&field->base, "size", node.buttonSize != nullptr ? node.buttonSize : "md");
      RoboUISetProperty(&field->base, "type", node.fieldType != nullptr ? node.fieldType : "text");
      RoboUISetProperty(&field->base, "preview", node.preview != nullptr ? node.preview : "");
      RoboUISetProperty(&field->base, "icon", node.iconName != nullptr ? node.iconName : "");
      RoboUISetProperty(&field->base, "invalid", node.invalid ? "true" : "false");
      RoboUISetProperty(&field->base, "valid", node.valid ? "true" : "false");
      RoboUISetProperty(&field->base, "disabled", node.disabled ? "true" : "false");
      RoboUISetProperty(&field->base, "readOnly", node.readOnly ? "true" : "false");
      RoboUISetProperty(&field->base, "required", node.required ? "true" : "false");
      RoboUISetProperty(&field->base, "optional", node.optional ? "true" : "false");
      char lengthText[16];
      std::snprintf(lengthText, sizeof(lengthText), "%d", node.maxLength);
      RoboUISetProperty(&field->base, "maxLength", lengthText);
      if (node.maxWidth > 0) {
        RoboWindowsSetPx(&field->base, "maxWidth", node.maxWidth, view->dpi);
      }
      view->components[index] = &field->base;
      ++fieldIndex;
      continue;
    }
    if (node.switchControl) {
      RoboSwitch* control = &view->switches[switchIndex];
      RoboSwitchInit(control, "compiled");
      RoboSwitchSetVariant(control, node.variant);
      RoboSwitchSetSize(control, node.buttonSize);
      RoboSwitchSetChecked(control, node.checked ? ROBO_TRUE : ROBO_FALSE);
      RoboSwitchSetCopy(control, node.switchItem ? node.content : "", node.hint);
      RoboSwitchSetFlags(control, node.disabled ? ROBO_TRUE : ROBO_FALSE, node.loading ? ROBO_TRUE : ROBO_FALSE,
                         node.switchItem ? ROBO_TRUE : ROBO_FALSE, node.divider ? ROBO_TRUE : ROBO_FALSE);
      if (node.loading) {
        view->spinButtons = ROBO_TRUE;
      }
      view->components[index] = &control->base;
      ++switchIndex;
      continue;
    }
    RoboUIText* text = &view->items[textIndex];
    const auto kind = static_cast<RoboCompilerTextKind>(node.textKind);
    if (kind == RoboCompilerTextKind::Heading) {
      RoboUITextInitHeading(text, "compiled", node.level);
    } else if (kind == RoboCompilerTextKind::Body) {
      RoboUITextInitParagraph(text, "compiled");
      if (node.bodySmall) {
        RoboUITextApplyRole(text, RoboUITextRole::BodySmall);
      }
    } else {
      RoboUITextInit(text, "compiled");
      RoboUITextRole role = RoboUITextRole::Rich;
      if (kind == RoboCompilerTextKind::Subhead) {
        role = RoboUITextRole::Subhead;
      } else if (kind == RoboCompilerTextKind::Caption) {
        role = RoboUITextRole::Caption;
      } else if (kind == RoboCompilerTextKind::Eyebrow) {
        role = RoboUITextRole::Eyebrow;
      } else if (kind == RoboCompilerTextKind::Label) {
        role = RoboUITextRole::Label;
      }
      RoboUITextApplyRole(text, role);
    }
    const char* face = node.font != nullptr && node.font[0] != 0 ? node.font : "Inter-Regular";
    RoboUITextSetFontName(text, &context, face);
    RoboUITextSetContent(text, node.content != nullptr ? node.content : "");
    view->designSize[textIndex] = text->sizePx;
    view->components[index] = &text->base;
    ++textIndex;
  }
  for (int index = 0; index < compiled.count; ++index) {
    const RoboWindowsCompiledNode& node = compiled.nodes[index];
    if (compiled.childIds == nullptr || node.childCount <= 0) {
      continue;
    }
    for (int child = 0; child < node.childCount; ++child) {
      const int childIndex = compiled.childIds[node.childBegin + child];
      if (childIndex < 0 || childIndex >= compiled.count) {
        continue;
      }
      RoboUIAddChild(view->components[index], view->components[childIndex]);
    }
  }
  view->root = view->components[0];
}

static RoboBool RoboWindowsEnsureSurface(RoboWindowsView* view, RoboU32 width, RoboU32 height) {
  if (view == nullptr || view->swap == nullptr || view->device == nullptr || width == 0 || height == 0) {
    return ROBO_FALSE;
  }
  if (view->upload != nullptr && view->pixelWidth == width && view->pixelHeight == height && view->front != nullptr &&
      view->back != nullptr) {
    return ROBO_TRUE;
  }
  if (view->upload != nullptr) {
    view->upload->Release();
    view->upload = nullptr;
  }
  if (view->uploadQuery != nullptr) {
    view->uploadQuery->Release();
    view->uploadQuery = nullptr;
  }
  view->uploadBusy = ROBO_FALSE;
  if (FAILED(view->swap->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) {
    return ROBO_FALSE;
  }
  D3D11_TEXTURE2D_DESC uploadDesc{};
  uploadDesc.Width = width;
  uploadDesc.Height = height;
  uploadDesc.MipLevels = 1;
  uploadDesc.ArraySize = 1;
  uploadDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  uploadDesc.SampleDesc.Count = 1;
  uploadDesc.Usage = D3D11_USAGE_DEFAULT;
  if (FAILED(view->device->CreateTexture2D(&uploadDesc, nullptr, &view->upload))) {
    return ROBO_FALSE;
  }
  D3D11_QUERY_DESC queryDesc{};
  queryDesc.Query = D3D11_QUERY_EVENT;
  view->device->CreateQuery(&queryDesc, &view->uploadQuery);
  const RoboU64 byteCount = static_cast<RoboU64>(width) * height * 4u;
  auto* front = static_cast<RoboU8*>(_aligned_malloc(static_cast<size_t>(byteCount), 16));
  auto* back = static_cast<RoboU8*>(_aligned_malloc(static_cast<size_t>(byteCount), 16));
  if (front == nullptr || back == nullptr) {
    _aligned_free(front);
    _aligned_free(back);
    return ROBO_FALSE;
  }
  _aligned_free(view->front);
  _aligned_free(view->back);
  _aligned_free(view->sheet);
  view->sheet = nullptr;
  view->sheetWidth = 0;
  view->sheetHeight = 0;
  view->front = front;
  view->back = back;
  view->pixelWidth = width;
  view->pixelHeight = height;
  RoboHandoff handoff{};
  handoff.magic = ROBO_HANDOFF_MAGIC;
  handoff.version = ROBO_HANDOFF_VERSION;
  handoff.frameBuffer.base = reinterpret_cast<RoboU64>(view->front);
  handoff.frameBuffer.width = width;
  handoff.frameBuffer.height = height;
  handoff.frameBuffer.pixelsPerScanLine = width;
  handoff.frameBuffer.bytesPerPixel = 4;
  handoff.frameBuffer.redMask = 0x00FF0000u;
  handoff.frameBuffer.greenMask = 0x0000FF00u;
  handoff.frameBuffer.blueMask = 0x000000FFu;
  RoboGPUInitFromHandoff(&view->gpu, &handoff, view->back, byteCount);
  return ROBO_TRUE;
}

static void RoboWindowsRelaxText(RoboUIComponent* node) {
  if (node == nullptr) {
    return;
  }
  RoboUIText* text = RoboUITextFromComponent(node);
  if (text != nullptr && node->layout.preferWidth > 0 && node->width > node->layout.preferWidth) {
    node->width = 0;
  }
  for (RoboU32 index = 0; index < node->childCount; ++index) {
    RoboWindowsRelaxText(node->children[index]);
  }
}

static void RoboWindowsFitText(RoboUIComponent* node) {
  if (node == nullptr) {
    return;
  }
  RoboUIText* text = RoboUITextFromComponent(node);
  if (text != nullptr && node->width > 0) {
    text->maxWidth = node->width;
    RoboI32 measuredW = 0;
    RoboI32 measuredH = 0;
    RoboUITextMeasure(text, &measuredW, &measuredH);
    if (measuredH > 0) {
      node->height = measuredH;
      node->layout.preferHeight = measuredH;
    }
  }
  for (RoboU32 index = 0; index < node->childCount; ++index) {
    RoboWindowsFitText(node->children[index]);
  }
}

static RoboI32 RoboWindowsContentBottom(RoboUIComponent* node) {
  if (node == nullptr) {
    return 0;
  }
  RoboI32 bottom = 0;
  for (RoboU32 index = 0; index < node->childCount; ++index) {
    RoboUIComponent* child = node->children[index];
    if (child == nullptr) {
      continue;
    }
    const RoboI32 childBottom = child->y + child->height;
    if (childBottom > bottom) {
      bottom = childBottom;
    }
  }
  RoboContainer* box = RoboContainerFromComponent(node);
  if (box != nullptr) {
    bottom += box->paddingT + box->paddingB;
  }
  return bottom;
}

static int RoboWindowsFindButton(RoboUIComponent* node, RoboI32 absX, RoboI32 absY, RoboI32 originX, RoboI32 originY,
                                 RoboButton* buttons, int buttonCount, RoboIconButton* iconButtons, int iconButtonCount,
                                 RoboTextField* fields, int fieldCount, RoboSwitch* switches, int switchCount) {
  if (node == nullptr) {
    return -1;
  }
  RoboI32 x = originX + node->x;
  RoboI32 y = originY + node->y;
  if (node->layout.position == RoboUIPosition::Fixed) {
    x = node->x;
    y = node->y;
  }
  RoboI32 childX = x;
  RoboI32 childY = y;
  RoboContainer* box = RoboContainerFromComponent(node);
  if (box != nullptr) {
    childX += box->paddingL;
    childY += box->paddingT;
  }
  for (RoboU32 index = node->childCount; index > 0; --index) {
    const int hit = RoboWindowsFindButton(node->children[index - 1], absX, absY, childX, childY, buttons, buttonCount,
                                         iconButtons, iconButtonCount, fields, fieldCount, switches, switchCount);
    if (hit >= 0) {
      return hit;
    }
  }
  if (absX >= x && absY >= y && absX < x + node->width && absY < y + node->height) {
    if (node->kind == RoboUINodeKind::Button) {
      for (int index = 0; index < buttonCount; ++index) {
        if (&buttons[index].base == node) {
          return index;
        }
      }
    }
    if (node->kind == RoboUINodeKind::IconButton && iconButtons != nullptr) {
      for (int index = 0; index < iconButtonCount; ++index) {
        if (&iconButtons[index].base == node) {
          return buttonCount + index;
        }
      }
    }
    if (node->kind == RoboUINodeKind::TextField && fields != nullptr) {
      for (int index = 0; index < fieldCount; ++index) {
        if (&fields[index].base == node) {
          return buttonCount + iconButtonCount + index;
        }
      }
    }
    if (node->kind == RoboUINodeKind::Switch && switches != nullptr) {
      for (int index = 0; index < switchCount; ++index) {
        if (&switches[index].base == node) {
          return buttonCount + iconButtonCount + fieldCount + index;
        }
      }
    }
  }
  return -1;
}

static int RoboWindowsControlCount(const RoboWindowsView* view) {
  if (view == nullptr) {
    return 0;
  }
  return view->buttonCount + view->iconButtonCount + view->fieldCount + view->switchCount;
}

static int RoboWindowsSwitchIndex(const RoboWindowsView* view, int index) {
  if (view == nullptr) {
    return -1;
  }
  const int control = index - view->buttonCount - view->iconButtonCount - view->fieldCount;
  if (control < 0 || control >= view->switchCount || view->switches == nullptr) {
    return -1;
  }
  return control;
}

static int RoboWindowsFieldIndex(const RoboWindowsView* view, int index) {
  if (view == nullptr) {
    return -1;
  }
  const int field = index - view->buttonCount - view->iconButtonCount;
  if (field < 0 || field >= view->fieldCount || view->fields == nullptr) {
    return -1;
  }
  return field;
}

static RoboBool RoboWindowsControlDisabled(const RoboWindowsView* view, int index) {
  if (view == nullptr || index < 0) {
    return ROBO_TRUE;
  }
  if (index < view->buttonCount) {
    return view->buttons != nullptr && view->buttons[index].disabled ? ROBO_TRUE : ROBO_FALSE;
  }
  const int field = RoboWindowsFieldIndex(view, index);
  if (field >= 0) {
    return view->fields[field].disabled ? ROBO_TRUE : ROBO_FALSE;
  }
  const int toggle = RoboWindowsSwitchIndex(view, index);
  if (toggle >= 0) {
    return (view->switches[toggle].disabled || view->switches[toggle].loading) ? ROBO_TRUE : ROBO_FALSE;
  }
  const int icon = index - view->buttonCount;
  if (view->iconButtons == nullptr || icon < 0 || icon >= view->iconButtonCount) {
    return ROBO_TRUE;
  }
  return (view->iconButtons[icon].disabled || view->iconButtons[icon].status == 1) ? ROBO_TRUE : ROBO_FALSE;
}

static void RoboWindowsPointButton(RoboWindowsView* view, int index, RoboU8 pointer) {
  if (view == nullptr || index < 0) {
    return;
  }
  if (index < view->buttonCount && view->buttons != nullptr) {
    if (view->buttons[index].disabled) {
      RoboButtonSetPointer(&view->buttons[index], 0);
      return;
    }
    RoboButtonSetPointer(&view->buttons[index], pointer);
    return;
  }
  const int field = RoboWindowsFieldIndex(view, index);
  if (field >= 0) {
    RoboTextFieldSetHover(&view->fields[field], pointer == 0 || view->fields[field].disabled ? ROBO_FALSE : ROBO_TRUE);
    return;
  }
  const int toggle = RoboWindowsSwitchIndex(view, index);
  if (toggle >= 0) {
    RoboSwitchSetPressed(&view->switches[toggle], pointer == 2 ? ROBO_TRUE : ROBO_FALSE,
                         static_cast<RoboU32>(GetTickCount()));
    return;
  }
  const int icon = index - view->buttonCount;
  if (view->iconButtons == nullptr || icon < 0 || icon >= view->iconButtonCount) {
    return;
  }
  if (RoboWindowsControlDisabled(view, index)) {
    RoboIconButtonSetPointer(&view->iconButtons[icon], 0);
    return;
  }
  RoboIconButtonSetPointer(&view->iconButtons[icon], pointer);
}

static RoboUIComponent* RoboWindowsControlNode(const RoboWindowsView* view, int index) {
  if (view == nullptr || index < 0) {
    return nullptr;
  }
  if (index < view->buttonCount && view->buttons != nullptr) {
    return &view->buttons[index].base;
  }
  const int icon = index - view->buttonCount;
  if (view->iconButtons != nullptr && icon >= 0 && icon < view->iconButtonCount) {
    return &view->iconButtons[icon].base;
  }
  const int field = RoboWindowsFieldIndex(view, index);
  if (field >= 0) {
    return &view->fields[field].base;
  }
  const int toggle = RoboWindowsSwitchIndex(view, index);
  if (toggle >= 0) {
    return &view->switches[toggle].base;
  }
  return nullptr;
}

static void RoboWindowsMarkButton(RoboWindowsView* view, int index);

static void RoboWindowsFocusButton(RoboWindowsView* view, int index, RoboBool keyboard) {
  if (view == nullptr) {
    return;
  }
  for (int button = 0; button < view->buttonCount && view->buttons != nullptr; ++button) {
    RoboButtonSetFocused(&view->buttons[button], (keyboard && button == index) ? ROBO_TRUE : ROBO_FALSE);
  }
  for (int icon = 0; icon < view->iconButtonCount && view->iconButtons != nullptr; ++icon) {
    const RoboBool show = (keyboard && view->buttonCount + icon == index) ? ROBO_TRUE : ROBO_FALSE;
    RoboIconButtonSetFocused(&view->iconButtons[icon], show);
  }
  for (int field = 0; field < view->fieldCount && view->fields != nullptr; ++field) {
    if (view->buttonCount + view->iconButtonCount + field == index) {
      view->fields[field].focused = ROBO_TRUE;
      view->fields[field].caretOn = ROBO_TRUE;
    } else {
      RoboTextFieldBlur(&view->fields[field]);
    }
  }
  for (int toggle = 0; toggle < view->switchCount && view->switches != nullptr; ++toggle) {
    const int control = view->buttonCount + view->iconButtonCount + view->fieldCount + toggle;
    RoboSwitchSetFocused(&view->switches[toggle], (keyboard && control == index) ? ROBO_TRUE : ROBO_FALSE);
  }
  const int previous = view->focusIndex;
  view->focusIndex = index;
  RoboWindowsMarkButton(view, previous);
  RoboWindowsMarkButton(view, index);
}

static RoboBool RoboWindowsLocateButton(RoboUIComponent* node, RoboI32 originX, RoboI32 originY, RoboUIComponent* target,
                                        RoboI32* outX, RoboI32* outY);

static void RoboWindowsRevealFocus(RoboWindowsView* view, int index) {
  if (view == nullptr) {
    return;
  }
  RoboUIComponent* node = RoboWindowsControlNode(view, index);
  if (node != nullptr && view->root != nullptr) {
    RoboI32 x = 0;
    RoboI32 y = 0;
    if (RoboWindowsLocateButton(view->root, 0, -view->scrollY, node, &x, &y)) {
      const int margin = 24;
      const int viewHeight = static_cast<int>(view->pixelHeight);
      if (y < margin) {
        view->scrollY -= margin - y;
      } else if (y + node->height > viewHeight - margin) {
        view->scrollY += y + node->height - (viewHeight - margin);
      }
      if (view->scrollY < 0) {
        view->scrollY = 0;
      }
      const int maxScroll = view->contentHeight - viewHeight;
      if (view->scrollY > maxScroll) {
        view->scrollY = maxScroll > 0 ? maxScroll : 0;
      }
      view->scrollTarget = view->scrollY;
    }
  }
}

static int RoboWindowsButtonAt(RoboWindowsView* view, int x, int y) {
  if (view == nullptr || view->root == nullptr) {
    return -1;
  }
  return RoboWindowsFindButton(view->root, x, y, 0, -view->scrollY, view->buttons, view->buttonCount, view->iconButtons,
                               view->iconButtonCount, view->fields, view->fieldCount, view->switches, view->switchCount);
}

static void RoboWindowsHoverButton(RoboWindowsView* view, int index) {
  if (view == nullptr || view->hoverIndex == index) {
    return;
  }
  if (view->hoverIndex >= 0 && view->hoverIndex != view->pressIndex) {
    RoboWindowsPointButton(view, view->hoverIndex, 0);
  }
  view->hoverIndex = index;
  if (index >= 0 && index != view->pressIndex) {
    RoboWindowsPointButton(view, index, 1);
  }
}

static int RoboWindowsStepFocus(RoboWindowsView* view, int direction) {
  const int count = RoboWindowsControlCount(view);
  if (view == nullptr || count <= 0) {
    return -1;
  }
  int index = view->focusIndex;
  for (int step = 0; step < count; ++step) {
    index += direction;
    if (index < 0) {
      index = count - 1;
    } else if (index >= count) {
      index = 0;
    }
    if (!RoboWindowsControlDisabled(view, index)) {
      return index;
    }
  }
  return -1;
}

static void RoboWindowsMarkButton(RoboWindowsView* view, int index) {
  if (view == nullptr || index < 0 || index >= RoboWindowsControlCount(view)) {
    return;
  }
  if (view->dirtyCount >= static_cast<int>(sizeof(view->dirtyButtons) / sizeof(view->dirtyButtons[0]))) {
    view->fullPaint = ROBO_TRUE;
    return;
  }
  for (int dirty = 0; dirty < view->dirtyCount; ++dirty) {
    if (view->dirtyButtons[dirty] == index) {
      return;
    }
  }
  view->dirtyButtons[view->dirtyCount++] = index;
}

static RoboBool RoboWindowsLocateButton(RoboUIComponent* node, RoboI32 originX, RoboI32 originY, RoboUIComponent* target,
                                        RoboI32* outX, RoboI32* outY) {
  if (node == nullptr || target == nullptr) {
    return ROBO_FALSE;
  }
  RoboI32 x = originX + node->x;
  RoboI32 y = originY + node->y;
  if (node->layout.position == RoboUIPosition::Fixed) {
    x = node->x;
    y = node->y;
  }
  if (node == target) {
    if (outX != nullptr) {
      *outX = x;
    }
    if (outY != nullptr) {
      *outY = y;
    }
    return ROBO_TRUE;
  }
  RoboI32 childX = x;
  RoboI32 childY = y;
  RoboContainer* box = RoboContainerFromComponent(node);
  if (box != nullptr) {
    childX += box->paddingL;
    childY += box->paddingT;
  }
  for (RoboU32 index = 0; index < node->childCount; ++index) {
    if (RoboWindowsLocateButton(node->children[index], childX, childY, target, outX, outY)) {
      return ROBO_TRUE;
    }
  }
  return ROBO_FALSE;
}

static void RoboWindowsNote(RoboWindowsView* view, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height) {
  if (view == nullptr || width <= 0 || height <= 0) {
    return;
  }
  if (view->sheetBound) {
    y -= view->scrollY;
  }
  if (x < 0) {
    width += x;
    x = 0;
  }
  if (y < 0) {
    height += y;
    y = 0;
  }
  if (x + width > static_cast<RoboI32>(view->pixelWidth)) {
    width = static_cast<RoboI32>(view->pixelWidth) - x;
  }
  if (y + height > static_cast<RoboI32>(view->pixelHeight)) {
    height = static_cast<RoboI32>(view->pixelHeight) - y;
  }
  if (width <= 0 || height <= 0) {
    return;
  }
  if (view->noteCount >= static_cast<int>(sizeof(view->notes) / sizeof(view->notes[0]))) {
    view->noteFull = ROBO_TRUE;
    return;
  }
  view->notes[view->noteCount].left = x;
  view->notes[view->noteCount].top = y;
  view->notes[view->noteCount].right = x + width;
  view->notes[view->noteCount].bottom = y + height;
  ++view->noteCount;
}

static void RoboWindowsRedrawButton(RoboWindowsView* view, int index) {
  if (view == nullptr || index < 0 || view->root == nullptr) {
    return;
  }
  const int toggleIndex = RoboWindowsSwitchIndex(view, index);
  if (toggleIndex >= 0) {
    RoboSwitch* control = &view->switches[toggleIndex];
    RoboI32 x = 0;
    RoboI32 y = 0;
    const RoboI32 originY = view->sheetBound ? 0 : -view->scrollY;
    if (!RoboWindowsLocateButton(view->root, 0, originY, &control->base, &x, &y)) {
      return;
    }
    const RoboI32 pad = control->item ? 0 : control->thumbPx * 18 / 100 + 4;
    const RoboI32 extra = control->thumbPx * 18 / 100 + 4;
    const RoboU32 page = RoboFoundationPlatformColor("bg-primary");
    RoboGPUFillRect(&view->gpu, x - pad, y - pad, control->base.width + pad * 2, control->base.height + pad * 2, page);
    if (control->item && extra > 0) {
      RoboGPUFillRect(&view->gpu, x + control->base.width, y, extra, control->base.height, page);
    }
    if (control->base.vtable != nullptr && control->base.vtable->Paint != nullptr) {
      control->base.vtable->Paint(&control->base, &view->gpu, x, y);
    }
    RoboWindowsNote(view, x - pad, y - pad, control->base.width + pad * 2 + (control->item ? extra : 0),
                    control->base.height + pad * 2);
    return;
  }
  const int fieldIndex = RoboWindowsFieldIndex(view, index);
  if (fieldIndex >= 0) {
    RoboTextField* field = &view->fields[fieldIndex];
    RoboI32 x = 0;
    RoboI32 y = 0;
    const RoboI32 originY = view->sheetBound ? 0 : -view->scrollY;
    if (!RoboWindowsLocateButton(view->root, 0, originY, &field->base, &x, &y)) {
      return;
    }
    const RoboI32 pad = 4;
    RoboGPUFillRect(&view->gpu, x - pad, y - pad, field->base.width + pad * 2, field->base.height + pad * 2,
                    RoboFoundationPlatformColor("bg-primary"));
    if (field->base.vtable != nullptr && field->base.vtable->Paint != nullptr) {
      field->base.vtable->Paint(&field->base, &view->gpu, x, y);
    }
    RoboWindowsNote(view, x - pad, y - pad, field->base.width + pad * 2, field->base.height + pad * 2);
    return;
  }
  if (index >= view->buttonCount) {
    const int icon = index - view->buttonCount;
    if (view->iconButtons == nullptr || icon < 0 || icon >= view->iconButtonCount) {
      return;
    }
    RoboIconButton* button = &view->iconButtons[icon];
    RoboI32 x = 0;
    RoboI32 y = 0;
    const RoboI32 originY = view->sheetBound ? 0 : -view->scrollY;
    if (!RoboWindowsLocateButton(view->root, 0, originY, &button->base, &x, &y)) {
      return;
    }
    const RoboI32 pad = 8;
    RoboGPUFillRect(&view->gpu, x - pad, y - pad, button->base.width + pad * 2, button->base.height + pad * 2,
                    RoboFoundationPlatformColor("bg-primary"));
    if (button->base.vtable != nullptr && button->base.vtable->Paint != nullptr) {
      button->base.vtable->Paint(&button->base, &view->gpu, x, y);
    }
    RoboWindowsNote(view, x - pad, y - pad, button->base.width + pad * 2, button->base.height + pad * 2);
    return;
  }
  if (view->buttons == nullptr || index >= view->buttonCount) {
    return;
  }
  RoboButton* button = &view->buttons[index];
  RoboI32 x = 0;
  RoboI32 y = 0;
  const RoboI32 originY = view->sheetBound ? 0 : -view->scrollY;
  if (!RoboWindowsLocateButton(view->root, 0, originY, &button->base, &x, &y)) {
    return;
  }
  const RoboI32 pad = 10;
  if (view->spinFrame && button->loading && !view->sheetBound) {
    RoboButtonPaintSpin(button, &view->gpu, x, y);
    return;
  }
  RoboGPUFillRect(&view->gpu, x - pad, y - pad, button->base.width + pad * 2, button->base.height + pad * 2,
                  RoboFoundationPlatformColor("bg-primary"));
  if (button->base.vtable != nullptr && button->base.vtable->Paint != nullptr) {
    button->base.vtable->Paint(&button->base, &view->gpu, x, y);
  }
  RoboWindowsNote(view, x - pad, y - pad, button->base.width + pad * 2, button->base.height + pad * 2);
}

static RoboBool RoboWindowsClampRect(const RoboWindowsView* view, RECT* rect) {
  if (rect == nullptr || view == nullptr) {
    return ROBO_FALSE;
  }
  if (rect->left < 0) {
    rect->left = 0;
  }
  if (rect->top < 0) {
    rect->top = 0;
  }
  if (rect->right > static_cast<LONG>(view->pixelWidth)) {
    rect->right = static_cast<LONG>(view->pixelWidth);
  }
  if (rect->bottom > static_cast<LONG>(view->pixelHeight)) {
    rect->bottom = static_cast<LONG>(view->pixelHeight);
  }
  return rect->right > rect->left && rect->bottom > rect->top ? ROBO_TRUE : ROBO_FALSE;
}

static void RoboWindowsUploadBox(RoboWindowsView* view, ID3D11Texture2D* texture, LONG left, LONG top, LONG right, LONG bottom) {
  if (texture == nullptr || view->back == nullptr) {
    return;
  }
  if (left < 0) {
    left = 0;
  }
  if (top < 0) {
    top = 0;
  }
  if (right > static_cast<LONG>(view->pixelWidth)) {
    right = static_cast<LONG>(view->pixelWidth);
  }
  if (bottom > static_cast<LONG>(view->pixelHeight)) {
    bottom = static_cast<LONG>(view->pixelHeight);
  }
  if (right <= left || bottom <= top) {
    return;
  }
  D3D11_BOX box{};
  box.left = static_cast<UINT>(left);
  box.top = static_cast<UINT>(top);
  box.right = static_cast<UINT>(right);
  box.bottom = static_cast<UINT>(bottom);
  box.back = 1;
  const UINT pitch = view->pixelWidth * 4u;
  const auto* source = view->back + static_cast<RoboSize>(box.top) * pitch + static_cast<RoboSize>(box.left) * 4u;
  view->context->UpdateSubresource(texture, 0, &box, source, pitch, 0);
}

static void RoboWindowsCopyBox(RoboWindowsView* view, ID3D11Texture2D* target, LONG left, LONG top, LONG right, LONG bottom) {
  RECT rect{left, top, right, bottom};
  if (target == nullptr || view->upload == nullptr || !RoboWindowsClampRect(view, &rect)) {
    return;
  }
  D3D11_BOX box{};
  box.left = static_cast<UINT>(rect.left);
  box.top = static_cast<UINT>(rect.top);
  box.right = static_cast<UINT>(rect.right);
  box.bottom = static_cast<UINT>(rect.bottom);
  box.back = 1;
  view->context->CopySubresourceRegion(target, 0, box.left, box.top, 0, view->upload, 0, &box);
}

static void RoboWindowsScrollUpload(RoboWindowsView* view, ID3D11Texture2D* target, int shift) {
  const UINT width = view->pixelWidth;
  const UINT height = view->pixelHeight;
  if (target == nullptr || view->upload == nullptr || shift == 0 || height == 0) {
    return;
  }
  const int distance = shift < 0 ? -shift : shift;
  if (distance <= 0 || static_cast<UINT>(distance) >= height) {
    return;
  }
  view->context->CopyResource(target, view->upload);
  D3D11_BOX box{};
  box.left = 0;
  box.right = width;
  box.front = 0;
  box.back = 1;
  if (shift > 0) {
    box.top = static_cast<UINT>(shift);
    box.bottom = height;
    view->context->CopySubresourceRegion(view->upload, 0, 0, 0, 0, target, 0, &box);
  } else {
    box.top = 0;
    box.bottom = height - static_cast<UINT>(distance);
    view->context->CopySubresourceRegion(view->upload, 0, 0, static_cast<UINT>(distance), 0, target, 0, &box);
  }
}

static void RoboWindowsPublishFlip(RoboWindowsView* view) {
  if (view->uploadBusy && view->uploadQuery != nullptr) {
    if (view->context->GetData(view->uploadQuery, nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
      return;
    }
    view->uploadBusy = ROBO_FALSE;
  }
  const RoboBool full = view->gpu.damage.full;
  RECT dirty[24];
  UINT dirtyCount = 0;
  if (full) {
    dirty[0] = RECT{0, 0, static_cast<LONG>(view->pixelWidth), static_cast<LONG>(view->pixelHeight)};
    dirtyCount = 1;
  } else {
    for (RoboU32 index = 0; index < view->gpu.damage.count && dirtyCount < 24; ++index) {
      const RoboDamageRect& rect = view->gpu.damage.rects[index];
      dirty[dirtyCount].left = rect.x;
      dirty[dirtyCount].top = rect.y;
      dirty[dirtyCount].right = rect.x + rect.width;
      dirty[dirtyCount].bottom = rect.y + rect.height;
      ++dirtyCount;
    }
    if (view->gpu.damage.count > 24) {
      dirty[0] = RECT{0, 0, static_cast<LONG>(view->pixelWidth), static_cast<LONG>(view->pixelHeight)};
      dirtyCount = 1;
    }
  }
  ID3D11Texture2D* target = nullptr;
  if (FAILED(view->swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&target))) ||
      target == nullptr) {
    return;
  }
  const UINT pitch = view->pixelWidth * 4u;
  const int gpuShift = view->gpuScroll < 0 ? 0 : view->scrollY - view->gpuScroll;
  const RoboBool uploadAll = full || view->gpuFull || view->gpuScroll < 0 ||
                             (gpuShift != 0 && (gpuShift >= static_cast<int>(view->pixelHeight) ||
                                                -gpuShift >= static_cast<int>(view->pixelHeight)));
  if (uploadAll) {
    if (view->upload != nullptr) {
      view->context->UpdateSubresource(view->upload, 0, nullptr, view->back, pitch, 0);
      view->context->CopyResource(target, view->upload);
    } else {
      view->context->UpdateSubresource(target, 0, nullptr, view->back, pitch, 0);
    }
    view->gpuFull = ROBO_FALSE;
    view->gpuScroll = view->scrollY;
    view->bufferReady[0] = ROBO_FALSE;
    view->bufferReady[1] = ROBO_FALSE;
    view->bufferReady[view->presentIndex & 1] = ROBO_TRUE;
    view->recentCount[0] = 0;
    view->recentCount[1] = 0;
    target->Release();
    if (view->uploadQuery != nullptr) {
      view->context->End(view->uploadQuery);
      view->uploadBusy = ROBO_TRUE;
    }
    view->swap->Present(1, 0);
    view->presentIndex ^= 1;
    RoboDamageClear(&view->gpu.damage);
    view->shownScroll = view->scrollY;
    view->presentDelta = 0;
    return;
  }
  if (gpuShift != 0 && view->upload != nullptr) {
    RoboWindowsScrollUpload(view, target, gpuShift);
    for (UINT index = 0; index < dirtyCount; ++index) {
      RoboWindowsUploadBox(view, view->upload, dirty[index].left, dirty[index].top, dirty[index].right,
                           dirty[index].bottom);
    }
    view->context->CopyResource(target, view->upload);
    view->gpuScroll = view->scrollY;
    view->bufferReady[0] = ROBO_FALSE;
    view->bufferReady[1] = ROBO_FALSE;
    view->bufferReady[view->presentIndex & 1] = ROBO_TRUE;
    view->recentCount[0] = 0;
    view->recentCount[1] = 0;
    target->Release();
    if (view->uploadQuery != nullptr) {
      view->context->End(view->uploadQuery);
      view->uploadBusy = ROBO_TRUE;
    }
    view->swap->Present(1, 0);
    view->presentIndex ^= 1;
    RoboDamageClear(&view->gpu.damage);
    view->shownScroll = view->scrollY;
    view->presentDelta = 0;
    return;
  }
  RoboBool heavy = ROBO_FALSE;
  if (view->upload != nullptr && dirtyCount > 16) {
    for (UINT index = 0; index < dirtyCount; ++index) {
      RoboWindowsUploadBox(view, view->upload, dirty[index].left, dirty[index].top, dirty[index].right, dirty[index].bottom);
    }
    view->context->CopyResource(target, view->upload);
    view->bufferReady[0] = ROBO_FALSE;
    view->bufferReady[1] = ROBO_FALSE;
    view->bufferReady[view->presentIndex & 1] = ROBO_TRUE;
    view->recentCount[0] = 0;
    view->recentCount[1] = 0;
    heavy = ROBO_TRUE;
  } else if (view->upload != nullptr) {
    const int slot = view->presentIndex & 1;
    const int older = slot ^ 1;
    view->recentCount[slot] = 0;
    for (UINT index = 0; index < dirtyCount && view->recentCount[slot] < 16; ++index) {
      view->recent[slot][view->recentCount[slot]++] = dirty[index];
    }
    if (!view->bufferReady[slot]) {
      view->context->CopyResource(target, view->upload);
      view->bufferReady[slot] = ROBO_TRUE;
      heavy = ROBO_TRUE;
    }
    for (int index = 0; index < view->recentCount[slot]; ++index) {
      const RECT& rect = view->recent[slot][index];
      RoboWindowsUploadBox(view, view->upload, rect.left, rect.top, rect.right, rect.bottom);
    }
    for (int generation = 0; generation < 2; ++generation) {
      const int which = generation == 0 ? older : slot;
      for (int index = 0; index < view->recentCount[which]; ++index) {
        const RECT& rect = view->recent[which][index];
        RoboWindowsCopyBox(view, target, rect.left, rect.top, rect.right, rect.bottom);
      }
    }
  } else {
    view->context->UpdateSubresource(target, 0, nullptr, view->back, pitch, 0);
    heavy = ROBO_TRUE;
  }
  target->Release();
  if (heavy && view->uploadQuery != nullptr) {
    view->context->End(view->uploadQuery);
    view->uploadBusy = ROBO_TRUE;
  }
  RECT presentRects[16];
  UINT presentCount = 0;
  if (dirtyCount > 0 && dirtyCount <= 16) {
    for (UINT index = 0; index < dirtyCount; ++index) {
      RECT rect = dirty[index];
      if (RoboWindowsClampRect(view, &rect)) {
        presentRects[presentCount++] = rect;
      }
    }
  }
  HRESULT presented = E_FAIL;
  if (presentCount > 0) {
    DXGI_PRESENT_PARAMETERS params{};
    params.DirtyRectsCount = presentCount;
    params.pDirtyRects = presentRects;
    presented = view->swap->Present1(0, DXGI_PRESENT_DO_NOT_WAIT, &params);
    if (presented == DXGI_ERROR_INVALID_CALL) {
      presented = view->swap->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
    }
  } else {
    presented = view->swap->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
  }
  if (presented == DXGI_ERROR_WAS_STILL_DRAWING) {
    return;
  }
  if (FAILED(presented)) {
    view->swap->Present(0, 0);
  }
  view->presentIndex ^= 1;
  RoboDamageClear(&view->gpu.damage);
  view->shownScroll = view->scrollY;
  view->presentDelta = 0;
}

static void RoboWindowsPublish(RoboWindowsView* view) {
  if (view == nullptr || view->context == nullptr || view->swap == nullptr || view->back == nullptr) {
    return;
  }
  if (RoboDamageIsEmpty(&view->gpu.damage)) {
    return;
  }
  if (view->flip) {
    RoboWindowsPublishFlip(view);
    return;
  }
  if (view->upload == nullptr) {
    return;
  }
  const RoboBool full = view->gpu.damage.full;
  RoboDamageRect merged{};
  if (!full && !RoboDamageMerge(&view->gpu.damage, &merged)) {
    return;
  }
  RoboGPUPresent(&view->gpu);
  const UINT pitch = RoboGPUPitch(&view->gpu);
  if (!full) {
    if (merged.x < 0) {
      merged.width += merged.x;
      merged.x = 0;
    }
    if (merged.y < 0) {
      merged.height += merged.y;
      merged.y = 0;
    }
    const RoboI32 limitW = static_cast<RoboI32>(view->pixelWidth);
    const RoboI32 limitH = static_cast<RoboI32>(view->pixelHeight);
    if (merged.x + merged.width > limitW) {
      merged.width = limitW - merged.x;
    }
    if (merged.y + merged.height > limitH) {
      merged.height = limitH - merged.y;
    }
    if (merged.width <= 0 || merged.height <= 0) {
      return;
    }
    D3D11_BOX box{};
    box.left = static_cast<UINT>(merged.x);
    box.top = static_cast<UINT>(merged.y);
    box.right = static_cast<UINT>(merged.x + merged.width);
    box.bottom = static_cast<UINT>(merged.y + merged.height);
    box.back = 1;
    const auto* source = view->gpu.front + static_cast<RoboSize>(merged.y) * pitch + static_cast<RoboSize>(merged.x) * 4u;
    view->context->UpdateSubresource(view->upload, 0, &box, source, pitch, 0);
  } else {
    view->context->UpdateSubresource(view->upload, 0, nullptr, view->gpu.front, pitch, 0);
  }
  ID3D11Texture2D* target = nullptr;
  if (FAILED(view->swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&target))) ||
      target == nullptr) {
    return;
  }
  view->context->CopyResource(target, view->upload);
  target->Release();
  view->swap->Present(1, 0);
}

static int RoboWindowsMaxScroll(const RoboWindowsView* view) {
  if (view == nullptr) {
    return 0;
  }
  const int maxScroll = view->contentHeight - static_cast<int>(view->pixelHeight);
  return maxScroll > 0 ? maxScroll : 0;
}

static void RoboWindowsBarBox(const RoboWindowsView* view, RoboI32* x, RoboI32* y, RoboI32* width, RoboI32* height,
                              RoboI32* trackY, RoboI32* trackH, RoboI32* thumbY, RoboI32* thumbH) {
  const RoboI32 thick = RoboWindowsScale(6, view->dpi);
  const RoboI32 edge = RoboWindowsScale(2, view->dpi);
  const RoboI32 arrow = RoboWindowsScale(6, view->dpi);
  const RoboI32 gap = RoboWindowsScale(3, view->dpi);
  const RoboI32 minThumb = RoboWindowsScale(28, view->dpi);
  *width = thick;
  *x = static_cast<RoboI32>(view->pixelWidth) - edge - thick;
  *y = edge;
  *height = static_cast<RoboI32>(view->pixelHeight) - edge * 2;
  *trackY = *y + arrow + gap;
  *trackH = *height - (arrow + gap) * 2;
  if (*trackH < minThumb) {
    *trackH = *height > minThumb ? minThumb : *height;
    *trackY = *y + (*height - *trackH) / 2;
  }
  const int maxScroll = RoboWindowsMaxScroll(view);
  const int content = view->contentHeight > 0 ? view->contentHeight : 1;
  RoboI32 thumb = static_cast<RoboI32>(static_cast<long long>(*trackH) * view->pixelHeight / content);
  if (thumb < minThumb) {
    thumb = minThumb;
  }
  if (thumb > *trackH) {
    thumb = *trackH;
  }
  const RoboI32 travel = *trackH - thumb > 1 ? *trackH - thumb : 1;
  *thumbH = thumb;
  *thumbY = *trackY;
  if (maxScroll > 0) {
    *thumbY += static_cast<RoboI32>(static_cast<long long>(view->scrollY) * travel / maxScroll);
  }
}

static RoboU8 RoboWindowsPillCoverage(RoboI32 localX, RoboI32 localY, RoboI32 width, RoboI32 height) {
  const RoboI32 radius = (width < height ? width : height) * 2;
  const RoboI32 left = radius;
  const RoboI32 right = width * 4 - radius;
  const RoboI32 top = radius;
  const RoboI32 bottom = height * 4 - radius;
  const RoboI32 limit = radius * radius;
  RoboU32 hits = 0;
  for (RoboI32 sampleY = 0; sampleY < 4; ++sampleY) {
    for (RoboI32 sampleX = 0; sampleX < 4; ++sampleX) {
      RoboI32 px = localX * 4 + sampleX;
      RoboI32 py = localY * 4 + sampleY;
      RoboI32 cx = px < left ? left : (px > right ? right : px);
      RoboI32 cy = py < top ? top : (py > bottom ? bottom : py);
      const RoboI32 dx = px - cx;
      const RoboI32 dy = py - cy;
      if (dx * dx + dy * dy <= limit) {
        ++hits;
      }
    }
  }
  return static_cast<RoboU8>(hits * 255u / 16u);
}

static RoboI32 RoboWindowsCross(RoboI32 x0, RoboI32 y0, RoboI32 x1, RoboI32 y1, RoboI32 x, RoboI32 y) {
  return (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
}

static RoboU8 RoboWindowsArrowCoverage(RoboI32 localX, RoboI32 localY, RoboI32 size, RoboBool up) {
  const RoboI32 span = size * 4;
  const RoboI32 tipX = span / 2;
  const RoboI32 base = span - 2;
  const RoboI32 tipY = up ? 2 : base;
  const RoboI32 baseY = up ? base : 2;
  RoboU32 hits = 0;
  for (RoboI32 sampleY = 0; sampleY < 4; ++sampleY) {
    for (RoboI32 sampleX = 0; sampleX < 4; ++sampleX) {
      const RoboI32 px = localX * 4 + sampleX;
      const RoboI32 py = localY * 4 + sampleY;
      const RoboI32 a = RoboWindowsCross(tipX, tipY, 2, baseY, px, py);
      const RoboI32 b = RoboWindowsCross(2, baseY, span - 2, baseY, px, py);
      const RoboI32 c = RoboWindowsCross(span - 2, baseY, tipX, tipY, px, py);
      if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) {
        ++hits;
      }
    }
  }
  return static_cast<RoboU8>(hits * 255u / 16u);
}

static void RoboWindowsMix(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboU32 color, RoboU8 coverage) {
  if (gpu == nullptr || gpu->back == nullptr || coverage == 0 || x < 0 || y < 0 ||
      x >= static_cast<RoboI32>(gpu->info.width) || y >= static_cast<RoboI32>(gpu->info.height)) {
    return;
  }
  const RoboU32 pitch = RoboGPUPitch(gpu);
  auto* row = reinterpret_cast<RoboU32*>(RoboFrameBufferRow(gpu->back, pitch, y));
  const RoboU32 pixel = row[x];
  const RoboU8 dr = static_cast<RoboU8>((pixel >> gpu->redShift) & 0xFFu);
  const RoboU8 dg = static_cast<RoboU8>((pixel >> gpu->greenShift) & 0xFFu);
  const RoboU8 db = static_cast<RoboU8>((pixel >> gpu->blueShift) & 0xFFu);
  const RoboU32 keep = 255u - coverage;
  const RoboU8 r = static_cast<RoboU8>((((color >> 16) & 0xFFu) * coverage + dr * keep) / 255u);
  const RoboU8 g = static_cast<RoboU8>((((color >> 8) & 0xFFu) * coverage + dg * keep) / 255u);
  const RoboU8 b = static_cast<RoboU8>(((color & 0xFFu) * coverage + db * keep) / 255u);
  row[x] = RoboGPUPackColor(gpu, r, g, b, 255);
}

static void RoboWindowsPill(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height, RoboU32 color,
                            RoboU8 coverage) {
  if (gpu == nullptr || width <= 0 || height <= 0 || coverage == 0) {
    return;
  }
  for (RoboI32 row = 0; row < height; ++row) {
    for (RoboI32 col = 0; col < width; ++col) {
      const RoboU8 edge = RoboWindowsPillCoverage(col, row, width, height);
      if (edge == 0) {
        continue;
      }
      RoboWindowsMix(gpu, x + col, y + row, color, static_cast<RoboU8>(edge * coverage / 255));
    }
  }
  RoboDamageAdd(&gpu->damage, x, y, width, height);
}

static void RoboWindowsArrow(RoboGPU* gpu, RoboI32 x, RoboI32 y, RoboI32 size, RoboBool up, RoboU32 color,
                             RoboU8 coverage) {
  if (gpu == nullptr || size <= 1 || coverage == 0) {
    return;
  }
  for (RoboI32 row = 0; row < size; ++row) {
    for (RoboI32 col = 0; col < size; ++col) {
      const RoboU8 edge = RoboWindowsArrowCoverage(col, row, size, up);
      if (edge == 0) {
        continue;
      }
      RoboWindowsMix(gpu, x + col, y + row, color, static_cast<RoboU8>(edge * coverage / 255));
    }
  }
  RoboDamageAdd(&gpu->damage, x, y, size, size);
}

static void RoboWindowsWakeBar(RoboWindowsView* view) {
  if (view == nullptr || RoboWindowsMaxScroll(view) <= 0) {
    return;
  }
  view->barOpacity = 255;
  view->barIdle = GetTickCount();
}

static int RoboWindowsBarHit(const RoboWindowsView* view, int x, int y) {
  if (view == nullptr || RoboWindowsMaxScroll(view) <= 0) {
    return 0;
  }
  RoboI32 barX = 0;
  RoboI32 barY = 0;
  RoboI32 barW = 0;
  RoboI32 barH = 0;
  RoboI32 trackY = 0;
  RoboI32 trackH = 0;
  RoboI32 thumbY = 0;
  RoboI32 thumbH = 0;
  RoboWindowsBarBox(view, &barX, &barY, &barW, &barH, &trackY, &trackH, &thumbY, &thumbH);
  const RoboI32 edge = RoboWindowsScale(2, view->dpi);
  if (x < barX - edge || x >= barX + barW + edge || y < barY || y >= barY + barH) {
    return 0;
  }
  const RoboI32 arrow = RoboWindowsScale(6, view->dpi);
  if (y < barY + arrow + edge) {
    return 1;
  }
  if (y >= barY + barH - arrow - edge) {
    return 2;
  }
  if (y >= thumbY && y < thumbY + thumbH) {
    return 3;
  }
  return 4;
}

static void RoboWindowsScrollBy(RoboWindowsView* view, int delta) {
  view->scrollY += delta;
  if (view->scrollY < 0) {
    view->scrollY = 0;
  }
  const int maxScroll = RoboWindowsMaxScroll(view);
  if (view->scrollY > maxScroll) {
    view->scrollY = maxScroll;
  }
  view->scrollTarget = view->scrollY;
}

static void RoboWindowsScrollToBar(RoboWindowsView* view, int pointerY) {
  RoboI32 barX = 0;
  RoboI32 barY = 0;
  RoboI32 barW = 0;
  RoboI32 barH = 0;
  RoboI32 trackY = 0;
  RoboI32 trackH = 0;
  RoboI32 thumbY = 0;
  RoboI32 thumbH = 0;
  RoboWindowsBarBox(view, &barX, &barY, &barW, &barH, &trackY, &trackH, &thumbY, &thumbH);
  const int maxScroll = RoboWindowsMaxScroll(view);
  if (trackH <= 0 || maxScroll <= 0) {
    return;
  }
  int ratio = (pointerY - trackY) * 1000 / trackH;
  if (ratio < 0) {
    ratio = 0;
  }
  if (ratio > 1000) {
    ratio = 1000;
  }
  view->scrollY = maxScroll * ratio / 1000;
  view->scrollTarget = view->scrollY;
}

static void RoboWindowsDragBar(RoboWindowsView* view, int pointerY) {
  RoboI32 barX = 0;
  RoboI32 barY = 0;
  RoboI32 barW = 0;
  RoboI32 barH = 0;
  RoboI32 trackY = 0;
  RoboI32 trackH = 0;
  RoboI32 thumbY = 0;
  RoboI32 thumbH = 0;
  RoboWindowsBarBox(view, &barX, &barY, &barW, &barH, &trackY, &trackH, &thumbY, &thumbH);
  const int maxScroll = RoboWindowsMaxScroll(view);
  const RoboI32 travel = trackH - thumbH > 1 ? trackH - thumbH : 1;
  int pos = pointerY - trackY - view->barGrab;
  if (pos < 0) {
    pos = 0;
  }
  if (pos > travel) {
    pos = travel;
  }
  view->scrollY = maxScroll > 0 ? static_cast<int>(static_cast<long long>(pos) * maxScroll / travel) : 0;
  view->scrollTarget = view->scrollY;
}

static RoboU32 RoboWindowsThemeMix(RoboU32 base, RoboU32 toward, int basePercent) {
  const int towardPercent = 100 - basePercent;
  RoboU32 mixed = 0xFF000000u;
  for (int shift = 16; shift >= 0; shift -= 8) {
    const int channel = (((base >> shift) & 0xFF) * basePercent + ((toward >> shift) & 0xFF) * towardPercent) / 100;
    mixed |= static_cast<RoboU32>(channel) << shift;
  }
  return mixed;
}

static void RoboWindowsPaintBar(RoboWindowsView* view) {
  if (view == nullptr || view->barOpacity <= 0 || RoboWindowsMaxScroll(view) <= 0) {
    return;
  }
  RoboI32 barX = 0;
  RoboI32 barY = 0;
  RoboI32 barW = 0;
  RoboI32 barH = 0;
  RoboI32 trackY = 0;
  RoboI32 trackH = 0;
  RoboI32 thumbY = 0;
  RoboI32 thumbH = 0;
  RoboWindowsBarBox(view, &barX, &barY, &barW, &barH, &trackY, &trackH, &thumbY, &thumbH);
  const RoboU32 trackColor = RoboFoundationPlatformColor("border-default");
  const RoboU32 strong = RoboFoundationPlatformColor("border-strong");
  const RoboU32 tertiary = RoboFoundationPlatformColor("text-tertiary");
  const RoboU32 restColor = RoboWindowsThemeMix(strong, tertiary, 45);
  const RoboU32 hoverColor = tertiary;
  const RoboU32 pressColor = RoboWindowsThemeMix(tertiary, RoboFoundationPlatformColor("text-primary"), 58);
  const RoboU8 fade = static_cast<RoboU8>(view->barOpacity);
  const RoboI32 arrow = RoboWindowsScale(6, view->dpi);
  const RoboU32 thumbInk = view->barDrag ? pressColor : (view->barPart == 3 ? hoverColor : restColor);
  const RoboU32 upInk = view->barHold < 0 ? pressColor : (view->barPart == 1 ? hoverColor : restColor);
  const RoboU32 downInk = view->barHold > 0 ? pressColor : (view->barPart == 2 ? hoverColor : restColor);
  RoboWindowsPill(&view->gpu, barX, trackY, barW, trackH, trackColor, fade);
  RoboWindowsPill(&view->gpu, barX, thumbY, barW, thumbH, thumbInk, fade);
  RoboWindowsArrow(&view->gpu, barX, barY, arrow, ROBO_TRUE, upInk, fade);
  RoboWindowsArrow(&view->gpu, barX, barY + barH - arrow, arrow, ROBO_FALSE, downInk, fade);
}

static void RoboWindowsBindTarget(RoboWindowsView* view, RoboBool sheet) {
  if (sheet && view->sheet != nullptr) {
    view->gpu.back = view->sheet;
    view->gpu.info.height = view->sheetHeight;
    view->gpu.backSize = static_cast<RoboU64>(view->sheetWidth) * view->sheetHeight * 4u;
    view->sheetBound = ROBO_TRUE;
  } else {
    view->gpu.back = view->back;
    view->gpu.info.height = view->pixelHeight;
    view->gpu.backSize = static_cast<RoboU64>(view->pixelWidth) * view->pixelHeight * 4u;
    view->sheetBound = ROBO_FALSE;
  }
  RoboDamageClear(&view->gpu.damage);
}

static RoboBool RoboWindowsEnsureSheet(RoboWindowsView* view) {
  const RoboU32 height = view->contentHeight > static_cast<int>(view->pixelHeight)
                             ? static_cast<RoboU32>(view->contentHeight)
                             : view->pixelHeight;
  if (height == 0 || view->pixelWidth == 0) {
    return ROBO_FALSE;
  }
  if (view->sheet != nullptr && view->sheetWidth == view->pixelWidth && view->sheetHeight == height) {
    return ROBO_TRUE;
  }
  const RoboU64 bytes = static_cast<RoboU64>(view->pixelWidth) * height * 4u;
  auto* memory = static_cast<RoboU8*>(_aligned_malloc(static_cast<size_t>(bytes), 16));
  if (memory == nullptr) {
    return ROBO_FALSE;
  }
  _aligned_free(view->sheet);
  view->sheet = memory;
  view->sheetWidth = view->pixelWidth;
  view->sheetHeight = height;
  return ROBO_TRUE;
}

static void RoboWindowsCopyVisible(RoboWindowsView* view, RoboI32 x, RoboI32 y, RoboI32 width, RoboI32 height) {
  if (view->sheet == nullptr || view->back == nullptr || width <= 0 || height <= 0) {
    return;
  }
  if (x < 0) {
    width += x;
    x = 0;
  }
  if (y < 0) {
    height += y;
    y = 0;
  }
  if (x + width > static_cast<RoboI32>(view->pixelWidth)) {
    width = static_cast<RoboI32>(view->pixelWidth) - x;
  }
  if (y + height > static_cast<RoboI32>(view->pixelHeight)) {
    height = static_cast<RoboI32>(view->pixelHeight) - y;
  }
  if (width <= 0 || height <= 0) {
    return;
  }
  const UINT pitch = view->pixelWidth * 4u;
  const int sheetRow = view->scrollY + y;
  if (sheetRow < 0 || sheetRow + height > static_cast<int>(view->sheetHeight)) {
    return;
  }
  for (RoboI32 row = 0; row < height; ++row) {
    std::memcpy(view->back + static_cast<RoboSize>(y + row) * pitch + static_cast<RoboSize>(x) * 4u,
                view->sheet + static_cast<RoboSize>(sheetRow + row) * pitch + static_cast<RoboSize>(x) * 4u,
                static_cast<size_t>(width) * 4u);
  }
  RoboDamageAdd(&view->gpu.damage, x, y, width, height);
}

static void RoboWindowsCompose(RoboWindowsView* view) {
  const UINT pitch = view->pixelWidth * 4u;
  const int height = static_cast<int>(view->pixelHeight);
  const int width = static_cast<int>(view->pixelWidth);
  int shift = view->shownScroll < 0 ? height : view->scrollY - view->shownScroll;
  const int distance = shift < 0 ? -shift : shift;
  const RoboBool scrolled = view->shownScroll < 0 || view->scrollY != view->shownScroll;
  const RoboBool full = view->shownScroll < 0 || view->noteFull || distance >= height ||
                        (view->scrollY != view->shownScroll && view->gpuScroll != view->shownScroll);
  if (full) {
    const int limit = static_cast<int>(view->sheetHeight) - height;
    int y = view->scrollY;
    if (y < 0) {
      y = 0;
    }
    if (y > limit) {
      y = limit > 0 ? limit : 0;
    }
    std::memcpy(view->back, view->sheet + static_cast<RoboSize>(y) * pitch, static_cast<size_t>(pitch) * view->pixelHeight);
    view->shownScroll = view->scrollY;
    view->gpuFull = ROBO_TRUE;
    RoboGPUMarkFullDamage(&view->gpu);
  } else if (view->scrollY != view->shownScroll) {
    if (shift > 0) {
      std::memmove(view->back, view->back + static_cast<RoboSize>(shift) * pitch,
                   static_cast<size_t>(height - shift) * pitch);
      RoboWindowsCopyVisible(view, 0, height - shift, width, shift);
    } else {
      const int reveal = -shift;
      std::memmove(view->back + static_cast<RoboSize>(reveal) * pitch, view->back,
                   static_cast<size_t>(height - reveal) * pitch);
      RoboWindowsCopyVisible(view, 0, 0, width, reveal);
    }
    view->shownScroll = view->scrollY;
  }
  for (int index = 0; index < view->noteCount; ++index) {
    const RECT& note = view->notes[index];
    RoboWindowsCopyVisible(view, note.left, note.top, note.right - note.left, note.bottom - note.top);
  }
  view->noteCount = 0;
  view->noteFull = ROBO_FALSE;
  if (!full && view->barOpacity == 0 && view->barShown > 0) {
    RoboI32 barX = 0;
    RoboI32 barY = 0;
    RoboI32 barW = 0;
    RoboI32 barH = 0;
    RoboI32 trackY = 0;
    RoboI32 trackH = 0;
    RoboI32 thumbY = 0;
    RoboI32 thumbH = 0;
    RoboWindowsBarBox(view, &barX, &barY, &barW, &barH, &trackY, &trackH, &thumbY, &thumbH);
    const RoboI32 edge = RoboWindowsScale(2, view->dpi);
    RoboWindowsCopyVisible(view, barX - edge, 0, barW + edge * 2, height);
  }
  const RoboBool barLook = view->barPart != view->barShownPart || view->barHold != view->barShownHold ||
                           view->barDrag != view->barShownDrag;
  if (view->barOpacity > 0 && (view->barOpacity != view->barShown || barLook || scrolled || full)) {
    if (!full) {
      RoboI32 barX = 0;
      RoboI32 barY = 0;
      RoboI32 barW = 0;
      RoboI32 barH = 0;
      RoboI32 trackY = 0;
      RoboI32 trackH = 0;
      RoboI32 thumbY = 0;
      RoboI32 thumbH = 0;
      RoboWindowsBarBox(view, &barX, &barY, &barW, &barH, &trackY, &trackH, &thumbY, &thumbH);
      const RoboI32 edge = RoboWindowsScale(2, view->dpi);
      RoboWindowsCopyVisible(view, barX - edge, 0, barW + edge * 2, height);
    }
    RoboWindowsPaintBar(view);
    view->barShownPart = view->barPart;
    view->barShownHold = view->barHold;
    view->barShownDrag = view->barDrag;
  }
  view->barShown = view->barOpacity;
}

static RoboBool RoboWindowsGlideScroll(RoboWindowsView* view) {
  const int delta = view->scrollTarget - view->scrollY;
  if (delta == 0) {
    return ROBO_FALSE;
  }
  int step = delta * 2 / 3;
  if (step == 0) {
    step = delta > 0 ? 1 : -1;
  }
  view->scrollY += step;
  return ROBO_TRUE;
}

static void RoboWindowsOverlaySpins(RoboWindowsView* view) {
  if (view->buttons != nullptr) {
    for (int index = 0; index < view->buttonCount; ++index) {
      RoboButton* button = &view->buttons[index];
      if (!button->loading) {
        continue;
      }
      RoboI32 x = 0;
      RoboI32 y = 0;
      if (!RoboWindowsLocateButton(view->root, 0, -view->scrollY, &button->base, &x, &y)) {
        continue;
      }
      if (y + button->base.height < 0 || y > static_cast<RoboI32>(view->pixelHeight)) {
        continue;
      }
      RoboButtonPaintSpin(button, &view->gpu, x, y);
    }
  }
  if (view->iconButtons != nullptr) {
    for (int index = 0; index < view->iconButtonCount; ++index) {
      RoboIconButton* button = &view->iconButtons[index];
      if (button->status != 1) {
        continue;
      }
      RoboI32 x = 0;
      RoboI32 y = 0;
      if (!RoboWindowsLocateButton(view->root, 0, -view->scrollY, &button->base, &x, &y)) {
        continue;
      }
      if (y + button->base.height < 0 || y > static_cast<RoboI32>(view->pixelHeight)) {
        continue;
      }
      RoboIconButtonPaintSpin(button, &view->gpu, x, y);
    }
  }
  if (view->switches != nullptr) {
    for (int index = 0; index < view->switchCount; ++index) {
      RoboSwitch* control = &view->switches[index];
      if (!control->loading) {
        continue;
      }
      RoboI32 x = 0;
      RoboI32 y = 0;
      if (!RoboWindowsLocateButton(view->root, 0, -view->scrollY, &control->base, &x, &y)) {
        continue;
      }
      if (y + control->base.height < 0 || y > static_cast<RoboI32>(view->pixelHeight)) {
        continue;
      }
      const RoboI32 extra = control->thumbPx * 18 / 100 + 4;
      RoboWindowsCopyVisible(view, x, y, control->base.width + extra, control->base.height);
      RoboSwitchPaintSpin(control, &view->gpu, x, y);
    }
  }
}

static void RoboWindowsDraw(RoboWindowsView* view) {
  if (view == nullptr || !view->ready || view->context == nullptr || view->upload == nullptr) {
    return;
  }
  const RoboBool relayout = view->layoutWidth != view->pixelWidth || view->layoutHeight != view->pixelHeight;
  if (view->root != nullptr && view->pixelWidth > 0 && view->pixelHeight > 0) {
    const RoboI32 width = static_cast<RoboI32>(view->pixelWidth);
    const RoboI32 height = static_cast<RoboI32>(view->pixelHeight);
    if (relayout || !view->frameReady) {
      view->root->width = width;
      view->root->height = height;
      RoboUILayoutMeasure(view->root, width);
      RoboWindowsRelaxText(view->root);
      RoboUILayoutTree(view->root, width, height);
      RoboWindowsFitText(view->root);
      view->root->width = width;
      view->root->height = height;
      RoboUILayoutTree(view->root, width, height);
      view->contentHeight = RoboWindowsContentBottom(view->root);
      view->layoutWidth = view->pixelWidth;
      view->layoutHeight = view->pixelHeight;
      view->fullPaint = ROBO_TRUE;
    }
    const int maxScroll = RoboWindowsMaxScroll(view);
    if (view->scrollY > maxScroll) {
      view->scrollY = maxScroll;
    }
    if (view->scrollY < 0) {
      view->scrollY = 0;
    }
    if (view->scrollTarget > maxScroll) {
      view->scrollTarget = maxScroll;
    }
    if (view->scrollTarget < 0) {
      view->scrollTarget = 0;
    }
    const RoboBool rebuild = view->fullPaint || !view->frameReady || view->sheet == nullptr;
    if (rebuild && RoboWindowsEnsureSheet(view)) {
      RoboWindowsBindTarget(view, ROBO_TRUE);
      RoboGPUClear(&view->gpu, RoboFoundationPlatformColor("bg-primary"));
      RoboUIPaintComponent(view->root, &view->gpu, 0, 0);
      RoboWindowsBindTarget(view, ROBO_FALSE);
      view->fullPaint = ROBO_FALSE;
      view->frameReady = ROBO_TRUE;
      view->dirtyCount = 0;
    } else if (rebuild) {
      RoboGPUClear(&view->gpu, RoboFoundationPlatformColor("bg-primary"));
      RoboUIPaintComponent(view->root, &view->gpu, 0, -view->scrollY);
      RoboWindowsPaintBar(view);
      view->fullPaint = ROBO_FALSE;
      view->frameReady = ROBO_TRUE;
      view->dirtyCount = 0;
      view->spinFrame = ROBO_FALSE;
      RoboWindowsPublish(view);
      return;
    } else if (view->dirtyCount > 0) {
      RoboWindowsBindTarget(view, ROBO_TRUE);
      for (int index = 0; index < view->dirtyCount; ++index) {
        RoboWindowsRedrawButton(view, view->dirtyButtons[index]);
      }
      RoboWindowsBindTarget(view, ROBO_FALSE);
      view->dirtyCount = 0;
    }
    if (view->sheet != nullptr) {
      RoboWindowsCompose(view);
    }
    if (view->spinFrame && view->sheetBound == ROBO_FALSE) {
      RoboWindowsOverlaySpins(view);
    }
  }
  view->spinFrame = ROBO_FALSE;
  RoboWindowsPublish(view);
}

static void RoboWindowsResize(RoboWindowsView* view, RoboU32 width, RoboU32 height) {
  if (view == nullptr || !view->ready || view->inResize || width == 0 || height == 0) {
    return;
  }
  view->inResize = ROBO_TRUE;
  RoboWindowsEnsureSurface(view, width, height);
  view->shownScroll = -1;
  view->gpuScroll = -1;
  view->gpuFull = ROBO_TRUE;
  view->bufferReady[0] = ROBO_FALSE;
  view->bufferReady[1] = ROBO_FALSE;
  view->recentCount[0] = 0;
  view->recentCount[1] = 0;
  view->fullPaint = ROBO_TRUE;
  view->frameReady = ROBO_FALSE;
  view->inResize = ROBO_FALSE;
}

static void RoboWindowsCopyText(const char* text, int length) {
  if (text == nullptr || length < 0 || !OpenClipboard(nullptr)) {
    return;
  }
  EmptyClipboard();
  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(length) + 1);
  if (memory != nullptr) {
    auto* dest = static_cast<char*>(GlobalLock(memory));
    if (dest != nullptr) {
      std::memcpy(dest, text, static_cast<size_t>(length));
      dest[length] = 0;
      GlobalUnlock(memory);
      if (SetClipboardData(CF_TEXT, memory) == nullptr) {
        GlobalFree(memory);
      }
    } else {
      GlobalFree(memory);
    }
  }
  CloseClipboard();
}

static int RoboWindowsPasteText(char* buffer, int capacity) {
  if (buffer == nullptr || capacity <= 0) {
    return 0;
  }
  buffer[0] = 0;
  if (!OpenClipboard(nullptr)) {
    return 0;
  }
  HANDLE data = GetClipboardData(CF_TEXT);
  if (data != nullptr) {
    const auto* text = static_cast<const char*>(GlobalLock(data));
    if (text != nullptr) {
      int index = 0;
      while (text[index] != 0 && index + 1 < capacity) {
        buffer[index] = text[index];
        ++index;
      }
      buffer[index] = 0;
      GlobalUnlock(data);
    }
  }
  CloseClipboard();
  return buffer[0] != 0 ? 1 : 0;
}

static LRESULT CALLBACK RoboWindowsProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_NCCREATE) {
    auto* created = reinterpret_cast<CREATESTRUCTW*>(lParam);
    auto* view = static_cast<RoboWindowsView*>(created->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
    if (view != nullptr) {
      view->window = window;
    }
  }
  auto* view = reinterpret_cast<RoboWindowsView*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_GETDLGCODE) {
    return DLGC_WANTALLKEYS | DLGC_WANTTAB | DLGC_WANTARROWS | DLGC_WANTCHARS;
  }
  if (message == WM_DPICHANGED) {
    if (view != nullptr) {
      view->dpi = HIWORD(wParam);
      RoboWindowsApplyType(view);
      auto* suggested = reinterpret_cast<RECT*>(lParam);
      if (suggested != nullptr) {
        SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
      }
    }
    return 0;
  }
  if (message == WM_TIMER && view != nullptr && view->ready) {
    int marked = 0;
    if (RoboWindowsGlideScroll(view)) {
      RoboWindowsWakeBar(view);
      SetTimer(window, 1, 16, nullptr);
      ++marked;
    } else {
      SetTimer(window, 1, 50, nullptr);
    }
    const RoboBool pace = view->pace == 0 || GetTickCount() - view->pace >= 48;
    if (pace) {
      view->pace = GetTickCount();
    }
    RoboBool spinning = ROBO_FALSE;
    if (view->buttons != nullptr) {
      for (int index = 0; index < view->buttonCount; ++index) {
        if (view->buttons[index].loading) {
          spinning = ROBO_TRUE;
          break;
        }
      }
    }
    const RoboU32 now = static_cast<RoboU32>(GetTickCount());
    if (view->iconButtons != nullptr) {
      for (int index = 0; index < view->iconButtonCount; ++index) {
        RoboIconButton* button = &view->iconButtons[index];
        const RoboBool changed = RoboIconButtonTick(button, now);
        if (button->status == 1) {
          spinning = ROBO_TRUE;
        } else if (changed) {
          RoboWindowsMarkButton(view, view->buttonCount + index);
          ++marked;
        }
      }
    }
    if (view->switches != nullptr) {
      const RoboU32 clock = static_cast<RoboU32>(GetTickCount());
      for (int index = 0; index < view->switchCount; ++index) {
        RoboSwitch* control = &view->switches[index];
        if (control->loading) {
          spinning = ROBO_TRUE;
        }
        if (RoboSwitchTick(control, clock)) {
          RoboWindowsMarkButton(view, view->buttonCount + view->iconButtonCount + view->fieldCount + index);
          SetTimer(window, 1, 16, nullptr);
          ++marked;
        }
      }
    }
    if (pace && spinning) {
      RoboButtonAdvanceSpin();
      view->spinFrame = ROBO_TRUE;
      ++marked;
    }
    if (pace) {
      ++view->caretTick;
    }
    if (view->pressIndex >= 0) {
      const int held = RoboWindowsFieldIndex(view, view->pressIndex);
      if (held >= 0 && view->fields[held].pointerDown && !view->fields[held].longDone && view->fields[held].pressTick != 0 &&
          GetTickCount() - view->fields[held].pressTick >= 500) {
        RoboTextFieldLongPress(&view->fields[held]);
        RoboWindowsMarkButton(view, view->pressIndex);
        ++marked;
      }
    }
    const int caretField = RoboWindowsFieldIndex(view, view->focusIndex);
    if (pace && caretField >= 0 && (view->caretTick % 10) == 0) {
      view->fields[caretField].caretOn = view->fields[caretField].caretOn ? ROBO_FALSE : ROBO_TRUE;
      RoboWindowsMarkButton(view, view->focusIndex);
      ++marked;
    }
    if (view->barHold != 0 && GetTickCount() >= view->barRepeat) {
      RoboWindowsScrollBy(view, view->barHold * RoboWindowsScale(56, view->dpi));
      RoboWindowsWakeBar(view);
      view->barRepeat = GetTickCount() + 70;
      ++marked;
    }
    if (pace && view->barOpacity > 0 && !view->barHot && !view->barDrag && view->barHold == 0 &&
        GetTickCount() - view->barIdle > 900) {
      const int drop = view->barOpacity / 6;
      view->barOpacity -= drop < 8 ? 8 : drop;
      if (view->barOpacity < 0) {
        view->barOpacity = 0;
      }
      ++marked;
    }
    if (view->uploadBusy && view->uploadQuery != nullptr &&
        view->context->GetData(view->uploadQuery, nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK) {
      view->uploadBusy = ROBO_FALSE;
    }
    if (marked > 0 || (view->uploadBusy && !RoboDamageIsEmpty(&view->gpu.damage))) {
      InvalidateRect(window, nullptr, FALSE);
    }
    return 0;
  }
  if (message == WM_SETCURSOR && view != nullptr && LOWORD(lParam) == HTCLIENT) {
    POINT cursor{};
    GetCursorPos(&cursor);
    ScreenToClient(window, &cursor);
    if (RoboWindowsBarHit(view, cursor.x, cursor.y) != 0) {
      SetCursor(LoadCursorW(nullptr, IDC_ARROW));
      return TRUE;
    }
    if (view->hoverIndex >= 0 && view->hoverIndex < RoboWindowsControlCount(view)) {
      const wchar_t* cursor = IDC_HAND;
      const int toggle = RoboWindowsSwitchIndex(view, view->hoverIndex);
      if (toggle >= 0 && view->switches[toggle].loading) {
        cursor = IDC_WAIT;
      } else if (RoboWindowsControlDisabled(view, view->hoverIndex)) {
        cursor = IDC_NO;
      } else if (RoboWindowsFieldIndex(view, view->hoverIndex) >= 0) {
        cursor = IDC_IBEAM;
      }
      SetCursor(LoadCursorW(nullptr, cursor));
      return TRUE;
    }
  }
  if ((message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK || message == WM_LBUTTONUP ||
       message == WM_MOUSELEAVE) &&
      view != nullptr && view->ready) {
    if (message == WM_MOUSEMOVE && !view->trackLeave) {
      TRACKMOUSEEVENT track{};
      track.cbSize = sizeof(track);
      track.dwFlags = TME_LEAVE;
      track.hwndTrack = window;
      TrackMouseEvent(&track);
      view->trackLeave = ROBO_TRUE;
    }
    const int x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
    const int y = static_cast<int>(static_cast<short>(HIWORD(lParam)));
    const int barHit = message == WM_MOUSELEAVE ? 0 : RoboWindowsBarHit(view, x, y);
    if (view->barDrag && message == WM_MOUSEMOVE) {
      RoboWindowsDragBar(view, y);
      RoboWindowsWakeBar(view);
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
    if (message == WM_LBUTTONUP && (view->barDrag || view->barHold != 0)) {
      view->barDrag = ROBO_FALSE;
      view->barHold = 0;
      view->barPart = barHit == 3 ? 3 : 0;
      RoboWindowsWakeBar(view);
      ReleaseCapture();
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
    if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && barHit != 0) {
      SetFocus(window);
      if (RoboWindowsFieldIndex(view, view->focusIndex) >= 0) {
        RoboWindowsFocusButton(view, -1, ROBO_FALSE);
      }
      view->barPart = barHit == 4 ? 0 : barHit;
      RoboWindowsWakeBar(view);
      SetCapture(window);
      SetTimer(window, 1, 50, nullptr);
      if (barHit == 3) {
        RoboI32 barX = 0;
        RoboI32 barY = 0;
        RoboI32 barW = 0;
        RoboI32 barH = 0;
        RoboI32 trackY = 0;
        RoboI32 trackH = 0;
        RoboI32 thumbY = 0;
        RoboI32 thumbH = 0;
        RoboWindowsBarBox(view, &barX, &barY, &barW, &barH, &trackY, &trackH, &thumbY, &thumbH);
        view->barGrab = y - thumbY;
        view->barDrag = ROBO_TRUE;
        view->barHold = 0;
      } else if (barHit == 1 || barHit == 2) {
        view->barHold = barHit == 1 ? -1 : 1;
        view->barRepeat = GetTickCount() + 70;
        RoboWindowsScrollBy(view, view->barHold * RoboWindowsScale(56, view->dpi));
      } else {
        RoboWindowsScrollToBar(view, y);
      }
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
    if (message == WM_MOUSEMOVE || message == WM_MOUSELEAVE) {
      const int part = barHit == 4 ? 0 : barHit;
      if (barHit != 0) {
        view->barHot = ROBO_TRUE;
        view->barIdle = GetTickCount();
        if (view->barOpacity < 255) {
          RoboWindowsWakeBar(view);
          InvalidateRect(window, nullptr, FALSE);
        }
        SetTimer(window, 1, 50, nullptr);
      } else if (view->barHot && !view->barDrag) {
        view->barHot = ROBO_FALSE;
        view->barIdle = GetTickCount();
        SetTimer(window, 1, 50, nullptr);
      }
      if (part != view->barPart) {
        view->barPart = part;
        InvalidateRect(window, nullptr, FALSE);
      }
    }
    if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && barHit == 0) {
      view->barHot = ROBO_FALSE;
      view->barIdle = GetTickCount();
      SetTimer(window, 1, 50, nullptr);
    }
    const int hit = message == WM_MOUSELEAVE ? -1 : RoboWindowsButtonAt(view, x, y);
    if (message == WM_MOUSELEAVE) {
      view->trackLeave = ROBO_FALSE;
    }
    if (message == WM_LBUTTONDOWN) {
      SetFocus(window);
    }
    if (message == WM_LBUTTONDOWN && (hit < 0 || RoboWindowsControlDisabled(view, hit))) {
      if (RoboWindowsFieldIndex(view, view->focusIndex) >= 0) {
        RoboWindowsFocusButton(view, -1, ROBO_FALSE);
        InvalidateRect(window, nullptr, FALSE);
      }
    }
    if (message == WM_MOUSEMOVE && view->pressIndex >= 0) {
      const int dragging = RoboWindowsFieldIndex(view, view->pressIndex);
      if (dragging >= 0) {
        RoboI32 fieldX = 0;
        RoboI32 fieldY = 0;
        if (RoboWindowsLocateButton(view->root, 0, -view->scrollY, &view->fields[dragging].base, &fieldX, &fieldY)) {
          RoboTextFieldPointerDrag(&view->fields[dragging], x - fieldX, y - fieldY);
          RoboWindowsMarkButton(view, view->pressIndex);
          InvalidateRect(window, nullptr, FALSE);
        }
      }
    }
    if (message == WM_LBUTTONDBLCLK && hit >= 0 && RoboWindowsFieldIndex(view, hit) >= 0 &&
        !RoboWindowsControlDisabled(view, hit)) {
      const int field = RoboWindowsFieldIndex(view, hit);
      RoboI32 fieldX = 0;
      RoboI32 fieldY = 0;
      view->clickCount = 2;
      view->clickField = field;
      view->clickTick = GetTickCount();
      view->pressIndex = hit;
      RoboWindowsFocusButton(view, hit, ROBO_FALSE);
      if (RoboWindowsLocateButton(view->root, 0, -view->scrollY, &view->fields[field].base, &fieldX, &fieldY)) {
        RoboTextFieldSelectWord(&view->fields[field], x - fieldX, y - fieldY);
      }
      SetCapture(window);
      SetTimer(window, 1, 50, nullptr);
      RoboWindowsMarkButton(view, hit);
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
    if (message == WM_LBUTTONDOWN && hit >= 0 && !RoboWindowsControlDisabled(view, hit)) {
      const int previousFocus = view->focusIndex;
      view->pressIndex = hit;
      RoboWindowsPointButton(view, hit, 2);
      RoboWindowsFocusButton(view, hit, ROBO_FALSE);
      const int field = RoboWindowsFieldIndex(view, hit);
      if (field >= 0) {
        RoboI32 fieldX = 0;
        RoboI32 fieldY = 0;
        if (RoboWindowsLocateButton(view->root, 0, -view->scrollY, &view->fields[field].base, &fieldX, &fieldY)) {
          const RoboBool extend = (GetKeyState(VK_SHIFT) & 0x8000) != 0 ? ROBO_TRUE : ROBO_FALSE;
          const DWORD now = GetTickCount();
          if (view->clickField == field && now - view->clickTick <= GetDoubleClickTime()) {
            view->clickCount += 1;
          } else {
            view->clickCount = 1;
          }
          view->clickTick = now;
          view->clickField = field;
          view->fields[field].pressTick = now;
          view->fields[field].longDone = ROBO_FALSE;
          if (view->clickCount >= 3) {
            view->fields[field].anchor = 0;
            view->fields[field].caret = static_cast<RoboI32>(std::strlen(view->fields[field].value));
            view->fields[field].selectByWord = ROBO_FALSE;
            view->fields[field].pointerDown = ROBO_TRUE;
            view->fields[field].caretOn = ROBO_TRUE;
          } else {
            RoboTextFieldPointerDown(&view->fields[field], x - fieldX, y - fieldY, extend);
          }
        }
        SetTimer(window, 1, 50, nullptr);
      }
      const int toggle = RoboWindowsSwitchIndex(view, hit);
      if (toggle >= 0) {
        RoboSwitchToggle(&view->switches[toggle], static_cast<RoboU32>(GetTickCount()));
        SetTimer(window, 1, 16, nullptr);
      }
      SetCapture(window);
      view->spinFrame = ROBO_FALSE;
      RoboWindowsMarkButton(view, previousFocus);
      RoboWindowsMarkButton(view, hit);
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
    if (message == WM_LBUTTONUP) {
      const int pressed = view->pressIndex;
      view->pressIndex = -1;
      if (GetCapture() == window) {
        ReleaseCapture();
      }
      if (pressed >= 0) {
        const int releasedField = RoboWindowsFieldIndex(view, pressed);
        if (releasedField >= 0) {
          view->fields[releasedField].pointerDown = ROBO_FALSE;
        }
        RoboWindowsPointButton(view, pressed, hit == pressed ? static_cast<RoboU8>(1) : static_cast<RoboU8>(0));
        if (hit == pressed && pressed >= view->buttonCount && view->iconButtons != nullptr) {
          const int icon = pressed - view->buttonCount;
          if (icon >= 0 && icon < view->iconButtonCount) {
            RoboIconButton* iconButton = &view->iconButtons[icon];
            const RoboU8 before = iconButton->status;
            RoboIconButtonPress(iconButton, static_cast<RoboU32>(GetTickCount()));
            if (iconButton->status == 1 && before != 1) {
              view->spinButtons = ROBO_TRUE;
              SetTimer(window, 1, 50, nullptr);
            }
          }
        }
        RoboWindowsMarkButton(view, pressed);
      }
      RoboWindowsHoverButton(view, hit);
      view->spinFrame = ROBO_FALSE;
      RoboWindowsMarkButton(view, hit);
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
    if (view->pressIndex >= 0) {
      const int held = RoboWindowsSwitchIndex(view, view->pressIndex);
      if (held >= 0) {
        const RoboBool down = hit == view->pressIndex ? ROBO_TRUE : ROBO_FALSE;
        if (view->switches[held].pressed != down) {
          RoboSwitchSetPressed(&view->switches[held], down, static_cast<RoboU32>(GetTickCount()));
          RoboWindowsMarkButton(view, view->pressIndex);
          InvalidateRect(window, nullptr, FALSE);
        }
      }
    }
    const int previous = view->hoverIndex;
    RoboWindowsHoverButton(view, hit);
    if (previous != view->hoverIndex) {
      view->spinFrame = ROBO_FALSE;
      RoboWindowsMarkButton(view, previous);
      RoboWindowsMarkButton(view, view->hoverIndex);
      InvalidateRect(window, nullptr, FALSE);
    }
    return 0;
  }
  if (message == WM_CHAR && view != nullptr && view->ready) {
    const int field = RoboWindowsFieldIndex(view, view->focusIndex);
    if (field >= 0 && !view->fields[field].disabled && (GetKeyState(VK_CONTROL) & 0x8000) == 0 && wParam >= 32) {
      wchar_t units[2];
      int unitCount = 0;
      if (wParam >= 0xD800 && wParam <= 0xDBFF) {
        view->pendingChar = static_cast<wchar_t>(wParam);
        return 0;
      }
      if (view->pendingChar != 0 && wParam >= 0xDC00 && wParam <= 0xDFFF) {
        units[0] = view->pendingChar;
        units[1] = static_cast<wchar_t>(wParam);
        unitCount = 2;
        view->pendingChar = 0;
      } else {
        units[0] = static_cast<wchar_t>(wParam);
        unitCount = 1;
        view->pendingChar = 0;
      }
      char text[8];
      const int written = WideCharToMultiByte(CP_UTF8, 0, units, unitCount, text, static_cast<int>(sizeof(text)) - 1,
                                              nullptr, nullptr);
      if (written > 0) {
        text[written] = 0;
        RoboTextFieldInsertText(&view->fields[field], text);
      }
      RoboWindowsMarkButton(view, view->focusIndex);
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
  }
  if (message == WM_KEYDOWN && view != nullptr && view->ready) {
    if (wParam == VK_TAB) {
      const int next = RoboWindowsStepFocus(view, (GetKeyState(VK_SHIFT) & 0x8000) != 0 ? -1 : 1);
      RoboWindowsFocusButton(view, next, ROBO_TRUE);
      const int tabField = RoboWindowsFieldIndex(view, next);
      if (tabField >= 0) {
        const int length = static_cast<int>(std::strlen(view->fields[tabField].value));
        view->fields[tabField].caret = length;
        view->fields[tabField].anchor = length;
        SetTimer(window, 1, 50, nullptr);
      }
      view->spinFrame = ROBO_FALSE;
      RoboWindowsRevealFocus(view, next);
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
    const int keyField = RoboWindowsFieldIndex(view, view->focusIndex);
    if (keyField >= 0 && !view->fields[keyField].disabled) {
      const RoboBool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0 ? ROBO_TRUE : ROBO_FALSE;
      const RoboBool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0 ? ROBO_TRUE : ROBO_FALSE;
      RoboTextField* field = &view->fields[keyField];
      if (wParam == VK_BACK) {
        RoboTextFieldCommand(field, RoboTextFieldKey::Backspace, shift, ctrl);
      } else if (wParam == VK_DELETE) {
        RoboTextFieldCommand(field, RoboTextFieldKey::Delete, shift, ctrl);
      } else if (wParam == VK_LEFT) {
        RoboTextFieldCommand(field, RoboTextFieldKey::Left, shift, ctrl);
      } else if (wParam == VK_RIGHT) {
        RoboTextFieldCommand(field, RoboTextFieldKey::Right, shift, ctrl);
      } else if (wParam == VK_HOME) {
        RoboTextFieldCommand(field, RoboTextFieldKey::Home, shift, ctrl);
      } else if (wParam == VK_END) {
        RoboTextFieldCommand(field, RoboTextFieldKey::End, shift, ctrl);
      } else if (ctrl && (wParam == 'A' || wParam == 'a')) {
        RoboTextFieldCommand(field, RoboTextFieldKey::SelectAll, shift, ctrl);
      } else if (ctrl && (wParam == 'C' || wParam == 'c')) {
        RoboTextFieldCommand(field, RoboTextFieldKey::Copy, shift, ctrl);
      } else if (ctrl && (wParam == 'X' || wParam == 'x')) {
        RoboTextFieldCommand(field, RoboTextFieldKey::Cut, shift, ctrl);
      } else if (ctrl && (wParam == 'V' || wParam == 'v')) {
        RoboTextFieldCommand(field, RoboTextFieldKey::Paste, shift, ctrl);
      } else if (ctrl && (wParam == 'Z' || wParam == 'z')) {
        RoboTextFieldCommand(field, shift ? RoboTextFieldKey::Redo : RoboTextFieldKey::Undo, shift, ctrl);
      } else if (ctrl && (wParam == 'Y' || wParam == 'y')) {
        RoboTextFieldCommand(field, RoboTextFieldKey::Redo, shift, ctrl);
      } else if (wParam != VK_RETURN && wParam != VK_SPACE && wParam != VK_SHIFT && wParam != VK_CONTROL) {
        return 0;
      } else if (wParam == VK_RETURN) {
        return 0;
      }
      RoboWindowsMarkButton(view, view->focusIndex);
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
    if ((wParam == VK_RETURN || wParam == VK_SPACE) && view->focusIndex >= 0 &&
        view->focusIndex < RoboWindowsControlCount(view) && !RoboWindowsControlDisabled(view, view->focusIndex)) {
      view->pressIndex = view->focusIndex;
      RoboWindowsPointButton(view, view->focusIndex, 2);
      const int toggle = RoboWindowsSwitchIndex(view, view->focusIndex);
      if (toggle >= 0) {
        RoboSwitchToggle(&view->switches[toggle], static_cast<RoboU32>(GetTickCount()));
        SetTimer(window, 1, 16, nullptr);
      }
      RoboWindowsFocusButton(view, view->focusIndex, ROBO_TRUE);
      view->spinFrame = ROBO_FALSE;
      RoboWindowsMarkButton(view, view->focusIndex);
      InvalidateRect(window, nullptr, FALSE);
      return 0;
    }
  }
  if (message == WM_KEYUP && view != nullptr && view->ready && (wParam == VK_RETURN || wParam == VK_SPACE)) {
    if (view->pressIndex >= 0) {
      const int pressed = view->pressIndex;
      view->pressIndex = -1;
      RoboWindowsPointButton(view, pressed, view->hoverIndex == pressed ? static_cast<RoboU8>(1) : static_cast<RoboU8>(0));
      if (pressed >= view->buttonCount && view->iconButtons != nullptr) {
        const int icon = pressed - view->buttonCount;
        if (icon >= 0 && icon < view->iconButtonCount) {
          RoboIconButton* iconButton = &view->iconButtons[icon];
          const RoboU8 before = iconButton->status;
          RoboIconButtonPress(iconButton, static_cast<RoboU32>(GetTickCount()));
          if (iconButton->status == 1 && before != 1) {
            view->spinButtons = ROBO_TRUE;
            SetTimer(window, 1, 50, nullptr);
          }
        }
      }
      view->spinFrame = ROBO_FALSE;
      RoboWindowsMarkButton(view, pressed);
      InvalidateRect(window, nullptr, FALSE);
    }
    return 0;
  }
  if (message == WM_MOUSEWHEEL && view != nullptr && view->ready) {
    UINT lines = 3;
    SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
    const int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    int step = 0;
    if (lines == WHEEL_PAGESCROLL) {
      const int page = static_cast<int>(view->pixelHeight) - RoboWindowsScale(64, view->dpi);
      step = wheelDelta > 0 ? -page : page;
    } else {
      const int line = RoboWindowsScale(48, view->dpi);
      step = -static_cast<int>(static_cast<long long>(wheelDelta) * static_cast<long long>(lines) * line / WHEEL_DELTA);
    }
    int target = view->scrollTarget + step;
    const int maxScroll = RoboWindowsMaxScroll(view);
    if (target < 0) {
      target = 0;
    }
    if (target > maxScroll) {
      target = maxScroll;
    }
    view->scrollTarget = target;
    RoboWindowsGlideScroll(view);
    RoboWindowsWakeBar(view);
    SetTimer(window, 1, 16, nullptr);
    InvalidateRect(window, nullptr, FALSE);
    return 0;
  }
  if (message == WM_SIZE && view != nullptr && wParam != SIZE_MINIMIZED) {
    RoboWindowsResize(view, LOWORD(lParam), HIWORD(lParam));
    InvalidateRect(window, nullptr, FALSE);
    return 0;
  }
  if (message == WM_ERASEBKGND) {
    return 1;
  }
  if (message == WM_PAINT) {
    PAINTSTRUCT paint{};
    BeginPaint(window, &paint);
    RoboWindowsDraw(view);
    EndPaint(window, &paint);
    return 0;
  }
  if (message == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}

int RoboWindowsHost::Run(const char* fontPath) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  RoboWindowsView view{};
  view.barShownPart = -1;
  view.shownScroll = -1;
  view.gpuScroll = -1;
  view.dpi = 96;
  view.hoverIndex = -1;
  view.pressIndex = -1;
  view.focusIndex = -1;
  const char* candidates[] = {fontPath, "Build\\esp\\EFI\\Roboticela\\Inter.rft",
                              "Build\\assets\\Fonts.robofonts", "Build\\esp\\EFI\\ROBOTICE\\INTER.RFT"};
  RoboBool loaded = ROBO_FALSE;
  for (const char* candidate : candidates) {
    if (RoboWindowsLoadFont(&view, candidate)) {
      loaded = ROBO_TRUE;
      break;
    }
  }
  if (!loaded) {
    MessageBoxW(nullptr, L"Inter font pack was not found.", L"RoboDevkitWindows", MB_OK | MB_ICONERROR);
    return 1;
  }

  HINSTANCE instance = GetModuleHandleW(nullptr);
  WNDCLASSW windowClass{};
  windowClass.lpfnWndProc = RoboWindowsProcedure;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = L"RoboDevkitWindows";
  windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  windowClass.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
  RegisterClassW(&windowClass);

  const UINT systemDpi = GetDpiForSystem();
  const int clientWidth = static_cast<int>(ROBO_WINDOWS_DESIGN_WIDTH * systemDpi / 96u);
  const int clientHeight = static_cast<int>(ROBO_WINDOWS_DESIGN_HEIGHT * systemDpi / 96u);
  RECT bounds{0, 0, clientWidth, clientHeight};
  AdjustWindowRectExForDpi(&bounds, WS_OVERLAPPEDWINDOW, FALSE, 0, systemDpi);
  HWND window = CreateWindowExW(0, windowClass.lpszClassName, L"Roboticela DevKit", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr,
                                instance, &view);
  if (window == nullptr) {
    RoboWindowsRelease(&view);
    return 1;
  }
  view.dpi = GetDpiForWindow(window);
  if (view.dpi == 0) {
    view.dpi = systemDpi == 0 ? 96u : systemDpi;
  }

  RECT client{};
  GetClientRect(window, &client);
  const UINT pixelWidth = static_cast<UINT>(client.right - client.left);
  const UINT pixelHeight = static_cast<UINT>(client.bottom - client.top);

  const D3D_FEATURE_LEVEL feature = D3D_FEATURE_LEVEL_11_0;
  if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, &feature, 1,
                               D3D11_SDK_VERSION, &view.device, nullptr, &view.context))) {
    MessageBoxW(window, L"Direct3D 11 did not start.", L"RoboDevkitWindows", MB_OK | MB_ICONERROR);
    DestroyWindow(window);
    RoboWindowsRelease(&view);
    return 1;
  }
  IDXGIDevice* dxgiDevice = nullptr;
  IDXGIAdapter* adapter = nullptr;
  IDXGIFactory2* factory = nullptr;
  if (SUCCEEDED(view.device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice))) &&
      SUCCEEDED(dxgiDevice->GetAdapter(&adapter)) &&
      SUCCEEDED(adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&factory)))) {
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = pixelWidth;
    desc.Height = pixelHeight;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    if (SUCCEEDED(factory->CreateSwapChainForHwnd(view.device, window, &desc, nullptr, nullptr, &view.swap))) {
      factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
      view.flip = ROBO_TRUE;
    }
  }
  if (factory != nullptr) {
    factory->Release();
  }
  if (adapter != nullptr) {
    adapter->Release();
  }
  if (dxgiDevice != nullptr) {
    dxgiDevice->Release();
  }
  if (view.swap == nullptr) {
    DXGI_SWAP_CHAIN_DESC swapDesc{};
    swapDesc.BufferCount = 1;
    swapDesc.BufferDesc.Width = pixelWidth;
    swapDesc.BufferDesc.Height = pixelHeight;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.OutputWindow = window;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.Windowed = TRUE;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    IDXGIDevice* legacyDevice = nullptr;
    IDXGIAdapter* legacyAdapter = nullptr;
    IDXGIFactory* legacyFactory = nullptr;
    IDXGISwapChain* legacy = nullptr;
    if (SUCCEEDED(view.device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&legacyDevice))) &&
        SUCCEEDED(legacyDevice->GetAdapter(&legacyAdapter)) &&
        SUCCEEDED(legacyAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&legacyFactory)))) {
      legacyFactory->CreateSwapChain(view.device, &swapDesc, &legacy);
    }
    if (legacy != nullptr) {
      legacy->QueryInterface(__uuidof(IDXGISwapChain1), reinterpret_cast<void**>(&view.swap));
      legacy->Release();
    }
    if (legacyFactory != nullptr) {
      legacyFactory->Release();
    }
    if (legacyAdapter != nullptr) {
      legacyAdapter->Release();
    }
    if (legacyDevice != nullptr) {
      legacyDevice->Release();
    }
  }
  if (view.swap == nullptr) {
    MessageBoxW(window, L"Direct3D 11 did not start.", L"RoboDevkitWindows", MB_OK | MB_ICONERROR);
    DestroyWindow(window);
    RoboWindowsRelease(&view);
    return 1;
  }

  const char* iconCandidates[] = {"Build\\windows\\DevKit\\Assets\\Icons.pack",
                                  "Build\\windows\\DevKit\\assets\\Icons.pack"};
  for (const char* candidate : iconCandidates) {
    if (RoboWindowsLoadIcons(&view, candidate)) {
      break;
    }
  }

  RoboTextFieldSetClipboard(RoboWindowsCopyText, RoboWindowsPasteText);
  RoboWindowsBuildCompiled(&view);
  RoboWindowsApplyType(&view);
  if (view.spinButtons) {
    SetTimer(window, 1, 50, nullptr);
  }

  if (!RoboWindowsEnsureSurface(&view, pixelWidth, pixelHeight)) {
    DestroyWindow(window);
    RoboWindowsRelease(&view);
    return 1;
  }
  view.ready = ROBO_TRUE;

  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);
  SetForegroundWindow(window);
  SetFocus(window);
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  RoboWindowsRelease(&view);
  return static_cast<int>(message.wParam);
}
