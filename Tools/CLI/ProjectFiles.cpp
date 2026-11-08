#include "ProjectFiles.hpp"

#include "../../StdLib/Security/SecurityAndNetwork.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace fs = std::filesystem;

namespace Robix::Tools {

namespace {

std::string Trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

std::string ReadFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

bool WriteFile(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

std::string HashFile(const fs::path& path) {
    return Robix::StdLib::CryptoEngine::ComputeSHA256Hex(ReadFile(path));
}

std::string SignatureFor(const std::string& key, const std::string& name, const std::string& version, const std::string& sha) {
    return Robix::StdLib::CryptoEngine::ComputeHMACSHA256Hex(key, name + "@" + version + ":" + sha);
}

#ifdef _WIN32
bool DownloadHttp(const std::string& url, std::string& body, std::string& error) {
    const std::wstring wide(url.begin(), url.end());
    URL_COMPONENTS parts{};
    wchar_t host[256]{};
    wchar_t path[1024]{};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts)) {
        error = "could not parse " + url;
        return false;
    }
    HINTERNET session = WinHttpOpen(L"robix", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0);
    if (!session) {
        error = "WinHTTP did not open";
        return false;
    }
    HINTERNET connect = WinHttpConnect(session, host, parts.nPort, 0);
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, 0) : nullptr;
    const bool sent = request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) &&
                      WinHttpReceiveResponse(request, nullptr);
    if (!sent) {
        error = "download failed for " + url;
        if (request) WinHttpCloseHandle(request);
        if (connect) WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        error = "download status " + std::to_string(status) + " for " + url;
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) break;
        if (available == 0) break;
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read)) break;
        chunk.resize(read);
        body += chunk;
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return true;
}
#endif

struct LockedPackage {
    std::string name;
    std::string version = "1.0.0";
    std::string path;
    std::string sha256;
    std::string signature;
};

std::vector<LockedPackage> ReadLock(const fs::path& file) {
    std::vector<LockedPackage> packages;
    std::ifstream in(file);
    if (!in) return packages;
    LockedPackage current;
    auto push = [&]() {
        if (!current.name.empty()) packages.push_back(current);
        current = {};
    };
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty()) {
            push();
            continue;
        }
        const auto space = line.find(' ');
        if (space == std::string::npos) continue;
        const std::string key = line.substr(0, space);
        const std::string value = Trim(line.substr(space + 1));
        if (key == "name") {
            if (!current.name.empty()) push();
            current.name = value;
        } else if (key == "version") current.version = value;
        else if (key == "path") current.path = value;
        else if (key == "sha256") current.sha256 = value;
        else if (key == "signature") current.signature = value;
    }
    push();
    return packages;
}

bool WriteLock(const fs::path& file, const std::vector<LockedPackage>& packages) {
    std::ostringstream out;
    for (const auto& package : packages) {
        out << "name " << package.name << "\n"
            << "version " << package.version << "\n"
            << "path " << package.path << "\n"
            << "sha256 " << package.sha256 << "\n"
            << "signature " << package.signature << "\n\n";
    }
    return WriteFile(file, out.str());
}

fs::path LockPath(const ProjectFile& project) {
    return project.directory / (project.name + ".rbxlock");
}

bool LoadKeys(const fs::path& file, ProjectFile& project) {
    std::ifstream in(file);
    if (!in) return false;
    project.file = file;
    project.directory = file.parent_path();
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, colon));
        const std::string value = Trim(line.substr(colon + 1));
        if (key == "Name") project.name = value;
        else if (key == "Version") project.version = value;
        else if (key == "SignKey") project.signKey = value;
        else if (key == "SourceDir") project.sourceDir = value;
        else if (key == "RequireTests") project.requireTests = value == "true";
        else if (key == "Network") project.network = value == "true";
    }
    if (project.name.empty()) project.name = file.stem().string();
    return true;
}

std::string ArgValue(const std::vector<std::string>& args, const std::string& flag) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) return args[i + 1];
    }
    return {};
}

std::string Positional(const std::vector<std::string>& args, std::size_t nth) {
    std::size_t seen = 0;
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i].empty() || args[i][0] == '-') {
            if (args[i] == "--project" || args[i] == "--version") ++i;
            continue;
        }
        if (seen == nth) return args[i];
        ++seen;
    }
    return {};
}

