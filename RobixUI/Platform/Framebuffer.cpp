#include "Framebuffer.hpp"

#include <algorithm>
#include <cmath>

namespace Robo {

namespace {

inline std::uint32_t PackBytes(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    return static_cast<std::uint32_t>(r) | (static_cast<std::uint32_t>(g) << 8) | (static_cast<std::uint32_t>(b) << 16) |
           (static_cast<std::uint32_t>(a) << 24);
}

// RefCode AK::fast_u32_fill — dword store; freestanding-safe loop (clang kernel has no rep stosl path).
inline void FastU32Fill(std::uint32_t* dest, std::uint32_t value, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        dest[i] = value;
    }
}

// RefCode LibGfx::Color::blend
inline std::uint32_t BlendRgba(std::uint32_t dst, std::uint8_t sr, std::uint8_t sg, std::uint8_t sb, std::uint8_t sa) {
    if (sa == 0) {
        return dst;
    }
    if (sa == 255) {
        return PackBytes(sr, sg, sb, 255);
    }
    const std::uint8_t dr = static_cast<std::uint8_t>(dst & 0xFFu);
    const std::uint8_t dg = static_cast<std::uint8_t>((dst >> 8) & 0xFFu);
    const std::uint8_t db = static_cast<std::uint8_t>((dst >> 16) & 0xFFu);
    const std::uint8_t da = static_cast<std::uint8_t>((dst >> 24) & 0xFFu);
    if (da == 0) {
        return PackBytes(sr, sg, sb, sa);
    }
    const int d = 255 * (da + sa) - da * sa;
    if (d <= 0) {
        return dst;
    }
    const std::uint8_t r = static_cast<std::uint8_t>((dr * da * (255 - sa) + sr * 255 * sa) / d);
    const std::uint8_t g = static_cast<std::uint8_t>((dg * da * (255 - sa) + sg * 255 * sa) / d);
    const std::uint8_t b = static_cast<std::uint8_t>((db * da * (255 - sa) + sb * 255 * sa) / d);
    const std::uint8_t a = static_cast<std::uint8_t>(d / 255);
    return PackBytes(r, g, b, a);
}

}  // namespace

std::uint32_t FrameBuffer::PackRgba(const Color& color) {
    return PackBytes(color.red, color.green, color.blue, color.alpha);
}

FrameBuffer::FrameBuffer(int width, int height) {
    Resize(width, height);
}

void FrameBuffer::Resize(int width, int height) {
    const int nextW = std::max(0, width);
    const int nextH = std::max(0, height);
    const std::size_t need = static_cast<std::size_t>(nextW) * static_cast<std::size_t>(nextH) * 4u;
    pixels_.assign(need, 0);
    if (need > 0 && pixels_.size() != need) {
        return;
    }
    width_ = nextW;
    height_ = nextH;
    ClearPresentRects();
}

void FrameBuffer::Clear(const Color& color) {
    if (width_ <= 0 || height_ <= 0 || pixels_.empty()) {
        return;
    }
    FastU32Fill(reinterpret_cast<std::uint32_t*>(pixels_.data()), PackRgba(color),
                static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_));
}

void FrameBuffer::ClipDrawBounds(int& x0, int& y0, int& x1, int& y1) const {
    x0 = std::max(0, x0);
    y0 = std::max(0, y0);
    x1 = std::min(width_, x1);
    y1 = std::min(height_, y1);
    if (!clips_.empty()) {
        const ClipRect& clip = clips_.back();
        x0 = std::max(x0, clip.left);
        y0 = std::max(y0, clip.top);
        x1 = std::min(x1, clip.right);
        y1 = std::min(y1, clip.bottom);
    }
}

void FrameBuffer::ClearRect(int x, int y, int width, int height, const Color& color) {
    int x0 = x;
    int y0 = y;
    int x1 = x + width;
    int y1 = y + height;
    ClipDrawBounds(x0, y0, x1, y1);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    const std::uint32_t packed = PackRgba(color);
    const int span = x1 - x0;
    for (int py = y0; py < y1; ++py) {
        std::uint32_t* row = Scanline(py);
        if (row == nullptr) {
            return;
        }
        FastU32Fill(row + x0, packed, static_cast<std::size_t>(span));
    }
}

int FrameBuffer::GetWidth() const {
    return width_;
}

