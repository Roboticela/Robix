#pragma once

#include <filesystem>
#include <string>

namespace Robix::UI {

std::string AssetsFontsRoot();

/* HTML, CSS, and JS for the foundation. Component sheets are added with each component. */
bool WriteWebFoundation(const std::filesystem::path& outRoot, const std::filesystem::path& document, std::string& error);

}  // namespace Robix::UI
