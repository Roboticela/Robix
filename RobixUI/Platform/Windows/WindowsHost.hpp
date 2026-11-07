#pragma once

#include <string>

namespace Robix::UI {

struct WindowRunOptions {
    bool stayOpen = false;
    bool screen = false;
    bool light = false;
    bool movable = true;
    bool resizable = true;
    int width = 960;
    int height = 640;
    int x = 0;
    int y = 0;
    std::string place = "center";
    std::string title = "Robix";
    bool (*watch)(WindowRunOptions& options, void* user) = nullptr;
    void* watchUser = nullptr;
};

/* Windows host. Direct3D presents the same back buffer the RefCode soft GPU paints. */
int RunWindowsFoundation(const WindowRunOptions& options);

/* Paint the foundation into a BMP. The first pixel is the foundation background. */
bool WriteFoundationBmp(const std::string& path, std::string& error);

}  // namespace Robix::UI
