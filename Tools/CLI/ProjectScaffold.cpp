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

std::string CanonicalKind(std::string text) {
    for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (text == "1" || text == "blank") return "Blank";
    if (text == "2" || text == "cli") return "CLI";
    if (text == "3" || text == "network") return "Network";
    return {};
}

bool WriteText(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
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

bool WriteBuildFile(const fs::path& root, const std::string& name, const std::string& entry,
                    const std::vector<std::string>& order, const std::vector<std::string>& ui) {
    std::string text =
        "Output: Executable\n"
        "Target: x86_64-pc-windows-msvc\n"
        "Config: release\n"
        "Profile: release\n"
        "Entry: Main\n"
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

std::string AskKind() {
    for (;;) {
        std::cout << "Project kind:\n"
                     "  1 Blank    Separate files. Each file runs on its own.\n"
                     "  2 CLI      A command-line application.\n"
                     "  3 Network  Clients and servers that share one contract.\n"
                     "Kind [Blank]: ";
        std::cout.flush();
        std::string line;
        if (!ReadLine(line)) {
            std::cerr << "robix new: a project kind is required.\n";
            return {};
        }
        if (line.empty()) return "Blank";
        const std::string kind = CanonicalKind(line);
        if (!kind.empty()) return kind;
        std::cerr << "robix new: kind must be Blank, CLI, or Network.\n";
    }
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
    return "<?xml version=\"1.0\"?>\n"
           "<RobixUI Version=\"1.0\">\n"
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
    if (kind != "Network") {
        fs::create_directories(root / "Source", ec);
        fs::create_directories(root / "Tests", ec);
        if (ec) {
            std::cerr << "robix new: could not create '" << root.string() << "'.\n";
            return false;
        }
    }

    std::string source;
    std::string testBody = "    Assert.Equal(Hello(), 1);\n";
    if (kind == "Blank") {
        source = HelloFile(name);
    } else if (kind == "CLI") {
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
    } else {
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
        if (servers.empty()) {
            const std::string shared =
                "package " + name + ".Common;\n\npublic Function SharedReady() -> Int {\n    return 1;\n}\n";
            if (!WriteSourceAndTest(root / "Common" / "Source", root / "Common" / "Tests", name + ".Common",
                                    "Shared", "Common.Source.Shared", shared,
                                    "    Assert.Equal(SharedReady(), 1);\n")) {
                return false;
            }
            order.push_back("Common/Source/Shared.rbx");
        } else {
            for (const auto& server : servers) {
                const std::string service =
                    "package " + name + ".Common;\n\n"
                    "public class " + server.name + " {\n\n"
                    "    public Function SendText(String text) -> String {\n"
                    "        return Network.SendText(text);\n"
                    "    }\n"
                    "}\n";
                const std::string serviceTest =
                    "    " + server.name + " server1 = new " + server.name + "();\n"
                    "    String reply = server1.SendText(\"How are you\");\n"
                    "    Assert.Equal(reply.Length(), 18);\n";
                if (!WriteSourceAndTest(root / "Common" / "Source", root / "Common" / "Tests", name + ".Common",
                                        server.name, "Common.Source." + server.name, service, serviceTest)) {
                    return false;
                }
                order.push_back("Common/Source/" + server.name + ".rbx");
            }
        }
        auto writeEnv = [&](const fs::path& path, const std::string& text) {
            return WriteText(path, text);
        };
        if (!writeEnv(root / ".env", "APP_NAME=" + name + "\n") ||
            !writeEnv(root / ".env.client", "PUBLIC_HOST=127.0.0.1\n") ||
            !writeEnv(root / ".env.server", "SERVER_TOKEN=local-dev-token\n") ||
            !writeEnv(root / "exp.env.client", "PUBLIC_HOST=127.0.0.1\n") ||
            !writeEnv(root / "exp.env.server", "SERVER_TOKEN=local-dev-token\n")) {
            std::cerr << "robix new: could not write the environment files.\n";
            return false;
        }
        int port = 4010;
        std::vector<std::string> serverNames;
        for (const auto& server : servers) serverNames.push_back(server.name);
        std::string imports;
        for (const auto& serverName : serverNames) imports += "import Common.Source." + serverName + ";\n";
        std::vector<std::string> ui;
        for (const auto& client : clients) {
            const fs::path dir = root / client.name;
            const std::string action = client.gui
                ? "    public Function SendMessage() -> Void {\n" +
                      (servers.empty() ? "        Print(0);\n" : ServerCall(serverNames)) +
                      "    }\n\n"
                : "";
            const std::string clientSource =
                "package " + name + "." + client.name + ";\n\n" + imports +
                "\npublic class " + client.name + " {\n\n" + action +
                "    public Function Show() -> Int {\n" +
                (servers.empty() ? "        Print(0);\n        return 0;\n"
                                 : ServerCall(serverNames) + "        return 1;\n") +
                "    }\n}\n\npublic Function Main() -> Void {\n    " + client.name + " client = new " + client.name +
                "();\n    client." + (client.gui ? "SendMessage" : "Show") + "();\n}\n";
            const std::string clientTest = servers.empty()
                ? "    Assert.Equal(SharedReady(), 1);\n"
                : "    " + serverNames[0] + " server1 = new " + serverNames[0] + "();\n"
                  "    String reply = server1.SendText(\"How are you\");\n"
                  "    Assert.Equal(reply.Length(), 18);\n";
            const std::string clientImport = servers.empty() ? "Common.Source.Shared" : "Common.Source." + serverNames[0];
            const std::string clientType = client.gui ? "GUI" : "CLI";
            if (!WriteSourceAndTest(dir / "Source", dir / "Tests", name + "." + client.name, client.name, clientImport,
                                    clientSource, clientTest) ||
                !writeEnv(dir / ".env", "CLIENT_NAME=" + client.name + "\nCLIENT_TYPE=" + clientType + "\n") ||
                !writeEnv(dir / "exp.env", "CLIENT_NAME=" + client.name + "\nCLIENT_TYPE=" + clientType + "\n")) {
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
            const std::string action = server.gui
                ? "    public Function ShowReply() -> Void {\n"
                  "        " + server.name + " server1 = new " + server.name + "();\n"
                  "        Print(server1.SendText(\"How are you\"));\n"
                  "    }\n\n"
                : "";
            const std::string serverSource =
                "package " + name + "." + server.name + ";\n\nimport Common.Source." + server.name + ";\n\n"
                "public class " + server.name + "Program {\n\n" + action +
                "    public Function Show() -> Int {\n"
                "        " + server.name + " server1 = new " + server.name + "();\n"
                "        Print(server1.SendText(\"How are you\"));\n"
                "        return 1;\n"
                "    }\n}\n\n"
                "public Function Main() -> Void {\n"
                "    " + server.name + "Program program = new " + server.name + "Program();\n"
                "    program." + (server.gui ? "ShowReply" : "Show") + "();\n"
                "}\n";
            const std::string serverTest =
                "    " + server.name + " server1 = new " + server.name + "();\n"
                "    String reply = server1.SendText(\"How are you\");\n"
                "    Assert.Equal(reply.Length(), 18);\n";
            const std::string serverType = server.gui ? "GUI" : "CLI";
            if (!WriteSourceAndTest(dir / "Source", dir / "Tests", name + "." + server.name, server.name,
                                    "Common.Source." + server.name, serverSource, serverTest) ||
                !writeEnv(dir / ".env", "SERVER_NAME=" + server.name + "\nSERVER_PORT=" + std::to_string(port) +
                          "\nSERVER_TYPE=" + serverType + "\n") ||
                !writeEnv(dir / "exp.env", "SERVER_NAME=" + server.name + "\nSERVER_PORT=" + std::to_string(port) +
                          "\nSERVER_TYPE=" + serverType + "\n")) {
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
            ++port;
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
        project << "Name: " << name << "\nKind: Network\nSourceDir: Common/Source\nNetwork: true\n";
        std::cout << "robix new: wrote " << (root / (name + ".rbxproj")).string() << " kind Network\n";
        return true;
    }

    if (kind == "CLI") fs::create_directories(root / "Packages", ec);
    if (!WriteSourceAndTest(root / "Source", root / "Tests", name, "Program", "Source.Program", source, testBody)) return false;
    const std::string entry = "Source/Program.rbx";
    if (!WriteBuildFile(root, name, entry, { entry }, {})) return false;
    std::ofstream project(root / (name + ".rbxproj"), std::ios::binary | std::ios::trunc);
    if (!project) {
        std::cerr << "robix new: could not write the project file.\n";
        return false;
    }
    project << "Name: " << name << "\nKind: " << kind << "\nSourceDir: Source\n";
    std::cout << "robix new: wrote " << (root / (name + ".rbxproj")).string() << " kind " << kind << "\n";
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
    std::string name;
    std::string kind;
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
            const std::string maybeKind = CanonicalKind(token);
            if (maybeKind.empty()) {
                name = token;
                nameFromArgs = true;
            } else if (kind.empty()) {
                kind = maybeKind;
                kindFromArgs = true;
            } else {
                name = token;
                nameFromArgs = true;
            }
            continue;
        }
        if (!fileMode && kind.empty()) {
            kind = CanonicalKind(token);
            if (kind.empty()) {
                std::cerr << "robix new: kind must be Blank, CLI, or Network.\n";
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
    if (!kindFromArgs) kind = AskKind();
    if (kind.empty()) return 1;

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
