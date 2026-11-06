#include "LSPDebuggerProfilerAndBenchmarks.hpp"

#include "../../Compiler/Lexer/Lexer.hpp"
#include "../../Compiler/Parser/Parser.hpp"
#include "../../Compiler/Semantic/SemanticAnalyzer.hpp"
#include "../../Compiler/Types/TypeChecker.hpp"
#include "../../Runtime/Memory/UnifiedMemory.hpp"
#include "../../Runtime/Robotics/RoboticsAndControl.hpp"
#include "../../StdLib/AI/AIAndScientificComputing.hpp"
#include "../../StdLib/Collections/CollectionsAndIterators.hpp"
#include "../../StdLib/Core/CoreTypesAndRegex.hpp"
#include "../../StdLib/Data/JsonAndDataFormats.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace Robix::Tools {

// ============================================================================
// 1. ROBIX FORMATTER & STRICT LINTER IMPLEMENTATION
// ============================================================================

std::string RobixFormatterAndLinter::FormatSource(std::string_view sourceCode) {
    std::ostringstream out;
    int indentLevel = 0;

    auto lines = StdLib::StringOps::Split(sourceCode, "\n");
    if (!sourceCode.empty() && (sourceCode.back() == '\n' || sourceCode.back() == '\r') &&
        !lines.empty() && lines.back().empty()) {
        lines.pop_back();
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::string line = lines[i];
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string trimmed = StdLib::StringOps::Trim(line);
        if (trimmed.empty()) {
            out << "\n";
            continue;
        }

        if (StdLib::StringOps::StartsWith(trimmed, "}") && indentLevel > 0) {
            --indentLevel;
        }

        out << std::string(static_cast<std::size_t>(indentLevel * 4), ' ') << trimmed << "\n";

        if (StdLib::StringOps::EndsWith(trimmed, "{")) {
            ++indentLevel;
        }
    }
    return out.str();
}

bool RobixFormatterAndLinter::LintSource(std::string_view fileName,
                                         std::string_view sourceCode,
                                         bool strictMode,
                                         Frontend::DiagnosticBag& diagnostics) {
    Frontend::Lexer lexer(sourceCode, fileName, diagnostics);
    Frontend::Parser parser(lexer.TokenizeAll(), fileName, diagnostics);
    auto ast = parser.ParseCompilationUnit();

    Frontend::TypeCheckerOptions opts;
    opts.strictStyleMode = strictMode;
    opts.requireExplain  = strictMode;
    Frontend::TypeChecker tc(diagnostics, opts);
    tc.CheckCompilationUnit(ast);

    Frontend::SemanticAnalyzer sem(diagnostics);
    sem.AnalyzeCompilationUnit(ast);

    return !diagnostics.HasErrors();
}

// ============================================================================
// 2. LANGUAGE SERVER PROTOCOL (LSP) & DOCUMENTATION GENERATOR IMPLEMENTATION
// ============================================================================

static std::string ExplainText(const Frontend::ExplainBlockNode& block) {
    std::string text = block.description;
    if (!block.processing.empty()) {
        if (!text.empty()) text += "\n";
        text += "Processing: " + block.processing;
    }
    if (!block.output.empty()) {
        if (!text.empty()) text += "\n";
        text += "Output: " + block.output;
    }
    return text;
}

void RobixLanguageServer::IndexDocument(std::string_view fileUri, std::string_view sourceCode) {
    Frontend::DiagnosticBag diags;
    Frontend::Lexer lexer(sourceCode, fileUri, diags);
    Frontend::Parser parser(lexer.TokenizeAll(), fileUri, diags);
    const auto ast = parser.ParseCompilationUnit();

    const auto index = [&](const auto& self, const Frontend::DeclNode& decl) -> void {
        const auto& loc = decl.location;
        if (decl.kind == Frontend::DeclKind::Function) {
            IndexedSymbol sym;
            sym.name               = decl.name;
            sym.kind               = "Function";
            sym.signatureDetail    = "Function " + decl.name + "(...)";
            sym.explainDescription = decl.explainBlock ? ExplainText(*decl.explainBlock) : "";
            sym.definitionLoc      = LSPLocation{ std::string(fileUri), loc.line, loc.column };
            m_symbols[sym.name]    = std::move(sym);
        } else if (decl.kind == Frontend::DeclKind::Class || decl.kind == Frontend::DeclKind::Struct) {
            IndexedSymbol sym;
            sym.name               = decl.name;
            sym.kind               = decl.kind == Frontend::DeclKind::Class ? "Class" : "Struct";
            sym.signatureDetail    = sym.kind + " " + decl.name;
            sym.explainDescription = decl.explainBlock ? ExplainText(*decl.explainBlock) : "";
            sym.definitionLoc      = LSPLocation{ std::string(fileUri), loc.line, loc.column };
            m_symbols[sym.name]    = std::move(sym);
        }
        for (const auto& member : decl.members) {
            if (member) self(self, *member);
        }
    };
    for (const auto& declPtr : ast.declarations) {
        if (declPtr) index(index, *declPtr);
    }
}

