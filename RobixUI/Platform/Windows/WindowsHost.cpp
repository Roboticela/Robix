#include "WindowsHost.hpp"
#include "WindowsDocument.hpp"
#include "WindowsScroll.hpp"

#include "../../Compiler/Foundation/FoundationPlatform.hpp"
#include "../../RoboGPU/RoboGPU.hpp"
#include "../../RoboGPU/RoboText.hpp"
#include "../../RoboShared/RoboFont.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
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
    RoboGPU gpu{};
    RoboU8* front = nullptr;
    RoboU8* back = nullptr;
    UINT pixelWidth = 0;
    UINT pixelHeight = 0;
    bool flip = false;
    bool ready = false;
    bool painted = false;
    bool stayOpen = false;
    bool light = false;
    bool screen = false;
    bool movable = true;
    int scrollY = 0;
    int contentHeight = 0;
    unsigned dpi = 96;
    WindowScrollState bar{};
    std::vector<WindowControl> controls;
    int hotControl = -1;
    int pressedControl = -1;
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
    int shownScroll = -1;
};

void PaintFoundation(RoboGPU* gpu, bool light, bool drawText) {
    RoboWindowsInstallFontRaster(nullptr, 0);
    RoboGPUClear(gpu, RoboFoundationPlatformColorForTheme("bg-primary", light ? ROBO_TRUE : ROBO_FALSE));
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
    if (view->upload != nullptr && view->pixelWidth == width && view->pixelHeight == height && view->front != nullptr &&
        view->back != nullptr) {
        return true;
    }
    if (view->upload != nullptr) {
        view->upload->Release();
        view->upload = nullptr;
    }
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
    const RoboU64 byteCount = static_cast<RoboU64>(width) * height * 4u;
    auto* front = static_cast<RoboU8*>(_aligned_malloc(static_cast<size_t>(byteCount), 16));
    auto* back = static_cast<RoboU8*>(_aligned_malloc(static_cast<size_t>(byteCount), 16));
    if (front == nullptr || back == nullptr) {
        _aligned_free(front);
        _aligned_free(back);
        return false;
    }
    _aligned_free(view->front);
    _aligned_free(view->back);
    view->front = front;
    view->back = back;
    view->pixelWidth = width;
    view->pixelHeight = height;
    RoboHandoff handoff{};
    handoff.magic = ROBO_HANDOFF_MAGIC;
    handoff.version = ROBO_HANDOFF_VERSION;
    handoff.frameBuffer.base = reinterpret_cast<RoboU64>(view->front);
    handoff.frameBuffer.width = width;
    handoff.frameBuffer.height = height;
    handoff.frameBuffer.pixelsPerScanLine = width;
    handoff.frameBuffer.bytesPerPixel = 4;
    handoff.frameBuffer.redMask = 0x00FF0000u;
    handoff.frameBuffer.greenMask = 0x0000FF00u;
    handoff.frameBuffer.blueMask = 0x000000FFu;
    RoboGPUInitFromHandoff(&view->gpu, &handoff, view->back, byteCount);
    view->gpuScroll = -1;
    view->bufferReady[0] = false;
    view->bufferReady[1] = false;
    view->recentCount[0] = 0;
    view->recentCount[1] = 0;
    return true;
}

