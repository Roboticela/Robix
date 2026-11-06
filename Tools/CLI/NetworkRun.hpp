#pragma once

#include <string>
#include <vector>

namespace Robix::Tools {

// Returns -1 when the command is not a network-project run.
// Otherwise 0 or 1.
int RunProjectServices(const std::vector<std::string>& args, const std::string& toolDir);

}
