#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Robix::Tools {

// ============================================================================
// 1. MANDATORY 22 WORKING EXAMPLE APPLICATIONS CATALOG (ALL WITH .TEST.RX)
// ============================================================================

struct ExampleApplicationEntry {
    std::uint32_t exampleId = 0;
    std::string title;
    std::string primarySourceFile;
    std::string primarySourceContent;
    std::string companionTestFile;
    std::string companionTestContent;
};

class OfficialExamplesCatalog {
public:
    [[nodiscard]] static const std::vector<ExampleApplicationEntry>& GetAll22Examples();

    // Compiles, verifies, and runs a given example (or all 22 examples) through the Robix VM
    [[nodiscard]] static bool VerifyAndExecuteAll22Examples(std::size_t& passedCountOut);
};

// ============================================================================
// 2. COMPLETE 30-GUIDE OFFICIAL DOCUMENTATION SUITE (`Documentation/`)
// ============================================================================

struct DocumentationGuideEntry {
    std::uint32_t guideNumber = 0;
    std::string fileName;
    std::string title;
    std::string summaryMarkdown;
};

class OfficialDocumentationCatalog {
public:
    [[nodiscard]] static const std::vector<DocumentationGuideEntry>& GetAll30Guides();
    [[nodiscard]] static std::string RenderCompleteIndexMarkdown();
};

} // namespace Robix::Tools

