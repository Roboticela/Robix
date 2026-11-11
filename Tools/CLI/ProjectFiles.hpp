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
    std::string entry;
    bool requireTests = false;
    bool network = false;
};

struct RxbPlan {
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
    std::vector<std::string> ui;
};

// Returns 0 or 1 when args[0] is add, remove, update, install, or restore.
// Returns -1 for every other command.
int PackageCommand(const std::vector<std::string>& args);

// Empty when every source under the project's source directory has a sibling .test.rx,
// or when the project does not opt in. Otherwise a message naming the missing file.
std::string MissingTestMessage(const std::filesystem::path& start);

bool FindProject(const std::filesystem::path& start, ProjectFile& project);

// How `run` and `robixc` choose sources.
// Project: the folder is one program, so the build file's source (or the only program) is used.
// Each: every program in the folder, one by one. Tests, Build, and Packages are skipped.
// Network: the folder is a network project, so its clients and servers run.
// Absent: nothing in the folder can be chosen. `error` says why.
struct ProjectRun {
    enum class Mode { Project, Each, Network, Absent };
    Mode mode = Mode::Absent;
    std::filesystem::path root;
    std::filesystem::path buildFile;
    std::vector<std::filesystem::path> files;
    std::vector<std::filesystem::path> ui;
    std::vector<std::filesystem::path> order;
    std::string output = "Executable";
    std::string profile;
    std::string entrySymbol;
    std::string error;
};

// `folder` empty means the current directory. `eachFile` selects every program.
// `services` is for `robix run` on a network project. The compiler passes false.
ProjectRun ChooseProjectRun(const std::filesystem::path& folder, bool eachFile, bool services = true);

RxbPlan ReadRxb(const std::filesystem::path& file);
std::string ExplainRxb(const RxbPlan& plan);

// True when the output exists and the source and its imports still match the stamp.
bool IncrementalHit(const std::filesystem::path& source, const std::filesystem::path& output);
void WriteIncrementalStamp(const std::filesystem::path& source, const std::filesystem::path& output);

// One output per source. A project build file's order is that list.
// With no order, every program in the project is compiled on its own.
std::vector<std::filesystem::path> CompilerBuildSources(const ProjectRun& run);

// The program entry keeps the executable. Every other source is its own object.
// `all` treats every program as an entry.
bool CompilerBuildIsEntry(const ProjectRun& run, const std::filesystem::path& source);

} // namespace Robix::Tools
