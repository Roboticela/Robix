#include "Eyebrow.hpp"

#include "../../RoboCompilerShared.hpp"

bool RoboCompilerIsEyebrowName(const std::string& name) {
  const std::string n = RoboCompilerLower(name);
  return n == "eyebrow";
}
