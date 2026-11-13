#pragma once

#include "../../Compiler/RoboXML.hpp"

#include <filesystem>

namespace Robix::UI {

/* Start or keep the document's audio. Sound has no box, so this is separate from the painter. */
void WindowsAudioSync(const RoboXMLNode& root, const std::filesystem::path& directory);

}
