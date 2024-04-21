# Pick whatever version you have
cmake_minimum_required(VERSION 3.24)

project(skald CXX)
set(CMAKE_CXX_STANDARD 20)

# Unless you are writing a plugin that needs Qt's UI, specify this
set(HEADLESS 1)

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-D_GLIBCXX_ASSERTIONS -fasynchronous-unwind-tables \
    -fexceptions -g -Wall -Werror -pedantic -O0")

find_path(
    BN_API_PATH
    NAMES binaryninjaapi.h
    # List of paths to search for the clone of the api
    HINTS binaryninjaapi $ENV{BN_API_PATH}
    REQUIRED
)
add_subdirectory(${BN_API_PATH} api)

option (FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." TRUE)
if (${FORCE_COLORED_OUTPUT})
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
       add_compile_options (-fdiagnostics-color=always)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
       add_compile_options (-fcolor-diagnostics)
    endif ()
endif ()

# Use whichever sources and plugin name you want
add_library(skald SHARED
    skald.cpp inheritance_graph.cpp type_accessor.cpp
)

# Link with Binary Ninja
target_link_libraries(skald PUBLIC binaryninjaapi)

# Tell `cmake --install` to copy your plugin to the plugins directory
bn_install_plugin(skald)