std::string PublicApi(const std::string& packageName, const std::string& source) {
    std::ostringstream out;
    out << "package " << packageName << ";\n\n";
    std::istringstream lines(source);
    std::string line;
    while (std::getline(lines, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.rfind("public Function ", 0) != 0 && trimmed.rfind("public Int ", 0) != 0) continue;
        std::string signature = trimmed;
        const auto brace = signature.find('{');
        if (brace != std::string::npos) signature = Trim(signature.substr(0, brace));
        if (!signature.empty() && signature.back() == ';') signature.pop_back();
        out << signature << ";\n";
    }
    return out.str();
}

std::vector<fs::path> ImportPaths(const fs::path& source) {
    std::vector<fs::path> found;
    std::istringstream lines(ReadFile(source));
    std::string line;
    while (std::getline(lines, line)) {
        line = Trim(line);
        if (line.rfind("import ", 0) != 0) continue;
        std::string name = Trim(line.substr(7));
        const auto semi = name.find(';');
        if (semi != std::string::npos) name = Trim(name.substr(0, semi));
        const auto dot = name.rfind('.');
        const std::string last = dot == std::string::npos ? name : name.substr(dot + 1);
        std::string slashed = name;
        for (char& ch : slashed) {
            if (ch == '.') ch = '/';
        }
        const fs::path base = source.parent_path();
        const fs::path candidates[] = {
            base / (last + ".rbx"),
            base / (slashed + ".rbx"),
            fs::path(slashed + ".rbx")
        };
        for (const auto& candidate : candidates) {
            std::error_code ec;
            if (fs::is_regular_file(candidate, ec)) {
                found.push_back(fs::absolute(candidate).lexically_normal());
                break;
            }
        }
    }
    return found;
}

fs::path StampPath(const fs::path& source) {
    std::string name = fs::absolute(source).lexically_normal().string();
    for (char& ch : name) {
        if (ch == ':' || ch == '\\' || ch == '/' || ch == ' ') ch = '_';
    }
    return fs::path("Build/incremental") / (name + ".stamp");
}

} // namespace

bool IsTestSource(const fs::path& file) {
    const std::string stem = file.stem().string();
    return stem.size() > 5 && stem.ends_with(".test");
}

bool SkipProjectDir(const std::string& name) {
    return name == "Build" || name == "Packages" || name == "Tests" || name == ".git";
}

void CollectPrograms(const fs::path& dir, int depth, std::vector<fs::path>& files) {
    if (depth > 5) return;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_directory(ec)) {
            if (SkipProjectDir(entry.path().filename().string())) continue;
            CollectPrograms(entry.path(), depth + 1, files);
            continue;
        }
        if (entry.path().extension() == ".rbx" && !IsTestSource(entry.path())) files.push_back(entry.path());
    }
}

fs::path EntryInProject(const fs::path& root, const std::vector<fs::path>& files) {
    std::error_code ec;
    if (fs::is_directory(root, ec)) {
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (entry.path().extension() != ".rbxb") continue;
            const RbxbPlan plan = ReadRbxb(entry.path());
            if (!plan.ok || plan.source.empty()) continue;
            fs::path source(plan.source);
            if (source.is_relative()) source = root / source;
            if (fs::is_regular_file(source, ec)) return source;
        }
    }
    const char* names[] = { "Source/Program.rbx", "Source/Main.rbx", "Source/Start.rbx",
                            "Program.rbx", "Main.rbx", "Start.rbx" };
    for (const char* name : names) {
        const fs::path candidate = root / name;
        if (fs::is_regular_file(candidate, ec)) return candidate;
    }
    if (files.size() == 1) return files.front();
    return {};
}

void AttachBuild(ProjectRun& run) {
    std::error_code ec;
    if (!fs::is_directory(run.root, ec)) return;
    for (const auto& entry : fs::directory_iterator(run.root, ec)) {
        if (entry.path().extension() != ".rbxb") continue;
        const RbxbPlan plan = ReadRbxb(entry.path());
        if (!plan.ok) continue;
        run.buildFile = entry.path();
        run.output = plan.output;
        run.profile = plan.profile;
        run.entrySymbol = plan.entry;
        const std::vector<std::string>& listed = plan.order.empty() ? plan.sources : plan.order;
        for (const std::string& item : listed) {
            fs::path source(item);
            if (source.is_relative()) source = run.root / source;
            if (fs::is_regular_file(source, ec)) run.order.push_back(source);
        }
        if (run.order.empty() && !plan.source.empty()) {
            fs::path source(plan.source);
            if (source.is_relative()) source = run.root / source;
            if (fs::is_regular_file(source, ec)) run.order.push_back(source);
        }
        for (const std::string& item : plan.ui) {
            fs::path screen(item);
            if (screen.is_relative()) screen = run.root / screen;
            if (fs::is_regular_file(screen, ec)) run.ui.push_back(screen);
        }
        return;
    }
}

