#include "Window.hpp"
#include "../Framebuffer.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>

namespace Robix::UI::Platform::Windows {
namespace {

struct Frame {
    std::vector<std::uint32_t> pixels;
    std::vector<std::uint32_t> display;
    int width = 0;
    int height = 0;
    bool painted = false;
    bool movable = false;
    std::vector<int> buttons;
    bool applying = false;
    bool (*refresh)(std::vector<std::uint32_t>& pixels, int& width, int& height, std::string& title, void* user) = nullptr;
    bool (*resize)(std::vector<std::uint32_t>& pixels, int& width, int& height, std::string& title, void* user) = nullptr;
    void (*click)(int x, int y, void* user) = nullptr;
    void* user = nullptr;
    HWND window = nullptr;
};

Frame* g_frame = nullptr;

std::wstring Wide(const std::string& text) {
    if (text.empty()) return L"RobixUI";
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(size > 0 ? static_cast<std::size_t>(size) : 0, L'\0');
    if (size > 0) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_SIZE && g_frame != nullptr && !g_frame->applying && g_frame->resize != nullptr) {
        const int width = static_cast<int>(LOWORD(lparam));
        const int height = static_cast<int>(HIWORD(lparam));
        if (width > 0 && height > 0 && (width != g_frame->width || height != g_frame->height)) {
            std::vector<std::uint32_t> next;
            int nextW = width;
            int nextH = height;
            std::string title;
            if (g_frame->resize(next, nextW, nextH, title, g_frame->user) && !next.empty()) {
                g_frame->pixels = std::move(next);
                g_frame->width = nextW;
                g_frame->height = nextH;
                std::cout << "framebuffer resized " << nextW << "x" << nextH << "\n";
                std::cout.flush();
                InvalidateRect(window, nullptr, FALSE);
            }
        }
    }
    if (message == WM_PAINT && g_frame != nullptr && !g_frame->pixels.empty()) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        const int destW = std::max(1, static_cast<int>(client.right - client.left));
        const int destH = std::max(1, static_cast<int>(client.bottom - client.top));
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = g_frame->width;
        info.bmiHeader.biHeight = -g_frame->height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        CopyToDisplay(g_frame->pixels, g_frame->display);
        StretchDIBits(
            dc, 0, 0, destW, destH,
            0, 0, g_frame->width, g_frame->height,
            g_frame->display.data(), &info, DIB_RGB_COLORS, SRCCOPY);
        EndPaint(window, &paint);
        g_frame->painted = true;
        return 0;
    }
    if (message == WM_LBUTTONDOWN && g_frame != nullptr && g_frame->movable) {
        const int x = static_cast<int>(static_cast<short>(LOWORD(lparam)));
        const int y = static_cast<int>(static_cast<short>(HIWORD(lparam)));
        bool onButton = false;
        for (std::size_t i = 0; i + 3 < g_frame->buttons.size(); i += 4) {
            const int left = g_frame->buttons[i];
            const int top = g_frame->buttons[i + 1];
            const int right = left + g_frame->buttons[i + 2];
            const int bottom = top + g_frame->buttons[i + 3];
            if (x >= left && y >= top && x < right && y < bottom) onButton = true;
        }
        if (!onButton) {
            ReleaseCapture();
            SendMessageW(window, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
    }
    if (message == WM_LBUTTONUP && g_frame != nullptr && g_frame->click != nullptr) {
        g_frame->click(static_cast<int>(LOWORD(lparam)), static_cast<int>(HIWORD(lparam)), g_frame->user);
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace

HWND CreateFrameWindow(const std::wstring& title, int width, int height) {
    RECT rect{ 0, 0, width, height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    return CreateWindowExW(
        0, L"RobixUIWindow", title.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
}

bool OpenWindow(const std::string& title, int width, int height, const std::vector<std::uint32_t>& pixels) {
    Frame frame;
    frame.pixels = pixels;
    frame.width = width;
    frame.height = height;
    g_frame = &frame;

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"RobixUIWindow";
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    RegisterClassW(&windowClass);

    const std::wstring wide = Wide(title);
    HWND window = CreateFrameWindow(wide, width, height);
    if (window == nullptr) {
        g_frame = nullptr;
        return false;
    }
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    MSG message;
    const DWORD started = GetTickCount();
    while (!frame.painted && GetTickCount() - started < 2000) {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) break;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (IsWindow(window)) DestroyWindow(window);
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) break;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    g_frame = nullptr;
    if (frame.painted) std::cout << "framebuffer " << width << "x" << height << "\n";
    return frame.painted;
}

bool OpenWindowDev(const std::string& title, std::vector<std::uint32_t> pixels, int width, int height,
                   bool (*refresh)(std::vector<std::uint32_t>& pixels, int& width, int& height, std::string& title, void* user),
                   bool (*resize)(std::vector<std::uint32_t>& pixels, int& width, int& height, std::string& title, void* user),
                   void (*click)(int x, int y, void* user), void* user) {
    Frame frame;
    frame.pixels = std::move(pixels);
    frame.width = width;
    frame.height = height;
    frame.refresh = refresh;
    frame.resize = resize;
    frame.click = click;
    frame.user = user;
    g_frame = &frame;

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"RobixUIWindow";
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    RegisterClassW(&windowClass);

    std::wstring wide = Wide(title);
    HWND window = CreateFrameWindow(wide, frame.width, frame.height);
    if (window == nullptr) {
        g_frame = nullptr;
        return false;
    }
    frame.window = window;
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    std::cout << "framebuffer " << frame.width << "x" << frame.height << "\n";
    std::cout << "window: open " << frame.width << "x" << frame.height << " title " << title << "\n";
    std::cout << "window: development watching\n";
    std::cout.flush();

    std::string shown = title;
    DWORD last = GetTickCount();
    MSG message;
    while (true) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                g_frame = nullptr;
                std::cout << "window: closed\n";
                return frame.painted;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (refresh != nullptr && GetTickCount() - last > 300) {
            last = GetTickCount();
            std::vector<std::uint32_t> next = frame.pixels;
            int nextW = frame.width;
            int nextH = frame.height;
            std::string nextTitle = shown;
            if (refresh(next, nextW, nextH, nextTitle, user)) {
                frame.pixels = std::move(next);
                if (nextW != frame.width || nextH != frame.height) {
                    frame.width = nextW;
                    frame.height = nextH;
                    RECT rect{ 0, 0, frame.width, frame.height };
                    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
                    frame.applying = true;
                    SetWindowPos(window, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                                 SWP_NOMOVE | SWP_NOZORDER);
                    frame.applying = false;
                }
                if (nextTitle != shown) {
                    shown = nextTitle;
                    SetWindowTextW(window, Wide(shown).c_str());
                }
                InvalidateRect(window, nullptr, FALSE);
            }
        }
        Sleep(16);
    }
}

POINT PlaceOf(const ScreenHost& host, int width, int height) {
    if (host.fullscreen || host.state == "maximized") return {0, 0};
    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    std::string place = host.place;
    for (char& ch : place) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (place == "top") return {(screenW - width) / 2, 0};
    if (place == "bottom") return {(screenW - width) / 2, screenH - height};
    if (place == "left") return {0, (screenH - height) / 2};
    if (place == "right") return {screenW - width, (screenH - height) / 2};
    if (place == "top-left") return {0, 0};
    if (place == "top-right") return {screenW - width, 0};
    if (place == "bottom-left") return {0, screenH - height};
    if (place == "bottom-right") return {screenW - width, screenH - height};
    if (place == "absolute") return {host.x, host.y};
    return {(screenW - width) / 2, (screenH - height) / 2};
}

bool OpenScreen(const ScreenHost& host) {
    if (host.state == "closed") {
        std::cout << "screen state closed\n";
        return true;
    }
    int width = host.fullscreen ? GetSystemMetrics(SM_CXSCREEN) : host.width;
    int height = host.fullscreen ? GetSystemMetrics(SM_CYSCREEN) : host.height;
    const POINT at = PlaceOf(host, width, height);

    Frame frame;
    frame.pixels = host.pixels;
    frame.width = host.width;
    frame.height = host.height;
    frame.movable = host.movable;
    frame.buttons = host.buttons;
    g_frame = &frame;

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"RobixUIWindow";
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    RegisterClassW(&windowClass);

    DWORD style = WS_POPUP;
    if (host.resizable && !host.fullscreen && host.state == "normal") style |= WS_THICKFRAME;
    const DWORD extra = (host.topmost || host.fullscreen) ? WS_EX_TOPMOST : 0;
    HWND window = CreateWindowExW(
        extra, L"RobixUIWindow", Wide(host.title).c_str(), style,
        at.x, at.y, width, height, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (window == nullptr) {
        g_frame = nullptr;
        return false;
    }
    if (host.state == "maximized") ShowWindow(window, SW_MAXIMIZE);
    else if (host.state == "minimized") ShowWindow(window, SW_SHOWMINIMIZED);
    else ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    std::cout << "screen place " << host.place << " " << at.x << " " << at.y << " " << width << "x" << height << "\n";
    std::cout << "screen state " << host.state << " movable " << (host.movable ? "true" : "false")
              << " resizable " << (host.resizable ? "true" : "false") << "\n";

    if (host.state == "minimized") {
        if (IsWindow(window)) DestroyWindow(window);
        g_frame = nullptr;
        return true;
    }
    MSG message;
    const DWORD started = GetTickCount();
    while (!frame.painted && GetTickCount() - started < 2000) {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) break;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (IsWindow(window)) DestroyWindow(window);
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) break;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    g_frame = nullptr;
    if (frame.painted) std::cout << "framebuffer " << host.width << "x" << host.height << "\n";
    return frame.painted;
}

} // namespace Robix::UI::Platform::Windows
