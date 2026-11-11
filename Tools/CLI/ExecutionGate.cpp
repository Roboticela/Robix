#include "ExecutionGate.hpp"

#include <cctype>
#include <fstream>

namespace fs = std::filesystem;

namespace Robix::Tools {

namespace {

std::string Visible(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size();) {
        if (i + 1 < in.size() && in[i] == '/' && in[i + 1] == '/') {
            while (i < in.size() && in[i] != '\n') ++i;
            continue;
        }
        if (i + 1 < in.size() && in[i] == '/' && in[i + 1] == '*') {
            i += 2;
            while (i + 1 < in.size() && !(in[i] == '*' && in[i + 1] == '/')) ++i;
            if (i + 1 < in.size()) i += 2;
            else i = in.size();
            continue;
        }
        if (in[i] == '"') {
            ++i;
            while (i < in.size() && in[i] != '"') {
                if (in[i] == '\\' && i + 1 < in.size()) i += 2;
                else ++i;
            }
            if (i < in.size()) ++i;
            continue;
        }
        out.push_back(in[i]);
        ++i;
    }
    return out;
}

std::string Trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

const char* const kDirect[] = {
    "CPU.", "Port.", "Console.Write", "Timer.Arm", "Timer.Preempt", "System.Call",
    "Memory.Pages", "Memory.Address", "Memory.At"
};

const char* const kPlatform[] = {
    "Print(", "Input(", "Input<", "File.", "Args.", "Env.", "Network.", "Thread.",
    "GPIO.", "PWM.", "UART.", "SPI.", "I2C.", "CAN.", "USB.", "PCIe.", "DMA.",
    "Sensor.", "Motor.SetSpeed", "Interrupt.", "HardwareTimer."
};

std::string UseName(const char* needle) {
    std::string name = needle;
    while (!name.empty() && (name.back() == '.' || name.back() == '(' || name.back() == '<')) name.pop_back();
    return name;
}

} // namespace

CodeShape ScanCodeShape(std::string_view source) {
    const std::string text = Visible(source);
    CodeShape shape;
    for (const char* needle : kDirect) {
        if (text.find(needle) != std::string::npos) {
            shape.direct = true;
            if (shape.directUse.empty()) shape.directUse = UseName(needle);
        }
    }
    for (const char* needle : kPlatform) {
        if (text.find(needle) != std::string::npos) {
            shape.platform = true;
            if (shape.platformUse.empty()) shape.platformUse = UseName(needle);
        }
    }
    return shape;
}

std::string ProjectCategory(const fs::path& start) {
    std::error_code ec;
    fs::path dir = fs::is_directory(start, ec) ? start : start.parent_path();
    for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
        if (fs::is_directory(dir, ec)) {
            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                if (entry.path().extension() != ".rxproj") continue;
                std::ifstream in(entry.path());
                std::string line;
                while (std::getline(in, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line.rfind("Category:", 0) != 0) continue;
                    return Trim(line.substr(9));
                }
            }
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    return {};
}

bool IsPlatformName(std::string_view target) {
    return target == "windows" || target == "linux" || target == "macos" || target == "android" ||
           target == "ios" || target == "web";
}

bool IsDirectName(std::string_view target) {
    return target == "direct" || target == "image" || target == "x86_64-unknown-none" ||
           target == "aarch64-unknown-none" || target == "arm-unknown-none" || target == "riscv64-unknown-none";
}

std::string ProjectField(const fs::path& start, const char* key) {
    const std::string prefix = std::string(key) + ":";
    std::error_code ec;
    fs::path dir = fs::is_directory(start, ec) ? start : start.parent_path();
    for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
        if (fs::is_directory(dir, ec)) {
            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                if (entry.path().extension() != ".rxproj") continue;
                std::ifstream in(entry.path());
                std::string line;
                while (std::getline(in, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line.rfind(prefix, 0) != 0) continue;
                    return Trim(line.substr(prefix.size()));
                }
            }
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    return {};
}

fs::path ProjectRoot(const fs::path& start) {
    std::error_code ec;
    fs::path dir = fs::is_directory(start, ec) ? start : start.parent_path();
    fs::path fallback = dir;
    for (int depth = 0; depth < 8 && !dir.empty(); ++depth) {
        if (fs::is_directory(dir, ec)) {
            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                if (entry.path().extension() == ".rxproj") return dir;
            }
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    return fallback;
}

fs::path FileDir(const fs::path& file) {
    fs::path dir = file.parent_path();
    if (dir.empty()) dir = fs::current_path();
    return dir;
}

fs::path FirstRxu(const fs::path& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return {};
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.path().extension() == ".rxu") return entry.path();
    }
    return {};
}

fs::path FindScreen(const fs::path& file) {
    std::error_code ec;
    const fs::path root = ProjectRoot(file);
    fs::path found;
    auto walk = [&](auto&& self, const fs::path& dir, int depth) -> void {
        if (!found.empty() || depth > 3 || !fs::is_directory(dir, ec)) return;
        found = FirstRxu(dir);
        if (!found.empty()) return;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_directory(ec)) continue;
            const std::string name = entry.path().filename().string();
            if (name == "Build" || name == "Packages") continue;
            self(self, entry.path(), depth + 1);
            if (!found.empty()) return;
        }
    };
    walk(walk, root, 0);
    return found;
}

