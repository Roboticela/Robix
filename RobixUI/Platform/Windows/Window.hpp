#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Robix::UI::Platform::Windows {

struct ScreenHost {
    std::string title;
    std::string place = "center";
    std::string state = "normal";
    int x = 0;
    int y = 0;
    int width = 640;
    int height = 400;
    bool movable = true;
    bool resizable = true;
    bool topmost = false;
    bool fullscreen = false;
    std::vector<std::uint32_t> pixels;
    std::vector<int> buttons;
};

// Opens one Win32 window, paints one frame, and closes it.
bool OpenWindow(const std::string& title, int width, int height, const std::vector<std::uint32_t>& pixels);

// The screen is the window. place, move, and resize follow the Screen component.
bool OpenScreen(const ScreenHost& host);

// Keeps the window open until it is closed. refresh returns true when the frame changed.
bool OpenWindowDev(const std::string& title, std::vector<std::uint32_t> pixels, int width, int height,
                   bool (*refresh)(std::vector<std::uint32_t>& pixels, int& width, int& height, std::string& title, void* user),
                   bool (*resize)(std::vector<std::uint32_t>& pixels, int& width, int& height, std::string& title, void* user),
                   void (*click)(int x, int y, void* user), void* user);

}
