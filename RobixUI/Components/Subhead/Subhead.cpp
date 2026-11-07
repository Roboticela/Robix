#include "Subhead.hpp"

#include "../../RoboCompilerShared.hpp"

bool RoboCompilerIsSubheadName(const std::string& name) {
  const std::string n = RoboCompilerLower(name);
  return n == "subhead";
}
