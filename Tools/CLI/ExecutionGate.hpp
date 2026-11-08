#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Robix::Tools {

enum class RunSurface {
    Platform,
    Direct
};

struct CodeShape {
    bool platform = false;
    bool direct = false;
    std::string platformUse;
    std::string directUse;
};

[[nodiscard]] CodeShape ScanCodeShape(std::string_view source);

[[nodiscard]] std::string ProjectCategory(const std::filesystem::path& start);

[[nodiscard]] bool IsPlatformName(std::string_view target);

[[nodiscard]] bool IsDirectName(std::string_view target);

// Empty means the file may run on this surface. A mismatch is one direct error.
[[nodiscard]] std::string ExecutionError(const std::string& category,
                                         RunSurface surface,
                                         const std::string& targetName,
                                         const CodeShape& shape);

enum class AppForm {
    Console,
    Window,
    Network,
    Unknown
};

struct WebDecision {
    std::string error;
    std::string warning;
    std::string screen;
};

[[nodiscard]] std::string ProjectKind(const std::filesystem::path& start);

// Console refuses web. Window and Network allow web when a .rbxu screen exists.
// Canvas is classified from the file, then warned or refused the same way.
[[nodiscard]] WebDecision DecideWeb(const std::string& category,
                                    const std::string& kind,
                                    std::string_view source,
                                    const std::filesystem::path& file,
                                    bool targetIsWeb);

} // namespace Robix::Tools
