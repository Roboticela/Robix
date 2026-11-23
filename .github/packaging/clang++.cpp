// Windows stand-in for clang++. The compiler asks for clang++, and this
// program turns that request into an llc object file for the Windows target.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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

static std::wstring env_value(const wchar_t* name) {
    DWORD length = GetEnvironmentVariableW(name, NULL, 0);
    if (length == 0) {
        return L"";
    }
    std::vector<wchar_t> buffer(length + 1, L'\0');
    DWORD written = GetEnvironmentVariableW(name, buffer.data(), length);
    if (written == 0) {
        return L"";
    }
    return std::wstring(buffer.data(), written);
}

static std::wstring find_llc() {
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return L"";
    }
    std::wstring dir(buffer, length);
    size_t file_slash = dir.find_last_of(L"\\/");
    if (file_slash == std::wstring::npos) {
        return L"";
    }
    dir.erase(file_slash);
    for (int hop = 0; hop < 8; ++hop) {
        std::wstring candidate = dir + L"\\Shared\\Tools\\Llvm\\bin\\llc.exe";
        if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return candidate;
        }
        size_t slash = dir.find_last_of(L"\\/");
        if (slash == std::wstring::npos) {
            break;
        }
        dir.erase(slash);
    }
    return L"";
}

static int count_char(const std::string& line, char mark) {
    int count = 0;
    for (char character : line) {
        if (character == mark) {
            ++count;
        }
    }
    return count;
}

static bool strip_foreign_main(const std::wstring& path) {
    std::ifstream input(fs::path(path), std::ios::binary);
    if (!input) {
        std::wcerr << L"error: the IR file was not found: " << path << L"\n";
        return false;
    }
    std::vector<std::string> kept;
    std::string line;
    bool skip = false;
    int depth = 0;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!skip && line.rfind("define i32 @main", 0) == 0) {
            skip = true;
            depth = 0;
        }
        if (skip) {
            int opens = count_char(line, '{');
            int closes = count_char(line, '}');
            depth += opens - closes;
            if (opens + closes > 0 && depth <= 0) {
                skip = false;
            }
            continue;
        }
        kept.push_back(line);
    }
    input.close();
    std::ofstream output(fs::path(path), std::ios::trunc | std::ios::binary);
    if (!output) {
        std::wcerr << L"error: the IR file could not be updated: " << path << L"\n";
        return false;
    }
    for (const std::string& kept_line : kept) {
        output << kept_line << "\n";
    }
    return true;
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

static int compile_object(int argc, wchar_t** argv) {
    if (env_value(L"ROBIX_OBJECT_ONLY") != L"1") {
        std::cerr << "error: machine code is generated with llc, not clang++\n";
        return 1;
    }
    std::wstring out;
    std::wstring ll;
    std::wstring previous;
    for (int index = 1; index < argc; ++index) {
        std::wstring argument = argv[index];
        if (previous == L"-o") {
            out = argument;
        }
        if (argument.size() >= 3 && argument.substr(argument.size() - 3) == L".ll") {
            ll = argument;
        }
        previous = argument;
    }
    if (out.empty() || ll.empty()) {
        std::cerr << "error: the compiler did not provide an object and an IR file\n";
        return 1;
    }
    if (env_value(L"ROBIX_KEEP_MAIN") != L"1") {
        if (!strip_foreign_main(ll)) {
            return 1;
        }
    }
    std::wstring llc = find_llc();
    if (llc.empty()) {
        std::cerr << "error: the bundled llc is missing\n";
        return 1;
    }
    std::wstring command = quote_arg(llc)
        + L" -filetype=obj -O3 -relocation-model=pic -mtriple=x86_64-w64-windows-gnu -o "
        + quote_arg(out) + L" " + quote_arg(ll);
    int code = run_process(command);
    if (code < 0) {
        std::cerr << "error: llc could not be started\n";
        return 1;
    }
    return code;
}

int wmain(int argc, wchar_t** argv) {
    return compile_object(argc, argv);
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
