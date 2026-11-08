#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Robix::UI {

enum class WebForm { Script, Assembly };

/* script is HTML, CSS, and JS. assembly is HTML, CSS, and WebAssembly. */
WebForm ParseWebForm(const std::string& text, bool& recognized);
const char* WebFormName(WebForm form);

struct WebBuildResult {
    bool ok = false;
    std::string error;
    std::string script;
    std::vector<std::uint8_t> wasm;
};

/* Convert one Robix program. Warnings for features the browser cannot run are printed here. */
WebBuildResult BuildWebProgram(const std::filesystem::path& source, WebForm form, const std::string& toolDirectory);

/* The .rbx named by the project build file, or the program beside the screen. */
std::filesystem::path FindWebSource(const std::filesystem::path& document);

}  // namespace Robix::UI
