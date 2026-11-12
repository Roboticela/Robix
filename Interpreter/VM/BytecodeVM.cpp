#include "BytecodeVM.hpp"
#include "../../Compiler/Types/SpecValidator.hpp"
#include "../../Compiler/Runtime/SceneRaster.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#endif

extern "C" char* rx_network_call(const char* service, const char* method, const char* text);
extern "C" char* rx_env_get(const char* name);
extern "C" std::int64_t rx_env_exists(const char* name);
extern "C" std::int64_t rx_env_set(const char* name, const char* value);
extern "C" std::int64_t rx_env_add(const char* name, const char* value);
extern "C" std::int64_t rx_env_remove(const char* name);

namespace Robix::VM {

namespace {

[[noreturn]] void Fail(const char* message) {
    std::fputs(message, stderr);
    std::fputc('\n', stderr);
    std::abort();
}

std::FILE* OpenFile(const char* path, const char* mode) {
    std::FILE* file = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&file, path, mode) != 0) return nullptr;
#else
    file = std::fopen(path, mode);
#endif
    return file;
}

std::shared_ptr<VMObjectInstance> AsObj(const VMValue& value) {
    if (!value.IsObject()) return nullptr;
    return std::get<std::shared_ptr<VMObjectInstance>>(value.data);
}

