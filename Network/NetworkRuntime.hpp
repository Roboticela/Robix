#pragma once

#include <string>
#include <vector>

namespace Robix::Network {
int RunNetworkCommand(const std::vector<std::string>& args);
}

// The optional network runtime. A user executable links this object only when
// its project file sets Network: true.
extern "C" {
__declspec(dllexport) int rbx_network_run(const char* serverEnvPath, const char* methodsPath);
}
