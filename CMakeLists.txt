cmake_minimum_required(VERSION 3.20)
project(fsvng VERSION 0.10.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform detection
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX)
elseif(APPLE)
    add_definitions(-DPLATFORM_MACOS)
else()
    add_definitions(-DPLATFORM_LINUX)
endif()

# Fetch all dependencies
include(cmake/FetchDependencies.cmake)

# Main source target
add_subdirectory(src)

# Tests (optional)
option(FSVNG_BUILD_TESTS "Build tests" OFF)
if(FSVNG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
