#pragma once

#include "Compiler/RoboXML.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace Robix::D3 {

struct Prepared {
    RoboXMLNode root;
    std::vector<std::string> errors;
    bool degrees = false;
    std::string units = "meters";
    std::string three = "0.170.0";
    std::string title = "Robix3D";
    std::filesystem::path directory;
};

Prepared Prepare(const std::filesystem::path& document);
std::vector<std::string> UiBoundaryErrors(const RoboXMLNode& root);
bool IsSceneDocument(const RoboXMLNode& root);

}  // namespace Robix::D3