bool ProjectHere(const fs::path& dir, ProjectFile& project) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return false;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.path().extension() == ".rbxproj") return FindProject(entry.path(), project);
    }
    return false;
}

ProjectRun ChooseProjectRun(const fs::path& folder, bool eachFile, bool services) {
    ProjectRun run;
    const fs::path start = folder.empty() ? fs::current_path() : folder;
    ProjectFile project;
    const bool namedFolder = !folder.empty();
    const bool have = namedFolder ? ProjectHere(start, project) : FindProject(start, project);
    if (services && have && project.network) {
        std::error_code ec;
        const bool thisFolder = !namedFolder || (fs::exists(start, ec) && fs::equivalent(start, project.directory, ec));
        if (thisFolder || eachFile) {
            run.mode = ProjectRun::Mode::Network;
            run.root = project.directory;
            AttachBuild(run);
            return run;
        }
    }
    const fs::path root = have ? project.directory : start;
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        run.error = "robix: '" + start.string() + "' is not a project folder.\n";
        return run;
    }
    if (!have) {
        bool any = false;
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (entry.path().extension() == ".rbx" && !IsTestSource(entry.path())) any = true;
            if (entry.path().filename() == "Source" && entry.is_directory(ec)) any = true;
        }
        if (!any) {
            run.error = "robix: '" + root.string() + "' has no project and no program.\n";
            return run;
        }
    }
    std::vector<fs::path> files;
    if (have) CollectPrograms(root, 0, files);
    else {
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (entry.path().extension() == ".rbx" && !IsTestSource(entry.path())) files.push_back(entry.path());
        }
        if (fs::is_directory(root / "Source", ec)) CollectPrograms(root / "Source", 0, files);
    }
    std::sort(files.begin(), files.end());
    if (eachFile) {
        if (files.empty()) {
            run.error = "robix: '" + root.string() + "' has no programs to run.\n";
            return run;
        }
        run.mode = ProjectRun::Mode::Each;
        run.root = root;
        run.files = std::move(files);
        return run;
    }
    const fs::path entry = EntryInProject(have ? root : root, files);
    if (entry.empty()) {
        run.error = files.empty()
                        ? "robix: '" + root.string() + "' has no .rbx program.\n"
                        : "robix: this folder has more than one program. Name a file, or run all.\n";
        return run;
    }
    run.mode = ProjectRun::Mode::Project;
    run.root = root;
    AttachBuild(run);
    run.files.push_back(entry);
    return run;
}

bool FindProject(const fs::path& start, ProjectFile& project) {
    std::error_code ec;
    fs::path cursor = fs::absolute(start, ec);
    if (fs::is_regular_file(cursor, ec) && cursor.extension() == ".rbxproj") {
        return LoadKeys(cursor, project);
    }
    if (fs::is_regular_file(cursor, ec)) cursor = cursor.parent_path();
    for (int depth = 0; depth < 6 && !cursor.empty(); ++depth) {
        if (fs::is_directory(cursor, ec)) {
            for (const auto& entry : fs::directory_iterator(cursor, ec)) {
                if (entry.path().extension() == ".rbxproj") return LoadKeys(entry.path(), project);
            }
        }
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor) break;
        cursor = cursor.parent_path();
    }
    return false;
}

std::string MissingTestMessage(const fs::path& start) {
    ProjectFile project;
    if (!FindProject(start, project) || !project.requireTests) return {};
    const fs::path dir = project.directory / project.sourceDir;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return {};
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.path().extension() != ".rbx") continue;
        const std::string stem = entry.path().stem().string();
        if (stem.size() > 5 && stem.ends_with(".test")) continue;
        const fs::path test = entry.path().parent_path() / (stem + ".test.rbx");
        if (!fs::is_regular_file(test, ec)) {
            return "robix: required test is missing: " + test.string() + "\n";
        }
    }
    return {};
}