int FrameBuffer::GetHeight() const {
    return height_;
}

const std::uint8_t* FrameBuffer::GetPixels() const {
    return pixels_.data();
}

std::uint8_t* FrameBuffer::GetPixels() {
    return pixels_.data();
}

std::uint32_t* FrameBuffer::Scanline(int y) {
    if (y < 0 || y >= height_ || pixels_.empty()) {
        return nullptr;
    }
    return reinterpret_cast<std::uint32_t*>(pixels_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) * 4u);
}

const std::uint32_t* FrameBuffer::Scanline(int y) const {
    if (y < 0 || y >= height_ || pixels_.empty()) {
        return nullptr;
    }
    return reinterpret_cast<const std::uint32_t*>(pixels_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) * 4u);
}

void FrameBuffer::RebuildPresentUnion() {
    if (presentRectCount_ <= 0) {
        presentX_ = 0;
        presentY_ = 0;
        presentWidth_ = 0;
        presentHeight_ = 0;
        return;
    }
    int left = presentRects_[0].x;
    int top = presentRects_[0].y;
    int right = left + presentRects_[0].width;
    int bottom = top + presentRects_[0].height;
    for (int i = 1; i < presentRectCount_; ++i) {
        const PresentRect& r = presentRects_[i];
        left = std::min(left, r.x);
        top = std::min(top, r.y);
        right = std::max(right, r.x + r.width);
        bottom = std::max(bottom, r.y + r.height);
    }
    presentX_ = left;
    presentY_ = top;
    presentWidth_ = std::max(0, right - left);
    presentHeight_ = std::max(0, bottom - top);
}

void FrameBuffer::ClearPresentRects() {
    presentRectCount_ = 0;
    presentX_ = 0;
    presentY_ = 0;
    presentWidth_ = 0;
    presentHeight_ = 0;
}

void FrameBuffer::SetPresentRect(int x, int y, int width, int height) {
    ClearPresentRects();
    AddPresentRect(x, y, width, height);
}

void FrameBuffer::AddPresentRect(int x, int y, int width, int height) {
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(width_, x + std::max(0, width));
    int y1 = std::min(height_, y + std::max(0, height));
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    PresentRect next{x0, y0, x1 - x0, y1 - y0};
    for (int i = 0; i < presentRectCount_; ++i) {
        PresentRect& cur = presentRects_[i];
        const bool intersects = cur.x < next.x + next.width && next.x < cur.x + cur.width && cur.y < next.y + next.height &&
                                next.y < cur.y + cur.height;
        if (intersects) {
            const int left = std::min(cur.x, next.x);
            const int top = std::min(cur.y, next.y);
            const int right = std::max(cur.x + cur.width, next.x + next.width);
            const int bottom = std::max(cur.y + cur.height, next.y + next.height);
            cur.x = left;
            cur.y = top;
            cur.width = right - left;
            cur.height = bottom - top;
            RebuildPresentUnion();
            return;
        }
    }
    if (presentRectCount_ >= ROBO_FRAME_PRESENT_MAX) {
        RebuildPresentUnion();
        const int left = std::min(presentX_, next.x);
        const int top = std::min(presentY_, next.y);
        const int right = std::max(presentX_ + presentWidth_, next.x + next.width);
        const int bottom = std::max(presentY_ + presentHeight_, next.y + next.height);
        presentRectCount_ = 1;
        presentRects_[0] = {left, top, right - left, bottom - top};
        RebuildPresentUnion();
        return;
    }
    presentRects_[presentRectCount_++] = next;
    RebuildPresentUnion();
}

int FrameBuffer::GetPresentRectCount() const {
    return presentRectCount_;
}

bool FrameBuffer::GetPresentRectAt(int index, int& x, int& y, int& width, int& height) const {
    if (index < 0 || index >= presentRectCount_) {
        width = 0;
        height = 0;
        return false;
    }
    const PresentRect& r = presentRects_[index];
    x = r.x;
    y = r.y;
    width = r.width;
    height = r.height;
    return width > 0 && height > 0;
}

int FrameBuffer::GetPresentX() const {
    return presentX_;
}

int FrameBuffer::GetPresentY() const {
    return presentY_;
}

int FrameBuffer::GetPresentWidth() const {
    return presentWidth_;
}