int CountRx(const fs::path& dir) {
    std::error_code ec;
    int count = 0;
    if (!fs::is_directory(dir, ec)) return 0;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.path().extension() == ".rx") ++count;
    }
    return count;
}

fs::path FindLocalScreen(const fs::path& file) {
    const fs::path dir = FileDir(file);
    const fs::path named = dir / (file.stem().string() + ".rxu");
    std::error_code ec;
    if (fs::is_regular_file(named, ec)) return named;
    if (dir.filename() == "Source") {
        const fs::path ui = FirstRxu(dir.parent_path() / "UI");
        if (!ui.empty()) return ui;
    }
    if (CountRx(dir) == 1) return FirstRxu(dir);
    return {};
}

fs::path FindNetworkScreen(const fs::path& file) {
    const fs::path dir = FileDir(file);
    const fs::path beside = FirstRxu(dir);
    if (!beside.empty()) return beside;
    if (dir.filename() == "Source") return FirstRxu(dir.parent_path() / "UI");
    return {};
}

AppForm FormFromKind(const std::string& kind) {
    if (kind == "Console" || kind == "CLI") return AppForm::Console;
    if (kind == "Window" || kind == "GUI") return AppForm::Window;
    if (kind == "Network") return AppForm::Network;
    return AppForm::Unknown;
}

AppForm Classify(const std::string& kind, std::string_view source, const fs::path& file) {
    const AppForm fromKind = FormFromKind(kind);
    if (fromKind != AppForm::Unknown) return fromKind;
    const std::string text = Visible(source);
    if (text.find("Network.") != std::string::npos || text.find("SendText(") != std::string::npos) return AppForm::Network;
    if (file.extension() == ".rxu" || !FindLocalScreen(file).empty()) return AppForm::Window;
    if (text.find("Print(") != std::string::npos || text.find("Input(") != std::string::npos ||
        text.find("Input<") != std::string::npos || text.find("Args.") != std::string::npos) {
        return AppForm::Console;
    }
    return AppForm::Unknown;
}

std::string ExecutionError(const std::string& category, RunSurface surface, const std::string& targetName,
                           const CodeShape& shape) {
    const std::string where = targetName.empty() ? std::string("a platform") : targetName;
    if (shape.platform && shape.direct) {
        return "error: this file uses " + shape.platformUse + " and " + shape.directUse +
               ". Platform code and machine code cannot run as one target.\n";
    }
    if (category == "Application" && surface == RunSurface::Direct) {
        return "error: an Application runs on a platform. It does not build a machine image.\n";
    }
    if (category == "Bedrock" && surface == RunSurface::Platform) {
        return "error: Bedrock does not run on " + where + ". It builds a machine image.\n";
    }
    if (category == "Application" && shape.direct) {
        return "error: " + shape.directUse + " is machine code. An Application runs on a platform.\n";
    }
    if (category == "Bedrock" && shape.platform) {
        return "error: " + shape.platformUse + " is platform code. Bedrock builds a machine image and cannot run it.\n";
    }
    if (surface == RunSurface::Platform && shape.direct) {
        return "error: " + shape.directUse + " is machine code. It does not run on " + where + ".\n";
    }
    if (surface == RunSurface::Direct && shape.platform) {
        return "error: " + shape.platformUse + " is platform code. A machine image cannot run it.\n";
    }
    return {};
}

std::string ProjectKind(const fs::path& start) {
    return ProjectField(start, "Kind");
}

WebDecision DecideWeb(const std::string& category, const std::string& kind, std::string_view source,
                      const fs::path& file, bool targetIsWeb) {
    WebDecision decision;
    if (category == "Bedrock") return decision;
    const bool canvas = category == "Canvas" || kind == "Empty";
    const AppForm form = Classify(kind, source, file);
    const bool projectForm = FormFromKind(kind) != AppForm::Unknown;
    const fs::path screen = projectForm ? FindScreen(file) : (form == AppForm::Network ? FindNetworkScreen(file) : FindLocalScreen(file));
    if (!targetIsWeb) {
        if (canvas && form == AppForm::Console) {
            decision.warning = "warning: this file is a console program. It compiles for windows, linux, macos, android, and ios. Web is refused.\n";
        } else if (canvas && form == AppForm::Window) {
            decision.warning = "warning: a screen was found, so this file can also compile for web.\n";
        } else if (canvas && form == AppForm::Network) {
            decision.warning = "warning: this file is a network program, so it can also compile for web.\n";
        } else if (canvas && form == AppForm::Unknown) {
            decision.warning = "warning: this file does not show a console, a window, or a network program. It was treated as a console program. Web is refused.\n";
        }
        return decision;
    }
    if (form == AppForm::Console) {
        decision.error = "error: a console program does not compile for web. Use windows, linux, macos, android, or ios.\n";
        return decision;
    }
    if (form == AppForm::Unknown) {
        decision.error = "error: this file has no screen and no network call, so it cannot compile for web.\n";
        return decision;
    }
    if (screen.empty()) {
        decision.error = form == AppForm::Network
            ? "error: a network program can compile for web, but this project has no .rxu screen.\n"
            : "error: a window program can compile for web, but this project has no .rxu screen.\n";
        return decision;
    }
    decision.screen = screen.string();
    if (canvas) {
        decision.warning = form == AppForm::Network
            ? "warning: this file is a network program. Web is using " + decision.screen + ".\n"
            : "warning: this file has a screen. Web is using " + decision.screen + ".\n";
    }
    return decision;
}

} // namespace Robix::Tools
