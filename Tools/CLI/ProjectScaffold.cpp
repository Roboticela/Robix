#include "ProjectScaffold.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace Robix::Tools {

struct Service {
    std::string name;
    bool gui = false;
    bool typeSet = false;
};

namespace {

std::string Trim(std::string text) {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) text.pop_back();
    std::size_t start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) ++start;
    return text.substr(start);
}

bool ReadLine(std::string& line) {
    if (!std::getline(std::cin, line)) return false;
    line = Trim(line);
    return true;
}

bool IsReservedFolderName(std::string name) {
    for (char& ch : name) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (name == "CON" || name == "PRN" || name == "AUX" || name == "NUL") return true;
    if (name.size() == 4 && (name.rfind("COM", 0) == 0 || name.rfind("LPT", 0) == 0) &&
        name[3] >= '1' && name[3] <= '9') {
        return true;
    }
    return false;
}

bool IsFolderName(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    if (name.back() == ' ' || name.back() == '.') return false;
    const unsigned char first = static_cast<unsigned char>(name[0]);
    if (!(std::isalpha(first) || name[0] == '_')) return false;
    for (const unsigned char ch : name) {
        if (std::isalnum(ch) || ch == '_') continue;
        return false;
    }
    return !IsReservedFolderName(name);
}

std::string NameError(const std::string& name) {
    return "robix new: '" + name +
           "' cannot be a folder name. Start with a letter, then use letters, digits, or underscores.\n";
}

std::string Lower(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

std::string CanonicalCategory(std::string text) {
    text = Lower(std::move(text));
    if (text == "1" || text == "application" || text == "app") return "Application";
    if (text == "2" || text == "canvas") return "Canvas";
    if (text == "3" || text == "bedrock") return "Bedrock";
    return {};
}

std::string CanonicalKind(std::string text) {
    text = Lower(std::move(text));
    if (text == "console" || text == "cli") return "Console";
    if (text == "window" || text == "gui") return "Window";
    if (text == "network") return "Network";
    if (text == "empty" || text == "blank") return "Empty";
    if (text == "kernel") return "Kernel";
    if (text == "os" || text == "operatingsystem") return "OS";
    if (text == "bios") return "BIOS";
    if (text == "driver") return "Driver";
    if (text == "bootloader" || text == "boot") return "Bootloader";
    if (text == "firmware") return "Firmware";
    return {};
}

std::string CategoryForKind(const std::string& kind) {
    if (kind == "Console" || kind == "Window" || kind == "Network") return "Application";
    if (kind == "Empty") return "Canvas";
    if (kind == "Kernel" || kind == "OS" || kind == "BIOS" || kind == "Driver" ||
        kind == "Bootloader" || kind == "Firmware") {
        return "Bedrock";
    }
    return {};
}

bool IsMachineKind(const std::string& kind) {
    return CategoryForKind(kind) == "Bedrock";
}

bool WriteText(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

bool WriteGitignore(const fs::path& root) {
    return WriteText(root / ".gitignore", "Build/\n");
}

std::string HelloFile(const std::string& packageName) {
    return "package " + packageName + ";\n\n"
           "public Function Hello() -> Int {\n"
           "    return 1;\n"
           "}\n\n"
           "public Function Main() -> Void {\n"
           "    Print(\"Hello, World!\");\n"
           "}\n";
}

std::string EmptyFile(const std::string& packageName) {
    return "package " + packageName + ";\n\n"
           "public Function Main() -> Void {\n"
           "}\n";
}

std::string ClassFile(const std::string& packageName, const std::string& className) {
    return "package " + packageName + ";\n\n"
           "public class " + className + " {\n\n"
           "    public Function Hello() -> Int {\n"
           "        return 1;\n"
           "    }\n"
           "}\n\n"
           "public Function Main() -> Void {\n"
           "    " + className + " value = new " + className + "();\n"
           "    Print(value.Hello());\n"
           "}\n";
}

std::string PackageFile(const std::string& packageName) {
    return "package " + packageName + ";\n\n"
           "public Function PackageValue() -> Int {\n"
           "    return 1;\n"
           "}\n";
}

std::string TestFile(const std::string& packageName, const std::string& importName, const std::string& body) {
    return "package " + packageName + ".Tests;\n\n"
           "import " + importName + ";\n\n"
           "Test native \"" + importName + " is ready\" {\n"
           + body +
           "}\n";
}

bool WriteSourceAndTest(const fs::path& sourceDir, const fs::path& testDir, const std::string& packageName,
                        const std::string& fileName, const std::string& importName, const std::string& source,
                        const std::string& testBody) {
    const fs::path sourcePath = sourceDir / (fileName + ".rbx");
    const fs::path testPath = testDir / (fileName + ".test.rbx");
    if (fs::exists(sourcePath) || fs::exists(testPath)) {
        std::cerr << "robix new: '" << sourcePath.string() << "' already exists.\n";
        return false;
    }
    if (!WriteText(sourcePath, source) || !WriteText(testPath, TestFile(packageName, importName, testBody))) {
        std::cerr << "robix new: could not write '" << fileName << "'.\n";
        return false;
    }
    std::cout << "robix new: wrote " << sourcePath.string() << "\n";
    std::cout << "robix new: wrote " << testPath.string() << "\n";
    return true;
}

std::string JoinList(const std::vector<std::string>& items) {
    std::string list;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) list += ", ";
        list += items[i];
    }
    return list;
}

