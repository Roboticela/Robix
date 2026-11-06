#pragma once

#include <string>
#include <vector>

namespace Robix::UI {

// `robix ui validate` and `robix ui render`.
// Returns 0 or 1 when args[0] is "ui". Returns -1 otherwise.
int RunUiCommand(const std::vector<std::string>& args);

} // namespace Robix::UI
