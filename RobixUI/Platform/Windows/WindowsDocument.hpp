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

/* Recolor controls whose pointer state changed. Patches are document rectangles. -1 means raster the sheet. */
int CollectWindowsDocumentPatches(int hotControl, int pressedControl, WindowPatch* patches, int capacity);

/* Repaint one document rectangle into the bound sheet. */
void PaintWindowsDocumentRect(RoboGPU* gpu, int x, int y, int width, int height);

}  // namespace Robix::UI
