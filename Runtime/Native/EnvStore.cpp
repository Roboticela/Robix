// Development environment store. Values come from the process environment and
// from env files found by walking up from the current directory. Get warns
// when a name is missing or empty. Exists, Set, Add, and Remove do not.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>

#include <filesystem>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace fs = std::filesystem;

namespace {

std::mutex g_mu;
std::map<std::string, std::string> g_values;
std::map<std::string, fs::path> g_origin;
std::set<std::string> g_warned;
fs::path g_project;
bool g_loaded = false;
bool g_canPersist = false;

std::string Trim(std::string text) {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) text.pop_back();
    std::size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) ++start;
    return text.substr(start);
}

char* CopyText(const std::string& text) {
    char* copy = static_cast<char*>(std::malloc(text.size() + 1));
    if (copy == nullptr) return nullptr;
    std::memcpy(copy, text.data(), text.size());
    copy[text.size()] = '\0';
    return copy;
}

void WarnLocked(const std::string& name, bool missing) {
    const std::string key = (missing ? "missing:" : "empty:") + name;
    if (!g_warned.insert(key).second) return;
    std::fprintf(stderr, "warning: env %s %s\n", name.c_str(), missing ? "not found" : "is empty");
    std::fflush(stderr);
}

void ReadFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq));
        if (key.empty()) continue;
        g_values[key] = Trim(line.substr(eq + 1));
        g_origin[key] = path;
    }
}

void LoadLocked() {
    if (g_loaded) return;
    g_loaded = true;
    wchar_t directory[MAX_PATH];
    const DWORD length = GetCurrentDirectoryW(MAX_PATH, directory);
    fs::path cursor = length > 0 ? fs::path(directory) : fs::path(".");
    fs::path found;
    for (int depth = 0; depth < 8 && !cursor.empty(); ++depth) {
        std::error_code ec;
        bool project = fs::is_regular_file(cursor / ".env.common", ec);
        if (!project) {
            for (const auto& entry : fs::directory_iterator(cursor, ec)) {
                if (entry.path().extension() == ".rbxproj") {
                    project = true;
                    break;
                }
            }
        }
        if (project) {
            found = cursor;
            break;
        }
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor) break;
        cursor = cursor.parent_path();
    }
    if (found.empty()) return;
    g_project = found;
    g_canPersist = true;
    const char* names[] = {
        "exp.env.common", ".env.common",
        "exp.env.client", ".env.client",
        "exp.env.server", ".env.server",
        "exp.env", ".env"
    };
    for (const char* name : names) ReadFile(found / name);
    const fs::path cwd = length > 0 ? fs::path(directory) : fs::path(".");
    if (cwd != found) {
        ReadFile(cwd / "exp.env");
        ReadFile(cwd / ".env");
    }
}

struct Hit {
    bool found = false;
    std::string value;
};

Hit LookupLocked(const std::string& name) {
    LoadLocked();
    const auto it = g_values.find(name);
    if (it != g_values.end()) return Hit{ true, it->second };
    char* process = nullptr;
    std::size_t processLength = 0;
    if (_dupenv_s(&process, &processLength, name.c_str()) == 0 && process != nullptr) {
        Hit hit{ true, process };
        std::free(process);
        return hit;
    }
    std::free(process);
    return {};
}

void RewriteLocked(const std::string& name, const std::string& value, bool removing) {
    LoadLocked();
    if (!g_canPersist) {
        SetEnvironmentVariableA(name.c_str(), removing ? nullptr : value.c_str());
        return;
    }
    fs::path file;
    const auto origin = g_origin.find(name);
    if (origin != g_origin.end()) file = origin->second;
    else file = g_project / ".env.common";
    std::ifstream in(file);
    std::string rewritten;
    bool replaced = false;
    std::string line;
    while (std::getline(in, line)) {
        const std::string trimmed = Trim(line);
        const auto eq = trimmed.find('=');
        const bool same = eq != std::string::npos && Trim(trimmed.substr(0, eq)) == name;
        if (same) {
            replaced = true;
            if (!removing) rewritten += name + "=" + value + "\n";
            continue;
        }
        rewritten += line;
        rewritten.push_back('\n');
    }
    if (!removing && !replaced) rewritten += name + "=" + value + "\n";
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return;
    out << rewritten;
    if (removing) g_origin.erase(name);
    else g_origin[name] = file;
    SetEnvironmentVariableA(name.c_str(), removing ? nullptr : value.c_str());
}

} // namespace

extern "C" {

char* rbx_env_get(const char* name) {
    const std::string key = name != nullptr ? name : "";
    std::lock_guard<std::mutex> lock(g_mu);
    const Hit hit = LookupLocked(key);
    if (!hit.found) {
        WarnLocked(key, true);
        return CopyText("");
    }
    if (hit.value.empty()) WarnLocked(key, false);
    return CopyText(hit.value);
}

std::int64_t rbx_env_exists(const char* name) {
    const std::string key = name != nullptr ? name : "";
    std::lock_guard<std::mutex> lock(g_mu);
    return LookupLocked(key).found ? 1 : 0;
}

std::int64_t rbx_env_set(const char* name, const char* value) {
    const std::string key = name != nullptr ? name : "";
    const std::string stored = value != nullptr ? value : "";
    std::lock_guard<std::mutex> lock(g_mu);
    if (key.empty()) {
        if (g_warned.insert("missing:name").second) {
            std::fprintf(stderr, "warning: env name is empty\n");
            std::fflush(stderr);
        }
        return 0;
    }
    LoadLocked();
    g_values[key] = stored;
    g_warned.erase(std::string("missing:") + key);
    if (!stored.empty()) g_warned.erase(std::string("empty:") + key);
    RewriteLocked(key, stored, false);
    return 1;
}

std::int64_t rbx_env_add(const char* name, const char* value) {
    const std::string key = name != nullptr ? name : "";
    std::lock_guard<std::mutex> lock(g_mu);
    if (key.empty()) {
        if (g_warned.insert("missing:name").second) {
            std::fprintf(stderr, "warning: env name is empty\n");
            std::fflush(stderr);
        }
        return 0;
    }
    if (LookupLocked(key).found) return 0;
    const std::string stored = value != nullptr ? value : "";
    g_values[key] = stored;
    RewriteLocked(key, stored, false);
    return 1;
}

std::int64_t rbx_env_remove(const char* name) {
    const std::string key = name != nullptr ? name : "";
    std::lock_guard<std::mutex> lock(g_mu);
    LoadLocked();
    const bool present = g_values.find(key) != g_values.end() || LookupLocked(key).found;
    if (!present) return 0;
    g_values.erase(key);
    RewriteLocked(key, "", true);
    return 1;
}

}
