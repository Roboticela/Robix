#include "Heading.hpp"

#include "../../RoboCompilerShared.hpp"

bool RoboCompilerIsHeadingName(const std::string& name) {
  const std::string n = RoboCompilerLower(name);
  return n == "heading" || n == "h1" || n == "h2" || n == "h3" || n == "h4" || n == "h5" || n == "h6";
}
