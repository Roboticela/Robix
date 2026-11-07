#include "Subhead.Web.hpp"

std::string RoboWebSubheadClass(const std::string& name) {
  if (name == "subhead") return "robo-subhead";
  return {};
}
