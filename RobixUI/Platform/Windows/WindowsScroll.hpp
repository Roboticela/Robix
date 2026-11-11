#pragma once

#include "../../RoboGPU/RoboGPU.hpp"

namespace Robix::UI {

struct WindowScrollState {
    int scrollY = 0;
    int contentHeight = 0;
    int scrollX = 0;
    int contentWidth = 0;
    int pixelWidth = 0;
    int pixelHeight = 0;
    unsigned dpi = 96;
    int opacity = 0;
    int part = 0;
    int hold = 0;
    bool drag = false;
    int grab = 0;
    bool hot = false;
    unsigned idle = 0;
    unsigned repeat = 0;
    int opacityX = 0;
    int partX = 0;
    int holdX = 0;
    bool dragX = false;
    int grabX = 0;
    bool hotX = false;
    unsigned idleX = 0;
    unsigned repeatX = 0;
    bool light = false;
};

void PaintWindowScrollBar(RoboGPU* gpu, const WindowScrollState& bar);
void WindowScrollBarRect(const WindowScrollState& bar, int& x, int& y, int& width, int& height);
int HitWindowScrollBar(const WindowScrollState& bar, int x, int y);
int WindowScrollThumbY(const WindowScrollState& bar);
void WindowScrollBy(WindowScrollState& bar, int delta);
void WindowScrollToBar(WindowScrollState& bar, int pointerY);
void WindowDragScrollBar(WindowScrollState& bar, int pointerY);
int WindowScrollScale(int designPx, unsigned dpi);
int WindowMaxScroll(const WindowScrollState& bar);

void PaintWindowScrollBarX(RoboGPU* gpu, const WindowScrollState& bar);
void WindowScrollBarXRect(const WindowScrollState& bar, int& x, int& y, int& width, int& height);
int HitWindowScrollBarX(const WindowScrollState& bar, int x, int y);
int WindowScrollThumbX(const WindowScrollState& bar);
void WindowScrollByX(WindowScrollState& bar, int delta);
void WindowScrollToBarX(WindowScrollState& bar, int pointerX);
void WindowDragScrollBarX(WindowScrollState& bar, int pointerX);
int WindowMaxScrollX(const WindowScrollState& bar);

}  // namespace Robix::UI
