#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

static std::wstring quote_arg(const std::wstring& arg) {
    if (arg.empty()) return L"\"\"";
    if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring result = L"\"";
    for (size_t i = 0; i < arg.length(); ++i) {
        size_t backslashes = 0;
        while (i < arg.length() && arg[i] == L'\\') {
            backslashes++;
            i++;
        }
        if (i == arg.length()) {
            result.append(backslashes * 2, L'\\');
            break;
        } else if (arg[i] == L'"') {
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

static fs::path get_root_dir() {
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return fs::current_path();
    }
    fs::path exe_path(buffer);
    return exe_path.parent_path().parent_path();
}

static int run_process(const std::wstring& app, const std::wstring& cmdline) {
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> cmd_buf(cmdline.begin(), cmdline.end());
    cmd_buf.push_back(L'\0');

    BOOL success = CreateProcessW(
        app.empty() ? NULL : app.c_str(),
        cmd_buf.data(),
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!success) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exit_code);
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        std::cerr << "Robixc compiles every Robix file in a folder into one program.\n";
        std::cerr << "Usage: Robixc <Folder> <Output>\n";
        std::cerr << "       Robixc <File.rx> [More.rx ...] <Output>\n";
        return 1;
    }

    fs::path root = get_root_dir();
    fs::path script = root / "Shared" / "Tools" / "Robixc.ps1";

    if (fs::exists(script)) {
        std::wstring cmdline = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File " + quote_arg(script.wstring());
        for (int i = 1; i < argc; ++i) {
            cmdline += L" " + quote_arg(argv[i]);
        }
        int code = run_process(L"", cmdline);
        if (code >= 0) {
            return code;
        }
    }

    // Direct fallback if script is missing or powershell failed
    std::cerr << "Robixc compiles every Robix file in a folder into one program.\n";
    std::cerr << "Usage: Robixc <Folder> <Output>\n";
    std::cerr << "       Robixc <File.rx> [More.rx ...] <Output>\n";
    return 1;
}

int main() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;
    int res = wmain(argc, argv);
    LocalFree(argv);
    return res;
}