void WriteHostPicture(const VMObjectInstance& frame) {
    if (!frame.buffer || frame.width <= 0 || frame.index <= 0) return;
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    fs::path out = cwd.filename() == "RoboticelaOS"
                       ? cwd.parent_path() / "Build" / "RoboticelaOS" / "Interpreter.bmp"
                       : cwd / "Build" / "RoboticelaOS" / "Interpreter.bmp";
    fs::create_directories(out.parent_path(), ec);
    const int width = frame.width;
    const int height = static_cast<int>(frame.index);
    const int rowBytes = (width * 3 + 3) & ~3;
    const int pixels = rowBytes * height;
    const int fileSize = 54 + pixels;
    std::vector<std::uint8_t> bmp(static_cast<std::size_t>(fileSize), 0);
    auto put32 = [&](int at, int value) {
        bmp[static_cast<std::size_t>(at)] = static_cast<std::uint8_t>(value);
        bmp[static_cast<std::size_t>(at + 1)] = static_cast<std::uint8_t>(value >> 8);
        bmp[static_cast<std::size_t>(at + 2)] = static_cast<std::uint8_t>(value >> 16);
        bmp[static_cast<std::size_t>(at + 3)] = static_cast<std::uint8_t>(value >> 24);
    };
    bmp[0] = 'B';
    bmp[1] = 'M';
    put32(2, fileSize);
    put32(10, 54);
    put32(14, 40);
    put32(18, width);
    put32(22, height);
    bmp[26] = 1;
    bmp[28] = 24;
    const std::size_t need = static_cast<std::size_t>(width * height * 4);
    if (frame.buffer->bytes.size() < need) return;
    for (int y = 0; y < height; ++y) {
        const int sourceY = height - 1 - y;
        for (int x = 0; x < width; ++x) {
            const std::size_t at = static_cast<std::size_t>((sourceY * width + x) * 4);
            const std::size_t dst = static_cast<std::size_t>(54 + y * rowBytes + x * 3);
            bmp[dst] = frame.buffer->bytes[at];
            bmp[dst + 1] = frame.buffer->bytes[at + 1];
            bmp[dst + 2] = frame.buffer->bytes[at + 2];
        }
    }
    std::FILE* file = OpenFile(out.string().c_str(), "wb");
    if (!file) return;
    std::fwrite(bmp.data(), 1, bmp.size(), file);
    std::fclose(file);
    std::fputs("robix: picture ", stderr);
    std::fputs(out.string().c_str(), stderr);
    std::fputc('\n', stderr);
#ifdef _WIN32
    const std::wstring wide = out.wstring();
    ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
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
static std::vector<std::string> g_programArguments;

void BytecodeVM::SetLineTrace(std::vector<std::uint32_t>* lines) {
    g_lineTrace = lines;
}

void BytecodeVM::SetProgramArguments(const std::vector<std::string>& args) {
    g_programArguments = args;
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
    if (!mainChunk) mainChunk = m_module.FindChunk("Start");
    if (!mainChunk) {
        return 1;
    }
    m_hostPicture = true;
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
    if (!Gate() || !m_fault.empty() || m_halt) return VMValue{};
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
        if (!m_fault.empty() || m_halt) return VMValue{};
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
                } else if (inst.symbolImm == "Frame.Create" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t width = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t height = regs[inst.callArgRegs[1]].AsInt();
                    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
                        Fail("robix vm: Frame.Create size is out of range");
                    }
                    auto obj = std::make_shared<VMObjectInstance>();
                    obj->className = "Frame";
                    obj->width = static_cast<int>(width);
                    obj->index = height;
                    obj->buffer = std::make_shared<VMBuffer>();
                    obj->buffer->bytes.assign(static_cast<std::size_t>(width * height * 4), 0);
                    m_gpuFrame = obj;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ obj };
                } else if (inst.symbolImm == "Frame.Fill" && inst.callArgRegs.size() >= 6) {
                    auto obj = AsObj(regs[inst.callArgRegs[0]]);
                    if (!obj || !obj->buffer || obj->className != "Frame") Fail("robix vm: Frame.Fill needs a frame");
                    const std::int64_t x = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t y = regs[inst.callArgRegs[2]].AsInt();
                    const std::int64_t rw = regs[inst.callArgRegs[3]].AsInt();
                    const std::int64_t rh = regs[inst.callArgRegs[4]].AsInt();
                    const std::uint32_t color = static_cast<std::uint32_t>(regs[inst.callArgRegs[5]].AsInt());
                    const std::int64_t fw = obj->width;
                    const std::int64_t fh = obj->index;
                    for (std::int64_t row = 0; row < rh; ++row) {
                        for (std::int64_t col = 0; col < rw; ++col) {
                            const std::int64_t px = x + col;
                            const std::int64_t py = y + row;
                            if (px < 0 || py < 0 || px >= fw || py >= fh) continue;
                            const std::size_t at = static_cast<std::size_t>((py * fw + px) * 4);
                            obj->buffer->bytes[at] = static_cast<std::uint8_t>(color);
                            obj->buffer->bytes[at + 1] = static_cast<std::uint8_t>(color >> 8);
                            obj->buffer->bytes[at + 2] = static_cast<std::uint8_t>(color >> 16);
                            obj->buffer->bytes[at + 3] = static_cast<std::uint8_t>(color >> 24);
                        }
                    }
                } else if (inst.symbolImm == "Frame.Pixel" && inst.callArgRegs.size() >= 3) {
                    auto obj = AsObj(regs[inst.callArgRegs[0]]);
                    if (!obj || !obj->buffer || obj->className != "Frame") Fail("robix vm: Frame.Pixel needs a frame");
                    const std::int64_t x = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t y = regs[inst.callArgRegs[2]].AsInt();
                    std::int64_t value = 0;
                    if (x >= 0 && y >= 0 && x < obj->width && y < obj->index) {
                        const std::size_t at = static_cast<std::size_t>((y * obj->width + x) * 4);
                        value = static_cast<std::uint32_t>(obj->buffer->bytes[at]) |
                                (static_cast<std::uint32_t>(obj->buffer->bytes[at + 1]) << 8) |
                                (static_cast<std::uint32_t>(obj->buffer->bytes[at + 2]) << 16) |
                                (static_cast<std::uint32_t>(obj->buffer->bytes[at + 3]) << 24);
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ value };
                } else if (inst.symbolImm == "Frame.Present") {
                    auto obj = AsObj(regs[inst.callArgRegs.empty() ? 0 : inst.callArgRegs[0]]);
                    std::int64_t shown = 0;
                    if (obj && obj->buffer && !obj->buffer->bytes.empty()) {
                        shown = obj->buffer->bytes[0];
                        if (m_hostPicture) WriteHostPicture(*obj);
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ shown };
                } else if (inst.symbolImm == "Frame.Flush") {
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(0) };
                } else if (inst.symbolImm == "Page.Entry" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t phys = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t flags = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t entry = (phys & ~static_cast<std::int64_t>(0xFFF)) | flags;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ entry };
                } else if (inst.symbolImm == "Page.Split" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t root = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t userStart = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t userFlags = userStart > 0 ? 7 : 3;
                    const std::int64_t split = (root & ~static_cast<std::int64_t>(0xFFF)) | 3 | (userFlags << 8);
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ split };
                } else if (inst.symbolImm == "Fault.Decide" && !inst.callArgRegs.empty()) {
                    const std::int64_t fromUser = regs[inst.callArgRegs[0]].AsInt();
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ fromUser != 0 ? std::int64_t{ 1 } : std::int64_t{ 0 } };
                } else if (inst.symbolImm == "Memory.Frame") {
                    std::int64_t frame = 0;
                    if (m_frames < 8) {
                        frame = 2097152 + m_frames * 4096;
                        ++m_frames;
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ frame };
                } else if (inst.symbolImm == "Memory.At" && !inst.callArgRegs.empty()) {
                    auto obj = std::make_shared<VMObjectInstance>();
                    obj->className = "Addr";
                    obj->fields["addr"] = VMValue{ regs[inst.callArgRegs[0]].AsInt() };
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ obj };
                } else if (inst.symbolImm == "Volatile.Load" && !inst.callArgRegs.empty()) {
                    std::int64_t value = 0;
                    auto obj = AsObj(regs[inst.callArgRegs[0]]);
                    if (obj) {
                        const auto at = obj->fields.find("addr");
                        if (at != obj->fields.end()) {
                            const auto stored = m_mmio.find(at->second.AsInt());
                            if (stored != m_mmio.end()) value = stored->second;
                        }
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ value };
                } else if (inst.symbolImm == "Volatile.Store" && inst.callArgRegs.size() >= 2) {
                    auto obj = AsObj(regs[inst.callArgRegs[0]]);
                    if (obj) {
                        const auto at = obj->fields.find("addr");
                        if (at != obj->fields.end()) m_mmio[at->second.AsInt()] = regs[inst.callArgRegs[1]].AsInt();
                    }
                } else if (inst.symbolImm == "Volatile.Load32" && !inst.callArgRegs.empty()) {
                    std::int64_t value = 0;
                    auto obj = AsObj(regs[inst.callArgRegs[0]]);
                    if (obj && (obj->cell || obj->buffer)) {
                        value = ReadPointee(*obj) & 0xFFFFFFFFLL;
                    } else if (obj) {
                        const auto at = obj->fields.find("addr");
                        if (at != obj->fields.end()) {
                            const auto stored = m_mmio.find(at->second.AsInt());
                            if (stored != m_mmio.end()) value = stored->second & 0xFFFFFFFFLL;
                        }
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ value };
                } else if (inst.symbolImm == "Volatile.Store32" && inst.callArgRegs.size() >= 2) {
                    auto obj = AsObj(regs[inst.callArgRegs[0]]);
                    if (obj && (obj->cell || obj->buffer)) {
                        WritePointee(*obj, regs[inst.callArgRegs[1]].AsInt() & 0xFFFFFFFFLL);
                        if (obj->cell) {
                            for (auto& entry : cells) {
                                if (entry.second == obj->cell) locals[entry.first] = *obj->cell;
                            }
                        }
                    } else if (obj) {
                        const auto at = obj->fields.find("addr");
                        if (at != obj->fields.end()) {
                            m_mmio[at->second.AsInt()] = regs[inst.callArgRegs[1]].AsInt() & 0xFFFFFFFFLL;
                        }
                    }
                } else if (inst.symbolImm == "CPU.Gate" && !inst.callArgRegs.empty()) {
                    const std::int64_t handler = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t low = (handler & 65535) | (8LL << 16) | (142LL << 40) |
                                             (((handler >> 16) & 65535) << 48);
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ low };
                } else if (inst.symbolImm == "CPU.GateHigh" && !inst.callArgRegs.empty()) {
                    const std::int64_t handler = regs[inst.callArgRegs[0]].AsInt();
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ handler >> 32 };
                } else if (inst.symbolImm == "Proc.Create" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t parent = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t user = regs[inst.callArgRegs[1]].AsInt();
                    bool parentOk = parent == 0;
                    if (parent >= 1 && parent <= 8) parentOk = m_procState[parent - 1] == 1;
                    int slot = -1;
                    if (parentOk) {
                        for (int i = 0; i < 8; ++i) {
                            if (m_procState[i] == 0) { slot = i; break; }
                        }
                    }
                    std::int64_t ident = 0;
                    if (slot >= 0) {
                        m_procParent[slot] = parent;
                        m_procUser[slot] = user;
                        m_procState[slot] = 1;
                        m_procStatus[slot] = 0;
                        ident = slot + 1;
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ ident };
                } else if (inst.symbolImm == "Proc.Exit" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t ident = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t status = regs[inst.callArgRegs[1]].AsInt();
                    std::int64_t result = 0;
                    if (ident >= 1 && ident <= 8 && m_procState[ident - 1] == 1) {
                        m_procState[ident - 1] = 2;
                        m_procStatus[ident - 1] = status;
                        result = 1;
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "Proc.Wait" && !inst.callArgRegs.empty()) {
                    const std::int64_t ident = regs[inst.callArgRegs[0]].AsInt();
                    std::int64_t result = -1;
                    if (ident >= 1 && ident <= 8) {
                        if (m_procState[ident - 1] == 2) result = m_procStatus[ident - 1];
                        else if (m_procState[ident - 1] == 1) result = -2;
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "Cred.Allow" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t user = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t right = regs[inst.callArgRegs[1]].AsInt();
                    const bool known = user >= 0 && user < 2;
                    const bool guest = user == 2;
                    std::int64_t allow = 0;
                    if (right == 1 && user == 0) allow = 1;
                    else if (right == 2 && known) allow = 1;
                    else if (right == 4 && (known || guest)) allow = 1;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ allow };
                } else if (inst.symbolImm == "Syscall.Dispatch" && inst.callArgRegs.size() >= 3) {
                    const std::int64_t caller = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t number = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t argument = regs[inst.callArgRegs[2]].AsInt();
                    std::int64_t result = -1;
                    if (number == 0) result = argument + 4919;
                    else if (number == 1 || number == 2 || number == 4) result = argument;
                    else if (number == 3) result = 0;
                    else if (number == 5 && argument >= 8388608 && argument < 10485760) result = argument;
                    else if (number == 6) {
                        result = -1;
                        for (int i = 0; i < 4; ++i) {
                            if (m_fsState[i] == 1 && m_fsPath[i] == argument) result = 1;
                        }
                    }
                    else if (number == 7 && caller == 2) result = argument;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "FS.Lookup" && !inst.callArgRegs.empty()) {
                    const std::int64_t path = regs[inst.callArgRegs[0]].AsInt();
                    std::int64_t result = -1;
                    for (int i = 0; i < 4; ++i) {
                        if (m_fsState[i] == 1 && m_fsPath[i] == path) result = 1;
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "FS.Create" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t path = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t value = regs[inst.callArgRegs[1]].AsInt();
                    std::int64_t result = -1;
                    if (path > 0) {
                        int hole = -1;
                        for (int i = 0; i < 4; ++i) {
                            if (m_fsState[i] == 1 && m_fsPath[i] == path) {
                                m_fsValue[i] = value;
                                result = 1;
                                hole = -2;
                                break;
                            }
                            if (m_fsState[i] == 0 && hole < 0) hole = i;
                        }
                        if (result != 1 && hole >= 0) {
                            m_fsPath[hole] = path;
                            m_fsValue[hole] = value;
                            m_fsState[hole] = 1;
                            result = 1;
                        }
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "FS.Read" && !inst.callArgRegs.empty()) {
                    const std::int64_t path = regs[inst.callArgRegs[0]].AsInt();
                    std::int64_t result = -1;
                    for (int i = 0; i < 4; ++i) {
                        if (m_fsState[i] == 1 && m_fsPath[i] == path) result = m_fsValue[i];
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "FS.Write" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t path = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t value = regs[inst.callArgRegs[1]].AsInt();
                    std::int64_t result = -1;
                    for (int i = 0; i < 4; ++i) {
                        if (m_fsState[i] == 1 && m_fsPath[i] == path) {
                            m_fsValue[i] = value;
                            result = 1;
                        }
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "FS.List" && !inst.callArgRegs.empty()) {
                    const std::int64_t index = regs[inst.callArgRegs[0]].AsInt();
                    std::int64_t result = 0;
                    if (index >= 0 && index < 4 && m_fsState[index] == 1) result = m_fsPath[index];
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "Port.Create" && !inst.callArgRegs.empty()) {
                    const std::int64_t owner = regs[inst.callArgRegs[0]].AsInt();
                    int slot = -1;
                    for (int i = 0; i < 8; ++i) {
                        if (m_portState[i] == 0) { slot = i; break; }
                    }
                    std::int64_t ident = 0;
                    if (slot >= 0) {
                        m_portOwner[slot] = owner;
                        m_portState[slot] = owner == 0 ? 2 : 1;
                        m_portWord[slot] = 0;
                        m_portFull[slot] = 0;
                        ident = slot + 1;
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ ident };
                } else if (inst.symbolImm == "Port.Send" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t ident = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t word = regs[inst.callArgRegs[1]].AsInt() & 4294967295LL;
                    std::int64_t result = -1;
                    if (ident >= 1 && ident <= 8) {
                        const int slot = static_cast<int>(ident - 1);
                        if (m_portState[slot] == 1) {
                            if (m_portFull[slot] != 0) result = -2;
                            else {
                                m_portWord[slot] = word;
                                m_portFull[slot] = 1;
                                result = 4;
                            }
                        }
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "Port.Receive" && !inst.callArgRegs.empty()) {
                    const std::int64_t ident = regs[inst.callArgRegs[0]].AsInt();
                    std::int64_t result = -1;
                    if (ident >= 1 && ident <= 8) {
                        const int slot = static_cast<int>(ident - 1);
                        if (m_portState[slot] == 1) {
                            if (m_portFull[slot] == 0) result = -2;
                            else {
                                result = m_portWord[slot];
                                m_portFull[slot] = 0;
                            }
                        }
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "Sched.Enqueue" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t ident = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t pri = regs[inst.callArgRegs[1]].AsInt();
                    int slot = -1;
                    for (int i = 0; i < 8; ++i) {
                        if (m_schedState[i] != 0 && m_schedId[i] == ident) slot = i;
                    }
                    if (slot >= 0) {
                        m_schedPri[slot] = pri;
                        m_schedState[slot] = 1;
                    } else {
                        for (int i = 0; i < 8; ++i) {
                            if (m_schedState[i] == 0) { slot = i; break; }
                        }
                        if (slot >= 0) {
                            m_schedId[slot] = ident;
                            m_schedPri[slot] = pri;
                            m_schedState[slot] = 1;
                        }
                    }
                    std::int64_t ready = 0;
                    for (int i = 0; i < 8; ++i) if (m_schedState[i] == 1) ++ready;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ ready };
                } else if (inst.symbolImm == "Sched.Pick" && !inst.callArgRegs.empty()) {
                    int best = -1;
                    std::int64_t bestPri = -1;
                    for (int i = 0; i < 8; ++i) {
                        if (m_schedState[i] == 1 && m_schedPri[i] > bestPri) {
                            best = i;
                            bestPri = m_schedPri[i];
                        }
                    }
                    std::int64_t ident = 0;
                    if (best >= 0) {
                        m_schedState[best] = 3;
                        ident = m_schedId[best];
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ ident };
                } else if (inst.symbolImm == "Sched.Block" && !inst.callArgRegs.empty()) {
                    const std::int64_t ident = regs[inst.callArgRegs[0]].AsInt();
                    std::int64_t found = 0;
                    for (int i = 0; i < 8; ++i) {
                        if (m_schedState[i] != 0 && m_schedId[i] == ident) {
                            m_schedState[i] = 2;
                            found = 1;
                        }
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ found };
                } else if (inst.symbolImm == "Clock.Seconds" && !inst.callArgRegs.empty()) {
                    const std::int64_t cmos = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t seconds = ((cmos >> 4) & 15) * 10 + (cmos & 15);
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ seconds };
                } else if (inst.symbolImm == "CPU.TSC") {
                    m_tsc += 1;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ m_tsc };
                } else if (inst.symbolImm == "PCI.Decode" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t word0 = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t word8 = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t vendor = word0 & 65535;
                    std::int64_t result = -1;
                    if (vendor != 65535) {
                        const std::int64_t device = (word0 >> 16) & 65535;
                        const std::int64_t klass = (word8 >> 24) & 255;
                        result = vendor | (device << 16) | (klass << 32);
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "HID.Parse" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t kind = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t report = regs[inst.callArgRegs[1]].AsInt();
                    std::int64_t result = -1;
                    if (kind == 0 || kind == 2) result = report & 16777215;
                    else if (kind == 1) {
                        std::int64_t wheel = (report >> 24) & 255;
                        if (wheel >= 128) wheel -= 256;
                        result = wheel;
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "Driver.Match" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t device = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t personalities = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t count = personalities & 255;
                    std::int64_t result = -1;
                    if (count == 1) {
                        const std::int64_t deviceClass = (device >> 32) & 255;
                        const std::int64_t personalityClass = (personalities >> 16) & 255;
                        if (deviceClass == personalityClass) result = (personalities >> 8) & 255;
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "Driver.Bind" && inst.callArgRegs.size() >= 3) {
                    const std::int64_t device = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t builtIn = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t installed = regs[inst.callArgRegs[2]].AsInt();
                    const std::int64_t vendor = device & 65535;
                    const std::int64_t deviceId = (device >> 16) & 65535;
                    const std::int64_t packageId = installed & 65535;
                    const std::int64_t packageVendor = (installed >> 16) & 65535;
                    const std::int64_t packageDevice = (installed >> 32) & 65535;
                    std::int64_t result = -1;
                    if (installed != 0 && packageId != 0 && packageVendor == vendor && packageDevice == deviceId) {
                        result = packageId;
                    } else if (builtIn != 0) {
                        result = builtIn;
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "Driver.Package" && !inst.callArgRegs.empty()) {
                    const std::int64_t bytes = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t magic = (bytes >> 48) & 65535;
                    const std::int64_t version = (bytes >> 32) & 255;
                    const std::int64_t hmac = (bytes >> 40) & 255;
                    const std::int64_t vendor = (bytes >> 16) & 65535;
                    const std::int64_t device = bytes & 65535;
                    std::int64_t result = -1;
                    if (magic == 21072) {
                        if (version == 0) {
                            result = m_pkgPrevious;
                            m_pkgActive = m_pkgPrevious;
                            m_pkgPrevious = 0;
                        } else {
                            const std::int64_t expect = (21072 ^ version ^ vendor ^ device) & 255;
                            if (hmac == expect && (m_pkgActive == 0 || version > m_pkgActive)) {
                                m_pkgPrevious = m_pkgActive;
                                m_pkgActive = version;
                                result = (m_pkgPrevious << 16) | version;
                            }
                        }
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "Driver.Newer" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t current = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t offered = regs[inst.callArgRegs[1]].AsInt();
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ offered > current ? std::int64_t{ 1 } : std::int64_t{ 0 } };
                } else if (inst.symbolImm == "Vendor.Match" && inst.callArgRegs.size() >= 4) {
                    const std::int64_t catalog = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t vendor = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t device = regs[inst.callArgRegs[2]].AsInt();
                    std::int64_t result = 0;
                    if (catalog == 1 && vendor == 32902) {
                        if (device == 22802) result = 1;
                        else if (device == 40304) result = 2;
                        else if (device == 4110 || device == 4307 || device == 5618 || device == 4297 || device == 4347 || device == 5490) result = 3;
                        else if (device == 10019 || device == 17392) result = 4;
                        else if (device == 2602 || device == 752) result = 5;
                        else if (device == 40288) result = 6;
                        else if (device == 40239 || device == 39443 || device == 5586) result = 7;
                        else if (device == 39435 || device == 10274) result = 8;
                        else if (device == 6425) result = 9;
                        else if (device == 40224 || device == 6403 || device == 40227) result = 10;
                    } else if (catalog == 2 && (vendor == 4130 || vendor == 4098)) result = 1;
                    else if (catalog == 3 && vendor == 4318) result = 1;
                    else if (catalog == 4 && vendor == 4332) result = 1;
                    else if (catalog == 5 && vendor == 5348) result = 1;
                    else if (catalog == 6 && vendor == 6091) result = 1;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "GPU.Command" && inst.callArgRegs.size() >= 3) {
                    std::int64_t result = -1;
                    if (m_gpuCount < 4) {
                        const std::size_t slot = static_cast<std::size_t>(m_gpuCount);
                        m_gpuOp[slot] = regs[inst.callArgRegs[0]].AsInt();
                        m_gpuRect[slot] = regs[inst.callArgRegs[1]].AsInt();
                        m_gpuColor[slot] = regs[inst.callArgRegs[2]].AsInt();
                        ++m_gpuCount;
                        result = m_gpuCount;
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "GPU.Submit" && !inst.callArgRegs.empty()) {
                    const std::int64_t buffer = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t caller = (buffer >> 16) & 65535;
                    const std::int64_t count = buffer & 255;
                    std::int64_t result = -1;
                    if (caller == 2 && count == m_gpuCount && count > 0) {
                        for (int i = 0; i < 64; ++i) m_gpuPix[i] = 0;
                        auto plot = [&](std::int64_t x, std::int64_t y, std::int64_t color) {
                            if (x < 0 || y < 0 || x >= 8 || y >= 8) return;
                            m_gpuPix[y * 8 + x] = static_cast<std::int32_t>(color);
                            if (!m_gpuFrame || m_gpuFrame->index > 8 || !m_gpuFrame->buffer) return;
                            if (x >= m_gpuFrame->width || y >= m_gpuFrame->index) return;
                            const std::size_t at = static_cast<std::size_t>((y * m_gpuFrame->width + x) * 4);
                            const auto c = static_cast<std::uint32_t>(color);
                            m_gpuFrame->buffer->bytes[at] = static_cast<std::uint8_t>(c);
                            m_gpuFrame->buffer->bytes[at + 1] = static_cast<std::uint8_t>(c >> 8);
                            m_gpuFrame->buffer->bytes[at + 2] = static_cast<std::uint8_t>(c >> 16);
                            m_gpuFrame->buffer->bytes[at + 3] = static_cast<std::uint8_t>(c >> 24);
                        };
                        auto read = [&](std::int64_t x, std::int64_t y) -> std::int64_t {
                            if (x < 0 || y < 0 || x >= 8 || y >= 8) return 0;
                            return static_cast<std::uint32_t>(m_gpuPix[y * 8 + x]);
                        };
                        for (std::int64_t cmd = 0; cmd < count; ++cmd) {
                            const std::size_t slot = static_cast<std::size_t>(cmd);
                            const std::int64_t op = m_gpuOp[slot];
                            const std::int64_t rect = m_gpuRect[slot];
                            const std::int64_t color = m_gpuColor[slot];
                            const std::int64_t x = rect & 255;
                            const std::int64_t y = (rect >> 8) & 255;
                            const std::int64_t w = (rect >> 16) & 255;
                            const std::int64_t h = (rect >> 24) & 255;
                            if (op == 1) {
                                for (std::int64_t row = 0; row < h; ++row) {
                                    for (std::int64_t col = 0; col < w; ++col) plot(x + col, y + row, color);
                                }
                            } else if (op == 2) {
                                const std::int64_t sx = color & 255;
                                const std::int64_t sy = (color >> 8) & 255;
                                for (std::int64_t row = 0; row < h; ++row) {
                                    for (std::int64_t col = 0; col < w; ++col) {
                                        plot(x + col, y + row, read(sx + col, sy + row));
                                    }
                                }
                            }
                        }
                        result = static_cast<std::uint32_t>(m_gpuPix[0]);
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "Launch.Plan" && !inst.callArgRegs.empty()) {
                    const std::int64_t word = regs[inst.callArgRegs[0]].AsInt();
                    std::int64_t ident[4]{};
                    std::int64_t after[4]{};
                    int used[4]{};
                    int count = 0;
                    bool cycle = false;
                    for (int slot = 0; slot < 4; ++slot) {
                        const std::int64_t piece = (word >> (slot * 16)) & 65535;
                        ident[slot] = piece & 255;
                        after[slot] = (piece >> 8) & 255;
                        if (ident[slot] != 0) {
                            ++count;
                            if (after[slot] == ident[slot]) cycle = true;
                        }
                    }
                    std::int64_t order[4]{};
                    int placed = 0;
                    while (!cycle && placed < count) {
                        int choice = -1;
                        for (int slot = 0; slot < 4; ++slot) {
                            if (ident[slot] == 0 || used[slot] != 0) continue;
                            if (after[slot] == 0) {
                                choice = slot;
                                break;
                            }
                            for (int prior = 0; prior < 4; ++prior) {
                                if (used[prior] != 0 && ident[prior] == after[slot]) choice = slot;
                            }
                            if (choice >= 0) break;
                        }
                        if (choice < 0) {
                            cycle = true;
                            break;
                        }
                        used[choice] = 1;
                        order[placed] = ident[choice];
                        ++placed;
                    }
                    std::int64_t result = -1;
                    if (!cycle && placed == count && count > 0) {
                        result = order[0] | (order[1] << 8) | (order[2] << 16) | (order[3] << 24);
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "Auth.Check" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t hash = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t password = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t result = (password > 0 && hash == (password ^ 21072)) ? std::int64_t{ 1 } : std::int64_t{ -1 };
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "ACL.Allow" && inst.callArgRegs.size() >= 3) {
                    const std::int64_t user = regs[inst.callArgRegs[0]].AsInt();
                    const std::int64_t path = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t write = regs[inst.callArgRegs[2]].AsInt();
                    std::int64_t result = 0;
                    if (user >= 0 && user <= 2 && write == 0) result = 1;
                    if (write == 1 && path == 1 && user == 0) result = 1;
                    if (write == 1 && path == 2 && (user == 0 || user == 2)) result = 1;
                    if (write == 1 && path == 3 && (user == 0 || user == 1)) result = 1;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ result };
                } else if (inst.symbolImm == "Port.Read8") {
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ std::int64_t{ 0 } };
                } else if (inst.symbolImm == "Port.Write8" && inst.callArgRegs.size() >= 2) {
                    const std::int64_t port = regs[inst.callArgRegs[0]].AsInt();
                    if (port == 1016) {
                        const unsigned byte = static_cast<unsigned>(regs[inst.callArgRegs[1]].AsInt()) & 0xFFu;
                        m_stdoutBuffer.push_back(static_cast<char>(byte));
                    }
                } else if (inst.symbolImm == "CPU.Halt") {
                    m_halt = true;
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
                } else if (inst.symbolImm == "Args.Count") {
                    if (inst.destReg != 0) {
                        regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(g_programArguments.size()) };
                    }
                } else if (inst.symbolImm == "Args.Get" && !inst.callArgRegs.empty()) {
                    const std::int64_t index = regs[inst.callArgRegs[0]].AsInt();
                    std::string text;
                    if (index >= 0 && static_cast<std::size_t>(index) < g_programArguments.size()) {
                        text = g_programArguments[static_cast<std::size_t>(index)];
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ text };
                } else if (inst.symbolImm == "Env.Get" && !inst.callArgRegs.empty()) {
                    char* value = rx_env_get(regs[inst.callArgRegs[0]].ToString().c_str());
                    std::string text = value != nullptr ? value : "";
                    std::free(value);
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ std::move(text) };
                } else if (inst.symbolImm == "Env.Exists" && !inst.callArgRegs.empty()) {
                    if (inst.destReg != 0) {
                        regs[inst.destReg] = VMValue{ rx_env_exists(regs[inst.callArgRegs[0]].ToString().c_str()) };
                    }
                } else if ((inst.symbolImm == "Env.Set" || inst.symbolImm == "Env.Add") && inst.callArgRegs.size() >= 2) {
                    const std::string key = regs[inst.callArgRegs[0]].ToString();
                    const std::string value = regs[inst.callArgRegs[1]].ToString();
                    const std::int64_t wrote = inst.symbolImm == "Env.Set"
                        ? rx_env_set(key.c_str(), value.c_str())
                        : rx_env_add(key.c_str(), value.c_str());
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ wrote };
                } else if (inst.symbolImm == "Env.Remove" && !inst.callArgRegs.empty()) {
                    if (inst.destReg != 0) {
                        regs[inst.destReg] = VMValue{ rx_env_remove(regs[inst.callArgRegs[0]].ToString().c_str()) };
                    }
                } else if (inst.symbolImm == "Network.Call" && inst.callArgRegs.size() >= 3) {
                    const std::string service = regs[inst.callArgRegs[0]].ToString();
                    const std::string method = regs[inst.callArgRegs[1]].ToString();
                    const std::string text = regs[inst.callArgRegs[2]].ToString();
                    char* reply = rx_network_call(service.c_str(), method.c_str(), text.c_str());
                    std::string value = reply != nullptr ? reply : "";
                    std::free(reply);
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ std::move(value) };
                } else if (inst.symbolImm == "Network.SendText" && !inst.callArgRegs.empty()) {
                    const std::string text = regs[inst.callArgRegs[0]].ToString();
                    std::string reply;
#ifdef _WIN32
                    WSADATA wsa{};
                    if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
                        SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                        sockaddr_in address{};
                        address.sin_family = AF_INET;
                        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                        address.sin_port = 0;
                        if (listener != INVALID_SOCKET &&
                            bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 &&
                            listen(listener, 1) == 0) {
                            sockaddr_in bound{};
                            int boundLength = sizeof(bound);
                            getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &boundLength);
                            const unsigned short port = ntohs(bound.sin_port);
                            std::thread server([&] {
                                SOCKET accepted = accept(listener, nullptr, nullptr);
                                if (accepted == INVALID_SOCKET) return;
                                char buffer[4096];
                                const int got = recv(accepted, buffer, static_cast<int>(sizeof buffer) - 1, 0);
                                std::string inbound = got > 0 ? std::string(buffer, buffer + got) : std::string();
                                while (!inbound.empty() && (inbound.back() == '\n' || inbound.back() == '\r')) inbound.pop_back();
                                const std::string outbound = "Hello, " + inbound + "\n";
                                send(accepted, outbound.data(), static_cast<int>(outbound.size()), 0);
                                closesocket(accepted);
                            });
                            SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                            sockaddr_in destination{};
                            destination.sin_family = AF_INET;
                            destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                            destination.sin_port = htons(port);
                            if (client != INVALID_SOCKET &&
                                connect(client, reinterpret_cast<sockaddr*>(&destination), sizeof(destination)) == 0) {
                                const std::string outbound = text + "\n";
                                send(client, outbound.data(), static_cast<int>(outbound.size()), 0);
                                char buffer[4096];
                                const int got = recv(client, buffer, static_cast<int>(sizeof buffer) - 1, 0);
                                if (got > 0) reply.assign(buffer, buffer + got);
                                while (!reply.empty() && (reply.back() == '\n' || reply.back() == '\r')) reply.pop_back();
                            }
                            if (client != INVALID_SOCKET) closesocket(client);
                            server.join();
                        }
                        if (listener != INVALID_SOCKET) closesocket(listener);
                        WSACleanup();
                    }
#endif
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ reply };
                } else if (inst.symbolImm == "Input.Play" && !inst.callArgRegs.empty()) {
                    const std::string script = regs[inst.callArgRegs[0]].ToString();
                    if (!Robix::RoboParseInputScript(script, m_input)) {
                        m_input.clear();
                        std::fprintf(stderr, "robix vm: Input.Play could not read the script\n");
                    }
                    m_inputHead = 0;
                    Robix::RoboPlayedMouse(m_input, m_mouseX, m_mouseY);
                    const std::int64_t count = static_cast<std::int64_t>(m_input.size() - m_inputHead);
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ count };
                } else if (inst.symbolImm == "Input.Count") {
                    const std::int64_t count = static_cast<std::int64_t>(m_input.size() - m_inputHead);
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ count };
                } else if (inst.symbolImm == "Input.Clicks") {
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(m_clicks) };
                } else if (inst.symbolImm == "Input.MouseX") {
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(m_mouseX) };
                } else if (inst.symbolImm == "Input.MouseY") {
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(m_mouseY) };
                } else if (inst.symbolImm == "Input.Pop") {
                    std::int64_t popped = 0;
                    if (m_inputHead < m_input.size()) {
                        ++m_inputHead;
                        popped = 1;
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ popped };
                } else if (inst.symbolImm == "Input.Kind" || inst.symbolImm == "Input.Code" ||
                           inst.symbolImm == "Input.Button" || inst.symbolImm == "Input.X" ||
                           inst.symbolImm == "Input.Y") {
                    const std::int64_t index = inst.callArgRegs.empty() ? 0 : regs[inst.callArgRegs[0]].AsInt();
                    int field = 0;
                    if (inst.symbolImm == "Input.Code") field = 1;
                    else if (inst.symbolImm == "Input.Button") field = 2;
                    else if (inst.symbolImm == "Input.X") field = 3;
                    else if (inst.symbolImm == "Input.Y") field = 4;
                    std::int64_t value = 0;
                    if (index >= 0) {
                        const std::size_t at = m_inputHead + static_cast<std::size_t>(index);
                        if (at < m_input.size()) {
                            const Robix::RoboPlayedEvent& event = m_input[at];
                            if (field == 0) value = event.type;
                            else if (field == 1) value = event.code;
                            else if (field == 2) value = event.value;
                            else if (field == 3) value = event.x;
                            else if (field == 4) value = event.y;
                        }
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ value };
                } else if (inst.symbolImm == "Input.Push" && inst.callArgRegs.size() >= 5) {
                    if (m_input.size() < 128) {
                        Robix::RoboPlayedEvent event;
                        event.type = static_cast<int>(regs[inst.callArgRegs[0]].AsInt());
                        event.code = static_cast<int>(regs[inst.callArgRegs[1]].AsInt());
                        event.value = static_cast<int>(regs[inst.callArgRegs[2]].AsInt());
                        event.x = static_cast<int>(regs[inst.callArgRegs[3]].AsInt());
                        event.y = static_cast<int>(regs[inst.callArgRegs[4]].AsInt());
                        m_input.push_back(event);
                    }
                    const std::int64_t count = static_cast<std::int64_t>(m_input.size() - m_inputHead);
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ count };
                } else if (inst.symbolImm == "Input.Clear") {
                    m_input.clear();
                    m_inputHead = 0;
                    m_mouseX = 0;
                    m_mouseY = 0;
                    m_clicks = 0;
                } else if (inst.symbolImm == "Input.SetMouse" && inst.callArgRegs.size() >= 2) {
                    m_mouseX = static_cast<int>(regs[inst.callArgRegs[0]].AsInt());
                    m_mouseY = static_cast<int>(regs[inst.callArgRegs[1]].AsInt());
                } else if (inst.symbolImm == "Input.AddClick") {
                    ++m_clicks;
                } else if (inst.symbolImm == "Scene.Render" && inst.callArgRegs.size() >= 3) {
                    auto obj = AsObj(regs[inst.callArgRegs[1]]);
                    const std::string path = regs[inst.callArgRegs[0]].ToString();
                    const std::string camera = regs[inst.callArgRegs[2]].ToString();
                    std::vector<std::uint32_t> pixels;
                    std::int64_t drawn = 0;
                    if (obj && obj->buffer && obj->className == "Frame" &&
                        RoboRasterizeScene(path, camera, obj->width, static_cast<int>(obj->index), pixels) &&
                        pixels.size() == obj->buffer->bytes.size() / 4) {
                        for (std::size_t i = 0; i < pixels.size(); ++i) {
                            const std::size_t at = i * 4;
                            obj->buffer->bytes[at] = static_cast<std::uint8_t>(pixels[i]);
                            obj->buffer->bytes[at + 1] = static_cast<std::uint8_t>(pixels[i] >> 8);
                            obj->buffer->bytes[at + 2] = static_cast<std::uint8_t>(pixels[i] >> 16);
                            obj->buffer->bytes[at + 3] = static_cast<std::uint8_t>(pixels[i] >> 24);
                        }
                        drawn = 1;
                    } else {
                        std::fprintf(stderr, "robix vm: Scene.Render could not read '%s'\n", path.c_str());
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ drawn };
                } else if (inst.symbolImm == "UI.LoadCooked" && !inst.callArgRegs.empty()) {
                    const std::string path = regs[inst.callArgRegs[0]].ToString();
                    if (!Robix::UI::RoboCookedLoad(path, m_cooked)) {
                        m_cooked.clear();
                        std::fprintf(stderr, "robix vm: UI.LoadCooked could not read '%s'\n", path.c_str());
                    }
                    m_focus = -1;
                    m_menu = 0;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(m_cooked.size()) };
                } else if (inst.symbolImm == "UI.Role" || inst.symbolImm == "UI.BoxX" || inst.symbolImm == "UI.BoxY" ||
                           inst.symbolImm == "UI.BoxWidth" || inst.symbolImm == "UI.BoxHeight") {
                    const std::int64_t index = inst.callArgRegs.empty() ? 0 : regs[inst.callArgRegs[0]].AsInt();
                    int field = 0;
                    if (inst.symbolImm == "UI.BoxX") field = 1;
                    else if (inst.symbolImm == "UI.BoxY") field = 2;
                    else if (inst.symbolImm == "UI.BoxWidth") field = 3;
                    else if (inst.symbolImm == "UI.BoxHeight") field = 4;
                    std::int64_t value = 0;
                    if (index >= 0 && static_cast<std::size_t>(index) < m_cooked.size()) {
                        value = Robix::UI::RoboCookedField(m_cooked[static_cast<std::size_t>(index)], field);
                    }
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ value };
                } else if (inst.symbolImm == "UI.Hit" && inst.callArgRegs.size() >= 2) {
                    const int x = static_cast<int>(regs[inst.callArgRegs[0]].AsInt());
                    const int y = static_cast<int>(regs[inst.callArgRegs[1]].AsInt());
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(Robix::UI::RoboCookedHit(m_cooked, x, y)) };
                } else if (inst.symbolImm == "UI.Place" && inst.callArgRegs.size() >= 5) {
                    const int index = static_cast<int>(regs[inst.callArgRegs[0]].AsInt());
                    const int x = static_cast<int>(regs[inst.callArgRegs[1]].AsInt());
                    const int y = static_cast<int>(regs[inst.callArgRegs[2]].AsInt());
                    const int width = static_cast<int>(regs[inst.callArgRegs[3]].AsInt());
                    const int height = static_cast<int>(regs[inst.callArgRegs[4]].AsInt());
                    const std::int64_t kind = Robix::UI::RoboCookedPlace(m_cooked, index, x, y, width, height);
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ kind };
                } else if (inst.symbolImm == "UI.Drag" && inst.callArgRegs.size() >= 3) {
                    const int index = static_cast<int>(regs[inst.callArgRegs[0]].AsInt());
                    const int dx = static_cast<int>(regs[inst.callArgRegs[1]].AsInt());
                    const int dy = static_cast<int>(regs[inst.callArgRegs[2]].AsInt());
                    const std::int64_t kind = Robix::UI::RoboCookedDrag(m_cooked, index, dx, dy);
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ kind };
                } else if (inst.symbolImm == "UI.Snap" && inst.callArgRegs.size() >= 2) {
                    const int index = static_cast<int>(regs[inst.callArgRegs[0]].AsInt());
                    const int side = static_cast<int>(regs[inst.callArgRegs[1]].AsInt());
                    const std::int64_t kind = Robix::UI::RoboCookedSnap(m_cooked, index, side);
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ kind };
                } else if (inst.symbolImm == "UI.Focus" && !inst.callArgRegs.empty()) {
                    m_focus = static_cast<int>(regs[inst.callArgRegs[0]].AsInt());
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(m_focus) };
                } else if (inst.symbolImm == "UI.Focused") {
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(m_focus) };
                } else if (inst.symbolImm == "UI.Menu" && !inst.callArgRegs.empty()) {
                    m_menu = static_cast<int>(regs[inst.callArgRegs[0]].AsInt());
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(m_menu) };
                } else if (inst.symbolImm == "UI.MenuOpen") {
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ static_cast<std::int64_t>(m_menu) };
                } else if ((inst.symbolImm == "UI.Set" || inst.symbolImm == "Scene.Set") && inst.callArgRegs.size() >= 3) {
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{};
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
                } else if (inst.symbolImm.rfind("Port.", 0) == 0 || inst.symbolImm.rfind("CPU.", 0) == 0 ||
                           inst.symbolImm.rfind("Timer.", 0) == 0 || inst.symbolImm.rfind("System.", 0) == 0 ||
                           inst.symbolImm.rfind("Console.", 0) == 0 ||
                           inst.symbolImm == "Memory.Pages" || inst.symbolImm == "Memory.Address") {
                    Reject("robix vm: " + inst.symbolImm + " runs in a native freestanding image. Compile with robixc --target x86_64-unknown-none.");
                } else if (inst.symbolImm == "PWM.SetDuty" || inst.symbolImm == "UART.Write" ||
                           inst.symbolImm == "SPI.Transfer" || inst.symbolImm == "I2C.Write" ||
                           inst.symbolImm == "CAN.Send" || inst.symbolImm == "USB.Write" ||
                           inst.symbolImm == "PCIe.Read" || inst.symbolImm == "DMA.Start" ||
                           inst.symbolImm == "Sensor.Read" || inst.symbolImm == "Motor.SetSpeed" ||
                           inst.symbolImm == "Interrupt.Enable" || inst.symbolImm == "HardwareTimer.Read") {
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
                } else if (inst.symbolImm == "AssertPixel" && inst.callArgRegs.size() >= 4) {
                    auto obj = AsObj(regs[inst.callArgRegs[0]]);
                    const std::int64_t x = regs[inst.callArgRegs[1]].AsInt();
                    const std::int64_t y = regs[inst.callArgRegs[2]].AsInt();
                    const std::int64_t expected = regs[inst.callArgRegs[3]].AsInt();
                    std::int64_t value = 0;
                    if (obj && obj->buffer && obj->className == "Frame" &&
                        x >= 0 && y >= 0 && x < obj->width && y < obj->index) {
                        const std::size_t at = static_cast<std::size_t>((y * obj->width + x) * 4);
                        value = static_cast<std::uint32_t>(obj->buffer->bytes[at]) |
                                (static_cast<std::uint32_t>(obj->buffer->bytes[at + 1]) << 8) |
                                (static_cast<std::uint32_t>(obj->buffer->bytes[at + 2]) << 16) |
                                (static_cast<std::uint32_t>(obj->buffer->bytes[at + 3]) << 24);
                    }
                    ok = value == expected;
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

