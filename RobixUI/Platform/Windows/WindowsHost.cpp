#include "WindowsHost.hpp"
#include "WindowsDocument.hpp"
#include "WindowsIcon.hpp"
#include "WindowsScroll.hpp"
#include "../ViewZoom.hpp"

#include "../../Compiler/Foundation/FoundationPlatform.hpp"
#include "../../RoboGPU/RoboGPU.hpp"
#include "../../RoboGPU/RoboText.hpp"
#include "../../RoboShared/RoboFont.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <malloc.h>

void RoboWindowsInstallFontRaster(const RoboU8* pack, RoboSize packBytes);

namespace Robix::UI {
namespace {

constexpr UINT kDesignWidth = 960;
constexpr UINT kDesignHeight = 640;

struct HostView {
    HWND window = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain1* swap = nullptr;
    ID3D11Texture2D* upload = nullptr;
    ID3D11Query* uploadQuery = nullptr;
    RoboGPU gpu{};
    RoboU8* back = nullptr;
    RoboU8* sheet = nullptr;
    UINT pixelWidth = 0;
    UINT pixelHeight = 0;
    UINT sheetWidth = 0;
    UINT sheetHeight = 0;
    bool flip = false;
    bool uploadBusy = false;
    bool gpuFull = false;
    bool inResize = false;
    bool frameNoted = false;
    bool ready = false;
    bool painted = false;
    bool stayOpen = false;
    bool light = false;
    bool screen = false;
    bool hasScreenColor = false;
    RoboU32 screenColor = 0;
    bool movable = true;
    int scrollY = 0;
    int scrollX = 0;
    int contentHeight = 0;
    int contentWidth = 0;
    int shownScrollX = -1;
    bool regionDrag = false;
    bool panLive = false;
    unsigned baseDpi = 96;
    unsigned dpi = 96;
    ViewZoom zoom{};
    ViewZoomBar zoomBar{};
    int zoomOpacity = 0;
    bool zoomMenu = false;
    unsigned zoomIdle = 0;
    int zoomBoxX = 0;
    int zoomBoxY = 0;
    int zoomBoxW = 0;
    int zoomBoxH = 0;
    bool spinArmed = false;
    unsigned spinClock = 0;
    bool pinchLive = false;
    double pinchOrigin = 1;
    double pinchDistance = 0;
    UINT pinchId[2] = {};
    int pinchX[2] = {};
    int pinchY[2] = {};
    int pinchCount = 0;
    WindowScrollState bar{};
    std::vector<WindowControl> controls;
    int hotControl = -1;
    int pressedControl = -1;
    int focusControl = -1;
    int pointerX = -1;
    int pointerY = -1;
    bool trackLeave = false;
    std::string document;
    WindowRunOptions* watched = nullptr;
    int gpuScroll = -1;
    int presentIndex = 0;
    bool bufferReady[2] = {};
    RECT recent[2][16] = {};
    int recentCount[2] = {};
    int shownOpacity = -1;
    int shownPart = -1;
    int shownHold = 0;
    bool shownDrag = false;
    bool shownHot = false;
    int shownOpacityX = -1;
    int shownPartX = -1;
    int shownHoldX = 0;
    bool shownDragX = false;
    bool shownHotX = false;
    int shownScroll = -1;
};

void PaintFoundation(RoboGPU* gpu, bool light, bool drawText, bool hasScreenColor, RoboU32 screenColor) {
    RoboWindowsInstallFontRaster(nullptr, 0);
    const RoboU32 color = hasScreenColor
                              ? screenColor
                              : RoboFoundationPlatformColorForTheme("bg-primary", light ? ROBO_TRUE : ROBO_FALSE);
    RoboGPUClear(gpu, color);
    if (!drawText) {
        return;
    }
    RoboFoundationTypeMetrics metrics{};
    RoboU16 size = 17;
    if (RoboFoundationPlatformType("body", &metrics) == ROBO_TRUE) {
        size = metrics.sizePx;
    }
    RoboFontHeader header{};
    header.magic = ROBO_FONT_MAGIC;
    header.ascent = size;
    header.reserved0 = size;
    header.glyphCount = 0;
    RoboFontView font{};
    font.header = &header;
    RoboTextDrawUTF8(gpu, font, 16, 16, "Robix", RoboFoundationPlatformColor("text-primary"), size);
}

bool EnsureSurface(HostView* view, UINT width, UINT height) {
    if (view == nullptr || view->swap == nullptr || view->device == nullptr || width == 0 || height == 0) {
        return false;
    }
    if (view->upload != nullptr && view->pixelWidth == width && view->pixelHeight == height && view->back != nullptr) {
        return true;
    }
    if (view->upload != nullptr) {
        view->upload->Release();
        view->upload = nullptr;
    }
    if (view->uploadQuery != nullptr) {
        view->uploadQuery->Release();
        view->uploadQuery = nullptr;
    }
    view->uploadBusy = false;
    if (FAILED(view->swap->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) {
        return false;
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
        return false;
    }
    D3D11_QUERY_DESC queryDesc{};
    queryDesc.Query = D3D11_QUERY_EVENT;
    view->device->CreateQuery(&queryDesc, &view->uploadQuery);
    const RoboU64 byteCount = static_cast<RoboU64>(width) * height * 4u;
    auto* back = static_cast<RoboU8*>(_aligned_malloc(static_cast<size_t>(byteCount), 16));
    if (back == nullptr) {
        return false;
    }
    std::memset(back, 0, static_cast<size_t>(byteCount));
    _aligned_free(view->back);
    _aligned_free(view->sheet);
    view->sheet = nullptr;
    view->sheetWidth = 0;
    view->sheetHeight = 0;
    view->back = back;
    view->pixelWidth = width;
    view->pixelHeight = height;
    RoboHandoff handoff{};
    handoff.magic = ROBO_HANDOFF_MAGIC;
    handoff.version = ROBO_HANDOFF_VERSION;
    handoff.frameBuffer.base = reinterpret_cast<RoboU64>(view->back);
    handoff.frameBuffer.width = width;
    handoff.frameBuffer.height = height;
    handoff.frameBuffer.pixelsPerScanLine = width;
    handoff.frameBuffer.bytesPerPixel = 4;
    handoff.frameBuffer.redMask = 0x00FF0000u;
    handoff.frameBuffer.greenMask = 0x0000FF00u;
    handoff.frameBuffer.blueMask = 0x000000FFu;
    RoboGPUInitFromHandoff(&view->gpu, &handoff, view->back, byteCount);
    view->gpuScroll = -1;
    view->gpuFull = true;
    view->shownScroll = -1;
    view->bufferReady[0] = false;
    view->bufferReady[1] = false;
    view->recentCount[0] = 0;
    view->recentCount[1] = 0;
    return true;
}

double PageScale(const HostView* view) {
    double scale = view == nullptr ? 1.0 : view->zoom.scale;
    if (!(scale > 0.0)) {
        scale = 1.0;
    }
    if (scale < ViewZoom::kMin) {
        scale = ViewZoom::kMin;
    }
    if (scale > ViewZoom::kMax) {
        scale = ViewZoom::kMax;
    }
    return scale;
}

int ToDoc(int windowPx, double scale) { return static_cast<int>(std::lround(static_cast<double>(windowPx) / scale)); }

int ToWin(int docPx, double scale) { return static_cast<int>(std::lround(static_cast<double>(docPx) * scale)); }

void FillBar(HostView* view, WindowScrollState& bar) {
    const double scale = PageScale(view);
    bar = view->bar;
    bar.scrollY = ToWin(view->scrollY, scale);
    bar.contentHeight = std::max(static_cast<int>(view->pixelHeight), ToWin(view->contentHeight, scale));
    bar.scrollX = ToWin(view->scrollX, scale);
    bar.contentWidth = std::max(static_cast<int>(view->pixelWidth), ToWin(view->contentWidth, scale));
    bar.pixelWidth = static_cast<int>(view->pixelWidth);
    bar.pixelHeight = static_cast<int>(view->pixelHeight);
    bar.dpi = view->baseDpi;
    bar.light = view->light;
}

void KeepScroll(HostView* view, const WindowScrollState& bar) {
    const double scale = PageScale(view);
    const int layoutH = std::max(1, ToDoc(static_cast<int>(view->pixelHeight), scale));
    int next = ToDoc(bar.scrollY, scale);
    const int maxScroll = std::max(0, view->contentHeight - layoutH);
    if (next < 0) {
        next = 0;
    }
    if (next > maxScroll) {
        next = maxScroll;
    }
    view->scrollY = next;
}

void KeepScrollX(HostView* view, const WindowScrollState& bar) {
    const double scale = PageScale(view);
    const int layoutW = std::max(1, ToDoc(static_cast<int>(view->pixelWidth), scale));
    int next = ToDoc(bar.scrollX, scale);
    const int maxScroll = std::max(0, view->contentWidth - layoutW);
    if (next < 0) {
        next = 0;
    }
    if (next > maxScroll) {
        next = maxScroll;
    }
    view->scrollX = next;
}

void RevealBar(HostView* view, bool horizontal) {
    if (view == nullptr || view->window == nullptr) {
        return;
    }
    if (horizontal) {
        view->bar.opacityX = 255;
        view->bar.idleX = GetTickCount();
    } else {
        view->bar.opacity = 255;
        view->bar.idle = GetTickCount();
    }
    SetTimer(view->window, 2, 50, nullptr);
}

bool HitDocControl(const WindowControl& control, int x, int y, int scrollX, int scrollY, double scale) {
    const int docX = scrollX + ToDoc(x, scale);
    const int docY = scrollY + ToDoc(y, scale);
    return docX >= control.x && docX < control.x + control.width && docY >= control.y && docY < control.y + control.height;
}

void BindSheet(HostView* view, bool sheet) {
    if (sheet && view->sheet != nullptr) {
        view->gpu.back = view->sheet;
        view->gpu.info.width = view->sheetWidth;
        view->gpu.info.height = view->sheetHeight;
        view->gpu.info.pixelsPerScanLine = view->sheetWidth;
        view->gpu.backSize = static_cast<RoboU64>(view->sheetWidth) * view->sheetHeight * 4u;
    } else {
        view->gpu.back = view->back;
        view->gpu.info.width = view->pixelWidth;
        view->gpu.info.height = view->pixelHeight;
        view->gpu.info.pixelsPerScanLine = view->pixelWidth;
        view->gpu.backSize = static_cast<RoboU64>(view->pixelWidth) * view->pixelHeight * 4u;
    }
    RoboDamageClear(&view->gpu.damage);
}

bool EnsureSheet(HostView* view, int contentWidth, int contentHeight, bool& fresh) {
    fresh = false;
    const UINT width = contentWidth > static_cast<int>(view->pixelWidth) ? static_cast<UINT>(contentWidth) : view->pixelWidth;
    const UINT height = contentHeight > static_cast<int>(view->pixelHeight) ? static_cast<UINT>(contentHeight) : view->pixelHeight;
    if (height == 0 || width == 0) {
        return false;
    }
    if (view->sheet != nullptr && view->sheetWidth == width && view->sheetHeight == height) {
        return true;
    }
    const RoboU64 bytes = static_cast<RoboU64>(width) * height * 4u;
    if (bytes > 64ull * 1024ull * 1024ull) {
        return false;
    }
    auto* memory = static_cast<RoboU8*>(_aligned_malloc(static_cast<size_t>(bytes), 16));
    if (memory == nullptr) {
        return false;
    }
    std::memset(memory, 0, static_cast<size_t>(bytes));
    _aligned_free(view->sheet);
    view->sheet = memory;
    view->sheetWidth = width;
    view->sheetHeight = height;
    fresh = true;
    return true;
}

void CopyVisible(HostView* view, int x, int y, int width, int height) {
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
    const int limitW = static_cast<int>(view->pixelWidth);
    const int limitH = static_cast<int>(view->pixelHeight);
    if (x + width > limitW) {
        width = limitW - x;
    }
    if (y + height > limitH) {
        height = limitH - y;
    }
    if (width <= 0 || height <= 0) {
        return;
    }
    const UINT pitch = view->pixelWidth * 4u;
    const int sheetRow = view->scrollY + y;
    if (sheetRow < 0 || sheetRow + height > static_cast<int>(view->sheetHeight)) {
        return;
    }
    for (int row = 0; row < height; ++row) {
        std::memcpy(view->back + (static_cast<size_t>(y + row) * pitch) + static_cast<size_t>(x) * 4u,
                    view->sheet + (static_cast<size_t>(sheetRow + row) * pitch) + static_cast<size_t>(x) * 4u,
                    static_cast<size_t>(width) * 4u);
    }
    RoboDamageAdd(&view->gpu.damage, x, y, width, height);
}

void ComposeSheet(HostView* view, const WindowPatch* patches, int patchCount) {
    const int height = static_cast<int>(view->pixelHeight);
    const int width = static_cast<int>(view->pixelWidth);
    const UINT pitch = view->pixelWidth * 4u;
    const int shift = view->shownScroll < 0 ? height : view->scrollY - view->shownScroll;
    const int distance = shift < 0 ? -shift : shift;
    const bool full = view->shownScroll < 0 || distance >= height;
    if (full) {
        const int limit = static_cast<int>(view->sheetHeight) - height;
        int y = view->scrollY;
        if (y < 0) {
            y = 0;
        }
        if (y > limit) {
            y = limit > 0 ? limit : 0;
        }
        std::memcpy(view->back, view->sheet + static_cast<size_t>(y) * pitch, static_cast<size_t>(pitch) * view->pixelHeight);
        view->shownScroll = view->scrollY;
        view->gpuFull = true;
        RoboGPUMarkFullDamage(&view->gpu);
    } else if (view->scrollY != view->shownScroll) {
        if (shift > 0) {
            std::memmove(view->back, view->back + static_cast<size_t>(shift) * pitch,
                         static_cast<size_t>(height - shift) * pitch);
            CopyVisible(view, 0, height - shift, width, shift);
        } else {
            const int reveal = -shift;
            std::memmove(view->back + static_cast<size_t>(reveal) * pitch, view->back,
                         static_cast<size_t>(height - reveal) * pitch);
            CopyVisible(view, 0, 0, width, reveal);
        }
        view->shownScroll = view->scrollY;
    }
    if (!full && patches != nullptr && patchCount > 0) {
        for (int index = 0; index < patchCount; ++index) {
            CopyVisible(view, patches[index].x, patches[index].y - view->scrollY, patches[index].width, patches[index].height);
        }
    }
}

bool EnsureViewport(HostView* view, int width, int height, bool& fresh) {
    fresh = false;
    if (width <= 0 || height <= 0) {
        return false;
    }
    const UINT sheetW = static_cast<UINT>(width);
    const UINT sheetH = static_cast<UINT>(height);
    if (view->sheet != nullptr && view->sheetWidth == sheetW && view->sheetHeight == sheetH) {
        return true;
    }
    const RoboU64 bytes = static_cast<RoboU64>(sheetW) * sheetH * 4u;
    if (bytes > 64ull * 1024ull * 1024ull) {
        return false;
    }
    auto* memory = static_cast<RoboU8*>(_aligned_malloc(static_cast<size_t>(bytes), 16));
    if (memory == nullptr) {
        return false;
    }
    std::memset(memory, 0, static_cast<size_t>(bytes));
    _aligned_free(view->sheet);
    view->sheet = memory;
    view->sheetWidth = sheetW;
    view->sheetHeight = sheetH;
    fresh = true;
    return true;
}

void ComposeZoomed(HostView* view, double scale, bool viewport) {
    const int dstW = static_cast<int>(view->pixelWidth);
    const int dstH = static_cast<int>(view->pixelHeight);
    const int srcW = static_cast<int>(view->sheetWidth);
    const int srcH = static_cast<int>(view->sheetHeight);
    const UINT srcPitch = view->sheetWidth * 4u;
    const UINT dstPitch = view->pixelWidth * 4u;
    const int scroll = viewport ? 0 : view->scrollY;
    for (int y = 0; y < dstH; ++y) {
        int srcY = scroll + static_cast<int>(static_cast<double>(y) / scale);
        if (srcY < 0) {
            srcY = 0;
        }
        if (srcY >= srcH) {
            srcY = srcH - 1;
        }
        const auto* src = reinterpret_cast<const RoboU32*>(view->sheet + static_cast<size_t>(srcY) * srcPitch);
        auto* dst = reinterpret_cast<RoboU32*>(view->back + static_cast<size_t>(y) * dstPitch);
        for (int x = 0; x < dstW; ++x) {
            int srcX = static_cast<int>(static_cast<double>(x) / scale);
            if (srcX < 0) {
                srcX = 0;
            }
            if (srcX >= srcW) {
                srcX = srcW - 1;
            }
            dst[x] = src[srcX];
        }
    }
    if (!viewport) {
        view->shownScroll = -1;
    }
    view->gpuScroll = -1;
    view->gpuFull = true;
    RoboGPUMarkFullDamage(&view->gpu);
}

bool ClampRect(const HostView* view, RECT* rect) {
    if (rect == nullptr || view == nullptr) {
        return false;
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
    return rect->right > rect->left && rect->bottom > rect->top;
}

void UploadBox(HostView* view, ID3D11Texture2D* texture, LONG left, LONG top, LONG right, LONG bottom) {
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
    const auto* source = view->back + static_cast<size_t>(box.top) * pitch + static_cast<size_t>(box.left) * 4u;
    view->context->UpdateSubresource(texture, 0, &box, source, pitch, 0);
}

void CopyBox(HostView* view, ID3D11Texture2D* target, LONG left, LONG top, LONG right, LONG bottom) {
    RECT rect{left, top, right, bottom};
    if (target == nullptr || view->upload == nullptr || !ClampRect(view, &rect)) {
        return;
    }
    left = rect.left;
    top = rect.top;
    right = rect.right;
    bottom = rect.bottom;
    D3D11_BOX box{};
    box.left = static_cast<UINT>(left);
    box.top = static_cast<UINT>(top);
    box.right = static_cast<UINT>(right);
    box.bottom = static_cast<UINT>(bottom);
    box.back = 1;
    view->context->CopySubresourceRegion(target, 0, box.left, box.top, 0, view->upload, 0, &box);
}

void ScrollUpload(HostView* view, ID3D11Texture2D* target, int shift) {
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

bool PresentNow(HostView* view, const RECT* rects, UINT count) {
    HRESULT presented = E_FAIL;
    if (view->flip && count > 0 && count <= 16) {
        RECT dirty[16];
        for (UINT i = 0; i < count; ++i) dirty[i] = rects[i];
        DXGI_PRESENT_PARAMETERS params{};
        params.DirtyRectsCount = count;
        params.pDirtyRects = dirty;
        presented = view->swap->Present1(0, DXGI_PRESENT_DO_NOT_WAIT, &params);
        if (presented == DXGI_ERROR_INVALID_CALL) {
            presented = view->swap->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
        }
    } else {
        presented = view->swap->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
    }
    if (presented == DXGI_ERROR_WAS_STILL_DRAWING || presented == DXGI_STATUS_OCCLUDED) {
        return false;
    }
    if (FAILED(presented)) {
        presented = view->swap->Present(0, 0);
        if (presented == DXGI_ERROR_WAS_STILL_DRAWING || presented == DXGI_STATUS_OCCLUDED || FAILED(presented)) {
            return false;
        }
    }
    view->presentIndex ^= 1;
    RoboDamageClear(&view->gpu.damage);
    view->shownScroll = view->scrollY;
    return true;
}

bool Publish(HostView* view) {
    if (view == nullptr || view->context == nullptr || view->swap == nullptr || view->back == nullptr) {
        return true;
    }
    if (view->uploadBusy && view->uploadQuery != nullptr) {
        if (view->context->GetData(view->uploadQuery, nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
            view->context->Flush();
            return false;
        }
        view->uploadBusy = false;
    }
    if (RoboDamageIsEmpty(&view->gpu.damage)) {
        return true;
    }
    const bool full = view->gpu.damage.full == ROBO_TRUE;
    RECT dirty[24]{};
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
    if (FAILED(view->swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&target))) || target == nullptr) {
        return false;
    }
    const UINT pitch = view->pixelWidth * 4u;
    const int gpuShift = view->gpuScroll < 0 ? 0 : view->scrollY - view->gpuScroll;
    const bool whole = dirtyCount == 1 && dirty[0].left == 0 && dirty[0].top == 0 &&
                       dirty[0].right == static_cast<LONG>(view->pixelWidth) &&
                       dirty[0].bottom == static_cast<LONG>(view->pixelHeight);
    const bool uploadAll = full || view->gpuFull || view->gpuScroll < 0 || whole;
    const bool shiftTooFar = gpuShift != 0 && (gpuShift >= static_cast<int>(view->pixelHeight) ||
                                               -gpuShift >= static_cast<int>(view->pixelHeight));
    if (uploadAll || shiftTooFar) {
        if (view->upload != nullptr) {
            view->context->UpdateSubresource(view->upload, 0, nullptr, view->back, pitch, 0);
            view->context->CopyResource(target, view->upload);
        } else {
            view->context->UpdateSubresource(target, 0, nullptr, view->back, pitch, 0);
        }
        view->gpuFull = false;
        view->gpuScroll = view->scrollY;
        view->bufferReady[0] = false;
        view->bufferReady[1] = false;
        view->bufferReady[view->presentIndex & 1] = true;
        view->recentCount[0] = 0;
        view->recentCount[1] = 0;
        target->Release();
        if (view->uploadQuery != nullptr) {
            view->context->End(view->uploadQuery);
            view->uploadBusy = true;
        }
        return PresentNow(view, nullptr, 0);
    }
    if (gpuShift != 0 && view->upload != nullptr) {
        ScrollUpload(view, target, gpuShift);
        for (UINT index = 0; index < dirtyCount; ++index) {
            UploadBox(view, view->upload, dirty[index].left, dirty[index].top, dirty[index].right, dirty[index].bottom);
        }
        view->context->CopyResource(target, view->upload);
        view->gpuScroll = view->scrollY;
        view->bufferReady[0] = false;
        view->bufferReady[1] = false;
        view->bufferReady[view->presentIndex & 1] = true;
        view->recentCount[0] = 0;
        view->recentCount[1] = 0;
        target->Release();
        if (view->uploadQuery != nullptr) {
            view->context->End(view->uploadQuery);
            view->uploadBusy = true;
        }
        return PresentNow(view, nullptr, 0);
    }
    bool heavy = false;
    if (view->upload != nullptr && dirtyCount > 16) {
        for (UINT index = 0; index < dirtyCount; ++index) {
            UploadBox(view, view->upload, dirty[index].left, dirty[index].top, dirty[index].right, dirty[index].bottom);
        }
        view->context->CopyResource(target, view->upload);
        view->bufferReady[0] = false;
        view->bufferReady[1] = false;
        view->bufferReady[view->presentIndex & 1] = true;
        view->recentCount[0] = 0;
        view->recentCount[1] = 0;
        heavy = true;
    } else if (view->upload != nullptr) {
        const int slot = view->presentIndex & 1;
        const int older = slot ^ 1;
        view->recentCount[slot] = 0;
        for (UINT index = 0; index < dirtyCount && view->recentCount[slot] < 16; ++index) {
            view->recent[slot][view->recentCount[slot]++] = dirty[index];
        }
        if (!view->bufferReady[slot]) {
            view->context->CopyResource(target, view->upload);
            view->bufferReady[slot] = true;
            heavy = true;
        }
        for (int index = 0; index < view->recentCount[slot]; ++index) {
            const RECT& rect = view->recent[slot][index];
            UploadBox(view, view->upload, rect.left, rect.top, rect.right, rect.bottom);
        }
        for (int generation = 0; generation < 2; ++generation) {
            const int which = generation == 0 ? older : slot;
            for (int index = 0; index < view->recentCount[which]; ++index) {
                const RECT& rect = view->recent[which][index];
                CopyBox(view, target, rect.left, rect.top, rect.right, rect.bottom);
            }
        }
    } else {
        view->context->UpdateSubresource(target, 0, nullptr, view->back, pitch, 0);
        heavy = true;
    }
    target->Release();
    if (heavy && view->uploadQuery != nullptr) {
        view->context->End(view->uploadQuery);
        view->uploadBusy = true;
    }
    RECT presentRects[16]{};
    UINT presentCount = 0;
    if (dirtyCount > 0 && dirtyCount <= 16) {
        for (UINT index = 0; index < dirtyCount; ++index) {
            RECT rect = dirty[index];
            if (ClampRect(view, &rect)) {
                presentRects[presentCount++] = rect;
            }
        }
    }
    return PresentNow(view, presentRects, presentCount);
}

void Request(HostView* view) {
    if (view != nullptr && view->window != nullptr) {
        InvalidateRect(view->window, nullptr, FALSE);
    }
}

RoboU32 ZoomColor(const char* name, bool light) {
    return RoboFoundationPlatformColorForTheme(name, light ? ROBO_TRUE : ROBO_FALSE);
}

void ZoomText(RoboGPU* gpu, int x, int y, int width, int height, const char* text, RoboU32 color, int size, bool center) {
    if (gpu == nullptr || text == nullptr || text[0] == '\0' || size <= 0) {
        return;
    }
    RoboFontHeader header{};
    header.magic = ROBO_FONT_MAGIC;
    header.ascent = static_cast<RoboU16>(size);
    header.firstCodepoint = 1;
    RoboFontView font{};
    font.header = &header;
    const int measured = RoboTextMeasureUTF8(font, text, static_cast<RoboU16>(size));
    const int mid = y + height / 2;
    const int baseline = mid + size * 745 / 2048;
    const int left = center ? x + std::max(0, (width - measured) / 2) : x;
    RoboTextDrawUTF8(gpu, font, left, baseline - size, text, color, static_cast<RoboU16>(size));
}

void BlendRound(RoboGPU* gpu, int x, int y, int width, int height, int radius, RoboU32 color, int coverage) {
    if (gpu == nullptr || coverage <= 0 || width <= 0 || height <= 0) {
        return;
    }
    if (radius < 0) {
        radius = 0;
    }
    if (radius > std::min(width, height) / 2) {
        radius = std::min(width, height) / 2;
    }
    const int alpha = static_cast<int>((color >> 24) & 0xFF);
    const int scaled = coverage > 255 ? 255 : coverage;
    const int fade = alpha == 0 ? 0 : scaled * alpha / 255;
    if (fade <= 0) {
        return;
    }
    for (int row = 0; row < height; ++row) {
        int inset = 0;
        if (row < radius || row >= height - radius) {
            const int dy = row < radius ? radius - 1 - row : row - (height - radius);
            int inside = 0;
            while (inside * inside + dy * dy <= radius * radius) {
                ++inside;
            }
            inset = radius - std::max(0, inside - 1);
        }
        const int span = width - inset * 2;
        if (span <= 0) {
            continue;
        }
        if (fade >= 255) {
            RoboGPUFillRect(gpu, x + inset, y + row, span, 1, color);
        } else {
            for (int column = 0; column < span; ++column) {
                RoboGPUBlendPixel(gpu, x + inset + column, y + row, color, static_cast<RoboU8>(fade));
            }
        }
    }
}

void BlendCapsule(RoboGPU* gpu, int x, int y, int width, int height, RoboU32 color, int coverage) {
    BlendRound(gpu, x, y, width, height, std::min(width, height) / 2, color, coverage);
}

RoboU32 MixColor(RoboU32 ink, RoboU32 fill, int coverage) {
    if (coverage >= 255) {
        return ink;
    }
    if (coverage <= 0) {
        return fill;
    }
    const auto channel = [&](int shift) {
        const int from = static_cast<int>((fill >> shift) & 0xFF);
        const int to = static_cast<int>((ink >> shift) & 0xFF);
        return from + (to - from) * coverage / 255;
    };
    return (static_cast<RoboU32>(channel(16)) << 16) | (static_cast<RoboU32>(channel(8)) << 8) | static_cast<RoboU32>(channel(0));
}

void RevealZoom(HostView* view) {
    if (view == nullptr || view->window == nullptr || WindowsDocumentZoomAnchor() < 0) {
        return;
    }
    view->zoomOpacity = 255;
    view->zoomIdle = GetTickCount();
    SetTimer(view->window, 5, 16, nullptr);
}

int ZoomHit(HostView* view, int x, int y) {
    if (view == nullptr || view->zoomOpacity < 32 || WindowsDocumentZoomAnchor() < 0) {
        return 0;
    }
    view->zoom.Place(static_cast<int>(view->pixelWidth), static_cast<int>(view->pixelHeight), view->baseDpi,
                     WindowsDocumentZoomAnchor(), view->zoomBar);
    return view->zoom.Hit(view->zoomBar, x, y, view->zoomMenu);
}

void ClearZoomBox(HostView* view) {
    if (view == nullptr || view->zoomBoxH <= 0) {
        return;
    }
    CopyVisible(view, view->zoomBoxX, view->zoomBoxY, view->zoomBoxW, view->zoomBoxH);
    view->zoomBoxH = 0;
}

bool ZoomPartEnabled(const HostView* view, int part) {
    if (view == nullptr) {
        return false;
    }
    if (part == 1) {
        return view->zoom.scale > ViewZoom::kMin + 0.0005;
    }
    if (part == 4) {
        return view->zoom.scale < ViewZoom::kMax - 0.0005;
    }
    if (part == 3 || part == 2 || part >= 10) {
        return true;
    }
    return false;
}

void PaintZoomIcon(HostView* view, int x, int y, int size, const std::string& svg, RoboU32 color) {
    if (svg.empty() || size <= 0) {
        return;
    }
    PaintWindowsSvg(&view->gpu, x, y, size, svg, color);
}

void PaintZoomBar(HostView* view) {
    if (view == nullptr || view->back == nullptr || view->zoomOpacity <= 0 || WindowsDocumentZoomAnchor() < 0) {
        return;
    }
    static std::string minus;
    static std::string plus;
    static std::string chevron;
    static std::string check;
    if (minus.empty()) {
        minus = WindowsDocumentIconSvg("SubtractMinus");
    }
    if (plus.empty()) {
        plus = WindowsDocumentIconSvg("AddPlusCreate");
    }
    if (chevron.empty()) {
        chevron = WindowsDocumentIconSvg("ChevronDown");
    }
    if (check.empty()) {
        check = WindowsDocumentIconSvg("TaskCompleteCheckmark");
    }
    view->zoom.Place(static_cast<int>(view->pixelWidth), static_cast<int>(view->pixelHeight), view->baseDpi,
                     WindowsDocumentZoomAnchor(), view->zoomBar);
    const ViewZoomBar& bar = view->zoomBar;
    const bool light = view->light;
    const int fade = view->zoomOpacity;
    const int dpi = static_cast<int>(view->baseDpi == 0 ? 96u : view->baseDpi);
    const auto px = [&](int value) { return std::max(1, value * dpi / 96); };
    const RoboU32 fill = ZoomColor("bg-elevated", light);
    const RoboU32 line = ZoomColor("border-default", light);
    const RoboU32 quiet = MixColor(ZoomColor("text-secondary", light), fill, fade);
    const RoboU32 valueInk = MixColor(ZoomColor("text-primary", light), fill, fade);
    const RoboU32 disabled = MixColor(ZoomColor("text-disabled", light), fill, fade);
    const RoboU32 hoverFill = ZoomColor("bg-tertiary", light);
    const RoboU32 pressFill = ZoomColor("bg-active", light);
    const RoboU32 iconHover = ZoomColor("brand-primary-subtle", light);
    const RoboU32 iconPress = ZoomColor("brand-primary-range", light);
    const RoboU32 brand = ZoomColor("brand-primary", light);
    const int type = std::max(12, 15 * dpi / 96);
    const int icon = px(16);
    const int shadow = px(10);
    BlendCapsule(&view->gpu, bar.x, bar.y + px(4), bar.width, bar.height, 0xFF000000, fade * 22 / 255);
    BlendCapsule(&view->gpu, bar.x, bar.y, bar.width, bar.height, line, fade);
    if (bar.width > 2 && bar.height > 2) {
        BlendCapsule(&view->gpu, bar.x + 1, bar.y + 1, bar.width - 2, bar.height - 2, fill, fade);
    }
    const auto ink = [&](int part) {
        if (!ZoomPartEnabled(view, part)) {
            return disabled;
        }
        return part == 2 || part == 3 ? valueInk : quiet;
    };
    const auto hover = [&](int part, int x, int width) {
        if (!ZoomPartEnabled(view, part)) {
            return;
        }
        const RoboU32 tone = view->zoom.pressed == part ? iconPress : view->zoom.hot == part ? iconHover : 0;
        if (tone != 0) {
            BlendCapsule(&view->gpu, x + px(2), bar.y + px(4), std::max(1, width - px(4)), bar.height - px(8), tone, fade);
        }
    };
    hover(1, bar.minusX, bar.stepW);
    hover(2, bar.selectX, bar.selectW);
    hover(4, bar.plusX, bar.stepW);
    const int resetY = bar.y + px(4);
    const int resetH = bar.height - px(8);
    RoboU32 resetFill = hoverFill;
    if (view->zoom.pressed == 3) {
        resetFill = pressFill;
    } else if (view->zoom.hot == 3) {
        resetFill = ZoomColor("bg-hover", light);
    }
    BlendCapsule(&view->gpu, bar.resetX + px(2), resetY, std::max(1, bar.resetW - px(4)), resetH, resetFill, fade);
    const auto rule = [&](int x) {
        RoboGPUFillRect(&view->gpu, x, bar.y + (bar.height - px(18)) / 2, 1, px(18), line);
    };
    rule(bar.selectX);
    rule(bar.plusX);
    rule(bar.resetX);
    PaintZoomIcon(view, bar.minusX + (bar.stepW - icon) / 2, bar.y + (bar.height - icon) / 2, icon, minus, ink(1));
    char percent[16];
    std::snprintf(percent, sizeof(percent), "%d%%", view->zoom.Percent());
    ZoomText(&view->gpu, bar.selectX + px(14), bar.y, bar.selectW - px(36), bar.height, percent, ink(2), type, false);
    PaintZoomIcon(view, bar.selectX + bar.selectW - px(12) - icon, bar.y + (bar.height - icon) / 2, icon, chevron, quiet);
    PaintZoomIcon(view, bar.plusX + (bar.stepW - icon) / 2, bar.y + (bar.height - icon) / 2, icon, plus, ink(4));
    ZoomText(&view->gpu, bar.resetX, resetY, bar.resetW, resetH, "Reset", valueInk, type, true);
    int boxX = std::max(0, bar.x - px(2));
    int boxY = std::max(0, bar.y - px(2));
    int boxR = bar.x + bar.width + shadow;
    int boxB = bar.y + bar.height + shadow;
    if (view->zoomMenu) {
        const int radius = px(14);
        const RoboU32 menuFill = light ? ZoomColor("bg-elevated", true) : ZoomColor("bg-secondary", false);
        const RoboU32 rowHover = ZoomColor("bg-hover", light);
        const RoboU32 rowPress = ZoomColor("bg-active", light);
        BlendRound(&view->gpu, bar.menuX, bar.menuY + px(4), bar.menuW, bar.menuH, radius, 0xFF000000, fade * 28 / 255);
        BlendRound(&view->gpu, bar.menuX, bar.menuY, bar.menuW, bar.menuH, radius, line, fade);
        BlendRound(&view->gpu, bar.menuX + 1, bar.menuY + 1, bar.menuW - 2, bar.menuH - 2, std::max(1, radius - 1), menuFill, fade);
        const int pad = (bar.menuH - bar.optionH * ViewZoom::kPresetCount) / 2;
        const int current = view->zoom.Percent();
        int chosen = 0;
        int nearest = 100000;
        for (int index = 0; index < ViewZoom::kPresetCount; ++index) {
            const int distance = std::abs(ViewZoom::kPresets[index] - current);
            if (distance < nearest) {
                nearest = distance;
                chosen = index;
            }
        }
        for (int index = 0; index < ViewZoom::kPresetCount; ++index) {
            const int rowY = bar.menuY + pad + index * bar.optionH;
            const int part = 10 + index;
            const bool on = index == chosen;
            const bool pressed = view->zoom.pressed == part;
            const bool hot = view->zoom.hot == part;
            if (pressed || hot) {
                BlendRound(&view->gpu, bar.menuX + px(6), rowY + px(2), bar.menuW - px(12), bar.optionH - px(4), px(8),
                           pressed ? rowPress : rowHover, fade);
            }
            const RoboU32 rowInk = on ? brand : valueInk;
            const int textX = bar.menuX + px(14);
            char label[16];
            std::snprintf(label, sizeof(label), "%d%%", ViewZoom::kPresets[index]);
            ZoomText(&view->gpu, textX, rowY, bar.menuW - px(14) - (on ? icon + px(20) : px(14)), bar.optionH, label, rowInk, type,
                     false);
            if (on) {
                PaintZoomIcon(view, bar.menuX + bar.menuW - px(12) - icon, rowY + (bar.optionH - icon) / 2, icon, check, brand);
            }
        }
        boxX = std::min(boxX, std::max(0, bar.menuX - px(4)));
        boxY = std::min(boxY, std::max(0, bar.menuY - px(4)));
        boxR = std::max(boxR, bar.menuX + bar.menuW + shadow);
        boxB = std::max(boxB, bar.menuY + bar.menuH + shadow);
    }
    view->zoomBoxX = boxX;
    view->zoomBoxY = boxY;
    view->zoomBoxW = std::max(1, boxR - boxX);
    view->zoomBoxH = std::max(1, boxB - boxY);
    RoboGPUMarkDamage(&view->gpu, view->zoomBoxX, view->zoomBoxY, view->zoomBoxW, view->zoomBoxH);
}

void CatchUpSpin(HostView* view) {
    if (view == nullptr || !WindowsDocumentNeedsSpin()) {
        return;
    }
    const unsigned now = GetTickCount();
    if (view->spinClock == 0) {
        view->spinClock = now;
        return;
    }
    const unsigned elapsed = now - view->spinClock;
    int steps = static_cast<int>(elapsed / 50u);
    if (steps <= 0) {
        return;
    }
    if (steps > 3) {
        steps = 3;
    }
    for (int index = 0; index < steps; ++index) {
        WindowsDocumentAdvanceSpin();
    }
    view->spinClock += static_cast<unsigned>(steps) * 50u;
    if (now - view->spinClock > 200u) {
        view->spinClock = now;
    }
}

void Draw(HostView* view) {
    if (view == nullptr || !view->ready || view->back == nullptr) {
        return;
    }
    const double scale = PageScale(view);
    const bool zoomed = std::abs(scale - 1.0) > 0.0005;
    if (view->screen) {
        CatchUpSpin(view);
    }
    view->dpi = view->baseDpi;
    BindSheet(view, false);
    if (view->screen && !view->document.empty()) {
        const int height = static_cast<int>(view->pixelHeight);
        const int width = static_cast<int>(view->pixelWidth);
        const int layoutW = std::max(1, ToDoc(width, scale));
        const int layoutH = std::max(1, ToDoc(height, scale));
        const int previousScroll = view->shownScroll;
        const int previousScrollX = view->shownScrollX;
        const int pointerDocX = view->pointerX < 0 ? -1 : view->scrollX + ToDoc(view->pointerX, scale);
        const int pointerDocY = view->pointerY < 0 ? -1 : view->scrollY + ToDoc(view->pointerY, scale);
        const int layout = LayoutWindowsDocument(view->document, view->light, layoutW, layoutH, view->dpi, view->hotControl,
                                                 view->pressedControl, pointerDocX, pointerDocY, &view->contentHeight,
                                                 &view->controls);
        const bool rebuilt = layout > 0;
        const int maxScroll = std::max(0, view->contentHeight - layoutH);
        if (view->scrollY > maxScroll) {
            view->scrollY = maxScroll;
        }
        if (view->scrollY < 0) {
            view->scrollY = 0;
        }
        view->contentWidth = WindowsDocumentContentWidth();
        const int maxScrollX = std::max(0, view->contentWidth - layoutW);
        if (view->scrollX > maxScrollX) {
            view->scrollX = maxScrollX;
        }
        if (view->scrollX < 0) {
            view->scrollX = 0;
        }
        const bool wide = view->contentWidth > layoutW;
        WindowPatch patches[32]{};
        int patchCount = CollectWindowsDocumentPatches(view->hotControl, view->pressedControl, view->focusControl, patches, 32);
        bool fresh = false;
        bool usedSheet = false;
        if (zoomed) {
            usedSheet = EnsureViewport(view, width, height, fresh);
            if (usedSheet) {
                const bool dirty = rebuilt || fresh || patchCount != 0 || view->shownScroll != view->scrollY ||
                                   view->shownScrollX != view->scrollX || WindowsDocumentNeedsSpin();
                if (dirty) {
                    BindSheet(view, true);
                    WindowsDocumentSetScrollX(view->scrollX);
                    PaintWindowsDocumentScaled(&view->gpu, view->scrollY, width, height, scale);
                    BindSheet(view, false);
                    view->shownScroll = view->scrollY;
                    view->shownScrollX = view->scrollX;
                }
                ComposeZoomed(view, 1.0, true);
            }
        } else if (wide) {
            WindowsDocumentSetScrollX(view->scrollX);
            BindSheet(view, false);
            PaintWindowsDocumentView(&view->gpu, view->scrollY, width, height);
            view->shownScroll = view->scrollY;
            view->shownScrollX = view->scrollX;
            view->gpuFull = true;
        } else {
            WindowsDocumentSetScrollX(0);
            usedSheet = layout >= 0 && EnsureSheet(view, std::max(width, layoutW), view->contentHeight, fresh);
            if (usedSheet) {
                if (rebuilt || fresh || patchCount < 0) {
                    BindSheet(view, true);
                    RasterWindowsDocument(&view->gpu);
                    BindSheet(view, false);
                    view->shownScroll = -1;
                    view->gpuScroll = -1;
                    view->gpuFull = true;
                    patchCount = 0;
                } else if (WindowsDocumentNeedsSpin()) {
                    WindowPatch spinRects[32]{};
                    const int spinCount = WindowsDocumentSpinRects(spinRects, 32);
                    for (int index = 0; index < spinCount && patchCount < 32; ++index) {
                        patches[patchCount] = spinRects[index];
                        ++patchCount;
                    }
                }
                if (patchCount > 0) {
                    BindSheet(view, true);
                    for (int index = 0; index < patchCount; ++index) {
                        PaintWindowsDocumentRect(&view->gpu, patches[index].x, patches[index].y, patches[index].width,
                                                 patches[index].height);
                    }
                    BindSheet(view, false);
                }
                if (view->zoomBoxH > 0) {
                    ClearZoomBox(view);
                }
                ComposeSheet(view, patches, patchCount);
            }
        }
        if (!usedSheet) {
            PaintWindowsDocumentView(&view->gpu, view->scrollY, layoutW, layoutH);
        }
        WindowScrollState bar = view->bar;
        bar.scrollY = ToWin(view->scrollY, scale);
        bar.contentHeight = std::max(height, ToWin(view->contentHeight, scale));
        bar.scrollX = ToWin(view->scrollX, scale);
        bar.contentWidth = std::max(width, ToWin(view->contentWidth, scale));
        bar.pixelWidth = width;
        bar.pixelHeight = height;
        bar.dpi = view->dpi;
        bar.light = view->light;
        const bool barChanged = bar.opacity != view->shownOpacity || bar.part != view->shownPart || bar.hold != view->shownHold ||
                                bar.drag != view->shownDrag || bar.hot != view->shownHot || previousScroll != view->scrollY;
        const bool barChangedX = bar.opacityX != view->shownOpacityX || bar.partX != view->shownPartX || bar.holdX != view->shownHoldX ||
                                 bar.dragX != view->shownDragX || bar.hotX != view->shownHotX || previousScrollX != view->scrollX;
        if (!zoomed && view->gpu.damage.full != ROBO_TRUE) {
            if (barChanged) {
                int barX = 0;
                int barY = 0;
                int barW = 0;
                int barH = 0;
                WindowScrollBarRect(bar, barX, barY, barW, barH);
                if (usedSheet) {
                    CopyVisible(view, barX, barY, barW, barH);
                } else {
                    RestoreWindowsDocumentRect(&view->gpu, barX, barY, barW, barH);
                }
            }
            if (barChangedX) {
                int barX = 0;
                int barY = 0;
                int barW = 0;
                int barH = 0;
                WindowScrollBarXRect(bar, barX, barY, barW, barH);
                if (usedSheet) {
                    CopyVisible(view, barX, barY, barW, barH);
                } else {
                    RestoreWindowsDocumentRect(&view->gpu, barX, barY, barW, barH);
                }
            }
        }
        if (view->gpu.damage.full == ROBO_TRUE || (bar.opacity > 0 && barChanged)) {
            PaintWindowScrollBar(&view->gpu, bar);
        }
        if (view->gpu.damage.full == ROBO_TRUE || (bar.opacityX > 0 && barChangedX)) {
            PaintWindowScrollBarX(&view->gpu, bar);
        }
        if (view->focusControl >= 0 && view->focusControl < static_cast<int>(view->controls.size())) {
            const WindowControl& control = view->controls[static_cast<std::size_t>(view->focusControl)];
            const int reach = std::max(4, 9 * static_cast<int>(view->dpi) / 96);
            const int ringX = ToWin(control.x - view->scrollX, scale) - reach;
            const int ringY = ToWin(control.y - view->scrollY, scale) - reach;
            const int ringW = std::max(1, ToWin(control.width, scale));
            const int ringH = std::max(1, ToWin(control.height, scale));
            if (usedSheet && !zoomed) {
                CopyVisible(view, ringX, ringY, ringW + reach * 2, ringH + reach * 2);
            }
            PaintWindowsFocusRing(&view->gpu, ToWin(control.x - view->scrollX, scale), ToWin(control.y - view->scrollY, scale), ringW, ringH);
        }
        if (view->screen) {
            PaintZoomBar(view);
        }
        view->shownOpacity = bar.opacity;
        view->shownPart = bar.part;
        view->shownHold = bar.hold;
        view->shownDrag = bar.drag;
        view->shownHot = bar.hot;
        view->shownOpacityX = bar.opacityX;
        view->shownPartX = bar.partX;
        view->shownHoldX = bar.holdX;
        view->shownDragX = bar.dragX;
        view->shownHotX = bar.hotX;
        if (!usedSheet) {
            view->shownScroll = view->scrollY;
        }
    } else {
        PaintFoundation(&view->gpu, view->light, !view->screen, view->hasScreenColor, view->screenColor);
    }
    if (!Publish(view)) {
        if (view->window != nullptr) {
            SetTimer(view->window, 3, 16, nullptr);
        }
        return;
    }
    if (view->window != nullptr) {
        KillTimer(view->window, 3);
        if (view->stayOpen && view->screen && WindowsDocumentNeedsSpin()) {
            if (!view->spinArmed) {
                view->spinClock = GetTickCount();
                SetTimer(view->window, 4, 50, nullptr);
                view->spinArmed = true;
            }
        } else if (view->spinArmed) {
            KillTimer(view->window, 4);
            view->spinArmed = false;
            view->spinClock = 0;
        }
    }
    view->painted = true;
    if (!view->frameNoted) {
        std::printf("framebuffer %ux%u\n", view->pixelWidth, view->pixelHeight);
        std::printf("renderer platform-framebuffer\n");
        view->frameNoted = true;
    }
    if (!view->stayOpen) {
        PostQuitMessage(0);
    }
}

void Release(HostView* view) {
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
    _aligned_free(view->sheet);
    _aligned_free(view->back);
    view->sheet = nullptr;
    view->back = nullptr;
}

void Resize(HostView* view, UINT width, UINT height) {
    if (view == nullptr || !view->ready || view->inResize || width == 0 || height == 0) {
        return;
    }
    if (view->upload != nullptr && view->pixelWidth == width && view->pixelHeight == height) {
        return;
    }
    view->inResize = true;
    if (EnsureSurface(view, width, height)) {
        Request(view);
    }
    view->inResize = false;
}

LRESULT CALLBACK Procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* view = reinterpret_cast<HostView*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
        case WM_NCCREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return DefWindowProcW(window, message, wParam, lParam);
        }
        case WM_DPICHANGED: {
            auto* suggested = reinterpret_cast<RECT*>(lParam);
            if (view != nullptr) {
                const UINT next = GetDpiForWindow(window);
                view->baseDpi = next == 0 ? 96u : next;
                Request(view);
            }
            SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            if (view != nullptr && view->ready && wParam != SIZE_MINIMIZED) {
                Resize(view, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            BeginPaint(window, &paint);
            if (view != nullptr && view->ready && (view->stayOpen || !view->painted)) {
                Draw(view);
            }
            EndPaint(window, &paint);
            return 0;
        }
        case WM_SETCURSOR:
            if (view != nullptr && view->ready && LOWORD(lParam) == HTCLIENT) {
                POINT cursor{};
                GetCursorPos(&cursor);
                ScreenToClient(window, &cursor);
                WindowScrollState bar{};
                FillBar(view, bar);
                const int barHit = HitWindowScrollBar(bar, cursor.x, cursor.y);
                const int barHitX = HitWindowScrollBarX(bar, cursor.x, cursor.y);
                bool onControl = false;
                bool controlEnabled = true;
                const double scale = PageScale(view);
                for (const WindowControl& control : view->controls) {
                    if (HitDocControl(control, cursor.x, cursor.y, view->scrollX, view->scrollY, scale)) {
                        onControl = true;
                        controlEnabled = control.enabled;
                        break;
                    }
                }
                const int zoomHit = ZoomHit(view, cursor.x, cursor.y);
                const wchar_t* cursorId = IDC_ARROW;
                if (zoomHit != 0 && ZoomPartEnabled(view, zoomHit)) {
                    cursorId = IDC_HAND;
                } else if (barHit == 0 && barHitX == 0 && onControl) {
                    cursorId = controlEnabled ? IDC_HAND : IDC_NO;
                }
                SetCursor(LoadCursorW(nullptr, cursorId));
                return TRUE;
            }
            return DefWindowProcW(window, message, wParam, lParam);
        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
            if (view != nullptr && view->ready && view->screen) {
                if (message == WM_MOUSEMOVE && !view->trackLeave) {
                    TRACKMOUSEEVENT track{};
                    track.cbSize = sizeof(track);
                    track.dwFlags = TME_LEAVE;
                    track.hwndTrack = window;
                    TrackMouseEvent(&track);
                    view->trackLeave = true;
                }
                const int x = message == WM_MOUSELEAVE ? -1 : static_cast<int>(static_cast<short>(LOWORD(lParam)));
                const int y = message == WM_MOUSELEAVE ? -1 : static_cast<int>(static_cast<short>(HIWORD(lParam)));
                WindowScrollState bar{};
                FillBar(view, bar);
                const int barHit = message == WM_MOUSELEAVE ? 0 : HitWindowScrollBar(bar, x, y);
                const int barHitX = message == WM_MOUSELEAVE ? 0 : HitWindowScrollBarX(bar, x, y);
                const int zoomHit = message == WM_MOUSELEAVE ? 0 : ZoomHit(view, x, y);
                if (message == WM_LBUTTONUP && view->zoom.pressed != 0) {
                    const int part = view->zoom.pressed;
                    view->zoom.pressed = 0;
                    if (GetCapture() == window) {
                        ReleaseCapture();
                    }
                    if (part == zoomHit && ZoomPartEnabled(view, part)) {
                        if (part == 1) {
                            view->zoom.Step(-1);
                            view->zoomMenu = false;
                        } else if (part == 2) {
                            view->zoomMenu = !view->zoomMenu;
                        } else if (part == 3) {
                            view->zoom.Step(0);
                            view->zoomMenu = false;
                        } else if (part == 4) {
                            view->zoom.Step(1);
                            view->zoomMenu = false;
                        } else if (part >= 10 && part < 10 + ViewZoom::kPresetCount) {
                            view->zoom.Apply(ViewZoom::kPresets[part - 10] / 100.0);
                            view->zoomMenu = false;
                        }
                        RevealZoom(view);
                    }
                    Request(view);
                    return 0;
                }
                if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && view->zoomMenu && zoomHit == 0) {
                    view->zoomMenu = false;
                    Request(view);
                    return 0;
                }
                if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && zoomHit != 0 &&
                    (ZoomPartEnabled(view, zoomHit) || zoomHit == 9)) {
                    SetFocus(window);
                    view->zoom.pressed = zoomHit;
                    view->zoom.hot = zoomHit;
                    view->hotControl = -1;
                    SetCapture(window);
                    Request(view);
                    return 0;
                }
                const double pointerScale = PageScale(view);
                const int docX = x < 0 ? -1 : view->scrollX + ToDoc(x, pointerScale);
                const int docY = y < 0 ? -1 : view->scrollY + ToDoc(y, pointerScale);
                if (view->regionDrag && message == WM_MOUSEMOVE) {
                    WindowsDocumentScrollDrag(docX);
                    Request(view);
                    return 0;
                }
                if (message == WM_LBUTTONUP && view->regionDrag) {
                    view->regionDrag = false;
                    WindowsDocumentScrollEnd();
                    if (GetCapture() == window) {
                        ReleaseCapture();
                    }
                    Request(view);
                    return 0;
                }
                if (view->bar.dragX && message == WM_MOUSEMOVE) {
                    WindowDragScrollBarX(bar, x);
                    KeepScrollX(view, bar);
                    view->bar.opacityX = 255;
                    view->bar.idleX = GetTickCount();
                    Request(view);
                    return 0;
                }
                if (message == WM_LBUTTONUP && (view->bar.dragX || view->bar.holdX != 0)) {
                    view->bar.dragX = false;
                    view->bar.holdX = 0;
                    view->bar.partX = barHitX == 3 ? 3 : 0;
                    view->bar.opacityX = 255;
                    view->bar.idleX = GetTickCount();
                    if (GetCapture() == window) {
                        ReleaseCapture();
                    }
                    Request(view);
                    return 0;
                }
                if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && x >= 0) {
                    const int nest = WindowsDocumentScrollHit(docX, docY);
                    if (nest != 0) {
                        SetFocus(window);
                        view->regionDrag = nest == 3;
                        SetCapture(window);
                        Request(view);
                        return 0;
                    }
                }
                if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && barHitX != 0) {
                    SetFocus(window);
                    view->bar.partX = barHitX == 4 ? 0 : barHitX;
                    view->bar.opacityX = 255;
                    view->bar.idleX = GetTickCount();
                    view->bar.hotX = true;
                    SetCapture(window);
                    SetTimer(window, 2, 50, nullptr);
                    if (barHitX == 3) {
                        view->bar.grabX = x - WindowScrollThumbX(bar);
                        view->bar.dragX = true;
                        view->bar.holdX = 0;
                    } else if (barHitX == 1 || barHitX == 2) {
                        view->bar.holdX = barHitX == 1 ? -1 : 1;
                        view->bar.repeatX = GetTickCount() + 70;
                        WindowScrollByX(bar, view->bar.holdX * WindowScrollScale(56, view->baseDpi));
                        KeepScrollX(view, bar);
                    } else {
                        WindowScrollToBarX(bar, x);
                        KeepScrollX(view, bar);
                    }
                    Request(view);
                    return 0;
                }
                if (view->bar.drag && message == WM_MOUSEMOVE) {
                    WindowDragScrollBar(bar, y);
                    KeepScroll(view, bar);
                    view->bar.opacity = 255;
                    view->bar.idle = GetTickCount();
                    Request(view);
                    return 0;
                }
                if (message == WM_LBUTTONUP && (view->bar.drag || view->bar.hold != 0)) {
                    view->bar.drag = false;
                    view->bar.hold = 0;
                    view->bar.part = barHit == 3 ? 3 : 0;
                    view->bar.opacity = 255;
                    view->bar.idle = GetTickCount();
                    view->pressedControl = -1;
                    if (GetCapture() == window) {
                        ReleaseCapture();
                    }
                    Request(view);
                    return 0;
                }
                if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) && barHit != 0) {
                    SetFocus(window);
                    view->bar.part = barHit == 4 ? 0 : barHit;
                    view->bar.opacity = 255;
                    view->bar.idle = GetTickCount();
                    view->bar.hot = true;
                    SetCapture(window);
                    SetTimer(window, 2, 50, nullptr);
                    if (barHit == 3) {
                        view->bar.grab = y - WindowScrollThumbY(bar);
                        view->bar.drag = true;
                        view->bar.hold = 0;
                    } else if (barHit == 1 || barHit == 2) {
                        view->bar.hold = barHit == 1 ? -1 : 1;
                        view->bar.repeat = GetTickCount() + 70;
                        WindowScrollBy(bar, view->bar.hold * WindowScrollScale(56, view->baseDpi));
                        KeepScroll(view, bar);
                    } else {
                        WindowScrollToBar(bar, y);
                        KeepScroll(view, bar);
                    }
                    Request(view);
                    return 0;
                }
                int hit = -1;
                if (message != WM_MOUSELEAVE) {
                    const double scale = PageScale(view);
                    for (int index = 0; index < static_cast<int>(view->controls.size()); ++index) {
                        if (HitDocControl(view->controls[static_cast<std::size_t>(index)], x, y, view->scrollX, view->scrollY, scale)) {
                            hit = index;
                            break;
                        }
                    }
                } else {
                    view->trackLeave = false;
                    view->pointerX = -1;
                    view->pointerY = -1;
                }
                if (message != WM_MOUSELEAVE) {
                    view->pointerX = x;
                    view->pointerY = y;
                }
                if (message == WM_MOUSEMOVE || message == WM_MOUSELEAVE) {
                    if (view->zoom.hot != zoomHit) {
                        view->zoom.hot = zoomHit;
                        if (zoomHit != 0) {
                            RevealZoom(view);
                        }
                        Request(view);
                    }
                    if (zoomHit != 0) {
                        if (view->hotControl != -1) {
                            view->hotControl = -1;
                            Request(view);
                        }
                        return 0;
                    }
                    const int part = barHit == 4 ? 0 : barHit;
                    const int partX = barHitX == 4 ? 0 : barHitX;
                    bool paint = false;
                    if (barHit != 0) {
                        view->bar.hot = true;
                        view->bar.idle = GetTickCount();
                        if (view->bar.opacity < 255) {
                            view->bar.opacity = 255;
                            paint = true;
                        }
                        SetTimer(window, 2, 50, nullptr);
                    } else if (view->bar.hot && !view->bar.drag) {
                        view->bar.hot = false;
                        view->bar.idle = GetTickCount();
                        SetTimer(window, 2, 50, nullptr);
                    }
                    if (barHitX != 0) {
                        view->bar.hotX = true;
                        view->bar.idleX = GetTickCount();
                        if (view->bar.opacityX < 255) {
                            view->bar.opacityX = 255;
                            paint = true;
                        }
                        SetTimer(window, 2, 50, nullptr);
                    } else if (view->bar.hotX && !view->bar.dragX) {
                        view->bar.hotX = false;
                        view->bar.idleX = GetTickCount();
                        SetTimer(window, 2, 50, nullptr);
                    }
                    if (part != view->bar.part) {
                        view->bar.part = part;
                        paint = true;
                    }
                    if (partX != view->bar.partX) {
                        view->bar.partX = partX;
                        paint = true;
                    }
                    if (hit != view->hotControl) {
                        view->hotControl = hit;
                        paint = true;
                    }
                    if (paint) {
                        Request(view);
                    }
                    return 0;
                }
                if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) {
                    SetFocus(window);
                    view->pressedControl = hit;
                    SetCapture(window);
                    Request(view);
                    return 0;
                }
                if (message == WM_LBUTTONUP) {
                    view->pressedControl = -1;
                    if (GetCapture() == window) {
                        ReleaseCapture();
                    }
                    Request(view);
                    return 0;
                }
            }
            return 0;
        case WM_MOUSEWHEEL:
            if (view != nullptr && view->ready && view->screen) {
                const int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                if ((GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0 && view->zoom.enabled) {
                    view->zoom.Wheel(static_cast<double>(-wheelDelta));
                    RevealZoom(view);
                    Request(view);
                    return 0;
                }
                UINT lines = 3;
                SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
                int step = 0;
                if (lines == WHEEL_PAGESCROLL) {
                    const int page = static_cast<int>(view->pixelHeight) - WindowScrollScale(64, view->dpi);
                    step = wheelDelta > 0 ? -page : page;
                } else {
                    const int line = WindowScrollScale(48, view->dpi);
                    step = -static_cast<int>(static_cast<long long>(wheelDelta) * static_cast<long long>(lines) * line /
                                             WHEEL_DELTA);
                }
                POINT cursor{};
                cursor.x = static_cast<LONG>(static_cast<short>(LOWORD(lParam)));
                cursor.y = static_cast<LONG>(static_cast<short>(HIWORD(lParam)));
                ScreenToClient(window, &cursor);
                const double scale = PageScale(view);
                const int docX = view->scrollX + ToDoc(cursor.x, scale);
                const int docY = view->scrollY + ToDoc(cursor.y, scale);
                const int docStep = ToDoc(step, scale);
                const bool shift = (GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT) != 0;
                if (WindowsDocumentScrollBy(docX, docY, shift ? docStep : 0, shift ? 0 : docStep) != 0) {
                    Request(view);
                    return 0;
                }
                if (shift) {
                    WindowScrollState bar{};
                    FillBar(view, bar);
                    WindowScrollByX(bar, step);
                    KeepScrollX(view, bar);
                    RevealBar(view, true);
                    Request(view);
                    return 0;
                }
                WindowScrollState bar{};
                FillBar(view, bar);
                WindowScrollBy(bar, step);
                KeepScroll(view, bar);
                view->bar.opacity = 255;
                view->bar.idle = GetTickCount();
                SetTimer(window, 2, 50, nullptr);
                Request(view);
            }
            return 0;
        case WM_MOUSEHWHEEL:
            if (view != nullptr && view->ready && view->screen) {
                const int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                UINT lines = 3;
                SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
                int step = 0;
                if (lines == WHEEL_PAGESCROLL) {
                    const int page = static_cast<int>(view->pixelWidth) - WindowScrollScale(64, view->dpi);
                    step = wheelDelta > 0 ? page : -page;
                } else {
                    const int line = WindowScrollScale(48, view->dpi);
                    step = static_cast<int>(static_cast<long long>(wheelDelta) * static_cast<long long>(lines) * line /
                                            WHEEL_DELTA);
                }
                POINT cursor{};
                cursor.x = static_cast<LONG>(static_cast<short>(LOWORD(lParam)));
                cursor.y = static_cast<LONG>(static_cast<short>(HIWORD(lParam)));
                ScreenToClient(window, &cursor);
                const double scale = PageScale(view);
                const int docX = view->scrollX + ToDoc(cursor.x, scale);
                const int docY = view->scrollY + ToDoc(cursor.y, scale);
                const int docStep = ToDoc(step, scale);
                if (WindowsDocumentScrollBy(docX, docY, docStep, 0) != 0) {
                    Request(view);
                    return 0;
                }
                WindowScrollState bar{};
                FillBar(view, bar);
                WindowScrollByX(bar, step);
                KeepScrollX(view, bar);
                RevealBar(view, true);
                Request(view);
            }
            return 0;
        case WM_KEYDOWN:
            if (view != nullptr && view->zoomMenu && wParam == VK_ESCAPE) {
                view->zoomMenu = false;
                Request(view);
                return 0;
            }
            if (view != nullptr && view->ready && view->screen && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                int direction = 2;
                if (wParam == VK_OEM_PLUS || wParam == VK_ADD) {
                    direction = 1;
                } else if (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT) {
                    direction = -1;
                } else if (wParam == '0' || wParam == VK_NUMPAD0) {
                    direction = 0;
                }
                if (direction != 2) {
                    view->zoom.Step(direction);
                    RevealZoom(view);
                    Request(view);
                    return 0;
                }
            }
            if (view != nullptr && view->ready && view->screen) {
                const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                const int line = WindowScrollScale(48, view->baseDpi);
                const int pageY = static_cast<int>(view->pixelHeight) - WindowScrollScale(64, view->dpi);
                const int pageX = static_cast<int>(view->pixelWidth) - WindowScrollScale(64, view->dpi);
                int axis = 0;
                int delta = 0;
                if (wParam == VK_LEFT) {
                    axis = 2;
                    delta = ctrl ? -pageX : -line;
                } else if (wParam == VK_RIGHT) {
                    axis = 2;
                    delta = ctrl ? pageX : line;
                } else if (wParam == VK_UP) {
                    axis = 1;
                    delta = -line;
                } else if (wParam == VK_DOWN) {
                    axis = 1;
                    delta = line;
                } else if (wParam == VK_PRIOR) {
                    axis = ctrl ? 2 : 1;
                    delta = ctrl ? -pageX : -pageY;
                } else if (wParam == VK_NEXT) {
                    axis = ctrl ? 2 : 1;
                    delta = ctrl ? pageX : pageY;
                } else if (wParam == VK_HOME) {
                    axis = ctrl ? 2 : 1;
                    delta = -100000000;
                } else if (wParam == VK_END) {
                    axis = ctrl ? 2 : 1;
                    delta = 100000000;
                }
                if (axis != 0) {
                    WindowScrollState bar{};
                    FillBar(view, bar);
                    if (axis == 2) {
                        WindowScrollByX(bar, delta);
                        KeepScrollX(view, bar);
                        RevealBar(view, true);
                    } else {
                        WindowScrollBy(bar, delta);
                        KeepScroll(view, bar);
                        RevealBar(view, false);
                    }
                    Request(view);
                    return 0;
                }
            }
            if (view != nullptr && view->ready && view->screen && wParam == VK_TAB) {
                const int count = static_cast<int>(view->controls.size());
                if (count > 0) {
                    const bool backward = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    int next = view->focusControl;
                    for (int step = 0; step < count; ++step) {
                        if (backward) {
                            next = next <= 0 ? count - 1 : next - 1;
                        } else {
                            next = next + 1 >= count ? 0 : next + 1;
                        }
                        if (view->controls[static_cast<std::size_t>(next)].enabled) {
                            view->focusControl = next;
                            const WindowControl& control = view->controls[static_cast<std::size_t>(next)];
                            const double scale = PageScale(view);
                            const int viewDoc = std::max(1, ToDoc(static_cast<int>(view->pixelHeight), scale));
                            if (control.y < view->scrollY) {
                                view->scrollY = control.y;
                            } else if (control.y + control.height > view->scrollY + viewDoc) {
                                view->scrollY = control.y + control.height - viewDoc;
                            }
                            Request(view);
                            break;
                        }
                    }
                }
            } else if (view != nullptr && (wParam == VK_SPACE || wParam == VK_RETURN) && view->focusControl >= 0 &&
                       view->focusControl < static_cast<int>(view->controls.size()) &&
                       view->controls[static_cast<std::size_t>(view->focusControl)].enabled) {
                view->pressedControl = view->focusControl;
                Request(view);
            }
            return 0;
        case WM_KEYUP:
            if (view != nullptr && (wParam == VK_SPACE || wParam == VK_RETURN) && view->pressedControl == view->focusControl &&
                view->pressedControl >= 0) {
                view->pressedControl = -1;
                Request(view);
            }
            return 0;
        case WM_TIMER:
            if (wParam == 5) {
                if (view == nullptr) {
                    KillTimer(window, 5);
                    return 0;
                }
                if (view->zoom.hot != 0 || view->zoom.pressed != 0 || view->zoomMenu) {
                    view->zoomOpacity = 255;
                    view->zoomIdle = GetTickCount();
                    return 0;
                }
                if (view->zoomOpacity > 0 && GetTickCount() - view->zoomIdle > 900) {
                    const int drop = view->zoomOpacity / 5;
                    view->zoomOpacity -= drop < 18 ? 18 : drop;
                    if (view->zoomOpacity < 0) {
                        view->zoomOpacity = 0;
                    }
                    InvalidateRect(window, nullptr, FALSE);
                }
                if (view->zoomOpacity <= 0) {
                    KillTimer(window, 5);
                }
                return 0;
            }
            if (wParam == 4) {
                if (view == nullptr || !WindowsDocumentNeedsSpin()) {
                    KillTimer(window, 4);
                    return 0;
                }
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (wParam == 3) {
                InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            if (view != nullptr && wParam == 2 && view->ready) {
                bool paint = false;
                if (view->bar.hold != 0 && GetTickCount() >= view->bar.repeat) {
                    WindowScrollState bar{};
                    FillBar(view, bar);
                    WindowScrollBy(bar, view->bar.hold * WindowScrollScale(56, view->baseDpi));
                    KeepScroll(view, bar);
                    view->bar.opacity = 255;
                    view->bar.idle = GetTickCount();
                    view->bar.repeat = GetTickCount() + 70;
                    paint = true;
                }
                if (view->bar.holdX != 0 && GetTickCount() >= view->bar.repeatX) {
                    WindowScrollState bar{};
                    FillBar(view, bar);
                    WindowScrollByX(bar, view->bar.holdX * WindowScrollScale(56, view->baseDpi));
                    KeepScrollX(view, bar);
                    view->bar.opacityX = 255;
                    view->bar.idleX = GetTickCount();
                    view->bar.repeatX = GetTickCount() + 70;
                    paint = true;
                }
                if (view->bar.opacity > 0 && !view->bar.hot && !view->bar.drag && view->bar.hold == 0 &&
                    GetTickCount() - view->bar.idle > 900) {
                    const int drop = view->bar.opacity / 6;
                    view->bar.opacity -= drop < 8 ? 8 : drop;
                    if (view->bar.opacity < 0) {
                        view->bar.opacity = 0;
                    }
                    paint = true;
                }
                if (view->bar.opacityX > 0 && !view->bar.hotX && !view->bar.dragX && view->bar.holdX == 0 &&
                    GetTickCount() - view->bar.idleX > 900) {
                    const int drop = view->bar.opacityX / 6;
                    view->bar.opacityX -= drop < 8 ? 8 : drop;
                    if (view->bar.opacityX < 0) {
                        view->bar.opacityX = 0;
                    }
                    paint = true;
                }
                if (paint) {
                    Request(view);
                }
                return 0;
            }
            if (view != nullptr && view->watched != nullptr && view->watched->watch != nullptr &&
                view->watched->watch(*view->watched, view->watched->watchUser)) {
                view->light = view->watched->light;
                view->screen = view->watched->screen;
                view->movable = view->watched->movable;
                view->hasScreenColor = view->watched->hasScreenColor;
                view->screenColor = view->watched->background;
                SetWindowTextA(window, view->watched->title.c_str());
                view->painted = false;
                Request(view);
                std::printf("interpreter: refreshed\n");
            }
            return 0;
        case WM_POINTERDOWN:
        case WM_POINTERUPDATE:
        case WM_POINTERUP:
        case WM_POINTERCAPTURECHANGED:
            if (view != nullptr && view->ready && view->screen) {
                const UINT id = GET_POINTERID_WPARAM(wParam);
                POINTER_INFO info{};
                if (GetPointerInfo(id, &info) && info.pointerType != PT_MOUSE) {
                    POINT point = info.ptPixelLocation;
                    ScreenToClient(window, &point);
                    int slot = -1;
                    for (int index = 0; index < view->pinchCount; ++index) {
                        if (view->pinchId[index] == id) {
                            slot = index;
                            break;
                        }
                    }
                    if (message == WM_POINTERDOWN) {
                        if (slot < 0 && view->pinchCount < 2) {
                            view->pinchId[view->pinchCount] = id;
                            view->pinchX[view->pinchCount] = point.x;
                            view->pinchY[view->pinchCount] = point.y;
                            ++view->pinchCount;
                        }
                    } else if (message == WM_POINTERUPDATE) {
                        if (slot >= 0) {
                            const int dx = point.x - view->pinchX[slot];
                            const int dy = point.y - view->pinchY[slot];
                            view->pinchX[slot] = point.x;
                            view->pinchY[slot] = point.y;
                            if (view->pinchCount == 1 && (dx != 0 || dy != 0)) {
                                view->panLive = true;
                                WindowScrollState bar{};
                                FillBar(view, bar);
                                if (dx != 0) {
                                    WindowScrollByX(bar, -dx);
                                    KeepScrollX(view, bar);
                                    RevealBar(view, true);
                                }
                                if (dy != 0) {
                                    FillBar(view, bar);
                                    WindowScrollBy(bar, -dy);
                                    KeepScroll(view, bar);
                                    RevealBar(view, false);
                                }
                                Request(view);
                            }
                        }
                    } else if (slot >= 0) {
                        const int last = view->pinchCount - 1;
                        view->pinchId[slot] = view->pinchId[last];
                        view->pinchX[slot] = view->pinchX[last];
                        view->pinchY[slot] = view->pinchY[last];
                        --view->pinchCount;
                        view->pinchLive = false;
                        view->panLive = false;
                    }
                    if (view->pinchCount == 2) {
                        const double dx = static_cast<double>(view->pinchX[0] - view->pinchX[1]);
                        const double dy = static_cast<double>(view->pinchY[0] - view->pinchY[1]);
                        const double distance = std::sqrt(dx * dx + dy * dy);
                        if (!view->pinchLive && distance > 0) {
                            view->pinchLive = true;
                            view->pinchOrigin = view->zoom.scale;
                            view->pinchDistance = distance;
                        } else if (view->pinchLive && view->pinchDistance > 0) {
                            const bool changed = view->zoom.Pinch(view->pinchOrigin, distance / view->pinchDistance);
                            const bool hidden = view->zoomOpacity <= 0;
                            RevealZoom(view);
                            if (changed || hidden) {
                                Request(view);
                            }
                        }
                    } else {
                        view->pinchLive = false;
                    }
                    return 0;
                }
            }
            return DefWindowProcW(window, message, wParam, lParam);
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}

bool WriteBmp24(const std::string& path, const RoboU8* bgra, UINT width, UINT height, UINT pitch, std::string& error) {
    if (bgra == nullptr || width == 0 || height == 0) {
        error = "the framebuffer is empty";
        return false;
    }
    const UINT row = width * 3u;
    const UINT padded = (row + 3u) & ~3u;
    std::vector<RoboU8> pixels(static_cast<size_t>(padded) * height);
    for (UINT y = 0; y < height; ++y) {
        const RoboU8* source = bgra + static_cast<size_t>(height - 1u - y) * pitch;
        RoboU8* dest = pixels.data() + static_cast<size_t>(y) * padded;
        for (UINT x = 0; x < width; ++x) {
            dest[x * 3u] = source[x * 4u];
            dest[x * 3u + 1u] = source[x * 4u + 1u];
            dest[x * 3u + 2u] = source[x * 4u + 2u];
        }
    }
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER info{};
    file.bfType = 0x4D42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
    info.biSize = sizeof(info);
    info.biWidth = static_cast<LONG>(width);
    info.biHeight = static_cast<LONG>(height);
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biSizeImage = static_cast<DWORD>(pixels.size());
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        error = "could not write '" + path + "'";
        return false;
    }
    out.write(reinterpret_cast<const char*>(&file), sizeof(file));
    out.write(reinterpret_cast<const char*>(&info), sizeof(info));
    out.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    return true;
}

}  // namespace