std::vector<LSPCompletionItem> RobixLanguageServer::GetCompletions(std::string_view prefix) const {
    std::vector<LSPCompletionItem> items;
    for (const auto& [name, sym] : m_symbols) {
        if (prefix.empty() || StdLib::StringOps::StartsWith(name, prefix)) {
            items.push_back(LSPCompletionItem{
                sym.name,
                sym.kind,
                sym.signatureDetail,
                sym.explainDescription
            });
        }
    }
    return items;
}

std::optional<std::string> RobixLanguageServer::GetHoverDocumentation(std::string_view symbolName) const {
    auto it = m_symbols.find(std::string(symbolName));
    if (it == m_symbols.end()) return std::nullopt;
    std::string hover = "**" + it->second.signatureDetail + "**";
    if (!it->second.explainDescription.empty()) {
        hover += "\n\nExplain: " + it->second.explainDescription;
    }
    return hover;
}

std::optional<LSPLocation> RobixLanguageServer::GoToDefinition(std::string_view symbolName) const {
    auto it = m_symbols.find(std::string(symbolName));
    if (it == m_symbols.end()) return std::nullopt;
    return it->second.definitionLoc;
}

std::string RobixLanguageServer::GenerateMarkdownDocumentation(std::string_view moduleTitle) const {
    std::ostringstream md;
    md << "# Robix Documentation: " << moduleTitle << "\n\n";
    for (const auto& [name, sym] : m_symbols) {
        md << "## `" << sym.signatureDetail << "` (" << sym.kind << ")\n";
        if (!sym.explainDescription.empty()) {
            md << "- **Explain:** " << sym.explainDescription << "\n";
        }
        md << "- **Defined at:** `" << sym.definitionLoc.fileUri << ":" << sym.definitionLoc.line << "`\n\n";
    }
    return md.str();
}

// ============================================================================
// 3. COMPLETE DEBUGGING INFRASTRUCTURE IMPLEMENTATION
// ============================================================================

std::uint32_t RobixDebuggerEngine::AddConditionalBreakpoint(
    std::string fileName,
    std::uint32_t line,
    std::string conditionExpression,
    std::function<bool()> predicate) {

    const std::uint32_t id = m_nextBreakpointId++;
    m_breakpoints.push_back(ConditionalBreakpoint{
        id,
        std::move(fileName),
        line,
        std::move(conditionExpression),
        std::move(predicate),
        0
    });
    return id;
}

std::uint32_t RobixDebuggerEngine::AddWatchpoint(
    std::string variableOrAddressName,
    std::int64_t initialValue,
    bool triggerOnWrite,
    bool triggerOnRead) {

    const std::uint32_t id = m_nextWatchpointId++;
    m_watchpoints.push_back(MemoryWatchpoint{
        id,
        std::move(variableOrAddressName),
        initialValue,
        triggerOnWrite,
        triggerOnRead,
        0
    });
    return id;
}

bool RobixDebuggerEngine::CheckBreakpointHit(std::string_view fileName, std::uint32_t line) {
    for (auto& bp : m_breakpoints) {
        if (bp.fileName == fileName && bp.line == line) {
            const bool condMet = !bp.conditionPredicate || bp.conditionPredicate();
            if (condMet) {
                ++bp.hitCount;
                return true;
            }
        }
    }
    return false;
}

bool RobixDebuggerEngine::NotifyMemoryAccess(
    std::string_view variableOrAddressName,
    std::int64_t newValue,
    bool isWrite) {

    bool triggered = false;
    for (auto& wp : m_watchpoints) {
        if (wp.variableOrAddressName == variableOrAddressName) {
            if (isWrite && wp.triggerOnWrite && newValue != wp.lastObservedValue) {
                wp.lastObservedValue = newValue;
                ++wp.triggerCount;
                triggered = true;
            } else if (!isWrite && wp.triggerOnRead) {
                ++wp.triggerCount;
                triggered = true;
            }
        }
    }
    return triggered;
}

void RobixDebuggerEngine::PushFrame(
    std::string functionName,
    std::string fileName,
    std::uint32_t line,
    std::unordered_map<std::string, std::string> locals) {

    m_callStack.push_back(DebugStackFrame{
        m_callStack.size(),
        std::move(functionName),
        std::move(fileName),
        line,
        std::move(locals)
    });
}