RbxbPlan ReadRbxb(const fs::path& file) {
    RbxbPlan plan;
    std::ifstream in(file);
    if (!in) {
        plan.error = "robixc: could not open '" + file.string() + "'\n";
        return plan;
    }
    auto takeList = [](const std::string& value) {
        std::vector<std::string> items;
        std::string current;
        for (char ch : value) {
            if (ch == ',') {
                current = Trim(current);
                if (!current.empty()) items.push_back(current);
                current.clear();
            } else {
                current.push_back(ch);
            }
        }
        current = Trim(current);
        if (!current.empty()) items.push_back(current);
        return items;
    };
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        const auto hash = line.find('#');
        if (hash != std::string::npos) line = Trim(line.substr(0, hash));
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, colon));
        const std::string value = Trim(line.substr(colon + 1));
        if (key == "Output") plan.output = value;
        else if (key == "Target") plan.target = value;
        else if (key == "Entry") plan.entry = value;
        else if (key == "Profile") plan.profile = value;
        else if (key == "Config") plan.config = value;
        else if (key == "Source") plan.source = value;
        else if (key == "Sources") plan.sources = takeList(value);
        else if (key == "Order") plan.order = takeList(value);
        else if (key == "UI") plan.ui = takeList(value);
    }
    plan.ok = true;
    return plan;
}

std::string ExplainRbxb(const RbxbPlan& plan) {
    std::ostringstream out;
    out << "output " << plan.output << "\n";
    out << "target " << (plan.target.empty() ? "host" : plan.target) << "\n";
    out << "config " << plan.config << "\n";
    out << "profile " << (plan.profile.empty() ? plan.config : plan.profile) << "\n";
    out << "entry " << (plan.source.empty() ? plan.entry : plan.source) << "\n";
    out << "order";
    const auto& order = plan.order.empty() ? plan.sources : plan.order;
    if (order.empty() && !plan.source.empty()) out << " " << plan.source;
    for (const auto& item : order) out << " " << item;
    out << "\n";
    if (!plan.ui.empty()) {
        out << "ui";
        for (const auto& item : plan.ui) out << " " << item;
        out << "\n";
    }
    out << "dry-run: compile was not started\n";
    return out.str();
}

bool IncrementalHit(const fs::path& source, const fs::path& output) {
    std::error_code ec;
    if (!fs::is_regular_file(output, ec) || !fs::is_regular_file(source, ec)) return false;
    std::ifstream in(StampPath(source));
    if (!in) return false;
    std::string line;
    std::string stampedOutput;
    std::string stampedSource;
    std::vector<std::pair<std::string, std::string>> deps;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.rfind("output ", 0) == 0) stampedOutput = Trim(line.substr(7));
        else if (line.rfind("source ", 0) == 0) stampedSource = Trim(line.substr(7));
        else if (line.rfind("dep ", 0) == 0) {
            const std::string rest = Trim(line.substr(4));
            const auto eq = rest.rfind('=');
            if (eq != std::string::npos) deps.emplace_back(rest.substr(0, eq), rest.substr(eq + 1));
        }
    }
    if (stampedSource != HashFile(source)) return false;
    if (fs::absolute(output).lexically_normal().string() != stampedOutput) return false;
    const auto current = ImportPaths(source);
    if (current.size() != deps.size()) return false;
    for (std::size_t i = 0; i < current.size(); ++i) {
        if (current[i].string() != deps[i].first) return false;
        if (HashFile(current[i]) != deps[i].second) return false;
    }
    return true;
}

void WriteIncrementalStamp(const fs::path& source, const fs::path& output) {
    std::ostringstream out;
    out << "output " << fs::absolute(output).lexically_normal().string() << "\n";
    out << "source " << HashFile(source) << "\n";
    for (const auto& dep : ImportPaths(source)) {
        out << "dep " << dep.string() << "=" << HashFile(dep) << "\n";
    }
    WriteFile(StampPath(source), out.str());
}