int RunWindowsFoundation(const WindowRunOptions& options) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HostView view{};
    view.stayOpen = options.stayOpen;
    view.light = options.light;
    view.screen = options.screen;
    view.hasScreenColor = options.hasScreenColor;
    view.screenColor = options.background;
    view.movable = options.movable;
    view.document = options.document;
    WindowRunOptions watched = options;
    view.watched = options.watch != nullptr ? &watched : nullptr;

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = Procedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"RobixUIFoundation";
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
    RegisterClassW(&windowClass);

    const UINT systemDpi = GetDpiForSystem();
    const int designWidth = options.screen ? options.width : static_cast<int>(kDesignWidth);
    const int designHeight = options.screen ? options.height : static_cast<int>(kDesignHeight);
    const int clientWidth = static_cast<int>(designWidth * systemDpi / 96u);
    const int clientHeight = static_cast<int>(designHeight * systemDpi / 96u);
    const DWORD style = options.screen
                            ? (WS_OVERLAPPEDWINDOW & ~(options.resizable ? 0 : WS_THICKFRAME))
                            : WS_OVERLAPPEDWINDOW;
    RECT bounds{0, 0, clientWidth, clientHeight};
    AdjustWindowRectExForDpi(&bounds, style, FALSE, 0, systemDpi);
    int originX = CW_USEDEFAULT;
    int originY = CW_USEDEFAULT;
    if (options.screen && options.place == "center") {
        originX = (GetSystemMetrics(SM_CXSCREEN) - (bounds.right - bounds.left)) / 2;
        originY = (GetSystemMetrics(SM_CYSCREEN) - (bounds.bottom - bounds.top)) / 2;
    } else if (options.screen && options.place == "absolute") {
        originX = options.x;
        originY = options.y;
    }
    std::wstring title(options.title.begin(), options.title.end());
    HWND window = CreateWindowExW(0, windowClass.lpszClassName, title.c_str(), style, originX, originY,
                                  bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr, instance, &view);
    if (window == nullptr) {
        return 1;
    }
    view.window = window;
    RECT client{};
    GetClientRect(window, &client);
    const UINT pixelWidth = static_cast<UINT>(client.right - client.left);
    const UINT pixelHeight = static_cast<UINT>(client.bottom - client.top);

    const D3D_FEATURE_LEVEL feature = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, &feature, 1,
                                 D3D11_SDK_VERSION, &view.device, nullptr, &view.context))) {
        MessageBoxW(window, L"Direct3D 11 did not start.", L"Robix", MB_OK | MB_ICONERROR);
        DestroyWindow(window);
        return 1;
    }
    IDXGIDevice1* latencyDevice = nullptr;
    if (SUCCEEDED(view.device->QueryInterface(__uuidof(IDXGIDevice1), reinterpret_cast<void**>(&latencyDevice))) &&
        latencyDevice != nullptr) {
        latencyDevice->SetMaximumFrameLatency(1);
        latencyDevice->Release();
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
            view.flip = true;
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
        MessageBoxW(window, L"Direct3D 11 did not start.", L"Robix", MB_OK | MB_ICONERROR);
        DestroyWindow(window);
        Release(&view);
        return 1;
    }
    if (!EnsureSurface(&view, pixelWidth, pixelHeight)) {
        DestroyWindow(window);
        Release(&view);
        return 1;
    }
    view.ready = true;
    view.dpi = GetDpiForWindow(window);
    if (view.dpi == 0) {
        view.dpi = 96;
    }
    view.baseDpi = view.dpi;
    if (options.screen) {
        std::printf("component Screen\n");
        std::printf("screen place %s %d %d %dx%d\n", options.place.c_str(), originX, originY, clientWidth, clientHeight);
        std::printf("screen state normal movable %s resizable %s\n", options.movable ? "true" : "false",
                    options.resizable ? "true" : "false");
    }
    std::printf("window: shown %ux%u title %s\n", pixelWidth, pixelHeight, options.title.c_str());
    ShowWindow(window, SW_SHOW);
    SetFocus(window);
    UpdateWindow(window);
    if (view.watched != nullptr) {
        SetTimer(window, 1, 400, nullptr);
    }
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (!options.stayOpen) {
        std::printf("window: closed after paint\n");
    }
    Release(&view);
    return 0;
}