void RobixDebuggerEngine::PopFrame() {
    if (!m_callStack.empty()) {
        m_callStack.pop_back();
    }
}

const DebugStackFrame* RobixDebuggerEngine::InspectFrame(std::size_t frameIndex) const noexcept {
    if (frameIndex >= m_callStack.size()) return nullptr;
    return &m_callStack[frameIndex];
}

std::string RobixDebuggerEngine::InspectMemoryHex(const void* address, std::size_t byteCount) {
    if (!address || byteCount == 0) return "";
    const auto* bytes = static_cast<const std::uint8_t*>(address);
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    for (std::size_t i = 0; i < byteCount; ++i) {
        if (i > 0) oss << ' ';
        oss << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }
    return oss.str();
}

std::string RobixDebuggerEngine::GenerateNativeCrashDump(
    std::string_view faultReason,
    std::string_view engineMode) const {

    std::ostringstream dump;
    dump << "=== ROBIX SYMBOLICATED CRASH DUMP ===\n";
    dump << "EngineMode: " << engineMode << "\n";
    dump << "FaultReason: " << faultReason << "\n";
    dump << "CallStack (" << m_callStack.size() << " frame(s)):\n";
    for (std::size_t i = m_callStack.size(); i-- > 0;) {
        const auto& f = m_callStack[i];
        dump << "  #" << f.frameIndex << " " << f.functionName << " at " << f.fileName << ":" << f.line << "\n";
    }
    return dump.str();
}

// ============================================================================
// 4. COMPLETE PROFILING & RACE-CONDITION DIAGNOSTICS IMPLEMENTATION
// ============================================================================

void RobixProfilerEngine::RecordCPUSample(std::vector<std::string> callStack, std::uint64_t durationUs) {
    m_cpuSamples.push_back(ProfileSpanSample{ std::move(callStack), durationUs });
}

void RobixProfilerEngine::RecordHeapAllocation(std::string callsiteName, std::size_t bytes) {
    auto& h = m_heapHotspots[callsiteName];
    h.callsiteName = callsiteName;
    h.activeAllocations += 1;
    h.totalBytesAllocated += bytes;
}

std::string RobixProfilerEngine::ExportFoldedFlameGraph() const {
    std::ostringstream out;
    for (const auto& s : m_cpuSamples) {
        for (std::size_t i = 0; i < s.callStackFrames.size(); ++i) {
            if (i > 0) out << ';';
            out << s.callStackFrames[i];
        }
        out << ' ' << s.durationMicroseconds << "\n";
    }
    return out.str();
}

std::string RobixProfilerEngine::ExportSVGFlameGraph(std::string_view title) const {
    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"300\">\n";
    svg << "  <text x=\"20\" y=\"30\">" << title << "</text>\n";
    std::uint64_t yOffset = 50;
    for (const auto& s : m_cpuSamples) {
        std::string label;
        for (std::size_t i = 0; i < s.callStackFrames.size(); ++i) {
            if (i > 0) label += " -> ";
            label += s.callStackFrames[i];
        }
        svg << "  <rect x=\"20\" y=\"" << yOffset << "\" width=\"" << (s.durationMicroseconds + 50)
            << "\" height=\"24\" fill=\"#FF6B35\"/>\n";
        svg << "  <text x=\"25\" y=\"" << (yOffset + 16) << "\">" << label
            << " (" << s.durationMicroseconds << " us)</text>\n";
        yOffset += 30;
    }
    svg << "</svg>\n";
    return svg.str();
}

std::string RobixProfilerEngine::ExportHeapVisualizationJSON() const {
    std::ostringstream js;
    js << "{\"heapHotspots\":[";
    bool first = true;
    for (const auto& [name, spot] : m_heapHotspots) {
        if (!first) js << ",";
        first = false;
        js << "{\"callsite\":\"" << spot.callsiteName
           << "\",\"allocations\":" << spot.activeAllocations
           << ",\"totalBytes\":" << spot.totalBytesAllocated << "}";
    }
    js << "]}";
    return js.str();
}

