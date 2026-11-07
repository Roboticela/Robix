#include "HeadingWeb.hpp"

std::string RoboWebHeadingClass(const std::string& name) {
  if (name == "heading") return "robo-heading";
  if (name.size() == 2 && name[0] == 'h' && name[1] >= '1' && name[1] <= '6') return "robo-" + name;
  return {};
}