struct BuildInfo {
    std::string output = "Executable";
    std::string target = "x86_64-pc-windows-msvc";
    std::string entrySymbol = "Main";
    std::string profile = "release";
};

bool WriteBuildFile(const fs::path& root, const std::string& name, const std::string& entry,
                    const std::vector<std::string>& order, const std::vector<std::string>& ui,
                    const BuildInfo& info = {}) {
    std::string text =
        "Output: " + info.output + "\n"
        "Target: " + info.target + "\n"
        "Config: release\n"
        "Profile: " + info.profile + "\n"
        "Entry: " + info.entrySymbol + "\n"
        "Source: " + entry + "\n"
        "Sources: " + JoinList(order) + "\n"
        "Order: " + JoinList(order) + "\n";
    if (!ui.empty()) text += "UI: " + JoinList(ui) + "\n";
    if (!WriteText(root / (name + ".rbxb"), text)) {
        std::cerr << "robix new: could not write the build file.\n";
        return false;
    }
    std::cout << "robix new: wrote " << (root / (name + ".rbxb")).string() << "\n";
    return true;
}

std::string AskName(const char* prompt) {
    for (;;) {
        std::cout << prompt;
        std::cout.flush();
        std::string line;
        if (!ReadLine(line)) {
            std::cerr << "robix new: a name is required.\n";
            return {};
        }
        if (IsFolderName(line)) return line;
        std::cerr << NameError(line);
    }
}

std::string AskListed(const char* menu, const char* prompt, const char* reject,
                     std::string (*parse)(std::string)) {
    for (;;) {
        std::cout << menu << prompt;
        std::cout.flush();
        std::string line;
        if (!ReadLine(line)) {
            std::cerr << "robix new: a choice is required.\n";
            return {};
        }
        if (line.empty()) {
            std::cerr << "robix new: choose a number or a name from the list.\n";
            continue;
        }
        const std::string choice = parse(std::move(line));
        if (!choice.empty()) return choice;
        std::cerr << reject;
    }
}

std::string ParseApplicationKind(std::string text) {
    text = Lower(std::move(text));
    if (text == "1" || text == "console" || text == "cli") return "Console";
    if (text == "2" || text == "window" || text == "gui") return "Window";
    if (text == "3" || text == "network") return "Network";
    return {};
}

std::string ParseBedrockKind(std::string text) {
    text = Lower(std::move(text));
    if (text == "1" || text == "kernel") return "Kernel";
    if (text == "2" || text == "os" || text == "operatingsystem") return "OS";
    if (text == "3" || text == "bios") return "BIOS";
    if (text == "4" || text == "driver") return "Driver";
    if (text == "5" || text == "bootloader" || text == "boot") return "Bootloader";
    if (text == "6" || text == "firmware") return "Firmware";
    return {};
}

std::string AskCategory() {
    return AskListed(
        "Project:\n"
        "  1 Application   A program a person runs: a console, a window, or a client and a server.\n"
        "  2 Canvas        An empty folder. Run a file on a platform, or build it as a machine image.\n"
        "  3 Bedrock       The machine itself: kernel, operating system, BIOS, driver, bootloader, and firmware.\n",
        "Project: ",
        "robix new: project must be Application, Canvas, or Bedrock.\n",
        CanonicalCategory);
}

