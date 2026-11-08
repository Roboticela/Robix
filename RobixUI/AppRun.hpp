#pragma once

#include "Web/WebLanguage.hpp"

#include <string>
#include <vector>

namespace Robix::UI {

std::string HostTarget();
bool InterpreterCanRun(const std::string& target);
bool CompilerCanBuild(const std::string& target);
std::string NormalizeTarget(const std::string& target);

int RunInterpreter(const std::string& input, const std::string& target, bool watch, int port, const std::string& toolDirectory,
                   WebForm form = WebForm::Script);
int CompileProduction(const std::string& input, const std::vector<std::string>& targets, const std::string& toolDirectory,
                      WebForm form = WebForm::Script);

}  // namespace Robix::UI
