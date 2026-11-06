#include "BytecodeVM.hpp"
#include "../../Compiler/Types/SpecValidator.hpp"

#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace Robix::VM {

namespace {

[[noreturn]] void Fail(const char* message) {
    std::fputs(message, stderr);
    std::fputc('\n', stderr);
    std::abort();
}

std::shared_ptr<VMObjectInstance> AsObj(const VMValue& value) {
    if (!value.IsObject()) return nullptr;
    return std::get<std::shared_ptr<VMObjectInstance>>(value.data);
}

std::int64_t ReadPointee(const VMObjectInstance& obj) {
    if (obj.cell) return obj.cell->AsInt();
    if (!obj.buffer || obj.buffer->freed) Fail("robix vm: use after free");
    const int width = obj.width <= 0 ? 1 : obj.width;
    if (obj.index < 0 || obj.index + width > static_cast<std::int64_t>(obj.buffer->bytes.size())) {
        Fail("robix vm: pointer out of bounds");
    }
    std::uint64_t bits = 0;
    for (int i = 0; i < width; ++i) {
        bits |= static_cast<std::uint64_t>(obj.buffer->bytes[static_cast<std::size_t>(obj.index + i)]) << (8 * i);
    }
    if (obj.signedPointee && width < 8) {
        const std::uint64_t sign = 1ull << (width * 8 - 1);
        if (bits & sign) bits |= ~((1ull << (width * 8)) - 1);
    }
    return static_cast<std::int64_t>(bits);
}

void WritePointee(VMObjectInstance& obj, std::int64_t value) {
    if (obj.cell) {
        *obj.cell = VMValue{ value };
        return;
    }
    if (!obj.buffer || obj.buffer->freed) Fail("robix vm: use after free");
    const int width = obj.width <= 0 ? 1 : obj.width;
    if (obj.index < 0 || obj.index + width > static_cast<std::int64_t>(obj.buffer->bytes.size())) {
        Fail("robix vm: pointer out of bounds");
    }
    auto bits = static_cast<std::uint64_t>(value);
    for (int i = 0; i < width; ++i) {
        obj.buffer->bytes[static_cast<std::size_t>(obj.index + i)] =
            static_cast<std::uint8_t>((bits >> (8 * i)) & 0xFFu);
    }
}

} // namespace

std::string VMValue::ToString() const {
    if (IsInt()) {
        return std::to_string(std::get<std::int64_t>(data));
    }
    if (IsFloat()) {
        std::ostringstream oss;
        oss << std::get<double>(data);
        return oss.str();
    }
    if (IsBool()) {
        return std::get<bool>(data) ? "true" : "false";
    }
    if (IsString()) {
        return std::get<std::string>(data);
    }
    if (IsObject()) {
        const auto& ptr = std::get<std::shared_ptr<VMObjectInstance>>(data);
        return ptr ? ("<" + ptr->className + ">") : "null";
    }
    return "void";
}

BytecodeVM::BytecodeVM(const BytecodeModule& module, std::uint32_t jitHotThreshold)
    : m_module(module), m_jitHotThreshold(jitHotThreshold) {}

std::vector<std::string> BytecodeVM::GetHotJITCandidateFunctions() const {
    std::vector<std::string> hot;
    for (const auto& [name, count] : m_callCounts) {
        if (count >= m_jitHotThreshold) {
            hot.push_back(name);
        }
    }
    return hot;
}

bool BytecodeVM::Gate() {
    if (m_gateChecked) return m_gateOk;
    m_gateChecked = true;
    if (m_module.magic != std::string(BYTECODE_MAGIC) || m_module.version != BYTECODE_VERSION) {
        Reject("robix vm: bytecode version " + std::to_string(m_module.version) +
               " is not supported. This VM accepts version " + std::to_string(BYTECODE_VERSION) + ".");
        m_gateOk = false;
        return false;
    }
    for (const auto& chunk : m_module.chunks) {
        for (const auto& inst : chunk.code) {
            if ((inst.opcode == BCOpcode::Jump || inst.opcode == BCOpcode::JumpIfFalse) &&
                inst.targetPC >= chunk.code.size()) {
                Reject("robix vm: bytecode jump in '" + chunk.name + "' is outside the chunk.");
                m_gateOk = false;
                return false;
            }
        }
    }
    m_gateOk = true;
    return true;
}

