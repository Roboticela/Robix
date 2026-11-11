#pragma once

#include "RoboXML.hpp"

#include <string>
#include <vector>

enum class RoboCompilerMode { Development, Production };
enum class RoboCompilerTarget { RoboticelaOS, Web, Windows };

struct RoboCompilerDiagnostic {
  enum class Severity { Error, Warning } severity;
  std::string path;
  std::string message;
};

struct RoboCompilerOptions {
  std::string appRoot;     /* e.g. Desktop/ */
  std::string outputRoot;  /* e.g. Build/apps/Desktop */
  std::string componentsRoot; /* Compiler/Components */
  std::string assetsRoot;  /* repo Assets/ fallback */
  RoboCompilerMode mode = RoboCompilerMode::Production;
  RoboCompilerTarget target = RoboCompilerTarget::RoboticelaOS;
};

struct RoboCompilerResult {
  bool ok = false;
  std::vector<RoboCompilerDiagnostic> diagnostics;
  std::string generatedCPPPath;
  std::string screenIrPath;
  std::string videoAssetPath;
  std::string manifestPath;
};

RoboCompilerResult RoboCompilerBuild(const RoboCompilerOptions& options);
std::string RoboCompilerRouteFromPage(const std::string& relativeForward);
