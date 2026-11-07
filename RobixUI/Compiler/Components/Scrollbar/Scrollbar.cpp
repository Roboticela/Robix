#include "Scrollbar.hpp"

#include "../../RoboCompilerShared.hpp"

#include <sstream>
#include <string>

bool RoboCompilerIsScrollbarName(const std::string& name) {
  return RoboCompilerLower(name) == "scrollbar";
}