void UploadBox(HostView* view, ID3D11Texture2D* texture, LONG left, LONG top, LONG right, LONG bottom) {
    if (texture == nullptr || view->gpu.back == nullptr) {
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
    const auto* source = view->gpu.back + static_cast<size_t>(box.top) * pitch + static_cast<size_t>(box.left) * 4u;
    view->context->UpdateSubresource(texture, 0, &box, source, pitch, 0);
}

void CopyBox(HostView* view, ID3D11Texture2D* target, LONG left, LONG top, LONG right, LONG bottom) {
    if (target == nullptr || view->upload == nullptr || right <= left || bottom <= top) {
        return;
    }
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

void Publish(HostView* view) {
    if (view == nullptr || view->context == nullptr || view->swap == nullptr || view->gpu.back == nullptr) {
        return;
    }
    if (RoboDamageIsEmpty(&view->gpu.damage)) {
        return;
    }
    const bool full = view->gpu.damage.full == ROBO_TRUE;
    RECT dirty[24]{};
    UINT dirtyCount = 0;
    if (!full) {
        for (RoboU32 index = 0; index < view->gpu.damage.count && dirtyCount < 24; ++index) {
            const RoboDamageRect& rect = view->gpu.damage.rects[index];
            dirty[dirtyCount].left = rect.x;
            dirty[dirtyCount].top = rect.y;
            dirty[dirtyCount].right = rect.x + rect.width;
            dirty[dirtyCount].bottom = rect.y + rect.height;
            ++dirtyCount;
        }
        if (view->gpu.damage.count > 24) {
            dirtyCount = 0;
        }
    }
    ID3D11Texture2D* target = nullptr;
    if (FAILED(view->swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&target))) || target == nullptr) {
        return;
    }
    const UINT pitch = view->pixelWidth * 4u;
    const int shift = view->gpuScroll < 0 ? 0 : view->scrollY - view->gpuScroll;
    const bool uploadAll = full || dirtyCount == 0 || view->gpuScroll < 0 ||
                           shift >= static_cast<int>(view->pixelHeight) || -shift >= static_cast<int>(view->pixelHeight);
    if (uploadAll) {
        if (view->upload != nullptr) {
            view->context->UpdateSubresource(view->upload, 0, nullptr, view->gpu.back, pitch, 0);
            view->context->CopyResource(target, view->upload);
        } else {
            view->context->UpdateSubresource(target, 0, nullptr, view->gpu.back, pitch, 0);
        }
        view->gpuScroll = view->scrollY;
        view->bufferReady[0] = false;
        view->bufferReady[1] = false;
        view->bufferReady[view->presentIndex & 1] = true;
        view->recentCount[0] = 0;
        view->recentCount[1] = 0;
        target->Release();
        view->swap->Present(1, 0);
        view->presentIndex ^= 1;
        RoboDamageClear(&view->gpu.damage);
        return;
    }
    if (shift != 0 && view->upload != nullptr) {
        ScrollUpload(view, target, shift);
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
        view->swap->Present(1, 0);
        view->presentIndex ^= 1;
        RoboDamageClear(&view->gpu.damage);
        return;
    }
    const int slot = view->presentIndex & 1;
    const int older = slot ^ 1;
    view->recentCount[slot] = 0;
    for (UINT index = 0; index < dirtyCount && view->recentCount[slot] < 16; ++index) {
        view->recent[slot][view->recentCount[slot]++] = dirty[index];
    }
    if (view->upload != nullptr && !view->bufferReady[slot]) {
        view->context->CopyResource(target, view->upload);
        view->bufferReady[slot] = true;
    }
    if (view->upload != nullptr) {
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
        view->context->UpdateSubresource(target, 0, nullptr, view->gpu.back, pitch, 0);
    }
    target->Release();
    RECT presentRects[16]{};
    UINT presentCount = 0;
    for (UINT index = 0; index < dirtyCount && presentCount < 16; ++index) {
        presentRects[presentCount++] = dirty[index];
    }
    DXGI_PRESENT_PARAMETERS params{};
    params.DirtyRectsCount = presentCount;
    params.pDirtyRects = presentRects;
    if (presentCount == 0 || FAILED(view->swap->Present1(1, 0, &params))) {
        view->swap->Present(1, 0);
    }
    view->presentIndex ^= 1;
    RoboDamageClear(&view->gpu.damage);
}

void Draw(HostView* view) {
    if (view == nullptr || !view->ready || view->gpu.back == nullptr) {
        return;
    }
    if (view->screen && !view->document.empty()) {
        const int height = static_cast<int>(view->pixelHeight);
        const int maxScroll = std::max(0, view->contentHeight - height);
        if (view->scrollY > maxScroll) {
            view->scrollY = maxScroll;
        }
        if (view->scrollY < 0) {
            view->scrollY = 0;
        }
        const int pointerDocY = view->pointerX < 0 ? -1 : view->pointerY + view->scrollY;
        PaintWindowsDocument(&view->gpu, view->document, view->light, static_cast<int>(view->pixelWidth), height,
                             view->scrollY, &view->contentHeight, &view->controls, view->hotControl, view->pressedControl,
                             view->dpi, view->pointerX, pointerDocY);
        WindowScrollState bar = view->bar;
        bar.scrollY = view->scrollY;
        bar.contentHeight = view->contentHeight;
        bar.pixelWidth = static_cast<int>(view->pixelWidth);
        bar.pixelHeight = height;
        bar.dpi = view->dpi;
        bar.light = view->light;
        const bool barChanged = bar.opacity != view->shownOpacity || bar.part != view->shownPart || bar.hold != view->shownHold ||
                                bar.drag != view->shownDrag || bar.hot != view->shownHot || bar.scrollY != view->shownScroll;
        if (barChanged && view->gpu.damage.full != ROBO_TRUE) {
            int barX = 0;
            int barY = 0;
            int barW = 0;
            int barH = 0;
            WindowScrollBarRect(bar, barX, barY, barW, barH);
            RestoreWindowsDocumentRect(&view->gpu, barX, barY, barW, barH);
        }
        if (view->gpu.damage.full == ROBO_TRUE || barChanged) {
            PaintWindowScrollBar(&view->gpu, bar);
        }
        view->shownOpacity = bar.opacity;
        view->shownPart = bar.part;
        view->shownHold = bar.hold;
        view->shownDrag = bar.drag;
        view->shownHot = bar.hot;
        view->shownScroll = bar.scrollY;
    } else {
        PaintFoundation(&view->gpu, view->light, !view->screen);
    }
    Publish(view);
    view->painted = true;
    std::printf("framebuffer %ux%u\n", view->pixelWidth, view->pixelHeight);
    std::printf("renderer platform-framebuffer\n");
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
    _aligned_free(view->front);
    _aligned_free(view->back);
    view->front = nullptr;
    view->back = nullptr;
}

