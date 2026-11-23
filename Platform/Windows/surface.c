/* Windows adapter. One window presents the universal RGBA buffer.
   Win32 is used for the window, the message loop, DPI, and the blit.
   No Win32 control stands in for an RXU control. */

#include "../../Runtime/Raster/rxu_raster.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static RxuRaster frame;

static LRESULT CALLBACK PaintWindow(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        BITMAPINFO info;
        memset(&info, 0, sizeof(info));
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = frame.width;
        info.bmiHeader.biHeight = -frame.height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        int count = frame.width * frame.height;
        unsigned char *swapped = (unsigned char *)malloc((size_t)count * 4u);
        if (swapped != NULL) {
            int index = 0;
            while (index < count) {
                unsigned char *src = frame.pixels + index * 4;
                unsigned char *dst = swapped + index * 4;
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = src[3];
                index = index + 1;
            }
            StretchDIBits(dc, 0, 0, frame.width, frame.height, 0, 0, frame.width, frame.height, swapped, &info, DIB_RGB_COLORS, SRCCOPY);
            free(swapped);
        }
        EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_LBUTTONDOWN) {
        printf("Pointer %d %d\n", (int)(short)LOWORD(lparam), (int)(short)HIWORD(lparam));
        fflush(stdout);
        return 0;
    }
    if (message == WM_LBUTTONUP) {
        printf("Pointer %d %d release\n", (int)(short)LOWORD(lparam), (int)(short)HIWORD(lparam));
        fflush(stdout);
        return 0;
    }
    if (message == WM_MOUSEMOVE) {
        printf("Pointer %d %d move\n", (int)(short)LOWORD(lparam), (int)(short)HIWORD(lparam));
        fflush(stdout);
        return 0;
    }
    if (message == WM_KEYDOWN) {
        printf("Key %u\n", (unsigned)wparam);
        fflush(stdout);
        return 0;
    }
    if (message == WM_KEYUP) {
        printf("KeyRelease %u\n", (unsigned)wparam);
        fflush(stdout);
        return 0;
    }
    if (message == WM_SETFOCUS) {
        printf("Focus\n");
        fflush(stdout);
        return 0;
    }
    if (message == WM_KILLFOCUS) {
        printf("Blur\n");
        fflush(stdout);
        return 0;
    }
    if (message == WM_MOUSEWHEEL) {
        printf("Scroll 0 %d\n", GET_WHEEL_DELTA_WPARAM(wparam) / 120);
        fflush(stdout);
        return 0;
    }
    if (message == WM_SIZE) {
        printf("Resize %d %d\n", LOWORD(lparam), HIWORD(lparam));
        fflush(stdout);
        return 0;
    }
    if (message == WM_DESTROY) {
        printf("Close\n");
        fflush(stdout);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(window, message, wparam, lparam);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "present failed\n");
        return 1;
    }
    FILE *file = fopen(argv[1], "rb");
    if (file == NULL) {
        fprintf(stderr, "present failed\n");
        return 1;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = (char *)malloc((size_t)length + 1u);
    if (text == NULL) {
        fclose(file);
        return 1;
    }
    size_t got = fread(text, 1, (size_t)length, file);
    fclose(file);
    text[got] = 0;
    if (!rxu_raster_text(text, 0, 0, 0, 0, &frame)) {
        fprintf(stderr, "present failed\n");
        free(text);
        return 1;
    }
    printf("HASH %016llx\n", (unsigned long long)frame.hash);
    printf("SCALE %d\n", GetDpiForSystem() / 96);
    fflush(stdout);
    HINSTANCE instance = GetModuleHandle(NULL);
    WNDCLASSW klass;
    memset(&klass, 0, sizeof(klass));
    klass.lpfnWndProc = PaintWindow;
    klass.hInstance = instance;
    klass.lpszClassName = L"RobixSurface";
    RegisterClassW(&klass);
    HWND window = CreateWindowExW(0, L"RobixSurface", L"Arm console", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, frame.width, frame.height, NULL, NULL, instance, NULL);
    if (window == NULL) {
        fprintf(stderr, "present failed\n");
        return 1;
    }
    ShowWindow(window, SW_SHOW);
    MSG message;
    while (GetMessage(&message, NULL, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    rxu_raster_free(&frame);
    free(text);
    return 0;
}
#else
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    /* This translation unit is the Windows window. Pixel identity is the shared rasterizer. */
    return 0;
}
#endif
