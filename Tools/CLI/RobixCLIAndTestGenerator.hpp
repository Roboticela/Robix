#pragma once

#include "../../Compiler/Semantic/Diagnostics.hpp"
#include "../../Packages/Registry/PackageManagerAndRegistry.hpp"
#include "../Verifier/RobixVerifier.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Robix::Tools {

// ============================================================================
// 1. AUTOMATIC .TEST.RX SCAFFOLD GENERATOR (IDEMPOTENT IF NOT EXISTS)
// ============================================================================
// Implements:
//   - `robix test --generate [Path]`
//   - `robix test-gen [Path]`
//   - `robix generate-tests [Path]`
//   - `robixc --generate-tests [Path]`
// Inspects target `.rx` source file(s), extracts package name and public/top-level
// functions/classes, and creates `<ModuleName>.test.rx` ONLY if it does not
// already exist (never overwrites existing user tests).

struct TestScaffoldReport {
    std::vector<std::string> createdTestFiles;
    std::vector<std::string> skippedExistingTestFiles;
};

class TestScaffoldGenerator {
public:
    // Computes the companion `.test.rx` path for a given `.rx` source path
    // (e.g. "Source/Robot.rx" -> "Source/Robot.test.rx")
    [[nodiscard]] static std::string ComputeCompanionTestPath(std::string_view sourceFilePath);

    // Generates valid `.test.rx` content with empty/starter `test "..." { }` blocks
    // for every public function and class in `sourceCode`.
    [[nodiscard]] static std::string SynthesizeTestFileContent(
        std::string_view sourceFilePath,
        std::string_view sourceCode);

    // Ensures `<FileStem>.test.rx` exists on disk for a single `.rx` file.
    // Returns true if a new `.test.rx` file was created, false if it already existed.
    static bool EnsureTestFileForSourceOnDisk(
        const std::filesystem::path& rxFilePath,
        TestScaffoldReport& reportOut);

    // Scans a directory or single file path on disk and creates missing `.test.rx` files.
    static TestScaffoldReport GenerateMissingTestFilesOnDisk(
        const std::filesystem::path& targetPath);

    // Scans an in-memory project file map (`path -> content`) and inserts missing `.test.rx` files
    // only when neither `<Dir>/<Stem>.test.rx` nor `Tests/<Stem>.test.rx` exists.
    static TestScaffoldReport GenerateMissingTestFilesInMemory(
        std::map<std::string, std::string>& projectFiles);
};

// ============================================================================
// 2. UNIFIED CLI DISPATCHER (`robix` & `robixc`)
// ============================================================================

enum class CLIBinaryKind : std::uint8_t {
    RobixUnifiedCLI,    // `robix`
    RobixNativeCompiler // `robixc`
};

enum class ExecutionEngineMode : std::uint8_t {
    Interpreter,
    Native,
    Auto
};

struct CLICommandResult {
    int exitCode = 0;
    std::string stdoutText;
    std::string stderrText;
    TestScaffoldReport generatedTests;
    std::int64_t executionNanoseconds = -1;
};

class RobixCLIDispatcher {
public:
    static CLICommandResult ExecuteCommand(
        CLIBinaryKind binaryKind,
        const std::vector<std::string>& args,
        std::map<std::string, std::string>* optionalInMemoryFiles = nullptr);

    // Interactive REPL single-line evaluator (`robix repl`)
    static CLICommandResult EvaluateReplLine(std::string_view replInputLine);
};

} // namespace Robix::Tools
