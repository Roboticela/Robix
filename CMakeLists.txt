cmake_minimum_required(VERSION 3.20)
project(Robix VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Stage 1: Specification & ABI Validator Library
add_library(RobixSpec STATIC
    Compiler/Types/SpecValidator.cpp
)
target_include_directories(RobixSpec PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Compiler/Types
)

# Stage 1 Conformance Test Executable
enable_testing()
add_executable(SpecificationTests
    Tests/Semantics/SpecificationTests.cpp
)
target_link_libraries(SpecificationTests PRIVATE RobixSpec)
add_test(NAME SpecificationTests COMMAND SpecificationTests)

# Stage 2 & 3: Shared C++20 Frontend & Semantic Analyzer Library
add_library(RobixFrontend STATIC
    Compiler/Semantic/Diagnostics.cpp
    Compiler/Lexer/Lexer.cpp
    Compiler/Parser/Parser.cpp
    Compiler/Types/TypeChecker.cpp
    Compiler/Semantic/SemanticAnalyzer.cpp
)
target_include_directories(RobixFrontend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Compiler/Lexer
    ${CMAKE_CURRENT_SOURCE_DIR}/Compiler/AST
    ${CMAKE_CURRENT_SOURCE_DIR}/Compiler/Parser
    ${CMAKE_CURRENT_SOURCE_DIR}/Compiler/Semantic
    ${CMAKE_CURRENT_SOURCE_DIR}/Compiler/Types
)
target_link_libraries(RobixFrontend PUBLIC RobixSpec)

# Stage 2 Conformance Test Executable
add_executable(FrontendTests
    Tests/Compiler/FrontendTests.cpp
)
target_link_libraries(FrontendTests PRIVATE RobixFrontend)
add_test(NAME FrontendTests COMMAND FrontendTests)

# Stage 3 Conformance Test Executable
add_executable(SemanticTests
    Tests/Semantics/SemanticTests.cpp
)
target_link_libraries(SemanticTests PRIVATE RobixFrontend)
add_test(NAME SemanticTests COMMAND SemanticTests)

# Stage 4: Dual Execution Architecture (Validated SSA IR, Optimizer Passes, Bytecode VM & LLVM IR CodeGen)
add_library(RobixExecutionEngine STATIC
    Compiler/IR/SSAIR.cpp
    Compiler/Optimizer/IRPassManager.cpp
    Compiler/CodeGen/LLVMIRGenerator.cpp
    Interpreter/Bytecode/BytecodeGenerator.cpp
    Interpreter/VM/BytecodeVM.cpp
)
target_include_directories(RobixExecutionEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Compiler/IR
    ${CMAKE_CURRENT_SOURCE_DIR}/Compiler/Optimizer
    ${CMAKE_CURRENT_SOURCE_DIR}/Compiler/CodeGen
    ${CMAKE_CURRENT_SOURCE_DIR}/Interpreter/Bytecode
    ${CMAKE_CURRENT_SOURCE_DIR}/Interpreter/VM
)
target_link_libraries(RobixExecutionEngine PUBLIC RobixFrontend)

# Stage 4 Conformance Test Executable
add_executable(DualEngineTests
    Tests/Interpreter/DualEngineTests.cpp
)
target_link_libraries(DualEngineTests PRIVATE RobixExecutionEngine)
add_test(NAME DualEngineTests COMMAND DualEngineTests)

# Stage 5: Unified Memory Architecture, GC/RC/Manual Coexistence, Pointers & Low-Level Systems
add_library(RobixMemoryRuntime STATIC
    Runtime/Memory/UnifiedMemory.cpp
    Runtime/Memory/LowLevelSystems.cpp
)
target_include_directories(RobixMemoryRuntime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Runtime/Memory
)
target_link_libraries(RobixMemoryRuntime PUBLIC RobixFrontend)

# Stage 5 Conformance Test Executable
add_executable(MemoryTests
    Tests/Memory/MemoryTests.cpp
)
target_link_libraries(MemoryTests PRIVATE RobixMemoryRuntime)
add_test(NAME MemoryTests COMMAND MemoryTests)

# Stage 6: Mandatory Testing System & Single-Command Verification Engine
add_library(RobixTestingAndVerifier STATIC
    StdLib/Testing/TestingFramework.cpp
    Tools/Verifier/RobixVerifier.cpp
)
target_include_directories(RobixTestingAndVerifier PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/StdLib/Testing
    ${CMAKE_CURRENT_SOURCE_DIR}/Tools/Verifier
)
target_link_libraries(RobixTestingAndVerifier PUBLIC RobixExecutionEngine RobixMemoryRuntime)

# Stage 6 Conformance Test Executable
add_executable(VerifierTests
    Tests/Compiler/VerifierTests.cpp
)
target_link_libraries(VerifierTests PRIVATE RobixTestingAndVerifier)
add_test(NAME VerifierTests COMMAND VerifierTests)

# Stage 7: Built-In Types, Standard Library, Concurrency & Formal Security Architecture
add_library(RobixStdLibAndConcurrency STATIC
    StdLib/Core/CoreTypesAndRegex.cpp
    StdLib/Data/JsonAndDataFormats.cpp
    Runtime/Threading/ConcurrencyAndProcess.cpp
    StdLib/Security/SecurityAndNetwork.cpp
)
target_include_directories(RobixStdLibAndConcurrency PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/StdLib/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/StdLib/Collections
    ${CMAKE_CURRENT_SOURCE_DIR}/StdLib/Data
    ${CMAKE_CURRENT_SOURCE_DIR}/Runtime/Threading
    ${CMAKE_CURRENT_SOURCE_DIR}/StdLib/Security
)
target_link_libraries(RobixStdLibAndConcurrency PUBLIC RobixFrontend)

# Stage 7 Conformance Test Executable
add_executable(StandardLibraryTests
    Tests/StdLib/StandardLibraryTests.cpp
)
target_link_libraries(StandardLibraryTests PRIVATE RobixStdLibAndConcurrency)
add_test(NAME StandardLibraryTests COMMAND StandardLibraryTests)

# Stage 8: Package Ecosystem & Binary Distribution (.rxapi, .rxp, robix)
add_library(RobixPackageSystem STATIC
    Packages/Manager/RxApiAndPackageFormat.cpp
    Packages/Resolver/DependencyResolver.cpp
    Packages/Registry/PackageManagerAndRegistry.cpp
)
target_include_directories(RobixPackageSystem PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Packages/Manager
    ${CMAKE_CURRENT_SOURCE_DIR}/Packages/Resolver
    ${CMAKE_CURRENT_SOURCE_DIR}/Packages/Registry
)
target_link_libraries(RobixPackageSystem PUBLIC RobixStdLibAndConcurrency RobixFrontend)

# Stage 8 Conformance Test Executable
add_executable(PackageTests
    Tests/Packages/PackageTests.cpp
)
target_link_libraries(PackageTests PRIVATE RobixPackageSystem)
add_test(NAME PackageTests COMMAND PackageTests)

# Stage 9: Hardware Access, Real-Time Execution, Robotics & AI Libraries
add_library(RobixHardwareRoboticsAI STATIC
    Runtime/Hardware/HardwareAndRealTime.cpp
    Runtime/Robotics/RoboticsAndControl.cpp
    StdLib/AI/AIAndScientificComputing.cpp
)
target_include_directories(RobixHardwareRoboticsAI PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Runtime/Hardware
    ${CMAKE_CURRENT_SOURCE_DIR}/Runtime/Robotics
    ${CMAKE_CURRENT_SOURCE_DIR}/StdLib/AI
)
target_link_libraries(RobixHardwareRoboticsAI PUBLIC RobixMemoryRuntime RobixStdLibAndConcurrency RobixFrontend)

# Stage 9 Conformance Test Executable
add_executable(HardwareTests
    Tests/Performance/HardwareTests.cpp
)
target_link_libraries(HardwareTests PRIVATE RobixHardwareRoboticsAI)
add_test(NAME HardwareTests COMMAND HardwareTests)

# Stage 13: Developer Experience, CLI, Test Scaffold Generator, Tooling, Benchmarks & Documentation
add_library(RobixCLIToolingAndBenchmarks STATIC
    Tools/CLI/RobixCLIAndTestGenerator.cpp
    Tools/CLI/ExamplesAndDocumentationCatalog.cpp
    Tools/Debugger/LSPDebuggerProfilerAndBenchmarks.cpp
)
target_include_directories(RobixCLIToolingAndBenchmarks PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Tools/CLI
    ${CMAKE_CURRENT_SOURCE_DIR}/Tools/Debugger
)
target_link_libraries(RobixCLIToolingAndBenchmarks PUBLIC
    RobixTestingAndVerifier
    RobixHardwareRoboticsAI
    RobixPackageSystem
    RobixExecutionEngine
    RobixStdLibAndConcurrency
    RobixMemoryRuntime
    RobixFrontend
)

# Stage 13 Conformance Test Executable
add_executable(ToolingTests
    Tests/Compiler/ToolingTests.cpp
)
target_link_libraries(ToolingTests PRIVATE RobixCLIToolingAndBenchmarks)
add_test(NAME ToolingTests COMMAND ToolingTests)







