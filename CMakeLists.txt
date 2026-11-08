cmake_minimum_required(VERSION 3.20)
project(luauDec VERSION 1.0.0 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(LUAUDEC_LUAU_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/luau" CACHE PATH "Luau source tree to build against (defaults to the submodule)")
option(LUAUDEC_BUILD_CLI "Build the luauDec command-line tool" ON)

option(LUAU_BUILD_CLI "Build CLI" OFF)
option(LUAU_BUILD_TESTS "Build tests" OFF)
option(LUAU_BUILD_WEB "Build Web module" OFF)
option(LUAU_WERROR "Warnings as errors" OFF)
option(LUAU_STATIC_CRT "Link with the static CRT (/MT)" OFF)

add_subdirectory(${LUAUDEC_LUAU_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/luau EXCLUDE_FROM_ALL)

# The library: one self-contained static archive. Luau's AST and common objects are folded in, so consumers only need
# include/luauDec/Decompile.h and libluaudec; the public API exposes no Luau types.
add_library(luaudec STATIC
    Decompiler/BytecodeReader.cpp
    Decompiler/FlowAnalysis.cpp
    Decompiler/FunctionBuilder.cpp
    Decompiler/Naming.cpp
    Decompiler/Emitter.cpp
    Decompiler/Decompile.cpp
    $<TARGET_OBJECTS:Luau.Ast>
    $<TARGET_OBJECTS:Luau.Common>
)
add_library(luauDec::luaudec ALIAS luaudec)
target_compile_features(luaudec PUBLIC cxx_std_17)
target_include_directories(luaudec
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<TARGET_PROPERTY:Luau.Ast,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:Luau.Common,INTERFACE_INCLUDE_DIRECTORIES>
)
set_target_properties(luaudec PROPERTIES PUBLIC_HEADER include/luauDec/Decompile.h)

install(TARGETS luaudec
    EXPORT luauDecTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT library
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/luauDec COMPONENT library
)
install(EXPORT luauDecTargets
    NAMESPACE luauDec::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/luauDec
    COMPONENT library
)
configure_package_config_file(cmake/luauDecConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/luauDecConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/luauDec
)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/luauDecConfigVersion.cmake COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/luauDecConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/luauDecConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/luauDec
    COMPONENT library
)
configure_file(cmake/luaudec.pc.in ${CMAKE_CURRENT_BINARY_DIR}/luaudec.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/luaudec.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig COMPONENT library)

if(LUAUDEC_BUILD_CLI)
    add_executable(luauDec main.cpp)
    target_link_libraries(luauDec PRIVATE luaudec Luau.Compiler)
    install(TARGETS luauDec RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT cli)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(luauDec.tests tests/RoundTrip.cpp)
    target_link_libraries(luauDec.tests PRIVATE luaudec Luau.Compiler Luau.VM)
    add_test(NAME roundtrip COMMAND luauDec.tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus)

    # Not a test: decompiles a directory of bytecode files and compares the recompiled programs (tests/Bench.cpp).
    add_executable(luauDec.bench tests/Bench.cpp)
    target_include_directories(luauDec.bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(luauDec.bench PRIVATE luaudec Luau.Compiler)
endif()
