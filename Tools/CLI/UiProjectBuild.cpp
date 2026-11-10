#include "UiProjectBuild.hpp"

#include "NativeCompilerDriver.hpp"
#include "ProjectFiles.hpp"
#include "../../RobixUI/Document.hpp"
#include "../../RobixUI/Web/WebFoundation.hpp"

#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace Robix::UI {
namespace {

bool HasMain(const std::string& source) {
    return source.find("Main(") != std::string::npos;
}

void CopyTree(const fs::path& from, const fs::path& to) {
    std::error_code error;
    if (!fs::exists(from, error)) {
        return;
    }
    fs::create_directories(to, error);
    for (const auto& entry : fs::recursive_directory_iterator(from, error)) {
        const fs::path relative = fs::relative(entry.path(), from, error);
        const fs::path dest = to / relative;
        if (entry.is_directory()) {
            fs::create_directories(dest, error);
        } else if (entry.is_regular_file()) {
            fs::create_directories(dest.parent_path(), error);
            fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing, error);
        }
    }
}

std::string ReadText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void WriteText(const fs::path& path, const std::string& body) {
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    std::ofstream out(path, std::ios::binary);
    out << body;
}

fs::path ProjectRoot(const fs::path& input) {
    std::error_code error;
    if (fs::is_directory(input, error)) {
        return input;
    }
    if (input.extension() == ".rbxproj") {
        return input.parent_path();
    }
    return input.parent_path();
}

int RunDocument(const fs::path& document, bool web, bool stayOpen) {
    std::vector<std::string> args = {"ui", web ? "web" : "window", document.string()};
    if (stayOpen) {
        args.push_back("--dev");
    } else if (web) {
        args.push_back("--once");
    }
    return RunUiCommand(args);
}

std::vector<fs::path> FindDocuments(const fs::path& root) {
    std::vector<fs::path> documents;
    std::error_code error;
    for (const auto& entry : fs::recursive_directory_iterator(root, error)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".rbxu") {
            continue;
        }
        if (entry.path().string().find("\\Build\\") != std::string::npos) {
            continue;
        }
        documents.push_back(entry.path());
    }
    std::sort(documents.begin(), documents.end());
    return documents;
}

fs::path ChooseDocument(const std::vector<fs::path>& documents) {
    for (const fs::path& document : documents) {
        if (document.filename() == "MainWindow.rbxu") {
            return document;
        }
    }
    return documents.empty() ? fs::path{} : documents.front();
}

}  // namespace