bool RobixProfilerEngine::RecordSharedMemoryAccess(
    std::string_view variableName,
    std::uint32_t threadId,
    bool isWrite,
    std::string_view heldLockName,
    Frontend::DiagnosticBag& diagnostics,
    Frontend::SourceLocation loc) {

    const std::string key(variableName);
    auto& history = m_accessHistory[key];

    for (const auto& prev : history) {
        if (prev.threadId != threadId && (isWrite || prev.isWrite)) {
            const bool sharesCommonLock = !heldLockName.empty() && (heldLockName == prev.heldLockName);
            if (!sharesCommonLock) {
                diagnostics.ReportError(
                    "RACE9001",
                    "Data race detected on shared variable '" + key + "' between thread " +
                        std::to_string(prev.threadId) + " and thread " + std::to_string(threadId) +
                        " without a common synchronization lock.",
                    loc,
                    "Protect shared state with a Robix.Threading.Mutex, ReadWriteLock, or Atomic<T>.");
                history.push_back(MemoryAccessRecord{ threadId, isWrite, std::string(heldLockName) });
                return false;
            }
        }
    }

    history.push_back(MemoryAccessRecord{ threadId, isWrite, std::string(heldLockName) });
    return true;
}

// ============================================================================
// 5. FORMAL 16-WORKLOAD BENCHMARK SUITE IMPLEMENTATION
// ============================================================================

