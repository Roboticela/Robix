#pragma once

#include <string>

namespace Robix::UI {

struct WindowRunOptions {
    bool stayOpen = false;
};

/* Windows host. Direct3D presents the same back buffer the RefCode soft GPU paints. */
int RunWindowsFoundation(const WindowRunOptions& options);

/* Paint the foundation into a BMP. The first pixel is the foundation background. */
bool WriteFoundationBmp(const std::string& path, std::string& error);

}  // namespace Robix::UI
