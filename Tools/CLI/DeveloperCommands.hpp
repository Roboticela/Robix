#pragma once

#include <string>
#include <vector>

namespace Robix::Tools {

// Returns -1 when the command is not format, lint, check, doc, debug, lsp, or repl.
int RunDeveloperCommand(const std::vector<std::string>& args, const std::string& toolDir);

} // namespace Robix::Tools
