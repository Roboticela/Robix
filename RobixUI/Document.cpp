#include "Document.hpp"

#include "Platform/Windows/WindowsHost.hpp"
#include "Web/WebFoundation.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace Robix::UI {
namespace {

std::string Option(const std::vector<std::string>& args, const std::string& name, const std::string& fallback) {
    for (std::size_t index = 0; index + 1 < args.size(); ++index) {
        if (args[index] == name) {
            return args[index + 1];
        }
    }
    return fallback;
}

bool Has(const std::vector<std::string>& args, const std::string& name) {
    for (const std::string& arg : args) {
        if (arg == name) {
            return true;
        }
    }
    return false;
}

}  // namespace

int RunUiCommand(const std::vector<std::string>& args) {
    if (args.empty() || args[0] != "ui" || args.size() < 2) {
        return -1;
    }
    const std::string command = args[1];
    std::string document;
    for (std::size_t index = 2; index < args.size(); ++index) {
        if (args[index].size() > 0 && args[index][0] == '-') {
            if (index + 1 < args.size() && (args[index] == "-o" || args[index] == "--code" || args[index] == "--out")) {
                ++index;
            }
            continue;
        }
        document = args[index];
        break;
    }

    if (command == "render") {
        const std::string output = Option(args, "-o", "framebuffer.bmp");
        std::string error;
        if (!WriteFoundationBmp(output, error)) {
            std::cerr << "robix ui: " << error << "\n";
            return 1;
        }
        std::cout << "bmp " << output << "\n";
        return 0;
    }
    if (command == "window") {
        WindowRunOptions options;
        options.stayOpen = Has(args, "--dev");
        return RunWindowsFoundation(options);
    }
    if (command == "web") {
        const std::string output = Option(args, "-o", "Build/web");
        std::string error;
        if (!WriteWebFoundation(output, document, error)) {
            std::cerr << "robix ui: " << error << "\n";
            return 1;
        }
        return 0;
    }
    if (command == "validate") {
        if (document.empty()) {
            std::cerr << "robix ui: expected a .rbxu file\n";
            return 1;
        }
        std::ifstream in(document, std::ios::binary);
        if (!in) {
            std::cerr << "robix ui: could not open '" << document << "'\n";
            return 1;
        }
        std::cout << "foundation validated " << document << "\n";
        return 0;
    }
    return -1;
}

}  // namespace Robix::UI
