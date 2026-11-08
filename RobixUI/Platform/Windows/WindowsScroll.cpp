#include "WindowsScroll.hpp"

#include "../../Compiler/Foundation/FoundationPlatform.hpp"
#include "../../RoboGPU/RoboDamage.hpp"
#include "../../RoboGPU/RoboFrameBuffer.hpp"

namespace Robix::UI {
namespace {

int Scale(int designPx, unsigned dpi) {
    const unsigned used = dpi == 0 ? 96u : dpi;
    const int scaled = static_cast<int>(static_cast<unsigned>(designPx) * used / 96u);
    return scaled < 1 ? 1 : scaled;
}

void BarBox(const WindowScrollState& bar, int& x, int& y, int& width, int& height, int& trackY, int& trackH, int& thumbY,
            int& thumbH) {
    const int thick = Scale(6, bar.dpi);
    const int edge = Scale(2, bar.dpi);
    const int arrow = Scale(6, bar.dpi);
    const int gap = Scale(3, bar.dpi);
    const int minThumb = Scale(28, bar.dpi);
    width = thick;
    x = bar.pixelWidth - edge - thick;
    y = edge;
    height = bar.pixelHeight - edge * 2;
    trackY = y + arrow + gap;
    trackH = height - (arrow + gap) * 2;
    if (trackH < minThumb) {
        trackH = height > minThumb ? minThumb : height;
        trackY = y + (height - trackH) / 2;
    }
    const int maxScroll = WindowMaxScroll(bar);
    const int content = bar.contentHeight > 0 ? bar.contentHeight : 1;
    int thumb = static_cast<int>(static_cast<long long>(trackH) * bar.pixelHeight / content);
    if (thumb < minThumb) {
        thumb = minThumb;
    }
    if (thumb > trackH) {
        thumb = trackH;
    }
    const int travel = trackH - thumb > 1 ? trackH - thumb : 1;
    thumbH = thumb;
    thumbY = trackY;
    if (maxScroll > 0) {
        thumbY += static_cast<int>(static_cast<long long>(bar.scrollY) * travel / maxScroll);
    }
}

RoboU32 Mix(RoboU32 base, RoboU32 toward, int basePercent) {
    const int towardPercent = 100 - basePercent;
    RoboU32 mixed = 0xFF000000u;
    for (int shift = 16; shift >= 0; shift -= 8) {
        const int channel = (((base >> shift) & 0xFF) * basePercent + ((toward >> shift) & 0xFF) * towardPercent) / 100;
        mixed |= static_cast<RoboU32>(channel) << shift;
    }
    return mixed;
}

RoboU8 PillCoverage(int localX, int localY, int width, int height) {
    const int radius = (width < height ? width : height) * 2;
    const int left = radius;
    const int right = width * 4 - radius;
    const int top = radius;
    const int bottom = height * 4 - radius;
    const int limit = radius * radius;
    unsigned hits = 0;
    for (int sampleY = 0; sampleY < 4; ++sampleY) {
        for (int sampleX = 0; sampleX < 4; ++sampleX) {
            const int px = localX * 4 + sampleX;
            const int py = localY * 4 + sampleY;
            const int cx = px < left ? left : (px > right ? right : px);
            const int cy = py < top ? top : (py > bottom ? bottom : py);
            const int dx = px - cx;
            const int dy = py - cy;
            if (dx * dx + dy * dy <= limit) {
                ++hits;
            }
        }
    }
    return static_cast<RoboU8>(hits * 255u / 16u);
}

int Cross(int x0, int y0, int x1, int y1, int x, int y) {
    return (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);
}

RoboU8 ArrowCoverage(int localX, int localY, int size, bool up) {
    const int span = size * 4;
    const int tipX = span / 2;
    const int base = span - 2;
    const int tipY = up ? 2 : base;
    const int baseY = up ? base : 2;
    unsigned hits = 0;
    for (int sampleY = 0; sampleY < 4; ++sampleY) {
        for (int sampleX = 0; sampleX < 4; ++sampleX) {
            const int px = localX * 4 + sampleX;
            const int py = localY * 4 + sampleY;
            const int a = Cross(tipX, tipY, 2, baseY, px, py);
            const int b = Cross(2, baseY, span - 2, baseY, px, py);
            const int c = Cross(span - 2, baseY, tipX, tipY, px, py);
            if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) {
                ++hits;
            }
        }
    }
    return static_cast<RoboU8>(hits * 255u / 16u);
}

void Pill(RoboGPU* gpu, int x, int y, int width, int height, RoboU32 color, RoboU8 coverage) {
    if (gpu == nullptr || width <= 0 || height <= 0 || coverage == 0) {
        return;
    }
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const RoboU8 edge = PillCoverage(col, row, width, height);
            if (edge == 0) {
                continue;
            }
            RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>(edge * coverage / 255));
        }
    }
    RoboDamageAdd(&gpu->damage, x, y, width, height);
}

void Arrow(RoboGPU* gpu, int x, int y, int size, bool up, RoboU32 color, RoboU8 coverage) {
    if (gpu == nullptr || size <= 1 || coverage == 0) {
        return;
    }
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            const RoboU8 edge = ArrowCoverage(col, row, size, up);
            if (edge == 0) {
                continue;
            }
            RoboGPUBlendPixel(gpu, x + col, y + row, color, static_cast<RoboU8>(edge * coverage / 255));
        }
    }
    RoboDamageAdd(&gpu->damage, x, y, size, size);
}

