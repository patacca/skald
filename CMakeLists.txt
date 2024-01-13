# Pick whatever version you have
cmake_minimum_required(VERSION 3.24)

# Name your plugin
project(skald CXX)

set(CMAKE_CXX_STANDARD 17)

# Unless you are writing a plugin that needs Qt's UI, specify this
set(HEADLESS 1)

find_path(
    BN_API_PATH
    NAMES binaryninjaapi.h
    # List of paths to search for the clone of the api
    HINTS binaryninjaapi $ENV{BN_API_PATH}
    REQUIRED
)
add_subdirectory(${BN_API_PATH} api)

# Use whichever sources and plugin name you want
add_library(skald SHARED skald.cpp)

# Link with Binary Ninja
target_link_libraries(skald PUBLIC binaryninjaapi)

# Tell `cmake --install` to copy your plugin to the plugins directory
bn_install_plugin(skald)