std::string AskKindForCategory(const std::string& category) {
    if (category == "Canvas") return "Empty";
    if (category == "Application") {
        return AskListed(
            "Application:\n"
            "  1 Console   Text in a terminal.\n"
            "  2 Window    A window, a .rbxu file, and a small test.\n"
            "  3 Network   Clients and servers that share one contract.\n",
            "Application: ",
            "robix new: application must be Console, Window, or Network.\n",
            ParseApplicationKind);
    }
    if (category == "Bedrock") {
        return AskListed(
            "Bedrock:\n"
            "  1 Kernel       Runs after the machine starts.\n"
            "  2 OS           A small operating-system image.\n"
            "  3 BIOS         The program that runs at reset.\n"
            "  4 Driver       Talks to one device.\n"
            "  5 Bootloader   Loads the kernel.\n"
            "  6 Firmware     Software stored on the board.\n",
            "Bedrock: ",
            "robix new: bedrock must be Kernel, OS, BIOS, Driver, Bootloader, or Firmware.\n",
            ParseBedrockKind);
    }
    return {};
}

bool AskCount(const char* prompt, int& count) {
    for (;;) {
        std::cout << prompt;
        std::cout.flush();
        std::string line;
        if (!ReadLine(line)) {
            std::cerr << "robix new: a count is required.\n";
            return false;
        }
        if (line.empty()) {
            count = 1;
            return true;
        }
        bool digits = !line.empty();
        for (const unsigned char ch : line) digits = digits && std::isdigit(ch);
        if (!digits) {
            std::cerr << "robix new: enter a number, or press Enter for 1. Zero skips that side.\n";
            continue;
        }
        try {
            count = std::stoi(line);
        } catch (...) {
            std::cerr << "robix new: enter a number, or press Enter for 1. Zero skips that side.\n";
            continue;
        }
        if (count < 0) {
            std::cerr << "robix new: the count cannot be negative.\n";
            continue;
        }
        return true;
    }
}

std::string CanonicalSurface(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (text == "1" || text == "cli") return "CLI";
    if (text == "2" || text == "gui") return "GUI";
    return {};
}

bool AskSurface(const std::string& label, bool& gui) {
    for (;;) {
        std::cout << label << " type:\n"
                     "  1 CLI\n"
                     "  2 GUI\n"
                     "Type [CLI]: ";
        std::cout.flush();
        std::string line;
        if (!ReadLine(line)) {
            std::cerr << "robix new: a type is required.\n";
            return false;
        }
        if (line.empty()) {
            gui = false;
            return true;
        }
        const std::string surface = CanonicalSurface(line);
        if (surface == "CLI") {
            gui = false;
            return true;
        }
        if (surface == "GUI") {
            gui = true;
            return true;
        }
        std::cerr << "robix new: type must be CLI or GUI.\n";
    }
}

bool AskServices(const char* label, int count, std::vector<Service>& services) {
    while (static_cast<int>(services.size()) < count) {
        Service service;
        service.name = AskName((std::string(label) + " " + std::to_string(services.size() + 1) + " name: ").c_str());
        if (service.name.empty()) return false;
        if (!AskSurface(std::string(label) + " " + service.name, service.gui)) return false;
        service.typeSet = true;
        services.push_back(std::move(service));
    }
    if (static_cast<int>(services.size()) > count) services.resize(static_cast<std::size_t>(count));
    for (auto& service : services) {
        if (!service.typeSet && !AskSurface(std::string(label) + " " + service.name, service.gui)) return false;
        service.typeSet = true;
    }
    return true;
}

std::string WindowFile(const std::string& serviceName, const std::string& handler) {
    return "<RobixUI Version=\"1.0\">\n"
           "  <Window Name=\"" + serviceName + "Window\" Title=\"" + serviceName +
           "\" Width=\"320\" Height=\"180\" Theme=\"Light\" Padding=\"16\">\n"
           "    <Column Spacing=\"16\">\n"
           "      <Text Name=\"Title\" Value=\"" + serviceName + "\" FontSize=\"16\"/>\n"
           "      <Button Name=\"ActionButton\" Text=\"Send\" OnClick=\"" + handler + "\"/>\n"
           "      <Text Name=\"StatusText\" Value=\"Ready\"/>\n"
           "    </Column>\n"
           "  </Window>\n"
           "</RobixUI>\n";
}

std::string ServerCall(const std::vector<std::string>& servers) {
    std::string body;
    for (std::size_t i = 0; i < servers.size(); ++i) {
        const std::string variable = "server" + std::to_string(i + 1);
        body += "        " + servers[i] + " " + variable + " = new " + servers[i] + "();\n";
        body += "        Print(" + variable + ".SendText(\"How are you\"));\n";
    }
    return body;
}

