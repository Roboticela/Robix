#pragma once

#include "../../Compiler/Semantic/Diagnostics.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Robix::Tools {

// ============================================================================
// 1. ROBIX FORMATTER (`robix fmt` / `robix format`) & STRICT LINTER (`robix lint --strict`)
// ============================================================================

class RobixFormatterAndLinter {
public:
    // Canonicalizes indentation (4 spaces), brace spacing, and trailing whitespace
    [[nodiscard]] static std::string FormatSource(std::string_view sourceCode);

    // Enforces PascalCase, camelCase, UPPER_SNAKE_CASE, uppercase acronyms, and Explain blocks
    static bool LintSource(std::string_view fileName,
                           std::string_view sourceCode,
                           bool strictMode,
                           Frontend::DiagnosticBag& diagnostics);
};

// ============================================================================
// 2. LANGUAGE SERVER PROTOCOL (LSP) & DOCUMENTATION GENERATOR (`robix doc`)
// ============================================================================

struct LSPCompletionItem {
    std::string label;
    std::string kind; // "Function", "Class", "Struct", "Keyword"
    std::string detail;
    std::string documentation;
};

struct LSPLocation {
    std::string fileUri;
    std::uint32_t line   = 1;
    std::uint32_t column = 1;
};

class RobixLanguageServer {
public:
    void IndexDocument(std::string_view fileUri, std::string_view sourceCode);

    [[nodiscard]] std::vector<LSPCompletionItem> GetCompletions(std::string_view prefix) const;
    [[nodiscard]] std::optional<std::string> GetHoverDocumentation(std::string_view symbolName) const;
    [[nodiscard]] std::optional<LSPLocation> GoToDefinition(std::string_view symbolName) const;
    [[nodiscard]] std::string GenerateMarkdownDocumentation(std::string_view moduleTitle) const;

private:
    struct IndexedSymbol {
        std::string name;
        std::string kind;
        std::string signatureDetail;
        std::string explainDescription;
        LSPLocation definitionLoc;
    };
    std::unordered_map<std::string, IndexedSymbol> m_symbols;
};

// ============================================================================
// 3. COMPLETE DEBUGGING INFRASTRUCTURE (`robix debug` — VM & NATIVE PARITY)
// ============================================================================
// Supports conditional breakpoints, memory/variable watchpoints, call stack
// navigation, raw memory hex inspection, and structured crash dump generation.

struct ConditionalBreakpoint {
    std::uint32_t id = 0;
    std::string fileName;
    std::uint32_t line = 0;
    std::string conditionExpression; // e.g. "speed > 100"
    std::function<bool()> conditionPredicate;
    std::uint64_t hitCount = 0;
};

struct MemoryWatchpoint {
    std::uint32_t id = 0;
    std::string variableOrAddressName;
    std::int64_t lastObservedValue = 0;
    bool triggerOnWrite            = true;
    bool triggerOnRead             = false;
    std::uint64_t triggerCount     = 0;
};

struct DebugStackFrame {
    std::size_t frameIndex = 0;
    std::string functionName;
    std::string fileName;
    std::uint32_t line = 1;
    std::unordered_map<std::string, std::string> locals;
};

class RobixDebuggerEngine {
public:
    std::uint32_t AddConditionalBreakpoint(
        std::string fileName,
        std::uint32_t line,
        std::string conditionExpression,
        std::function<bool()> predicate = {});

    std::uint32_t AddWatchpoint(
        std::string variableOrAddressName,
        std::int64_t initialValue,
        bool triggerOnWrite = true,
        bool triggerOnRead = false);

    // Evaluates whether execution should pause at (fileName, line)
    bool CheckBreakpointHit(std::string_view fileName, std::uint32_t line);

    // Notifies debugger of a variable/memory read or write; returns true if watchpoint triggered
    bool NotifyMemoryAccess(std::string_view variableOrAddressName,
                            std::int64_t newValue,
                            bool isWrite);

