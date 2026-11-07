#include "LabelWeb.hpp"

std::string RoboWebLabelClass(const std::string& name) {
  if (name == "label") return "robo-label";
  return {};
}
