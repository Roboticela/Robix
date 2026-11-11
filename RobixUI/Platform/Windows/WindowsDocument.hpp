#pragma once

#include "../../RoboGPU/RoboGPU.hpp"

#include <string>
#include <vector>

namespace Robix::UI {

struct WindowControl {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool enabled = true;
};

void PaintWindowsDocument(RoboGPU* gpu, const std::string& path, bool light, int viewWidth, int viewHeight, int scrollY,
                          int* contentHeight, std::vector<WindowControl>* controls = nullptr, int hotControl = -1,
                          int pressedControl = -1, unsigned dpi = 96, int pointerX = -1, int pointerY = -1);

/* Repaint one screen rectangle from the retained document. The rest of the framebuffer stays as it is. */
void RestoreWindowsDocumentRect(RoboGPU* gpu, int x, int y, int width, int height);

struct WindowPatch {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

/* Lay out the document without painting. 1 when the sheet must be rasterized, 0 when it is current, -1 on failure. */
int LayoutWindowsDocument(const std::string& path, bool light, int viewWidth, int viewHeight, unsigned dpi, int hotControl,
                          int pressedControl, int pointerX, int pointerY, int* contentHeight,
                          std::vector<WindowControl>* controls);

/* Paint the laid-out document in document coordinates. The caller binds gpu at the sheet. */
void RasterWindowsDocument(RoboGPU* gpu);

/* Paint the visible window when a retained sheet is not available. */
void PaintWindowsDocumentView(RoboGPU* gpu, int scrollY, int viewWidth, int viewHeight);

/* Paint the visible document already scaled into screen pixels, so zoomed controls stay sharp. */
void PaintWindowsDocumentScaled(RoboGPU* gpu, int scrollY, int screenWidth, int screenHeight, double scale);

/* Recolor controls whose pointer or focus state changed. Patches are document rectangles. -1 means raster the sheet. */
int CollectWindowsDocumentPatches(int hotControl, int pressedControl, int focusControl, WindowPatch* patches, int capacity);

/* Draw the web focus ring in the bound buffer. Coordinates are buffer pixels. */
void PaintWindowsFocusRing(RoboGPU* gpu, int x, int y, int width, int height);

/* Repaint one document rectangle into the bound sheet. */
void PaintWindowsDocumentRect(RoboGPU* gpu, int x, int y, int width, int height);

/* True while a button, switch, or segment is loading. */
bool WindowsDocumentNeedsSpin();

/* Step every loader by one frame. */
void WindowsDocumentAdvanceSpin();

/* Document rectangles that contain a loader. */
int WindowsDocumentSpinRects(WindowPatch* patches, int capacity);

/* Where the zoom capsule sits. -1 hides it. The other values match ViewZoom::Anchor. */
int WindowsDocumentZoomAnchor();

/* Laid-out page width in document pixels, including a screen that is wider than the window. */
int WindowsDocumentContentWidth();

/* Page scroll applied while painting the visible window. The retained sheet stays at zero. */
void WindowsDocumentSetScrollX(int scrollX);

/* Scroll the overflow region under a document point. 1 when that region moved. */
int WindowsDocumentScrollBy(int docX, int docY, int dx, int dy);

/* 3 when the pointer is on a nested horizontal thumb, 4 when the track moved, 0 otherwise. */
int WindowsDocumentScrollHit(int docX, int docY);

void WindowsDocumentScrollDrag(int docX);
void WindowsDocumentScrollEnd();

struct WindowBox {
    std::string kind;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    std::string text;
};

/* Boxes the Windows host recorded for this document. -1 when the document does not lay out. */
int WindowsDocumentBoxes(const std::string& path, int viewWidth, int viewHeight, std::vector<WindowBox>& boxes);

/* SVG for a catalog icon, or the matching file in Assets/Icons. */
std::string WindowsDocumentIconSvg(const char* name);

}  // namespace Robix::UI
