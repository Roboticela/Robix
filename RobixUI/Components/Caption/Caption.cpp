#include "Caption.hpp"

#include "../../RoboCompilerShared.hpp"

bool RoboCompilerIsCaptionName(const std::string& name) {
  const std::string n = RoboCompilerLower(name);
  return n == "caption";
}