RoboU32 Theme(const char* name, bool light) {
    return RoboFoundationPlatformColorForTheme(name, light ? ROBO_TRUE : ROBO_FALSE);
}

}  // namespace

int WindowScrollScale(int designPx, unsigned dpi) { return Scale(designPx, dpi); }

void WindowScrollBarRect(const WindowScrollState& bar, int& x, int& y, int& width, int& height) {
    int trackY = 0;
    int trackH = 0;
    int thumbY = 0;
    int thumbH = 0;
    BarBox(bar, x, y, width, height, trackY, trackH, thumbY, thumbH);
    const int edge = Scale(2, bar.dpi);
    x -= edge;
    y -= edge;
    width += edge * 2;
    height += edge * 2;
}

int WindowMaxScroll(const WindowScrollState& bar) {
    const int maxScroll = bar.contentHeight - bar.pixelHeight;
    return maxScroll > 0 ? maxScroll : 0;
}

void WindowScrollBy(WindowScrollState& bar, int delta) {
    bar.scrollY += delta;
    if (bar.scrollY < 0) {
        bar.scrollY = 0;
    }
    const int maxScroll = WindowMaxScroll(bar);
    if (bar.scrollY > maxScroll) {
        bar.scrollY = maxScroll;
    }
}

void WindowScrollToBar(WindowScrollState& bar, int pointerY) {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int trackY = 0;
    int trackH = 0;
    int thumbY = 0;
    int thumbH = 0;
    BarBox(bar, x, y, width, height, trackY, trackH, thumbY, thumbH);
    const int maxScroll = WindowMaxScroll(bar);
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
    bar.scrollY = maxScroll * ratio / 1000;
}

void WindowDragScrollBar(WindowScrollState& bar, int pointerY) {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int trackY = 0;
    int trackH = 0;
    int thumbY = 0;
    int thumbH = 0;
    BarBox(bar, x, y, width, height, trackY, trackH, thumbY, thumbH);
    const int maxScroll = WindowMaxScroll(bar);
    const int travel = trackH - thumbH > 1 ? trackH - thumbH : 1;
    int pos = pointerY - trackY - bar.grab;
    if (pos < 0) {
        pos = 0;
    }
    if (pos > travel) {
        pos = travel;
    }
    bar.scrollY = maxScroll > 0 ? static_cast<int>(static_cast<long long>(pos) * maxScroll / travel) : 0;
}

int WindowScrollThumbY(const WindowScrollState& bar) {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int trackY = 0;
    int trackH = 0;
    int thumbY = 0;
    int thumbH = 0;
    BarBox(bar, x, y, width, height, trackY, trackH, thumbY, thumbH);
    return thumbY;
}

int HitWindowScrollBar(const WindowScrollState& bar, int x, int y) {
    if (WindowMaxScroll(bar) <= 0) {
        return 0;
    }
    int barX = 0;
    int barY = 0;
    int barW = 0;
    int barH = 0;
    int trackY = 0;
    int trackH = 0;
    int thumbY = 0;
    int thumbH = 0;
    BarBox(bar, barX, barY, barW, barH, trackY, trackH, thumbY, thumbH);
    const int edge = Scale(2, bar.dpi);
    if (x < barX - edge || x >= barX + barW + edge || y < barY || y >= barY + barH) {
        return 0;
    }
    const int arrow = Scale(6, bar.dpi);
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

void PaintWindowScrollBar(RoboGPU* gpu, const WindowScrollState& bar) {
    if (gpu == nullptr || bar.opacity <= 0 || WindowMaxScroll(bar) <= 0) {
        return;
    }
    int barX = 0;
    int barY = 0;
    int barW = 0;
    int barH = 0;
    int trackY = 0;
    int trackH = 0;
    int thumbY = 0;
    int thumbH = 0;
    BarBox(bar, barX, barY, barW, barH, trackY, trackH, thumbY, thumbH);
    const RoboU32 trackColor = Theme("border-default", bar.light);
    const RoboU32 strong = Theme("border-strong", bar.light);
    const RoboU32 tertiary = Theme("text-tertiary", bar.light);
    const RoboU32 restColor = Mix(strong, tertiary, 45);
    const RoboU32 hoverColor = tertiary;
    const RoboU32 pressColor = Mix(tertiary, Theme("text-primary", bar.light), 58);
    const RoboU8 fade = static_cast<RoboU8>(bar.opacity > 255 ? 255 : bar.opacity);
    const int arrow = Scale(6, bar.dpi);
    const RoboU32 thumbInk = bar.drag ? pressColor : (bar.part == 3 ? hoverColor : restColor);
    const RoboU32 upInk = bar.hold < 0 ? pressColor : (bar.part == 1 ? hoverColor : restColor);
    const RoboU32 downInk = bar.hold > 0 ? pressColor : (bar.part == 2 ? hoverColor : restColor);
    Pill(gpu, barX, trackY, barW, trackH, trackColor, fade);
    Pill(gpu, barX, thumbY, barW, thumbH, thumbInk, fade);
    Arrow(gpu, barX, barY, arrow, true, upInk, fade);
    Arrow(gpu, barX, barY + barH - arrow, arrow, false, downInk, fade);
}

}  // namespace Robix::UI