int FrameBuffer::GetPresentHeight() const {
    return presentHeight_;
}

void FrameBuffer::SetPump(void (*pump)(void*), void* cookie) {
    pump_ = pump;
    pumpCookie_ = cookie;
}

void FrameBuffer::Pump() {
    if (pump_ != nullptr) {
        pump_(pumpCookie_);
    }
}

bool FrameBuffer::MissesClip(int x, int y, int width, int height) const {
    if (clips_.empty() || width <= 0 || height <= 0) {
        return false;
    }
    const ClipRect& clip = clips_.back();
    return x + width <= clip.left || y + height <= clip.top || x >= clip.right || y >= clip.bottom;
}

void FrameBuffer::BlendPixel(int x, int y, const Color& color, float opacity) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }
    if (!clips_.empty() && !InsideClip(x, y)) {
        return;
    }

    float sourceAlphaF = (static_cast<float>(color.alpha) / 255.0f) * opacity;
    if (sourceAlphaF <= 0.0f) {
        return;
    }
    std::uint8_t sa = static_cast<std::uint8_t>(std::min(255.0f, sourceAlphaF * 255.0f + 0.5f));
    if (sa == 0) {
        return;
    }

    std::uint32_t* row = Scanline(y);
    if (row == nullptr) {
        return;
    }
    if (sa >= 254) {
        row[x] = PackBytes(color.red, color.green, color.blue, 255);
        return;
    }
    row[x] = BlendRgba(row[x], color.red, color.green, color.blue, sa);
}

void FrameBuffer::FillRoundedRect(float x, float y, float width, float height, float radius, const Color& color, float opacity) {
    if (width <= 0.0f || height <= 0.0f || opacity <= 0.0f || color.alpha == 0) {
        return;
    }

    const float cappedRadius = std::max(0.0f, std::min(radius, std::min(width, height) * 0.5f));
    const float centerX = x + width * 0.5f;
    const float centerY = y + height * 0.5f;
    const float halfW = width * 0.5f - cappedRadius;
    const float halfH = height * 0.5f - cappedRadius;
    int x0 = static_cast<int>(std::floor(x - 1.0f));
    int y0 = static_cast<int>(std::floor(y - 1.0f));
    int x1 = static_cast<int>(std::ceil(x + width + 1.0f));
    int y1 = static_cast<int>(std::ceil(y + height + 1.0f));
    if (x1 <= 0 || y1 <= 0 || x0 >= width_ || y0 >= height_) {
        return;
    }
    x0 = std::max(0, x0);
    y0 = std::max(0, y0);
    x1 = std::min(width_, x1);
    y1 = std::min(height_, y1);

    const int innerLeft = std::max(x0, static_cast<int>(std::ceil(x + 1.0f)));
    const int innerRight = std::min(x1, static_cast<int>(std::floor(x + width - 1.0f)));
    const int innerTop = std::max(y0, static_cast<int>(std::ceil(y + cappedRadius)));
    const int innerBottom = std::min(y1, static_cast<int>(std::floor(y + height - cappedRadius)));
    const bool solid = innerRight > innerLeft && innerBottom > innerTop;
    if (solid) {
        FillRect(innerLeft, innerTop, innerRight - innerLeft, innerBottom - innerTop, color, opacity);
    }

    auto Cover = [&](int px, int py) {
        const float fx = static_cast<float>(px) + 0.5f - centerX;
        const float fy = static_cast<float>(py) + 0.5f - centerY;
        const float qx = std::abs(fx) - halfW;
        const float qy = std::abs(fy) - halfH;
        const float outsideX = std::max(qx, 0.0f);
        const float outsideY = std::max(qy, 0.0f);
        const float distance = std::sqrt(outsideX * outsideX + outsideY * outsideY) + std::min(std::max(qx, qy), 0.0f) - cappedRadius;
        float coverage = std::max(0.0f, std::min(1.0f, 0.5f - distance));
        if (coverage > 0.0f && coverage < 1.0f) {
            coverage = std::pow(coverage, 1.0f / 1.8f);
        }
        if (coverage > 0.0f) {
            BlendPixel(px, py, color, opacity * coverage);
        }
    };

    if (!solid) {
        for (int py = y0; py < y1; ++py) {
            for (int px = x0; px < x1; ++px) {
                Cover(px, py);
            }
        }
        return;
    }

    for (int py = y0; py < innerTop; ++py) {
        for (int px = x0; px < x1; ++px) {
            Cover(px, py);
        }
    }
    for (int py = innerTop; py < innerBottom; ++py) {
        for (int px = x0; px < innerLeft; ++px) {
            Cover(px, py);
        }
        for (int px = innerRight; px < x1; ++px) {
            Cover(px, py);
        }
    }
    for (int py = innerBottom; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            Cover(px, py);
        }
    }
}

