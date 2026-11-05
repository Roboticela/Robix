#include "BytecodeVM.hpp"
#include "../../Compiler/Types/SpecValidator.hpp"

#include <sstream>

namespace Robix::VM {

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

VMValue BytecodeVM::ExecuteFunction(std::string_view fnName, const std::vector<VMValue>& args) {
    const BytecodeChunk* chunk = m_module.FindChunk(fnName);
    if (!chunk) {
        return VMValue{};
    }
    return ExecuteChunk(*chunk, args);
}

std::int32_t BytecodeVM::ExecuteMainAndGetExitCode() {
    const BytecodeChunk* mainChunk = m_module.FindChunk("Main");
    if (!mainChunk) {
        return 1;
    }
    VMValue ret = ExecuteChunk(*mainChunk, {});

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
    for (const auto& chunk : m_module.chunks) {
        if (!chunk.isTestChunk) continue;
        const std::size_t failsBefore = m_assertFailCount;
        (void)ExecuteChunk(chunk, {});
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
    ++m_callCounts[chunk.name];

    std::unordered_map<std::uint32_t, VMValue> regs;
    std::unordered_map<std::string, VMValue> locals;

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
        const BCInstruction& inst = chunk.code[pc];
        ++pc;

        switch (inst.opcode) {
            case BCOpcode::Nop:
            case BCOpcode::DeferCleanup:
            case BCOpcode::InlineASM:
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
                    regs[inst.destReg] = VMValue{ l.AsInt() + r.AsInt() };
                }
                break;
            }

            case BCOpcode::Sub: {
                const VMValue& l = regs[inst.srcReg1];
                const VMValue& r = regs[inst.srcReg2];
                if (l.IsFloat() || r.IsFloat()) {
                    regs[inst.destReg] = VMValue{ l.AsFloat() - r.AsFloat() };
                } else {
                    regs[inst.destReg] = VMValue{ l.AsInt() - r.AsInt() };
                }
                break;
            }

            case BCOpcode::Mul: {
                const VMValue& l = regs[inst.srcReg1];
                const VMValue& r = regs[inst.srcReg2];
                if (l.IsFloat() || r.IsFloat()) {
                    regs[inst.destReg] = VMValue{ l.AsFloat() * r.AsFloat() };
                } else {
                    regs[inst.destReg] = VMValue{ l.AsInt() * r.AsInt() };
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
                obj->className = inst.symbolImm;
                VMValue objVal{ obj };
                regs[inst.destReg] = objVal;

                // Invoke constructor `ClassName.ClassName` if present
                const std::string ctorName = inst.symbolImm + "." + inst.symbolImm;
                if (const BytecodeChunk* ctorChunk = m_module.FindChunk(ctorName)) {
                    std::vector<VMValue> ctorArgs = { objVal };
                    for (std::uint32_t rId : inst.callArgRegs) {
                        ctorArgs.push_back(regs[rId]);
                    }
                    (void)ExecuteChunk(*ctorChunk, ctorArgs);
                }
                break;
            }

            case BCOpcode::LoadVar:
                regs[inst.destReg] = locals[inst.symbolImm];
                break;

            case BCOpcode::StoreVar:
                locals[inst.symbolImm] = regs[inst.srcReg1];
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

            case BCOpcode::CallOSIntrinsic: {
                if (inst.symbolImm == "File.WriteText" && inst.callArgRegs.size() >= 2) {
                    const std::string path    = regs[inst.callArgRegs[0]].ToString();
                    const std::string content = regs[inst.callArgRegs[1]].ToString();
                    m_virtualFileSystem[path] = content;
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ true };
                } else if (inst.symbolImm == "File.ReadText" && !inst.callArgRegs.empty()) {
                    const std::string path = regs[inst.callArgRegs[0]].ToString();
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ m_virtualFileSystem[path] };
                } else if (inst.symbolImm == "Time.NowMicroseconds") {
                    if (inst.destReg != 0) regs[inst.destReg] = VMValue{ std::int64_t(1000000) };
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
        }
    }

    return VMValue{};
}

} // namespace Robix::VM

