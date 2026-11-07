#include "WindowsHost.hpp"

#include "../../Compiler/Foundation/FoundationPlatform.hpp"
#include "../../RoboGPU/RoboGPU.hpp"
#include "../../RoboGPU/RoboText.hpp"
#include "../../RoboShared/RoboFont.hpp"

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
    WindowRunOptions* watched = nullptr;
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
    return true;
}

void Publish(HostView* view) {
    if (view == nullptr || view->context == nullptr || view->swap == nullptr || view->gpu.back == nullptr) {
        return;
    }
    if (RoboDamageIsEmpty(&view->gpu.damage)) {
        return;
    }
    ID3D11Texture2D* target = nullptr;
    if (FAILED(view->swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&target))) || target == nullptr) {
        return;
    }
    const UINT pitch = view->pixelWidth * 4u;
    if (view->upload != nullptr) {
        view->context->UpdateSubresource(view->upload, 0, nullptr, view->gpu.back, pitch, 0);
        view->context->CopyResource(target, view->upload);
    } else {
        view->context->UpdateSubresource(target, 0, nullptr, view->gpu.back, pitch, 0);
    }
    target->Release();
    view->swap->Present(1, 0);
    RoboDamageClear(&view->gpu.damage);
}

void Draw(HostView* view) {
    if (view == nullptr || !view->ready || view->gpu.back == nullptr) {
        return;
    }
    PaintFoundation(&view->gpu, view->light, !view->screen);
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
        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_LBUTTONDOWN:
            if (view != nullptr && view->screen && view->movable) {
                ReleaseCapture();
                SendMessageW(window, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
            return 0;
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        case WM_KEYDOWN:
            return 0;
        case WM_TIMER:
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
    const DWORD style = options.screen ? (WS_POPUP | (options.resizable ? WS_THICKFRAME : 0)) : WS_OVERLAPPEDWINDOW;
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