void Resize(HostView* view, UINT width, UINT height) {
    if (view == nullptr || !view->ready || width == 0 || height == 0) {
        return;
    }
    if (view->upload != nullptr && view->pixelWidth == width && view->pixelHeight == height) {
        return;
    }
    if (EnsureSurface(view, width, height)) {
        Draw(view);
    }
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
            SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_SIZE:
            if (view != nullptr && view->ready) {
                Resize(view, LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            BeginPaint(window, &paint);
            if (view != nullptr && view->ready && !view->painted) {
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
                WindowScrollState bar = view->bar;
                bar.scrollY = view->scrollY;
                bar.contentHeight = view->contentHeight;
                bar.pixelWidth = static_cast<int>(view->pixelWidth);
                bar.pixelHeight = static_cast<int>(view->pixelHeight);
                bar.dpi = view->dpi;
                const int barHit = HitWindowScrollBar(bar, cursor.x, cursor.y);
                bool onControl = false;
                bool controlEnabled = true;
                const int docY = cursor.y + view->scrollY;
                for (const WindowControl& control : view->controls) {
                    if (cursor.x >= control.x && cursor.x < control.x + control.width && docY >= control.y &&
                        docY < control.y + control.height) {
                        onControl = true;
                        controlEnabled = control.enabled;
                        break;
                    }
                }
                const wchar_t* cursorId = IDC_ARROW;
                if (barHit == 0 && onControl) {
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
                WindowScrollState bar = view->bar;
                bar.scrollY = view->scrollY;
                bar.contentHeight = view->contentHeight;
                bar.pixelWidth = static_cast<int>(view->pixelWidth);
                bar.pixelHeight = static_cast<int>(view->pixelHeight);
                bar.dpi = view->dpi;
                bar.light = view->light;
                const int barHit = message == WM_MOUSELEAVE ? 0 : HitWindowScrollBar(bar, x, y);
                if (view->bar.drag && message == WM_MOUSEMOVE) {
                    WindowDragScrollBar(bar, y);
                    view->scrollY = bar.scrollY;
                    view->bar.opacity = 255;
                    view->bar.idle = GetTickCount();
                    Draw(view);
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
                    Draw(view);
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
                        WindowScrollBy(bar, view->bar.hold * WindowScrollScale(56, view->dpi));
                        view->scrollY = bar.scrollY;
                    } else {
                        WindowScrollToBar(bar, y);
                        view->scrollY = bar.scrollY;
                    }
                    Draw(view);
                    return 0;
                }
                int hit = -1;
                if (message != WM_MOUSELEAVE) {
                    const int docY = y + view->scrollY;
                    for (int index = 0; index < static_cast<int>(view->controls.size()); ++index) {
                        const WindowControl& control = view->controls[index];
                        if (x >= control.x && x < control.x + control.width && docY >= control.y &&
                            docY < control.y + control.height) {
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
                    const int part = barHit == 4 ? 0 : barHit;
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
                    if (part != view->bar.part) {
                        view->bar.part = part;
                        paint = true;
                    }
                    if (hit != view->hotControl) {
                        view->hotControl = hit;
                        paint = true;
                    }
                    if (paint) {
                        Draw(view);
                    }
                    return 0;
                }
                if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) {
                    SetFocus(window);
                    view->pressedControl = hit;
                    SetCapture(window);
                    Draw(view);
                    return 0;
                }
                if (message == WM_LBUTTONUP) {
                    view->pressedControl = -1;
                    if (GetCapture() == window) {
                        ReleaseCapture();
                    }
                    Draw(view);
                    return 0;
                }
            }
            return 0;
        case WM_MOUSEWHEEL:
            if (view != nullptr && view->ready && view->screen) {
                UINT lines = 3;
                SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
                const int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                int step = 0;
                if (lines == WHEEL_PAGESCROLL) {
                    const int page = static_cast<int>(view->pixelHeight) - WindowScrollScale(64, view->dpi);
                    step = wheelDelta > 0 ? -page : page;
                } else {
                    const int line = WindowScrollScale(48, view->dpi);
                    step = -static_cast<int>(static_cast<long long>(wheelDelta) * static_cast<long long>(lines) * line /
                                             WHEEL_DELTA);
                }
                WindowScrollState bar = view->bar;
                bar.scrollY = view->scrollY;
                bar.contentHeight = view->contentHeight;
                bar.pixelHeight = static_cast<int>(view->pixelHeight);
                WindowScrollBy(bar, step);
                view->scrollY = bar.scrollY;
                view->bar.opacity = 255;
                view->bar.idle = GetTickCount();
                SetTimer(window, 2, 50, nullptr);
                Draw(view);
            }
            return 0;
        case WM_KEYDOWN:
            return 0;
        case WM_TIMER:
            if (view != nullptr && wParam == 2 && view->ready) {
                bool paint = false;
                if (view->bar.hold != 0 && GetTickCount() >= view->bar.repeat) {
                    WindowScrollState bar = view->bar;
                    bar.scrollY = view->scrollY;
                    bar.contentHeight = view->contentHeight;
                    bar.pixelHeight = static_cast<int>(view->pixelHeight);
                    WindowScrollBy(bar, view->bar.hold * WindowScrollScale(56, view->dpi));
                    view->scrollY = bar.scrollY;
                    view->bar.opacity = 255;
                    view->bar.idle = GetTickCount();
                    view->bar.repeat = GetTickCount() + 70;
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
                if (paint) {
                    Draw(view);
                }
                return 0;
            }
            if (view != nullptr && view->watched != nullptr && view->watched->watch != nullptr &&
                view->watched->watch(*view->watched, view->watched->watchUser)) {
                view->light = view->watched->light;
                view->screen = view->watched->screen;
                view->movable = view->watched->movable;
                SetWindowTextA(window, view->watched->title.c_str());
                view->painted = false;
                Draw(view);
                std::printf("interpreter: refreshed\n");
            }
            return 0;
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
    if (options.screen) {
        std::printf("component Screen\n");
        std::printf("screen place %s %d %d %dx%d\n", options.place.c_str(), originX, originY, clientWidth, clientHeight);
        std::printf("screen state normal movable %s resizable %s\n", options.movable ? "true" : "false",
                    options.resizable ? "true" : "false");
    }
    std::printf("window: shown %ux%u title %s\n", pixelWidth, pixelHeight, options.title.c_str());
    ShowWindow(window, SW_SHOW);
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
    auto* front = static_cast<RoboU8*>(_aligned_malloc(static_cast<size_t>(byteCount), 16));
    auto* back = static_cast<RoboU8*>(_aligned_malloc(static_cast<size_t>(byteCount), 16));
    if (front == nullptr || back == nullptr) {
        _aligned_free(front);
        _aligned_free(back);
        error = "the framebuffer is empty";
        return false;
    }
    RoboGPU gpu{};
    RoboHandoff handoff{};
    handoff.magic = ROBO_HANDOFF_MAGIC;
    handoff.version = ROBO_HANDOFF_VERSION;
    handoff.frameBuffer.base = reinterpret_cast<RoboU64>(front);
    handoff.frameBuffer.width = width;
    handoff.frameBuffer.height = height;
    handoff.frameBuffer.pixelsPerScanLine = width;
    handoff.frameBuffer.bytesPerPixel = 4;
    handoff.frameBuffer.redMask = 0x00FF0000u;
    handoff.frameBuffer.greenMask = 0x0000FF00u;
    handoff.frameBuffer.blueMask = 0x000000FFu;
    RoboGPUInitFromHandoff(&gpu, &handoff, back, byteCount);
    PaintFoundation(&gpu, false, true);
    const bool wrote = WriteBmp24(path, gpu.back, width, height, width * 4u, error);
    _aligned_free(front);
    _aligned_free(back);
    if (wrote) {
        std::printf("renderer platform-framebuffer\n");
        std::printf("framebuffer %ux%u\n", width, height);
    }
    return wrote;
}

}  // namespace Robix::UI
