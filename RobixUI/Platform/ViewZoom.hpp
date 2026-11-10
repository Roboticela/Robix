#pragma once

#include <algorithm>
#include <cmath>

namespace Robix::UI {

/* Page zoom shared by every platform host.
   The scale matches the web document zoom: 0.25 to 4, Ctrl+wheel uses exp(-deltaY * 0.0025),
   and a pinch multiplies the scale captured when the gesture began.
   A host feeds pointer, wheel, and key events in. It does not depend on an operating system. */
struct ViewZoomBar {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int toggleX = 0;
    int toggleW = 0;
    int minusX = 0;
    int labelX = 0;
    int labelW = 0;
    int plusX = 0;
    int stepW = 0;
};

struct ViewZoom {
    bool enabled = true;
    double scale = 1;
    int hot = 0;
    int pressed = 0;

    static constexpr double kMin = 0.25;
    static constexpr double kMax = 4;

    bool Apply(double next) {
        if (next < kMin) {
            next = kMin;
        }
        if (next > kMax) {
            next = kMax;
        }
        if (std::abs(next - scale) < 0.0005) {
            return false;
        }
        scale = next;
        return true;
    }

    /* Browser wheel delta. Positive deltaY zooms out. */
    bool Wheel(double deltaY) {
        if (!enabled) {
            return false;
        }
        return Apply(scale * std::exp(-deltaY * 0.0025));
    }

    /* origin is the scale when the pinch began. ratio is the current span divided by the starting span. */
    bool Pinch(double origin, double ratio) {
        if (!enabled || !(ratio > 0)) {
            return false;
        }
        return Apply(origin * ratio);
    }

    /* -1 zooms out, +1 zooms in, 0 returns to 100%. */
    bool Step(int direction) {
        if (!enabled) {
            return false;
        }
        if (direction == 0) {
            return Apply(1);
        }
        const double factor = direction > 0 ? 1.1 : 1.0 / 1.1;
        return Apply(scale * factor);
    }

    /* Turns pinch, wheel, and keyboard zoom on or off. Turning it off returns the page to 100%. */
    bool Toggle() {
        enabled = !enabled;
        if (!enabled) {
            scale = 1;
        }
        return true;
    }

    unsigned ScaledDpi(unsigned baseDpi) const {
        const double dpi = static_cast<double>(baseDpi == 0 ? 96u : baseDpi) * scale;
        if (dpi < 1) {
            return 1;
        }
        return static_cast<unsigned>(dpi + 0.5);
    }

    int Percent() const { return static_cast<int>(scale * 100 + 0.5); }

    void Place(int viewWidth, int viewHeight, unsigned baseDpi, ViewZoomBar& bar) const {
        const int dpi = static_cast<int>(baseDpi == 0 ? 96u : baseDpi);
        const auto px = [&](int value) { return std::max(1, value * dpi / 96); };
        const int gap = px(6);
        const int height = px(36);
        const int toggleW = px(72);
        const int stepW = px(36);
        const int labelW = px(68);
        const int margin = px(16);
        const int width = toggleW + stepW + labelW + stepW + gap * 3;
        bar.height = height;
        bar.width = width;
        bar.x = margin;
        bar.y = std::max(0, viewHeight - margin - height);
        bar.toggleX = bar.x;
        bar.toggleW = toggleW;
        bar.minusX = bar.toggleX + toggleW + gap;
        bar.stepW = stepW;
        bar.labelX = bar.minusX + stepW + gap;
        bar.labelW = labelW;
        bar.plusX = bar.labelX + labelW + gap;
    }

    /* 1 toggle, 2 minus, 3 percent, 4 plus. */
    int Hit(const ViewZoomBar& bar, int x, int y) const {
        if (x < bar.x || y < bar.y || x >= bar.x + bar.width || y >= bar.y + bar.height) {
            return 0;
        }
        if (x < bar.toggleX + bar.toggleW) {
            return 1;
        }
        if (x >= bar.minusX && x < bar.minusX + bar.stepW) {
            return 2;
        }
        if (x >= bar.labelX && x < bar.labelX + bar.labelW) {
            return 3;
        }
        if (x >= bar.plusX && x < bar.plusX + bar.stepW) {
            return 4;
        }
        return 0;
    }
};

}  // namespace Robix::UI
