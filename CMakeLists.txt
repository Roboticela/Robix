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
add_executable(Stage1SpecTests
    Tests/Semantics/Stage1SpecTests.cpp
)
target_link_libraries(Stage1SpecTests PRIVATE RobixSpec)
add_test(NAME Stage1SpecTests COMMAND Stage1SpecTests)

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
add_executable(Stage2FrontendTests
    Tests/Compiler/Stage2FrontendTests.cpp
)
target_link_libraries(Stage2FrontendTests PRIVATE RobixFrontend)
add_test(NAME Stage2FrontendTests COMMAND Stage2FrontendTests)

# Stage 3 Conformance Test Executable
add_executable(Stage3SemanticTests
    Tests/Semantics/Stage3SemanticTests.cpp
)
target_link_libraries(Stage3SemanticTests PRIVATE RobixFrontend)
add_test(NAME Stage3SemanticTests COMMAND Stage3SemanticTests)

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
add_executable(Stage4DualEngineTests
    Tests/Interpreter/Stage4DualEngineTests.cpp
)
target_link_libraries(Stage4DualEngineTests PRIVATE RobixExecutionEngine)
add_test(NAME Stage4DualEngineTests COMMAND Stage4DualEngineTests)

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
add_executable(Stage5MemoryTests
    Tests/Memory/Stage5MemoryTests.cpp
)
target_link_libraries(Stage5MemoryTests PRIVATE RobixMemoryRuntime)
add_test(NAME Stage5MemoryTests COMMAND Stage5MemoryTests)

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
add_executable(Stage6TestingAndVerifierTests
    Tests/Compiler/Stage6TestingAndVerifierTests.cpp
)
target_link_libraries(Stage6TestingAndVerifierTests PRIVATE RobixTestingAndVerifier)
add_test(NAME Stage6TestingAndVerifierTests COMMAND Stage6TestingAndVerifierTests)

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
add_executable(Stage7StdLibAndConcurrencyTests
    Tests/StdLib/Stage7StdLibAndConcurrencyTests.cpp
)
target_link_libraries(Stage7StdLibAndConcurrencyTests PRIVATE RobixStdLibAndConcurrency)
add_test(NAME Stage7StdLibAndConcurrencyTests COMMAND Stage7StdLibAndConcurrencyTests)

# Stage 8: Package Ecosystem & Binary Distribution (.rbxapi, .rbxp, robix)
add_library(RobixPackageSystem STATIC
    Packages/Manager/RbxApiAndPackageFormat.cpp
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
add_executable(Stage8PackageEcosystemTests
    Tests/Packages/Stage8PackageEcosystemTests.cpp
)
target_link_libraries(Stage8PackageEcosystemTests PRIVATE RobixPackageSystem)
add_test(NAME Stage8PackageEcosystemTests COMMAND Stage8PackageEcosystemTests)