int BuildOrRunProject(const std::string& input, bool development, const std::string& target, const std::string& toolDirectory) {
    const fs::path root = ProjectRoot(fs::absolute(input));
    const bool web = target == "web";
    const std::vector<fs::path> documents = FindDocuments(root);
    const fs::path document = fs::is_regular_file(input) && fs::path(input).extension() == ".rbxu" ? fs::path(input) : ChooseDocument(documents);
    if (development) {
        if (document.empty()) {
            std::cerr << "robixc: development mode found no .rbxu screen\n";
            return 1;
        }
        std::cout << "development: running " << (web ? "web" : "windows") << " without writing HTML\n";
        return RunDocument(document, web, true);
    }

    const fs::path build = root / "Build";
    std::error_code error;
    fs::create_directories(build / "compile", error);
    fs::create_directories(build / "content", error);
    fs::create_directories(build / "components", error);
    fs::create_directories(build / "generated", error);
    fs::create_directories(build / "assets" / "Fonts", error);
    CopyTree(Robix::UI::AssetsFontsRoot(), build / "assets" / "Fonts");
    if (fs::is_directory(root / "Assets")) {
        CopyTree(root / "Assets", build / "assets");
    }
    const fs::path schemaRoot = fs::path("RobixUI") / "Components";
    fs::path repo = fs::current_path();
    for (int depth = 0; depth < 6 && !fs::exists(repo / schemaRoot); ++depth) {
        repo = repo.parent_path();
    }
    if (fs::is_directory(repo / schemaRoot)) {
        for (const fs::directory_entry& entry : fs::directory_iterator(repo / schemaRoot)) {
            if (!entry.is_directory()) {
                continue;
            }
            const fs::path schema = entry.path() / (entry.path().filename().string() + ".Schema.rbxu");
            if (!fs::is_regular_file(schema)) {
                continue;
            }
            const fs::path destination = build / "components" / entry.path().filename();
            fs::create_directories(destination, error);
            fs::copy_file(schema, destination / schema.filename(), fs::copy_options::overwrite_existing, error);
        }
    }

    for (const fs::path& uiDocument : documents.empty() && !document.empty() ? std::vector<fs::path>{document} : documents) {
        fs::copy_file(uiDocument, build / "content" / uiDocument.filename(), fs::copy_options::overwrite_existing, error);
    }
    WriteText(build / "generated" / "Screen.gen.cpp",
              "/* Foundation only. Component emitters are added with each component. */\n");

    std::vector<fs::path> sources;
    for (const auto& entry : fs::recursive_directory_iterator(root, error)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".rbx") {
            continue;
        }
        if (entry.path().string().find("\\Build\\") != std::string::npos) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (name.size() > 9 && name.ends_with(".test.rbx")) {
            continue;
        }
        sources.push_back(entry.path());
    }
    fs::path mainSource;
    for (const fs::path& source : sources) {
        if (source.filename() == "Main.rbx" || HasMain(ReadText(source))) {
            mainSource = source;
            break;
        }
    }
    if (mainSource.empty() && !sources.empty()) {
        mainSource = sources.front();
    }

    int status = 0;
    for (const fs::path& source : sources) {
        const std::string text = ReadText(source);
        Robix::Tools::NativeBuildOptions options;
        options.toolDirectory = toolDirectory;
        options.release = true;
        const bool executable = source == mainSource;
        options.omitMain = !executable;
        if (executable) {
            options.outputKind = Robix::Tools::NativeOutputKind::HostedExecutable;
            options.outputPath = (build / (root.filename().string() + ".exe")).string();
        } else {
            options.outputKind = Robix::Tools::NativeOutputKind::HostedObject;
            options.outputPath = (build / "compile" / (source.stem().string() + ".obj")).string();
        }
        if (Robix::Tools::IncrementalHit(source, options.outputPath)) {
            std::cout << "incremental: unchanged " << options.outputPath << "\n";
            continue;
        }
        auto built = Robix::Tools::NativeCompilerDriver::CompileSource(source.string(), text, options);
        if (!built.diagnosticsText.empty()) {
            std::cerr << built.diagnosticsText;
        }
        if (!built.toolOutput.empty()) {
            std::cout << built.toolOutput;
        }
        if (!built.success) {
            std::cerr << "robixc: compilation failed for " << source.string() << "\n";
            status = 1;
            continue;
        }
        Robix::Tools::WriteIncrementalStamp(source, built.executablePath);
        std::cout << "compiled " << source.filename().string() << " -> " << built.executablePath << "\n";
    }

    if (web) {
        const fs::path webRoot = build / "web";
        fs::create_directories(webRoot / "content", error);
        fs::create_directories(webRoot / "components", error);
        fs::create_directories(webRoot / "assets" / "Fonts", error);
        fs::create_directories(webRoot / "screens", error);
        fs::create_directories(webRoot / "generated", error);
        CopyTree(build / "assets" / "Fonts", webRoot / "assets" / "Fonts");
        CopyTree(build / "components", webRoot / "components");
        CopyTree(build / "content", webRoot / "content");
        fs::copy_file(build / "generated" / "Screen.gen.cpp", webRoot / "generated" / "Screen.gen.cpp",
                      fs::copy_options::overwrite_existing, error);
        if (!document.empty()) {
            fs::copy_file(document, webRoot / "screens" / document.filename(), fs::copy_options::overwrite_existing, error);
        }
        std::string webError;
        if (!WriteWebFoundation(webRoot, document, webError, {}, true)) {
            std::cerr << "robixc: " << webError << "\n";
            status = 1;
        }
        std::ostringstream manifest;
        manifest << "{\n  \"name\": \"" << root.filename().string() << "\",\n  \"target\": \"web\",\n  \"entry\": \"index.html\"\n}\n";
        WriteText(webRoot / "AppManifest.json", manifest.str());
        std::cout << "web app " << (webRoot / "index.html").string() << "\n";
    } else {
        std::ostringstream manifest;
        manifest << "{\n  \"name\": \"" << root.filename().string() << "\",\n  \"target\": \"windows\",\n  \"fonts\": \"assets/Fonts\"\n}\n";
        WriteText(build / "AppManifest.json", manifest.str());
        std::cout << "app " << (build / "AppManifest.json").string() << "\n";
    }
    return status;
}

}  // namespace Robix::UI