std::vector<BenchmarkWorkloadMetrics> RobixBenchmarkSuite::RunAll16Workloads() {
    std::vector<BenchmarkWorkloadMetrics> results;
    results.reserve(16);

    auto measureWorkload = [&](std::uint32_t id,
                               std::string name,
                               std::size_t memBytes,
                               std::size_t allocs,
                               const std::function<std::uint64_t()>& fn) {
        const auto t0 = std::chrono::steady_clock::now();
        const std::uint64_t checksum = fn();
        const auto t1 = std::chrono::steady_clock::now();
        const double elapsedUs = std::chrono::duration<double, std::micro>(t1 - t0).count();
        results.push_back(BenchmarkWorkloadMetrics{
            id,
            std::move(name),
            elapsedUs >= 0.01 ? elapsedUs : 0.01,
            memBytes,
            allocs,
            checksum
        });
    };

    // 1. Integer arithmetic
    measureWorkload(1, "IntegerArithmetic", 0, 0, []() {
        std::uint64_t acc = 1;
        for (std::uint64_t i = 1; i <= 1000; ++i) acc = (acc * 31 + i) ^ (i << 1);
        return acc;
    });

    // 2. Floating-point calculations
    measureWorkload(2, "FloatingPointCalculations", 0, 0, []() {
        double s = 0.0;
        for (int i = 1; i <= 500; ++i) s += std::sin(static_cast<double>(i) * 0.01) * std::cos(static_cast<double>(i) * 0.01);
        return static_cast<std::uint64_t>( std::abs(s) * 1000.0 ) + 1;
    });

    // 3. Matrix multiplication
    measureWorkload(3, "MatrixMultiplication", 8 * 8 * sizeof(double) * 3, 3, []() {
        StdLib::Matrix<double> a(8, 8, 1.5);
        StdLib::Matrix<double> b(8, 8, 2.0);
        auto c = a.Multiply(b);
        return static_cast<std::uint64_t>(c.At(0, 0));
    });

    // 4. Memory allocation (Arena O(1) bump allocator)
    measureWorkload(4, "MemoryAllocation", 4096, 64, []() {
        Runtime::ArenaAllocator arena(4096, 1);
        std::uint64_t count = 0;
        for (int i = 0; i < 64; ++i) {
            if (arena.Allocate(32, 8) != nullptr) ++count;
        }
        return count;
    });

    // 5. Memory copying (DMA aligned transfer)
    measureWorkload(5, "MemoryCopying", 512, 2, []() {
        Runtime::DMABufferRegion src(256, 64);
        Runtime::DMABufferRegion dst(256, 64);
        std::memset(src.Data(), 0x42, 256);
        std::memcpy(dst.Data(), src.Data(), 256);
        return static_cast<std::uint64_t>(static_cast<std::uint8_t*>(dst.Data())[128]);
    });

    // 6. Array traversal
    measureWorkload(6, "ArrayTraversal", 256 * sizeof(int), 1, []() {
        std::vector<int> arr(256, 3);
        std::uint64_t sum = 0;
        for (int v : arr) sum += static_cast<std::uint64_t>(v);
        return sum;
    });

    // 7. Function calls
    measureWorkload(7, "FunctionCalls", 0, 0, []() {
        auto stepFn = [](std::uint64_t x) noexcept { return x * 3 + 7; };
        std::uint64_t v = 1;
        for (int i = 0; i < 200; ++i) v = stepFn(v);
        return v;
    });

    // 8. Object creation
    measureWorkload(8, "ObjectCreation", 640, 10, []() {
        Frontend::DiagnosticBag diags;
        Runtime::UnifiedMemoryManager mm(diags);
        std::uint64_t validCount = 0;
        for (int i = 0; i < 10; ++i) {
            auto h = mm.AllocateManagedObject("RobotNode", 64);
            if (h.IsValid()) ++validCount;
        }
        return validCount;
    });

    // 9. Virtual dispatch
    measureWorkload(9, "VirtualDispatch", 64, 2, []() {
        struct BaseActuator {
            virtual ~BaseActuator() = default;
            virtual std::uint64_t Step(std::uint64_t u) const noexcept = 0;
        };
        struct ServoActuator final : BaseActuator {
            std::uint64_t Step(std::uint64_t u) const noexcept override { return u + 5; }
        };
        ServoActuator servo;
        const BaseActuator* ptr = &servo;
        std::uint64_t acc = 0;
        for (int i = 0; i < 100; ++i) acc = ptr->Step(acc);
        return acc;
    });

    // 10. String processing
    measureWorkload(10, "StringProcessing", 256, 4, []() {
        const std::string s = StdLib::StringOps::Replace("Robix_Intelligent_Machines", "_", " ");
        return static_cast<std::uint64_t>(s.size());
    });

    // 11. Multithreading
    measureWorkload(11, "Multithreading", 128, 1, []() {
        std::atomic<std::uint64_t> counter{ 0 };
        for (int i = 0; i < 100; ++i) counter.fetch_add(1, std::memory_order_relaxed);
        return counter.load();
    });

    // 12. Concurrency (Lock-free SPSC queue)
    measureWorkload(12, "Concurrency", sizeof(int) * 16, 0, []() {
        Runtime::LockFreeSPSCQueue<int, 16> q;
        for (int i = 1; i <= 10; ++i) q.TryPush(i);
        std::uint64_t total = 0;
        while (auto v = q.TryPop()) total += static_cast<std::uint64_t>(*v);
        return total;
    });

    // 13. Robotics calculations (Forward & Inverse Kinematics)
    measureWorkload(13, "RoboticsCalculations", 0, 0, []() {
        Runtime::Planar2LinkManipulator arm{ 1.0, 1.0 };
        auto pos = arm.ForwardKinematics(0.3, 0.6);
        return static_cast<std::uint64_t>((pos.x + pos.y) * 1000.0);
    });

    // 14. PID control
    measureWorkload(14, "PIDControl", 0, 0, []() {
        Runtime::PIDController pid(2.0, 0.5, 0.05, -10.0, 10.0);
        double x = 0.0;
        for (int i = 0; i < 50; ++i) x += pid.Update(1.0, x, 0.01) * 0.01;
        return static_cast<std::uint64_t>(x * 1000.0);
    });

    // 15. Serialization (JSON serialize + parse)
    measureWorkload(15, "Serialization", 256, 3, []() {
        StdLib::JsonValue obj = StdLib::JsonValue::MakeObject();
        obj.objectVal["robot"] = StdLib::JsonValue::MakeString("RobixBot");
        obj.objectVal["speed"] = StdLib::JsonValue::MakeInt(120);
        const std::string text = StdLib::JsonCodec::Serialize(obj);
        auto parsed = StdLib::JsonCodec::Parse(text);
        return static_cast<std::uint64_t>(parsed.GetField("speed")->intVal);
    });

    // 16. Neural network inference
    measureWorkload(16, "NeuralNetworkInference", 256, 2, []() {
        StdLib::Matrix<double> w(2, 2, 0.5);
        StdLib::DenseLayer layer(2, 2, w, { 0.1, 0.2 }, StdLib::ActivationKind::ReLU);
        Frontend::DiagnosticBag diags;
        auto out = layer.Forward({ 1.0, 2.0 }, diags);
        return out.has_value() ? static_cast<std::uint64_t>((*out)[0] * 100.0) : 0ULL;
    });

    return results;
}

std::string RobixBenchmarkSuite::FormatBenchmarkReport(const std::vector<BenchmarkWorkloadMetrics>& results) {
    std::ostringstream oss;
    oss << "=== ROBIX 16-WORKLOAD PERFORMANCE BENCHMARK REPORT ===\n";
    for (const auto& m : results) {
        oss << "[" << std::setw(2) << m.workloadId << "] " << std::left << std::setw(26) << m.workloadName
            << " | Time: " << std::fixed << std::setprecision(2) << m.executionTimeMicroseconds << " us"
            << " | Mem: " << m.memoryConsumptionBytes << " B"
            << " | Allocs: " << m.allocationCount
            << " | Checksum: " << m.verificationChecksum << "\n";
    }
    return oss.str();
}

} // namespace Robix::Tools