    // Call Stack Navigation
    void PushFrame(std::string functionName,
                   std::string fileName,
                   std::uint32_t line,
                   std::unordered_map<std::string, std::string> locals = {});
    void PopFrame();
    [[nodiscard]] const std::vector<DebugStackFrame>& CallStack() const noexcept { return m_callStack; }
    [[nodiscard]] const DebugStackFrame* InspectFrame(std::size_t frameIndex) const noexcept;

    // Memory Hex Inspection & Symbolicated Crash Dump Generation
    [[nodiscard]] static std::string InspectMemoryHex(const void* address, std::size_t byteCount);
    [[nodiscard]] std::string GenerateNativeCrashDump(std::string_view faultReason,
                                                      std::string_view engineMode = "VM+NativeParity") const;

private:
    std::uint32_t m_nextBreakpointId = 1;
    std::uint32_t m_nextWatchpointId = 1;
    std::vector<ConditionalBreakpoint> m_breakpoints;
    std::vector<MemoryWatchpoint> m_watchpoints;
    std::vector<DebugStackFrame> m_callStack;
};

// ============================================================================
// 4. COMPLETE PROFILING & RACE-CONDITION DIAGNOSTICS (`robix profile`)
// ============================================================================
// Supports CPU flame graph SVG/folded generation, heap allocation hotspot
// visualization, and vector-clock / lock-set data-race detection (RACE9001).

struct ProfileSpanSample {
    std::vector<std::string> callStackFrames; // e.g. {"Main", "ControlLoop", "PIDUpdate"}
    std::uint64_t durationMicroseconds = 0;
};

struct HeapAllocationHotspot {
    std::string callsiteName;
    std::size_t activeAllocations = 0;
    std::size_t totalBytesAllocated = 0;
};

class RobixProfilerEngine {
public:
    void RecordCPUSample(std::vector<std::string> callStack, std::uint64_t durationUs);
    void RecordHeapAllocation(std::string callsiteName, std::size_t bytes);

    // Exports folded stack format and SVG Flame Graph visualization
    [[nodiscard]] std::string ExportFoldedFlameGraph() const;
    [[nodiscard]] std::string ExportSVGFlameGraph(std::string_view title = "Robix CPU Flame Graph") const;
    [[nodiscard]] std::string ExportHeapVisualizationJSON() const;

    // Data-Race Detector: detects unsynchronized concurrent read/write or write/write
    // across distinct threads without a shared lock held (reports RACE9001).
    bool RecordSharedMemoryAccess(std::string_view variableName,
                                  std::uint32_t threadId,
                                  bool isWrite,
                                  std::string_view heldLockName,
                                  Frontend::DiagnosticBag& diagnostics,
                                  Frontend::SourceLocation loc = {});

private:
    struct MemoryAccessRecord {
        std::uint32_t threadId = 0;
        bool isWrite           = false;
        std::string heldLockName;
    };

    std::vector<ProfileSpanSample> m_cpuSamples;
    std::unordered_map<std::string, HeapAllocationHotspot> m_heapHotspots;
    std::unordered_map<std::string, std::vector<MemoryAccessRecord>> m_accessHistory;
};

// ============================================================================
// 5. FORMAL 16-WORKLOAD BENCHMARK SUITE (`robix bench` / `robix benchmark`)
// ============================================================================

struct BenchmarkWorkloadMetrics {
    std::uint32_t workloadId = 0;
    std::string workloadName;
    double executionTimeMicroseconds = 0.0;
    std::size_t memoryConsumptionBytes = 0;
    std::size_t allocationCount        = 0;
    std::uint64_t verificationChecksum = 0;
};

class RobixBenchmarkSuite {
public:
    // Executes all 16 mandatory representative workloads and returns measured metrics
    [[nodiscard]] static std::vector<BenchmarkWorkloadMetrics> RunAll16Workloads();
    [[nodiscard]] static std::string FormatBenchmarkReport(const std::vector<BenchmarkWorkloadMetrics>& results);
};

} // namespace Robix::Tools

