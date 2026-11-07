#include "Paragraph.hpp"

#include "../../RoboCompilerShared.hpp"

bool RoboCompilerIsParagraphName(const std::string& name) {
  const std::string n = RoboCompilerLower(name);
  return n == "paragraph" || n == "p";
}
