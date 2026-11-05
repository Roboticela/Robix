#include "Diagnostics.hpp"

#include <sstream>
#include <utility>

namespace Robix::Frontend {

void DiagnosticBag::Report(DiagnosticSeverity severity,
                           std::string code,
                           std::string message,
                           SourceLocation location,
                           std::string suggestion,
                           std::string expectedType,
                           std::string receivedType) {
    if (severity == DiagnosticSeverity::Error) {
        ++m_errorCount;
    } else if (severity == DiagnosticSeverity::Warning) {
        ++m_warningCount;
    }
    m_diagnostics.push_back(Diagnostic{
        severity,
        std::move(code),
        std::move(message),
        location,
        std::move(expectedType),
        std::move(receivedType),
        std::move(suggestion),
    });
}

void DiagnosticBag::ReportError(std::string code,
                                std::string message,
                                SourceLocation location,
                                std::string suggestion,
                                std::string expectedType,
                                std::string receivedType) {
    Report(DiagnosticSeverity::Error,
           std::move(code),
           std::move(message),
           location,
           std::move(suggestion),
           std::move(expectedType),
           std::move(receivedType));
}

void DiagnosticBag::ReportWarning(std::string code,
                                  std::string message,
                                  SourceLocation location,
                                  std::string suggestion) {
    Report(DiagnosticSeverity::Warning,
           std::move(code),
           std::move(message),
           location,
           std::move(suggestion));
}

bool DiagnosticBag::ContainsCode(std::string_view code) const noexcept {
    for (const auto& d : m_diagnostics) {
        if (d.code == code) {
            return true;
        }
    }
    return false;
}

std::string DiagnosticBag::FormatHumanReadable() const {
    std::ostringstream out;
    for (const auto& d : m_diagnostics) {
        const char* sevStr = (d.severity == DiagnosticSeverity::Error)     ? "Error"
                             : (d.severity == DiagnosticSeverity::Warning) ? "Warning"
                                                                           : "Note";
        out << "Robix " << sevStr << " [" << d.code << "]: " << d.message << "\n\n";
        out << "File: " << d.location.fileName << "\n";
        out << "Line: " << d.location.line << "\n";
        out << "Column: " << d.location.column << "\n";
        if (!d.expectedType.empty() || !d.receivedType.empty()) {
            out << "\nExpected: " << d.expectedType << "\n";
            out << "Received: " << d.receivedType << "\n";
        }
        if (!d.suggestion.empty()) {
            out << "\nSuggestion:\n" << d.suggestion << "\n";
        }
        out << "\n";
    }
    return out.str();
}

std::string DiagnosticBag::FormatJSON() const {
    std::ostringstream out;
    out << "{\"diagnostics\":[";
    for (std::size_t i = 0; i < m_diagnostics.size(); ++i) {
        const auto& d = m_diagnostics[i];
        if (i > 0) {
            out << ',';
        }
        const char* sevStr = (d.severity == DiagnosticSeverity::Error)     ? "error"
                             : (d.severity == DiagnosticSeverity::Warning) ? "warning"
                                                                           : "note";
        out << "{\"severity\":\"" << sevStr << "\","
            << "\"code\":\"" << d.code << "\","
            << "\"message\":\"" << d.message << "\","
            << "\"file\":\"" << d.location.fileName << "\","
            << "\"line\":" << d.location.line << ","
            << "\"column\":" << d.location.column;
        if (!d.expectedType.empty()) {
            out << ",\"expected\":\"" << d.expectedType << "\"";
        }
        if (!d.receivedType.empty()) {
            out << ",\"received\":\"" << d.receivedType << "\"";
        }
        if (!d.suggestion.empty()) {
            out << ",\"suggestion\":\"" << d.suggestion << "\"";
        }
        out << "}";
    }
    out << "]}";
    return out.str();
}

} // namespace Robix::Frontend