bool CreateProject(const fs::path& root, const std::string& name, const std::string& kind,
                   const std::vector<Service>& clients, const std::vector<Service>& servers) {
    std::error_code ec;
    if (fs::exists(root / (name + ".rbxproj"))) {
        std::cerr << "robix new: '" << (root / (name + ".rbxproj")).string() << "' already exists.\n";
        return false;
    }
    if (!WriteGitignore(root)) {
        std::cerr << "robix new: could not write .gitignore.\n";
        return false;
    }
    if (kind == "Empty") {
        std::ofstream project(root / (name + ".rbxproj"), std::ios::binary | std::ios::trunc);
        if (!project) {
            std::cerr << "robix new: could not write the project file.\n";
            return false;
        }
        project << "Name: " << name << "\nCategory: Canvas\nKind: Empty\n";
        std::cout << "robix new: wrote " << (root / (name + ".rbxproj")).string()
                  << " category Canvas kind Empty\n";
        std::cout << "robix new: empty folder. Add .rbx files, then robix run File.rbx or robix run File.rbx --image.\n";
        return true;
    }
    if (kind != "Network") {
        fs::create_directories(root / "Source", ec);
        fs::create_directories(root / "Tests", ec);
        if (ec) {
            std::cerr << "robix new: could not create '" << root.string() << "'.\n";
            return false;
        }
    }

    std::string source;
    std::string testBody = "    Assert.Equal(1, 1);\n";
    std::string fileName = "Program";
    std::string entry = "Source/Program.rbx";
    BuildInfo build;
    std::vector<std::string> ui;
    if (kind == "Empty") {
        source = EmptyFile(name);
    } else if (kind == "Console") {
        source = "package " + name + ";\n\n"
                 "public class Program {\n\n"
                 "    public Function ArgumentCount() -> Int {\n"
                 "        return Args.Count();\n"
                 "    }\n\n"
                 "    public Function Show() -> Int {\n"
                 "        Int count = Args.Count();\n"
                 "        Print(count);\n"
                 "        Int index = 0;\n"
                 "        while (index < count) {\n"
                 "            Print(Args.Get(index));\n"
                 "            index = index + 1;\n"
                 "        }\n"
                 "        return count;\n"
                 "    }\n"
                 "}\n\n"
                 "public Function Main() -> Void {\n"
                 "    Program program = new Program();\n"
                 "    program.Show();\n"
                 "}\n";
        testBody = "    Program program = new Program();\n    Assert.Equal(program.ArgumentCount(), 0);\n";
    } else if (kind == "Window") {
        source = "package " + name + ";\n\n"
                 "public class Program {\n\n"
                 "    public Function Greet() -> Int {\n"
                 "        return 1;\n"
                 "    }\n\n"
                 "    public Function SayHello() -> Void {\n"
                 "        Print(\"Hello from RobixUI\");\n"
                 "    }\n"
                 "}\n\n"
                 "public Function Main() -> Void {\n"
                 "    Program program = new Program();\n"
                 "    program.SayHello();\n"
                 "}\n";
        testBody = "    Program program = new Program();\n    Assert.Equal(program.Greet(), 1);\n";
        fs::create_directories(root / "UI", ec);
        const fs::path window = root / "UI" / "MainWindow.rbxu";
        const std::string markup =
            "<RobixUI Version=\"1.0\">\n"
            "  <Window Name=\"MainWindow\" Title=\"" + name +
            "\" Width=\"640\" Height=\"400\" Theme=\"Light\" Padding=\"16\">\n"
            "    <Column Spacing=\"16\">\n"
            "      <Text Name=\"Title\" Value=\"" + name + "\" FontSize=\"28\" FontWeight=\"Bold\"/>\n"
            "      <Text Name=\"Tagline\" Value=\"The Language of Intelligent Machines.\"/>\n"
            "      <Button Name=\"HelloButton\" Text=\"Say Hello\" OnClick=\"SayHello\"/>\n"
            "      <Text Name=\"StatusText\" Value=\"Ready\"/>\n"
            "    </Column>\n"
            "  </Window>\n"
            "</RobixUI>\n";
        if (!WriteText(window, markup)) {
            std::cerr << "robix new: could not write '" << window.string() << "'.\n";
            return false;
        }
        std::cout << "robix new: wrote " << window.string() << "\n";
        ui.push_back("UI/MainWindow.rbxu");
    } else if (IsMachineKind(kind)) {
        fileName = "Start";
        entry = "Source/Start.rbx";
        build.output = (kind == "BIOS" || kind == "Firmware") ? "Firmware" : "Kernel";
        build.target = "x86_64-unknown-none";
        build.entrySymbol = "Start";
        build.profile = "embedded";
        if (kind == "OS") {
            source = "package " + name + ";\n\n"
                     "public Function Boot() -> Int {\n"
                     "    return 1;\n"
                     "}\n\n"
                     "public Function Start() -> Void {\n"
                     "    Unsafe {\n"
                     "        if (Boot() == 1) {\n"
                     "            Console.Write(82);\n"
                     "        }\n"
                     "        Cpu.Halt();\n"
                     "    }\n"
                     "}\n";
            testBody = "    Assert.Equal(Boot(), 1);\n";
        } else if (kind == "Driver") {
            source = "package " + name + ";\n\n"
                     "public Function StatusPort() -> Int {\n"
                     "    return 503;\n"
                     "}\n\n"
                     "public Function Start() -> Void {\n"
                     "    Unsafe {\n"
                     "        Int status = Port.Read8(StatusPort());\n"
                     "        Console.Write(status);\n"
                     "        Cpu.Halt();\n"
                     "    }\n"
                     "}\n";
            testBody = "    Assert.Equal(StatusPort(), 503);\n";
        } else if (kind == "Bootloader") {
            source = "package " + name + ";\n\n"
                     "public Function KernelAddress() -> Int {\n"
                     "    return 1048576;\n"
                     "}\n\n"
                     "public Function Start() -> Void {\n"
                     "    Unsafe {\n"
                     "        Console.Write(82);\n"
                     "        Cpu.Halt();\n"
                     "    }\n"
                     "}\n";
            testBody = "    Assert.Equal(KernelAddress(), 1048576);\n";
        } else {
            source = "package " + name + ";\n\n"
                     "public Function Mark() -> Int {\n"
                     "    return 82;\n"
                     "}\n\n"
                     "public Function Start() -> Void {\n"
                     "    Unsafe {\n"
                     "        Console.Write(Mark());\n"
                     "        Cpu.Halt();\n"
                     "    }\n"
                     "}\n";
            testBody = "    Assert.Equal(Mark(), 82);\n";
        }
    } else if (kind == "Network") {
        for (const auto& client : clients) {
            if (client.name == "Common" || client.name == "Packages") {
                std::cerr << "robix new: '" << client.name << "' is reserved in a Network project.\n";
                return false;
            }
        }
        for (const auto& server : servers) {
            if (server.name == "Common" || server.name == "Packages") {
                std::cerr << "robix new: '" << server.name << "' is reserved in a Network project.\n";
                return false;
            }
            for (const auto& client : clients) {
                if (client.name == server.name) {
                    std::cerr << "robix new: '" << client.name << "' cannot be both a client and a server.\n";
                    return false;
                }
            }
        }
        fs::create_directories(root / "Common" / "Source", ec);
        fs::create_directories(root / "Common" / "Tests", ec);
        fs::create_directories(root / "Packages", ec);
        std::vector<std::string> order;
        auto writeInterface = [&](const std::string& serviceName, const std::string& definition) {
            const std::string source =
                "package " + name + ".Common;\n\n"
                "public interface " + serviceName + " {\n"
                "    " + definition + "\n"
                "}\n";
            if (!WriteSourceAndTest(root / "Common" / "Source", root / "Common" / "Tests", name + ".Common",
                                    serviceName, "Common.Source." + serviceName, source,
                                    "    Assert.Equal(1, 1);\n")) {
                return false;
            }
            order.push_back("Common/Source/" + serviceName + ".rbx");
            return true;
        };
        if (clients.empty() && servers.empty()) {
            const std::string shared =
                "package " + name + ".Common;\n\npublic Function SharedReady() -> Int {\n    return 1;\n}\n";
            if (!WriteSourceAndTest(root / "Common" / "Source", root / "Common" / "Tests", name + ".Common",
                                    "Shared", "Common.Source.Shared", shared,
                                    "    Assert.Equal(SharedReady(), 1);\n")) {
                return false;
            }
            order.push_back("Common/Source/Shared.rbx");
        }
        for (const auto& client : clients) {
            if (!writeInterface(client.name, "public Function Ask(String text) -> String;")) return false;
        }
        for (const auto& server : servers) {
            if (!writeInterface(server.name, "public Function SendText(String text) -> String;")) return false;
        }
        auto writeEnv = [&](const fs::path& path, const std::string& text) {
            return WriteText(path, text);
        };
        std::ostringstream commonEnv;
        commonEnv << "SECRET=robix-local-secret\n";
        int port = 4010;
        for (const auto& client : clients) {
            commonEnv << client.name << "_HOST=127.0.0.1\n" << client.name << "_PORT=" << port++ << "\n";
        }
        for (const auto& server : servers) {
            commonEnv << server.name << "_HOST=127.0.0.1\n" << server.name << "_PORT=" << port++ << "\n";
        }
        const std::string commonText = commonEnv.str();
        if (!writeEnv(root / ".env.client", "") ||
            !writeEnv(root / ".env.server", "") ||
            !writeEnv(root / "exp.env.client", "") ||
            !writeEnv(root / "exp.env.server", "") ||
            !writeEnv(root / ".env.common", commonText) ||
            !writeEnv(root / "exp.env.common", commonText)) {
            std::cerr << "robix new: could not write the environment files.\n";
            return false;
        }
        auto peerCalls = [&](const std::string& self) {
            std::string calls;
            for (const auto& client : clients) {
                if (client.name == self) continue;
                calls += "    Print(" + client.name + ".Ask(\"How are you\"));\n";
            }
            for (const auto& server : servers) {
                if (server.name == self) continue;
                calls += "    Print(" + server.name + ".SendText(\"How are you\"));\n";
            }
            if (calls.empty()) calls = "    Print(0);\n";
            return calls;
        };
        auto peerImports = [&](const std::string& self) {
            std::string imports;
            for (const auto& client : clients) {
                if (client.name == self) continue;
                imports += "import Common.Source." + client.name + ";\n";
            }
            for (const auto& server : servers) {
                if (server.name == self) continue;
                imports += "import Common.Source." + server.name + ";\n";
            }
            return imports;
        };
        std::vector<std::string> ui;
        for (const auto& client : clients) {
            const fs::path dir = root / client.name;
            const std::string clientSource =
                "package " + name + "." + client.name + ";\n\n" + peerImports(client.name) +
                "\npublic Function Ask(String text) -> String {\n"
                "    return text;\n"
                "}\n\n"
                "public Function SendMessage() -> Void {\n" +
                peerCalls(client.name) +
                "}\n\n"
                "public Function Main() -> Void {\n"
                "    SendMessage();\n"
                "}\n";
            if (!WriteSourceAndTest(dir / "Source", dir / "Tests", name + "." + client.name, client.name,
                                    client.name + ".Source." + client.name, clientSource,
                                    "    Assert.Equal(Ask(\"How are you\").Length(), 10);\n") ||
                !writeEnv(dir / ".env", "") ||
                !writeEnv(dir / "exp.env", "")) {
                std::cerr << "robix new: could not write client '" << client.name << "'.\n";
                return false;
            }
            if (client.gui) {
                const fs::path window = dir / (client.name + ".rbxu");
                if (!WriteText(window, WindowFile(client.name, "SendMessage"))) {
                    std::cerr << "robix new: could not write '" << window.string() << "'.\n";
                    return false;
                }
                std::cout << "robix new: wrote " << window.string() << "\n";
                ui.push_back(client.name + "/" + client.name + ".rbxu");
            }
            order.push_back(client.name + "/Source/" + client.name + ".rbx");
        }
        for (const auto& server : servers) {
            const fs::path dir = root / server.name;
            const std::string serverSource =
                "package " + name + "." + server.name + ";\n\n" + peerImports(server.name) +
                "\npublic Function SendText(String text) -> String {\n"
                "    return \"Hello, \" + text;\n"
                "}\n\n"
                "public Function ShowReply() -> Void {\n" +
                peerCalls(server.name) +
                "}\n\n"
                "public Function Main() -> Void {\n"
                "    ShowReply();\n"
                "}\n";
            if (!WriteSourceAndTest(dir / "Source", dir / "Tests", name + "." + server.name, server.name,
                                    server.name + ".Source." + server.name, serverSource,
                                    "    Assert.Equal(SendText(\"How are you\").Length(), 18);\n") ||
                !writeEnv(dir / ".env", "") ||
                !writeEnv(dir / "exp.env", "")) {
                std::cerr << "robix new: could not write server '" << server.name << "'.\n";
                return false;
            }
            if (server.gui) {
                const fs::path window = dir / (server.name + ".rbxu");
                if (!WriteText(window, WindowFile(server.name, "ShowReply"))) {
                    std::cerr << "robix new: could not write '" << window.string() << "'.\n";
                    return false;
                }
                std::cout << "robix new: wrote " << window.string() << "\n";
                ui.push_back(server.name + "/" + server.name + ".rbxu");
            }
            order.push_back(server.name + "/Source/" + server.name + ".rbx");
        }
        const std::string entry = !clients.empty() ? clients[0].name + "/Source/" + clients[0].name + ".rbx"
                                 : !servers.empty() ? servers[0].name + "/Source/" + servers[0].name + ".rbx"
                                                    : "Common/Source/Shared.rbx";
        if (!WriteBuildFile(root, name, entry, order, ui)) return false;
        std::ofstream project(root / (name + ".rbxproj"), std::ios::binary | std::ios::trunc);
        if (!project) {
            std::cerr << "robix new: could not write the project file.\n";
            return false;
        }
        project << "Name: " << name << "\nCategory: Application\nKind: Network\nSourceDir: Common/Source\nNetwork: true\n";
        std::cout << "robix new: wrote " << (root / (name + ".rbxproj")).string()
                  << " category Application kind Network\n";
        return true;
    }

    if (source.empty()) {
        std::cerr << "robix new: unknown project kind '" << kind << "'.\n";
        return false;
    }

    if (kind == "Console") fs::create_directories(root / "Packages", ec);
    if (!WriteSourceAndTest(root / "Source", root / "Tests", name, fileName, "Source." + fileName, source, testBody)) {
        return false;
    }
    if (!WriteBuildFile(root, name, entry, { entry }, ui, build)) return false;
    std::ofstream project(root / (name + ".rbxproj"), std::ios::binary | std::ios::trunc);
    if (!project) {
        std::cerr << "robix new: could not write the project file.\n";
        return false;
    }
    const std::string category = CategoryForKind(kind);
    project << "Name: " << name << "\nCategory: " << category << "\nKind: " << kind << "\nSourceDir: Source\n";
    if (IsMachineKind(kind)) project << "Output: " << build.output << "\n";
    std::cout << "robix new: wrote " << (root / (name + ".rbxproj")).string()
              << " category " << category << " kind " << kind << "\n";
    return true;
}

