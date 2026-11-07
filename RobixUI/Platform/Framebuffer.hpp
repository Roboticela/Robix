#pragma once

#include "Color.hpp"

#include <cstdint>
#include <vector>

namespace Robo {

constexpr int ROBO_FRAME_PRESENT_MAX = 32;

class FrameBuffer {
public:
    FrameBuffer() = default;
    FrameBuffer(int width, int height);

    void Resize(int width, int height);
    void Clear(const Color& color);
    void ClearRect(int x, int y, int width, int height, const Color& color);

    int GetWidth() const;
    int GetHeight() const;
    const std::uint8_t* GetPixels() const;
    std::uint8_t* GetPixels();
    std::uint32_t* Scanline(int y);
    const std::uint32_t* Scanline(int y) const;

    // Present: primary rect (union) plus optional multi-rect list.
    void SetPresentRect(int x, int y, int width, int height);
    void ClearPresentRects();
    void AddPresentRect(int x, int y, int width, int height);
    int GetPresentRectCount() const;
    bool GetPresentRectAt(int index, int& x, int& y, int& width, int& height) const;
    int GetPresentX() const;
    int GetPresentY() const;
    int GetPresentWidth() const;
    int GetPresentHeight() const;

    bool MissesClip(int x, int y, int width, int height) const;
    void SetPump(void (*pump)(void*), void* cookie);
    void Pump();

    void BlendPixel(int x, int y, const Color& color, float opacity = 1.0f);
    void FillRect(int x, int y, int width, int height, const Color& color, float opacity = 1.0f);
    void BeginDisplayList();
    void EndDisplayList();
    int DisplayOpCount() const;
    bool TakeDisplayBounds(int& x, int& y, int& width, int& height);
    void FillRoundedRect(float x, float y, float width, float height, float radius, const Color& color, float opacity = 1.0f);

    void PushClipRect(float x, float y, float width, float height);
    void PopClip();
    bool HasClip() const;
    void ResetClip();

private:
    struct ClipRect {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
    };

    struct DisplayOp {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::uint32_t color = 0;
        float opacity = 1.0f;
    };

    struct PresentRect {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    bool InsideClip(int x, int y) const;
    void ClipDrawBounds(int& x0, int& y0, int& x1, int& y1) const;
    static std::uint32_t PackRgba(const Color& color);
    void RebuildPresentUnion();

    int width_ = 0;
    int height_ = 0;
    int presentX_ = 0;
    int presentY_ = 0;
    int presentWidth_ = 0;
    int presentHeight_ = 0;
    // Byte store so freestanding vector::assign uses memset (sizeof==1). Scanline views as u32.
    std::vector<std::uint8_t> pixels_;
    PresentRect presentRects_[ROBO_FRAME_PRESENT_MAX]{};
    int presentRectCount_ = 0;
    std::vector<ClipRect> clips_;
    std::vector<DisplayOp> displayOps_;
    bool recordDisplay_ = false;
    void (*pump_)(void*) = nullptr;
    void* pumpCookie_ = nullptr;
};

}  // namespace Robo
