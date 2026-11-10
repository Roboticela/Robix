#pragma once

#include <algorithm>
#include <cmath>
#include <string>

namespace Robix::UI {

/* Page zoom shared by every platform host.
   The scale matches the web document zoom: 0.25 to 4, Ctrl+wheel uses exp(-deltaY * 0.0025),
   and a pinch multiplies the scale captured when the gesture began.
   The on-screen capsule is a host concern. This type only places it and changes the scale. */
struct ViewZoomBar {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
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

    /* -1 hidden. 0 bottom, 1 bottom-left, 2 bottom-right, 3 top, 4 top-left, 5 top-right, 6 left, 7 right, 8 center. */
    static int Anchor(const char* value) {
        std::string text;
        if (value != nullptr) {
            for (const char* cursor = value; *cursor != '\0'; ++cursor) {
                const char raw = *cursor;
                if (raw == ' ' || raw == '_' || raw == '-') {
                    continue;
                }
                text.push_back(raw >= 'A' && raw <= 'Z' ? static_cast<char>(raw - 'A' + 'a') : raw);
            }
        }
        if (text.empty() || text == "bottom" || text == "bottomcenter") {
            return 0;
        }
        if (text == "none" || text == "off" || text == "false" || text == "hidden" || text == "0") {
            return -1;
        }
        if (text == "bottomleft") {
            return 1;
        }
        if (text == "bottomright") {
            return 2;
        }
        if (text == "top" || text == "topcenter") {
            return 3;
        }
        if (text == "topleft") {
            return 4;
        }
        if (text == "topright") {
            return 5;
        }
        if (text == "left" || text == "centerleft") {
            return 6;
        }
        if (text == "right" || text == "centerright") {
            return 7;
        }
        if (text == "center" || text == "middle") {
            return 8;
        }
        return 0;
    }

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

    unsigned ScaledDpi(unsigned baseDpi) const {
        const double dpi = static_cast<double>(baseDpi == 0 ? 96u : baseDpi) * scale;
        if (dpi < 1) {
            return 1;
        }
        return static_cast<unsigned>(dpi + 0.5);
    }

    int Percent() const { return static_cast<int>(scale * 100 + 0.5); }

    void Place(int viewWidth, int viewHeight, unsigned baseDpi, int anchor, ViewZoomBar& bar) const {
        const int dpi = static_cast<int>(baseDpi == 0 ? 96u : baseDpi);
        const auto px = [&](int value) { return std::max(1, value * dpi / 96); };
        const int height = px(32);
        const int stepW = px(32);
        const int labelW = px(58);
        const int margin = px(16);
        const int width = stepW + labelW + stepW + px(6);
        bar.height = height;
        bar.width = width;
        bar.stepW = stepW;
        bar.labelW = labelW;
        int x = margin;
        int y = std::max(0, viewHeight - margin - height);
        if (anchor == 2 || anchor == 5 || anchor == 7) {
            x = std::max(0, viewWidth - margin - width);
        } else if (anchor == 0 || anchor == 3 || anchor == 8) {
            x = std::max(0, (viewWidth - width) / 2);
        }
        if (anchor == 3 || anchor == 4 || anchor == 5) {
            y = margin;
        } else if (anchor == 6 || anchor == 7 || anchor == 8) {
            y = std::max(0, (viewHeight - height) / 2);
        }
        bar.x = x;
        bar.y = y;
        bar.minusX = x + px(3);
        bar.labelX = bar.minusX + stepW;
        bar.plusX = bar.labelX + labelW;
    }

    /* 1 minus, 2 percent, 3 plus. */
    int Hit(const ViewZoomBar& bar, int x, int y) const {
        if (x < bar.x || y < bar.y || x >= bar.x + bar.width || y >= bar.y + bar.height) {
            return 0;
        }
        if (x < bar.labelX) {
            return 1;
        }
        if (x < bar.plusX) {
            return 2;
        }
        return 3;
    }
};

}  // namespace Robix::UI
