// Windows compiler driver. It compiles every Robix source with Compiler.exe
// and links the objects with the bundled clang++ into one program.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::wstring quote_arg(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }
    if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring result = L"\"";
    for (size_t i = 0; i < arg.size(); ++i) {
        size_t backslashes = 0;
        while (i < arg.size() && arg[i] == L'\\') {
            ++backslashes;
            ++i;
        }
        if (i == arg.size()) {
            result.append(backslashes * 2, L'\\');
            break;
        }
        if (arg[i] == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
        } else {
            result.append(backslashes, L'\\');
            result.push_back(arg[i]);
        }
    }
    result += L"\"";
    return result;
}

static void show_usage() {
    std::cerr << "Robixc compiles every Robix file in a folder into one program.\n";
    std::cerr << "Usage: Robixc <Folder> <Output>\n";
    std::cerr << "       Robixc <File.rx> [More.rx ...] <Output>\n";
}

static fs::path install_root() {
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return fs::current_path();
    }
    return fs::path(buffer).parent_path().parent_path();
}

static int run_process(const std::wstring& cmdline) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    std::vector<wchar_t> cmd(cmdline.begin(), cmdline.end());
    cmd.push_back(L'\0');
    if (!CreateProcessW(NULL, cmd.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(code);
}

static bool contains_main(const fs::path& source) {
    std::ifstream input(source, std::ios::binary);
    if (!input) {
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return text.find("Function Main") != std::string::npos;
}

static bool is_test_source(const fs::path& source) {
    std::wstring name = source.filename().wstring();
    const std::wstring suffix = L".test.rx";
    return name.size() >= suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool is_robix_source(const fs::path& source) {
    std::wstring name = source.filename().wstring();
    const std::wstring suffix = L".rx";
    if (name.size() < suffix.size() || name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return false;
    }
    return !is_test_source(source);
}

struct TempWork {
    fs::path path;
    explicit TempWork(fs::path directory) : path(std::move(directory)) {}
    ~TempWork() {
        std::error_code ignored;
        fs::remove_all(path, ignored);
    }
};

static int compile_sources(const std::vector<fs::path>& sources, const fs::path& output) {
    fs::path root = install_root();
    fs::path engine = root / "Shared" / "Tools" / "Compiler.exe";
    fs::path linker = root / "Shared" / "Tools" / "Llvm" / "bin" / "robix-clang++.exe";
    fs::path runtime = root / "Runtime" / "Runtime.o";
    fs::path tools = root / "Shared" / "Tools";
    if (!fs::exists(engine) || !fs::exists(linker) || !fs::exists(runtime)) {
        std::cerr << "error: the Windows compiler is incomplete. Compiler.exe, robix-clang++.exe, and Runtime.o must be installed with Robixc.\n";
        return 1;
    }

    wchar_t current_path[32767];
    DWORD path_length = GetEnvironmentVariableW(L"PATH", current_path, 32767);
    std::wstring path = tools.wstring();
    if (path_length > 0) {
        path += L";";
        path += current_path;
    }
    SetEnvironmentVariableW(L"PATH", path.c_str());
    SetEnvironmentVariableW(L"ROBIX_OBJECT_ONLY", L"1");

    wchar_t temp_root[MAX_PATH];
    DWORD temp_length = GetTempPathW(MAX_PATH, temp_root);
    if (temp_length == 0 || temp_length >= MAX_PATH) {
        std::cerr << "error: a temporary directory was not available\n";
        return 1;
    }
    std::wstring folder_name = L"robixc-";
    folder_name += std::to_wstring(GetCurrentProcessId());
    TempWork work(fs::path(temp_root) / folder_name);
    std::error_code mkdir_error;
    fs::create_directories(work.path, mkdir_error);
    if (mkdir_error) {
        std::cerr << "error: the temporary directory could not be created\n";
        return 1;
    }

    fs::path entry;
    for (const fs::path& source : sources) {
        if (contains_main(source)) {
            entry = source;
            break;
        }
    }
    if (entry.empty()) {
        entry = sources.front();
    }

    std::vector<fs::path> objects;
    int index = 0;
    for (const fs::path& source : sources) {
        ++index;
        fs::path object = work.path / (std::to_wstring(index) + L"-" + source.stem().wstring() + L".o");
        SetEnvironmentVariableW(L"ROBIX_KEEP_MAIN", source == entry ? L"1" : L"0");
        std::wstring command = quote_arg(engine.wstring()) + L" " + quote_arg(source.wstring()) + L" " + quote_arg(object.wstring());
        int code = run_process(command);
        if (code != 0) {
            if (code < 0) {
                std::cerr << "error: Compiler.exe could not be started\n";
                return 1;
            }
            return code;
        }
        objects.push_back(object);
    }

    if (output.has_parent_path() && !output.parent_path().empty()) {
        fs::create_directories(output.parent_path(), mkdir_error);
    }
    std::wstring link = quote_arg(linker.wstring()) + L" -static -std=c++17 -O2 -o " + quote_arg(output.wstring());
    for (const fs::path& object : objects) {
        link += L" ";
        link += quote_arg(object.wstring());
    }
    link += L" ";
    link += quote_arg(runtime.wstring());
    int linked = run_process(link);
    if (linked < 0) {
        std::cerr << "error: the Windows linker could not be started\n";
        return 1;
    }
    return linked;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) {
        show_usage();
        return 1;
    }
    std::vector<fs::path> sources;
    fs::path output;
    fs::path first = fs::absolute(argv[1]);
    std::error_code status;
    if (fs::is_directory(first, status)) {
        if (argc != 3) {
            std::cerr << "error: a folder compile takes the folder and the output program\n";
            return 1;
        }
        for (const fs::directory_entry& entry : fs::directory_iterator(first)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            if (is_robix_source(entry.path())) {
                sources.push_back(entry.path());
            }
        }
        std::sort(sources.begin(), sources.end());
        output = fs::absolute(argv[2]);
    } else {
        output = fs::absolute(argv[argc - 1]);
        for (int index = 1; index < argc - 1; ++index) {
            fs::path source = fs::absolute(argv[index]);
            if (!fs::is_regular_file(source)) {
                std::wcerr << L"error: source was not found: " << source.wstring() << L"\n";
                return 1;
            }
            sources.push_back(source);
        }
    }
    if (sources.empty()) {
        std::cerr << "error: the folder has no Robix files\n";
        return 1;
    }
    return compile_sources(sources, output);
}

int main() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return 1;
    }
    int result = wmain(argc, argv);
    LocalFree(argv);
    return result;
}
