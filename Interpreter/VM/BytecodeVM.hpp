#pragma once

#include "../Bytecode/BytecodeModule.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Robix::VM {

struct VMBuffer {
    std::vector<std::uint8_t> bytes;
    bool freed = false;
};

struct VMObjectInstance {
    std::string className;
    std::string baseChain;
    bool freed = false;
    std::unordered_map<std::string, struct VMValue> fields;
    std::shared_ptr<struct VMValue> cell;
    std::shared_ptr<VMBuffer> buffer;
    std::int64_t index = 0;
    int width = 8;
    bool signedPointee = false;
};

struct VMValue {
    using Storage = std::variant<
        std::monostate,
        std::int64_t,
        double,
        bool,
        std::string,
        std::shared_ptr<VMObjectInstance>>;

    Storage data{};

    [[nodiscard]] bool IsVoid() const noexcept { return std::holds_alternative<std::monostate>(data); }
    [[nodiscard]] bool IsInt() const noexcept { return std::holds_alternative<std::int64_t>(data); }
    [[nodiscard]] bool IsFloat() const noexcept { return std::holds_alternative<double>(data); }
    [[nodiscard]] bool IsBool() const noexcept { return std::holds_alternative<bool>(data); }
    [[nodiscard]] bool IsString() const noexcept { return std::holds_alternative<std::string>(data); }
    [[nodiscard]] bool IsObject() const noexcept { return std::holds_alternative<std::shared_ptr<VMObjectInstance>>(data); }

    [[nodiscard]] std::int64_t AsInt() const noexcept {
        if (IsInt()) return std::get<std::int64_t>(data);
        if (IsFloat()) return static_cast<std::int64_t>(std::get<double>(data));
        if (IsBool()) return std::get<bool>(data) ? 1 : 0;
        return 0;
    }

    [[nodiscard]] double AsFloat() const noexcept {
        if (IsFloat()) return std::get<double>(data);
        if (IsInt()) return static_cast<double>(std::get<std::int64_t>(data));
        return 0.0;
    }

    [[nodiscard]] bool AsBool() const noexcept {
        if (IsBool()) return std::get<bool>(data);
        if (IsInt()) return std::get<std::int64_t>(data) != 0;
        return false;
    }

    [[nodiscard]] std::string ToString() const;
};

struct VMTestSuiteResult {
    std::size_t passedTests = 0;
    std::size_t failedTests = 0;
    std::vector<std::string> failureMessages;
};

class BytecodeVM {
public:
    explicit BytecodeVM(const BytecodeModule& module, std::uint32_t jitHotThreshold = 10);

    // Execute a specific function (e.g. "Main") and return its VMValue result
    VMValue ExecuteFunction(std::string_view fnName, const std::vector<VMValue>& args = {});

    // Compute process exit code for Main() according to Robix Flexible Main() Specification
    [[nodiscard]] std::int32_t ExecuteMainAndGetExitCode();

    // Execute all Test "..." blocks in the module (Single-File Testing Mode)
    [[nodiscard]] VMTestSuiteResult ExecuteAllTests();

    [[nodiscard]] const std::string& GetStdoutBuffer() const noexcept { return m_stdoutBuffer; }
    [[nodiscard]] std::size_t GetAssertionPassCount() const noexcept { return m_assertPassCount; }
    [[nodiscard]] std::size_t GetAssertionFailCount() const noexcept { return m_assertFailCount; }
    [[nodiscard]] std::vector<std::string> GetHotJITCandidateFunctions() const;
    [[nodiscard]] const std::string& Fault() const noexcept { return m_fault; }
    static void SetLineTrace(std::vector<std::uint32_t>* lines);

private:
    const BytecodeModule& m_module;
    std::uint32_t m_jitHotThreshold = 10;
    std::string m_stdoutBuffer;
    std::size_t m_assertPassCount = 0;
    std::size_t m_assertFailCount = 0;
    std::string m_fault;
    bool m_gateChecked = false;
    bool m_gateOk = false;
    std::unordered_map<std::string, std::uint32_t> m_callCounts;
    std::unordered_map<std::string, std::string> m_virtualFileSystem;

    VMValue ExecuteChunk(const BytecodeChunk& chunk, const std::vector<VMValue>& args);
    [[nodiscard]] bool Gate();
    void Reject(const std::string& message);
};

} // namespace Robix::VM

