#pragma once

#include <algorithm>
#include <cmath>
#include <string>

namespace Robix::UI {

/* A page scale shared by pinch and viewport zoom.
   Pinch magnifies the laid-out page and does not change the layout viewport.
   Viewport zoom lays the page out in a smaller viewport, then paints it back to the window.
   Each scale is 0.25 to 5. Ctrl+wheel uses exp(-deltaY * 0.0025).
   A pinch multiplies the scale captured when the gesture began.
   The on-screen capsule is a host concern. This type only places it and changes the scale. */
struct ViewZoomBar {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int minusX = 0;
    int selectX = 0;
    int selectW = 0;
    int plusX = 0;
    int resetX = 0;
    int resetW = 0;
    int stepW = 0;
    int menuX = 0;
    int menuY = 0;
    int menuW = 0;
    int menuH = 0;
    int optionH = 0;
    int menuUp = 0;
};

struct ViewZoom {
    bool enabled = true;
    double scale = 1;
    int hot = 0;
    int pressed = 0;

    static constexpr double kMin = 0.25;
    static constexpr double kMax = 5;
    static constexpr int kPresets[6] = {25, 50, 100, 125, 150, 200};
    static constexpr int kPresetCount = 6;

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
        const int height = px(44);
        const int stepW = px(36);
        const int selectW = px(104);
        const int resetW = px(84);
        const int pad = px(4);
        const int margin = px(16);
        const int width = pad * 2 + stepW + selectW + stepW + resetW;
        bar.height = height;
        bar.width = width;
        bar.stepW = stepW;
        bar.selectW = selectW;
        bar.resetW = resetW;
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
        bar.minusX = x + pad;
        bar.selectX = bar.minusX + stepW;
        bar.plusX = bar.selectX + selectW;
        bar.resetX = bar.plusX + stepW;
        bar.optionH = px(34);
        const int menuPad = px(6);
        bar.menuW = px(148);
        bar.menuH = menuPad * 2 + bar.optionH * kPresetCount;
        bar.menuX = bar.selectX + selectW / 2 - bar.menuW / 2;
        if (bar.menuX < px(8)) {
            bar.menuX = px(8);
        }
        if (bar.menuX + bar.menuW > viewWidth - px(8)) {
            bar.menuX = std::max(px(8), viewWidth - px(8) - bar.menuW);
        }
        const int gap = px(8);
        const bool up = y + height + gap + bar.menuH > viewHeight - px(8);
        bar.menuUp = up ? 1 : 0;
        bar.menuY = up ? y - gap - bar.menuH : y + height + gap;
    }

    /* 1 minus, 2 percent select, 3 reset, 4 plus, 9 menu padding, 10+ preset row. */
    int Hit(const ViewZoomBar& bar, int x, int y, bool menuOpen) const {
        if (menuOpen && bar.optionH > 0 && x >= bar.menuX && y >= bar.menuY && x < bar.menuX + bar.menuW &&
            y < bar.menuY + bar.menuH) {
            const int pad = (bar.menuH - bar.optionH * kPresetCount) / 2;
            const int row = (y - bar.menuY - pad) / bar.optionH;
            if (row < 0 || row >= kPresetCount) {
                return 9;
            }
            return 10 + row;
        }
        if (x < bar.x || y < bar.y || x >= bar.x + bar.width || y >= bar.y + bar.height) {
            return 0;
        }
        if (x < bar.selectX) {
            return 1;
        }
        if (x < bar.plusX) {
            return 2;
        }
        if (x < bar.resetX) {
            return 4;
        }
        return 3;
    }
};

}  // namespace Robix::UI
