#pragma once

#include <cstdint>
#include <string>

namespace Robix::UI::Web {

struct DevPage {
    std::string html;
    int generation = 1;
};

// Writes the Window component as one HTML page. The attributes are the same
// names the platform window reads from a .rbxu document.
bool WriteWindow(const std::string& path, const std::string& name, const std::string& title,
                 int width, int height, const std::string& theme, std::uint32_t background, std::uint32_t foreground,
                 const std::string& body = {}, bool live = false);

std::string RenderWindow(const std::string& name, const std::string& title, int width, int height,
                         const std::string& theme, std::uint32_t background, std::uint32_t foreground,
                         const std::string& body, bool live, const std::string& fontPrefix = "Assets/Fonts");

// Serves the page at http://127.0.0.1 and reloads it while the process runs.
bool ServeDev(void (*provider)(DevPage& page, void* user), void* user);

}
