#pragma once

#include "../Lexer/Token.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace Robix::Frontend {

enum class DiagnosticSeverity : std::uint8_t {
    Note,
    Warning,
    Error
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string code;           // e.g. "R1001", "R1004", "D3001", "S1001", "L0001"
    std::string message;
    SourceLocation location{};
    std::string expectedType;
    std::string receivedType;
    std::string suggestion;
};

class DiagnosticBag {
public:
    void Report(DiagnosticSeverity severity,
                std::string code,
                std::string message,
                SourceLocation location,
                std::string suggestion = {},
                std::string expectedType = {},
                std::string receivedType = {});

    void ReportError(std::string code,
                     std::string message,
                     SourceLocation location,
                     std::string suggestion = {},
                     std::string expectedType = {},
                     std::string receivedType = {});

    void ReportWarning(std::string code,
                       std::string message,
                       SourceLocation location,
                       std::string suggestion = {});

    [[nodiscard]] bool HasErrors() const noexcept { return m_errorCount > 0; }
    [[nodiscard]] bool HasWarnings() const noexcept { return m_warningCount > 0; }
    [[nodiscard]] std::size_t ErrorCount() const noexcept { return m_errorCount; }
    [[nodiscard]] std::size_t WarningCount() const noexcept { return m_warningCount; }
    [[nodiscard]] const std::vector<Diagnostic>& All() const noexcept { return m_diagnostics; }
    [[nodiscard]] bool ContainsCode(std::string_view code) const noexcept;

    [[nodiscard]] std::string FormatHumanReadable() const;
    [[nodiscard]] std::string FormatJSON() const;

private:
    std::vector<Diagnostic> m_diagnostics;
    std::size_t m_errorCount   = 0;
    std::size_t m_warningCount = 0;
};

} // namespace Robix::Frontend

