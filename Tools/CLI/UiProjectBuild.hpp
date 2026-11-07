#pragma once

#include <string>

namespace Robix::UI {

/* Production writes <project>/Build. Development runs the screen or the web host and does not write HTML. */
int BuildOrRunProject(const std::string& input, bool development, const std::string& target, const std::string& toolDirectory);

}  // namespace Robix::UI