void BytecodeVM::Reject(const std::string& message) {
    if (!m_fault.empty()) return;
    m_fault = message;
    std::fputs(message.c_str(), stderr);
    std::fputc('\n', stderr);
}

static std::vector<std::uint32_t>* g_lineTrace = nullptr;

void BytecodeVM::SetLineTrace(std::vector<std::uint32_t>* lines) {
    g_lineTrace = lines;
}

VMValue BytecodeVM::ExecuteFunction(std::string_view fnName, const std::vector<VMValue>& args) {
    if (!Gate()) return VMValue{};
    if (fnName == "puts") {
        if (!args.empty()) {
            std::puts(args[0].ToString().c_str());
        }
        return VMValue{ std::int64_t{ 0 } };
    }
    const BytecodeChunk* chunk = m_module.FindChunk(fnName);
    if (!chunk) {
        return VMValue{};
    }
    return ExecuteChunk(*chunk, args);
}

std::int32_t BytecodeVM::ExecuteMainAndGetExitCode() {
    if (!Gate()) return 1;
    const BytecodeChunk* mainChunk = m_module.FindChunk("Main");
    if (!mainChunk) {
        return 1;
    }
    VMValue ret = ExecuteChunk(*mainChunk, {});
    if (!m_fault.empty()) return 1;

    if (mainChunk->returnTypeName == "Void") {
        return Spec::ComputeProcessExitCode(Spec::MainReturnCategory::VoidReturn, 0, true);
    }
    if (mainChunk->returnTypeName == "Int" || ret.IsInt()) {
        return Spec::ComputeProcessExitCode(Spec::MainReturnCategory::IntExitCode, ret.AsInt(), true);
    }
    return Spec::ComputeProcessExitCode(Spec::MainReturnCategory::StructuredOutput, 0, true);
}

VMTestSuiteResult BytecodeVM::ExecuteAllTests() {
    VMTestSuiteResult suite;
    if (!Gate()) {
        suite.failedTests = 1;
        suite.failureMessages.push_back(m_fault);
        return suite;
    }
    for (const auto& chunk : m_module.chunks) {
        if (!chunk.isTestChunk) continue;
        const std::size_t failsBefore = m_assertFailCount;
        (void)ExecuteChunk(chunk, {});
        if (!m_fault.empty()) {
            ++suite.failedTests;
            suite.failureMessages.push_back(m_fault);
            break;
        }
        if (m_assertFailCount == failsBefore) {
            ++suite.passedTests;
        } else {
            ++suite.failedTests;
            suite.failureMessages.push_back("Failed test block: " + chunk.name);
        }
    }
    return suite;
}

