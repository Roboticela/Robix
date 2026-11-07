#pragma once

#include "Platform/Windows/WindowsHost.hpp"

#include <string>
#include <vector>

namespace Robix::UI {

int RunUiCommand(const std::vector<std::string>& args);
void LoadScreenOptions(const std::string& document, WindowRunOptions& options);

}  // namespace Robix::UI
