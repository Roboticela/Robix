#pragma once

#include "../../Scene.hpp"

namespace Robix::UI {

struct WindowRunOptions {
    bool stayOpen = false;
    bool trace = false;
};

/* Windows screen host. It uses the same client messages as the RefCode window procedure. */
int RunWindowsScreen(Scene& scene, const WindowRunOptions& options);

}  // namespace Robix::UI
