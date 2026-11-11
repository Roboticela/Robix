#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct RoboXMLNode;

namespace Robix::D3 {

void Reset();
void UseDocument(const std::filesystem::path& document, const std::filesystem::path& outRoot);
std::string ModuleSource();
std::string CssSource();
void Restore(const std::string& module, const std::string& css);

/* True when the document root is Robix3D. */
bool IsSceneDocument(const RoboXMLNode& root);

/* A RobixUI tree may host a scene only through View3D Source="file.rx3d". */
std::vector<std::string> UiBoundaryErrors(const RoboXMLNode& root);

/* Writes canvas markup for one hosted scene. The scene file is compiled into the page module. */
void EmitView(std::string& html, const RoboXMLNode& node);

/* A whole .rx3d document becomes index.html, app.css, robix3d.js, and scene.js. */
bool WritePage(const std::filesystem::path& outRoot, const std::filesystem::path& document, std::string& error);

const std::vector<std::string>& Errors();
bool CommitViews(const std::filesystem::path& outRoot, std::string& error);

}  // namespace Robix::D3
