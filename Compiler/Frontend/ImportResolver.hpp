#pragma once

#include "../Lexer/Lexer.hpp"
#include "../Parser/Parser.hpp"
#include "../Semantic/Diagnostics.hpp"

#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace Robix::Frontend {

// Loads `import Name;` and `import Package.Name;` as sibling `.rbx` files.
// The last segment is tried next to the importing file first, then the dotted
// name as a relative path. Declarations are appended to the root unit.
inline void ResolveImports(CompilationUnitAST& root, std::string_view rootFile, DiagnosticBag& diagnostics) {
    namespace fs = std::filesystem;
    // Token text is a view into these buffers. They outlive this function so later
    // diagnostics still name the imported file.
    static std::deque<std::string> sourceText;
    static std::deque<std::string> sourceNames;
    std::unordered_set<std::string> loaded;
    std::error_code ec;
    const fs::path rootPath = fs::absolute(fs::path(std::string(rootFile)), ec).lexically_normal();
    loaded.insert(rootPath.string());

    struct Pending {
        fs::path baseDir;
        std::string importName;
    };
    std::vector<Pending> queue;
    for (const auto& name : root.imports) {
        queue.push_back(Pending{ rootPath.parent_path(), name });
    }

    auto existsFile = [](const fs::path& path) {
        std::error_code existsEc;
        return fs::is_regular_file(path, existsEc);
    };

    while (!queue.empty()) {
        const Pending item = queue.back();
        queue.pop_back();

        const auto dot = item.importName.rfind('.');
        const std::string last = dot == std::string::npos ? item.importName : item.importName.substr(dot + 1);
        std::string slashed = item.importName;
        for (char& ch : slashed) {
            if (ch == '.') ch = '/';
        }

        fs::path found;
        const fs::path beside = item.baseDir / (last + ".rbx");
        const fs::path nested = item.baseDir / (slashed + ".rbx");
        const fs::path fromCwd = fs::path(slashed + ".rbx");
        if (existsFile(beside)) found = beside;
        else if (existsFile(nested)) found = nested;
        else if (existsFile(fromCwd)) found = fromCwd;
        if (found.empty()) {
            auto walk = [&](fs::path cursor, const fs::path& relative) {
                for (int depth = 0; depth < 8 && !cursor.empty(); ++depth) {
                    const fs::path candidate = cursor / relative;
                    if (existsFile(candidate)) return candidate;
                    if (!cursor.has_parent_path() || cursor.parent_path() == cursor) break;
                    cursor = cursor.parent_path();
                }
                return fs::path{};
            };
            const fs::path stdlib = fs::path("StdLib") / "Robix" / (last + ".rbx");
            const fs::path package = fs::path("Packages") / item.importName / (last + ".rbx");
            found = walk(item.baseDir, stdlib);
            if (found.empty()) found = walk(fs::current_path(), stdlib);
            if (found.empty()) found = walk(item.baseDir, package);
            if (found.empty()) found = walk(fs::current_path(), package);
        }
        if (found.empty()) {
            diagnostics.ReportError(
                "I4013",
                "Import '" + item.importName + "' did not match a .rbx file.",
                SourceLocation{},
                "Place " + last + ".rbx next to the importing file, or use a path such as Examples.StageF." + last + ".");
            continue;
        }

        const fs::path full = fs::absolute(found, ec).lexically_normal();
        if (!loaded.insert(full.string()).second) continue;

        std::ifstream in(full, std::ios::binary);
        if (!in) {
            diagnostics.ReportError("I4013", "Could not read imported file '" + full.string() + "'.", SourceLocation{});
            continue;
        }
        std::ostringstream text;
        text << in.rdbuf();
        sourceText.push_back(text.str());
        sourceNames.push_back(full.string());
        Lexer lexer(sourceText.back(), sourceNames.back(), diagnostics);
        Parser parser(lexer.TokenizeAll(), full.string(), diagnostics);
        CompilationUnitAST imported = parser.ParseCompilationUnit();
        for (auto& decl : imported.declarations) {
            root.declarations.push_back(std::move(decl));
        }
        for (const auto& nestedImport : imported.imports) {
            queue.push_back(Pending{ full.parent_path(), nestedImport });
        }
    }
}

} // namespace Robix::Frontend