void FrameBuffer::BeginDisplayList() {
    displayOps_.clear();
    recordDisplay_ = true;
}

void FrameBuffer::EndDisplayList() {
    recordDisplay_ = false;
}

int FrameBuffer::DisplayOpCount() const {
    return static_cast<int>(displayOps_.size());
}

bool FrameBuffer::TakeDisplayBounds(int& x, int& y, int& width, int& height) {
    if (displayOps_.empty()) {
        width = 0;
        height = 0;
        return false;
    }
    int left = displayOps_[0].x;
    int top = displayOps_[0].y;
    int right = left + displayOps_[0].width;
    int bottom = top + displayOps_[0].height;
    for (const DisplayOp& op : displayOps_) {
        if (op.width <= 0 || op.height <= 0) {
            continue;
        }
        left = std::min(left, op.x);
        top = std::min(top, op.y);
        right = std::max(right, op.x + op.width);
        bottom = std::max(bottom, op.y + op.height);
    }
    displayOps_.clear();
    if (right <= left || bottom <= top) {
        width = 0;
        height = 0;
        return false;
    }
    x = left;
    y = top;
    width = right - left;
    height = bottom - top;
    return true;
}

void FrameBuffer::FillRect(int x, int y, int width, int height, const Color& color, float opacity) {
    if (recordDisplay_ && displayOps_.size() < 4096u) {
        DisplayOp op;
        op.x = x;
        op.y = y;
        op.width = width;
        op.height = height;
        op.color = (static_cast<std::uint32_t>(color.red) << 16) | (static_cast<std::uint32_t>(color.green) << 8) |
                   static_cast<std::uint32_t>(color.blue);
        op.opacity = opacity;
        displayOps_.push_back(op);
    }
    int x0 = x;
    int y0 = y;
    int x1 = x + width;
    int y1 = y + height;
    ClipDrawBounds(x0, y0, x1, y1);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    if (opacity >= 0.996f && color.alpha == 255) {
        const std::uint32_t packed = PackBytes(color.red, color.green, color.blue, 255);
        const int span = x1 - x0;
        for (int py = y0; py < y1; ++py) {
            std::uint32_t* row = Scanline(py);
            if (row == nullptr) {
                return;
            }
            FastU32Fill(row + x0, packed, static_cast<std::size_t>(span));
        }
        return;
    }
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            BlendPixel(px, py, color, opacity);
        }
    }
}

bool FrameBuffer::InsideClip(int x, int y) const {
    for (const ClipRect& clip : clips_) {
        if (x < clip.left || y < clip.top || x >= clip.right || y >= clip.bottom) {
            return false;
        }
    }
    return true;
}

void FrameBuffer::PushClipRect(float x, float y, float width, float height) {
    const int left = std::max(0, static_cast<int>(std::floor(x)));
    const int top = std::max(0, static_cast<int>(std::floor(y)));
    const int right = std::min(width_, static_cast<int>(std::ceil(x + std::max(0.0f, width))));
    const int bottom = std::min(height_, static_cast<int>(std::ceil(y + std::max(0.0f, height))));

    ClipRect rect{left, top, std::max(left, right), std::max(top, bottom)};
    if (!clips_.empty()) {
        const ClipRect& parent = clips_.back();
        rect.left = std::max(rect.left, parent.left);
        rect.top = std::max(rect.top, parent.top);
        rect.right = std::min(rect.right, parent.right);
        rect.bottom = std::min(rect.bottom, parent.bottom);
    }
    clips_.push_back(rect);
}

void FrameBuffer::PopClip() {
    if (!clips_.empty()) {
        clips_.pop_back();
    }
}

bool FrameBuffer::HasClip() const {
    return !clips_.empty();
}

void FrameBuffer::ResetClip() {
    clips_.clear();
}

}  // namespace Robo
