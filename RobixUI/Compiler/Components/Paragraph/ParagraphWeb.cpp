#include "ParagraphWeb.hpp"

std::string RoboWebParagraphClass(const std::string& name) {
  if (name == "paragraph") return "robo-paragraph";
  if (name == "p") return "robo-p";
  return {};
}
