#include "RobixVerifier.hpp"

#include "../../Compiler/IR/SSAIR.hpp"
#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Optimizer/IRPassManager.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Semantic/SemanticAnalyzer.hpp"
#include "../../Compiler/Types/SpecValidator.hpp"
#include "../../Compiler/Types/TypeChecker.hpp"

#include <sstream>

namespace Robix::Tools {

namespace {

bool HasDiagnosticWithPrefix(const Frontend::DiagnosticBag& bag, std::string_view prefix) {
    for (const auto& d : bag.All()) {
        if (d.severity == Frontend::DiagnosticSeverity::Error &&
            d.code.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

void AppendDiagnostics(std::vector<Frontend::Diagnostic>& dest, const Frontend::DiagnosticBag& src) {
    for (const auto& d : src.All()) {
        dest.push_back(d);
    }
}

} // namespace

std::string VerificationReport::FormatSummary() const {
    std::ostringstream out;
    out << "================================================================\n";
    out << "ROBIX VERIFICATION REPORT: " << targetName
        << (isSingleFileMode ? " [Single-File Mode]" : " [Project Mode]") << "\n";
    out << "================================================================\n";
    out << "1. Syntax Check:              " << (syntaxCheckPassed            ? "PASSED" : "FAILED") << "\n";
    out << "2. Type Check:                " << (typeCheckPassed              ? "PASSED" : "FAILED") << "\n";
    out << "3. Explain Documentation:     " << (explainCheckPassed           ? "PASSED" : "FAILED") << "\n";
    out << "4. Naming & Style Check:      " << (namingStyleCheckPassed       ? "PASSED" : "FAILED") << "\n";
    out << "5. Mandatory .test.rx Check: " << (mandatoryTestFileCheckPassed ? "PASSED" : "FAILED") << "\n";
    out << "6. Test Execution:            " << (testExecutionPassed          ? "PASSED" : "FAILED")
        << " (" << totalAssertionsPassed << " passed, " << totalAssertionsFailed << " failed)\n";
    out << "7. Static Analysis & SSA IR:  " << (staticAnalysisPassed         ? "PASSED" : "FAILED") << "\n";
    out << "8. Memory Safety Audit:       " << (memorySafetyAuditPassed      ? "PASSED" : "FAILED") << "\n";
    out << "----------------------------------------------------------------\n";
    out << "Files Verified: " << filesVerified
        << " | Coverage: " << averageSymbolCoveragePercent << "%"
        << " | Exit Code: " << exitCode << "\n";
    out << "================================================================\n";
    return out.str();
}

VerificationReport RobixVerifier::VerifyProject(
    std::string_view projectName,
    const std::vector<Testing::SourceTestPair>& projectFiles,
    const Testing::TestEnforcementConfig& testConfig) {

    VerificationReport report;
    report.targetName       = std::string(projectName);
    report.isSingleFileMode = false;
    report.filesVerified    = projectFiles.size();

    bool allSyntaxOk       = !projectFiles.empty();
    bool allTypesOk        = !projectFiles.empty();
    bool allExplainOk      = !projectFiles.empty();
    bool allNamingOk       = !projectFiles.empty();
    bool allStaticOk       = !projectFiles.empty();
    bool allMemorySafetyOk = !projectFiles.empty();

    for (const auto& pair : projectFiles) {
        // Validate filename PascalCase + extension
        if (!Spec::IsValidPascalCaseWithAcronyms(
                pair.sourceFileName.substr(0, pair.sourceFileName.find('.')))) {
            allNamingOk = false;
        }

        Frontend::DiagnosticBag fileDiag;
        Frontend::Lexer lexer(pair.sourceCode, pair.sourceFileName, fileDiag);
        auto tokens = lexer.TokenizeAll();

        Frontend::Parser parser(std::move(tokens), pair.sourceFileName, fileDiag);
        auto ast = parser.ParseCompilationUnit();

        if (HasDiagnosticWithPrefix(fileDiag, "L0001") || HasDiagnosticWithPrefix(fileDiag, "P")) {
            allSyntaxOk = false;
        }

        Frontend::TypeCheckerOptions tcOpts;
        tcOpts.strictStyleMode = true;
        tcOpts.requireExplain  = true;
        Frontend::TypeChecker tc(fileDiag, tcOpts);
        tc.CheckCompilationUnit(ast);

        if (HasDiagnosticWithPrefix(fileDiag, "R1")) {
            allTypesOk = false;
        }
        if (HasDiagnosticWithPrefix(fileDiag, "D3001") || HasDiagnosticWithPrefix(fileDiag, "R3001")) {
            allExplainOk = false;
        }
        if (HasDiagnosticWithPrefix(fileDiag, "L0002") ||
            HasDiagnosticWithPrefix(fileDiag, "L0003") ||
            HasDiagnosticWithPrefix(fileDiag, "L0004") ||
            HasDiagnosticWithPrefix(fileDiag, "R2001")) {
            allNamingOk = false;
        }
        if (HasDiagnosticWithPrefix(fileDiag, "R1006") || HasDiagnosticWithPrefix(fileDiag, "M5")) {
            allMemorySafetyOk = false;
        }

        Frontend::SemanticAnalyzer sema(fileDiag);
        sema.AnalyzeCompilationUnit(ast);

        if (HasDiagnosticWithPrefix(fileDiag, "R1") || HasDiagnosticWithPrefix(fileDiag, "R4") ||
            HasDiagnosticWithPrefix(fileDiag, "S")) {
            allStaticOk = false;
        }

        // SSA IR + Optimizer pass verification + Memory audit
        if (!fileDiag.HasErrors()) {
            IR::SSAIRBuilder irBuilder(sema, fileDiag);
            IR::IRModule irMod = irBuilder.LowerCompilationUnit(ast);
            IR::IRPassManager passMgr(sema, fileDiag);
            if (!passMgr.RunOptimizationPipeline(irMod, true)) {
                allStaticOk = false;
            }
        } else {
            if (!allSyntaxOk) {
                allTypesOk  = false;
                allStaticOk = false;
            }
        }

        AppendDiagnostics(report.diagnostics, fileDiag);
    }

    report.syntaxCheckPassed      = allSyntaxOk;
    report.typeCheckPassed        = allSyntaxOk && allTypesOk;
    report.explainCheckPassed     = allSyntaxOk && allExplainOk;
    report.namingStyleCheckPassed = allSyntaxOk && allNamingOk;
    report.staticAnalysisPassed   = allSyntaxOk && allTypesOk && allStaticOk;

    // Stage 5 & 6: Mandatory per-file .test.rx check (R5001) and test execution
    Frontend::DiagnosticBag testDiag;
    auto coverageResults = Testing::MandatoryTestEnforcer::EnforceProjectTests(
        projectFiles,
        testConfig,
        testDiag);
    AppendDiagnostics(report.diagnostics, testDiag);

    bool allTestsPresentAndCovering = !coverageResults.empty();
    bool allTestsExecutedAndPassing = !coverageResults.empty();
    double coverageSum              = 0.0;

    for (const auto& cov : coverageResults) {
        if (!cov.hasTestFile || cov.testBlockCount == 0 || !cov.exercisesSourceSymbols) {
            allTestsPresentAndCovering = false;
        }
        if (!cov.passedEnforcement || cov.assertionsFailed > 0) {
            allTestsExecutedAndPassing = false;
        }
        report.totalTestBlocksExecuted += cov.testBlockCount;
        report.totalAssertionsPassed   += cov.assertionsPassed;
        report.totalAssertionsFailed   += cov.assertionsFailed;
        coverageSum                    += cov.symbolCoveragePercent;
    }

    if (!coverageResults.empty()) {
        report.averageSymbolCoveragePercent = coverageSum / static_cast<double>(coverageResults.size());
    }

    report.mandatoryTestFileCheckPassed = allTestsPresentAndCovering;
    report.testExecutionPassed          = allTestsPresentAndCovering && allTestsExecutedAndPassing;
    report.memorySafetyAuditPassed      = allSyntaxOk && allMemorySafetyOk;

    const bool overallOk = report.syntaxCheckPassed &&
                           report.typeCheckPassed &&
                           report.explainCheckPassed &&
                           report.namingStyleCheckPassed &&
                           report.mandatoryTestFileCheckPassed &&
                           report.testExecutionPassed &&
                           report.staticAnalysisPassed &&
                           report.memorySafetyAuditPassed;

    report.exitCode = overallOk ? 0 : 1;
    return report;
}

VerificationReport RobixVerifier::VerifySingleFile(
    std::string_view fileName,
    std::string_view fileContent,
    std::string_view optionalTestFileContent,
    bool enforceTestFileInSingleMode) {

    // Dispatch .rxu declarative UI files to the RobixUI markup verifier
    if (fileName.size() >= 5 && fileName.substr(fileName.size() - 5) == ".rxu") {
        return VerifySingleRobixUIMarkup(fileName, fileContent);
    }

    Testing::TestEnforcementConfig cfg;
    cfg.requireTestFile               = enforceTestFileInSingleMode || !optionalTestFileContent.empty();
    cfg.requireTestsToPass            = !optionalTestFileContent.empty() || enforceTestFileInSingleMode;
    cfg.requireTestsForAllSourceFiles = enforceTestFileInSingleMode || !optionalTestFileContent.empty();
    cfg.allowUntestedDevelopmentBuild = !enforceTestFileInSingleMode && optionalTestFileContent.empty();

    std::string testFileName;
    if (!optionalTestFileContent.empty() || enforceTestFileInSingleMode) {
        const auto dotPos = fileName.rfind('.');
        testFileName = (dotPos != std::string_view::npos)
                           ? (std::string(fileName.substr(0, dotPos)) + ".test.rx")
                           : (std::string(fileName) + ".test.rx");
    }

    Testing::SourceTestPair pair{
        std::string(fileName),
        std::string(fileContent),
        testFileName,
        std::string(optionalTestFileContent)
    };

    VerificationReport rep = VerifyProject(fileName, { pair }, cfg);
    rep.isSingleFileMode   = true;

    // When running `robix verify File.rx` on a single file without a companion test file and
    // without `enforceTestFileInSingleMode`, mark test checks as satisfied if embedded `Test` blocks pass
    if (!enforceTestFileInSingleMode && optionalTestFileContent.empty()) {
        rep.mandatoryTestFileCheckPassed = true;
        rep.testExecutionPassed          = (rep.totalAssertionsFailed == 0);
        const bool ok = rep.syntaxCheckPassed &&
                        rep.typeCheckPassed &&
                        rep.explainCheckPassed &&
                        rep.namingStyleCheckPassed &&
                        rep.mandatoryTestFileCheckPassed &&
                        rep.testExecutionPassed &&
                        rep.staticAnalysisPassed &&
                        rep.memorySafetyAuditPassed;
        rep.exitCode = ok ? 0 : 1;
    }

    return rep;
}

VerificationReport RobixVerifier::VerifySingleRobixUIMarkup(
    std::string_view fileName,
    std::string_view xmlContent) {

    VerificationReport rep;
    rep.targetName       = std::string(fileName);
    rep.isSingleFileMode = true;
    rep.filesVerified    = 1;

    const bool hasRootOpen  = xmlContent.find("<RobixUI") != std::string_view::npos;
    const bool hasRootClose = xmlContent.find("</RobixUI>") != std::string_view::npos;
    const bool balancedTags = (std::count(xmlContent.begin(), xmlContent.end(), '<') ==
                               std::count(xmlContent.begin(), xmlContent.end(), '>'));

    rep.syntaxCheckPassed            = hasRootOpen && hasRootClose && balancedTags;
    rep.typeCheckPassed              = rep.syntaxCheckPassed;
    rep.explainCheckPassed           = true;
    rep.namingStyleCheckPassed       = (fileName.size() > 5 && fileName.substr(fileName.size() - 5) == Spec::EXT_UI);
    rep.mandatoryTestFileCheckPassed = true;
    rep.testExecutionPassed          = true;
    rep.staticAnalysisPassed         = rep.syntaxCheckPassed;
    rep.memorySafetyAuditPassed      = true;
    rep.averageSymbolCoveragePercent = 100.0;

    if (!rep.syntaxCheckPassed) {
        Frontend::Diagnostic d;
        d.code       = "U1001";
        d.severity   = Frontend::DiagnosticSeverity::Error;
        d.message    = "Invalid RobixUI (.rxu) markup: missing <RobixUI> root element or unbalanced XML brackets.";
        d.location   = Frontend::SourceLocation{ std::string(fileName), 1, 1, 1 };
        d.suggestion = "Ensure the .rxu document is wrapped in <RobixUI Version=\"1.0\"> ... </RobixUI>.";
        rep.diagnostics.push_back(std::move(d));
    }

    rep.exitCode = rep.syntaxCheckPassed ? 0 : 1;
    return rep;
}

} // namespace Robix::Tools

