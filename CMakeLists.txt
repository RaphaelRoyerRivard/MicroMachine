cmake_minimum_required(VERSION 3.1)

project(CommandCenter)

# Specify output directories.
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")

# Include custom macros to find SC2Api.
set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(SC2Api REQUIRED)

# Build with c++14 support, required by sc2api.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Activate dynamic linking to allow the backtrace to find symbol info
SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-rdynamic")

add_subdirectory("src")
