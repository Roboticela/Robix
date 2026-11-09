#pragma once

#include "RoboCompiler.hpp"
#include "RoboFoundation.hpp"
#include "RoboXML.hpp"

#include <filesystem>
#include <string>
#include <unordered_set>

struct RoboWebPackage {
  const RoboCompilerOptions* options = nullptr;
  RoboCompilerResult* result = nullptr;
  std::unordered_set<std::string> copied;
  std::unordered_set<std::string> used;
  std::unordered_set<std::string> routes;
};

void RoboWebNote(RoboCompilerResult& result, bool error, const std::string& path, const std::string& message);
std::string RoboWebRead(const std::filesystem::path& path);
bool RoboWebWrite(const std::filesystem::path& path, const std::string& body);
std::string RoboWebLower(std::string text);
std::string RoboWebEscape(const std::string& text);
std::string RoboWebColor(const std::string& value);
bool RoboWebIsNumber(const std::string& text);
const RoboFoundationToken* RoboWebToken(const RoboFoundationLibrary& library, const std::string& name);
std::string RoboWebCssValue(const RoboFoundationLibrary& library, const std::string& raw, bool color);
std::string RoboWebAlign(const std::string& value);
void RoboWebAddDecl(std::string& style, const char* property, const std::string& value);
std::string RoboWebStyle(const RoboXMLNode& node, const RoboFoundationLibrary& library);
std::string RoboWebPaintMode(const RoboXMLNode& node);
std::string RoboWebBleedDistance(const RoboXMLNode& node);
std::string RoboWebPaintAttrs(const RoboXMLNode& node);
void RoboWebUseDocument(const std::filesystem::path& document);
std::string RoboWebSvg(const std::filesystem::path& assetsRoot, const std::string& name);
std::string RoboWebIconMarkup(const std::filesystem::path& assetsRoot, const RoboXMLNode& node);
void RoboWebNoteFamily(RoboWebPackage* package, const std::string& name);
std::string RoboWebCopyAsset(RoboWebPackage& package, const std::string& src, const std::string& webPath);
void RoboWebEmit(std::string& html, const RoboXMLNode& node, const RoboFoundationLibrary& library,
                 const std::string& source, const std::filesystem::path& assetsRoot, bool root, bool& warnedVideo,
                 RoboWebPackage* package, int depth);
