#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct RoboImageVariant {
    std::string url;
    int width = 0;
};

/* Writes the original and, when resize is true, smaller widths beside it.
   Returns the files that were written, narrowest first. An empty result means the source was not written. */
std::vector<RoboImageVariant> RoboImageWriteSizes(const std::filesystem::path& source, const std::filesystem::path& folder,
                                                  const std::string& stem, bool resize);
