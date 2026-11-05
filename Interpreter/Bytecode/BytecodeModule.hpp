#pragma once

#include "../../Compiler/IR/SSAIR.hpp"
#include "../../Compiler/Semantic/Diagnostics.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Robix::VM {

inline constexpr std::string_view BYTECODE_MAGIC   = "RBXBC";
inline constexpr std::uint32_t    BYTECODE_VERSION = 1;

enum class BCOpcode : std::uint8_t {
    Nop = 0,
    LoadConstInt,
    LoadConstFloat,
    LoadConstBool,
    LoadConstStr,
    CopyReg,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or,
    Neg,
    Not,
    AllocObj,
    LoadVar,
    StoreVar,
    GetField,
    SetField,
    CallFn,
    CallOSIntrinsic,
    PrintReg,
    AssertReg,
    Jump,
    JumpIfFalse,
    ReturnReg,
    ReturnVoid,
    DeferCleanup,
    InlineASM
};

struct BCInstruction {
    BCOpcode opcode        = BCOpcode::Nop;
    std::uint32_t destReg  = 0;
    std::uint32_t srcReg1  = 0;
    std::uint32_t srcReg2  = 0;
    std::uint32_t targetPC = 0; // For Jump / JumpIfFalse
    std::int64_t intImm    = 0;
    double floatImm        = 0.0;
    bool boolImm           = false;
    std::string symbolImm;      // Variable/field/function name or string literal
    std::vector<std::uint32_t> callArgRegs;
    std::uint32_t sourceLine = 1;
};

struct BytecodeChunk {
    std::string name;
    std::string mangledName;
    std::vector<std::string> paramNames;
    std::string returnTypeName = "Void";
    bool isTestChunk           = false;
    std::vector<BCInstruction> code;
};

struct BytecodeModule {
    std::string magic         = std::string(BYTECODE_MAGIC);
    std::uint32_t version     = BYTECODE_VERSION;
    std::string moduleName;
    std::vector<BytecodeChunk> chunks;

    [[nodiscard]] const BytecodeChunk* FindChunk(std::string_view name) const noexcept;
};

class BytecodeGenerator {
public:
    explicit BytecodeGenerator(Frontend::DiagnosticBag& diagnostics) noexcept;

    [[nodiscard]] BytecodeModule GenerateFromIR(const IR::IRModule& irMod);
    [[nodiscard]] bool VerifyBytecodeModule(const BytecodeModule& bcMod);

private:
    Frontend::DiagnosticBag& m_diagnostics;
    BytecodeChunk LowerIRFunction(const IR::IRFunction& irFn);
};

} // namespace Robix::VM

