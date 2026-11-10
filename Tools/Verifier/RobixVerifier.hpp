#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../../Compiler/Semantic/Diagnostics.hpp"
#include "../../StdLib/Testing/TestingFramework.hpp"

namespace Robix::Tools {

struct VerificationReport {
    std::string targetName;
    bool        isSingleFileMode             = false;
    bool        syntaxCheckPassed            = false;
    bool        typeCheckPassed              = false;
    bool        explainCheckPassed           = false;
    bool        namingStyleCheckPassed       = false;
    bool        mandatoryTestFileCheckPassed = false;
    bool        testExecutionPassed          = false;
    bool        staticAnalysisPassed         = false;
    bool        memorySafetyAuditPassed      = false;

    std::size_t filesVerified                = 0;
    std::size_t totalTestBlocksExecuted      = 0;
    std::size_t totalAssertionsPassed        = 0;
    std::size_t totalAssertionsFailed        = 0;
    double      averageSymbolCoveragePercent = 0.0;

    int         exitCode                     = 1; // 0 = all checks passed, non-zero = failed
    std::vector<Frontend::Diagnostic> diagnostics;

    [[nodiscard]] bool AllChecksPassed() const noexcept {
        return exitCode == 0;
    }

    [[nodiscard]] std::string FormatSummary() const;
};

class RobixVerifier {
public:
    // Full project verification (`robix verify`)
    static VerificationReport VerifyProject(
        std::string_view projectName,
        const std::vector<Testing::SourceTestPair>& projectFiles,
        const Testing::TestEnforcementConfig& testConfig = {});

    // Single-file verification (`robix verify File.rx` or `robix verify Window.rxu`)
    static VerificationReport VerifySingleFile(
        std::string_view fileName,
        std::string_view fileContent,
        std::string_view optionalTestFileContent = "",
        bool enforceTestFileInSingleMode = false);

private:
    static VerificationReport VerifySingleRobixUIMarkup(
        std::string_view fileName,
        std::string_view xmlContent);
};

} // namespace Robix::Tools

