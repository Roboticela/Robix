#pragma once

#include <string>
#include <vector>

namespace Robix::Tools {

// Returns -1 when the command is not a Stage L tool.
int RunStageLCommand(const std::vector<std::string>& args, const std::string& toolDir);

} // namespace Robix::Tools
