#include "WindowsHost.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

namespace Robix::UI {
namespace {

struct HostView {
    Scene* scene = nullptr;
    WindowRunOptions options;
    Robo::FrameBuffer buffer;
    bool painted = false;
    bool trackLeave = false;
    bool dragging = false;
    int grabX = 0;
    int grabY = 0;
    int wheel = 0;
    int mouseX = 0;
    int mouseY = 0;
    bool mouseDown = false;
    UINT dpi = 96;
};

void Trace(HostView* view, const char* text) {
    if (view != nullptr && view->options.trace) {
        std::printf("%s\n", text);
    }
}

void Present(HWND window, HostView* view) {
    if (view == nullptr || view->buffer.GetPixels() == nullptr) {
        return;
    }
    const int width = view->buffer.GetWidth();
    const int height = view->buffer.GetHeight();
    std::vector<unsigned char> bgra(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    const unsigned char* source = view->buffer.GetPixels();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t index = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4u;
            bgra[index] = source[index + 2];
            bgra[index + 1] = source[index + 1];
            bgra[index + 2] = source[index];
            bgra[index + 3] = source[index + 3];
        }
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    StretchDIBits(dc, 0, 0, width, height, 0, 0, width, height, bgra.data(), &info, DIB_RGB_COLORS, SRCCOPY);
    EndPaint(window, &paint);
}

LRESULT CALLBACK ScreenProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        auto* created = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* view = static_cast<HostView*>(created->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
    }
    auto* view = reinterpret_cast<HostView*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_GETDLGCODE) {
        return DLGC_WANTALLKEYS | DLGC_WANTTAB | DLGC_WANTARROWS | DLGC_WANTCHARS;
    }
    if (message == WM_DPICHANGED && view != nullptr) {
        view->dpi = HIWORD(wParam);
        auto* suggested = reinterpret_cast<RECT*>(lParam);
        if (suggested != nullptr) {
            SetWindowPos(window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        Trace(view, "dpi");
        return 0;
    }
    if (message == WM_SETCURSOR && view != nullptr && LOWORD(lParam) == HTCLIENT) {
        SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)));
        return TRUE;
    }
    if ((message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK || message == WM_LBUTTONUP ||
         message == WM_MOUSELEAVE) &&
        view != nullptr) {
        if (message == WM_MOUSEMOVE && !view->trackLeave) {
            TRACKMOUSEEVENT track{};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = window;
            TrackMouseEvent(&track);
            view->trackLeave = true;
        }
        view->mouseX = static_cast<int>(static_cast<short>(LOWORD(lParam)));
        view->mouseY = static_cast<int>(static_cast<short>(HIWORD(lParam)));
        if (message == WM_MOUSELEAVE) {
            view->trackLeave = false;
            Trace(view, "leave");
        } else if (message == WM_LBUTTONDOWN) {
            view->mouseDown = true;
            SetFocus(window);
            if (view->scene != nullptr && view->scene->screen.movable) {
                POINT cursor{};
                GetCursorPos(&cursor);
                RECT frame{};
                GetWindowRect(window, &frame);
                view->dragging = true;
                view->grabX = cursor.x - frame.left;
                view->grabY = cursor.y - frame.top;
                SetCapture(window);
            }
            Trace(view, "down");
        } else if (message == WM_MOUSEMOVE && view->dragging) {
            POINT cursor{};
            GetCursorPos(&cursor);
            SetWindowPos(window, nullptr, cursor.x - view->grabX, cursor.y - view->grabY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            Trace(view, "move");
        } else if (message == WM_LBUTTONDBLCLK) {
            Trace(view, "dblclick");
        } else if (message == WM_LBUTTONUP) {
            view->mouseDown = false;
            view->dragging = false;
            if (GetCapture() == window) {
                ReleaseCapture();
            }
            Trace(view, "up");
        } else {
            Trace(view, "move");
        }
        return 0;
    }
    if (message == WM_KEYDOWN && view != nullptr) {
        if (wParam == VK_TAB) {
            Trace(view, "keydown Tab");
            return 0;
        }
        if (wParam == VK_RETURN || wParam == VK_SPACE) {
            Trace(view, wParam == VK_RETURN ? "keydown Enter" : "keydown Space");
            return 0;
        }
    }
    if (message == WM_KEYUP && view != nullptr && (wParam == VK_RETURN || wParam == VK_SPACE)) {
        Trace(view, "keyup");
        return 0;
    }
    if (message == WM_MOUSEWHEEL && view != nullptr) {
        view->wheel += GET_WHEEL_DELTA_WPARAM(wParam);
        if (view->options.trace) {
            std::printf("wheel %d\n", GET_WHEEL_DELTA_WPARAM(wParam));
        }
        return 0;
    }
    if (message == WM_SIZE && view != nullptr && view->scene != nullptr && wParam != SIZE_MINIMIZED) {
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);
        if (width > 0 && height > 0) {
            view->scene->screen.width = width;
            view->scene->screen.height = height;
            LayoutScene(*view->scene);
            PaintScene(*view->scene, view->buffer);
            if (view->options.trace) {
                std::printf("framebuffer resized %dx%d\n", width, height);
            }
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    }
    if (message == WM_ERASEBKGND) {
        return 1;
    }
    if (message == WM_PAINT) {
        if (view != nullptr && view->scene != nullptr && !view->painted) {
            PaintScene(*view->scene, view->buffer);
        }
        Present(window, view);
        if (view != nullptr) {
            view->painted = true;
            if (!view->options.stayOpen) {
                PostQuitMessage(0);
            }
        }
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace

int RunWindowsScreen(Scene& scene, const WindowRunOptions& options) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HostView view;
    view.scene = &scene;
    view.options = options;
    view.dpi = GetDpiForSystem();
    PaintScene(scene, view.buffer);

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = ScreenProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"RobixScreen";
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClassW(&windowClass);

    const DWORD style = scene.screen.resizable ? (WS_POPUP | WS_THICKFRAME) : WS_POPUP;
    const DWORD exStyle = (scene.screen.topmost ? WS_EX_TOPMOST : 0) | (scene.screen.transparent ? WS_EX_LAYERED : 0);
    RECT bounds{0, 0, scene.screen.width, scene.screen.height};
    if (scene.screen.fullscreen) {
        bounds.right = GetSystemMetrics(SM_CXSCREEN);
        bounds.bottom = GetSystemMetrics(SM_CYSCREEN);
        scene.screen.width = bounds.right;
        scene.screen.height = bounds.bottom;
        LayoutScene(scene);
        PaintScene(scene, view.buffer);
    }
    AdjustWindowRectExForDpi(&bounds, style, FALSE, exStyle, view.dpi);
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int outerW = bounds.right - bounds.left;
    const int outerH = bounds.bottom - bounds.top;
    int originX = scene.screen.x;
    int originY = scene.screen.y;
    if (scene.screen.place == "center" || scene.screen.place.empty()) {
        originX = work.left + ((work.right - work.left) - outerW) / 2;
        originY = work.top + ((work.bottom - work.top) - outerH) / 2;
    }
    std::wstring title(scene.screen.name.begin(), scene.screen.name.end());
    if (title.empty()) {
        title = L"Robix";
    }
    HWND window = CreateWindowExW(exStyle, windowClass.lpszClassName, title.c_str(), style, originX, originY, outerW, outerH,
                                  nullptr, nullptr, instance, &view);
    if (window == nullptr) {
        std::fprintf(stderr, "window: could not open the screen\n");
        return 1;
    }
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    RECT placed{};
    GetWindowRect(window, &placed);
    std::printf("component Screen\n");
    std::printf("screen place %s %ld %ld %dx%d\n", scene.screen.place.c_str(), placed.left, placed.top, scene.screen.width,
                scene.screen.height);
    std::printf("screen state %s movable %s resizable %s\n", scene.screen.state.c_str(), scene.screen.movable ? "true" : "false",
                scene.screen.resizable ? "true" : "false");
    std::printf("%s\n", scene.foundationLine.c_str());
    std::printf("framebuffer %dx%d\n", view.buffer.GetWidth(), view.buffer.GetHeight());
    std::printf("renderer platform-framebuffer\n");
    std::printf("window: shown %dx%d title %s\n", scene.screen.width, scene.screen.height,
                scene.screen.name.empty() ? "Robix" : scene.screen.name.c_str());
    for (const std::string& line : scene.layoutLines) {
        std::printf("%s\n", line.c_str());
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    std::printf("window: closed after paint\n");
    return 0;
}

}  // namespace Robix::UI
