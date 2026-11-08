cmake_minimum_required(VERSION 3.20)
project(luauDec LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LUAU_BUILD_CLI "Build CLI" OFF)
option(LUAU_BUILD_TESTS "Build tests" OFF)
option(LUAU_BUILD_WEB "Build Web module" OFF)
option(LUAU_WERROR "Warnings as errors" OFF)
option(LUAU_STATIC_CRT "Link with the static CRT (/MT)" OFF)

add_subdirectory(luau EXCLUDE_FROM_ALL)

add_library(luauDec.lib STATIC
    Decompiler/BytecodeReader.cpp
    Decompiler/FlowAnalysis.cpp
    Decompiler/FunctionBuilder.cpp
    Decompiler/Naming.cpp
    Decompiler/Emitter.cpp
    Decompiler/Decompile.cpp
)
target_include_directories(luauDec.lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(luauDec.lib PUBLIC Luau.Ast Luau.Common)

add_executable(luauDec main.cpp)
target_link_libraries(luauDec PRIVATE luauDec.lib Luau.Compiler)

include(CTest)
if(BUILD_TESTING)
    add_executable(luauDec.tests tests/RoundTrip.cpp)
    target_link_libraries(luauDec.tests PRIVATE luauDec.lib Luau.Compiler Luau.VM)
    add_test(NAME roundtrip COMMAND luauDec.tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus)
endif()