VMValue BytecodeVM::ExecuteChunk(const BytecodeChunk& chunk, const std::vector<VMValue>& args) {
    if (!Gate() || !m_fault.empty()) return VMValue{};
    ++m_callCounts[chunk.name];

    std::unordered_map<std::uint32_t, VMValue> regs;
    std::unordered_map<std::string, VMValue> locals;
    std::unordered_map<std::string, std::shared_ptr<VMValue>> cells;

    // Bind `this` if executing a class method where arg[0] is an object
    std::size_t paramStartIdx = 0;
    if (!args.empty() && args[0].IsObject() && chunk.name.find('.') != std::string::npos) {
        locals["this"] = args[0];
        paramStartIdx  = 1;
    }

    for (std::size_t i = 0; i < chunk.paramNames.size() && (i + paramStartIdx) < args.size(); ++i) {
        locals[chunk.paramNames[i]] = args[i + paramStartIdx];
    }

    std::size_t pc = 0;
    while (pc < chunk.code.size()) {
        if (!m_fault.empty()) return VMValue{};
        const BCInstruction& inst = chunk.code[pc];
        ++pc;
        if (g_lineTrace != nullptr && inst.sourceLine != 0 &&
            (g_lineTrace->empty() || g_lineTrace->back() != inst.sourceLine)) {
            g_lineTrace->push_back(inst.sourceLine);
        }

        switch (inst.opcode) {
            case BCOpcode::Nop:
            case BCOpcode::DeferCleanup:
                break;

            case BCOpcode::InlineASM:
                if (!(inst.symbolImm.empty() || inst.symbolImm == "NOP" || inst.symbolImm == "nop")) {
                    Reject("robix vm: InlineASM runs in a native build.");
                }
                break;

            case BCOpcode::LoadConstInt:
                regs[inst.destReg] = VMValue{ inst.intImm };
                break;

            case BCOpcode::LoadConstFloat:
                regs[inst.destReg] = VMValue{ inst.floatImm };
                break;

            case BCOpcode::LoadConstBool:
                regs[inst.destReg] = VMValue{ inst.boolImm };
                break;

            case BCOpcode::LoadConstStr:
                regs[inst.destReg] = VMValue{ inst.symbolImm };
                break;

            case BCOpcode::CopyReg: {
                const VMValue& src = regs[inst.srcReg1];
                if (inst.symbolImm == "ToFloat") {
                    regs[inst.destReg] = VMValue{ src.AsFloat() };
                } else if (inst.symbolImm == "ToInt") {
                    regs[inst.destReg] = VMValue{ src.AsInt() };
                } else if (inst.symbolImm == "ToString") {
                    regs[inst.destReg] = VMValue{ src.ToString() };
                } else {
                    regs[inst.destReg] = src;
                }
                break;
            }

            case BCOpcode::Add: {
                const VMValue& l = regs[inst.srcReg1];
                const VMValue& r = regs[inst.srcReg2];
                if (l.IsString() || r.IsString()) {
                    regs[inst.destReg] = VMValue{ l.ToString() + r.ToString() };
                } else if (l.IsFloat() || r.IsFloat()) {
                    regs[inst.destReg] = VMValue{ l.AsFloat() + r.AsFloat() };
                } else {
                    regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(
                        static_cast<std::uint64_t>(l.AsInt()) + static_cast<std::uint64_t>(r.AsInt())) };
                }
                break;
            }

            case BCOpcode::Sub: {
                const VMValue& l = regs[inst.srcReg1];
                const VMValue& r = regs[inst.srcReg2];
                if (l.IsFloat() || r.IsFloat()) {
                    regs[inst.destReg] = VMValue{ l.AsFloat() - r.AsFloat() };
                } else {
                    regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(
                        static_cast<std::uint64_t>(l.AsInt()) - static_cast<std::uint64_t>(r.AsInt())) };
                }
                break;
            }

            case BCOpcode::Mul: {
                const VMValue& l = regs[inst.srcReg1];
                const VMValue& r = regs[inst.srcReg2];
                if (l.IsFloat() || r.IsFloat()) {
                    regs[inst.destReg] = VMValue{ l.AsFloat() * r.AsFloat() };
                } else {
                    regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(
                        static_cast<std::uint64_t>(l.AsInt()) * static_cast<std::uint64_t>(r.AsInt())) };
                }
                break;
            }

            case BCOpcode::Div: {
                const VMValue& l = regs[inst.srcReg1];
                const VMValue& r = regs[inst.srcReg2];
                if (l.IsFloat() || r.IsFloat()) {
                    const double d = r.AsFloat();
                    regs[inst.destReg] = VMValue{ d != 0.0 ? (l.AsFloat() / d) : 0.0 };
                } else {
                    const std::int64_t d = r.AsInt();
                    regs[inst.destReg] = VMValue{ d != 0 ? (l.AsInt() / d) : 0 };
                }
                break;
            }

            case BCOpcode::Mod: {
                const std::int64_t l = regs[inst.srcReg1].AsInt();
                const std::int64_t r = regs[inst.srcReg2].AsInt();
                regs[inst.destReg]   = VMValue{ r != 0 ? (l % r) : 0 };
                break;
            }

            case BCOpcode::Eq: {
                const VMValue& l = regs[inst.srcReg1];
                const VMValue& r = regs[inst.srcReg2];
                if (l.IsString() && r.IsString()) {
                    regs[inst.destReg] = VMValue{ l.ToString() == r.ToString() };
                } else if (l.IsFloat() || r.IsFloat()) {
                    regs[inst.destReg] = VMValue{ l.AsFloat() == r.AsFloat() };
                } else {
                    regs[inst.destReg] = VMValue{ l.AsInt() == r.AsInt() };
                }
                break;
            }

            case BCOpcode::Ne: {
                const VMValue& l = regs[inst.srcReg1];
                const VMValue& r = regs[inst.srcReg2];
                if (l.IsString() && r.IsString()) {
                    regs[inst.destReg] = VMValue{ l.ToString() != r.ToString() };
                } else if (l.IsFloat() || r.IsFloat()) {
                    regs[inst.destReg] = VMValue{ l.AsFloat() != r.AsFloat() };
                } else {
                    regs[inst.destReg] = VMValue{ l.AsInt() != r.AsInt() };
                }
                break;
            }

            case BCOpcode::Lt: {
                const VMValue& l = regs[inst.srcReg1];
                const VMValue& r = regs[inst.srcReg2];
                regs[inst.destReg] = VMValue{ (l.IsFloat() || r.IsFloat()) ? (l.AsFloat() < r.AsFloat()) : (l.AsInt() < r.AsInt()) };
                break;
            }

            case BCOpcode::Le: {
                const VMValue& l = regs[inst.srcReg1];
                const VMValue& r = regs[inst.srcReg2];
                regs[inst.destReg] = VMValue{ (l.IsFloat() || r.IsFloat()) ? (l.AsFloat() <= r.AsFloat()) : (l.AsInt() <= r.AsInt()) };
                break;
            }

            case BCOpcode::Gt: {
                const VMValue& l = regs[inst.srcReg1];
                const VMValue& r = regs[inst.srcReg2];
                regs[inst.destReg] = VMValue{ (l.IsFloat() || r.IsFloat()) ? (l.AsFloat() > r.AsFloat()) : (l.AsInt() > r.AsInt()) };
                break;
            }

            case BCOpcode::Ge: {
                const VMValue& l = regs[inst.srcReg1];
                const VMValue& r = regs[inst.srcReg2];
                regs[inst.destReg] = VMValue{ (l.IsFloat() || r.IsFloat()) ? (l.AsFloat() >= r.AsFloat()) : (l.AsInt() >= r.AsInt()) };
                break;
            }

            case BCOpcode::And:
                regs[inst.destReg] = VMValue{ regs[inst.srcReg1].AsBool() && regs[inst.srcReg2].AsBool() };
                break;

            case BCOpcode::Or:
                regs[inst.destReg] = VMValue{ regs[inst.srcReg1].AsBool() || regs[inst.srcReg2].AsBool() };
                break;

            case BCOpcode::BitAnd:
                regs[inst.destReg] = VMValue{ regs[inst.srcReg1].AsInt() & regs[inst.srcReg2].AsInt() };
                break;

            case BCOpcode::BitOr:
                regs[inst.destReg] = VMValue{ regs[inst.srcReg1].AsInt() | regs[inst.srcReg2].AsInt() };
                break;

            case BCOpcode::BitXor:
                regs[inst.destReg] = VMValue{ regs[inst.srcReg1].AsInt() ^ regs[inst.srcReg2].AsInt() };
                break;

            case BCOpcode::Shl: {
                const auto bits = static_cast<unsigned>(regs[inst.srcReg2].AsInt() & 63);
                const auto value = static_cast<std::uint64_t>(regs[inst.srcReg1].AsInt());
                regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(value << bits) };
                break;
            }

            case BCOpcode::Shr: {
                const auto bits = static_cast<unsigned>(regs[inst.srcReg2].AsInt() & 63);
                regs[inst.destReg] = VMValue{ regs[inst.srcReg1].AsInt() >> bits };
                break;
            }

            case BCOpcode::BitNot:
                regs[inst.destReg] = VMValue{ ~regs[inst.srcReg1].AsInt() };
                break;

            case BCOpcode::Neg: {
                const VMValue& v = regs[inst.srcReg1];
                regs[inst.destReg] = v.IsFloat() ? VMValue{ -v.AsFloat() } : VMValue{ -v.AsInt() };
                break;
            }

            case BCOpcode::Not:
                regs[inst.destReg] = VMValue{ !regs[inst.srcReg1].AsBool() };
                break;

            case BCOpcode::AllocObj: {
                auto obj = std::make_shared<VMObjectInstance>();
                const auto at = inst.symbolImm.find('@');
                obj->className = (at == std::string::npos) ? inst.symbolImm : inst.symbolImm.substr(0, at);
                obj->baseChain = (at == std::string::npos) ? std::string{} : inst.symbolImm.substr(at + 1);
                if (obj->className == "Array") {
                    obj->fields["len"] = VMValue{ inst.intImm };
                }
                regs[inst.destReg] = VMValue{ obj };
                break;
            }

            case BCOpcode::LoadVar:
                regs[inst.destReg] = locals[inst.symbolImm];
                break;

            case BCOpcode::StoreVar:
                locals[inst.symbolImm] = regs[inst.srcReg1];
                if (auto it = cells.find(inst.symbolImm); it != cells.end() && it->second) {
                    *it->second = locals[inst.symbolImm];
                }
                break;

            case BCOpcode::GetField: {
                const VMValue& objVal = regs[inst.srcReg1];
                if (objVal.IsObject()) {
                    const auto& ptr = std::get<std::shared_ptr<VMObjectInstance>>(objVal.data);
                    if (ptr) {
                        regs[inst.destReg] = ptr->fields[inst.symbolImm];
                    }
                }
                break;
            }

            case BCOpcode::SetField: {
                const VMValue& objVal = regs[inst.srcReg1];
                const VMValue& rhsVal = regs[inst.srcReg2];
                if (objVal.IsObject()) {
                    const auto& ptr = std::get<std::shared_ptr<VMObjectInstance>>(objVal.data);
                    if (ptr) {
                        ptr->fields[inst.symbolImm] = rhsVal;
                    }
                }
                break;
            }

            case BCOpcode::CallFn: {
                std::vector<VMValue> callArgs;
                for (std::uint32_t rId : inst.callArgRegs) {
                    callArgs.push_back(regs[rId]);
                }
                VMValue ret = ExecuteFunction(inst.symbolImm, callArgs);
                if (inst.destReg != 0) {
                    regs[inst.destReg] = ret;
                }
                break;
            }

            case BCOpcode::CallVirtual: {
                std::vector<VMValue> callArgs;
                for (std::uint32_t rId : inst.callArgRegs) {
                    callArgs.push_back(regs[rId]);
                }
                std::shared_ptr<VMObjectInstance> obj = callArgs.empty() ? nullptr : AsObj(callArgs[0]);
                if (!obj) {
                    const std::string msg = "robix vm: virtual call '" + inst.symbolImm + "' needs an object";
                    Fail(msg.c_str());
                }
                std::string cursor = obj->className;
                std::string rest = obj->baseChain;
                const BytecodeChunk* found = nullptr;
                for (int guard = 0; guard < 16 && !cursor.empty(); ++guard) {
                    found = m_module.FindChunk(cursor + "." + inst.symbolImm);
                    if (found) break;
                    const auto sep = rest.find('@');
                    if (rest.empty()) break;
                    if (sep == std::string::npos) {
                        cursor = rest;
                        rest.clear();
                    } else {
                        cursor = rest.substr(0, sep);
                        rest = rest.substr(sep + 1);
                    }
                }
                if (!found) {
                    const std::string msg = "robix vm: no implementation of '" + inst.symbolImm + "'";
                    Fail(msg.c_str());
                }
                VMValue ret = ExecuteChunk(*found, callArgs);
                if (inst.destReg != 0) regs[inst.destReg] = ret;
                break;
            }

            case BCOpcode::CallOSIntrinsic: {
                if (inst.symbolImm == "Memory.Allocate" && !inst.callArgRegs.empty()) {
                    const std::int64_t bytes = regs[inst.callArgRegs[0]].AsInt();
                    if (bytes < 0) Fail("robix vm: negative allocation");
                    auto obj = std::make_shared<VMObjectInstance>();
                    obj->className = "Mem";
                    obj->buffer = std::make_shared<VMBuffer>();
                    obj->buffer->bytes.assign(static_cast<std::size_t>(bytes), 0);
                    obj->width = 1;
                    obj->signedPointee = false;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ obj };
                } else if (inst.symbolImm == "Memory.Free" && !inst.callArgRegs.empty()) {
                    auto obj = AsObj(regs[inst.callArgRegs[0]]);
                    if (!obj) Fail("robix vm: Memory.Free requires an allocated pointer");
                    if (obj->buffer) {
                        if (obj->buffer->freed) Fail("robix vm: double free");
                        obj->buffer->freed = true;
                        obj->buffer->bytes.clear();
                    } else if (obj->freed) {
                        Fail("robix vm: double free");
                    } else {
                        obj->freed = true;
                        obj->fields.clear();
                    }
                } else if (inst.symbolImm == "File.WriteText" && inst.callArgRegs.size() >= 2) {
                    const std::string path    = regs[inst.callArgRegs[0]].ToString();
                    const std::string content = regs[inst.callArgRegs[1]].ToString();
                    m_virtualFileSystem[path] = content;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ true };
                } else if (inst.symbolImm == "File.ReadText" && !inst.callArgRegs.empty()) {
                    const std::string path = regs[inst.callArgRegs[0]].ToString();
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ m_virtualFileSystem[path] };
                } else if (inst.symbolImm == "Time.NowMicroseconds") {
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ std::int64_t(1000000) };
                } else if (inst.symbolImm == "String.Length" && !inst.callArgRegs.empty()) {
                    const VMValue& text = regs[inst.callArgRegs[0]];
                    const std::int64_t n = text.IsString()
                        ? static_cast<std::int64_t>(std::get<std::string>(text.data).size())
                        : 0;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ n };
                } else if (inst.symbolImm == "Input" || inst.symbolImm == "Input.Int") {
                    if (!inst.callArgRegs.empty()) {
                        const VMValue& prompt = regs[inst.callArgRegs[0]];
                        if (prompt.IsString()) std::fputs(std::get<std::string>(prompt.data).c_str(), stdout);
                        std::fflush(stdout);
                    }
                    char buffer[4096];
                    std::string line;
                    if (std::fgets(buffer, static_cast<int>(sizeof buffer), stdin) != nullptr) {
                        line = buffer;
                        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
                    }
                    if (inst.destReg != 0) {
                        if (inst.symbolImm == "Input.Int") {
                            regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(std::strtoll(line.c_str(), nullptr, 10)) };
                        } else {
                            regs[inst.destReg] = VMValue{ line };
                        }
                    }
                } else if (inst.symbolImm == "Math.Abs" && !inst.callArgRegs.empty()) {
                    const std::int64_t v = regs[inst.callArgRegs[0]].AsInt();
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ v < 0 ? -v : v };
                } else if ((inst.symbolImm == "Math.Min" || inst.symbolImm == "Math.Max") && inst.callArgRegs.size() >= 2) {
                    const std::int64_t a = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t b = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t v = inst.symbolImm == "Math.Min" ? (a < b ? a : b) : (a > b ? a : b);
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ v };
                } else if (inst.symbolImm.rfind("Port.", 0) == 0 || inst.symbolImm.rfind("Cpu.", 0) == 0 ||
                           inst.symbolImm.rfind("Timer.", 0) == 0 || inst.symbolImm.rfind("System.", 0) == 0 ||
                           inst.symbolImm.rfind("Console.", 0) == 0 ||
                           inst.symbolImm == "Memory.Pages" || inst.symbolImm == "Memory.Address" ||
                           inst.symbolImm == "Memory.At") {
                    Reject("robix vm: " + inst.symbolImm + " runs in a native freestanding image. Compile with robixc --target x86_64-unknown-none.");
                } else if (inst.symbolImm == "PWM.SetDuty" || inst.symbolImm == "UART.Write" ||
                           inst.symbolImm == "SPI.Transfer" || inst.symbolImm == "I2C.Write") {
                    std::int64_t id = 0;
                    if (!inst.callArgRegs.empty()) id = regs[inst.callArgRegs[0]].AsInt();
                    std::fprintf(stderr, "%s is not available on this target (%lld)\n",
                                 inst.symbolImm.c_str(), static_cast<long long>(id));
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ std::int64_t{ 1 } };
                } else if (inst.symbolImm == "GPIO.Read") {
                    std::int64_t pin = 0;
                    if (!inst.callArgRegs.empty()) pin = regs[inst.callArgRegs[0]].AsInt();
                    std::fprintf(stderr, "GPIO.Read is not available on this target (pin %lld)\n",
                                 static_cast<long long>(pin));
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ std::int64_t{ 1 } };
                } else if (inst.symbolImm == "Atomic.FetchAdd" && inst.callArgRegs.size() >= 2) {
                    auto obj = AsObj(regs[inst.callArgRegs[0]]);
                    if (!obj) Fail("robix vm: Atomic.FetchAdd requires a pointer");
                    const std::int64_t old = ReadPointee(*obj);
                    WritePointee(*obj, old + regs[inst.callArgRegs[1]].AsInt());
                    if (obj->cell) {
                        for (auto& entry : cells) {
                            if (entry.second == obj->cell) locals[entry.first] = *obj->cell;
                        }
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ old };
                } else if (inst.symbolImm == "Thread.AddPair" && inst.callArgRegs.size() >= 3) {
                    auto obj = AsObj(regs[inst.callArgRegs[0]]);
                    if (!obj) Fail("robix vm: Thread.AddPair requires a pointer");
                    const std::int64_t sum = ReadPointee(*obj) + regs[inst.callArgRegs[1]].AsInt() +
                                             regs[inst.callArgRegs[2]].AsInt();
                    WritePointee(*obj, sum);
                    if (obj->cell) {
                        for (auto& entry : cells) {
                            if (entry.second == obj->cell) locals[entry.first] = *obj->cell;
                        }
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ sum };
                } else if (inst.symbolImm == "Math.Clamp" && inst.callArgRegs.size() >= 3) {
                    std::int64_t v = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t lo = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t hi = regs[inst.callArgRegs[2]].AsInt();
                    if (v < lo) v = lo;
                    if (v > hi) v = hi;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ v };
                } else {
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ std::int64_t(0) };
                }
                break;
            }

            case BCOpcode::PrintReg: {
                m_stdoutBuffer += regs[inst.srcReg1].ToString() + "\n";
                break;
            }

            case BCOpcode::AssertReg: {
                bool ok = false;
                if ((inst.symbolImm == "AssertEqual" || inst.symbolImm == "Assert.Equal") && inst.callArgRegs.size() >= 2) {
                    ok = (regs[inst.callArgRegs[0]].ToString() == regs[inst.callArgRegs[1]].ToString());
                } else if (inst.symbolImm == "Assert.NotEqual" && inst.callArgRegs.size() >= 2) {
                    ok = (regs[inst.callArgRegs[0]].ToString() != regs[inst.callArgRegs[1]].ToString());
                } else if (inst.symbolImm == "AssertFalse" || inst.symbolImm == "Assert.False") {
                    ok = !inst.callArgRegs.empty() && !regs[inst.callArgRegs[0]].AsBool();
                } else if (inst.symbolImm == "Assert.Null") {
                    ok = !inst.callArgRegs.empty() &&
                         (std::holds_alternative<std::monostate>(regs[inst.callArgRegs[0]].data) ||
                          regs[inst.callArgRegs[0]].ToString() == "null");
                } else if (inst.symbolImm == "Assert.NotNull") {
                    ok = !inst.callArgRegs.empty() &&
                         !std::holds_alternative<std::monostate>(regs[inst.callArgRegs[0]].data) &&
                         regs[inst.callArgRegs[0]].ToString() != "null";
                } else if (inst.symbolImm == "Assert.Contains" && inst.callArgRegs.size() >= 2) {
                    const std::string haystack = regs[inst.callArgRegs[0]].ToString();
                    const std::string needle   = regs[inst.callArgRegs[1]].ToString();
                    ok = (haystack.find(needle) != std::string::npos);
                } else if (inst.symbolImm == "Assert.Success") {
                    ok = !inst.callArgRegs.empty() &&
                         (regs[inst.callArgRegs[0]].AsBool() ||
                          regs[inst.callArgRegs[0]].ToString().find("Error") == std::string::npos);
                } else if (inst.symbolImm == "Assert.Error" ||
                           inst.symbolImm == "Assert.Throws" ||
                           inst.symbolImm == "Assert.ThrowsAsync") {
                    ok = !inst.callArgRegs.empty() &&
                         (!regs[inst.callArgRegs[0]].AsBool() ||
                          regs[inst.callArgRegs[0]].ToString().find("Error") != std::string::npos);
                } else if (inst.symbolImm == "Assert.Timeout") {
                    ok = !inst.callArgRegs.empty() && (regs[inst.callArgRegs[0]].AsInt() > 0);
                } else if (!inst.callArgRegs.empty()) {
                    ok = regs[inst.callArgRegs[0]].AsBool();
                }
                if (ok) {
                    ++m_assertPassCount;
                } else {
                    ++m_assertFailCount;
                }
                break;
            }

            case BCOpcode::Jump:
                pc = inst.targetPC;
                break;

            case BCOpcode::JumpIfFalse:
                if (!regs[inst.srcReg1].AsBool()) {
                    pc = inst.targetPC;
                }
                break;

            case BCOpcode::ReturnReg:
                return regs[inst.srcReg1];

            case BCOpcode::ReturnVoid:
                return VMValue{};

            case BCOpcode::LoadIndex: {
                auto obj = AsObj(regs[inst.srcReg1]);
                if (!obj) Fail("robix vm: index of a non-array");
                const std::int64_t index = regs[inst.srcReg2].AsInt();
                const std::int64_t len = obj->fields["len"].AsInt();
                if (index < 0 || index >= len) Fail("robix vm: index out of bounds");
                regs[inst.destReg] = obj->fields[std::to_string(index)];
                break;
            }

            case BCOpcode::StoreIndex: {
                if (inst.callArgRegs.size() < 3) Fail("robix vm: malformed store index");
                auto obj = AsObj(regs[inst.callArgRegs[0]]);
                if (!obj) Fail("robix vm: index of a non-array");
                const std::int64_t index = regs[inst.callArgRegs[1]].AsInt();
                const std::int64_t len = obj->fields["len"].AsInt();
                if (index < 0 || index >= len) Fail("robix vm: index out of bounds");
                obj->fields[std::to_string(index)] = regs[inst.callArgRegs[2]];
                break;
            }

            case BCOpcode::AddrOf: {
                auto obj = std::make_shared<VMObjectInstance>();
                obj->className = "Ptr";
                if (!cells[inst.symbolImm]) {
                    cells[inst.symbolImm] = std::make_shared<VMValue>(locals[inst.symbolImm]);
                }
                obj->cell = cells[inst.symbolImm];
                obj->width = inst.intImm <= 0 ? 8 : static_cast<int>(inst.intImm);
                obj->signedPointee = inst.boolImm;
                regs[inst.destReg] = VMValue{ obj };
                break;
            }

            case BCOpcode::PtrLoad: {
                auto obj = AsObj(regs[inst.srcReg1]);
                if (!obj) Fail("robix vm: dereference of a non-pointer");
                obj->width = inst.intImm <= 0 ? obj->width : static_cast<int>(inst.intImm);
                obj->signedPointee = inst.boolImm;
                regs[inst.destReg] = VMValue{ ReadPointee(*obj) };
                break;
            }

            case BCOpcode::PtrStore: {
                auto obj = AsObj(regs[inst.srcReg1]);
                if (!obj) Fail("robix vm: dereference of a non-pointer");
                obj->width = inst.intImm <= 0 ? obj->width : static_cast<int>(inst.intImm);
                obj->signedPointee = inst.boolImm;
                WritePointee(*obj, regs[inst.srcReg2].AsInt());
                if (obj->cell) {
                    for (auto& entry : cells) {
                        if (entry.second == obj->cell) locals[entry.first] = *obj->cell;
                    }
                }
                break;
            }

            case BCOpcode::PtrOffset: {
                auto obj = AsObj(regs[inst.srcReg1]);
                if (!obj) Fail("robix vm: pointer arithmetic on a non-pointer");
                const std::int64_t scale = inst.intImm <= 0 ? 1 : inst.intImm;
                const std::int64_t delta = regs[inst.srcReg2].AsInt() * scale;
                if (obj->cell && delta != 0) Fail("robix vm: cannot offset the address of a local");
                auto copy = std::make_shared<VMObjectInstance>(*obj);
                copy->index += delta;
                copy->width = static_cast<int>(scale);
                copy->signedPointee = inst.boolImm;
                regs[inst.destReg] = VMValue{ copy };
                break;
            }

            case BCOpcode::SliceCopy: {
                if (inst.callArgRegs.size() < 3) Fail("robix vm: malformed slice");
                auto src = AsObj(regs[inst.callArgRegs[0]]);
                if (!src) Fail("robix vm: slice of a non-array");
                std::int64_t start = regs[inst.callArgRegs[1]].AsInt();
                std::int64_t end = regs[inst.callArgRegs[2]].AsInt();
                if (inst.intImm != 0) end += 1;
                const std::int64_t len = src->fields["len"].AsInt();
                if (start < 0 || end < start || end > len) Fail("robix vm: slice out of bounds");
                auto dst = std::make_shared<VMObjectInstance>();
                dst->className = "Array";
                dst->fields["len"] = VMValue{ end - start };
                for (std::int64_t i = 0; i < end - start; ++i) {
                    dst->fields[std::to_string(i)] = src->fields[std::to_string(start + i)];
                }
                regs[inst.destReg] = VMValue{ dst };
                break;
            }
        }
    }

    return VMValue{};
}

} // namespace Robix::VM

