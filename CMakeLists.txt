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

# Stage 2: Shared C++20 Frontend Library (Lexer, Parser, AST, Diagnostics, TypeChecker, ExplainValidator)
add_library(RobixFrontend STATIC
    Compiler/Semantic/Diagnostics.cpp
    Compiler/Lexer/Lexer.cpp
    Compiler/Parser/Parser.cpp
    Compiler/Types/TypeChecker.cpp
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