int PackageCommand(const std::vector<std::string>& args) {
    if (args.empty()) return -1;
    const std::string& cmd = args[0];
    if (cmd != "add" && cmd != "remove" && cmd != "update" && cmd != "install" && cmd != "restore") return -1;

    const std::string projectArg = ArgValue(args, "--project");
    ProjectFile project;
    if (!FindProject(projectArg.empty() ? fs::current_path() : fs::path(projectArg), project)) {
        std::cerr << "robix " << cmd << ": no .rbxproj found.\n";
        return 1;
    }
    if (project.signKey.empty()) {
        std::cerr << "robix " << cmd << ": " << project.file.string() << " has no SignKey.\n";
        return 1;
    }
    const fs::path lock = LockPath(project);
    auto packages = ReadLock(lock);

    if (cmd == "restore" || cmd == "install") {
        if (packages.empty()) {
            std::cerr << "robix " << cmd << ": " << lock.string() << " has no packages.\n";
            return 1;
        }
        for (const auto& package : packages) {
            const fs::path file = fs::path(package.path);
            if (!fs::is_regular_file(file)) {
                std::cerr << "robix " << cmd << ": missing " << package.path << "\n";
                return 1;
            }
            const std::string sha = HashFile(file);
            const std::string signature = SignatureFor(project.signKey, package.name, package.version, sha);
            if (sha != package.sha256 || signature != package.signature) {
                std::cerr << "robix " << cmd << ": " << package.name << " hash or signature does not match the lock.\n";
                return 1;
            }
            if (cmd == "install") {
                const fs::path destDir = project.directory / "Packages" / package.name;
                const fs::path dest = destDir / file.filename();
                const std::string bytes = ReadFile(file);
                if (!WriteFile(dest, bytes)) {
                    std::cerr << "robix install: could not write " << dest.string() << "\n";
                    return 1;
                }
                const std::string api = PublicApi(package.name, bytes);
                if (!WriteFile(destDir / (package.name + ".rbxapi"), api)) return 1;
                std::ostringstream rbxp;
                rbxp << "name " << package.name << "\n"
                     << "version " << package.version << "\n"
                     << "sha256 " << sha << "\n"
                     << "signature " << signature << "\n"
                     << "file " << dest.filename().string() << "\n";
                if (!WriteFile(destDir / (package.name + ".rbxp"), rbxp.str())) return 1;
                std::cout << "robix install: " << package.name << " -> " << dest.string() << "\n";
            } else {
                std::cout << "robix restore: " << package.name << " sha256 " << sha << "\n";
            }
        }
        return 0;
    }

    const std::string name = Positional(args, 0);
    if (name.empty()) {
        std::cerr << "robix " << cmd << ": package name is required.\n";
        return 1;
    }
    if (cmd == "remove") {
        const auto before = packages.size();
        packages.erase(std::remove_if(packages.begin(), packages.end(), [&](const LockedPackage& package) {
            return package.name == name;
        }), packages.end());
        if (packages.size() == before) {
            std::cerr << "robix remove: " << name << " is not in the lock.\n";
            return 1;
        }
        if (!WriteLock(lock, packages)) return 1;
        std::error_code ec;
        fs::remove_all(project.directory / "Packages" / name, ec);
        std::cout << "robix remove: " << name << "\n";
        return 0;
    }

    std::string path = cmd == "update" ? [&]() {
        for (const auto& package : packages) if (package.name == name) return package.path;
        return std::string{};
    }() : Positional(args, 1);
    if (path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0) {
#ifndef _WIN32
        std::cerr << "robix " << cmd << ": package download is implemented for Windows.\n";
        return 1;
#else
        const std::string publisherKey = ArgValue(args, "--publisher-key");
        if (publisherKey.empty()) {
            std::cerr << "robix " << cmd << ": --publisher-key is required for a downloaded package.\n";
            return 1;
        }
        std::string bytes;
        std::string signature;
        std::string downloadError;
        if (!DownloadHttp(path, bytes, downloadError) || !DownloadHttp(path + ".sig", signature, downloadError)) {
            std::cerr << "robix " << cmd << ": " << downloadError << "\n";
            return 1;
        }
        signature = Trim(signature);
        const std::string expected = Robix::StdLib::CryptoEngine::ComputeHMACSHA256Hex(publisherKey, bytes);
        if (signature != expected) {
            std::cerr << "robix " << cmd << ": publisher signature does not match.\n";
            return 1;
        }
        const fs::path cache = project.directory / "Packages" / "cache" / (name + ".rbx");
        if (!WriteFile(cache, bytes)) {
            std::cerr << "robix " << cmd << ": could not store the download.\n";
            return 1;
        }
        path = cache.string();
        std::cout << "robix " << cmd << ": downloaded " << name << " publisher signature matched\n";
#endif
    }
    if (path.empty() || !fs::is_regular_file(path)) {
        std::cerr << "robix " << cmd << ": package file was not found.\n";
        return 1;
    }
    std::string version = ArgValue(args, "--version");
    if (version.empty()) {
        version = "1.0.0";
        for (const auto& package : packages) if (package.name == name) version = package.version;
    }
    const std::string sha = HashFile(path);
    LockedPackage next;
    next.name = name;
    next.version = version;
    next.path = path;
    next.sha256 = sha;
    next.signature = SignatureFor(project.signKey, name, version, sha);
    bool replaced = false;
    for (auto& package : packages) {
        if (package.name == name) {
            package = next;
            replaced = true;
        }
    }
    if (!replaced) packages.push_back(next);
    if (!WriteLock(lock, packages)) return 1;
    std::cout << "robix " << cmd << ": " << name << " sha256 " << sha << "\n";
    return 0;
}

} // namespace Robix::Tools
