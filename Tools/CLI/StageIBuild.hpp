#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Robix::Tools {

struct ProjectFile {
    std::filesystem::path file;
    std::filesystem::path directory;
    std::string name;
    std::string version = "1.0.0";
    std::string signKey;
    std::string sourceDir = "Source";
    bool requireTests = false;
};

struct RbxbPlan {
    bool ok = false;
    std::string error;
    std::string output = "Executable";
    std::string target;
    std::string entry;
    std::string profile;
    std::string config = "release";
    std::string source;
    std::vector<std::string> sources;
    std::vector<std::string> order;
};

// Returns 0 or 1 when args[0] is add, remove, update, install, or restore.
// Returns -1 for every other command.
int PackageCommand(const std::vector<std::string>& args);

// Empty when every source under the project's source directory has a sibling .test.rbx,
// or when the project does not opt in. Otherwise a message naming the missing file.
std::string MissingTestMessage(const std::filesystem::path& start);

bool FindProject(const std::filesystem::path& start, ProjectFile& project);

RbxbPlan ReadRbxb(const std::filesystem::path& file);
std::string ExplainRbxb(const RbxbPlan& plan);

// True when the output exists and the source and its imports still match the stamp.
bool IncrementalHit(const std::filesystem::path& source, const std::filesystem::path& output);
void WriteIncrementalStamp(const std::filesystem::path& source, const std::filesystem::path& output);

} // namespace Robix::Tools