bool AddFile(const fs::path& root, const std::string& fileName, const std::string& kindTemplate) {
    fs::path projectDir = root;
    std::error_code ec;
    fs::path sourceDir = projectDir / "Source";
    fs::path testDir = projectDir / "Tests";
    std::string importPrefix = "Source.";
    if (!fs::is_directory(sourceDir, ec) && fs::is_directory(projectDir / "Common" / "Source", ec)) {
        sourceDir = projectDir / "Common" / "Source";
        testDir = projectDir / "Common" / "Tests";
        importPrefix = "Common.Source.";
    }
    if (!fs::is_directory(sourceDir, ec)) {
        std::cerr << "robix new: no Source directory in '" << projectDir.string() << "'. Create a project first.\n";
        return false;
    }
    std::string packageName = fileName;
    for (const auto& entry : fs::directory_iterator(projectDir, ec)) {
        if (entry.path().extension() == ".rbxproj") {
            std::ifstream in(entry.path());
            std::string line;
            while (std::getline(in, line)) {
                line = Trim(line);
                if (line.rfind("Name:", 0) == 0) packageName = Trim(line.substr(5));
            }
        }
    }
    std::string source;
    std::string testBody = "    Assert.Equal(Hello(), 1);\n";
    if (kindTemplate == "empty") {
        source = EmptyFile(packageName);
        testBody = "    Assert.Equal(1, 1);\n";
    } else if (kindTemplate == "class") {
        source = ClassFile(packageName, fileName);
        testBody = "    " + fileName + " value = new " + fileName + "();\n    Assert.Equal(value.Hello(), 1);\n";
    } else if (kindTemplate == "package") {
        source = PackageFile(packageName);
        testBody = "    Assert.Equal(PackageValue(), 1);\n";
    } else {
        source = HelloFile(packageName);
    }
    return WriteSourceAndTest(sourceDir, testDir, packageName, fileName, importPrefix + fileName, source, testBody);
}

} // namespace