bool WriteFoundationBmp(const std::string& path, std::string& error) {
    constexpr UINT width = kDesignWidth;
    constexpr UINT height = kDesignHeight;
    const RoboU64 byteCount = static_cast<RoboU64>(width) * height * 4u;
    auto* back = static_cast<RoboU8*>(_aligned_malloc(static_cast<size_t>(byteCount), 16));
    if (back == nullptr) {
        error = "the framebuffer is empty";
        return false;
    }
    RoboGPU gpu{};
    RoboHandoff handoff{};
    handoff.magic = ROBO_HANDOFF_MAGIC;
    handoff.version = ROBO_HANDOFF_VERSION;
    handoff.frameBuffer.base = reinterpret_cast<RoboU64>(back);
    handoff.frameBuffer.width = width;
    handoff.frameBuffer.height = height;
    handoff.frameBuffer.pixelsPerScanLine = width;
    handoff.frameBuffer.bytesPerPixel = 4;
    handoff.frameBuffer.redMask = 0x00FF0000u;
    handoff.frameBuffer.greenMask = 0x0000FF00u;
    handoff.frameBuffer.blueMask = 0x000000FFu;
    RoboGPUInitFromHandoff(&gpu, &handoff, back, byteCount);
    PaintFoundation(&gpu, false, true, false, 0);
    const bool wrote = WriteBmp24(path, gpu.back, width, height, width * 4u, error);
    _aligned_free(back);
    if (wrote) {
        std::printf("renderer platform-framebuffer\n");
        std::printf("framebuffer %ux%u\n", width, height);
    }
    return wrote;
}

}  // namespace Robix::UI
