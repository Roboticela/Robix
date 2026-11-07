#include "Label.hpp"

#include "../../RoboCompilerShared.hpp"

bool RoboCompilerIsLabelName(const std::string& name) {
  const std::string n = RoboCompilerLower(name);
  return n == "label";
}
