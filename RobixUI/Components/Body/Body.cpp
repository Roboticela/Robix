#include "Body.hpp"

#include "../../RoboCompilerShared.hpp"

bool RoboCompilerIsBodyName(const std::string& name) {
  const std::string n = RoboCompilerLower(name);
  return n == "body";
}