int RunNewCommand(const std::vector<std::string>& args) {
    if (args.empty() || args[0] != "new") return -1;
    bool here = false;
    bool fileMode = false;
    bool nameFromArgs = false;
    bool kindFromArgs = false;
    bool categoryFromArgs = false;
    std::string name;
    std::string kind;
    std::string category;
    std::string fileTemplate = "hello";
    int clientCount = -1;
    int serverCount = -1;
    std::vector<Service> clients;
    std::vector<Service> servers;

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& token = args[i];
        if (token == "--clients" || token == "--servers") {
            if (i + 1 >= args.size()) {
                std::cerr << "robix new: " << token << " needs a number.\n";
                return 1;
            }
            int count = 0;
            const std::string& number = args[++i];
            for (const unsigned char ch : number) {
                if (!std::isdigit(ch)) {
                    std::cerr << "robix new: " << token << " needs a number.\n";
                    return 1;
                }
            }
            count = std::stoi(number);
            if (token == "--clients") clientCount = count;
            else serverCount = count;
            continue;
        }
        if (token == "--client" || token == "--server") {
            if (i + 1 >= args.size() || !IsFolderName(args[i + 1])) {
                std::cerr << NameError(i + 1 < args.size() ? args[i + 1] : "");
                return 1;
            }
            Service service;
            service.name = args[++i];
            if (i + 1 < args.size()) {
                const std::string surface = CanonicalSurface(args[i + 1]);
                if (!surface.empty()) {
                    service.gui = surface == "GUI";
                    service.typeSet = true;
                    ++i;
                }
            }
            if (token == "--client") clients.push_back(std::move(service));
            else servers.push_back(std::move(service));
            continue;
        }
        if (token == "--template" && i + 1 < args.size()) {
            fileTemplate = args[++i];
            continue;
        }
        if (!fileMode && token == "file") {
            fileMode = true;
            continue;
        }
        if (!here && token == "." && name.empty() && !fileMode) {
            here = true;
            continue;
        }
        if (!fileMode && name.empty()) {
            const std::string maybeCategory = CanonicalCategory(token);
            const std::string maybeKind = CanonicalKind(token);
            if (!maybeCategory.empty() && !categoryFromArgs) {
                category = maybeCategory;
                categoryFromArgs = true;
            } else if (!maybeKind.empty() && !kindFromArgs) {
                kind = maybeKind;
                kindFromArgs = true;
            } else {
                name = token;
                nameFromArgs = true;
            }
            continue;
        }
        if (!fileMode && !categoryFromArgs) {
            category = CanonicalCategory(token);
            if (!category.empty()) {
                categoryFromArgs = true;
                continue;
            }
        }
        if (!fileMode && !kindFromArgs) {
            kind = CanonicalKind(token);
            if (kind.empty()) {
                std::cerr << "robix new: '" << token
                          << "' is not a project. Choose Application, Canvas, or Bedrock, then a kind.\n";
                return 1;
            }
            kindFromArgs = true;
            continue;
        }
        if (fileMode && name.empty()) {
            name = token;
            nameFromArgs = true;
            continue;
        }
        if (fileMode) {
            const std::string lower = [&] {
                std::string text = token;
                for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                return text;
            }();
            if (lower == "hello" || lower == "empty" || lower == "class" || lower == "package") {
                fileTemplate = lower;
                continue;
            }
        }
        std::cerr << "robix new: unexpected '" << token << "'.\n";
        return 1;
    }

    if (fileMode) {
        if (!nameFromArgs) name = AskName("File name: ");
        if (name.empty() || !IsFolderName(name)) {
            if (nameFromArgs) std::cerr << NameError(name);
            return 1;
        }
        std::string lower = fileTemplate;
        for (char& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (lower != "hello" && lower != "empty" && lower != "class" && lower != "package") {
            std::cerr << "robix new: template must be hello, empty, class, or package.\n";
            return 1;
        }
        return AddFile(fs::current_path(), name, lower) ? 0 : 1;
    }

    if (!nameFromArgs) name = AskName("Project name: ");
    if (name.empty()) return 1;
    if (!IsFolderName(name)) {
        std::cerr << NameError(name);
        return 1;
    }
    if (!categoryFromArgs) {
        category = kindFromArgs ? CategoryForKind(kind) : AskCategory();
    }
    if (category.empty()) return 1;
    if (!kindFromArgs) kind = AskKindForCategory(category);
    if (kind.empty()) return 1;
    const std::string kindCategory = CategoryForKind(kind);
    if (kindCategory != category) {
        std::cerr << "robix new: " << kind << " belongs to " << kindCategory << ", not " << category << ".\n";
        return 1;
    }

    if (kind == "Network") {
        if (clientCount < 0 && !AskCount("Number of clients [1]: ", clientCount)) return 1;
        if (serverCount < 0 && !AskCount("Number of servers [1]: ", serverCount)) return 1;
        if (clientCount < 0) clientCount = 1;
        if (serverCount < 0) serverCount = 1;
        if (!AskServices("Client", clientCount, clients)) return 1;
        if (!AskServices("Server", serverCount, servers)) return 1;
    }

    const fs::path root = here ? fs::current_path() : fs::current_path() / name;
    if (!here && kind == "Network" && (fs::exists(root / "Common") || fs::exists(root / (name + ".rbxproj")))) {
        std::cerr << "robix new: '" << root.string() << "' already has a project.\n";
        return 1;
    }
    if (!here && kind != "Network" && fs::exists(root / "Source")) {
        std::cerr << "robix new: '" << root.string() << "' already has a Source directory.\n";
        return 1;
    }
    return CreateProject(root, name, kind, clients, servers) ? 0 : 1;
}

} // namespace Robix::Tools
